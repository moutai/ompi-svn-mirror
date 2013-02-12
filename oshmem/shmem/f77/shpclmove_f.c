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
#include "oshmem/runtime/runtime.h"

OMPI_GENERATE_F77_BINDINGS (void,
        SHPCLMOVE,
        shpclmove_,
        shpclmove__,
        shpclmove_f,
        (FORTRAN_POINTER_T *addr, MPI_Fint *length, MPI_Fint *status, MPI_Fint *abort), 
        (addr,length,status,abort) )


void shpclmove_f(FORTRAN_POINTER_T *addr, MPI_Fint *length, MPI_Fint *status, MPI_Fint *abort)
{
    FORTRAN_POINTER_T prev_addr = *addr;
   
    *status = 0;
    if (*length <= 0)
    {
        *status = -1;
        goto Exit;
    }

    *addr = (FORTRAN_POINTER_T)shrealloc(FPTR_2_VOID_PTR(*addr), OMPI_FINT_2_INT(*length) * 4);

    if (*addr == 0)
    {
        *status = -2;
        goto Exit;
    }

    if (prev_addr != *addr)
    {
        *status = 1;
    }

Exit:
    if (*status < 0)
    {
        if (*abort) 
        {
            oshmem_shmem_abort(-1);
        }
    }
}

