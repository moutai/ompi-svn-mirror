/* -*- Mode: C; c-basic-offset:4 ; -*- */

/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 The Trustees of the University of Tennessee.
 *                         All rights reserved.
 * Copyright (c) 2004 The Ohio State University.
 *                    All rights reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */
#include "ompi_config.h"
#include "include/types.h"
#include "mca/pml/base/pml_base_sendreq.h"
#include "ptl_gm.h"
#include "ptl_gm_req.h"

/*
static void mca_ptl_gm_send_request_construct (
    mca_ptl_gm_send_request_t *);
static void mca_ptl_gm_send_request_destruct (
    mca_ptl_gm_send_request_t *);


ompi_class_t mca_ptl_gm_send_request_t_class = {
    "mca_ptl_gm_send_request_t",
    OBJ_CLASS (mca_pml_base_send_request_t),
    (ompi_construct_t) mca_ptl_gm_send_request_construct,
    (ompi_destruct_t) mca_ptl_gm_send_request_destruct
};


void
mca_ptl_gm_send_request_construct (
    mca_ptl_gm_send_request_t * request)
{
    OBJ_CONSTRUCT (&request->req_frag, mca_ptl_gm_send_frag_t);
}


void
mca_ptl_gm_send_request_destruct (
    mca_ptl_gm_send_request_t * request)
{
    OBJ_DESTRUCT (&request->req_frag);
}
*/
