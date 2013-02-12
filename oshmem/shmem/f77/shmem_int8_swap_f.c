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
#include "oshmem/runtime/runtime.h"
#include "oshmem/mca/atomic/atomic.h"
#include "ompi/datatype/ompi_datatype.h"
#include "stdio.h"

OMPI_GENERATE_F77_BINDINGS (ompi_fortran_integer8_t,
        SHMEM_INT8_SWAP,
        shmem_int8_swap_,
        shmem_int8_swap__,
        shmem_int8_swap_f,
        (FORTRAN_POINTER_T target, FORTRAN_POINTER_T value, MPI_Fint *pe), 
        (target,value,pe) )

ompi_fortran_integer8_t shmem_int8_swap_f(FORTRAN_POINTER_T target, FORTRAN_POINTER_T value, MPI_Fint *pe)
{
    ompi_fortran_integer8_t out_value = 0;

    MCA_ATOMIC_CALL(cswap(FPTR_2_VOID_PTR(target), 
        (void *)&out_value, 
        NULL, 
        FPTR_2_VOID_PTR(value), 
        sizeof(out_value), 
        OMPI_FINT_2_INT(*pe)));

    return out_value;
}

