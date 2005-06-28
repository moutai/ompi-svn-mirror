/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 The Trustees of the University of Tennessee.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "ompi_config.h"

#include "mpi/f77/bindings.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILE_LAYER
#pragma weak PMPI_BUFFER_DETACH = mpi_buffer_detach_f
#pragma weak pmpi_buffer_detach = mpi_buffer_detach_f
#pragma weak pmpi_buffer_detach_ = mpi_buffer_detach_f
#pragma weak pmpi_buffer_detach__ = mpi_buffer_detach_f
#elif OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (PMPI_BUFFER_DETACH,
                           pmpi_buffer_detach,
                           pmpi_buffer_detach_,
                           pmpi_buffer_detach__,
                           pmpi_buffer_detach_f,
                           (char *buffer, MPI_Fint *size, MPI_Fint *ierr),
                           (buffer, size, ierr) )
#endif

#if OMPI_HAVE_WEAK_SYMBOLS
#pragma weak MPI_BUFFER_DETACH = mpi_buffer_detach_f
#pragma weak mpi_buffer_detach = mpi_buffer_detach_f
#pragma weak mpi_buffer_detach_ = mpi_buffer_detach_f
#pragma weak mpi_buffer_detach__ = mpi_buffer_detach_f
#endif

#if ! OMPI_HAVE_WEAK_SYMBOLS && ! OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (MPI_BUFFER_DETACH,
                           mpi_buffer_detach,
                           mpi_buffer_detach_,
                           mpi_buffer_detach__,
                           mpi_buffer_detach_f,
                           (char *buffer, MPI_Fint *size, MPI_Fint *ierr),
                           (buffer, size, ierr) )
#endif


#if OMPI_PROFILE_LAYER && ! OMPI_HAVE_WEAK_SYMBOLS
#include "mpi/f77/profile/defines.h"
#endif

void mpi_buffer_detach_f(char *buffer, MPI_Fint *size, MPI_Fint *ierr)
{
    OMPI_SINGLE_NAME_DECL(size);
    *ierr = OMPI_INT_2_FINT(MPI_Buffer_detach(buffer, 
					      OMPI_SINGLE_NAME_CONVERT(size)));
    if (MPI_SUCCESS == OMPI_FINT_2_INT(*ierr)) {
        OMPI_SINGLE_INT_2_FINT(size);
    }
}
