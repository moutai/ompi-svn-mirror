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
        SHMEM_CHARACTER_PUT,
        shmem_character_put_,
        shmem_character_put__,
        shmem_character_put_f,
        (FORTRAN_POINTER_T target, FORTRAN_POINTER_T source, MPI_Fint *length, MPI_Fint *pe), 
        (target,source,length,pe) )

void shmem_character_put_f(FORTRAN_POINTER_T target, FORTRAN_POINTER_T source, MPI_Fint *length, MPI_Fint *pe)
{
    size_t character_type_size = 0;
    ompi_datatype_type_size(&ompi_mpi_character.dt, &character_type_size);

    MCA_SPML_CALL(put(FPTR_2_VOID_PTR(target), 
        OMPI_FINT_2_INT(*length) * character_type_size, 
        FPTR_2_VOID_PTR(source), 
        OMPI_FINT_2_INT(*pe)));
}


