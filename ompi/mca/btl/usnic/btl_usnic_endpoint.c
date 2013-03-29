/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2006      Sandia National Laboratories. All rights
 *                         reserved.
 * Copyright (c) 2007      The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2012 Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ompi_config.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "opal/prefetch.h"
#include "ompi/types.h"

#include "btl_usnic.h"
#include "btl_usnic_endpoint.h"
#include "btl_usnic_frag.h"
#include "btl_usnic_proc.h"
#include "btl_usnic_util.h"
#include "btl_usnic_ack.h"
#include "btl_usnic_send.h"


/*
 * Post a send to the work queue
 */
int ompi_btl_usnic_endpoint_post_send(ompi_btl_usnic_module_t* module,
                                        ompi_btl_usnic_frag_t* frag)
{
    int ret;
    struct ibv_send_wr* bad_wr;
    struct ibv_send_wr* wr = &frag->wr_desc.sr_desc;
    ompi_btl_usnic_endpoint_t* endpoint = frag->endpoint;
#if RELIABILITY
    ompi_btl_usnic_pending_frag_t *sent_frag;
    int room;
    uint16_t sfi;
#endif

    frag->sg_entry.length = 
        sizeof(ompi_btl_usnic_btl_header_t) +
        frag->segment.seg_len;
    wr->send_flags = IBV_SEND_SIGNALED;

    /* Do we have a send descriptor available? */
    if (OPAL_UNLIKELY(0 == module->sd_wqe)) {
        /* Stats */
        ++module->num_deferred_sends_no_wqe;
        goto queue_send;
    }
    --module->sd_wqe;

#if RELIABILITY
    /* Do we have room in the endpoint's sender window?

       Sender window:

                       |-------- WINDOW_SIZE ----------|
                      +---------------------------------+
                      |         next_seq_to_send        |
                      |     somewhere in this range     |
                     ^+---------------------------------+
                     |
                     +-- ack_seq_rcvd: one less than the window left edge

       Assuming that next_seq_to_send is > ack_seq_rcvd (verified
       by assert), then the good condition to send is:

            next_seq_to_send <= ack_seq_rcvd + WINDOW_SIZE

       And therefore the bad condition is

            next_seq_to_send > ack_seq_rcvd + WINDOW_SIZE
    */
    assert(endpoint->endpoint_next_seq_to_send > 
           endpoint->endpoint_ack_seq_rcvd);
    if (OPAL_UNLIKELY(endpoint->endpoint_next_seq_to_send >
                      endpoint->endpoint_ack_seq_rcvd + WINDOW_SIZE)) {
        ++module->sd_wqe;
        /* Stats */
        ++module->num_deferred_sends_outside_window;
        goto queue_send;
    }
    frag->btl_header->seq = endpoint->endpoint_next_seq_to_send++;

    /* Track this header by stashing in an array on the endpoint that
       is the same length as the sender's window (i.e., WINDOW_SIZE).
       To find a unique slot in this array, use (seq %
       WINDOW_SIZE). */
    sfi = WINDOW_SIZE_MOD(frag->btl_header->seq);
    sent_frag = &(endpoint->endpoint_sent_frags[sfi]);

    /* If we have room in the sender's window, we also have room in
       endpoint hotel */
    ret = opal_hotel_checkin(&endpoint->endpoint_hotel, frag, &room);
    if (OPAL_UNLIKELY(OPAL_ERR_TEMP_OUT_OF_RESOURCE == ret)) {
        ++module->sd_wqe;
        /* Hotel is full; this shouldn't happen! */
        BTL_ERROR(("=== Hotel is full on endpoint send, module %p, endpoint %p, frag %p", 
                   (void*) module,
                   (void*) endpoint,
                   (void*) frag));
        ++module->num_deferred_sends_hotel_full;
        goto queue_send;
    }
    FRAG_STATE_SET(frag, FRAG_IN_HOTEL);

    /* Save this fragment and meta information about the fragment in
       the sent window so that we can find it when it it ACKed. */
    sent_frag->frag = frag;
    sent_frag->hotel_room = room;
    sent_frag->occupied = true;
#endif /* Reliability */

#if BTL_USNIC_USNIC
    /* Fill in my MAC address as the source MAC, and fill in the
       ethertype */
    /* JMS Since we have a send pool for each BTL module, it would be
       better to do this in the frag constructor... but we need to get
       the module in there somehow. :-( */
    /* JMS For the moment, the MAC is in the lower 6 bytes of the raw
       GID */
    memcpy(frag->protocol_header->l2_src_mac, &(module->addr.gid.raw), 6);
    memcpy(frag->protocol_header->l2_dest_mac, 
           &(endpoint->endpoint_remote_addr.gid.raw), 6);
    frag->protocol_header->qp_num =
        htons(endpoint->endpoint_remote_addr.qp_num);
#else
    wr->wr.ud.ah = endpoint->endpoint_remote_ah;
    wr->wr.ud.remote_qpn = endpoint->endpoint_remote_addr.qp_num;
#endif

    /* JMS If this is going to be a general UD verbs BTL, we need to:
       - check for endian heterogeneity
       - check for inline support */

#if BTL_USNIC_USNIC && MSGDEBUG
    /* JMS Remove me */
    {
        char mac1[32], mac2[32];
        ompi_btl_usnic_sprintf_mac(mac1, frag->protocol_header->l2_dest_mac);
        ompi_btl_usnic_sprintf_mac(mac2, frag->protocol_header->l2_src_mac);
        opal_output(0, "--> Sending MSG: frame: dest MAC %s, src MAC %s, qpn: 0x%x, seq: %" UDSEQ ", room %d, wc len %u, module %p, ep %p",
                    mac1, mac2, 
                    ntohs(frag->protocol_header->qp_num),
                    frag->btl_header->seq, room,
                    frag->sg_entry.length, (void*)module, (void*)endpoint);
    }
#endif

    if (OPAL_UNLIKELY((ret = ibv_post_send(module->qp, wr, &bad_wr)))) {
        BTL_ERROR(("error posting send request: %d %s\n", ret, strerror(ret)));
        /* JMS handle the error here */
        abort();
    }
    /* It is possible for a send frag to be posted twice
       simultaneously (e.g., send that hasn't been reaped via
       cq_poll() yet and a resend), so we maintain a counter (vs. a
       bool). */
    ++frag->send_wr_posted;

    /* Stats */
    ++module->num_total_sends;
    ++module->num_frag_sends;

    return OMPI_SUCCESS;

    /*****************************************************************/
 queue_send:
    /* JMS See the comment in btl_usnic_module.c:usnic_send()
       about why it's a better idea to queue up here.  We'll re-enable
       this optimization later when I have time to debug it... */
    return OMPI_ERR_OUT_OF_RESOURCE;
}


/*
 * Construct/destruct an endpoint structure.
 */
static void endpoint_construct(mca_btl_base_endpoint_t* endpoint)
{
    endpoint->endpoint_module = NULL;
    endpoint->endpoint_proc = NULL;

    endpoint->endpoint_remote_addr.qp_num = 0;
    endpoint->endpoint_remote_addr.gid.global.subnet_prefix = 0;
    endpoint->endpoint_remote_addr.gid.global.interface_id = 0;
    endpoint->endpoint_remote_ah = NULL;

#if RELIABILITY
    endpoint->endpoint_next_seq_to_send = 10;
    endpoint->endpoint_ack_seq_rcvd = 9;
    endpoint->endpoint_next_contig_seq_to_recv = 10;
    endpoint->endpoint_highest_seq_rcvd = 9;

    endpoint->endpoint_sent_frags = 
        calloc(WINDOW_SIZE * 2, sizeof(ompi_btl_usnic_pending_frag_t));
    endpoint->endpoint_rcvd_frags = endpoint->endpoint_sent_frags + WINDOW_SIZE;
    endpoint->endpoint_sfstart = endpoint->endpoint_rfstart = 0;

    /* Make a new OPAL hotel for this module */
    OBJ_CONSTRUCT(&endpoint->endpoint_hotel, opal_hotel_t);
    opal_hotel_init(&endpoint->endpoint_hotel, 
                    WINDOW_SIZE,
                    mca_btl_usnic_component.retrans_timeout,
                    0,
                    ompi_btl_usnic_ack_timeout);

    /* Setup this endpoint's list item */
    OBJ_CONSTRUCT(&(endpoint->endpoint_li.super), opal_list_item_t);
    endpoint->endpoint_li.endpoint = endpoint;
#endif

    endpoint->endpoint_nbo = false;
}

static void endpoint_destruct(mca_btl_base_endpoint_t* endpoint)
{
#if RELIABILITY
    OBJ_DESTRUCT(&(endpoint->endpoint_li.super));
    OBJ_DESTRUCT(&endpoint->endpoint_hotel);
#if 0
    /* JMS Restore when we keep the original calloc pointer */
    free(endpoint->endpoint_sent_frags);
#endif
#endif /* RELIABILITY */

    if (NULL != endpoint->endpoint_remote_ah) {
        ibv_destroy_ah(endpoint->endpoint_remote_ah);
    }
}

OBJ_CLASS_INSTANCE(ompi_btl_usnic_endpoint_t,
                   opal_list_item_t, 
                   endpoint_construct,
                   endpoint_destruct);
