/*
 * $HEADER$
 */

#include "ompi_config.h"

#include <stdio.h>

#include "mpi.h"
#include "mpi/f77/bindings.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILE_LAYER
#pragma weak PMPI_TYPE_FREE_KEYVAL = mpi_type_free_keyval_f
#pragma weak pmpi_type_free_keyval = mpi_type_free_keyval_f
#pragma weak pmpi_type_free_keyval_ = mpi_type_free_keyval_f
#pragma weak pmpi_type_free_keyval__ = mpi_type_free_keyval_f
#elif OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (PMPI_TYPE_FREE_KEYVAL,
                           pmpi_type_free_keyval,
                           pmpi_type_free_keyval_,
                           pmpi_type_free_keyval__,
                           pmpi_type_free_keyval_f,
                           (MPI_Fint *type_keyval, MPI_Fint *ierr),
                           (type_keyval, ierr) )
#endif

#if OMPI_HAVE_WEAK_SYMBOLS
#pragma weak MPI_TYPE_FREE_KEYVAL = mpi_type_free_keyval_f
#pragma weak mpi_type_free_keyval = mpi_type_free_keyval_f
#pragma weak mpi_type_free_keyval_ = mpi_type_free_keyval_f
#pragma weak mpi_type_free_keyval__ = mpi_type_free_keyval_f
#endif

#if ! OMPI_HAVE_WEAK_SYMBOLS && ! OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (MPI_TYPE_FREE_KEYVAL,
                           mpi_type_free_keyval,
                           mpi_type_free_keyval_,
                           mpi_type_free_keyval__,
                           mpi_type_free_keyval_f,
                           (MPI_Fint *type_keyval, MPI_Fint *ierr),
                           (type_keyval, ierr) )
#endif


#if OMPI_PROFILE_LAYER && ! OMPI_HAVE_WEAK_SYMBOLS
#include "mpi/f77/profile/defines.h"
#endif

void mpi_type_free_keyval_f(MPI_Fint *type_keyval, MPI_Fint *ierr)
{

}
