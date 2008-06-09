/*
 * Copyright (c) 2007      Los Alamos National Security, LLC.
 *                         All rights reserved. 
 * Copyright (c) 2004-2008 The Trustees of Indiana University.
 *                         All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orte_config.h"
#include "orte/constants.h"
#include "orte/types.h"

#include "orte/util/show_help.h"
#include "opal/class/opal_hash_table.h"
#include "opal/mca/base/base.h"
#include "opal/mca/base/mca_base_param.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/runtime/orte_globals.h"

#include "orte/mca/routed/base/base.h"
#include "routed_linear.h"

static int orte_routed_linear_component_query(mca_base_module_t **module, int *priority);

/**
 * component definition
 */
orte_routed_component_t mca_routed_linear_component = {
      /* First, the mca_base_component_t struct containing meta
         information about the component itself */

      {
        /* Indicate that we are a rml v1.0.0 component (which also
           implies a specific MCA version) */

        ORTE_ROUTED_BASE_VERSION_1_0_0,

        "linear", /* MCA component name */
        ORTE_MAJOR_VERSION,  /* MCA component major version */
        ORTE_MINOR_VERSION,  /* MCA component minor version */
        ORTE_RELEASE_VERSION,  /* MCA component release version */
        NULL,
        NULL,
        orte_routed_linear_component_query
      },

      /* Next the MCA v1.0.0 component meta data */
      {
          /* This component can be checkpointed */
          MCA_BASE_METADATA_PARAM_CHECKPOINT
      }
};

static int orte_routed_linear_component_query(mca_base_module_t **module, int *priority)
{
    *priority = 40;
    *module = (mca_base_module_t *) &orte_routed_linear_module;
    return ORTE_SUCCESS;
}
