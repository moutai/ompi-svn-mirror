/*
 * Copyright (c) 2004-2007 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
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
#include "opal/mca/memchecker/memchecker.h"
#include "opal/mca/memchecker/base/base.h"


/*
 * The following file was created by configure.  It contains extern
 * statements and the definition of an array of pointers to each
 * component's public mca_base_component_t struct.
 */
#include "opal/mca/memchecker/base/static-components.h"

/*
 * Globals
 */
int opal_memchecker_base_output = -1;
bool opal_memchecker_base_components_opened_valid = false;
opal_list_t opal_memchecker_base_components_opened;

/*
 * Locals
 */
static int opal_memchecker_base_verbose = 0;

static int opal_memchecker_base_register(int flags)
{
    /* Debugging / verbose output */
    opal_memchecker_base_verbose = 0;
    (void) mca_base_var_register("opal", "memchecker", "base", "verbose",
                                 "Verbosity level of the memchecker framework",
                                 MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                 OPAL_INFO_LVL_9,
                                 MCA_BASE_VAR_SCOPE_READONLY,
                                 &opal_memchecker_base_verbose);

    return OPAL_SUCCESS;
}

/*
 * Function for finding and opening either all MCA components, or the one
 * that was specifically requested via a MCA parameter.
 */
int opal_memchecker_base_open(void)
{
    (void) opal_memchecker_base_register(0);

    OBJ_CONSTRUCT( &opal_memchecker_base_components_opened, opal_list_t );

    if (0 != opal_memchecker_base_verbose) {
        opal_memchecker_base_output = opal_output_open(NULL);
    } else {
        opal_memchecker_base_output = -1;
    }

    opal_memchecker_base_components_opened_valid = false;

    /* Open up all available components */
    if (OPAL_SUCCESS !=
        mca_base_components_open("memchecker", opal_memchecker_base_output,
                                 mca_memchecker_base_static_components,
                                 &opal_memchecker_base_components_opened,
                                 true)) {
        return OPAL_ERROR;
    }

    opal_memchecker_base_components_opened_valid = true;
 
    /* All done */
    return OPAL_SUCCESS;
}

