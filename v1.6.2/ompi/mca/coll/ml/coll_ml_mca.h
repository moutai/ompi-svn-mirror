/*
 * Copyright (c) 2009-2012 Oak Ridge National Laboratory.  All rights reserved.
 * Copyright (c) 2009-2012 Mellanox Technologies.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
 /** @file */

#ifndef MCA_COLL_ML_MCA_H
#define MCA_COLL_ML_MCA_H

#include<ctype.h>
#include "ompi_config.h"

int reg_int(const char* param_name,
        const char* deprecated_param_name,
        const char* param_desc,
        int default_value, int *out_value, int flags);

int mca_coll_ml_register_params(void);

#endif
