/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2008 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2006      Sandia National Laboratories. All rights
 *                         reserved.
 * Copyright (c) 2009-2013 Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ompi_config.h"

#include <errno.h>
#include <string.h>

#include "opal/class/opal_bitmap.h"
#include "opal/prefetch.h"
#include "opal/util/output.h"
#include "opal/datatype/opal_convertor.h"
#include "opal/include/opal_stdint.h"

#include "ompi/mca/btl/btl.h"
#include "ompi/mca/btl/base/btl_base_error.h"
#include "ompi/mca/mpool/base/base.h"
#include "ompi/mca/mpool/mpool.h"
#include "ompi/memchecker.h"

#include "btl_usnic.h"
#include "btl_usnic_frag.h"
#include "btl_usnic_proc.h"
#include "btl_usnic_endpoint.h"
#include "btl_usnic_util.h"

#define MAX_STATS_NUM_ENDPOINTS 256


/*
 *  Add procs to this BTL module, receiving endpoint information from
 *  the modex.
 */
static int usnic_add_procs(struct mca_btl_base_module_t* base_module,
                             size_t nprocs,
                             struct ompi_proc_t **ompi_procs,
                             struct mca_btl_base_endpoint_t** endpoints,
                             opal_bitmap_t* reachable)
{
    ompi_btl_usnic_module_t* module = (ompi_btl_usnic_module_t*) base_module;
    struct ibv_ah_attr ah_attr;
    ompi_proc_t* my_proc;
    size_t i, count;
    int rc;

    /* get pointer to my proc structure */
    my_proc = ompi_proc_local();
    if (NULL == my_proc) {
        return OMPI_ERR_OUT_OF_RESOURCE;
    }

    for (i = count = 0; i < nprocs; i++) {
        struct ompi_proc_t* ompi_proc = ompi_procs[i];
        ompi_btl_usnic_proc_t* usnic_proc;
        mca_btl_base_endpoint_t* usnic_endpoint;

        /* Do not create loopback usnic connections */
        if (ompi_proc == my_proc) {
            continue;
        }

        /* USNIC does not support loopback to the same machine */
        if (OPAL_PROC_ON_LOCAL_NODE(ompi_proc->proc_flags)) {
            continue;
        }

        /* Find (or create if it doesn't exist) this peer's proc.
           This will receive the modex info for that proc.  Note that
           the proc is shared by all usnic modules that are trying
           to reach this destination. */
        if (NULL == (usnic_proc = ompi_btl_usnic_proc_create(ompi_proc))) {
            return OMPI_ERR_OUT_OF_RESOURCE;
        }

        /* Create an endpoint specific for this usnic module for
           this specific destination/proc/peer. */
        usnic_endpoint = OBJ_NEW(ompi_btl_usnic_endpoint_t);
        if (NULL == usnic_endpoint) {
            return OMPI_ERR_OUT_OF_RESOURCE;
        }

        /* Stats */
        if (OPAL_UNLIKELY(mca_btl_usnic_component.stats_enabled)) {
            if (module->num_endpoints < MAX_STATS_NUM_ENDPOINTS) {
                module->all_endpoints[module->num_endpoints++] = 
                    usnic_endpoint;
            }
        }

        /* If the proc fails to insert, it's not an error -- it just
           means that the proc said "I couldn't find a matching
           address to communicate with you."  So just skip it and move
           on. */
        rc = ompi_btl_usnic_proc_insert(module, usnic_proc, 
					usnic_endpoint);
        if (rc != OMPI_SUCCESS) {
            OBJ_RELEASE(usnic_endpoint);
            OBJ_RELEASE(usnic_proc);
            continue;
        }

        /* Since the proc inserted successfully, associate the
           endpoint with this module */
        usnic_endpoint->endpoint_module = module;

        /* memset to both silence valgrind warnings (since the attr
           struct ends up getting written down an fd to the kernel)
           and actually zero out all the fields that we don't care
           about / want to be logically false. */
        memset(&ah_attr, 0, sizeof(ah_attr));
	ah_attr.is_global = 1;
        ah_attr.port_num = 1;
	ah_attr.grh.dgid = usnic_endpoint->endpoint_remote_addr.gid;

        usnic_endpoint->endpoint_remote_ah = 
            ibv_create_ah(module->pd, &ah_attr);
        if (NULL == usnic_endpoint->endpoint_remote_ah) {
            OBJ_RELEASE(usnic_endpoint);
            OBJ_RELEASE(usnic_proc);
            BTL_ERROR(("error creating address handle\n"));
            continue;
        }

        opal_output_verbose(5, mca_btl_base_output,
                            "new usnic peer: subnet = 0x%016" PRIx64 ", interface = 0x%016" PRIx64,
                            ntoh64(usnic_endpoint->endpoint_remote_addr.gid.global.subnet_prefix),
                            ntoh64(usnic_endpoint->endpoint_remote_addr.gid.global.interface_id));

        opal_bitmap_set_bit(reachable, i);
        endpoints[i] = usnic_endpoint;
        opal_output_verbose(5, mca_btl_base_output,
                            "made %p endpoint", (void*) usnic_endpoint);
        count++;
    }
    opal_output_verbose(5, mca_btl_base_output,
                        "made %" PRIsize_t " endpoints", count);

    return OMPI_SUCCESS;
}


/*
 * Delete the proc as reachable from this btl module
 */
static int usnic_del_procs(struct mca_btl_base_module_t* btl,
                             size_t nprocs,
                             struct ompi_proc_t** procs,
                             struct mca_btl_base_endpoint_t** peers)
{
    size_t i;

    for (i = 0; i < nprocs; i++) {
        ompi_btl_usnic_endpoint_t* endpoint = (ompi_btl_usnic_endpoint_t*)peers[i];
        ompi_btl_usnic_proc_t* proc = ompi_btl_usnic_proc_lookup_ompi(procs[i]);
        if (NULL != proc) {
            ompi_btl_usnic_proc_remove(proc, endpoint);
        }

        OBJ_RELEASE(endpoint);
    }

    return OMPI_SUCCESS;
}


/**
 * Allocate PML control messages or eager frags if BTL does not have
 * IN_PLACE flag.  To be clear: max it will ever alloc is eager_limit.
 * THEREFORE: eager_limit is the max that ALLOC must always be able to
 * alloc.
 *  --> Contraction in the btl.h documentation.
 */
static mca_btl_base_descriptor_t* 
usnic_alloc(struct mca_btl_base_module_t* btl,
              struct mca_btl_base_endpoint_t* endpoint,
              uint8_t order,
              size_t size,
              uint32_t flags)
{
    ompi_btl_usnic_frag_t* frag = NULL;
    ompi_btl_usnic_module_t *module = (ompi_btl_usnic_module_t*) btl;

    frag = ompi_btl_usnic_frag_send_alloc(module);
    if (NULL == frag) {
        return NULL;
    }
    /* JMS remove me */
    if (!(flags & MCA_BTL_DES_FLAGS_BTL_OWNERSHIP)) {
        opal_output(0, "==================btl alloc: flags: NONE");
    }

    frag->base.order = MCA_BTL_NO_ORDER;
    frag->base.des_flags = flags;
    frag->segment.seg_len = size;
    return (mca_btl_base_descriptor_t*)frag;
}


/**
 * Return a segment
 *
 * Return the segment to the appropriate preallocated segment list
 */
static int usnic_free(struct mca_btl_base_module_t* btl,
                        mca_btl_base_descriptor_t* des)
{
    ompi_btl_usnic_module_t *module = (ompi_btl_usnic_module_t*) btl;
    ompi_btl_usnic_frag_t* frag = (ompi_btl_usnic_frag_t*)des;

    ompi_btl_usnic_frag_send_return(module, frag);

    return OMPI_SUCCESS;
}

/*
 * Notes from george:
 *
 * - BTL ALLOC: allocating PML control messages or eager frags if BTL
     does not have IN_PLACE flag.  To be clear: max it will ever alloc
     is eager_limit.  THEREFORE: eager_limit is the max that ALLOC
     must always be able to alloc.
     --> Contraction in the btl.h documentation.
 *
 * - BTL PREPARE SRC: max_send_size frags go through here.  Can return
     a smaller size than was asked for.
 *
 * - BTL PREPARE DEST: not used if you don't have PUT/GET
 *
 * - BTL SEND: will be used after ALLOC / PREPARE
 */


/**
 * Pack data and return a descriptor that can be used for send (or
 * put, but we don't do that here in usnic).
 */
static mca_btl_base_descriptor_t* usnic_prepare_src(
    struct mca_btl_base_module_t* base_module,
    struct mca_btl_base_endpoint_t* endpoint,
    struct mca_mpool_base_registration_t* registration,
    struct opal_convertor_t* convertor,
    uint8_t order,
    size_t reserve,
    size_t* size,
    uint32_t flags)
{
    ompi_btl_usnic_module_t *module = (ompi_btl_usnic_module_t*) base_module;
    ompi_btl_usnic_frag_t *frag;
    struct iovec iov;
    uint32_t iov_count = 1;
    size_t max_data = *size;
    int rc;

    /* We will not make descriptors bigger than max_send_size (i.e.,
       let the PML do the fragmenting / reassembly) */
    if (max_data + reserve > base_module->btl_max_send_size) {
        max_data = base_module->btl_max_send_size - reserve;
    }
    if (!(flags & MCA_BTL_DES_FLAGS_BTL_OWNERSHIP)) {
        opal_output(0, "==================prepare_src: flags: NONE");
    }
    frag = ompi_btl_usnic_frag_send_alloc(module);
    if (OPAL_UNLIKELY(NULL == frag)) {
        return NULL;
    }

    /* We *always* have to pack, so copy from the convertor to the
       usnic packet buffer, right after the reserve data */
    iov.iov_len = max_data;
    iov.iov_base = (IOVBASE_TYPE*)
        (((uint8_t*) frag->segment.seg_addr.pval) + reserve);
    /* JMS From Dec 2012 OMPI meeting, Nathan says look at Vader BTL
       -- something about if opal convertor doesn't need buffers, then
       just memmove.  Will save 100ns or so. */
    rc = opal_convertor_pack(convertor, &iov, &iov_count, &max_data);
    if (OPAL_UNLIKELY(rc < 0)) {
        ompi_btl_usnic_frag_send_return(module, frag);
        return NULL;
    }

    /* Update segment length with the number of bytes that we were
       able to pack */
    frag->segment.seg_len = reserve + max_data;

    frag->base.des_src = &frag->segment;
    frag->base.des_dst = NULL;
    frag->base.des_dst_cnt = 0;
    frag->base.des_flags = flags;
    frag->base.order = MCA_BTL_NO_ORDER;
    *size = max_data;

    return &frag->base;
}


static inline void usnic_stats_reset(ompi_btl_usnic_module_t *module)
{
    module->num_total_sends =
        module->num_resends =
        module->num_frag_sends =
        module->num_ack_recvs =
        
        module->num_total_recvs =
        module->num_unk_recvs =
        module->num_dup_recvs =
        module->num_oow_recvs =
        module->num_frag_recvs =
        module->num_ack_sends =
        module->num_recv_reposts =
        
        module->num_deferred_sends_no_wqe =
        module->num_deferred_sends_outside_window =
        module->num_deferred_sends_hotel_full =
        
        module->max_sent_window_size =
        module->max_rcvd_window_size = 

#if 0        
        module->pml_module_sends = 
        module->pml_module_sends_deferred =
        module->pml_send_callbacks = 
#endif
        0;
}


static void usnic_stats_callback(int fd, short flags, void *arg)
{
    ompi_btl_usnic_module_t *module = (ompi_btl_usnic_module_t*) arg;
    char tmp[128], str[2048];

    if (!mca_btl_usnic_component.stats_enabled) {
        return;
    }

    if (module->final_stats) {
        snprintf(tmp, sizeof(tmp), "final");
    } else {
        snprintf(tmp, sizeof(tmp), "%4lu", ++module->stats_report_num);
    }

    /* The usuals */
    snprintf(str, sizeof(str), "%s:MCW:%2u, SndTot/F/RS/A:%8lu/%4lu/%8lu/%8lu, RcvTot/Chk/F/OOW/Dup/A/Unk:%8lu/%c%c/%8lu/%4lu/%4lu/%8lu/%1lu ",
             tmp,
             ompi_proc_local()->proc_name.vpid,

             module->num_total_sends,
             module->num_frag_sends,
             module->num_resends,
             module->num_ack_sends,
             
             module->num_total_recvs,
             (module->num_total_recvs - module->num_recv_reposts) == 0 ? 'g' : 'B',
             (module->num_total_recvs -
              module->num_frag_recvs -
              module->num_oow_recvs -
              module->num_dup_recvs -
              module->num_ack_recvs -
              module->num_unk_recvs) == 0 ? 'g' : 'B',
             module->num_frag_recvs,
             module->num_oow_recvs,
             module->num_dup_recvs,
             module->num_ack_recvs,
             module->num_unk_recvs);

#if RELIABILITY
    /* If our PML calls were 0, then show send and receive window
       extents instead */
    if (1 || module->pml_module_sends + module->pml_module_sends_deferred +
        module->pml_send_callbacks == 0) {
        int i;
        uint64_t size;
        uint64_t send_min = 99999999999, send_max = 0;
        uint64_t recv_min = 99999999999, recv_max = 0;
        ompi_btl_usnic_endpoint_t *endpoint;

        for (i = 0; i < module->num_endpoints; ++i) {
            endpoint = module->all_endpoints[i];

            /* Sender window */
            size = WINDOW_SIZE -
                (endpoint->endpoint_next_seq_to_send - 
                 endpoint->endpoint_ack_seq_rcvd - 1);
            if (size < send_min) send_min = size;
            if (size > send_max) send_max = size;

            /* Receiver window */
            size = WINDOW_SIZE -
                (endpoint->endpoint_highest_seq_rcvd - 
                 endpoint->endpoint_next_contig_seq_to_recv + 1);
            if (size < recv_min) recv_min = size;
            if (size > recv_max) recv_max = size;
        }
        snprintf(tmp, sizeof(tmp), "PML D:%1lu, Win Sn/Sx/Rn/R:%4lu/%4lu/%4lu/%4lu",
                 module->pml_module_sends - (module->pml_module_sends_deferred + module->pml_send_callbacks),
                 send_min,
                 send_max,
                 recv_min,
                 recv_max);
    } else {
#endif
        snprintf(tmp, sizeof(tmp), "PML S/SD/CB/Diff:%4lu/%4lu/%4lu=%4ld",
                module->pml_module_sends,
                module->pml_module_sends_deferred,
                module->pml_send_callbacks,
                module->pml_module_sends - (module->pml_module_sends_deferred + module->pml_send_callbacks)
                );
#if RELIABILITY
    }
#endif

    strncat(str, tmp, sizeof(str));
    opal_output(0, str);

    if (mca_btl_usnic_component.stats_relative) {
        usnic_stats_reset(module);
    }

    /* In OMPI v1.6, we have to re-add this event (because there's an
       old libevent in OMPI v1.6) */
    if (!module->final_stats) {
        opal_event_add(&(module->stats_timer_event),
                       &(module->stats_timeout));
    }
}


static int usnic_finalize(struct mca_btl_base_module_t* btl)
{
    ompi_btl_usnic_module_t* module = (ompi_btl_usnic_module_t*)btl;

    ibv_destroy_qp(module->qp);
    ibv_dealloc_pd(module->pd);
    
    /* Disable the stats callback event, and then call the stats
       callback manually to display the final stats */
    if (mca_btl_usnic_component.stats_enabled) {
        opal_event_del(&(module->stats_timer_event));
        module->final_stats = true;
        usnic_stats_callback(0, 0, module);
    }

    if (NULL != module->all_endpoints) {
        free(module->all_endpoints);
    }
#if RELIABILITY
    OBJ_DESTRUCT(&module->endpoints_that_need_acks);
    OBJ_DESTRUCT(&module->pending_resend_frags);
    OBJ_DESTRUCT(&module->ack_frags);
#endif
    OBJ_DESTRUCT(&module->send_frags);
    OBJ_DESTRUCT(&module->recv_frags);
    OBJ_DESTRUCT(&module->senders);
    mca_mpool_base_module_destroy(module->super.btl_mpool);
    ompi_btl_usnic_proc_finalize();

    return OMPI_SUCCESS;
}


/*
 *  Initiate a send.
 */
static int usnic_send(struct mca_btl_base_module_t* module,
                        struct mca_btl_base_endpoint_t* endpoint,
                        struct mca_btl_base_descriptor_t* descriptor,
                        mca_btl_base_tag_t tag)
{
    int rc;
    ompi_btl_usnic_frag_t* frag = (ompi_btl_usnic_frag_t*) descriptor;

    frag->endpoint = endpoint;
    frag->btl_header->payload_type = OMPI_BTL_USNIC_PAYLOAD_TYPE_MSG;
    frag->btl_header->payload_len = descriptor->des_src[0].seg_len;
    frag->payload.pml_header->tag = tag;

    /* JMS: According to George, if I fail to queued up here beause of
       limited resource, I can return something to the PML that says
       "queue up again later" (OMPI_ERR_OUT_OF_RESOURCE).  Or I can
       queue it up internally myself and return something else to the
       PML that says "I queued it up internally".  The latter is a
       little more optimal because I can do the following here in the
       BTL: - queue up the N pending frags - progress all N pending
       frags in component_progress (Vs. PML calling send N more times
       for the old frags)

       That being said:

       - if we do this deferred send to the PML, we must *always* make
         the PML callback in the send completion so that it can know
         to progress frags that were previously stuck due to resource
         limits
    */

    /* JMS From Dec OMPI meeting....

       if PML doesn't set SEND_ALWAYS_CALLBACK, then we can return 1
       here to say "the data is gone, PML can complete the request".
       And then we don't need to do the PML callback (!).  WE DON'T
       NEED TO SET ALWAYS_CALLBACK! */

    /* JMS: Point raised during Mar code review: might be simpler to
       return to PML when we can't send, because then we know
       everything in PML was never sent, and/or is not in the
       receiver's send window.  PML will also not HOL block, so if we
       defer a send to receiver X, PML may still send to receiver Y.
       Deferring to PML might be simpler than trying to combine these
       deferred sends to the deferred send list.  */

    /* JMS: Does the PML copy the header and payload into a contig
       buffer for me, i.e., so I can just do a SG list of 1?  I'm
       pretty sure it does, but I need to check this for sure... */

    /* JMS For UD, if the size of the (PML/BTL headers + payload) <
       max_inline, then set the INLINE flag and we don't need to copy
       to registered memory (because upinder will do the memcpy for me
       to the descriptor entry buffer that contains the L2 header) */

    /* JMS Future optimization: move fragmenting of message down here
       (vs. up in the PML) and only ask for SIGNALED completion for
       the last packet ibv_post_send().  NOTE: Upinder says that there
       is a limitation: we have to get a completion for at least one
       packet in the depth of the send ring.  NOTE 2: must set flag
       when we create the QP to NOT SIGNAL every completion; only the
       ones that set the SIGNALED flag on. */

    /* Stats */
    ++(((ompi_btl_usnic_module_t*)module)->pml_module_sends);

    frag->base.des_flags |= MCA_BTL_DES_SEND_ALWAYS_CALLBACK;
    rc = ompi_btl_usnic_endpoint_post_send((ompi_btl_usnic_module_t*)module, frag);

    /* Stats */
    if (OPAL_UNLIKELY(OMPI_SUCCESS != rc)) {
        ++(((ompi_btl_usnic_module_t*)module)->pml_module_sends_deferred);
    }

    return rc;
}


#if 0
/* JMS write me */
/*
 * Initiate an immediate send
 */
static int usnic_sendi(struct mca_btl_base_module_t* btl,
                         struct mca_btl_base_endpoint_t* endpoint,
                         struct opal_convertor_t* convertor,
                         void* header,
                         size_t header_size,
                         size_t payload_size,
                         uint8_t order,
                         uint32_t flags,
                         mca_btl_base_tag_t tag,
                         mca_btl_base_descriptor_t** descriptor)
{
    /* JMS write me */
    return OMPI_ERROR;
}
#endif


/*
 * RDMA Memory Pool (de)register callbacks
 */
static int usnic_reg_mr(void* reg_data, void* base, size_t size,
                          mca_mpool_base_registration_t* reg)
{
    ompi_btl_usnic_module_t* mod = (ompi_btl_usnic_module_t*)reg_data;
    ompi_btl_usnic_reg_t* ud_reg = (ompi_btl_usnic_reg_t*)reg;

    ud_reg->mr = ibv_reg_mr(mod->pd, base, size, IBV_ACCESS_LOCAL_WRITE |
                            IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ);

    if (NULL == ud_reg->mr) {
        return OMPI_ERR_OUT_OF_RESOURCE;
    }

    return OMPI_SUCCESS;
}


static int usnic_dereg_mr(void* reg_data,
                            mca_mpool_base_registration_t* reg)
{
    ompi_btl_usnic_reg_t* ud_reg = (ompi_btl_usnic_reg_t*)reg;

    if (ud_reg->mr != NULL) {
        if (ibv_dereg_mr(ud_reg->mr)) {
            opal_output(0, "%s: error unpinning UD memory: %s\n",
                        __func__, strerror(errno));
            return OMPI_ERROR;
        }
    }

    ud_reg->mr = NULL;
    return OMPI_SUCCESS;
}


/*
 * Create a single UD queue pair.
 */
static int init_qp(ompi_btl_usnic_module_t* module)
{
    struct ibv_qp_attr qp_attr;
    struct ibv_qp_init_attr qp_init_attr;

    /* memset to both silence valgrind warnings (since the attr struct
       ends up getting written down an fd to the kernel) and actually
       zero out all the fields that we don't care about / want to be
       logically false. */
    memset(&qp_init_attr, 0, sizeof(qp_init_attr));
    qp_init_attr.send_cq = module->cq;
    qp_init_attr.recv_cq = module->cq;
    qp_init_attr.cap.max_send_wr = module->sd_num;
    qp_init_attr.cap.max_recv_wr = module->rd_num;
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1;
    qp_init_attr.qp_type = IBV_QPT_UD;
    
    module->qp = ibv_create_qp(module->pd, &qp_init_attr);
    if (NULL == module->qp) {
        BTL_ERROR(("error creating QP: %s\n", strerror(errno)));
        return OMPI_ERR_OUT_OF_RESOURCE;
    }

    /* memset to both silence valgrind warnings (since the attr struct
       ends up getting written down an fd to the kernel) and actually
       zero out all the fields that we don't care about / want to be
       logically false. */
    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.qp_state = IBV_QPS_INIT;
    qp_attr.port_num = module->port_num;

    if (ibv_modify_qp(module->qp, &qp_attr,
                      IBV_QP_STATE | IBV_QP_PORT)) {
        BTL_ERROR(("error modifying QP to INIT: %s", strerror(errno)));
        ibv_destroy_qp(module->qp);
        module->qp = NULL;
        return OMPI_ERR_TEMP_OUT_OF_RESOURCE;
    }

    return OMPI_SUCCESS;
}


static int move_qp_to_rtr(ompi_btl_usnic_module_t* module)
{
    struct ibv_qp_attr qp_attr;

    memset(&qp_attr, 0, sizeof(qp_attr));

    qp_attr.qp_state = IBV_QPS_RTR;
    if (ibv_modify_qp(module->qp, &qp_attr, IBV_QP_STATE)) {
        /* JMS handle error better */
        opal_output(0, "***************************************BARF on modify qp RTR");
        return OMPI_ERROR;
    }

    return OMPI_SUCCESS;
}


static int move_qp_to_rts(ompi_btl_usnic_module_t* module)
{
    struct ibv_qp_attr qp_attr;

    memset(&qp_attr, 0, sizeof(qp_attr));

    qp_attr.qp_state = IBV_QPS_RTS;
    if (ibv_modify_qp(module->qp, &qp_attr, IBV_QP_STATE)) {
        /* JMS handle error better */
        opal_output(0, "***************************************BARF on modify qp RTS");
        return OMPI_ERROR;
    }

    return OMPI_SUCCESS;
}


/*
 * Initialize the btl module by allocating a protection domain,
 *  memory pool, completion queue, and free lists
 */
int ompi_btl_usnic_module_init(ompi_btl_usnic_module_t *module)
{
    int rc;
    struct mca_mpool_base_resources_t mpool_resources;
    struct ibv_context *ctx = module->device_context;
    uint32_t length_payload;
    ompi_btl_usnic_frag_t* frag;
    struct ibv_recv_wr* bad_wr;
    ompi_free_list_item_t* item;
    int32_t i;

    module->sd_wqe = module->sd_num;

    /* Get a PD */
    module->pd = ibv_alloc_pd(ctx);
    if (NULL == module->pd) {
        BTL_ERROR(("error allocating PD for %s: %s\n",
                   ibv_get_device_name(module->device), strerror(errno)));
        return OMPI_ERROR;
    }

    /* Setup the mpool */
    mpool_resources.reg_data = (void*)module;
    mpool_resources.sizeof_reg = sizeof(ompi_btl_usnic_reg_t);
    mpool_resources.register_mem = usnic_reg_mr;
    mpool_resources.deregister_mem = usnic_dereg_mr;
    module->super.btl_mpool =
        mca_mpool_base_module_create(mca_btl_usnic_component.usnic_mpool_name,
                                     &module->super, &mpool_resources);
    if (NULL == module->super.btl_mpool) {
        BTL_ERROR(("error creating IB mpool for %s: %s\n",
                   ibv_get_device_name(module->device), strerror(errno)));
        goto dealloc_pd;
    }

    /* Create the completion queue */
    module->cq = ibv_create_cq(ctx, module->cq_num, NULL, NULL, 0);
    if (NULL == module->cq) {
        BTL_ERROR(("error creating CQ for %s: %s\n",
                   ibv_get_device_name(module->device), strerror(errno)));
        goto mpool_destroy;
    }

    /* Set up the QP for this BTL */
    rc = init_qp(module);
    if (OMPI_SUCCESS != rc) {
	if (OMPI_ERR_TEMP_OUT_OF_RESOURCE == rc) {
	    goto qp_destroy;
	} else {
	    goto mpool_destroy;
	}
    }

    /* Place our QP numbers in our local address information */
    module->addr.qp_num = module->qp->qp_num;

    /* Initialize pool of receive fragments */
    module->recv_frags.ctx = module;
    OBJ_CONSTRUCT(&module->recv_frags, ompi_free_list_t);
    length_payload = 
        sizeof(ompi_btl_usnic_protocol_header_t) +
        sizeof(ompi_btl_usnic_btl_header_t) +
        module->super.btl_eager_limit;

    ompi_free_list_init_new(&module->recv_frags,
                            sizeof(ompi_btl_usnic_frag_t),
                            opal_cache_line_size,
                            OBJ_CLASS(ompi_btl_usnic_recv_frag_t),
                            length_payload, 
                            opal_cache_line_size,
                            module->rd_num,
                            module->rd_num,
                            module->rd_num,
                            module->super.btl_mpool);

    /* Post receive descriptors */
    for (i = 0; i < module->rd_num; i++) {
        OMPI_FREE_LIST_GET(&module->recv_frags, item, rc);
        assert(OMPI_SUCCESS == rc);
        frag = (ompi_btl_usnic_frag_t*)item;
        FRAG_STATE_CLR(frag, FRAG_RECV_WR_POSTED);

        if (NULL == frag) {
            BTL_ERROR(("error getting receive buffer from free list\n"));
            goto obj_destruct;
        }

        frag->sg_entry.length = 
            sizeof(ompi_btl_usnic_protocol_header_t) +
            sizeof(ompi_btl_usnic_btl_header_t) +
            module->super.btl_eager_limit;
        frag->wr_desc.rd_desc.next = NULL;

        if (ibv_post_recv(module->qp, &frag->wr_desc.rd_desc, &bad_wr)) {
            BTL_ERROR(("error posting recv, errno %s\n", strerror(errno)));
            goto obj_destruct;
        }
        FRAG_STATE_SET(frag, FRAG_RECV_WR_POSTED);
        FRAG_HISTORY(frag, "Initial ibv_post_recv");
    }

    if (OMPI_SUCCESS != move_qp_to_rtr(module)) {
        goto obj_destruct;
    }

    /* No more errors anticipated - initialize everything else */
    /* Pending send frags list */
#if RELIABILITY
    OBJ_CONSTRUCT(&module->pending_resend_frags, opal_list_t);
    OBJ_CONSTRUCT(&module->endpoints_that_need_acks, opal_list_t);
#endif

    /* Send frags freelist */
    module->send_frags.ctx = module;
    OBJ_CONSTRUCT(&module->send_frags, ompi_free_list_t);
    ompi_free_list_init_new(&module->send_frags,
                            sizeof(ompi_btl_usnic_frag_t),
                            opal_cache_line_size,
                            OBJ_CLASS(ompi_btl_usnic_send_frag_t),
                            length_payload, 
			    opal_cache_line_size,
                            module->sd_num >> 1,
                            -1,
                            module->sd_num << 2,
                            module->super.btl_mpool);

#if RELIABILITY
    /* ACK frags freelist */
    module->ack_frags.ctx = module;
    OBJ_CONSTRUCT(&module->ack_frags, ompi_free_list_t);
    ompi_free_list_init_new(&module->ack_frags,
                            sizeof(ompi_btl_usnic_frag_t),
                            opal_cache_line_size,
                            OBJ_CLASS(ompi_btl_usnic_ack_frag_t),
                            length_payload, 
			    opal_cache_line_size,
                            module->sd_num >> 1,
                            -1,
                            module->sd_num << 2,
                            module->super.btl_mpool);
#endif

    if (OMPI_SUCCESS != move_qp_to_rts(module)) {
        goto obj_destruct;
    }

    module->all_endpoints = NULL;
    module->num_endpoints = 0;
    if (mca_btl_usnic_component.stats_enabled) {
        usnic_stats_reset(module);

        /* JMS Redundant for now */
        module->pml_module_sends = 
            module->pml_module_sends_deferred =
            module->pml_send_callbacks = 0;

        module->final_stats = false;
        module->stats_timeout.tv_sec = mca_btl_usnic_component.stats_frequency;
        module->stats_timeout.tv_usec = 0;

        /* Limited to first MAX_STATS_NUM_ENDPOINTS endpoints (for now?) */
        module->all_endpoints = 
            calloc(MAX_STATS_NUM_ENDPOINTS, sizeof(ompi_btl_usnic_endpoint_t*));

        opal_event_set(&(module->stats_timer_event),
                       -1, EV_TIMEOUT | EV_PERSIST, usnic_stats_callback, module);
        opal_event_add(&(module->stats_timer_event),
                       &(module->stats_timeout));
    }

    return OMPI_SUCCESS;

obj_destruct:
    OBJ_DESTRUCT(&module->recv_frags);

qp_destroy:
    ibv_destroy_qp(module->qp);

mpool_destroy:
    mca_mpool_base_module_destroy(module->super.btl_mpool);

dealloc_pd:
    ibv_dealloc_pd(module->pd);
    return OMPI_ERROR;
}


static int usnic_ft_event(int state) 
{
    return OMPI_SUCCESS;
}


ompi_btl_usnic_module_t ompi_btl_usnic_module_template = {
    {
        &mca_btl_usnic_component.super,
        0, /* eager_limit */
        0, /* min_send_size */
        0, /* max_send_size */
        0, /* rdma_pipeline_send_length */
        0, /* rdma_pipeline_frag_size */
        0, /* min_rdma_pipeline_size */
        MCA_BTL_EXCLUSIVITY_DEFAULT,
        0, /* latency */
        0, /* bandwidth */
        MCA_BTL_FLAGS_SEND,
        usnic_add_procs,
        usnic_del_procs,
        NULL, /*ompi_btl_usnic_register*/
        usnic_finalize,
        usnic_alloc,
        usnic_free,
        usnic_prepare_src,
        NULL, /*ompi_btl_usnic_prepare_dst */
        usnic_send,
        /* JMS write ompi_btl_usnic_sendi */ NULL,
        NULL, /*ompi_btl_usnic_put */
        NULL, /*ompi_btl_usnic_get */
        mca_btl_base_dump,
        NULL, /* mpool -- to be filled in later */
        NULL, /* register error */
        usnic_ft_event
    }
};
