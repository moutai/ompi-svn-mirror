/*
 * $HEADERS$
 */
#include "lam_config.h"
#include <stdio.h>

#include "mpi.h"
#include "mpi/c/bindings.h"
#include "errhandler/errhandler.h"
#include "communicator/communicator.h"
#include "win/win.h"

#if LAM_HAVE_WEAK_SYMBOLS && LAM_PROFILING_DEFINES
#pragma weak MPI_Win_call_errhandler = PMPI_Win_call_errhandler
#endif

int MPI_Win_call_errhandler(MPI_Win win, int errorcode) {
  /* Error checking */

  if (MPI_PARAM_CHECK) {
    if (NULL == win ||
        MPI_WIN_NULL == win) {
      return LAM_ERRHANDLER_INVOKE(MPI_COMM_WORLD, MPI_ERR_ARG,
                                   "MPI_Win_call_errhandler");
    }
  }

  /* Invoke the errhandler */

  return LAM_ERRHANDLER_INVOKE(win, errorcode,
                               "MPI_Win_call_errhandler");
}
