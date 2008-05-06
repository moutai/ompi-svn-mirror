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


#include "orte_config.h"
#include "orte/constants.h"

#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"

#include "orte/mca/odls/base/base.h"
#include "orte/mca/odls/base/odls_private.h"


/**
 * Function for selecting one component from all those that are
 * available.
 */
int orte_odls_base_select(void)
{
    int ret, exit_status = OPAL_SUCCESS;
    orte_odls_base_component_t *best_component = NULL;
    orte_odls_base_module_t *best_module = NULL;

    orte_odls_base.selected = false;

    if (!orte_odls_base.components_available) {
        return ORTE_SUCCESS;
    }

    /*
     * Select the best component
     */
    if( OPAL_SUCCESS != (ret = mca_base_select("odls", orte_odls_globals.output,
                                               &orte_odls_base.available_components,
                                               (mca_base_module_t **) &best_module,
                                               (mca_base_component_t **) &best_component) ) ) {
        /* This will only happen if no component was selected */
        exit_status = ORTE_ERR_NOT_FOUND;
        goto cleanup;
    }

    /* Save the winner */
    orte_odls = *best_module;
    orte_odls_base.selected_component = *best_component;
    orte_odls_base.selected = true;

 cleanup:
    return exit_status;
}
