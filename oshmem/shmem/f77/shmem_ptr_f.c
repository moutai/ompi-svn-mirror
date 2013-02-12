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

OMPI_GENERATE_F77_BINDINGS (FORTRAN_POINTER_T *,
        SHMEM_PTR,
        shmem_ptr_,
        shmem_ptr__,
        shmem_ptr_f,
        (FORTRAN_POINTER_T target, MPI_Fint *pe), 
        (target,pe) )

FORTRAN_POINTER_T* shmem_ptr_f(FORTRAN_POINTER_T target, MPI_Fint *pe)
{
    return (FORTRAN_POINTER_T *)shmem_ptr(FPTR_2_VOID_PTR(target), OMPI_FINT_2_INT(*pe));
}

