/*
 * Copyright (c) 2004-2008 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2011 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2006      Sandia National Laboratories. All rights
 *                         reserved.
 * Copyright (c) 2008-2013 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2012      Los Alamos National Security, LLC.  All rights
 *                         reserved. 
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

/*
 * General notes:
 *
 * - OB1 handles out of order receives
 * - OB1 does NOT handle duplicate receives well (it probably does for
 *   MATCH tags, but for non-MATCH tags, it doesn't have enough info
 *   to know when duplicates are received), so we have to ensure not
 *   to pass duplicates up to the PML.
 */

#include "ompi_config.h"

#include <string.h>
#include <errno.h>
#include <infiniband/verbs.h>
#include <unistd.h>

#include "opal_stdint.h"
#include "opal/prefetch.h"
#include "opal/mca/timer/base/base.h"
#include "opal/util/argv.h"
#include "opal/mca/base/mca_base_param.h"
#include "opal/mca/memchecker/base/base.h"
#include "opal/util/show_help.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/runtime/orte_globals.h"

#include "ompi/constants.h"
#include "ompi/mca/btl/btl.h"
#include "ompi/mca/btl/base/base.h"
#include "ompi/runtime/ompi_module_exchange.h"
#include "ompi/runtime/mpiruntime.h"
#include "ompi/proc/proc.h"
#include "common_verbs.h"

#include "btl_usnic.h"
#include "btl_usnic_frag.h"
#include "btl_usnic_endpoint.h"
#include "btl_usnic_util.h"
#include "btl_usnic_ack.h"
#include "btl_usnic_send.h"
#include "btl_usnic_recv.h"
#include "btl_usnic_if.h"

#define OMPI_BTL_USNIC_NUM_WC       500


static int usnic_component_open(void);
static int usnic_component_close(void);
static mca_btl_base_module_t **
usnic_component_init(int* num_btl_modules, bool want_progress_threads,
                       bool want_mpi_threads);
static int usnic_component_progress(void);


ompi_btl_usnic_component_t mca_btl_usnic_component = {
    {
        /* First, the mca_base_component_t struct containing meta information
           about the component itself */
        {
            MCA_BTL_BASE_VERSION_2_0_0,

            "usnic", /* MCA component name */
            OMPI_MAJOR_VERSION,  /* MCA component major version */
            OMPI_MINOR_VERSION,  /* MCA component minor version */
            OMPI_RELEASE_VERSION,  /* MCA component release version */
            usnic_component_open,  /* component open */
            usnic_component_close,  /* component close */
            NULL, /* component query */
            ompi_btl_usnic_component_register, /* component register */
        },
        {
            /* The component is not checkpoint ready */
            MCA_BASE_METADATA_PARAM_NONE
        },

        usnic_component_init,
        usnic_component_progress,
    }
};


/*
 *  Called by MCA framework to open the component
 */
static int usnic_component_open(void)
{
    /* initialize state */
    mca_btl_usnic_component.num_modules = 0;
    mca_btl_usnic_component.usnic_modules = NULL;
    
    /* initialize objects */
    OBJ_CONSTRUCT(&mca_btl_usnic_component.usnic_procs, opal_list_t);
    
    /* Sanity check: if_include and if_exclude need to be mutually
       exclusive */
    if (OPAL_SUCCESS != 
        mca_base_param_check_exclusive_string(
        mca_btl_usnic_component.super.btl_version.mca_type_name,
        mca_btl_usnic_component.super.btl_version.mca_component_name,
        "if_include",
        mca_btl_usnic_component.super.btl_version.mca_type_name,
        mca_btl_usnic_component.super.btl_version.mca_component_name,
        "if_exclude")) {
        /* Return ERR_NOT_AVAILABLE so that a warning message about
           "open" failing is not printed */
        return OMPI_ERR_NOT_AVAILABLE;
    }
    
    return OMPI_SUCCESS;
}


/*
 * Component cleanup 
 */
static int usnic_component_close(void)
{
    OBJ_DESTRUCT(&mca_btl_usnic_component.usnic_procs);

    /* JMS This is only here so that we can use the custom copy of
       opal_if here in the usnic BTL (so that this can remain a
       plugin to an existing v1.6 tree and not require an updated
       opal_if interface in libopen-pal). */
    btl_usnic_opal_iffinalize();

    return OMPI_SUCCESS;
}


/*
 * Register UD address information.  The MCA framework will make this
 * available to all peers.
 */
static int usnic_modex_send(void)
{
    int rc;
    size_t i;
    size_t size;
    ompi_btl_usnic_addr_t* addrs = NULL;

    size = mca_btl_usnic_component.num_modules * 
        sizeof(ompi_btl_usnic_addr_t);
    if (size != 0) {
        addrs = (ompi_btl_usnic_addr_t*) malloc(size);
        if (NULL == addrs) {
            return OMPI_ERR_OUT_OF_RESOURCE;
        }

        for (i = 0; i < mca_btl_usnic_component.num_modules; i++) {
            ompi_btl_usnic_module_t* module = 
                &mca_btl_usnic_component.usnic_modules[i];
            addrs[i] = module->addr;
            BTL_VERBOSE(("modex_send QP num %d, subnet = %" PRIx64,
                         addrs[i].qp_num, 
                         addrs[i].subnet));
        }
    }

    rc = ompi_modex_send(&mca_btl_usnic_component.super.btl_version, 
                         addrs, size);
    if (NULL != addrs) {
        free(addrs);
    }
    return rc;
}


/*
 *  UD component initialization:
 *  (1) read interface list from kernel and compare against component
 *      parameters then create a BTL instance for selected interfaces
 *  (2) post OOB receive for incoming connection attempts
 *  (3) register BTL parameters with the MCA
 */
static mca_btl_base_module_t** usnic_component_init(int* num_btl_modules,
                                                    bool want_progress_threads,
                                                    bool want_mpi_threads)
{
    int flags;
    mca_btl_base_module_t **btls;
    uint32_t i, max_payload;
    ompi_btl_usnic_module_t *module;
    opal_list_item_t *item;
    unsigned short seedv[3];
    opal_list_t *port_list;
    ompi_common_verbs_port_item_t *port;
    struct ibv_device_attr device_attr;
    union ibv_gid gid;

    /* Currently refuse to run if MPI_THREAD_MULTIPLE is enabled */
    if (ompi_mpi_thread_multiple && !mca_btl_base_thread_multiple_override) {
        return NULL;
    }

    /************************************************************************
     * Below this line, we assume that usnic is loaded on all procs,
     * and therefore we will guarantee to the the modex send, even if
     * we fail.
     ************************************************************************/

    /* initialization */
    mca_btl_usnic_component.my_hashed_orte_name = 
        orte_util_hash_name(&(ompi_proc_local()->proc_name));

    *num_btl_modules = 0;

    seedv[0] = ORTE_PROC_MY_NAME->vpid;
    seedv[1] = opal_timer_base_get_cycles();
    usleep(1);
    seedv[2] = opal_timer_base_get_cycles();
    seed48(seedv);

    /* Find the ports that we want to use */
    flags = OMPI_COMMON_VERBS_FLAGS_UD;
    if (!mca_btl_usnic_component.rc_devices_ok) {
        flags |= OMPI_COMMON_VERBS_FLAGS_NOT_RC;
    }
    port_list = 
        ompi_common_verbs_find_ports(mca_btl_usnic_component.if_include,
                                     mca_btl_usnic_component.if_exclude,
                                     flags, mca_btl_base_output);
    if (NULL == port_list || 0 == opal_list_get_size(port_list)) {
        mca_btl_base_error_no_nics("USNIC", "device");
        btls = NULL;
        goto free_include_list;
    }

    /* Setup an array of pointers to point to each module (which we'll
       return upstream) */
    mca_btl_usnic_component.num_modules = opal_list_get_size(port_list);
    btls = (struct mca_btl_base_module_t**)
        malloc(mca_btl_usnic_component.num_modules * 
               sizeof(ompi_btl_usnic_module_t*));
    if (NULL == btls) {
        ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
        btls = NULL;
        goto free_include_list;
    }

    /* Allocate space for btl module instances */
    mca_btl_usnic_component.usnic_modules = (ompi_btl_usnic_module_t*)
        calloc(mca_btl_usnic_component.num_modules,
               sizeof(ompi_btl_usnic_module_t));
    if (NULL == mca_btl_usnic_component.usnic_modules) {
        free(btls);
        ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
        btls = NULL;
        goto free_include_list;
    }

    /* Fill each module */
    for (i = 0, item = opal_list_get_first(port_list);
         item != opal_list_get_end(port_list) &&
             (0 == mca_btl_usnic_component.max_modules ||
              i < mca_btl_usnic_component.max_modules);
         ++i, item = opal_list_get_next(item)) {
        port = (ompi_common_verbs_port_item_t*) item;

        opal_output_verbose(5, mca_btl_base_output,
                            "btl:usnic: found: device %s, port %d",
                            port->device->device_name, port->port_num);

        /* This component only works with Cisco VIC/USNIC devices; it
           is not a general verbs UD component.  Reject any ports
           found on devices that are not Cisco VICs. */
        if (
#if BTL_USNIC_HAVE_IBV_USNIC
            /* If we have the IB_*_USNIC constants, then take any
               device which advertises them */
            (IBV_TRANSPORT_USNIC == port->device->device->transport_type &&
             IBV_NODE_USNIC == port->device->device->node_type) ||
#endif
            /* Or take any specific device that we know is a Cisco
               VIC.  Cisco's vendor ID is 0x1137, and Sereno-based
               VICs are part ID 127. */
              /* JMS Would be good to put this in a text config file
                 so that users can patch a binary install of Open MPI
                 to allow for new VIC product numbers if necessary */
            (0x1137 == port->device->device_attr.vendor_id &&
             127 == port->device->device_attr.vendor_part_id)) {
	    /* Happiness */
	} else {
            opal_output_verbose(5, mca_btl_base_output,
                                "btl:usnic: this is not a usnic (expected vendor/part 0x1137/127, got 0x%x/%d)",
                                port->device->device_attr.vendor_id,
                                port->device->device_attr.vendor_part_id);
            --mca_btl_usnic_component.num_modules;
            --i;
            continue;
        }
    
        module = &(mca_btl_usnic_component.usnic_modules[i]);
        memcpy(module, &ompi_btl_usnic_module_template,
               sizeof(ompi_btl_usnic_module_t));
        module->device = port->device->device;
        module->device_context = port->device->context;
        module->port_num = port->port_num;

        /* If we fail to query the GID, just warn and skip this port */
        if (0 != ibv_query_gid(port->device->context, 
                               port->port_num,
                               mca_btl_usnic_component.gid_index, &gid)) {
            opal_memchecker_base_mem_defined(&gid, sizeof(gid));
            opal_show_help("help-mpi-btl-usnic.txt", "ibv_query_gid failed",
                           true, 
                           orte_process_info.nodename,
                           ibv_get_device_name(port->device->device), 
                           port->port_num);
            --mca_btl_usnic_component.num_modules;
            --i;
            continue;
        }

        BTL_VERBOSE(("GID for verbs device %s port %d: subnet 0xx%016" PRIx64 ", interface 0x%016 " PRIx64,
                     ibv_get_device_name(port->device->device), 
                     port->port_num, 
		     ntoh64(gid.global.subnet_prefix),
		     ntoh64(gid.global.interface_id)));
	module->addr.gid = gid;

        /* Get this port's bandwidth */
        if (0 == module->super.btl_bandwidth) {
            if (OMPI_SUCCESS !=
                ompi_common_verbs_port_bw(&port->port_attr,
                                          &module->super.btl_bandwidth)) {

                /* If we don't get OMPI_SUCCESS, then we weren't able
                   to figure out what the bandwidth was of this port.
                   That's a bad sign.  Let's ignore this port. */
                opal_show_help("help-mpi-btl-usnic.txt", "verbs_port_bw failed",
                               true, 
                               orte_process_info.nodename,
                               ibv_get_device_name(port->device->device), 
                               port->port_num);
                --mca_btl_usnic_component.num_modules;
                --i;
                continue;
            }
        }
        opal_output_verbose(5, mca_btl_base_output,
                            "btl:usnic: bandwidth for %s:%d = %u",
                            port->device->device_name, port->port_num,
                            module->super.btl_bandwidth);

        /* Query this device */
        if (0 != ibv_query_device(module->device_context, &device_attr)) {
            opal_show_help("help-mpi-btl-usnic.txt", "ibv_query_device failed",
                           true, 
                           orte_process_info.nodename,
                           ibv_get_device_name(port->device->device), 
                           port->port_num);
            --mca_btl_usnic_component.num_modules;
            --i;
            continue;
        }
        opal_memchecker_base_mem_defined(&device_attr, sizeof(device_attr));

        /* How many xQ entries do we want? */
	if (-1 == mca_btl_usnic_component.sd_num) {
	    module->sd_num = device_attr.max_qp_wr;
	} else {
	    module->sd_num = mca_btl_usnic_component.sd_num;
	}
	if (-1 == mca_btl_usnic_component.sd_num) {
	    module->rd_num = device_attr.max_qp_wr;
	} else {
	    module->rd_num = mca_btl_usnic_component.rd_num;
	}
	if (-1 == mca_btl_usnic_component.sd_num) {
	    module->cq_num = device_attr.max_cqe;
	} else {
	    module->cq_num = mca_btl_usnic_component.cq_num;
	}
        opal_output_verbose(5, mca_btl_base_output,
                            "btl:usnic: num sqe=%d, num rqe=%d, num cqe=%d",
                            module->sd_num,
                            module->rd_num,
                            module->cq_num);

        /* Find the max payload this port can handle */
        max_payload = 
            ompi_common_verbs_mtu(&port->port_attr) - /* start with the MTU */
            sizeof(ompi_btl_usnic_protocol_header_t) - /* subtract size of
                                                           the protocol frame 
                                                           header */
            sizeof(ompi_btl_usnic_btl_header_t); /* subtract size of
                                                      the BTL
                                                      header */
        /* If the eager/rndv/max send sizes are 0, initialize it to
           the MTU of this port */
        if (0 == module->super.btl_eager_limit) {
            module->super.btl_eager_limit = max_payload;
        }

        /* See if the requested eager_limit is larger than the max
           payload */
        if (module->super.btl_eager_limit > max_payload) {
            opal_show_help("help-mpi-btl-usnic.txt", "eager_limit too high",
                           true, 
                           orte_process_info.nodename,
                           ibv_get_device_name(port->device->device), 
                           port->port_num,
                           max_payload,
                           module->super.btl_eager_limit);
            --mca_btl_usnic_component.num_modules;
            --i;
            continue;
        }

        /* JMS ULTIMATELY REMOVE ME. Temporarily set the value on the
           template so that we can get it in the frag constructor.
           See comment in btl_usnic_frag.c for more info. */
        ompi_btl_usnic_module_template.super.btl_eager_limit = 
            ompi_btl_usnic_module_template.super.btl_rndv_eager_limit = 
            ompi_btl_usnic_module_template.super.btl_max_send_size = 
            module->super.btl_eager_limit;

        opal_output_verbose(5, mca_btl_base_output,
                            "btl:usnic: eager limit %s:%d = %" PRIsize_t,
                            port->device->device_name, port->port_num,
                            module->super.btl_eager_limit);

        /* Set rndv_eager_limit and max_send_size to be the same as
           the eager limit */
        module->super.btl_rndv_eager_limit =
            module->super.btl_max_send_size =
            module->super.btl_eager_limit;

        /* Make a hash table of senders */
        OBJ_CONSTRUCT(&module->senders, opal_hash_table_t);
        /* JMS This is a fixed size -- BAD!  But since hash table
           doesn't grow dynamically, I don't know what size to put
           here.  I think the long-term solution is to write a better
           hash table... :-( */
        opal_hash_table_init(&module->senders, 4096);
        
        /* Initialize this module's state */
        if (ompi_btl_usnic_module_init(module) != OMPI_SUCCESS) {
            opal_output_verbose(5, mca_btl_base_output,
                                "btl:usnic: failed to init module for %s:%d",
                                port->device->device_name, port->port_num);
            --mca_btl_usnic_component.num_modules;
            --i;
            continue;
        }

        /* Ok, this is a good port -- we want it.  Save it.  And tell
           the common_verbs_device to not free the device context when
           the list is freed. */
        port->device->destructor_free_context = false;
        btls[i] = &(module->super);

        opal_output_verbose(5, mca_btl_base_output,
                            "btl:usnic: exclusivity %s:%d = %d",
                            port->device->device_name, port->port_num,
                            module->super.btl_exclusivity);
    }

    /* We've packed all the modules and pointers to those modules in
       the lower ends of their respective arrays.  If not all the
       modules initialized successfully, we're wasting a little space.
       We could realloc and re-form the btls[] array, but it doesn't
       seem worth it.  Just waste a little space.

       That being said, if we ended up with zero acceptable ports,
       then free everything. */
    if (0 == i) {
        if (NULL != mca_btl_usnic_component.usnic_modules) {
            free(mca_btl_usnic_component.usnic_modules);
            mca_btl_usnic_component.usnic_modules = NULL;
        }
        if (NULL != btls) {
            free(btls);
            btls = NULL;
        }

        opal_output_verbose(5, mca_btl_base_output,
                            "btl:usnic: returning 0 modules");
        goto modex_send;
    }

    *num_btl_modules = mca_btl_usnic_component.num_modules;
    opal_output_verbose(5, mca_btl_base_output,
                        "btl:usnic: returning %d modules", *num_btl_modules);

 free_include_list:
    while (NULL != (item = opal_list_remove_first(port_list))) {
        OBJ_RELEASE(item);
    }

 modex_send:
    usnic_modex_send();
    return btls;
}

/*
 * Component progress
 */
static int usnic_component_progress(void)
{
    uint32_t i;
    int j, count = 0, num_events;
    ompi_btl_usnic_frag_t* frag;
#if RELIABILITY
    opal_list_item_t *item;
#endif
    struct ibv_recv_wr *bad_wr, *repost_recv_head, *next;
    struct ibv_wc* cwc;
    ompi_btl_usnic_module_t* module;
    static struct ibv_wc wc[OMPI_BTL_USNIC_NUM_WC];

    /* Poll for completions */
    for (i = 0; i < mca_btl_usnic_component.num_modules; i++) {
        module = &mca_btl_usnic_component.usnic_modules[i];

#if RELIABILITY
        /* If we have any pending resend frags, try to send the
           first one. */
        /* JMS Is it enough to only try to progress the first pending
           frag?  Is that fair? */
        /* JMS Should be able to check if there is anything to
           progress first, and do it inline/here (without a function
           call) */
        if (module->sd_wqe > 0) {
            ompi_btl_usnic_frag_progress_pending_resends(module);
        }
#endif

        num_events = ibv_poll_cq(module->cq, OMPI_BTL_USNIC_NUM_WC, wc);
        opal_memchecker_base_mem_defined(&num_events, sizeof(num_events));
        opal_memchecker_base_mem_defined(wc, sizeof(wc[0]) * num_events);
        if (OPAL_UNLIKELY(num_events < 0)) {
            BTL_ERROR(("error polling CQ with %d: %s\n",
                    num_events, strerror(errno)));
            return OMPI_ERROR;
        }

        repost_recv_head = NULL;
#if RELIABILITY
        assert(opal_list_is_empty(&(module->endpoints_that_need_acks)));
#endif
        for (j = 0; j < num_events; j++) {
            cwc = &wc[j];
            frag = (ompi_btl_usnic_frag_t*)(unsigned long)cwc->wr_id;

            if (OPAL_UNLIKELY(cwc->status != IBV_WC_SUCCESS)) {
                BTL_ERROR(("error polling CQ with status %d for wr_id %" PRIx64 " opcode %d (%d of %d)\n",
                           cwc->status, cwc->wr_id, cwc->opcode, j, num_events));
                /* If it was a receive error, just drop it and keep
                   going.  The sender will eventually re-send it. */
                if (IBV_WC_RECV == cwc->opcode) {
                    frag->wr_desc.rd_desc.next = repost_recv_head;
                    repost_recv_head = &frag->wr_desc.rd_desc;
                    continue;
                }
                return OMPI_ERROR;
            }

            /* Handle work completions */
            switch(frag->type) {

            /**** Send ACK completions ****/
#if RELIABILITY
            case OMPI_BTL_USNIC_FRAG_ACK:
                assert(IBV_WC_SEND == cwc->opcode);
                ompi_btl_usnic_ack_complete(module, frag);
                break;
#endif

            /**** Send fragment completions ****/
            case OMPI_BTL_USNIC_FRAG_SEND:
                assert(IBV_WC_SEND == cwc->opcode);
                ompi_btl_usnic_send_complete(module, frag);
                break;

            /**** Receive completions ****/
            case OMPI_BTL_USNIC_FRAG_RECV:
                assert(IBV_WC_RECV == cwc->opcode);
                ompi_btl_usnic_recv(module, frag, &repost_recv_head);
                break;

            default:
                BTL_ERROR(("Unhandled completion opcode %d frag type %d",
                            cwc->opcode, frag->type));
                break;
            }
        }

#if RELIABILITY
        /* If any endpoints received activity, send them an ACK */
        for (item = opal_list_remove_first(&(module->endpoints_that_need_acks));
             NULL != item;
             item = opal_list_remove_first(&(module->endpoints_that_need_acks))) {
            ompi_btl_usnic_endpoint_list_item_t *eli;

            eli = (ompi_btl_usnic_endpoint_list_item_t*) item;
            assert(eli->enqueued);
            eli->enqueued = false;
            ompi_btl_usnic_ack_send(module, eli->endpoint);
        }
#endif

        /* Re-post all the remaining receive buffers */
        if (OPAL_LIKELY(repost_recv_head)) {
            /* JMS libusnic is broken -- it only posts 1 buffer at a
               time.  Remove this loop when it is fixed. */
            while (repost_recv_head) {
                /* JMS Do some extra work to ensure the list is
                   broken */
                frag = (ompi_btl_usnic_frag_t*)(unsigned long)repost_recv_head->wr_id;
                assert(!FRAG_STATE_GET(frag, FRAG_RECV_WR_POSTED));
                assert(OMPI_BTL_USNIC_FRAG_RECV == frag->type);

                next = repost_recv_head->next;
                repost_recv_head->next = NULL;
                if (OPAL_UNLIKELY(ibv_post_recv(module->qp, 
                                                repost_recv_head, &bad_wr) != 0)) {
                    BTL_ERROR(("error posting recv: %s\n", strerror(errno)));
                    return OMPI_ERROR;
                }
                FRAG_HISTORY(frag, "Initial ibv_post_recv");
                repost_recv_head = next;
            }
        }

        count += num_events;
    }

    return count;
}
