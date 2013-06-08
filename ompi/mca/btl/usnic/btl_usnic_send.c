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

#include "opal_stdint.h"

#include "ompi/constants.h"
#include "ompi/mca/btl/btl.h"
#include "ompi/mca/btl/base/base.h"
#include "common_verbs.h"

#include "btl_usnic.h"
#include "btl_usnic_frag.h"
#include "btl_usnic_send.h"
#include "btl_usnic_ack.h"


/*
 * This function is called when a send of a full-fragment segment completes
 * Return the WQE and also return the segment if no ACK pending
 */
void
ompi_btl_usnic_frag_send_complete(ompi_btl_usnic_module_t *module,
                                    ompi_btl_usnic_send_segment_t *sseg)
{
    ompi_btl_usnic_send_frag_t *frag;

    frag = sseg->ss_parent_frag;

    /* Reap a frag that was sent */
    --sseg->ss_send_posted;
    --frag->sf_seg_post_cnt;

    /* checks for returnability made inside */
    ompi_btl_usnic_send_frag_return_cond(module, frag);

    /* do bookkeeping */
    ++frag->sf_endpoint->endpoint_send_credits;
    ++sseg->ss_channel->sd_wqe;

    /* see if this endpoint needs to be made ready-to-send */
    ompi_btl_usnic_check_rts(frag->sf_endpoint);

}

/*
 * This function is called when a send segment completes
 * Return the WQE and also return the segment if no ACK pending
 */
void
ompi_btl_usnic_chunk_send_complete(ompi_btl_usnic_module_t *module,
                                    ompi_btl_usnic_send_segment_t *sseg)
{
    ompi_btl_usnic_send_frag_t *frag;

    frag = sseg->ss_parent_frag;

    /* Reap a frag that was sent */
    --sseg->ss_send_posted;
    --frag->sf_seg_post_cnt;

    if (sseg->ss_send_posted == 0 && !sseg->ss_ack_pending) {
        ompi_btl_usnic_chunk_segment_return(module, sseg);
    }

    /* done with whole fragment? */
    /* checks for returnability made inside */
    ompi_btl_usnic_send_frag_return_cond(module, frag);

    /* do bookkeeping */
    ++frag->sf_endpoint->endpoint_send_credits;
    ++sseg->ss_channel->sd_wqe;

    /* see if this endpoint needs to be made ready-to-send */
    ompi_btl_usnic_check_rts(frag->sf_endpoint);
}
