/*
 * $HEADERS$
 */
#include "lam_config.h"
#include <stdio.h>

#include "mpi.h"
#include "mpi/interface/c/bindings.h"

#if LAM_HAVE_WEAK_SYMBOLS && LAM_PROFILING_DEFINES
#pragma weak MPI_Request_free = PMPI_Request_free
#endif

int MPI_Request_free(MPI_Request *request) {
    return MPI_SUCCESS;
}
