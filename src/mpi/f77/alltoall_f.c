/*
 * $HEADER$
 */

#include "lam_config.h"

#include <stdio.h>

#include "mpi.h"
#include "mpi/f77/bindings.h"

#if LAM_HAVE_WEAK_SYMBOLS && LAM_PROFILE_LAYER
#pragma weak PMPI_ALLTOALL = mpi_alltoall_f
#pragma weak pmpi_alltoall = mpi_alltoall_f
#pragma weak pmpi_alltoall_ = mpi_alltoall_f
#pragma weak pmpi_alltoall__ = mpi_alltoall_f
#elif LAM_PROFILE_LAYER
LAM_GENERATE_F77_BINDINGS (PMPI_ALLTOALL,
                           pmpi_alltoall,
                           pmpi_alltoall_,
                           pmpi_alltoall__,
                           pmpi_alltoall_f,
                           (char *sendbuf, MPI_Fint *sendcount, MPI_Fint *sendtype, char *recvbuf, MPI_Fint *recvcount, MPI_Fint *recvtype, MPI_Fint *comm, MPI_Fint *ierr),
                           (sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm, ierr) )
#endif

#if LAM_HAVE_WEAK_SYMBOLS
#pragma weak MPI_ALLTOALL = mpi_alltoall_f
#pragma weak mpi_alltoall = mpi_alltoall_f
#pragma weak mpi_alltoall_ = mpi_alltoall_f
#pragma weak mpi_alltoall__ = mpi_alltoall_f
#endif

#if ! LAM_HAVE_WEAK_SYMBOLS && ! LAM_PROFILE_LAYER
LAM_GENERATE_F77_BINDINGS (MPI_ALLTOALL,
                           mpi_alltoall,
                           mpi_alltoall_,
                           mpi_alltoall__,
                           mpi_alltoall_f,
                           (char *sendbuf, MPI_Fint *sendcount, MPI_Fint *sendtype, char *recvbuf, MPI_Fint *recvcount, MPI_Fint *recvtype, MPI_Fint *comm, MPI_Fint *ierr),
                           (sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, comm, ierr) )
#endif

void mpi_alltoall_f(char *sendbuf, MPI_Fint *sendcount, MPI_Fint *sendtype, char *recvbuf, MPI_Fint *recvcount, MPI_Fint *recvtype, MPI_Fint *comm, MPI_Fint *ierr)
{

}
