/*
 * $HEADER$
 */

#include "ompi_config.h"

#include <stdio.h>

#include "mpi.h"
#include "mpi/f77/bindings.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILE_LAYER
#pragma weak PMPI_FILE_GET_SIZE = mpi_file_get_size_f
#pragma weak pmpi_file_get_size = mpi_file_get_size_f
#pragma weak pmpi_file_get_size_ = mpi_file_get_size_f
#pragma weak pmpi_file_get_size__ = mpi_file_get_size_f
#elif OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (PMPI_FILE_GET_SIZE,
                           pmpi_file_get_size,
                           pmpi_file_get_size_,
                           pmpi_file_get_size__,
                           pmpi_file_get_size_f,
                           (MPI_Fint *fh, MPI_Fint *size, MPI_Fint *ierr),
                           (fh, size, ierr) )
#endif

#if OMPI_HAVE_WEAK_SYMBOLS
#pragma weak MPI_FILE_GET_SIZE = mpi_file_get_size_f
#pragma weak mpi_file_get_size = mpi_file_get_size_f
#pragma weak mpi_file_get_size_ = mpi_file_get_size_f
#pragma weak mpi_file_get_size__ = mpi_file_get_size_f
#endif

#if ! OMPI_HAVE_WEAK_SYMBOLS && ! OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (MPI_FILE_GET_SIZE,
                           mpi_file_get_size,
                           mpi_file_get_size_,
                           mpi_file_get_size__,
                           mpi_file_get_size_f,
                           (MPI_Fint *fh, MPI_Fint *size, MPI_Fint *ierr),
                           (fh, size, ierr) )
#endif


#if OMPI_PROFILE_LAYER && ! OMPI_HAVE_WEAK_SYMBOLS
#include "mpi/f77/profile/defines.h"
#endif

void mpi_file_get_size_f(MPI_Fint *fh, MPI_Fint *size, MPI_Fint *ierr)
{

}
