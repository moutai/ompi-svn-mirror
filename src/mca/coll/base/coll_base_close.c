/*
 * $HEADER$
 */

#include "ompi_config.h"

#include <stdio.h>

#include "include/constants.h"
#include "mca/mca.h"
#include "mca/base/base.h"
#include "mca/coll/coll.h"
#include "mca/coll/base/base.h"


int mca_coll_base_close(void)
{
  /* Close all components that are still open.  This may be the opened
     list (if we're in ompi_info), or it may be the available list (if
     we're anywhere else). */

  if (mca_coll_base_components_opened_valid) {
    mca_base_components_close(mca_coll_base_output,
                              &mca_coll_base_components_opened, NULL);
    mca_coll_base_components_opened_valid = false;
  } else if (mca_coll_base_components_available_valid) {
    mca_base_components_close(mca_coll_base_output,
                              &mca_coll_base_components_available, NULL);
    mca_coll_base_components_available_valid = false;
  }

  /* All done */

  return OMPI_SUCCESS;
}
