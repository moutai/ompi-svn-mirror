/*
 * Copyright (c) 2012      Mellanox Technologies, Inc.
 *                         All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "oshmem_config.h"
#include "oshmem/shmem/f77/bindings.h"
#include "oshmem/include/shmem.h"
#include "oshmem/shmem/shmem_api_logger.h"
#include "stdio.h"

OMPI_GENERATE_F77_BINDINGS (ompi_fortran_logical_t,
        SHMEM_PE_ACCESSIBLE,
        shmem_pe_accessible_,
        shmem_pe_accessible__,
        shmem_pe_accessible_f,
        (MPI_Fint *pe), 
        (pe) )

ompi_fortran_logical_t shmem_pe_accessible_f(MPI_Fint *pe)
{
    return OMPI_INT_2_LOGICAL(shmem_pe_accessible(OMPI_FINT_2_INT(*pe)));
}

