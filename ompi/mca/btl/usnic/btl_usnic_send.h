/*
 * Copyright (c) 2013 Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef BTL_USNIC_SEND_H
#define BTL_USNIC_SEND_H

#include <infiniband/verbs.h>

#include "btl_usnic.h"
#include "btl_usnic_frag.h"


/*
 * Check if conditions are right, and if so, put endpoint on 
 * list of endpoints that have sends to be done
 */
static inline void
ompi_btl_usnic_check_rts(
    ompi_btl_usnic_endpoint_t *endpoint)
{
    /*
     * If endpoint not already ready,
     * and has packets to send,
     * and it has send credits,
     * and its retransmission window is open,
     * make it ready
     */
    if (!endpoint->endpoint_ready_to_send &&
        !opal_list_is_empty(&endpoint->endpoint_frag_send_queue) &&
         endpoint->endpoint_send_credits > 0 &&
         WINDOW_OPEN(endpoint)) {
        opal_list_append(&endpoint->endpoint_module->endpoints_with_sends,
                &endpoint->super);
        endpoint->endpoint_ready_to_send = true;
    }
}

/*
 * Common point for posting a segment to VERBS
 */
static inline void
ompi_btl_usnic_post_segment(
    ompi_btl_usnic_module_t *module,
    ompi_btl_usnic_endpoint_t *endpoint,
    ompi_btl_usnic_send_segment_t *sseg,
    bool priority)
{
    struct ibv_send_wr *bad_wr;
    ompi_btl_usnic_channel_t *channel;
    struct ibv_send_wr *wr;
    int ret;

#if MSGDEBUG1
    opal_output(0, "post_send: type=%d, addr=%p, len=%d\n",
        sseg->ss_base.us_type,
        sseg->ss_send_desc.sg_list->addr, sseg->ss_send_desc.sg_list->length);
    ompi_btl_usnic_dump_hex((void *)(sseg->ss_send_desc.sg_list->addr + sizeof(ompi_btl_usnic_btl_header_t)), 16);
#endif

    /* set target address */
    wr = &sseg->ss_send_desc;
    wr->wr.ud.ah = endpoint->endpoint_remote_ah;
    wr->send_flags = IBV_SEND_SIGNALED;

    /* priority queue or no? */
    if (priority) {
        channel = &module->cmd_channel;
        wr->wr.ud.remote_qpn = endpoint->endpoint_remote_addr.cmd_qp_num;
    } else {
        channel = &module->data_channel;
        wr->wr.ud.remote_qpn = endpoint->endpoint_remote_addr.data_qp_num;
    }
    sseg->ss_channel = channel;

    /* track # of time non-ACKs are posted */
    if (sseg->ss_base.us_type != OMPI_BTL_USNIC_SEG_ACK) {
        ++sseg->ss_send_posted;
        ++sseg->ss_parent_frag->sf_seg_post_cnt;
    }

    ret = ibv_post_send(channel->qp, &sseg->ss_send_desc, &bad_wr);
    if (OPAL_UNLIKELY(0 != ret)) {
        BTL_ERROR(("error posting send request: %d %s\n",
                    ret, strerror(ret)));
        /* JMS handle the error here */
        abort();
    }

    /* consume a WQE */
    --channel->sd_wqe;

    /* Stats */
    ++module->num_total_sends;
    ++channel->num_channel_sends;
}

void ompi_btl_usnic_frag_complete(ompi_btl_usnic_send_frag_t *frag);

void ompi_btl_usnic_frag_send_complete(ompi_btl_usnic_module_t *module,
                                    ompi_btl_usnic_send_segment_t *sseg);

void ompi_btl_usnic_chunk_send_complete(ompi_btl_usnic_module_t *module,
                                    ompi_btl_usnic_send_segment_t *sseg);

#endif /* BTL_USNIC_SEND_H */
