/*
 * $HEADER$
 */

#include "ompi_config.h"

#include <stdio.h>

#include "mpi.h"
#include "mpi/f77/bindings.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILE_LAYER
#pragma weak PMPI_WIN_CREATE_KEYVAL = mpi_win_create_keyval_f
#pragma weak pmpi_win_create_keyval = mpi_win_create_keyval_f
#pragma weak pmpi_win_create_keyval_ = mpi_win_create_keyval_f
#pragma weak pmpi_win_create_keyval__ = mpi_win_create_keyval_f
#elif OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (PMPI_WIN_CREATE_KEYVAL,
                           pmpi_win_create_keyval,
                           pmpi_win_create_keyval_,
                           pmpi_win_create_keyval__,
                           pmpi_win_create_keyval_f,
                           (MPI_Fint *win_copy_attr_fn, MPI_Fint *win_delete_attr_fn, MPI_Fint *win_keyval, char *extra_state, MPI_Fint *ierr),
                           (win_copy_attr_fn, win_delete_attr_fn, win_keyval, extra_state, ierr) )
#endif

#if OMPI_HAVE_WEAK_SYMBOLS
#pragma weak MPI_WIN_CREATE_KEYVAL = mpi_win_create_keyval_f
#pragma weak mpi_win_create_keyval = mpi_win_create_keyval_f
#pragma weak mpi_win_create_keyval_ = mpi_win_create_keyval_f
#pragma weak mpi_win_create_keyval__ = mpi_win_create_keyval_f
#endif

#if ! OMPI_HAVE_WEAK_SYMBOLS && ! OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (MPI_WIN_CREATE_KEYVAL,
                           mpi_win_create_keyval,
                           mpi_win_create_keyval_,
                           mpi_win_create_keyval__,
                           mpi_win_create_keyval_f,
                           (MPI_Fint *win_copy_attr_fn, MPI_Fint *win_delete_attr_fn, MPI_Fint *win_keyval, char *extra_state, MPI_Fint *ierr),
                           (win_copy_attr_fn, win_delete_attr_fn, win_keyval, extra_state, ierr) )
#endif


#if OMPI_PROFILE_LAYER && ! OMPI_HAVE_WEAK_SYMBOLS
#include "mpi/c/profile/defines.h"
#endif

void mpi_win_create_keyval_f(MPI_Fint *win_copy_attr_fn, MPI_Fint *win_delete_attr_fn, MPI_Fint *win_keyval, char *extra_state, MPI_Fint *ierr)
{

}
