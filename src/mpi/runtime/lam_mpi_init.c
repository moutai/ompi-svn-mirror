/*
 * $HEADER$
 */

#include "lam_config.h"

#include "lam/constants.h"
#include "lam/runtime/runtime.h"
#include "mpi.h"
#include "mpi/runtime/runtime.h"
#include "mpi/communicator/communicator.h"
#include "mpi/group/group.h"
#include "mca/lam/base/base.h"
#include "mca/mpi/base/base.h"
#include "mca/mpi/ptl/ptl.h"
#include "mca/mpi/ptl/base/base.h"
#include "mca/mpi/pml/pml.h"
#include "mca/mpi/pml/base/base.h"
#include "mca/mpi/coll/coll.h"
#include "mca/mpi/coll/base/base.h"


/*
 * Global variables and symbols for the MPI layer
 */

bool lam_mpi_initialized = false;
bool lam_mpi_finalized = false;
/* As a deviation from the norm, this variable is extern'ed in
   src/mpi/interface/c/bindings.h because it is already included in
   all MPI function imlementation files */
bool lam_mpi_param_check = true;

bool lam_mpi_thread_multiple = false;
int lam_mpi_thread_requested = MPI_THREAD_SINGLE;
int lam_mpi_thread_provided = MPI_THREAD_SINGLE;


int lam_mpi_init(int argc, char **argv, int requested, int *provided)
{
    int ret, param, value;
    bool allow_multi_user_threads;
    bool have_hidden_threads;

    /* Become a LAM process */

    if (LAM_SUCCESS != (ret = lam_init(argc, argv))) {
        return ret;
    }

    /* Open up the MCA */

    if (LAM_SUCCESS != (ret = mca_base_open())) {
        return ret;
    }
    if (LAM_SUCCESS != (ret = mca_mpi_open())) {
        return ret;
    }

    /* Join the run-time environment */

    if (LAM_SUCCESS != (ret = lam_rte_init(&allow_multi_user_threads, &have_hidden_threads))) {
        return ret;
    }

    /* Open up relevant MCA modules.    Do not open io, topo, or one
         module types here -- they are loaded upon demand (i.e., upon
         relevant constructors). */

    if (LAM_SUCCESS != (ret = mca_pml_base_open())) {
        /* JMS show_help */
        return ret;
    }
    if (LAM_SUCCESS != (ret = mca_ptl_base_open())) {
        /* JMS show_help */
        return ret;
    }
    if (LAM_SUCCESS != (ret = mca_coll_base_open())) {
        /* JMS show_help */
        return ret;
    }

    /* Select which pml, ptl, and coll modules to use, and determine the
         final thread level */

    if (LAM_SUCCESS != 
            (ret = mca_mpi_init_select_modules(requested, allow_multi_user_threads,
             have_hidden_threads, provided))) {
        /* JMS show_help */
        return ret;
    }

     /* initialize lam procs */
     if (LAM_SUCCESS != (ret = lam_proc_init())) {
         return ret;
     }

     /* initialize groups  */
     if (LAM_SUCCESS != (ret = lam_group_init())) {
         return ret;
     }

     /* initialize communicators */
     if (LAM_SUCCESS != (ret = lam_comm_init())) {
         return ret;
     }

     /* If we have run-time MPI parameter checking possible, register
        an MCA paramter to find out if the user wants it on or off by
        default */

     mca_base_param_lookup_int(param, &value);
     lam_mpi_param_check = (bool) value;

     /* do module exchange */

     /* add all lam_proc_t's to PML */

     /* save the resulting thread levels */

    lam_mpi_thread_requested = requested;
    *provided = lam_mpi_thread_provided;
    lam_mpi_thread_multiple = (lam_mpi_thread_provided == MPI_THREAD_MULTIPLE);

    /* All done */

    lam_mpi_initialized = true;
    return MPI_SUCCESS;
}
