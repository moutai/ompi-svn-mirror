/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 The Trustees of the University of Tennessee.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */


#include "ompi_config.h"
#include <stdio.h>

#include "include/constants.h"
#include "util/output.h"
#include "mca/mca.h"
#include "mca/base/base.h"
#include "mca/base/mca_base_param.h"
#include "mca/coll/coll.h"
#include "mca/coll/base/base.h"


/*
 * The following file was created by configure.  It contains extern
 * statements and the definition of an array of pointers to each
 * component's public mca_base_component_t struct.
 */
#include "mca/coll/base/static-components.h"



/*
 * Global variables; most of which are loaded by back-ends of MCA
 * variables
 */
int mca_coll_base_param = -1;
int mca_coll_base_output = -1;

int mca_coll_base_crossover = 4;
int mca_coll_base_associative = 1;
int mca_coll_base_reduce_crossover = 4;
int mca_coll_base_bcast_collmaxlin = 4;
int mca_coll_base_bcast_collmaxdim = 64;

bool mca_coll_base_components_opened_valid = false;
ompi_list_t mca_coll_base_components_opened;


/*
 * Function for finding and opening either all MCA components, or the one
 * that was specifically requested via a MCA parameter.
 */
int mca_coll_base_open(void)
{
  /* Open an output stream for this framework */

  mca_coll_base_output = ompi_output_open(NULL);

  /* Open up all available components */

  if (OMPI_SUCCESS != 
      mca_base_components_open("coll", mca_coll_base_output,
                               mca_coll_base_static_components, 
                               &mca_coll_base_components_opened)) {
    return OMPI_ERROR;
  }
  mca_coll_base_components_opened_valid = true;

  /* Find the index of the MCA "coll" param for selection */

  mca_coll_base_param = mca_base_param_find("coll", "base", NULL);

  /* All done */

  return OMPI_SUCCESS;
}
