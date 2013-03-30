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


void ompi_btl_usnic_send_complete(ompi_btl_usnic_module_t *module,
                                    ompi_btl_usnic_frag_t *frag)
{
    /* Reap a frag that was sent */
    --frag->send_wr_posted;

    if (frag->base.des_flags & MCA_BTL_DES_SEND_ALWAYS_CALLBACK) {
        frag->base.des_cbfunc(&module->super,
                              frag->endpoint, &frag->base, 
                              OMPI_SUCCESS);
        FRAG_STATE_SET(frag, FRAG_PML_CALLED_BACK);
        ++module->pml_send_callbacks;

        /* This frag will cycle back through here if it is resent.
           And according to the PML, it's already complete.  So let's
           turn off the SEND_ALWAYS_CALLBACK flag on this frag so that
           it doesn't invoke the PML callback again. */
        frag->base.des_flags &= ~MCA_BTL_DES_SEND_ALWAYS_CALLBACK;
    }

    /* Return the frag to the freelist (or not; this function does
       the Right Things) */
    ompi_btl_usnic_frag_send_return_cond(module, frag);

    ++module->sd_wqe;
#if RELIABILITY
    ompi_btl_usnic_frag_progress_pending_resends(module);
#endif
}
