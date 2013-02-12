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
#include "oshmem/mca/spml/spml.h"
#include "ompi/datatype/ompi_datatype.h"
#include "stdio.h"

OMPI_GENERATE_F77_BINDINGS (void,
        SHMEM_IPUT128,
        shmem_iput128_,
        shmem_iput128__,
        shmem_iput128_f,
        (FORTRAN_POINTER_T target, FORTRAN_POINTER_T source, MPI_Fint *tst, MPI_Fint *sst, MPI_Fint *len, MPI_Fint *pe), 
        (target,source,tst,sst,len,pe) )

void shmem_iput128_f(FORTRAN_POINTER_T target, FORTRAN_POINTER_T source, MPI_Fint *tst, MPI_Fint *sst, MPI_Fint *len, MPI_Fint *pe)
{
    int i;
    int length = OMPI_FINT_2_INT(*len);
    int tst_c = OMPI_FINT_2_INT(*tst);
    int sst_c = OMPI_FINT_2_INT(*sst);

    for (i=0; i<length; i++)
    {  
        MCA_SPML_CALL(put((uint8_t *)FPTR_2_VOID_PTR(target) + i * tst_c * 16, 
            16, 
            (uint8_t *)FPTR_2_VOID_PTR(source) + i * sst_c * 16, 
            OMPI_FINT_2_INT(*pe)));

    }
}
    
