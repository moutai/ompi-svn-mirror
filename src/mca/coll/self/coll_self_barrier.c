/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 The Trustees of the University of Tennessee.
 *                         All rights reserved.
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

#include "include/constants.h"
#include "coll_self.h"


/*
 *	barrier_intra
 *
 *	Function:	- barrier
 *	Accepts:	- same as MPI_Barrier()
 *	Returns:	- MPI_SUCCESS
 */
int mca_coll_self_barrier_intra(struct ompi_communicator_t *comm)
{
    /* Since there is only one process, this is a no-op */

    return MPI_SUCCESS;
}
