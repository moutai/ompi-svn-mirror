/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2006 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2009      Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */


#include "ompi_config.h"
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#include "ompi/mca/bml/base/base.h"
#include "ompi/mca/btl/base/base.h"
#include "ompi/mca/bml/base/static-components.h"
#include "opal/mca/base/base.h"

static int mca_bml_base_register(mca_base_register_flag_t flags);
static int mca_bml_base_open(mca_base_open_flag_t flags);
static int mca_bml_base_close(void);

MCA_BASE_FRAMEWORK_DECLARE(ompi, bml, "BTL Multiplexing Layer", mca_bml_base_register,
                           mca_bml_base_open, mca_bml_base_close, mca_bml_base_static_components,
                           0);

#if OPAL_ENABLE_DEBUG_RELIABILITY
int mca_bml_base_error_rate_floor;
int mca_bml_base_error_rate_ceiling;
int mca_bml_base_error_count;
static bool mca_bml_base_srand;
#endif

static int mca_bml_base_register(mca_base_register_flag_t flags)
{
#if OPAL_ENABLE_DEBUG_RELIABILITY
    do {
        int var_id;

        mca_bml_base_error_rate_floor = 0;
        var_id = mca_base_var_register("ompi", "bml", "base", "error_rate_floor", NULL,
                                       MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                       OPAL_INFO_LVL_9,
                                       MCA_BASE_VAR_SCOPE_READONLY,
                                       &mca_bml_base_error_rate_floor);
        (void) mca_base_var_register_synonym(var_id, "ompi", "bml", NULL, "error_rate_floor",
                                             MCA_BASE_VAR_SYN_FLAG_DEPRECATED);

        mca_bml_base_error_rate_ceiling = 0;
        var_id = mca_base_var_register("ompi", "bml", "base", "error_rate_ceiling", NULL,
                                       MCA_BASE_VAR_TYPE_INT, NULL, 0, 0,
                                       OPAL_INFO_LVL_9,
                                       MCA_BASE_VAR_SCOPE_READONLY,
                                       &mca_bml_base_error_rate_ceiling);
        (void) mca_base_var_register_synonym(var_id, "ompi", "bml", NULL, "error_rate_ceiling",
                                             MCA_BASE_VAR_SYN_FLAG_DEPRECATED);


        mca_bml_base_srand = true;
        var_id = mca_base_var_register("ompi", "bml", "base", "srand", NULL,
                                       MCA_BASE_VAR_TYPE_BOOL, NULL, 0, 0,
                                       OPAL_INFO_LVL_9,
                                       MCA_BASE_VAR_SCOPE_READONLY,
                                       &mca_bml_base_srand);
        (void) mca_base_var_register_synonym(var_id, "ompi", "bml", NULL, "srand",
                                             MCA_BASE_VAR_SYN_FLAG_DEPRECATED);
    } while (0);
#endif

    return OMPI_SUCCESS;
}

static int mca_bml_base_open(mca_base_open_flag_t flags) 
{
    int ret;

    if(OMPI_SUCCESS !=
       (ret = mca_base_framework_components_open(&ompi_bml_base_framework, flags))) {
        return ret;
    }

#if OPAL_ENABLE_DEBUG_RELIABILITY
    /* seed random number generator */
    if(mca_bml_base_srand) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        srand(getpid() * tv.tv_usec);
    }

    /* initialize count */
    if(mca_bml_base_error_rate_ceiling > 0 
       && mca_bml_base_error_rate_floor <= mca_bml_base_error_rate_ceiling) {
        mca_bml_base_error_count = (int) (((double) mca_bml_base_error_rate_ceiling * rand())/(RAND_MAX+1.0));
    }
#endif

    return mca_base_framework_open(&ompi_btl_base_framework, 0);
}

static int mca_bml_base_close( void )
{
  return mca_base_framework_close(&ompi_btl_base_framework);
}
