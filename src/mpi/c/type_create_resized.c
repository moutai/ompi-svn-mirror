/*
 * $HEADER$
 */

#include "ompi_config.h"

#include "mpi.h"
#include "mpi/c/bindings.h"
#include "runtime/runtime.h"
#include "datatype/datatype.h"
#include "errhandler/errhandler.h"
#include "communicator/communicator.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILING_DEFINES
#pragma weak MPI_Type_create_resized = PMPI_Type_create_resized
#endif

#if OMPI_PROFILING_DEFINES
#include "mpi/c/profile/defines.h"
#endif

static char FUNC_NAME[] = "MPI_Type_create_resized";

int
MPI_Type_create_resized(MPI_Datatype oldtype,
                        MPI_Aint lb,
                        MPI_Aint extent,
                        MPI_Datatype *newtype)
{
   int rc;

   if( MPI_PARAM_CHECK ) {
      if( OMPI_MPI_INVALID_STATE ) {
         OMPI_ERRHANDLER_RETURN( MPI_ERR_INTERN, (ompi_communicator_t*)NULL,
                                MPI_ERR_INTERN, FUNC_NAME );
      }
   }

   rc = ompi_ddt_create_resized( oldtype, lb, extent, newtype );
   if( rc != MPI_SUCCESS ) {
      ompi_ddt_destroy( newtype );
      OMPI_ERRHANDLER_RETURN( rc, (ompi_communicator_t*)NULL, rc, FUNC_NAME );
   }

   {
      MPI_Aint a_a[2];
      a_a[0] = lb;
      a_a[1] = extent;
      ompi_ddt_set_args( *newtype, 0, NULL, 2, a_a, 1, &oldtype, MPI_COMBINER_RESIZED );
   }
   return MPI_SUCCESS;
}


