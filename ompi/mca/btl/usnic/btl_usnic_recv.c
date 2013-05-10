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

#include "ompi_config.h"

#include <infiniband/verbs.h>
#include <unistd.h>

#include "opal_stdint.h"
#include "opal/mca/memchecker/base/base.h"

#include "ompi/constants.h"
#include "ompi/mca/btl/btl.h"
#include "ompi/mca/btl/base/base.h"
#include "common_verbs.h"

#include "btl_usnic.h"
#include "btl_usnic_frag.h"
#include "btl_usnic_proc.h"
#include "btl_usnic_ack.h"
#include "btl_usnic_recv.h"
#include "btl_usnic_util.h"


/*
 * Given an incoming fragment, lookup the endpoint that sent it
 */
static inline ompi_btl_usnic_endpoint_t *
lookup_sender(ompi_btl_usnic_module_t *module, ompi_btl_usnic_frag_t *frag)
{
    int ret;
    ompi_btl_usnic_endpoint_t *sender;

    /* Use the hashed ORTE process name in the BTL header to uniquely
       identify the sending process (using the MAC/hardware address
       only identifies the sending server -- not the sending ORTE
       process). */
    /* JMS Cesare suggests using a handshake before sending any data
       so that instead of looking up a hash on the btl_header->sender,
       echo back the ptr to the sender's ompi_proc */
    ret = opal_hash_table_get_value_uint64(&module->senders, 
                                           frag->btl_header->sender,
                                           (void**) &sender);
    if (OPAL_LIKELY(OPAL_SUCCESS == ret)) {
        return sender;
    }

    /* The sender wasn't in the hash table, so do a slow lookup and
       put the result in the hash table */
    sender = ompi_btl_usnic_proc_lookup_endpoint(module, 
                                                 frag->btl_header->sender);
    if (NULL != sender) {
        opal_hash_table_set_value_uint64(&module->senders, 
                                         frag->btl_header->sender, sender);
        return sender;
    }

    /* Whoa -- not found at all! */
    return NULL;
}


void ompi_btl_usnic_recv(ompi_btl_usnic_module_t *module,
                           ompi_btl_usnic_frag_t *frag,
                           struct ibv_recv_wr **repost_recv_head)
{
    mca_btl_active_message_callback_t* reg;
    ompi_btl_usnic_endpoint_t *endpoint;
#if RELIABILITY
    ompi_btl_usnic_seq_t seq;
    uint32_t i;
    ompi_btl_usnic_pending_frag_t *pfrag;
#endif
#if MSGDEBUG
    char src_mac[32];
    char dest_mac[32];
#endif

    FRAG_STATE_CLR(frag, FRAG_RECV_WR_POSTED);
    ++module->num_total_recvs;

    /* Valgrind help */
    opal_memchecker_base_mem_defined((void*) (frag->wr_desc.rd_desc.sg_list[0].addr),
                                     frag->wr_desc.rd_desc.sg_list[0].length);

#if MSGDEBUG
    memset(src_mac, 0, sizeof(src_mac));
    memset(dest_mac, 0, sizeof(dest_mac));
    ompi_btl_usnic_sprintf_gid_mac(src_mac, &frag->protocol_header->grh.sgid);
    ompi_btl_usnic_sprintf_gid_mac(dest_mac, &frag->protocol_header->grh.dgid);

    opal_output(0, "Got message from MAC %s", src_mac);
    opal_output(0, "Looking for sender: 0x%016lx",
		frag->btl_header->sender);
#endif

    /* Find out who sent this frag */
    endpoint = frag->endpoint = lookup_sender(module, frag);
    if (FAKE_RECV_FRAG_DROP || OPAL_UNLIKELY(NULL == endpoint)) {
        /* No idea who this was from, so drop it */
#if MSGDEBUG
        opal_output(0, "=== Unknown sender; dropped: from MAC %s to MAC %s, seq %" UDSEQ, 
                    src_mac, 
                    dest_mac, 
                    frag->btl_header->seq);
#endif
        ++module->num_unk_recvs;
        goto repost;
    }

    /***********************************************************************/
    /* Frag is an incoming message */
    if (OMPI_BTL_USNIC_PAYLOAD_TYPE_MSG == frag->btl_header->payload_type) {

#if RELIABILITY
        /* Any time we get a frag from a peer, we send them an ACK */
        if (!endpoint->endpoint_li.enqueued) {
            opal_list_append(&(module->endpoints_that_need_acks), 
                             &(endpoint->endpoint_li.super));
            endpoint->endpoint_li.enqueued = true;
        }

        /* Do we have room in the endpoint's receiver window?
           
           Receiver window:

                       |-------- WINDOW_SIZE ----------|
                      +---------------------------------+
                      |         highest_seq_rcvd        |
                      |     somewhere in this range     |
                      +^--------------------------------+
                       |
                       +-- next_contig_seq_to_recv: the window left edge;
                           will always be less than highest_seq_rcvd

           The good condition is 

             next_contig_seq_to_recv <= seq < next_contig_seq_to_recv + WINDOW_SIZE

           And the bad condition is

             seq < next_contig_seq_to_recv
               or
             seq >= next_contig_seg_to_recv + WINDOW_SIZE
        */
        seq = frag->btl_header->seq;
        if (seq < endpoint->endpoint_next_contig_seq_to_recv ||
            seq >= endpoint->endpoint_next_contig_seq_to_recv + WINDOW_SIZE) {
#if MSGDEBUG
            opal_output(0, "<-- Received MSG ep %p, seq %" UDSEQ " from %s to %s: outside of window (%" UDSEQ " - %" UDSEQ "), frag %p, module %p -- DROPPED",
                        (void*)endpoint, frag->btl_header->seq, 
                        src_mac, 
                        dest_mac,
                        endpoint->endpoint_next_contig_seq_to_recv,
                        (endpoint->endpoint_next_contig_seq_to_recv + 
                         WINDOW_SIZE - 1),
                        (void*) frag,
                        (void*) module);
#endif

            /* Stats */
            ++module->num_oow_recvs;
            goto repost;
        }

        /* Ok, this frag is within the receiver window.  Have we
           already received it?  It's possible that the sender has
           re-sent a frag that we've already received (but not yet
           ACKed).

           We have saved all un-ACKed frags in an array on the
           endpoint that is the same legnth as the receiver's window
           (i.e., WINDOW_SIZE).  We can use the incoming frag sequence
           number to find its position in the array.  It's a little
           tricky because the left edge of the receiver window keeps
           moving, so we use a starting reference point in the array
           that is updated when we sent ACKs (and therefore move the
           left edge of the receiver's window).

           So this frag's index into the endpoint array is:

               rel_posn_in_recv_win = seq - next_contig_seq_to_recv
               array_posn = (rel_posn_in_recv_win + rfstart) % WINDOW_SIZE
           
           rfstart is then updated when we send ACKs:

               rfstart = (rfstart + num_acks_sent) % WINDOW_SIZE
        */
        i = seq - endpoint->endpoint_next_contig_seq_to_recv;
        i = WINDOW_SIZE_MOD(i + endpoint->endpoint_rfstart);
        if (endpoint->endpoint_rcvd_frags[i].occupied) {
#if MSGDEBUG
            /* JMS Remove me */
            opal_output(0, "<-- Received MSG ep %p, seq %" UDSEQ " from %s to %s, frag %p: duplicate -- DROPPED",
                        (void*) endpoint, frag->btl_header->seq, src_mac, dest_mac,
                        (void*) frag);
#endif
            /* highest_seq_rcvd is for debug stats only; it's not used
               in any window calculations */
            assert(seq <= endpoint->endpoint_highest_seq_rcvd);
            /* next_contig_seq_to_recv-1 is the ack number we'll
               send */
            assert (seq > endpoint->endpoint_next_contig_seq_to_recv - 1);

            /* Stats */
            ++module->num_dup_recvs;
            goto repost;
        }

        /* Nope; we haven't received it yet. */
        /* Stats */
        ++module->num_frag_recvs;

        /* Stats: is this the highest sequence number we've received? */
        if (seq > endpoint->endpoint_highest_seq_rcvd) {
            endpoint->endpoint_highest_seq_rcvd = seq;
        }

        /* Save this incoming frag in the received frags array on the
           endpoint. */
        /* JMS Possible optimization: avoid this if i == rwstart */
        /* JMS Another optimization: make rcvd_window be a bitmask
           (don't need the fields that are in pending_frag_t) */
        pfrag = &(endpoint->endpoint_rcvd_frags[i]);
        pfrag->occupied = true;

#if MSGDEBUG
        /* JMS Remove me */
        opal_output(0, "<-- Received MSG ep %p, seq %" UDSEQ " from %s to %s: GOOD! (rel seq %d, lowest seq %" UDSEQ ", highest seq: %" UDSEQ ", rwstart %d) frag %p, module %p",
                    (void*) endpoint,
                    seq, 
                    src_mac, dest_mac,
                    i,
                    endpoint->endpoint_next_contig_seq_to_recv,
                    endpoint->endpoint_highest_seq_rcvd,
                    endpoint->endpoint_rfstart,
                    (void*) frag, (void*) module);
#endif
#endif /* RELIABILITY */

        /* Pass this frag up to the PML.  Be sure to get the payload
           length from the BTL header because the L2 layer may
           artificially inflate (or otherwise change) the frame length
           to meet minimum sizes, add protocol information, etc. */
        reg = mca_btl_base_active_message_trigger + frag->payload.pml_header->tag;
        frag->segment.seg_len = frag->btl_header->payload_len;
        reg->cbfunc(&module->super, frag->payload.pml_header->tag, 
                    &frag->base, reg->cbdata);

#if RELIABILITY
        /* See if the leftmost frag in the receiver window is
           occupied.  If so, advance the window.  Repeat until we hit
           an unoccupied position in the window. */
        /* JMS **** This can be optimized *** Right now, this check
           runs for _every_ received frag, and since we don't get a
           lot of out-of-order traffic from an endpoint, we end up
           ACKing every single frag because this received message will
           be the left-hand side of the window (rather than ganging
           bunches of them together in a single ACK message).

           --> JMS This might not be true.  We might actually end up
               getting a fair amount of out-of-order traffic.

           To optimize, I think we want to a) only check this window
           condition when there's no more CQEs to process, and/or b)
           if we've processed N receives (and there might still be
           more CQEs to process). */
        /* JMS Upinder suggests using bitmaps for the occupied slots
           -- will be much more cache friendly. */
        i = endpoint->endpoint_rfstart;
        while (endpoint->endpoint_rcvd_frags[i].occupied) {
            endpoint->endpoint_rcvd_frags[i].occupied = false;
            endpoint->endpoint_next_contig_seq_to_recv++;
            i = endpoint->endpoint_rfstart = WINDOW_SIZE_MOD(i + 1);

#if MSGDEBUG
            opal_output(0, "Advance window to %d; next seq to send %" UDSEQ, i,
                        endpoint->endpoint_next_contig_seq_to_recv);
#endif
        }
#endif /* RELIABILITY */
        goto repost;
    }

#if RELIABILITY
    /***********************************************************************/
    /* Frag is an incoming ACK */
    else if (OPAL_LIKELY(OMPI_BTL_USNIC_PAYLOAD_TYPE_SEQ_ACK == 
                         frag->btl_header->payload_type)) {
        ompi_btl_usnic_seq_t ack_seq = frag->payload.ack[0];
        ompi_btl_usnic_pending_frag_t *pfrag;
        ompi_btl_usnic_seq_t is;

        /* Stats */
        ++module->num_ack_recvs;

#if MSGDEBUG
        opal_output(0, "    Received ACK for sequence number %" UDSEQ " from %s to %s",
                    frag->payload.ack[0], src_mac, dest_mac);
#endif

        /* Does this ACK have a new sequence number that we haven't
           seen before? */
        for (is = endpoint->endpoint_ack_seq_rcvd + 1; is <= ack_seq; ++is) {
            pfrag = &(endpoint->endpoint_sent_frags[WINDOW_SIZE_MOD(is)]);

#if MSGDEBUG
            /* JMS DEBUG */
	    opal_output(0, "  Checking ACK/sent_frags window %p, index %lu, seq %lu, prag=%p, occupied=%d, frag state flags=0x%x",
			(void*) endpoint->endpoint_sent_frags,
			WINDOW_SIZE_MOD(is),
			is,
			(void*)pfrag,
			pfrag->occupied,
                        pfrag->frag->state_flags);
#endif
            assert(pfrag->occupied);
            assert(FRAG_STATE_GET(pfrag->frag, FRAG_ALLOCED));
            assert(pfrag->frag->btl_header->seq == is);
            if (!FRAG_STATE_GET(pfrag->frag, FRAG_IN_HOTEL) &&
                !FRAG_STATE_GET(pfrag->frag, FRAG_SEND_ENQUEUED)) {
                opal_output(0, "=== ACKed frag in sent_frags array is not in hotel/enqueued, module %p, endpoint %p, frag %p, seq %" UDSEQ ", slot %lu",
                            (void*) module,
                            (void*) endpoint,
                            (void*) pfrag->frag,
                            is,
                            WINDOW_SIZE_MOD(is));
                ompi_btl_usnic_frag_dump(pfrag->frag);
            }
            assert(FRAG_STATE_GET(pfrag->frag, FRAG_IN_HOTEL) || 
                   FRAG_STATE_GET(pfrag->frag, FRAG_SEND_ENQUEUED));
            /* JMS END DEBUG */

            /* Conditinally return the send frag to the freelist */
            FRAG_STATE_SET(pfrag->frag, FRAG_SEND_ACKED);

            /* Check the sending frag out from the hotel.  NOTE: The
               frag might not actually be in a hotel room if it had
               already been evicted, but there were no send WQEs, and
               so it's off waiting in the pending send queue.  So if
               it's not in the hotel, don't check it out! */
            if (FRAG_STATE_ISSET(pfrag->frag, FRAG_IN_HOTEL)) {
                opal_hotel_checkout(&endpoint->endpoint_hotel, 
                                    pfrag->hotel_room);
                FRAG_STATE_CLR(pfrag->frag, FRAG_IN_HOTEL);
            }

            /* From the ACK perspective, we're done with this
               fragment.  (Conditionally) Return it to the
               freelist. */
            ompi_btl_usnic_frag_send_return_cond(module, pfrag->frag);

            /* Reset the entry in the sent_frags array */
            pfrag->frag = NULL;
            pfrag->occupied = false;
            pfrag->hotel_room = -1;
        }
        endpoint->endpoint_ack_seq_rcvd = ack_seq;

        goto repost;
    }
#endif /* RELIABILIY */

    /***********************************************************************/
    /* Have no idea what the frag is; drop it */
    else {
        ++module->num_unk_recvs;
        opal_output(0, "==========================unknown 2");
        goto repost;
    }

    /***********************************************************************/
 repost:
    ++module->num_recv_reposts;
    /* Add recv to linked list for reposting */
    frag->wr_desc.rd_desc.next = *repost_recv_head;
    *repost_recv_head = &frag->wr_desc.rd_desc;
}
