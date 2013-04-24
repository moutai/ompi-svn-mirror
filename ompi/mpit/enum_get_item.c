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

static const char FUNC_NAME[] = "MPI_T_enum_get_item";

int MPI_T_enum_get_item(MPI_T_enum enumtype, int index, int *value, char *name,
                        int *name_len)
{
    const char *tmp;
    int rc, count;

    if (!mpit_is_initialized ()) {
        return MPI_T_ERR_NOT_INITIALIZED;
    }

    mpit_lock ();

    do {
        rc = enumtype->get_count (enumtype, &count);
        if (OPAL_SUCCESS != rc) {
            rc = MPI_ERR_OTHER;
            break;
        }

        if (index >= count) {
            rc = MPI_T_ERR_INVALID_INDEX;
            break;
        }

        rc = enumtype->get_value(enumtype, index, value, &tmp);
        if (OPAL_SUCCESS != rc) { 
            rc = MPI_ERR_OTHER;
            break;
        }

        mpit_copy_string(name, name_len, tmp);
    } while (0);

    mpit_unlock ();

    return rc;
}
