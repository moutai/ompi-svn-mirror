/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 The Trustees of the University of Tennessee.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "ompi_config.h"

#include <stdio.h>

#include "mpi.h"
#include "mpi/f77/bindings.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILE_LAYER
#pragma weak PMPI_GET_ADDRESS = mpi_get_address_f
#pragma weak pmpi_get_address = mpi_get_address_f
#pragma weak pmpi_get_address_ = mpi_get_address_f
#pragma weak pmpi_get_address__ = mpi_get_address_f
#elif OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (PMPI_GET_ADDRESS,
                           pmpi_get_address,
                           pmpi_get_address_,
                           pmpi_get_address__,
                           pmpi_get_address_f,
                           (char *location, MPI_Fint *address, MPI_Fint *ierr),
                           (location, address, ierr) )
#endif

#if OMPI_HAVE_WEAK_SYMBOLS
#pragma weak MPI_GET_ADDRESS = mpi_get_address_f
#pragma weak mpi_get_address = mpi_get_address_f
#pragma weak mpi_get_address_ = mpi_get_address_f
#pragma weak mpi_get_address__ = mpi_get_address_f
#endif

#if ! OMPI_HAVE_WEAK_SYMBOLS && ! OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (MPI_GET_ADDRESS,
                           mpi_get_address,
                           mpi_get_address_,
                           mpi_get_address__,
                           mpi_get_address_f,
                           (char *location, MPI_Fint *address, MPI_Fint *ierr),
                           (location, address, ierr) )
#endif


#if OMPI_PROFILE_LAYER && ! OMPI_HAVE_WEAK_SYMBOLS
#include "mpi/f77/profile/defines.h"
#endif

void mpi_get_address_f(char *location, MPI_Fint *address, MPI_Fint *ierr)
{
    MPI_Aint c_address;

    *ierr = OMPI_INT_2_FINT(MPI_Get_address(location, &c_address));
    *address = (MPI_Fint) c_address;
}
