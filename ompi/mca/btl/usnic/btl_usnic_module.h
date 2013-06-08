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
 * Copyright (c) 2011-2013 Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/**
 * @file
 */
#ifndef OMPI_BTL_USNIC_MODULE_H
#define OMPI_BTL_USNIC_MODULE_H

/*
 * Default limits
 */
#define USNIC_DFLT_EAGER_LIMIT (96*1024)
#define USNIC_DFLT_MAX_SEND INT_MAX

#include "btl_usnic_endpoint.h"

BEGIN_C_DECLS

/*
 * Forward declarations to avoid include loops
 */
struct ompi_btl_usnic_send_segment_t;


/*
 * Abstraction of a set of IB queus
 */
typedef struct ompi_btl_usnic_channel_t {
    struct ibv_cq *cq;

    /** available send WQ entries */
    int32_t sd_wqe;

    /** queue pair */
    struct ibv_qp* qp;

    /** receive segments & buffers */
    ompi_free_list_t recv_segs;

    /* statistics */
    uint32_t num_channel_sends;
} ompi_btl_usnic_channel_t;

/**
 * UD verbs BTL interface
 */
typedef struct ompi_btl_usnic_module_t {
    mca_btl_base_module_t super;

    mca_btl_base_module_error_cb_fn_t pml_error_callback;

    uint8_t port_num;
    struct ibv_device *device;
    struct ibv_context *device_context;
    struct event device_async_event;
    bool device_async_event_active;
    struct ibv_pd *pd;

    /* Information about the IP interface corresponding to this USNIC
       interface */
    char if_name[64];
    uint32_t if_ipv4_addr;
    uint32_t if_cidrmask;
    uint8_t if_mac[6];
    int if_mtu;

    /** desired send, receive, and completion queue entries (from MCA
        params; cached here on the component because the MCA param
        might == 0, which means "max supported on that device") */
    int sd_num;
    int rd_num;
    int cq_num;

    /* 
     * Fragments larger than max_frag_payload will be broken up into
     * multiple chunks.  The amount that can be held in a single chunk
     * segment is slightly less than what can be held in frag segment due
     * to fragment reassembly info.
     */
    size_t max_frag_payload;    /* most that fits in a frag segment */
    size_t max_chunk_payload;   /* most that can fit in chunk segment */

    /** Hash table to keep track of senders */
    opal_hash_table_t senders;

    /** local address information */
    struct ompi_btl_usnic_addr_t local_addr;

    /** send fragments & buffers */
    ompi_free_list_t small_send_frags;
    ompi_free_list_t large_send_frags;
    ompi_free_list_t put_dest_frags;
    ompi_free_list_t chunk_segs;

    /** list of endpoints with data to send */
    /* this list uses base endpoint ptr */
    opal_list_t endpoints_with_sends;

    /** list of send frags that are waiting to be resent (they
        previously deferred because of lack of resources) */
    opal_list_t pending_resend_segs;

    /** ack segments */
    ompi_free_list_t ack_segs;

    /** list of endpoints to which we need to send ACKs */
    /* this list uses endpoint->endpoint_ack_li */
    opal_list_t endpoints_that_need_acks;

    /* we use two IB queue pairs, each with its own CQ.  
     * We call a CQ + QP a "channel"
     */
    ompi_btl_usnic_channel_t data_channel;
    ompi_btl_usnic_channel_t cmd_channel;

    uint32_t qp_max_inline;

    /* Debugging statistics */
    bool final_stats;
    uint64_t stats_report_num;

    uint64_t num_total_sends;
    uint64_t num_resends;
    uint64_t num_chunk_sends;
    uint64_t num_frag_sends;
    uint64_t num_ack_sends;

    uint64_t num_total_recvs;
    uint64_t num_unk_recvs;
    uint64_t num_dup_recvs;
    uint64_t num_oow_low_recvs;
    uint64_t num_oow_high_recvs;
    uint64_t num_frag_recvs;
    uint64_t num_chunk_recvs;
    uint64_t num_badfrag_recvs;
    uint64_t num_ack_recvs;
    uint64_t num_old_dup_acks;
    uint64_t num_dup_acks;
    uint64_t num_fast_retrans;
    uint64_t num_recv_reposts;

    uint64_t max_sent_window_size;
    uint64_t max_rcvd_window_size;

    uint64_t pml_module_sends;
    uint64_t pml_send_callbacks;

    ompi_btl_usnic_endpoint_t **all_endpoints;
    int num_endpoints;

    opal_event_t stats_timer_event;
    struct timeval stats_timeout;
} ompi_btl_usnic_module_t;

struct ompi_btl_usnic_frag_t;
extern ompi_btl_usnic_module_t ompi_btl_usnic_module_template;

/*
 * Manipulate the "endpoints_that_need_acks" list
 */

/* get first endpoint needing ACK */
static inline ompi_btl_usnic_endpoint_t *
ompi_btl_usnic_get_first_endpoint_needing_ack(
    ompi_btl_usnic_module_t *module)
{
    opal_list_item_t *item;
    ompi_btl_usnic_endpoint_t *endpoint;

    item = opal_list_get_first(&module->endpoints_that_need_acks);
    if (item != opal_list_get_end(&module->endpoints_that_need_acks)) {
        endpoint = container_of(item, mca_btl_base_endpoint_t, endpoint_ack_li);
        return endpoint;
    } else {
        return NULL;
    }
}

/* get next item in chain */
static inline ompi_btl_usnic_endpoint_t *
ompi_btl_usnic_get_next_endpoint_needing_ack(
    ompi_btl_usnic_endpoint_t *endpoint)
{
    opal_list_item_t *item;
    ompi_btl_usnic_module_t *module;

    module = endpoint->endpoint_module;

    item = opal_list_get_next(&(endpoint->endpoint_ack_li));
    if (item != opal_list_get_end(&module->endpoints_that_need_acks)) {
        endpoint = container_of(item, mca_btl_base_endpoint_t, endpoint_ack_li);
        return endpoint;
    } else {
        return NULL;
    }
}

static inline void
ompi_btl_usnic_remove_from_endpoints_needing_ack(
    ompi_btl_usnic_endpoint_t *endpoint)
{
    opal_list_remove_item(
            &(endpoint->endpoint_module->endpoints_that_need_acks),
            &endpoint->endpoint_ack_li);
    endpoint->endpoint_ack_needed = false;
    endpoint->endpoint_acktime = 0;
#if MSGDEBUG1
    opal_output(0, "clear ack_needed on %p\n", endpoint);
#endif
}

static inline void
ompi_btl_usnic_add_to_endpoints_needing_ack(
    ompi_btl_usnic_endpoint_t *endpoint)
{
    opal_list_append(&(endpoint->endpoint_module->endpoints_that_need_acks),
            &endpoint->endpoint_ack_li);
    endpoint->endpoint_ack_needed = true;
#if MSGDEBUG1
    opal_output(0, "set ack_needed on %p\n", endpoint);
#endif
}

/*
 * Initialize a module
 */
int ompi_btl_usnic_module_init(ompi_btl_usnic_module_t* module);


/*
 * Progress pending sends on a module
 */
void ompi_btl_usnic_module_progress_sends(ompi_btl_usnic_module_t *module);


END_C_DECLS
#endif
