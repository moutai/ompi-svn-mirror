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
 * Copyright (c) 2013 Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ompi_config.h"

#include <string.h>

#include "ompi/proc/proc.h"

#include "btl_usnic.h"
#include "btl_usnic_frag.h"
#include "btl_usnic_ack.h"


static void ompi_btl_usnic_frag_reset_states(ompi_btl_usnic_frag_t *frag)
{
    frag->send_wr_posted = 0;
    frag->state_flags = 0;

#if HISTORY
    {
        int i;
        for (i = 0; i < NUM_FRAG_HISTORY; ++i) {
            memset(&(frag->history), 0, sizeof(frag->history[i]));
        }
        frag->history_start = 0;
        frag->history_next = 0;
    }
#endif
}


static void frag_common_constructor(ompi_btl_usnic_frag_t *frag)
{
#if 0
    /* JMS Not needed for USNIC UD */
    frag->ud_reg = (ompi_btl_usnic_reg_t*)frag->base.super.registration;
    frag->sg_entry.lkey = frag->ud_reg->mr->lkey;
#endif
    frag->base.des_flags = 0;
    frag->base.order = MCA_BTL_NO_ORDER;
    ompi_btl_usnic_frag_reset_states(frag);
}


static void send_frag_constructor(ompi_btl_usnic_frag_t *frag)
{
    frag_common_constructor(frag);

    frag->type = OMPI_BTL_USNIC_FRAG_SEND;
    frag->base.des_src = &frag->segment;
    frag->base.des_src_cnt = 1;
    frag->base.des_dst = NULL;
    frag->base.des_dst_cnt = 0;

    frag->protocol_header = frag->base.super.ptr;
    frag->btl_header = (ompi_btl_usnic_btl_header_t *) frag->base.super.ptr;
    frag->btl_header->sender = 
        mca_btl_usnic_component.my_hashed_orte_name;

    frag->payload.raw = (uint8_t *) (frag->btl_header + 1);

    /* This is what the PML sees */
    frag->segment.seg_addr.pval = frag->payload.raw;
    /* This is what the underlying transport (verbs) sees */
    frag->sg_entry.addr = (unsigned long) frag->protocol_header;

    frag->wr_desc.sr_desc.wr_id = (unsigned long) frag;
    frag->wr_desc.sr_desc.sg_list = &frag->sg_entry;
    frag->wr_desc.sr_desc.num_sge = 1;
    frag->wr_desc.sr_desc.opcode = IBV_WR_SEND;
    /* JMS Put inline back when it is tested more */
#if 0
    frag->wr_desc.sr_desc.send_flags = IBV_SEND_SIGNALED | IBV_SEND_INLINE;
#else
    frag->wr_desc.sr_desc.send_flags = IBV_SEND_SIGNALED;
#endif
    frag->wr_desc.sr_desc.next = NULL;
}


#if RELIABILITY
static void ack_frag_constructor(ompi_btl_usnic_frag_t *frag)
{
    send_frag_constructor(frag);

    frag->type = OMPI_BTL_USNIC_FRAG_ACK;
}
#endif

static void recv_frag_constructor(ompi_btl_usnic_frag_t *frag)
{
    frag_common_constructor(frag);

    frag->type = OMPI_BTL_USNIC_FRAG_RECV;
    frag->base.des_dst = &frag->segment;
    frag->base.des_dst_cnt = 1;
    frag->base.des_src = NULL;
    frag->base.des_src_cnt = 0;

    frag->protocol_header = frag->base.super.ptr;
    /* UD messages have a 40-byte global resource header (GRH) in
       received datagrams.  The BTL header is beyond this protocol
       header. */
    frag->btl_header = 
        (ompi_btl_usnic_btl_header_t *) (frag->protocol_header + 1);
    frag->payload.raw = (uint8_t *) (frag->btl_header + 1);

    /* This is what the PML sees */
    frag->segment.seg_addr.pval = frag->payload.raw;
    /* This is what the underlying transport sees (verbs) */
    frag->sg_entry.addr = (uintptr_t)frag->base.super.ptr;

    /* JMS Don't want to pull the eager limit from the template, but
       frag->endpoint is not yet initialized, so how do I pull it from
       the module? */
    frag->segment.seg_len = 
	    ompi_btl_usnic_module_template.super.btl_eager_limit;
    frag->sg_entry.length = 
	    ompi_btl_usnic_module_template.super.btl_eager_limit +
	    sizeof(ompi_btl_usnic_protocol_header_t) +
	    sizeof(ompi_btl_usnic_btl_header_t);

    frag->wr_desc.rd_desc.wr_id = (unsigned long)frag;
    frag->wr_desc.rd_desc.sg_list = &frag->sg_entry;
    frag->wr_desc.rd_desc.num_sge = 1;
    frag->wr_desc.rd_desc.next = NULL;
}


OBJ_CLASS_INSTANCE(ompi_btl_usnic_frag_t,
                   mca_btl_base_descriptor_t,
                   NULL,
                   NULL);

OBJ_CLASS_INSTANCE(ompi_btl_usnic_send_frag_t,
                   mca_btl_base_descriptor_t,
                   send_frag_constructor,
                   NULL);

#if RELIABILITY
OBJ_CLASS_INSTANCE(ompi_btl_usnic_ack_frag_t,
                   mca_btl_base_descriptor_t,
                   ack_frag_constructor,
                   NULL);
#endif

OBJ_CLASS_INSTANCE(ompi_btl_usnic_recv_frag_t,
                   mca_btl_base_descriptor_t,
                   recv_frag_constructor,
                   NULL);

/*******************************************************************************/

ompi_btl_usnic_frag_t *
ompi_btl_usnic_frag_send_alloc(ompi_btl_usnic_module_t *module)
{
    int rc;
    ompi_free_list_item_t *item;
    ompi_btl_usnic_frag_t *frag;

    OMPI_FREE_LIST_GET(&(module->send_frags), item, rc);
    if (OPAL_LIKELY(OMPI_SUCCESS == rc)) {
        frag = (ompi_btl_usnic_frag_t*) item;

        assert(frag);
        assert(OMPI_BTL_USNIC_FRAG_SEND == frag->type);
        assert(!FRAG_STATE_GET(frag, FRAG_ALLOCED));

        FRAG_STATE_SET(frag, FRAG_ALLOCED);
        return frag;
    }

    return NULL;
}


/* 
 * A send frag can be returned to the freelist when all of the
 * following are true:
 *
 * 1. PML is freeing it (via module.free())
 * 2. Or all of these:
 *    a) it finishes its send
 *    b) it has been ACKed
 *    c) it is owned by the BTL
 *    d) it is not queued up for a (re)send
 *
 * Note that because of sliding windows and ganged ACKs, it is
 * possible to have been ACKed already before the frag's send
 * completes (e.g., if an ACK comes in super fast, and/or if the
 * frag's pending completion is for a resent, and the ACK is for a
 * prior send/resend of this frag). 
 *
 * JMS This function is for debugging only -- it should be deleted
 * (and possibly just replaced with some asserts?) when we remove the
 * corresponding call to it in from _ack.c.
 */
bool ompi_btl_usnic_frag_send_ok_to_return(ompi_btl_usnic_module_t *module,
                                             ompi_btl_usnic_frag_t *frag)
{
    assert(frag);
    assert(OMPI_BTL_USNIC_FRAG_SEND == frag->type);

    if (FRAG_STATE_GET(frag, FRAG_SEND_ACKED) && 
        !FRAG_STATE_GET(frag, FRAG_SEND_ENQUEUED) &&
        0 == frag->send_wr_posted) {
        return true;
    }

    return false;
}

/* JMS This function should be inlined */
void ompi_btl_usnic_frag_send_return(ompi_btl_usnic_module_t *module,
                                     ompi_btl_usnic_frag_t *frag)
{
    assert(FRAG_STATE_GET(frag, FRAG_ALLOCED));
    assert(OMPI_BTL_USNIC_FRAG_SEND == frag->type);

    ompi_btl_usnic_frag_reset_states(frag);
    OMPI_FREE_LIST_RETURN(&(module->send_frags), &(frag->base.super));
}

/* JMS This function should be inlined */
void ompi_btl_usnic_frag_send_return_cond(struct ompi_btl_usnic_module_t *module,
                                            ompi_btl_usnic_frag_t *frag)
{
    assert(FRAG_STATE_GET(frag, FRAG_ALLOCED));
    assert(OMPI_BTL_USNIC_FRAG_SEND == frag->type);

    if (OPAL_LIKELY(frag->base.des_flags & MCA_BTL_DES_FLAGS_BTL_OWNERSHIP)) {
        if (FRAG_STATE_GET(frag, FRAG_SEND_ACKED) &&
            0 == frag->send_wr_posted && 
            !FRAG_STATE_GET(frag, FRAG_SEND_ENQUEUED)) {
            if (!FRAG_STATE_GET(frag, FRAG_PML_CALLED_BACK)) {
                opal_output(0, "=============== PML wasn't called back!  frag %p",
                            (void*) frag);
                ompi_btl_usnic_frag_dump(frag);
            }

            assert(FRAG_STATE_GET(frag, FRAG_SEND_ACKED));
            assert(0 == frag->send_wr_posted);
            assert(!FRAG_STATE_GET(frag, FRAG_SEND_ENQUEUED));
            assert(FRAG_STATE_GET(frag, FRAG_PML_CALLED_BACK));
            ompi_btl_usnic_frag_send_return(module, frag);
        }
    }
}


#if RELIABILITY
/* JMS This function should be inlined */
ompi_btl_usnic_frag_t *
ompi_btl_usnic_frag_ack_alloc(ompi_btl_usnic_module_t *module)
{
    int rc;
    ompi_free_list_item_t *item;
    ompi_btl_usnic_frag_t *ack;

    OMPI_FREE_LIST_GET(&(module->ack_frags), item, rc);
    if (OPAL_LIKELY(OMPI_SUCCESS == rc)) {
        ack = (ompi_btl_usnic_frag_t*) item;

        assert(ack);
        assert(OMPI_BTL_USNIC_FRAG_ACK == ack->type);
        assert(!FRAG_STATE_GET(ack, FRAG_ALLOCED));

        ompi_btl_usnic_frag_reset_states(ack);
        FRAG_STATE_SET(ack, FRAG_ALLOCED);
        return ack;
    }
    return NULL;
}


/* JMS This function should be inlined */
void ompi_btl_usnic_frag_ack_return(ompi_btl_usnic_module_t *module,
                                      ompi_btl_usnic_frag_t *ack)
{
    assert(ack);
    assert(FRAG_STATE_GET(ack, FRAG_ALLOCED));
    assert(OMPI_BTL_USNIC_FRAG_ACK == ack->type);

    FRAG_STATE_CLR(ack, FRAG_ALLOCED);
    OMPI_FREE_LIST_RETURN(&(module->ack_frags), &(ack->base.super));
}


/*
 * Progress the pending resends queue.  This function is called from
 * two places:
 *
 * component_progress(): 
 */
void 
ompi_btl_usnic_frag_progress_pending_resends(ompi_btl_usnic_module_t *module)
{
    if (OPAL_UNLIKELY(!opal_list_is_empty(&module->pending_resend_frags)) && 
        module->sd_wqe > 0) {
        ompi_btl_usnic_frag_t *frag;
        frag = (ompi_btl_usnic_frag_t*) 
            opal_list_remove_first(&module->pending_resend_frags);
        FRAG_STATE_CLR(frag, FRAG_SEND_ENQUEUED);
        ompi_btl_usnic_ack_timeout_part2(module, frag, 0);
    }
}
#endif

/*******************************************************************************/

#if RELIABILITY
static void dump_ack_frag(ompi_btl_usnic_frag_t* frag)
{
    char out[256];
    memset(out, 0, sizeof(out));

    snprintf(out, sizeof(out),
             "=== ACK frag %p (MCW %d): alloced %d", 
             (void*) frag, 
             ompi_proc_local()->proc_name.vpid,
             FRAG_STATE_ISSET(frag, FRAG_ALLOCED));
    opal_output(0, out);
}
#endif

static void dump_send_frag(ompi_btl_usnic_frag_t* frag)
{
    char out[256];
    memset(out, 0, sizeof(out));

    snprintf(out, sizeof(out),
             "=== SEND frag %p (MCW %d): alloced %d send_wr %d acked %d enqueued %d pml_callback %d hotel %d || seq %lu", 
             (void*) frag, 
             ompi_proc_local()->proc_name.vpid,
             FRAG_STATE_ISSET(frag, FRAG_ALLOCED),
             frag->send_wr_posted,
             FRAG_STATE_ISSET(frag, FRAG_SEND_ACKED),
             FRAG_STATE_ISSET(frag, FRAG_SEND_ENQUEUED),
             FRAG_STATE_ISSET(frag, FRAG_PML_CALLED_BACK),
             FRAG_STATE_ISSET(frag, FRAG_IN_HOTEL),
#if RELIABILITY
             FRAG_STATE_ISSET(frag, FRAG_ALLOCED) ?
                 frag->btl_header->seq : (ompi_btl_usnic_seq_t) ~0
#else
             (uint64_t) 0
#endif
             );
    opal_output(0, out);
}

static void dump_recv_frag(ompi_btl_usnic_frag_t* frag)
{
    char out[256];
    memset(out, 0, sizeof(out));

    snprintf(out, sizeof(out),
             "=== RECV frag %p (MCW %d): alloced %d posted %d", 
             (void*) frag, 
             ompi_proc_local()->proc_name.vpid,
             FRAG_STATE_ISSET(frag, FRAG_ALLOCED),
             FRAG_STATE_ISSET(frag, FRAG_RECV_WR_POSTED));
    opal_output(0, out);
}

void ompi_btl_usnic_frag_dump(ompi_btl_usnic_frag_t *frag)
{
    switch(frag->type) {
#if RELIABILITY
    case OMPI_BTL_USNIC_FRAG_ACK:
        dump_ack_frag(frag);
        break;
#endif

    case OMPI_BTL_USNIC_FRAG_SEND:
        dump_send_frag(frag);
        break;

    case OMPI_BTL_USNIC_FRAG_RECV:
        dump_recv_frag(frag);
        break;

    default:
        opal_output(0, "=== UNKNOWN type frag %p: (!)", (void*) frag);
        break;
    }
}

/*******************************************************************************/

#if HISTORY
void ompi_btl_usnic_frag_history(ompi_btl_usnic_frag_t *frag,
                                   char *file, int line,
                                   const char *message)
{
    int i = frag->history_next;
    ompi_btl_usnic_frag_history_t *h = &(frag->history[i]);

    memset(h, 0, sizeof(*h));
    strncpy(h->file, file, sizeof(h->file));
    h->line = line;
    strncpy(h->message, message, sizeof(h->message));

    frag->history_next = (frag->history_next + 1) % NUM_FRAG_HISTORY;
    if (frag->history_start == frag->history_next) {
        frag->history_start = (frag->history_start + 1) % NUM_FRAG_HISTORY;
    }
}
#endif
