/*
 * $HEADER$
 */

#include "ompi_config.h"
#include "coll_basic.h"

#include <stdio.h>
#include <errno.h>

#include "constants.h"
#include "mpi.h"
#include "datatype/datatype.h"
#include "mca/coll/coll.h"
#include "mca/coll/base/coll_tags.h"
#include "coll_basic.h"


/*
 *	alltoallw
 *
 *	Function:	- MPI_Alltoallw for non-ompid RPIs
 *	Accepts:	- same as MPI_Alltoallw()
 *	Returns:	- MPI_SUCCESS or an MPI error code
 */
int mca_coll_basic_alltoallw(void *sbuf, int *scounts, int *sdisps,
                             MPI_Datatype *sdtypes, void *rbuf,
                             int *rcounts, int *rdisps,
                             MPI_Datatype *rdtypes, MPI_Comm comm)
{
#if 1
  return OMPI_ERR_NOT_IMPLEMENTED;
#else
  int i;
  int size;
  int rank;
  int nreqs;
  int err;
  char *psnd;
  char *prcv;
  MPI_Request *req;
  MPI_Request *preq;

  /* Initialize. */

  MPI_Comm_size(comm, &size);
  MPI_Comm_rank(comm, &rank);

  /* Allocate arrays of requests. */

  nreqs = 2 * (size - 1);
  if (nreqs > 0) {
    req = malloc(nreqs * sizeof(MPI_Request));
    if (NULL == req) {
      free(req);
      return ENOMEM;
    }
  } else {
    req = NULL;
  }

  /* simple optimization */

  psnd = ((char *) sbuf) + sdisps[rank];
  prcv = ((char *) rbuf) + rdisps[rank];
#if 0
  /* JMS: Need a ompi_datatype_something() here that allows two
     different datatypes */
  err = ompi_dtsndrcv(psnd, scounts[rank], sdtypes[rank],
		     prcv, rcounts[rank], rdtypes[rank], BLKMPIALLTOALLW, comm);
  if (MPI_SUCCESS != err) {
    if (MPI_REQUEST_NULL != req)
      OMPI_FREE(req);
    return err;
  }
#endif

  /* If only one process, we're done. */

  if (1 == size) {
    return MPI_SUCCESS;
  }

  /* Initiate all send/recv to/from others. */

  preq = req;
  for (i = 0; i < size; ++i) {
    if (i == rank)
      continue;

    prcv = ((char *) rbuf) + rdisps[i];
#if 0
    /* JMS: Need to replace this with negative tags and and direct PML
       calls */
    err = MPI_Recv_init(prcv, rcounts[i], rdtypes[i],
			i, BLKMPIALLTOALLW, comm, preq++);
    if (MPI_SUCCESS != err) {
      OMPI_FREE(req);
      return err;
    }
#endif
  }

  for (i = 0; i < size; ++i) {
    if (i == rank)
      continue;

    psnd = ((char *) sbuf) + sdisps[i];
#if 0
    /* JMS: Need to replace this with negative tags and and direct PML
       calls */
    err = MPI_Send_init(psnd, scounts[i], sdtypes[i],
			i, BLKMPIALLTOALLW, comm, preq++);
    if (MPI_SUCCESS != err) {
      OMPI_FREE(req);
      return err;
    }
#endif
  }

  /* Start all requests. */

  err = MPI_Startall(nreqs, req);
  if (MPI_SUCCESS != err) {
    free(req);
    return err;
  }

  /* Wait for them all. */

  err = MPI_Waitall(nreqs, req, MPI_STATUSES_IGNORE);
  if (MPI_SUCCESS != err) {
    free(req);
    return err;
  }

  /* Free the requests. */

  for (i = 0, preq = req; i < nreqs; ++i, ++preq) {
    err = MPI_Request_free(preq);
    if (MPI_SUCCESS != err) {
      free(req);
      return err;
    }
  }

  /* All done */

  free(req);
  return MPI_SUCCESS;
#endif
}
