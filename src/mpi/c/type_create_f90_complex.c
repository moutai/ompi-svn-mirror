/*
 * $HEADER$
 */

#include "ompi_config.h"

#include "mpi.h"
#include "mpi/c/bindings.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILING_DEFINES
#pragma weak MPI_Type_create_f90_complex = PMPI_Type_create_f90_complex
#endif

#if OMPI_PROFILING_DEFINES
#include "mpi/c/profile/defines.h"
#endif

int
MPI_Type_create_f90_complex(int p, int r, MPI_Datatype *newtype)
{
    return MPI_SUCCESS;
}
