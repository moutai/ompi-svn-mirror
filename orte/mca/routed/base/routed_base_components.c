/*
 * Copyright (c) 2007      Los Alamos National Security, LLC.
 *                         All rights reserved. 
 * Copyright (c) 2007      Sun Microsystems, Inc.  All rights reserved.
 * Copyright (c) 2004-2010 The Trustees of Indiana University.
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
#include "opal/class/opal_bitmap.h"
#include "opal/dss/dss.h"
#include "opal/util/output.h"
#include "opal/mca/base/mca_base_component_repository.h"

#include "orte/util/proc_info.h"
#include "orte/runtime/orte_globals.h"

#include "orte/mca/routed/routed.h"
#include "orte/mca/routed/base/base.h"


/* The following file was created by configure.  It contains extern
 * statements and the definition of an array of pointers to each
 * component's public mca_base_component_t struct. */
#include "orte/mca/routed/base/static-components.h"

#if ORTE_DISABLE_FULL_SUPPORT
/* have to include a bogus function here so that
 * the build system sees at least one function
 * in the library
 */
int orte_routed_base_open(void)
{
    return ORTE_SUCCESS;
}

#else

static void construct(orte_routed_tree_t *rt)
{
    rt->vpid = ORTE_VPID_INVALID;
    OBJ_CONSTRUCT(&rt->relatives, opal_bitmap_t);
}
static void destruct(orte_routed_tree_t *rt)
{
    OBJ_DESTRUCT(&rt->relatives);
}
OBJ_CLASS_INSTANCE(orte_routed_tree_t, opal_list_item_t,
                   construct, destruct);

OBJ_CLASS_INSTANCE(orte_routed_jobfam_t, opal_object_t,
                   NULL, NULL);

int orte_routed_base_output = -1;
orte_routed_module_t orte_routed = {0};
opal_list_t orte_routed_base_components;
opal_mutex_t orte_routed_base_lock;
opal_condition_t orte_routed_base_cond;
bool orte_routed_base_wait_sync;

static orte_routed_component_t *active_component = NULL;
static bool component_open_called = false;
static bool opened = false;
static bool selected = false;

int
orte_routed_base_open(void)
{
    int ret;

    if (opened) {
        return ORTE_SUCCESS;
    }
    opened = true;
    
    /* setup the output stream */
    orte_routed_base_output = opal_output_open(NULL);
    OBJ_CONSTRUCT(&orte_routed_base_lock, opal_mutex_t);
    OBJ_CONSTRUCT(&orte_routed_base_cond, opal_condition_t);
    orte_routed_base_wait_sync = false;
    
    /* Initialize globals */
    OBJ_CONSTRUCT(&orte_routed_base_components, opal_list_t);
    
    /* Initialize storage of remote hnp uris */
    OBJ_CONSTRUCT(&orte_remote_hnps, opal_buffer_t);
    /* prime it with our HNP uri */
    opal_dss.pack(&orte_remote_hnps, &orte_process_info.my_hnp_uri, 1, OPAL_STRING);

    /* Open up all available components */
    ret = mca_base_components_open("routed",
                                   orte_routed_base_output,
                                   mca_routed_base_static_components, 
                                   &orte_routed_base_components,
                                   true);
    component_open_called = true;

    return ret;
}

int
orte_routed_base_select(void)
{
    int ret, exit_status = OPAL_SUCCESS;
    orte_routed_component_t *best_component = NULL;
    orte_routed_module_t *best_module = NULL;

    if (selected) {
        return ORTE_SUCCESS;
    }
    selected = true;
    
    /*
     * Select the best component
     */
    if( OPAL_SUCCESS != mca_base_select("routed", orte_routed_base_output,
                                        &orte_routed_base_components,
                                        (mca_base_module_t **) &best_module,
                                        (mca_base_component_t **) &best_component) ) {
        /* This will only happen if no component was selected */
        exit_status = ORTE_ERR_NOT_FOUND;
        goto cleanup;
    }

    /* Save the winner */
    orte_routed = *best_module;
    active_component = best_component;

    /* initialize the selected component */
    opal_output_verbose(10, orte_routed_base_output,
                        "orte_routed_base_select: initializing selected component %s",
                        best_component->base_version.mca_component_name);
    if (ORTE_SUCCESS != (ret = orte_routed.initialize()) ) {
        exit_status = ret;
        goto cleanup;
    }

 cleanup:
    return exit_status;
}


int
orte_routed_base_close(void)
{
    /* finalize the selected component */
    if (NULL != orte_routed.finalize) {
        orte_routed.finalize();
    }
    
    OBJ_DESTRUCT(&orte_remote_hnps);

    /* shutdown any remaining opened components */
    if (component_open_called) {
        mca_base_components_close(orte_routed_base_output, 
                                  &orte_routed_base_components, NULL);
    }

    OBJ_DESTRUCT(&orte_routed_base_components);
    OBJ_DESTRUCT(&orte_routed_base_lock);
    OBJ_DESTRUCT(&orte_routed_base_cond);
    
    opened   = false;
    selected = false;

    return ORTE_SUCCESS;
}

#endif /* ORTE_DISABLE_FULL_SUPPORT */
