/*
 * $HEADER$
 */

#include "lam_config.h"

#include <stdio.h>

#include "mpi.h"
#include "mpi/f77/bindings.h"

#if LAM_HAVE_WEAK_SYMBOLS && LAM_PROFILE_LAYER
#pragma weak PMPI_FILE_GET_ERRHANDLER = mpi_file_get_errhandler_f
#pragma weak pmpi_file_get_errhandler = mpi_file_get_errhandler_f
#pragma weak pmpi_file_get_errhandler_ = mpi_file_get_errhandler_f
#pragma weak pmpi_file_get_errhandler__ = mpi_file_get_errhandler_f
#elif LAM_PROFILE_LAYER
LAM_GENERATE_F77_BINDINGS (PMPI_FILE_GET_ERRHANDLER,
                           pmpi_file_get_errhandler,
                           pmpi_file_get_errhandler_,
                           pmpi_file_get_errhandler__,
                           pmpi_file_get_errhandler_f,
                           (MPI_Fint *file, MPI_Fint *errhandler, MPI_Fint *ierr),
                           (file, errhandler, ierr) )
#endif

#if LAM_HAVE_WEAK_SYMBOLS
#pragma weak MPI_FILE_GET_ERRHANDLER = mpi_file_get_errhandler_f
#pragma weak mpi_file_get_errhandler = mpi_file_get_errhandler_f
#pragma weak mpi_file_get_errhandler_ = mpi_file_get_errhandler_f
#pragma weak mpi_file_get_errhandler__ = mpi_file_get_errhandler_f
#endif

#if ! LAM_HAVE_WEAK_SYMBOLS && ! LAM_PROFILE_LAYER
LAM_GENERATE_F77_BINDINGS (MPI_FILE_GET_ERRHANDLER,
                           mpi_file_get_errhandler,
                           mpi_file_get_errhandler_,
                           mpi_file_get_errhandler__,
                           mpi_file_get_errhandler_f,
                           (MPI_Fint *file, MPI_Fint *errhandler, MPI_Fint *ierr),
                           (file, errhandler, ierr) )
#endif

void mpi_file_get_errhandler_f(MPI_Fint *file, MPI_Fint *errhandler, MPI_Fint *ierr)
{

}
