/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2012-2013 Los Alamos National Security, LLC. All rights
 *                         reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ompi/mpit/mpit-internal.h"

static const char FUNC_NAME[] = "MPI_T_catagory_get_num";

int MPI_T_category_get_num (int *num_cat)
{
    if (!mpit_is_initialized ()) {
        return MPI_T_ERR_NOT_INITIALIZED;
    }

    if (MPI_PARAM_CHECK && NULL == num_cat) {
        return MPI_ERR_ARG;
    }

    mpit_lock ();
    *num_cat = mca_base_var_group_get_count ();
    mpit_unlock ();

    return MPI_SUCCESS;
}
