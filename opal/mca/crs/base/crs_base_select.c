/*
 * Copyright (c) 2004-2010 The Trustees of Indiana University.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 The Trustees of the University of Tennessee.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2007      Evergrid, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "opal_config.h"

#ifdef HAVE_UNISTD_H
#include "unistd.h"
#endif

#include "opal/constants.h"
#include "opal/util/output.h"
#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"
#include "opal/mca/crs/crs.h"
#include "opal/mca/crs/base/base.h"

extern bool crs_base_do_not_select;

int opal_crs_base_select(void)
{
    int ret, exit_status = OPAL_SUCCESS;
    opal_crs_base_component_t *best_component = NULL;
    opal_crs_base_module_t *best_module = NULL;

    if( !opal_cr_is_enabled ) {
        opal_output_verbose(10, opal_crs_base_output,
                            "crs:select: FT is not enabled, skipping!");
        return OPAL_SUCCESS;
    }

    if( crs_base_do_not_select ) {
        opal_output_verbose(10, opal_crs_base_output,
                            "crs:select: Not selecting at this time!");
        return OPAL_SUCCESS;
    }

    /*
     * Select the best component
     */
    if( OPAL_SUCCESS != mca_base_select("crs", opal_crs_base_output,
                                        &opal_crs_base_components_available,
                                        (mca_base_module_t **) &best_module,
                                        (mca_base_component_t **) &best_component) ) {
        /* This will only happen if no component was selected */
        exit_status = OPAL_ERROR;
        goto cleanup;
    }

    /* Save the winner */
    opal_crs_base_selected_component = *best_component;
    opal_crs = *best_module;

    /* Initialize the winner */
    if (NULL != best_module) {
        if (OPAL_SUCCESS != (ret = opal_crs.crs_init()) ) {
            exit_status = ret;
            goto cleanup;
        }
    }

 cleanup:
    return exit_status;
}
