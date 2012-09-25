/*
 * Copyright (c) 2011-2012 Los Alamos National Security, LLC.
 *                         All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */


#include "orte_config.h"
#include "orte/constants.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"
#include "opal/mca/base/mca_base_param.h"
#include "opal/util/output.h"

#include "orte/mca/state/base/base.h"
#include "orte/mca/state/base/state_private.h"

int orte_state_base_select(void)
{
    int exit_status = OPAL_SUCCESS;
    orte_state_base_component_t *best_component = NULL;
    orte_state_base_module_t *best_module = NULL;

    /*
     * Select the best component
     */
    if( OPAL_SUCCESS != mca_base_select("state", orte_state_base_output,
                                        &orte_state_base_components_available,
                                        (mca_base_module_t **) &best_module,
                                        (mca_base_component_t **) &best_component) ) {
        /* This will only happen if no component was selected */
        exit_status = ORTE_ERROR;
        goto cleanup;
    }

    /* Save the winner */
    orte_state_base_selected_component = *best_component;
    orte_state = *best_module;

    /* Initialize the winner */
    if (NULL != best_module) {
        if (OPAL_SUCCESS != orte_state.init()) {
            exit_status = OPAL_ERROR;
            goto cleanup;
        }
    }

 cleanup:
    return exit_status;
}
