/*
 * Copyright (c) 2013 Cisco Systems, Inc.  All rights reserved.
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

#include "opal/util/output.h"
#include "opal_hotel.h"

#include "btl_usnic.h"
#include "btl_usnic_frag.h"
#include "btl_usnic_ack.h"
#include "btl_usnic_util.h"

#if RELIABILITY

/*
 * Send an ACK
 */
void ompi_btl_usnic_ack_send(ompi_btl_usnic_module_t *module,
                             ompi_btl_usnic_endpoint_t *endpoint)
{
    int rc;
    ompi_btl_usnic_frag_t *ack;
    struct ibv_send_wr *wr, *bad_wr;
#if MSGDEBUG
    uint8_t mac[6];
    char src_mac[32];
    char dest_mac[32];
#endif

    /* Get an ACK frag.  If we don't get one, just discard this ACK. */
    ack = ompi_btl_usnic_frag_ack_alloc(module);
    if (OPAL_UNLIKELY(NULL == ack)) {
        opal_output(0, "====================== No frag for sending the ACK -- skipped");
        return;
    }

    /* See if we have a WQE to send the ACK.  If not, just discard
       this ACK. */
    if (FAKE_FAIL_TO_SEND_ACK || OPAL_UNLIKELY(0 == module->sd_wqe)) {
        ompi_btl_usnic_frag_ack_return(module, ack);
        return;
    }
    --module->sd_wqe;

    /* Setup the work request */
    wr = &ack->wr_desc.sr_desc;

    /* Fill in addressing info */
    wr->wr.ud.ah = endpoint->endpoint_remote_ah;
    wr->wr.ud.remote_qpn = endpoint->endpoint_remote_addr.qp_num;

    /* Indicate that this is an ACK */
    ack->btl_header->payload_type = OMPI_BTL_USNIC_PAYLOAD_TYPE_SEQ_ACK;
    ack->btl_header->payload_len = sizeof(ack->payload.ack);

    /* Payload: send the seq of the lowest item in the window that
       we've received */
    ack->payload.ack[0] = endpoint->endpoint_next_contig_seq_to_recv - 1;

    ack->sg_entry.length = 
        sizeof(ompi_btl_usnic_btl_header_t) +
        sizeof(ack->payload.ack);

#if MSGDEBUG
    memset(src_mac, 0, sizeof(src_mac));
    memset(dest_mac, 0, sizeof(dest_mac));
    ompi_btl_usnic_sprintf_mac(src_mac, module->if_mac);
    ompi_btl_usnic_gid_to_mac(&endpoint->endpoint_remote_addr.gid, mac);
    ompi_btl_usnic_sprintf_mac(dest_mac, mac);

    opal_output(0, "--> Sending ACK wr_id 0x%lx, sg_entry length %d, seq %" UDSEQ " to %s, qp %u", 
                wr->wr_id, ack->sg_entry.length, ack->payload.ack[0], 
                dest_mac,
                endpoint->endpoint_remote_addr.qp_num);
#endif

    if (OPAL_UNLIKELY((rc = ibv_post_send(module->qp, wr, &bad_wr)))) {
        /* JMS Need to handle this properly */
        BTL_ERROR(("error posting send request: %d %s\n", rc, strerror(rc)));
        abort();
    }

    /* Stats */
    ++module->num_ack_sends;
    ++module->num_total_sends;

    return;
}


void ompi_btl_usnic_ack_complete(ompi_btl_usnic_module_t *module,
                                   ompi_btl_usnic_frag_t *frag)
{
    ompi_btl_usnic_frag_ack_return(module, frag);
    ++module->sd_wqe;

    ompi_btl_usnic_frag_progress_pending_resends(module);
}

/*****************************************************************************/

/*
 * Callback for when a send times out without receiving a
 * corresponding ACK.
 */
void ompi_btl_usnic_ack_timeout(opal_hotel_t *hotel, int room_num, 
                                  void *occupant)
{
    ompi_btl_usnic_frag_t *frag = (ompi_btl_usnic_frag_t*) occupant;
    ompi_btl_usnic_endpoint_t *endpoint = frag->endpoint;
    ompi_btl_usnic_module_t *module = endpoint->endpoint_module;

    /* This timeout function will never be called if this frag is
       ACKed, because processing the ACK will disable the timer (and
       therefore this function won't get called). */
    assert(!FRAG_STATE_GET(frag, FRAG_SEND_ACKED));
    FRAG_STATE_CLR(frag, FRAG_IN_HOTEL);

#if MSGDEBUG && 0
    {
        static int num_timeouts = 0;
        char mac[32];
        memset(mac, 0, sizeof(mac));
        ompi_btl_usnic_sprintf_mac(mac, frag->protocol_header->l2_dest_mac);
        opal_output(0, "Send timeout!  Room %d -- num_timeouts %d, frag 0x%p, seq %" UDSEQ ", to %s", 
                    room_num, ++num_timeouts, (void*) frag, frag->btl_header->seq,
                    mac);
    }
#endif

    /* Do we have a send WQE available for the resend? */
    if (FAKE_FAIL_TO_RESEND_FRAG || 0 == module->sd_wqe) {
        opal_output(-1, "============== ACK timeout: no send WQEs available: %d, send_wr_posted %d, pml_called_back %d, module %p, frag %p", module->sd_wqe, 
                    frag->send_wr_posted, 
                    FRAG_STATE_ISSET(frag, FRAG_PML_CALLED_BACK),
                    (void*) module, (void*) frag);

        /* Queue up this frag and retry sending it again later */
        opal_list_append(&(module->pending_resend_frags), 
                         &(frag->base.super.super));
        FRAG_STATE_SET(frag, FRAG_SEND_ENQUEUED);

        return;
    }

    /* Do the rest of the resend in a separate function so that we can
       call that same function when a resend frag is dequeued */
    ompi_btl_usnic_ack_timeout_part2(module, frag, true);
}


/*
 * This is the actual (re)send for a frag.  It is called when there is
 * a send WQE available.
 *
 * It is called from two places:
 *
 * 1. usnic_ack_timeout(): when a hotel timer expires (i.e., we
 *    haven't yet received an ACK for a fragment that was previously
 *    sent).
 *
 * 2. frag_progress_pending_resends(): a general function that
 *    attempts to progress the pending send list.
 */
void ompi_btl_usnic_ack_timeout_part2(ompi_btl_usnic_module_t *module,
                                      ompi_btl_usnic_frag_t *frag,
                                      bool direct)
{
    uint16_t swi;
    int ret, room;
    struct ibv_send_wr *bad_wr;
    ompi_btl_usnic_endpoint_t *endpoint = frag->endpoint;

    assert(!FRAG_STATE_GET(frag, FRAG_SEND_ENQUEUED));
    assert(module->sd_wqe > 0);

    /* Special case: if this frag was enqueued (vs. calling this
       function directly from ack_timeout(), it's possible that while
       it was waiting to be sent, the ACK came in for this frag.
       There's therefore no need to resend this frag any more, and we
       can just discard it. */
    if (FRAG_STATE_GET(frag, FRAG_SEND_ACKED)) {
#if MSGDEBUG
        opal_output(0, "================ Re-sending, but ACK already received, module %p, frag %p, pml_called %d", 
                    (void*) module, (void*) frag, 
                    FRAG_STATE_ISSET(frag, FRAG_PML_CALLED_BACK));
#endif

        /* JMS This is for debugging only -- we should directly just
           call frag_send_return() when finished. */
        if (OPAL_LIKELY(ompi_btl_usnic_frag_send_ok_to_return(module, frag))) {
            ompi_btl_usnic_frag_send_return(module, frag);
        } else {
            BTL_ERROR(("=== Returning a send frag that we shouldn't!  Frag seq=%" UDSEQ ", next seq to ACK=%" UDSEQ ", direct=%d", frag->btl_header->seq, endpoint->endpoint_next_contig_seq_to_recv, direct));
            ompi_btl_usnic_frag_dump(frag);
            abort();
        }
        return;
    }

    /* If we get here, we must be within both the sender's and
       receiver's windows.  Assert check the send window. */
    assert(frag->btl_header->seq > endpoint->endpoint_ack_seq_rcvd);
    assert(frag->btl_header->seq <= endpoint->endpoint_ack_seq_rcvd + WINDOW_SIZE);

    /* We need to check in to the hotel again, and therefore get a new
       room number. */
    ret = opal_hotel_checkin(&endpoint->endpoint_hotel, frag, &room);
    if (OPAL_UNLIKELY(OPAL_SUCCESS != ret)) {
        /* Hotel is full; this shouldn't happen! */
        /* JMS Need to separate this out into a part2/part3 function */
        BTL_ERROR(("ACK timeout: hotel is full!, module %p", (void*) module));
        ++module->sd_wqe;
        /* JMS Leaving this as an abort for now, because it should never happen */
        abort();
    }
    FRAG_STATE_SET(frag, FRAG_IN_HOTEL);

    swi = WINDOW_SIZE_MOD(frag->btl_header->seq);
    endpoint->endpoint_sent_frags[swi].hotel_room = room;

#if MSGDEBUG
    {
        uint8_t mac[6];
        char src_mac[32], dest_mac[32];
        ompi_btl_usnic_pending_frag_t *sent_frag;;

        sent_frag = &(endpoint->endpoint_sent_frags[swi]);

        memset(src_mac, 0, sizeof(src_mac));
        memset(dest_mac, 0, sizeof(dest_mac));
	ompi_btl_usnic_sprintf_mac(src_mac, module->addr.mac);
        ompi_btl_usnic_gid_to_mac(&endpoint->endpoint_remote_addr.gid, mac);
	ompi_btl_usnic_sprintf_mac(dest_mac, mac);

        opal_output(0, "--> Re-sending MSG, seq %" UDSEQ ", hotel room %d, frag 0x%p from %s qp %u to %s, qp %d",
                    sent_frag->frag->btl_header->seq, 
                    sent_frag->hotel_room,
                    (void*)frag, 
                    src_mac, 
                    module->addr.qp_num,
                    dest_mac,
                    endpoint->endpoint_remote_addr.qp_num);
    }
#endif

    --module->sd_wqe;
    if (OPAL_UNLIKELY((ret = ibv_post_send(frag->endpoint->endpoint_module->qp,
                                           &frag->wr_desc.sr_desc, &bad_wr)))) {
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
    ++module->num_resends;
    ++module->num_total_sends;
}

#endif
