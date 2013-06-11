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
#include <unistd.h>

#include "opal/class/opal_bitmap.h"
#include "opal/prefetch.h"
#include "opal/util/output.h"
#include "opal/datatype/opal_convertor.h"
#include "opal/include/opal_stdint.h"

#include "orte/util/show_help.h"
#include "orte/mca/errmgr/errmgr.h"

#include "ompi/mca/btl/btl.h"
#include "ompi/mca/btl/base/btl_base_error.h"
#include "ompi/mca/mpool/base/base.h"
#include "ompi/mca/mpool/mpool.h"
#include "ompi/memchecker.h"

#include "btl_usnic.h"
#include "btl_usnic_frag.h"
#include "btl_usnic_proc.h"
#include "btl_usnic_endpoint.h"
#include "btl_usnic_module.h"
#include "btl_usnic_util.h"
#include "btl_usnic_send.h"
#include "btl_usnic_ack.h"

#define MAX_STATS_NUM_ENDPOINTS 256

static int
ompi_btl_usnic_module_enqueue_frag(
    ompi_btl_usnic_endpoint_t *endpoint,
    ompi_btl_usnic_send_frag_t *frag);

static void
ompi_btl_usnic_channel_finalize(
    ompi_btl_usnic_module_t *module,
    struct ompi_btl_usnic_channel_t *channel);

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
                module->all_endpoints[module->num_endpoints++] = usnic_endpoint;
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
            orte_show_help("help-mpi-btl-usnic.txt", "ibv_FOO failed",
                           true, 
                           orte_process_info.nodename,
                           ibv_get_device_name(module->device), 
                           module->port_num,
                           "ibv_create_ah()", __FILE__, __LINE__,
                           "Failed to create an address handle");
            continue;
        }

        opal_output_verbose(5, mca_btl_base_output,
                            "btl:usnic: new usnic peer: subnet = 0x%016" PRIx64 ", interface = 0x%016" PRIx64,
                            ntoh64(usnic_endpoint->endpoint_remote_addr.gid.global.subnet_prefix),
                            ntoh64(usnic_endpoint->endpoint_remote_addr.gid.global.interface_id));

        opal_bitmap_set_bit(reachable, i);
        endpoints[i] = usnic_endpoint;
        opal_output_verbose(15, mca_btl_base_output,
                            "btl:usnic: made %p endpoint", (void*) usnic_endpoint);
        count++;
    }
    opal_output_verbose(5, mca_btl_base_output,
                        "btl:usnic: made %" PRIsize_t " endpoints", count);

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


/*
 * Let the PML register a callback function with me
 */
static int usnic_register_pml_err_cb(struct mca_btl_base_module_t* btl, 
                                     mca_btl_base_module_error_cb_fn_t cbfunc)
{
    ompi_btl_usnic_module_t *module = (ompi_btl_usnic_module_t*) btl;

    module->pml_error_callback = cbfunc;

    return OMPI_SUCCESS;
}

/**
 * Allocate PML control messages or eager frags if BTL does not have
 * INPLACE flag.  To be clear: max it will ever alloc is eager_limit.
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
    ompi_btl_usnic_send_frag_t *frag;
    ompi_btl_usnic_module_t *module = (ompi_btl_usnic_module_t*) btl;
    mca_btl_base_descriptor_t *desc;

    /* will this fit into a small send? */
    if (size <= module->max_frag_payload) {
        ompi_btl_usnic_small_send_frag_t *sfrag;

        sfrag = ompi_btl_usnic_small_send_frag_alloc(module);
        if (NULL == sfrag) {
            return NULL;
        }
        frag = &sfrag->ssf_base;

#if MSGDEBUG2
        opal_output(0, "usnic_alloc: small frag=%p, size=%d\n", sfrag, size);
#endif

    } else {
        ompi_btl_usnic_large_send_frag_t *lfrag;

        lfrag = ompi_btl_usnic_large_send_frag_alloc(module);
        if (NULL == lfrag) {
            return NULL;
        }
        frag = &lfrag->lsf_base;

#if MSGDEBUG2
        opal_output(0, "usnic_alloc: large frag=%p, size=%d\n", lfrag, size);
#endif
abort();
    }

    /* set # of bytes remaining to be ACKed */
    frag->sf_ack_bytes_left = size;

    /* set endpoint */
    frag->sf_endpoint = endpoint;

    /* set up descriptor */
    desc = &frag->sf_base.uf_base;
    desc->des_src[0].seg_len = size;
    desc->des_flags = flags;

    return desc;
}


/**
 * Return a small send fragment
 *
 * Return the send fragment to the appropriate list
 */
static int usnic_free(struct mca_btl_base_module_t* btl,
                        mca_btl_base_descriptor_t* des)
{
    ompi_btl_usnic_frag_t* frag = (ompi_btl_usnic_frag_t*)des;

#if MSGDEBUG1
    opal_output(0, "usnic_free: %p\n", frag);
#endif
    OMPI_FREE_LIST_RETURN(frag->uf_freelist, &(frag->uf_base.super));

    return OMPI_SUCCESS;
}

/*
 * Notes from george:
 *
 * - BTL ALLOC: allocating PML control messages or eager frags if BTL
     does not have INPLACE flag.  To be clear: max it will ever alloc
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
static mca_btl_base_descriptor_t*
usnic_prepare_src(
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
    ompi_btl_usnic_send_frag_t *frag;
    uint32_t payload_len;

    if (!(flags & MCA_BTL_DES_FLAGS_BTL_OWNERSHIP)) {
        opal_output(0, "==================prepare_src: flags: NONE");
    }

    /*
     * if total payload len fits in one segment, use small send, else large
     */
    payload_len = *size + reserve;
    if (payload_len > module->max_frag_payload) {
        ompi_btl_usnic_large_send_frag_t *lfrag;

        /* Get holder for the msg */
        lfrag = ompi_btl_usnic_large_send_frag_alloc(module);
        if (OPAL_UNLIKELY(NULL == lfrag)) {
            return NULL;
        }
        frag = &lfrag->lsf_base;

        /* Save info about the frag */
        lfrag->lsf_size = payload_len;
        lfrag->lsf_cur_offset = 0;
        lfrag->lsf_bytes_left = payload_len;

        /* give PML a place to put its header */
        assert(reserve < sizeof(lfrag->lsf_pml_header));
        lfrag->lsf_base.sf_base.uf_seg.seg_addr.pval = &lfrag->lsf_pml_header;

        /* for now this is only header len, set to full size before 
         * the completion callback.
         */
        lfrag->lsf_base.sf_base.uf_seg.seg_len = reserve;

        /* JMS From Dec 2012 OMPI meeting, Nathan says look at Vader BTL
           -- something about if opal convertor doesn't need buffers, then
           just memmove.  Will save 100ns or so. */
        lfrag->lsf_reserve = reserve;

        if (OPAL_UNLIKELY(opal_convertor_need_buffers(convertor))) {
            lfrag->lsf_convertor = convertor;
        } else {
            lfrag->lsf_convertor = NULL;
            opal_convertor_get_current_pointer (convertor,
                    (void **)&lfrag->lsf_data_ptr);
        }
#if MSGDEBUG2
        opal_output(0, "prep_src: %s large frag %p, size=%d+%d, src=%p\n",
            module->device->name,
            lfrag, reserve, *size,
            lfrag->lsf_base.sf_base.uf_base.des_src[0].seg_addr.pval);
        opal_output(0, "    data_ptr = %p\n", lfrag->lsf_data_ptr);
#endif

    /* small send */
    } else {
        ompi_btl_usnic_small_send_frag_t *sfrag;
        struct iovec iov;
        uint32_t iov_count;
        size_t max_data;
        int rc;

        /* Get holder for the msg */
        sfrag = ompi_btl_usnic_small_send_frag_alloc(module);
        if (OPAL_UNLIKELY(NULL == sfrag)) {
            return NULL;
        }
        frag = &sfrag->ssf_base;

        /* fragment can hold entire packet, copy it in  */

        iov.iov_len = *size;
        iov.iov_base = (IOVBASE_TYPE*)
                (((uint8_t*) sfrag->ssf_base.sf_base.uf_seg.seg_addr.pval)
                 + reserve);
        iov_count = 1;
        max_data = *size;
        rc = opal_convertor_pack(convertor, &iov, &iov_count, &max_data);
        if (OPAL_UNLIKELY(rc < 0)) {
            ompi_btl_usnic_send_frag_return_cond(module, frag);
            return NULL;
        }

        /* actual length */
        payload_len = reserve + max_data;
        sfrag->ssf_base.sf_base.uf_seg.seg_len = payload_len;
        *size = max_data;

#if MSGDEBUG2
        opal_output(0, "prep_src: %s small frag %p, size=%d, res=%d(%d)\n",
            module->device->name,
            sfrag, *size, reserve, sizeof(mca_btl_base_header_t));
#endif
    }

    /* set up common parts of frag */
    frag->sf_base.uf_base.des_flags = flags;
    frag->sf_endpoint = endpoint;

    /* fragment accounting */
    frag->sf_ack_bytes_left = payload_len;
    frag->sf_seg_post_cnt = 0;

    return &frag->sf_base.uf_base;
}

static mca_btl_base_descriptor_t*
usnic_prepare_dst(
    struct mca_btl_base_module_t* base_module,
    struct mca_btl_base_endpoint_t* endpoint,
    struct mca_mpool_base_registration_t* registration,
    struct opal_convertor_t* convertor,
    uint8_t order,
    size_t reserve,
    size_t* size,
    uint32_t flags)
{
    ompi_btl_usnic_put_dest_frag_t *pfrag;
    ompi_btl_usnic_module_t *module;
    void *data_ptr;

    module = (ompi_btl_usnic_module_t *)base_module;

    /* allocate a fragment for this */
    pfrag = (ompi_btl_usnic_put_dest_frag_t *)
        ompi_btl_usnic_put_dest_frag_alloc(module);
    if (NULL == pfrag) {
        return NULL;
    }

    /* find start of the data */
    opal_convertor_get_current_pointer (convertor, (void **) &data_ptr);

    /* make a seg entry pointing at data_ptr */
    pfrag->uf_seg.seg_addr.pval = data_ptr;
    pfrag->uf_seg.seg_len = *size;

    pfrag->uf_base.order       = order;
    pfrag->uf_base.des_flags   = flags;

#if MSGDEBUG1
    opal_output(0, "prep_dst size=%d, addr=%p, pfrag=%p\n", *size,
            data_ptr, pfrag);
#endif

    return &pfrag->uf_base;
}

/*
 * Emulate an RDMA put.  We'll send the remote address
 * across to the other side so it will know where to put the data
 */
static int
usnic_put(
    struct mca_btl_base_module_t *btl,
    struct mca_btl_base_endpoint_t *endpoint,
    struct mca_btl_base_descriptor_t *des)
{
    ompi_btl_usnic_send_frag_t *frag;

    frag = (ompi_btl_usnic_send_frag_t *)des;
    frag->sf_dest_addr = des->des_dst->seg_addr.pval;

#if MSGDEBUG2
    opal_output(0, "usnic_put: %d bytes to %p\n", 
            des->des_dst->seg_len,
            des->des_dst->seg_addr.pval);
#endif

    ompi_btl_usnic_module_enqueue_frag(endpoint, frag);
    return OMPI_SUCCESS;
}

static inline void usnic_stats_reset(ompi_btl_usnic_module_t *module)
{
    module->num_total_sends =
        module->num_resends =
        module->num_chunk_sends =
        module->num_frag_sends =
        module->num_ack_recvs =
        
        module->num_total_recvs =
        module->num_unk_recvs =
        module->num_dup_recvs =
        module->num_oow_low_recvs =
        module->num_oow_high_recvs =
        module->num_frag_recvs =
        module->num_chunk_recvs =
        module->num_badfrag_recvs =
        module->num_ack_sends =
        module->num_recv_reposts =

        module->num_old_dup_acks =
        module->num_dup_acks =
        module->num_fast_retrans =

        module->cmd_channel.num_channel_sends =
        module->data_channel.num_channel_sends =
        
        module->max_sent_window_size =
        module->max_rcvd_window_size = 

        module->pml_module_sends = 
        module->pml_send_callbacks = 

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
    snprintf(str, sizeof(str), "%s:MCW:%3u, ST(P+D)/F/C/R/A:%8lu(%8u+%8u)/%8lu/%8lu/%4lu/%8lu, RcvTot/Chk/F/C/L/H/D/BF/A:%8lu/%c%c/%8lu/%8lu/%4lu+%2lu/%4lu/%4lu/%6lu OA/DA/FR %4lu/%4lu/%4lu ",
             tmp,
             ompi_proc_local()->proc_name.vpid,

             module->num_total_sends,
             module->cmd_channel.num_channel_sends,
             module->data_channel.num_channel_sends,
             module->num_frag_sends,
             module->num_chunk_sends,
             module->num_resends,
             module->num_ack_sends,
             
             module->num_total_recvs,
             (module->num_total_recvs - module->num_recv_reposts) == 0 ? 'g' : 'B',
             (module->num_total_recvs -
              module->num_frag_recvs -
              module->num_chunk_recvs -
              module->num_badfrag_recvs -
              module->num_oow_low_recvs -
              module->num_oow_high_recvs -
              module->num_dup_recvs -
              module->num_ack_recvs -
              module->num_unk_recvs) == 0 ? 'g' : 'B',
             module->num_frag_recvs,
             module->num_chunk_recvs,
             module->num_oow_low_recvs,
             module->num_oow_high_recvs,
             module->num_dup_recvs,
             module->num_badfrag_recvs,
             module->num_ack_recvs,
             
             module->num_old_dup_acks,
             module->num_dup_acks,
             module->num_fast_retrans);

    /* If our PML calls were 0, then show send and receive window
       extents instead */
    if (1 || module->pml_module_sends + 
        module->pml_send_callbacks == 0) {
        int i;
        int64_t send_unacked, su_min = WINDOW_SIZE * 2, su_max = 0;
        int64_t recv_depth, rd_min = WINDOW_SIZE * 2, rd_max = 0;
        ompi_btl_usnic_endpoint_t *endpoint;

        rd_min = su_min = WINDOW_SIZE * 2;
        rd_max = su_max = 0;
        for (i = 0; i < module->num_endpoints; ++i) {
            endpoint = module->all_endpoints[i];

            /* Number of un-acked sends (i.e., sends for which we're
               still waiting for ACK) */
            send_unacked = 
                endpoint->endpoint_next_seq_to_send - 
                endpoint->endpoint_ack_seq_rcvd - 1;
            if (send_unacked > su_max) su_max = send_unacked;
            if (send_unacked < su_min) su_min = send_unacked;

            /* Receive window depth (i.e., difference between highest
               seq received and the next message we haven't ACKed
               yet) */
            recv_depth = 
                endpoint->endpoint_highest_seq_rcvd -
                endpoint->endpoint_next_contig_seq_to_recv;
            if (recv_depth > rd_max) rd_max = recv_depth;
            if (recv_depth < rd_min) rd_min = recv_depth;
        }
        snprintf(tmp, sizeof(tmp), "PML S:%1ld, Win!A/R:%4ld/%4ld %4ld/%4ld",
                 module->pml_module_sends,
                 su_min, su_max,
                 rd_min, rd_max);
    } else {
        snprintf(tmp, sizeof(tmp), "PML S/CB/Diff:%4lu/%4lu=%4ld",
                module->pml_module_sends,
                module->pml_send_callbacks,
                module->pml_module_sends - module->pml_send_callbacks);
    }

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
    int i;

    if (module->device_async_event_active) {
        opal_event_del(&(module->device_async_event));
        module->device_async_event_active = false;
    }

    ompi_btl_usnic_channel_finalize(module, &module->data_channel);
    ompi_btl_usnic_channel_finalize(module, &module->cmd_channel);
    ibv_dealloc_pd(module->pd);
    
    /* Disable the stats callback event, and then call the stats
       callback manually to display the final stats */
    if (mca_btl_usnic_component.stats_enabled) {
        opal_event_del(&(module->stats_timer_event));
        module->final_stats = true;
        usnic_stats_callback(0, 0, module);
    }

    for (i=0; i<module->num_endpoints; ++i) {
        OBJ_DESTRUCT(module->all_endpoints[i]);
    }
    if (NULL != module->all_endpoints) {
        free(module->all_endpoints);
    }

    OBJ_DESTRUCT(&module->endpoints_that_need_acks);
    OBJ_DESTRUCT(&module->ack_segs);
    OBJ_DESTRUCT(&module->endpoints_with_sends);
    OBJ_DESTRUCT(&module->small_send_frags);
    OBJ_DESTRUCT(&module->large_send_frags);
    OBJ_DESTRUCT(&module->put_dest_frags);
    OBJ_DESTRUCT(&module->senders);
    mca_mpool_base_module_destroy(module->super.btl_mpool);
    ompi_btl_usnic_proc_finalize();

    return OMPI_SUCCESS;
}

/*
 * Progress the send engine.
 */
void
ompi_btl_usnic_module_progress_sends(
    ompi_btl_usnic_module_t *module)
{
    ompi_btl_usnic_send_frag_t *frag;
    ompi_btl_usnic_send_segment_t *sseg;
    ompi_btl_usnic_endpoint_t *endpoint;
    struct ompi_btl_usnic_channel_t *data_channel;
    struct ompi_btl_usnic_channel_t *cmd_channel;
    struct iovec iov;
    uint32_t iov_count;
    size_t max_data;
    int ret;

    /*
     * Post all the sends that we can 
     * resends 1st priority
     * ACKs 2nd priority
     * new sends 3rd
     */
    data_channel = &module->data_channel;
    cmd_channel = &module->cmd_channel;

    /*
     * Handle all the retransmits we can
     */
    while (((size_t)data_channel->sd_wqe > 0) &&
           !opal_list_is_empty(&module->pending_resend_segs)) {

        /*
         * If a segment is on the re-send list, it will not
         * be in the retransmit hotel.  Post the segment, then check it in.
         */
        sseg = (ompi_btl_usnic_send_segment_t *)
            opal_list_remove_first(&module->pending_resend_segs);
        endpoint = sseg->ss_parent_frag->sf_endpoint;

        /* clobber any stale piggy-backed ACK */
        sseg->ss_base.us_btl_header->ack_seq = 0;

        /* If this send is still posted, do not repost */
        if (!sseg->ss_send_posted) {

            /* resends are always standard segments */
            sseg->ss_channel = USNIC_DATA_CHANNEL;
            ompi_btl_usnic_post_segment(module, endpoint, sseg, 0);

            /* consume a send credit for this endpoint.  May send us 
             * negative, oh well...  This is because the completion routine
             * always increments send credits, and we must balance.
             * Alternative is to mark this as a retrans segment and check in
             * completion, but this ugly way avoids extra checks in the
             * critical path.  And, really, respects the concept of send
             * credits more.
             */
            --endpoint->endpoint_send_credits;
        }

        /* restart the retrans timer */
        ret = opal_hotel_checkin(&endpoint->endpoint_hotel,
                sseg, &sseg->ss_hotel_room);
        if (OPAL_UNLIKELY(OPAL_SUCCESS != ret)) {
            abort();    /* should not be possible */
        }

        ++module->num_resends;
    }

    /*
     * As long as the number of WQEs is greater than the number of
     * endpoints needing ACKs, run the regular transmit queue.  This
     * will give the opportunity for ACK piggy-backing
     */
    while (((size_t) data_channel->sd_wqe > 0) &&
           !opal_list_is_empty(&module->endpoints_with_sends)) {
        ompi_btl_usnic_small_send_frag_t *sfrag;
        size_t payload_len;

        /*
         * Grab the first endpoint with a pending send.  Presence on this
         * list means there is a fragment with data ready to go and
         * the endpoint's send window is open, and the endpoint has send
         * credits.
         */

        endpoint = (ompi_btl_usnic_endpoint_t *)
            opal_list_get_first(&module->endpoints_with_sends);
        frag = (ompi_btl_usnic_send_frag_t *)
            opal_list_get_first(&endpoint->endpoint_frag_send_queue);

        /* small send?  (fragment fits in one segment) */
        if (frag->sf_base.uf_type == OMPI_BTL_USNIC_FRAG_SMALL_SEND) {
            sfrag = (ompi_btl_usnic_small_send_frag_t *)frag;
            sseg = &sfrag->ssf_segment;

            /* get payload len from segment */
            payload_len = sfrag->ssf_base.sf_base.uf_seg.seg_len;
            sseg->ss_base.us_btl_header->payload_len = payload_len;

            /* insert tag */
            sseg->ss_base.us_payload.pml_header->tag =
                sfrag->ssf_base.sf_tag;

            /* set actual packet length for verbs */
            sseg->ss_base.us_sg_entry.length =
                sizeof(ompi_btl_usnic_btl_header_t) + payload_len;
#if MSGDEBUG1
            opal_output(0, "send small, ptr=%p, payload=%d, len=%d\n",
                    sseg->ss_base.us_sg_entry.addr, payload_len,
                    sseg->ss_base.us_sg_entry.length);
#endif

            /* post the send */
            ompi_btl_usnic_endpoint_send_segment(module, sseg);

            /* don't to callback yet if this is a put */
            if (frag->sf_dest_addr == NULL) {
#if MSGDEBUG1
                opal_output(0, "    calling back %p, len=%d\n",
                        frag->sf_base.uf_base.des_cbfunc,
                        frag->sf_base.uf_seg.seg_len);
#endif
                /* we have copied the data, proceed with callback */
                /* could be done in usnic_send? XXX */
                frag->sf_base.uf_base.des_cbfunc(&module->super,
                              frag->sf_endpoint, &frag->sf_base.uf_base,
                              OMPI_SUCCESS);
                ++module->pml_send_callbacks;
            }

            /* remove frag from sending list */
            opal_list_remove_item(&endpoint->endpoint_frag_send_queue,
                    &sfrag->ssf_base.sf_base.uf_base.super.super);

        /* Large sends... */
        } else {
            ompi_btl_usnic_large_send_frag_t *lfrag;
            ompi_btl_usnic_btl_chunk_header_t *chp;
            size_t payload_len;

            endpoint = frag->sf_endpoint;
            lfrag = (ompi_btl_usnic_large_send_frag_t *)frag;

            sseg = ompi_btl_usnic_chunk_segment_alloc(module);
            if (OPAL_UNLIKELY(NULL == sseg)) {
                /* XXX do something better here */
                abort();
            }
#if MSGDEBUG1
            opal_output(0, "send large, frag=%p, addr=%p\n", lfrag, sseg->ss_base.us_payload.raw);
#endif

            /* save back pointer to fragment */
            sseg->ss_parent_frag = frag;

            /* If this is the first chunk of the frag, need to insert
             * the PML header at the start.  On subsequent chunks,
             * skip the PML header
             */
            if (lfrag->lsf_cur_offset == 0) {

                /* insert tag */
                sseg->ss_base.us_payload.pml_header->tag =
                    lfrag->lsf_base.sf_tag;

                /* copy in the PML header */
                memcpy(sseg->ss_base.us_payload.raw, lfrag->lsf_pml_header,
                        lfrag->lsf_reserve);

                /* adjust data pointer and len to skip PML */
                iov.iov_base = sseg->ss_base.us_payload.raw +
                    lfrag->lsf_reserve;

                payload_len = lfrag->lsf_reserve;
            } else {
                iov.iov_base = sseg->ss_base.us_payload.raw;
                payload_len = 0;
            }

            /* payload_len is how much payload we have already used up
             * with header.  max_data is the amount of further data we
             * can put into this segment
             */
            max_data = lfrag->lsf_bytes_left;
            if (max_data > module->max_chunk_payload) {
                max_data = module->max_chunk_payload;
            }
            max_data -= payload_len;

            /* fill in destination for pack */
            iov.iov_len = max_data;
            iov_count = 1;

            /* pack the next bit of data */
            if (OPAL_UNLIKELY(lfrag->lsf_convertor != NULL)) {
                ret = opal_convertor_pack(lfrag->lsf_convertor,
                        &iov, &iov_count, &max_data);
#if MSGDEBUG1
                opal_output(0, "  converted %d/%d bytes into %p, ret=%d\n",
                        max_data, iov.iov_len, iov.iov_base, ret);
#endif
                if (OPAL_UNLIKELY(ret < 0)) {
                    /* XXX do something better here */
                    abort();
                }
            } else {
                memcpy(iov.iov_base, lfrag->lsf_data_ptr, max_data);
                lfrag->lsf_data_ptr += max_data;
            }

            /* get real payload length */
            payload_len += max_data;

            /* fill in BTL header with frag info */
            chp = sseg->ss_base.us_btl_chunk_header;
            chp->ch_frag_id = lfrag->lsf_frag_id;
            chp->ch_frag_size = lfrag->lsf_size;
            chp->ch_frag_offset = lfrag->lsf_cur_offset;

            /* set actual packet length for verbs */
            sseg->ss_base.us_sg_entry.length =
                sizeof(ompi_btl_usnic_btl_chunk_header_t) + payload_len;

            /* payload length into the header*/
            sseg->ss_base.us_btl_header->payload_len = payload_len;

            /* do the send */
            ompi_btl_usnic_endpoint_send_segment(module, sseg);

            /* do fragment bookkeeping */
            lfrag->lsf_cur_offset += payload_len;
            lfrag->lsf_bytes_left -= payload_len;

#if MSGDEBUG1
            opal_output(0, "payload_len = %d, bytes_left=%d\n",
                    payload_len, lfrag->lsf_bytes_left);
#endif
            /* done with fragment? */
            if (lfrag->lsf_bytes_left == 0) {

                /* only callback now if this was not a PUT, otherwise
                 * we need to wait until last byte is ACKed
                 */
                if (frag->sf_dest_addr == NULL) {
#if MSGDEBUG1
                    opal_output(0, "    calling back %p, len=%d\n",
                            frag->sf_base.uf_base.des_cbfunc,
                            frag->sf_base.uf_seg.seg_len);
#endif
                    /* large frags need desc to have full frag len for CB */
                    lfrag->lsf_base.sf_base.uf_seg.seg_len = lfrag->lsf_size;
                    frag->sf_base.uf_base.des_cbfunc(&module->super,
                                  frag->sf_endpoint, &frag->sf_base.uf_base,
                                  OMPI_SUCCESS);
                    ++module->pml_send_callbacks;
                }

                /* remove frag from send queue */
                opal_list_remove_item(&endpoint->endpoint_frag_send_queue,
                        &frag->sf_base.uf_base.super.super);
            }
        }

        /* If no more sends or endpoint send window is closed,
         * or no more send credits, remove from send list
         */
        if (opal_list_is_empty(&endpoint->endpoint_frag_send_queue) ||
            endpoint->endpoint_send_credits <= 0 ||
            !WINDOW_OPEN(endpoint)) {

            opal_list_remove_item(&module->endpoints_with_sends,
                    &endpoint->super);
            endpoint->endpoint_ready_to_send = false;
        }

    }

    /*
     * Handle any ACKs that need to be sent
     */
    endpoint = ompi_btl_usnic_get_first_endpoint_needing_ack(module);
    while (((size_t)cmd_channel->sd_wqe) > 0 && endpoint != NULL) {
        ompi_btl_usnic_endpoint_t *next_endpoint;

        /* get next in list */
        next_endpoint = ompi_btl_usnic_get_next_endpoint_needing_ack(endpoint);

        /* Is it time to send ACK? */
        if (endpoint->endpoint_acktime == 0 ||
            endpoint->endpoint_acktime <= get_nsec()) {
            ompi_btl_usnic_ack_send(module, endpoint);
            ompi_btl_usnic_remove_from_endpoints_needing_ack(endpoint);
        }

        endpoint = next_endpoint;
    }
}

/*
 * This enqueues a fragment send into the system.  A send of a fragment
 * may result in the sending of multiple segments
 */
static int
ompi_btl_usnic_module_enqueue_frag(
    ompi_btl_usnic_endpoint_t *endpoint,
    ompi_btl_usnic_send_frag_t *frag)
{
    ompi_btl_usnic_module_t *module;
    ompi_btl_usnic_large_send_frag_t *lfrag;

    module = endpoint->endpoint_module;
#if MSGDEBUG1
    opal_output(0, "enq_frag: frag=%p, endpoint=%p, type=%d, len=%d\n",
            frag, endpoint, frag->sf_base.uf_type,
            frag->sf_base.uf_base.des_src->seg_len);
    if (frag->sf_base.uf_type == OMPI_BTL_USNIC_FRAG_LARGE_SEND) {
        lfrag = (ompi_btl_usnic_large_send_frag_t *)frag;
        opal_output(0, "   large size=%d\n", lfrag->lsf_size);
    }
#endif

    /* If this is a large send, we need to allocate a fragment_id
     * for it so that the pieces can be reassembled on the remote side
     */
    if (frag->sf_base.uf_type == OMPI_BTL_USNIC_FRAG_LARGE_SEND) {
        lfrag = (ompi_btl_usnic_large_send_frag_t *)frag;
        do {
            lfrag->lsf_frag_id = endpoint->endpoint_next_frag_id++;
        } while (lfrag->lsf_frag_id == 0);
    }

    /* add to tail of in-progress list */
    opal_list_append(&endpoint->endpoint_frag_send_queue,
            &frag->sf_base.uf_base.super.super);

    /* possibly make this endpoint ready to send again */
    ompi_btl_usnic_check_rts(endpoint);
    
    /* post sends now if space available */
    /* XXX ? */
    ompi_btl_usnic_module_progress_sends(module);

    return OMPI_SUCCESS;
}


/*
 *  Initiate a send.
 */
static int usnic_send(struct mca_btl_base_module_t* module,
                        struct mca_btl_base_endpoint_t* base_endpoint,
                        struct mca_btl_base_descriptor_t* descriptor,
                        mca_btl_base_tag_t tag)
{
    int rc;
    ompi_btl_usnic_send_frag_t *frag;
    ompi_btl_usnic_endpoint_t *endpoint;

    endpoint = (ompi_btl_usnic_endpoint_t *)base_endpoint;
    frag = (ompi_btl_usnic_send_frag_t*) descriptor;
#if MSGDEBUG2
    opal_output(0, "usnic_send: frag=%p, endpoint=%p, tag=%d, sendreq=%p\n",
            frag, endpoint, tag, descriptor->des_cbdata);
    opal_output(0, "   data = %p\n", descriptor->des_src[0].seg_addr.pval);
#endif

    assert(frag->sf_endpoint == endpoint);
    frag->sf_tag = tag;
    frag->sf_dest_addr = NULL;      /* not a PUT */

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

    /* Stats */
    ++(((ompi_btl_usnic_module_t*)module)->pml_module_sends);

    descriptor->des_flags |= MCA_BTL_DES_SEND_ALWAYS_CALLBACK;

    /* queue this fragment into the send engine */
    rc = ompi_btl_usnic_module_enqueue_frag(endpoint, frag);

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
 * Called back by libevent if an async event occurs on the device
 */
static void module_async_event_callback(int fd, short flags, void *arg)
{
    bool got_event = false;
    bool fatal = false;
    ompi_btl_usnic_module_t *module = (ompi_btl_usnic_module_t*) arg;
    struct ibv_async_event event;

    /* Get the async event */
    if (0 != ibv_get_async_event(module->device_context, &event)) {
        /* This shouldn't happen.  If it does, treat this as a fatal
           error. */
        fatal = true;
    } else {
        got_event = true;
    }

    /* Process the event */
    if (got_event) {
        switch (event.event_type) {
            /* For the moment, these are the only cases usnic_verbs.ko
               will report to us.  However, they're only listed here
               for completeness.  We currently abort if any async
               event occurs. */
        case IBV_EVENT_QP_FATAL:
        case IBV_EVENT_PORT_ERR:
        case IBV_EVENT_PORT_ACTIVE:
#if BTL_USNIC_HAVE_IBV_EVENT_GID_CHANGE
        case IBV_EVENT_GID_CHANGE:
#endif
        default:
            orte_show_help("help-mpi-btl-usnic.txt", "async event",
                           true, 
                           orte_process_info.nodename,
                           ibv_get_device_name(module->device), 
                           module->port_num,
                           ibv_event_type_str(event.event_type),
                           event.event_type);
            module->pml_error_callback(&module->super, 
                                       MCA_BTL_ERROR_FLAGS_FATAL,
                                       ompi_proc_local(), "usnic");
            /* The PML error callback will likely not return (i.e., it
               will likely kill the job).  But in case someone
               implements a non-fatal PML error callback someday, do
               reasonable things just in case it does actually
               return. */
            fatal = true;
            break;
        }

        /* Ack the event back to verbs */
        ibv_ack_async_event(&event);
    }

    /* If this is fatal, invoke the upper layer error handler to abort
       the job */
    if (fatal) {
        orte_errmgr.abort(1, NULL);
        /* If this returns, wait to be killed */
        while (1) {
            sleep(99999);
        }
    }
}

/*
 * Create a single UD queue pair.
 */
static int
init_qp(
    ompi_btl_usnic_module_t* module,
    struct ompi_btl_usnic_channel_t *channel)
{
    struct ibv_qp_attr qp_attr;
    struct ibv_qp_init_attr qp_init_attr;

    /* memset to both silence valgrind warnings (since the attr struct
       ends up getting written down an fd to the kernel) and actually
       zero out all the fields that we don't care about / want to be
       logically false. */
    memset(&qp_init_attr, 0, sizeof(qp_init_attr));

    qp_init_attr.send_cq = channel->cq;
    qp_init_attr.recv_cq = channel->cq;
    qp_init_attr.cap.max_send_wr = module->sd_num;
    qp_init_attr.cap.max_recv_wr = module->rd_num;
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1;
    qp_init_attr.qp_type = IBV_QPT_UD;
    
    channel->qp = ibv_create_qp(module->pd, &qp_init_attr);
    if (NULL == channel->qp) {
        orte_show_help("help-mpi-btl-usnic.txt", "ibv_FOO failed",
                       true, 
                       orte_process_info.nodename,
                       ibv_get_device_name(module->device), 
                       module->port_num,
                       "ibv_create_qp()", __FILE__, __LINE__,
                       "Failed to create a USNIC QP; check CIMC/UCSM to ensure enough USNIC devices are provisioned");
        return OMPI_ERR_OUT_OF_RESOURCE;
    }

    /* memset to both silence valgrind warnings (since the attr struct
       ends up getting written down an fd to the kernel) and actually
       zero out all the fields that we don't care about / want to be
       logically false. */
    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.qp_state = IBV_QPS_INIT;
    qp_attr.port_num = module->port_num;

    if (ibv_modify_qp(channel->qp, &qp_attr,
                      IBV_QP_STATE | IBV_QP_PORT)) {
        orte_show_help("help-mpi-btl-usnic.txt", "ibv_FOO failed",
                       true, 
                       orte_process_info.nodename,
                       ibv_get_device_name(module->device),
                       module->port_num,
                       "ibv_modify_qp()", __FILE__, __LINE__,
                       "Failed to modify an existing QP");
        return OMPI_ERR_TEMP_OUT_OF_RESOURCE;
    }

    /* Find the max inline size */
    memset(&qp_attr, 0, sizeof(qp_attr));
    memset(&qp_init_attr, 0, sizeof(qp_init_attr));
    if (ibv_query_qp(channel->qp, &qp_attr, IBV_QP_CAP,
                     &qp_init_attr) != 0) {
        orte_show_help("help-mpi-btl-usnic.txt", "ibv_FOO failed",
                       true, 
                       orte_process_info.nodename,
                       ibv_get_device_name(module->device), 
                       module->port_num,
                       "ibv_query_qp()", __FILE__, __LINE__,
                       "Failed to query an existing QP");
        return OMPI_ERR_TEMP_OUT_OF_RESOURCE;
    }

    /* only grab this once */
    if (channel == &module->data_channel) {
        module->qp_max_inline = qp_attr.cap.max_inline_data;
    }

    return OMPI_SUCCESS;
}


static int move_qp_to_rtr(struct ompi_btl_usnic_channel_t *channel)
{
    struct ibv_qp_attr qp_attr;

    memset(&qp_attr, 0, sizeof(qp_attr));

    qp_attr.qp_state = IBV_QPS_RTR;
    if (ibv_modify_qp(channel->qp, &qp_attr, IBV_QP_STATE)) {
        /* JMS handle error better */
        opal_output(0, "***************************************BARF on modify qp RTR");
        return OMPI_ERROR;
    }

    return OMPI_SUCCESS;
}


static int move_qp_to_rts(struct ompi_btl_usnic_channel_t *channel)
{
    struct ibv_qp_attr qp_attr;

    memset(&qp_attr, 0, sizeof(qp_attr));

    qp_attr.qp_state = IBV_QPS_RTS;
    if (ibv_modify_qp(channel->qp, &qp_attr, IBV_QP_STATE)) {
        /* JMS handle error better */
        opal_output(0, "***************************************BARF on modify qp RTS");
        return OMPI_ERROR;
    }

    return OMPI_SUCCESS;
}

/*
 * finalize channel - release all associated resources
 */
static void
ompi_btl_usnic_channel_finalize(
    ompi_btl_usnic_module_t *module,
    struct ompi_btl_usnic_channel_t *channel)
{
    /* gets set right after constructor called, lets us know recv_segs
     * have been constructed
    */
    if (channel->recv_segs.ctx == module) {
        OBJ_DESTRUCT(&channel->recv_segs);
    }

    if (NULL == channel->qp) {
        ibv_destroy_qp(channel->qp);
        channel->qp = NULL;
    }

    /* destroy CQ if created */
    if (NULL != channel->cq) {
        ibv_destroy_cq(channel->cq);
        channel->cq = NULL;
    }
}

/*
 * Initialize a channel
 */
static int
ompi_btl_usnic_channel_init(
    ompi_btl_usnic_module_t *module,
    struct ompi_btl_usnic_channel_t *channel)
{
    struct ibv_context *ctx;
    ompi_btl_usnic_recv_segment_t *rseg;
    ompi_free_list_item_t* item;
    struct ibv_recv_wr* bad_wr;
    int i;
    int rc;

    ctx = module->device_context;

    channel->sd_wqe = module->sd_num;

    /* Create the completion queue */
    channel->cq = ibv_create_cq(ctx, module->cq_num, NULL, NULL, 0);
    if (NULL == channel->cq) {
        orte_show_help("help-mpi-btl-usnic.txt", "ibv_FOO failed",
                       true, 
                       orte_process_info.nodename,
                       ibv_get_device_name(module->device),
                       module->port_num,
                       "ibv_create_cq()", __FILE__, __LINE__,
                       "Failed to create a CQ");
        rc = OMPI_ERR_TEMP_OUT_OF_RESOURCE;
        goto error;
    }

    /* Set up the QP for this channel */
    rc = init_qp(module, channel);
    if (OMPI_SUCCESS != rc) {
        goto error;
    }

    /* Initialize pool of receive segments */
    OBJ_CONSTRUCT(&channel->recv_segs, ompi_free_list_t);
    channel->recv_segs.ctx = module;
    ompi_free_list_init_new(&channel->recv_segs,
                            sizeof(ompi_btl_usnic_recv_segment_t),
                            opal_cache_line_size,
                            OBJ_CLASS(ompi_btl_usnic_recv_segment_t),
                            module->if_mtu,
                            opal_cache_line_size,
                            module->rd_num,
                            module->rd_num,
                            module->rd_num,
                            module->super.btl_mpool);

    /* Post receive descriptors */
    for (i = 0; i < module->rd_num; i++) {
        OMPI_FREE_LIST_GET(&channel->recv_segs, item, rc);
        assert(OMPI_SUCCESS == rc);
        rseg = (ompi_btl_usnic_recv_segment_t*)item;

        if (NULL == rseg) {
            orte_show_help("help-mpi-btl-usnic.txt", "internal error",
                           true, 
                           orte_process_info.nodename,
                           ibv_get_device_name(module->device),
                           module->port_num,
                           "get freelist buffer()", __FILE__, __LINE__,
                           "Failed to get receive buffer from freelist");
            abort();    /* this is impossible */
        }

        /* cannot find length from constructor, set it now */
        rseg->rs_base.us_sg_entry.length = module->if_mtu;

        rseg->rs_base.us_sg_entry.length =  module->if_mtu;
        rseg->rs_recv_desc.next = NULL;

        if (ibv_post_recv(channel->qp, &rseg->rs_recv_desc, &bad_wr)) {
            orte_show_help("help-mpi-btl-usnic.txt", "ibv_FOO failed",
                           true, 
                           orte_process_info.nodename,
                           ibv_get_device_name(module->device),
                           module->port_num,
                           "ibv_post_recv", __FILE__, __LINE__,
                           "Failed to post receive buffer");
            goto error;
        }
    }

    if (OMPI_SUCCESS != move_qp_to_rtr(channel)) {
        goto error;
    }

    if (OMPI_SUCCESS != move_qp_to_rts(channel)) {
        goto error;
    }

    return 0;

error:
    ompi_btl_usnic_channel_finalize(module, channel);
    return rc;
}

/*
 * Initialize the btl module by allocating a protection domain,
 *  memory pool, command and data channels, and free lists
 */
int ompi_btl_usnic_module_init(ompi_btl_usnic_module_t *module)
{
    int rc;
    struct mca_mpool_base_resources_t mpool_resources;
    struct ibv_context *ctx = module->device_context;
    uint32_t ack_segment_len;

    /* Get a PD */
    module->pd = ibv_alloc_pd(ctx);
    if (NULL == module->pd) {
        orte_show_help("help-mpi-btl-usnic.txt", "ibv_FOO failed",
                       true, 
                       orte_process_info.nodename,
                       ibv_get_device_name(module->device), 
                       module->port_num,
                       "ibv_alloc_pd()", __FILE__, __LINE__,
                       "Failed to create a PD; is the usnic_verbs Linux kernel module loaded?");
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
        orte_show_help("help-mpi-btl-usnic.txt", "ibv_FOO failed",
                       true, 
                       orte_process_info.nodename,
                       ibv_get_device_name(module->device),
                       module->port_num,
                       "create mpool", __FILE__, __LINE__,
                       "Failed to allocate registered memory");
        goto dealloc_pd;
    }

    /* initialize data and command channels */
    rc = ompi_btl_usnic_channel_init(module, &module->data_channel);
    if (rc == -1) {
        goto chan_destroy;
    }
    rc = ompi_btl_usnic_channel_init(module, &module->cmd_channel);
    if (rc == -1) {
        goto chan_destroy;
    }

    /* Place our QP numbers in our local address information */
    module->local_addr.data_qp_num = module->data_channel.qp->qp_num;
    module->local_addr.cmd_qp_num = module->cmd_channel.qp->qp_num;

    /* Get the fd to receive events on this device */
    opal_event_set(&(module->device_async_event),
                   module->device_context->async_fd, 
                   OPAL_EV_READ | OPAL_EV_PERSIST, 
                   module_async_event_callback, module);
    opal_event_add(&(module->device_async_event), NULL);
    module->device_async_event_active = true;

    /* No more errors anticipated - initialize everything else */

    /* Pending send segs list */
    OBJ_CONSTRUCT(&module->pending_resend_segs, opal_list_t);
    OBJ_CONSTRUCT(&module->endpoints_that_need_acks, opal_list_t);

    /* list of endpoints that are ready to send */
    OBJ_CONSTRUCT(&module->endpoints_with_sends, opal_list_t);

    /* Send frags freelists */
    module->small_send_frags.ctx = module;
    OBJ_CONSTRUCT(&module->small_send_frags, ompi_free_list_t);
    ompi_free_list_init_new(&module->small_send_frags,
                            sizeof(ompi_btl_usnic_small_send_frag_t),
                            opal_cache_line_size,
                            OBJ_CLASS(ompi_btl_usnic_small_send_frag_t),
                            module->if_mtu, 
                            opal_cache_line_size,
                            module->sd_num << 2,
                            -1,
                            module->sd_num >> 1,
                            module->super.btl_mpool);

    module->large_send_frags.ctx = module;
    OBJ_CONSTRUCT(&module->large_send_frags, ompi_free_list_t);
    ompi_free_list_init_new(&module->large_send_frags,
                            sizeof(ompi_btl_usnic_large_send_frag_t),
                            opal_cache_line_size,
                            OBJ_CLASS(ompi_btl_usnic_large_send_frag_t),
                            0,  /* payload size */
                            0,  /* payload align */
                            module->sd_num >> 3,
                            -1,
                            module->sd_num >> 3,
                            NULL);

    module->put_dest_frags.ctx = module;
    OBJ_CONSTRUCT(&module->put_dest_frags, ompi_free_list_t);
    ompi_free_list_init_new(&module->put_dest_frags,
                            sizeof(ompi_btl_usnic_put_dest_frag_t),
                            opal_cache_line_size,
                            OBJ_CLASS(ompi_btl_usnic_put_dest_frag_t),
                            0,  /* payload size */
                            0,  /* payload align */
                            module->sd_num >> 3,
                            -1,
                            module->sd_num >> 3,
                            NULL);

    /* list of segments to use for sending */
    module->chunk_segs.ctx = module;
    OBJ_CONSTRUCT(&module->chunk_segs, ompi_free_list_t);
    ompi_free_list_init_new(&module->chunk_segs,
                            sizeof(ompi_btl_usnic_chunk_segment_t),
                            opal_cache_line_size,
                            OBJ_CLASS(ompi_btl_usnic_chunk_segment_t),
                            module->if_mtu, 
                            opal_cache_line_size,
                            module->sd_num << 2,
                            -1,
                            module->sd_num >> 1,
                            module->super.btl_mpool);

    /* ACK segments freelist */
    module->ack_segs.ctx = module;
    ack_segment_len = sizeof(ompi_btl_usnic_btl_header_t);
    OBJ_CONSTRUCT(&module->ack_segs, ompi_free_list_t);
    ompi_free_list_init_new(&module->ack_segs,
                            sizeof(ompi_btl_usnic_ack_segment_t),
                            opal_cache_line_size,
                            OBJ_CLASS(ompi_btl_usnic_ack_segment_t),
                            ack_segment_len,
                            opal_cache_line_size,
                            module->sd_num << 2,
                            -1,
                            module->sd_num >> 1,
                            module->super.btl_mpool);

    module->all_endpoints = NULL;
    module->num_endpoints = 0;
    if (mca_btl_usnic_component.stats_enabled) {
        usnic_stats_reset(module);

        /* JMS Redundant for now */
        module->pml_module_sends = 
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

chan_destroy:
    ompi_btl_usnic_channel_finalize(module, &module->cmd_channel);   
    ompi_btl_usnic_channel_finalize(module, &module->data_channel);   

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
        MCA_BTL_FLAGS_SEND | 
            MCA_BTL_FLAGS_PUT |
            MCA_BTL_FLAGS_SEND_INPLACE,
        usnic_add_procs,
        usnic_del_procs,
        NULL, /*ompi_btl_usnic_register*/
        usnic_finalize,
        usnic_alloc,
        usnic_free,
        usnic_prepare_src,
        usnic_prepare_dst,
        usnic_send,
        /* JMS write ompi_btl_usnic_sendi */ NULL,
        usnic_put,
        NULL, /*ompi_btl_usnic_get */
        mca_btl_base_dump,
        NULL, /* mpool -- to be filled in later */
        usnic_register_pml_err_cb,
        usnic_ft_event
    }
};
