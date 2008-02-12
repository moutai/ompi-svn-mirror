/*
 * Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2008 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2006      Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */
#include "ompi_config.h"
#include <stdio.h>

#include "ompi/mpi/c/bindings.h"
#include "ompi/datatype/datatype.h"
#include "ompi/datatype/convertor.h"
#include "ompi/memchecker.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILING_DEFINES
#pragma weak MPI_Pack_external_size = PMPI_Pack_external_size
#endif

#if OMPI_PROFILING_DEFINES
#include "ompi/mpi/c/profile/defines.h"
#endif

static const char FUNC_NAME[] = "MPI_Pack_external_size";


int MPI_Pack_external_size(char *datarep, int incount,
                           MPI_Datatype datatype, MPI_Aint *size) 
{
    ompi_convertor_t local_convertor;
    size_t length;

    OPAL_CR_TEST_CHECKPOINT_READY();

    MEMCHECKER(
        memchecker_datatype(datatype);
    );
    
    if (MPI_PARAM_CHECK) {
        OMPI_ERR_INIT_FINALIZE(FUNC_NAME);
        if (NULL == size) {
            return OMPI_ERRHANDLER_INVOKE(MPI_COMM_WORLD, MPI_ERR_ARG, FUNC_NAME);
        } else if (MPI_DATATYPE_NULL == datatype || NULL == datatype) {
            return OMPI_ERRHANDLER_INVOKE(MPI_COMM_WORLD, MPI_ERR_TYPE, FUNC_NAME);
        }
    }

    OBJ_CONSTRUCT(&local_convertor, ompi_convertor_t);

    /* the resulting convertor will be set to the position ZERO */
    ompi_convertor_copy_and_prepare_for_send( ompi_mpi_external32_convertor,
                                              datatype, incount, NULL,
                                              CONVERTOR_SEND_CONVERSION,
                                              &local_convertor );

    ompi_convertor_get_packed_size( &local_convertor, &length );
    *size = (MPI_Aint)length;
    OBJ_DESTRUCT( &local_convertor );
    
    return OMPI_SUCCESS;
}
