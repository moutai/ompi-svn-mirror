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

#include "include/constants.h"
#include "coll_self.h"


/*
 *	scatterv_intra
 *
 *	Function:	- scatterv
 *	Accepts:	- same arguments as MPI_Scatter()
 *	Returns:	- MPI_SUCCESS or error code
 */
int mca_coll_self_scatterv_intra(void *sbuf, int *scounts,
                                 int *disps, struct ompi_datatype_t *sdtype,
                                 void *rbuf, int rcount,
                                 struct ompi_datatype_t *rdtype, int root,
                                 struct ompi_communicator_t *comm)
{
    return ompi_ddt_sndrcv(((char *) sbuf) + disps[0], scounts[0], sdtype,
                           rbuf, rcount, rdtype);
}
