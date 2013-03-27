/*
 * Copyright (c) 2010      Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */


#include "opal_config.h"

#include "opal/constants.h"
#include "opal/util/output.h"
#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"

#include "opal/mca/event/event.h"
#include "opal/mca/event/base/base.h"


/*
 * The following file was created by configure.  It contains extern
 * statements and the definition of an array of pointers to each
 * component's public mca_base_component_t struct.
 */
#include "opal/mca/event/base/static-components.h"


/*
 * Globals
 */
int opal_event_base_output = -1;
opal_list_t opal_event_components;
opal_event_base_t *opal_event_base=NULL;
int opal_event_base_inited = 0;

/*
 * Locals
 */
static int opal_event_base_verbose = 0;

static int opal_event_base_register(int flags)
{
    /* Debugging / verbose output */
    opal_event_base_verbose = 0;
    (void) mca_base_var_register("opal", "event", "base", "verbose",
                                 "Verbosity level of the event framework",
                                 MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                 OPAL_INFO_LVL_9,
                                 MCA_BASE_VAR_SCOPE_READONLY,
                                 &opal_event_base_verbose);

    return OPAL_SUCCESS;
}

int opal_event_base_open(void)
{
    int rc;

    if( opal_event_base_inited++ < 0 ) {
        return OPAL_SUCCESS;
    }

    (void) opal_event_base_register(0);

    if (0 != opal_event_base_verbose) {
        opal_event_base_output = opal_output_open(NULL);
    } else {
        opal_event_base_output = -1;
    }

    /* to support tools such as ompi_info, add the components
     * to a list
     */
    OBJ_CONSTRUCT(&opal_event_components, opal_list_t);
    if (OPAL_SUCCESS !=
        mca_base_components_open("event", opal_event_base_output,
                                 mca_event_base_static_components,
                                 &opal_event_components, true)) {
        return OPAL_ERROR;
    }

    /* init the lib */
    if (OPAL_SUCCESS != (rc = opal_event_init())) {
        return rc;
    }

    /* Declare our intent to use threads. If event library internal
     * thread support was not enabled during configuration, this
     * function defines to no-op
     */
    opal_event_use_threads();

    /* get our event base */
    if (NULL == (opal_event_base = opal_event_base_create())) {
        return OPAL_ERROR;
    }

    /* set the number of priorities */
    if (0 < OPAL_EVENT_NUM_PRI) {
        opal_event_base_priority_init(opal_event_base, OPAL_EVENT_NUM_PRI);
    }

    return rc;
}
