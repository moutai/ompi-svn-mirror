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

static const char FUNC_NAME[] = "MPI_T_category_get_categories";

int MPI_T_category_get_categories(int cat_index, int len, int indices[])
{
    const mca_base_var_group_t *group;
    const int *subgroups;
    int rc = MPI_SUCCESS;
    int i, size;

    if (!mpit_is_initialized ()) {
        return MPI_T_ERR_NOT_INITIALIZED;
    }

    mpit_lock ();

    do {
        rc = mca_base_var_group_get (cat_index, &group);
        if (0 > rc) {
            rc = (OPAL_ERR_NOT_FOUND == rc) ? MPI_T_ERR_INVALID_INDEX : MPI_ERR_OTHER;
            break;
        }

        size = opal_value_array_get_size((opal_value_array_t *) &group->group_subgroups);
        subgroups = OPAL_VALUE_ARRAY_GET_BASE(&group->group_subgroups, int);

        for (i = 0 ; i < len && i < size ; ++i) {
            indices[i] = subgroups[i];
        }
    } while (0);

    mpit_unlock ();

    return rc;
}
