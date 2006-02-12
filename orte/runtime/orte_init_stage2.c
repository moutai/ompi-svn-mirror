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

/** @file **/

#include "orte_config.h"

#include "orte/orte_constants.h"

#include "opal/util/show_help.h"

#include "orte/mca/errmgr/errmgr.h"

#include "orte/mca/rml/rml.h"
#include "orte/mca/ns/ns.h"
#include "orte/mca/gpr/gpr.h"

#include "orte/mca/rmgr/base/base.h"
#include "orte/mca/soh/base/base.h"
#include "orte/mca/iof/base/base.h"

#include "orte/runtime/runtime.h"

int orte_init_stage2(void)
{
    int ret;
    char *error;

    /* 
     * Initialize the selected modules now that all components/name are available.
     */

    if (ORTE_SUCCESS != (ret = orte_rml.init())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_rml.init";
        goto error;
    }

    if (ORTE_SUCCESS != (ret = orte_ns.init())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_ns.init";
        goto error;
    }

    if (ORTE_SUCCESS != (ret = orte_gpr.init())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_gpr.init";
        goto error;
    }

    /*
     * setup I/O forwarding system
     */
    if (ORTE_SUCCESS != (ret = orte_iof_base_open())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_iof_base_open";
        goto error;
    }
    if (ORTE_SUCCESS != (ret = orte_iof_base_select())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_iof_base_select";
        goto error;
    }
    
     /* 
     * All done 
     */
error:
    if (ret != ORTE_SUCCESS) {
        opal_show_help("help-orte-runtime",
                       "orte_init:startup:internal-failure",
                       true, error, ret);
    }
    
    return ret;
}
