/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
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

#include "mpi.h"
#include "ompi/constants.h"
#include "opal/util/output.h"
#include "ompi/mca/coll/coll.h"
#include "ompi/mca/coll/base/base.h"
#include "coll_demo.h"


/*
 *	alltoall_intra
 *
 *	Function:	- MPI_Alltoall 
 *	Accepts:	- same as MPI_Alltoall()
 *	Returns:	- MPI_SUCCESS or an MPI error code
 */
int mca_coll_demo_alltoall_intra(void *sbuf, int scount,
                                 struct ompi_datatype_t *sdtype, 
                                 void *rbuf, int rcount, 
                                 struct ompi_datatype_t *rdtype,
                                 struct ompi_communicator_t *comm)
{
    opal_output_verbose(10, mca_coll_base_output, "In demo alltoall_intra\n");
    return comm->c_coll_basic_module->coll_alltoall(sbuf, scount, sdtype,
                                                    rbuf, rcount, rdtype, 
                                                    comm);
}


/*
 *	alltoall_inter
 *
 *	Function:	- MPI_Alltoall 
 *	Accepts:	- same as MPI_Alltoall()
 *	Returns:	- MPI_SUCCESS or an MPI error code
 */
int mca_coll_demo_alltoall_inter(void *sbuf, int scount,
                                 struct ompi_datatype_t *sdtype, 
                                 void *rbuf, int rcount, 
                                 struct ompi_datatype_t *rdtype,
                                 struct ompi_communicator_t *comm)
{
    opal_output_verbose(10, mca_coll_base_output, "In demo alltoall_inter\n");
    return comm->c_coll_basic_module->coll_alltoall(sbuf, scount, sdtype,
                                                    rbuf, rcount, rdtype, 
                                                    comm);
}
