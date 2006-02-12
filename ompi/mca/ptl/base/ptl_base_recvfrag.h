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
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */
/**
 * @file
 */
#ifndef MCA_PTL_BASE_RECVFRAG_H
#define MCA_PTL_BASE_RECVFRAG_H

#include "ompi/mca/ptl/ptl.h"
#include "ompi/mca/ptl/base/ptl_base_recvreq.h"
#include "ompi/mca/ptl/base/ptl_base_fragment.h"
#include "ompi/mca/ptl/base/ptl_base_match.h"

#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif
OMPI_DECLSPEC extern opal_class_t mca_ptl_base_recv_frag_t_class;

/**
 * Base type for receive fragment descriptors.
 */
struct mca_ptl_base_recv_frag_t {
    mca_ptl_base_frag_t frag_base; /**< base fragment descriptor */
    mca_ptl_base_recv_request_t *frag_request; /**< matched posted receive */
    bool frag_is_buffered; /**< does fragment need to be unpacked into users buffer */
};
typedef struct mca_ptl_base_recv_frag_t mca_ptl_base_recv_frag_t;

#if defined(c_plusplus) || defined(__cplusplus)
}
#endif

#endif

