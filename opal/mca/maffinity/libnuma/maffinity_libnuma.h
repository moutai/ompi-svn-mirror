/*
 * Copyright (c) 2004-2008 The Trustees of Indiana University and Indiana
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
 *
 * Processor affinity for libnuma.
 */


#ifndef MCA_MAFFINITY_LIBNUMA_EXPORT_H
#define MCA_MAFFINITY_LIBNUMA_EXPORT_H

#include "opal_config.h"

#include "opal/mca/mca.h"
#include "opal/mca/maffinity/maffinity.h"


#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif

    /**
     * Globally exported variable
     */
    OPAL_DECLSPEC extern const opal_maffinity_base_component_1_0_0_t
        mca_maffinity_libnuma_component;


    /**
     * maffinity query API function
     *
     * Query function for maffinity components.  Simply returns a priority
     * to rank it against other available maffinity components (assumedly,
     * only one component will be available per platform, but it's
     * possible that there could be more than one available).
     */
    int opal_maffinity_libnuma_component_query(mca_base_module_t **module, int *priority);

#if defined(c_plusplus) || defined(__cplusplus)
}
#endif
#endif /* MCA_MAFFINITY_LIBNUMA_EXPORT_H */
