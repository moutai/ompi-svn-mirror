/*
 * $HEADER$
 */

#include "lam_config.h"

#include "mpi.h"
#include "mpi/c/bindings.h"
#include "lfc/lam_list.h"
#include "info/info.h"
#include <stdlib.h>
#include <string.h>

#if LAM_HAVE_WEAK_SYMBOLS && LAM_PROFILING_DEFINES
#pragma weak MPI_Info_get_valuelen = PMPI_Info_get_valuelen
#endif

#if LAM_PROFILING_DEFINES
#include "mpi/c/profile/defines.h"
#endif

/**
 *   MPI_Info_get_valuelen - Get the length of a value for a given key in an 'M
 *
 *   @param info - info object (handle)
 *   @param key - null-terminated character string of the index key
 *   @param valuelen - length of the value associated with 'key' (integer)
 *   @param flag - true (1) if 'key' defined on 'info', false (0) if not
 *   (logical)
 *
 *   @retval MPI_SUCCESS
 *   @retval MPI_ERR_ARG
 *   @retval MPI_ERR_INFO_KEY
 *
 *   The length returned in C and C++ does not include the end-of-string
 *   character.  If the 'key' is not found on 'info', 'valuelen' is left 
 *   alone.
 */
int MPI_Info_get_valuelen(MPI_Info info, char *key, int *valuelen,
                          int *flag) {

    int key_length;
    int err;

    /*
     * Simple function. All we need to do is search for the value
     * having the "key" associated with it and return the length
     */
    if (NULL == info){
        printf ("Invalid MPI_Info handle passed\n");
        return MPI_ERR_ARG;
    }

    key_length = (key) ? strlen (key) : 0;
    if ( (0 == key_length) || (MPI_MAX_INFO_KEY <= key_length)) {
        printf ("The key passed to MPI_INFO_SET is too long\n");
        return MPI_ERR_INFO_KEY;
    }

    err = lam_info_get_valuelen (info, key, valuelen, flag);

    /*
     * Once again, the error problem. lam_info_get_valuelen
     * does not have an obvious error return.
     */
    return err;
}
