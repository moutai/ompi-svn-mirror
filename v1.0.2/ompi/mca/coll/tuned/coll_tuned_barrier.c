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
#include "ompi/include/constants.h"
#include "datatype/datatype.h"
#include "communicator/communicator.h"
#include "mca/coll/coll.h"
#include "mca/coll/base/coll_tags.h"
#include "mca/pml/pml.h"
#include "op/op.h"
#include "coll_tuned.h"
#include "coll_tuned_topo.h"
#include "coll_tuned_util.h"

#include <sys/types.h>
#include <unistd.h>


int mca_coll_tuned_barrier_intra_doublering(struct ompi_communicator_t *comm)
{
    int rank, size;
    int err=0, line=0;
    int left, right;


    rank = ompi_comm_rank(comm);
    size = ompi_comm_size(comm);

    printf("mca_coll_tuned_bcast_intra_doublering rank %d\n", rank);
  
    left = ((rank-1)%size);
    right = ((rank+1)%size);

    if (rank > 0) { /* receive message from the left */
        err = MCA_PML_CALL(recv((void*)NULL, 0, MPI_BYTE, left, 
                              MCA_COLL_BASE_TAG_BARRIER, comm, MPI_STATUS_IGNORE));
        if (err != MPI_SUCCESS) { line = __LINE__; goto err_hndl; }
    }

    /* Send message to the right */
    err = MCA_PML_CALL(send((void*)NULL, 0, MPI_BYTE, right, MCA_COLL_BASE_TAG_BARRIER, 
                            MCA_PML_BASE_SEND_STANDARD, comm));
    if (err != MPI_SUCCESS) { line = __LINE__; goto err_hndl;  }

    /* root needs to receive from the last node */
    if (rank == 0) {
        err = MCA_PML_CALL(recv((void*)NULL, 0, MPI_BYTE, left, 
                              MCA_COLL_BASE_TAG_BARRIER, comm, MPI_STATUS_IGNORE));
        if (err != MPI_SUCCESS) { line = __LINE__; goto err_hndl; }
    }

    /* Allow nodes to exit */
    if (rank > 0) { /* post Receive from left */
        err = MCA_PML_CALL(recv((void*)NULL, 0, MPI_BYTE, left, 
                              MCA_COLL_BASE_TAG_BARRIER, comm, MPI_STATUS_IGNORE));
        if (err != MPI_SUCCESS) { line = __LINE__; goto err_hndl; }
    }

    /* send message to the right one */
    err = MCA_PML_CALL(send((void*)NULL, 0, MPI_BYTE, right, MCA_COLL_BASE_TAG_BARRIER, 
                            MCA_PML_BASE_SEND_STANDARD, comm));
    if (err != MPI_SUCCESS) { line = __LINE__; goto err_hndl;  }
 
    /* rank 0 post receive from the last node */
    if (rank == 0) {
        err = MCA_PML_CALL(recv((void*)NULL, 0, MPI_BYTE, left, 
                              MCA_COLL_BASE_TAG_BARRIER, comm, MPI_STATUS_IGNORE));
        if (err != MPI_SUCCESS) { line = __LINE__; goto err_hndl;  }
    }

    return MPI_SUCCESS;

 err_hndl:
   fprintf(stderr,"%s:%4d\tError occurred %d\n",__FILE__,line,err);
   return err;
}


int mca_coll_tuned_barrier_intra_recursivedoubling(struct ompi_communicator_t *comm)
{
    int rank, size, adjsize;
    int i, err, line;
    int mask, remote;

    rank = ompi_comm_rank(comm);
    size = ompi_comm_size(comm);
    printf("mca_coll_tuned_bcast_intra_recursivedoubling rank %d\n", rank);

    /* do nearest power of 2 less than size calc */
    adjsize = 1;
    for(i=0;adjsize*2<size;adjsize*=2) { }

    /* if size is not exact power of two, perform an extra step */
    if (adjsize != size) {
        if (rank >= adjsize) {
            /* send message to lower ranked node */
            err = MCA_PML_CALL(send((void*)NULL, 0, MPI_BYTE, rank-adjsize, 
                            MCA_COLL_BASE_TAG_BARRIER, MCA_PML_BASE_SEND_STANDARD, comm));

            if (err != MPI_SUCCESS) { line = __LINE__; goto err_hndl;}

            /* post receive from lower ranked node */
            err = MCA_PML_CALL(recv((void*)NULL, 0, MPI_BYTE, rank-adjsize,
                              MCA_COLL_BASE_TAG_BARRIER, comm, MPI_STATUS_IGNORE));

            if (err != MPI_SUCCESS) { line = __LINE__; goto err_hndl;}

        } else if (rank < (size - adjsize)) {

            /* receive message from high level rank */
            err = MCA_PML_CALL(recv((void*)NULL, 0, MPI_BYTE, rank+adjsize,
                              MCA_COLL_BASE_TAG_BARRIER, comm, MPI_STATUS_IGNORE));

            if (err != MPI_SUCCESS) { line = __LINE__; goto err_hndl;}
        }
    }

    /* exchange messages */
    if ( rank < adjsize ) {
        mask = 0x1;
        while ( mask < adjsize ) {
            remote = rank ^ mask;
            mask <<= 1;
            if (remote >= adjsize) continue;

            err = coll_tuned_sendrecv (NULL, 0, MPI_BYTE, remote, MCA_COLL_BASE_TAG_BARRIER, 
                                   NULL, 0, MPI_BYTE, remote, MCA_COLL_BASE_TAG_BARRIER,
                                   comm, MPI_STATUS_IGNORE, rank);

            if (err != MPI_SUCCESS) { line = __LINE__; goto err_hndl;}
        }
    }

    /* non-power of 2 case */
    if (adjsize != size) {
        if (rank < (size - adjsize)) {

            /* send enter message to higher ranked node */
            err = MCA_PML_CALL(send((void*)NULL, 0, MPI_BYTE, rank+adjsize, 
                            MCA_COLL_BASE_TAG_BARRIER, MCA_PML_BASE_SEND_STANDARD, comm));

            if (err != MPI_SUCCESS) { line = __LINE__; goto err_hndl;}
        }
    }

    return MPI_SUCCESS;

    err_hndl:
        fprintf(stderr,"%s:%4d\tError occurred %d\n",__FILE__,line,err);
        return err;
}


int mca_coll_tuned_barrier_intra_bruck(struct ompi_communicator_t *comm)
{
    int rank, size;
    int distance, to, from;
    int err, line = 0;

    rank = ompi_comm_rank(comm);
    size = ompi_comm_size(comm);
    printf("mca_coll_tuned_bcast_intra_bruck rank %d\n", rank);

    /* exchange data with rank-2^k and rank+2^k */
    for (distance = 1; distance < size; distance <<= 1) { 
        from = (rank + size - distance)%size;
        to   = (rank + distance)%size;
        err = coll_tuned_sendrecv (NULL, 0, MPI_BYTE, to, MCA_COLL_BASE_TAG_BARRIER,
                                    NULL, 0, MPI_BYTE, from, MCA_COLL_BASE_TAG_BARRIER,
                                    comm, MPI_STATUS_IGNORE, rank);
       if (err != MPI_SUCCESS) { line = __LINE__; goto err_hndl;}
    }

    return MPI_SUCCESS;

    err_hndl:
        fprintf(stderr,"%s:%4d\tError occurred %d\n",__FILE__,line,err);
        return err;
}


/* special case for two processes */
int mca_coll_tuned_barrier_intra_two_procs(struct ompi_communicator_t *comm)
{
    int rank;
    int err=0;

    rank = ompi_comm_rank(comm);
    printf("mca_coll_tuned_bcast_intra_two_procs rank %d\n", rank);

    if (0==rank) {
        err = coll_tuned_sendrecv (NULL, 0, MPI_BYTE, 1, MCA_COLL_BASE_TAG_BARRIER, 
                                   NULL, 0, MPI_BYTE, 1, MCA_COLL_BASE_TAG_BARRIER,
                                   comm, MPI_STATUS_IGNORE, rank);
    }
    else {
        err = coll_tuned_sendrecv (NULL, 0, MPI_BYTE, 0, MCA_COLL_BASE_TAG_BARRIER,
                                   NULL, 0, MPI_BYTE, 0, MCA_COLL_BASE_TAG_BARRIER,
                                   comm, MPI_STATUS_IGNORE, rank);
    }

    return (err);
}


int mca_coll_tuned_barrier_intra_linear(struct ompi_communicator_t *comm)
{
return OMPI_ERR_NOT_IMPLEMENTED;
}




