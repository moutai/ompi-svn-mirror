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
 * Copyright (c) 2013 Cisco Systems, Inc.  All rights reserved.
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
#include "btl_usnic_module.h"
#include "btl_usnic_frag.h"
#include "btl_usnic_proc.h"
#include "btl_usnic_util.h"
#include "btl_usnic_ack.h"
#include "btl_usnic_send.h"

/*
 * Post a send to the verbs work queue
 */
void
ompi_btl_usnic_endpoint_send_segment(
    ompi_btl_usnic_module_t *module,
    ompi_btl_usnic_send_segment_t *sseg)
{
    int ret;
    ompi_btl_usnic_send_frag_t *frag;
    ompi_btl_usnic_endpoint_t *endpoint;
    uint16_t sfi;

    frag = sseg->ss_parent_frag;
    endpoint = frag->sf_endpoint;

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
    if (OPAL_UNLIKELY(!WINDOW_OPEN(endpoint))) {
        /* Stats */
        abort();        /* checked by caller */
    }

    /* Assign sequence number and increment */
    sseg->ss_base.us_btl_header->seq = endpoint->endpoint_next_seq_to_send++;

    /* Fill in remote address to indicate PUT or not */
    sseg->ss_base.us_btl_header->put_addr = frag->sf_dest_addr;

    /* Track this header by stashing in an array on the endpoint that
       is the same length as the sender's window (i.e., WINDOW_SIZE).
       To find a unique slot in this array, use (seq % WINDOW_SIZE).
     */
    sfi = WINDOW_SIZE_MOD(sseg->ss_base.us_btl_header->seq);
    endpoint->endpoint_sent_segs[sfi] = sseg;
    sseg->ss_ack_pending = true;

    /* piggy-back an ACK if needed */
    ompi_btl_usnic_piggyback_ack(endpoint, sseg);

#if MSGDEBUG1
    /* JMS Remove me */
    {
        uint8_t mac[6];
    char mac_str1[128];
    char mac_str2[128];
    ompi_btl_usnic_sprintf_mac(mac_str1, module->addr.mac);
        ompi_btl_usnic_gid_to_mac(&endpoint->endpoint_remote_addr.gid, mac);
    ompi_btl_usnic_sprintf_mac(mac_str2, mac);

        opal_output(0, "--> Sending %s: seq: %" UDSEQ ", sender: 0x%016lx from device %s MAC %s, qp %u, seg %p, room %d, wc len %u, remote MAC %s, qp %u",
            (sseg->ss_parent_frag->sf_base.uf_type == OMPI_BTL_USNIC_FRAG_LARGE_SEND)?
                "CHUNK" : "FRAG",
            sseg->ss_base.us_btl_header->seq, 
            sseg->ss_base.us_btl_header->sender, 
            endpoint->endpoint_module->device->name,
            mac_str1, module->addr.data_qp_num, sseg, sseg->ss_hotel_room,
            sseg->ss_base.us_sg_entry.length,
            mac_str2, endpoint->endpoint_remote_addr.data_qp_num);
    }
#endif

    /* do the actual send */
    ompi_btl_usnic_post_segment(module, endpoint, sseg, 0);

    /* If we have room in the sender's window, we also have room in
       endpoint hotel */
    ret = opal_hotel_checkin(&endpoint->endpoint_hotel, sseg,
            &sseg->ss_hotel_room);
    if (OPAL_UNLIKELY(OPAL_SUCCESS != ret)) {
        /* Hotel is full; this shouldn't happen! */
        BTL_ERROR(("=== Hotel full on endpoint send, mod %p, ep %p, seg %p", 
                   (void*) module, (void*) endpoint, (void*) sseg));
        abort();
    }

    /* bookkeeping */
    --endpoint->endpoint_send_credits;

    /* Stats */
    if (sseg->ss_parent_frag->sf_base.uf_type == OMPI_BTL_USNIC_FRAG_LARGE_SEND)
        ++module->num_chunk_sends;
    else
        ++module->num_frag_sends;
}


/*
 * Construct/destruct an endpoint structure.
 */
static void endpoint_construct(mca_btl_base_endpoint_t* endpoint)
{
    endpoint->endpoint_module = NULL;
    endpoint->endpoint_proc = NULL;

    endpoint->endpoint_remote_addr.data_qp_num = 0;
    endpoint->endpoint_remote_addr.cmd_qp_num = 0;
    endpoint->endpoint_remote_addr.gid.global.subnet_prefix = 0;
    endpoint->endpoint_remote_addr.gid.global.interface_id = 0;
    endpoint->endpoint_remote_ah = NULL;

    endpoint->endpoint_send_credits = 8;

    /* list of fragments queued to be sent */
    OBJ_CONSTRUCT(&endpoint->endpoint_frag_send_queue, opal_list_t);

    endpoint->endpoint_next_seq_to_send = 10;
    endpoint->endpoint_ack_seq_rcvd = 9;
    endpoint->endpoint_next_contig_seq_to_recv = 10;
    endpoint->endpoint_highest_seq_rcvd = 9;

    endpoint->endpoint_sfstart = endpoint->endpoint_next_seq_to_send;
    endpoint->endpoint_rfstart = endpoint->endpoint_next_contig_seq_to_recv;

    /*
     * Make a new OPAL hotel for this module
     * "hotel" is a construct used for triggering segment retransmission
     * due to timeout
     */
    OBJ_CONSTRUCT(&endpoint->endpoint_hotel, opal_hotel_t);
    opal_hotel_init(&endpoint->endpoint_hotel, 
                    WINDOW_SIZE,
                    mca_btl_usnic_component.retrans_timeout,
                    0,
                    ompi_btl_usnic_ack_timeout);

    /* Setup this endpoint's ACK_needed list link */
    OBJ_CONSTRUCT(&(endpoint->endpoint_ack_li), opal_list_item_t);
    endpoint->endpoint_ack_needed = false;

    /* fragment reassembly info */
    endpoint->endpoint_rx_frag_info =
        calloc(sizeof(struct ompi_btl_usnic_rx_frag_info_t), MAX_ACTIVE_FRAGS);
    if (endpoint->endpoint_rx_frag_info == NULL) {
        abort();
    }

    endpoint->endpoint_nbo = false;
}

static void endpoint_destruct(mca_btl_base_endpoint_t* endpoint)
{
    OBJ_DESTRUCT(&(endpoint->endpoint_ack_li));
    OBJ_DESTRUCT(&endpoint->endpoint_hotel);
#if 0
    /* JMS Restore when we keep the original calloc pointer */
    free(endpoint->endpoint_sent_frags);
#endif

    if (NULL != endpoint->endpoint_remote_ah) {
        ibv_destroy_ah(endpoint->endpoint_remote_ah);
    }
}

OBJ_CLASS_INSTANCE(ompi_btl_usnic_endpoint_t,
                   opal_list_item_t, 
                   endpoint_construct,
                   endpoint_destruct);
