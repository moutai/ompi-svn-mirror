/*
 * $HEADER$
 */

#include "ompi_config.h"

#include <stdio.h>

#include "mpi.h"
#include "mpi/f77/bindings.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILE_LAYER
#pragma weak PMPI_INFO_CREATE = mpi_info_create_f
#pragma weak pmpi_info_create = mpi_info_create_f
#pragma weak pmpi_info_create_ = mpi_info_create_f
#pragma weak pmpi_info_create__ = mpi_info_create_f
#elif OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (PMPI_INFO_CREATE,
                           pmpi_info_create,
                           pmpi_info_create_,
                           pmpi_info_create__,
                           pmpi_info_create_f,
                           (MPI_Fint *info, MPI_Fint *ierr),
                           (info, ierr) )
#endif

#if OMPI_HAVE_WEAK_SYMBOLS
#pragma weak MPI_INFO_CREATE = mpi_info_create_f
#pragma weak mpi_info_create = mpi_info_create_f
#pragma weak mpi_info_create_ = mpi_info_create_f
#pragma weak mpi_info_create__ = mpi_info_create_f
#endif

#if ! OMPI_HAVE_WEAK_SYMBOLS && ! OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (MPI_INFO_CREATE,
                           mpi_info_create,
                           mpi_info_create_,
                           mpi_info_create__,
                           mpi_info_create_f,
                           (MPI_Fint *info, MPI_Fint *ierr),
                           (info, ierr) )
#endif


#if OMPI_PROFILE_LAYER && ! OMPI_HAVE_WEAK_SYMBOLS
#include "mpi/f77/profile/defines.h"
#endif

void mpi_info_create_f(MPI_Fint *info, MPI_Fint *ierr)
{

}
