/*
 * $HEADER$
 */

#include "lam_config.h"

#include <stdio.h>

#include "mpi.h"
#include "mpi/f77/bindings.h"

#if LAM_HAVE_WEAK_SYMBOLS && LAM_PROFILE_LAYER
#pragma weak PMPI_WIN_FREE = mpi_win_free_f
#pragma weak pmpi_win_free = mpi_win_free_f
#pragma weak pmpi_win_free_ = mpi_win_free_f
#pragma weak pmpi_win_free__ = mpi_win_free_f
#elif LAM_PROFILE_LAYER
LAM_GENERATE_F77_BINDINGS (PMPI_WIN_FREE,
                           pmpi_win_free,
                           pmpi_win_free_,
                           pmpi_win_free__,
                           pmpi_win_free_f,
                           (MPI_Fint *win, MPI_Fint *ierr),
                           (win, ierr) )
#endif

#if LAM_HAVE_WEAK_SYMBOLS
#pragma weak MPI_WIN_FREE = mpi_win_free_f
#pragma weak mpi_win_free = mpi_win_free_f
#pragma weak mpi_win_free_ = mpi_win_free_f
#pragma weak mpi_win_free__ = mpi_win_free_f
#endif

#if ! LAM_HAVE_WEAK_SYMBOLS && ! LAM_PROFILE_LAYER
LAM_GENERATE_F77_BINDINGS (MPI_WIN_FREE,
                           mpi_win_free,
                           mpi_win_free_,
                           mpi_win_free__,
                           mpi_win_free_f,
                           (MPI_Fint *win, MPI_Fint *ierr),
                           (win, ierr) )
#endif


#if LAM_PROFILE_LAYER && ! LAM_HAVE_WEAK_SYMBOLS
#include "mpi/c/profile/defines.h"
#endif

void mpi_win_free_f(MPI_Fint *win, MPI_Fint *ierr)
{

}
