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
#include "mpi/runtime/mpiruntime.h"
#include "mpi/runtime/params.h"
#include "runtime/runtime.h"
#include "runtime/ompi_progress.h"
#include "util/sys_info.h"
#include "util/proc_info.h"
#include "util/session_dir.h"
#include "mpi.h"
#include "communicator/communicator.h"
#include "group/group.h"
#include "info/info.h"
#include "util/show_help.h"
#include "util/stacktrace.h"
#include "errhandler/errcode.h"
#include "errhandler/errclass.h"
#include "request/request.h"
#include "op/op.h"
#include "file/file.h"
#include "attribute/attribute.h"
#include "threads/thread.h"

#include "mca/base/base.h"
#include "mca/allocator/base/base.h"
#include "mca/allocator/allocator.h"
#include "mca/mpool/base/base.h"
#include "mca/mpool/mpool.h"
#include "mca/ptl/ptl.h"
#include "mca/ptl/base/base.h"
#include "mca/pml/pml.h"
#include "mca/pml/base/base.h"
#include "mca/coll/coll.h"
#include "mca/coll/base/base.h"
#include "mca/topo/topo.h"
#include "mca/topo/base/base.h"
#include "mca/io/io.h"
#include "mca/io/base/base.h"
#include "mca/oob/oob.h"
#include "mca/oob/base/base.h"
#include "mca/ns/ns.h"
#include "mca/gpr/gpr.h"
#include "mca/rml/rml.h"
#include "mca/soh/soh.h"

#include "runtime/runtime.h"
#include "event/event.h"

/*
 * Global variables and symbols for the MPI layer
 */

bool ompi_mpi_initialized = false;
bool ompi_mpi_finalized = false;

bool ompi_mpi_thread_multiple = false;
int ompi_mpi_thread_requested = MPI_THREAD_SINGLE;
int ompi_mpi_thread_provided = MPI_THREAD_SINGLE;

ompi_thread_t *ompi_mpi_main_thread = NULL;


int ompi_mpi_init(int argc, char **argv, int requested, int *provided)
{
    int ret, param;
    bool allow_multi_user_threads;
    bool have_hidden_threads;
    ompi_proc_t** procs;
    size_t nprocs;
    char *error = NULL;
    char *jobid_string;
    orte_jobid_t jobid;

    /* Become an OMPI process */

    if (OMPI_SUCCESS != (ret = ompi_init(argc, argv))) {
        error = "ompi_init() failed";
        goto error;
    }

    /* Open up the MCA */

    if (OMPI_SUCCESS != (ret = mca_base_open())) {
        error = "mca_base_open() failed";
        goto error;
    }

    /* Join the run-time environment */
    allow_multi_user_threads = true;
    have_hidden_threads = false;
    if (OMPI_SUCCESS != (ret = orte_init(NULL, argc, argv))) {
	goto error;
    }

    /* start recording the compound command that starts us up */
    orte_gpr.begin_compound_cmd();

    /* Once we've joined the RTE, see if any MCA parameters were
       passed to the MPI level */

    if (OMPI_SUCCESS != (ret = ompi_mpi_register_params())) {
        error = "mca_mpi_register_params() failed";
        goto error;
    }

    if (OMPI_SUCCESS != (ret = ompi_util_register_stackhandlers ())) {
        error = "util_register_stackhandlers() failed";
        goto error;
    }

    /* initialize ompi procs */
    if (OMPI_SUCCESS != (ret = ompi_proc_init())) {
        error = "mca_proc_init() failed";
        goto error;
    }

    /* Open up relevant MCA modules. */

    if (OMPI_SUCCESS != (ret = mca_allocator_base_open())) {
        error = "mca_allocator_base_open() failed";
        goto error;
    }
    if (OMPI_SUCCESS != (ret = mca_mpool_base_open())) {
        error = "mca_mpool_base_open() failed";
        goto error;
    }
    if (OMPI_SUCCESS != (ret = mca_pml_base_open())) {
        error = "mca_pml_base_open() failed";
        goto error;
    }
    if (OMPI_SUCCESS != (ret = mca_ptl_base_open())) {
        error = "mca_ptl_base_open() failed";
        goto error;
    }
    if (OMPI_SUCCESS != (ret = mca_coll_base_open())) {
        error = "mca_coll_base_open() failed";
        goto error;
    }
    if (OMPI_SUCCESS != (ret = mca_topo_base_open())) {
        error = "mca_topo_base_open() failed";
        goto error;
    }
    if (OMPI_SUCCESS != (ret = mca_io_base_open())) {
        error = "mca_io_base_open() failed";
        goto error;
    }

    /* initialize module exchange */
    if (OMPI_SUCCESS != (ret = mca_base_modex_init())) {
        error = "mca_base_modex_init() failed";
        goto error;
    }

    /* Select which pml, ptl, and coll modules to use, and determine the
       final thread level */

    if (OMPI_SUCCESS != 
	(ret = mca_base_init_select_components(requested, 
					       allow_multi_user_threads,
					       have_hidden_threads, 
					       provided))) {
        error = "mca_base_init_select_components() failed";
        goto error;
    }

    /* initialize requests */
    if (OMPI_SUCCESS != (ret = ompi_request_init())) {
        error = "ompi_request_init() failed";
        goto error;
    }

    /* initialize info */
    if (OMPI_SUCCESS != (ret = ompi_info_init())) {
        error = "ompi_info_init() failed";
        goto error;
    }

    /* initialize error handlers */
    if (OMPI_SUCCESS != (ret = ompi_errhandler_init())) {
        error = "ompi_errhandler_init() failed";
        goto error;
    }

    /* initialize error codes */
    if (OMPI_SUCCESS != (ret = ompi_mpi_errcode_init())) {
        error = "ompi_mpi_errcode_init() failed";
        goto error;
    }

    /* initialize error classes */
    if (OMPI_SUCCESS != (ret = ompi_errclass_init())) {
        error = "ompi_errclass_init() failed";
        goto error;
    }
    
    /* initialize internal error codes */
    if (OMPI_SUCCESS != (ret = ompi_errcode_intern_init())) {
        error = "ompi_errcode_intern_init() failed";
        goto error;
    }
     
    /* initialize groups  */
    if (OMPI_SUCCESS != (ret = ompi_group_init())) {
        error = "ompi_group_init() failed";
        goto error;
    }

    /* initialize communicators */
    if (OMPI_SUCCESS != (ret = ompi_comm_init())) {
        error = "ompi_comm_init() failed";
        goto error;
    }

    /* initialize datatypes */
    if (OMPI_SUCCESS != (ret = ompi_ddt_init())) {
        error = "ompi_ddt_init() failed";
        goto error;
    }

    /* initialize ops */
    if (OMPI_SUCCESS != (ret = ompi_op_init())) {
        error = "ompi_op_init() failed";
        goto error;
    }

    /* initialize file handles */
    if (OMPI_SUCCESS != (ret = ompi_file_init())) {
        error = "ompi_file_init() failed";
        goto error;
    }

    /* initialize attribute meta-data structure for comm/win/dtype */
    if (OMPI_SUCCESS != (ret = ompi_attr_init())) {
        error = "ompi_attr_init() failed";
        goto error;
    }
    /* do module exchange */
    if (OMPI_SUCCESS != (ret = mca_base_modex_exchange())) {
        error = "ompi_base_modex_exchange() failed";
        goto error;
    }

    /*
     *  Set my process status to "starting". Note that this must be done
     *  after the rte init is completed.
     *
     *  Ensure we own the job status and the oob segments first
     */
    if (ORTE_SUCCESS != orte_ns.get_jobid(&jobid, orte_process_info.my_name)) {
        error = "orte_ns - failed to get jobid";
        goto error;
    }
    if (ORTE_SUCCESS != orte_ns.get_jobid_string(&jobid_string, orte_process_info.my_name)) {
        error = "orte_ns - failed to get jobid string";
        goto error;
    }
/*    if (ORTE_SUCCESS != orte_ns.get_vpid((orte_vpid_t*)(&my_status.rank), orte_process_info.my_name)) {
        error = "orte_ns - failed to get vpid";
        goto error;
    }
    if (OMPI_SUCCESS != (ret = ompi_rte_set_process_status(&my_status, orte_process_info.my_name))) {
        error = "ompi_mpi_init: failed in ompi_rte_set_process_status()\n";
        goto error;
    } 
*/
    /*
     * Set the virtual machine status for this node
     */
    
    /* execute the compound command - no return data requested
    *  we'll get it all from the startup message
    */
    if (OMPI_SUCCESS != (ret = orte_gpr.exec_compound_cmd())) {
	    error = "ompi_rte_init: orte_gpr.exec_compound_cmd failed";
	    goto error;
    }
    

    /* wait to receive startup message and info distributed */
    if (OMPI_SUCCESS != (ret = orte_rml.xcast(NULL, NULL, 0, NULL, orte_gpr.decode_startup_msg))) {
	    error = "ompi_rte_init: failed to see all procs register\n";
	    goto error;
    }

    if (orte_debug_flag) {
	ompi_output(0, "[%d,%d,%d] process startup message received",
		    ORTE_NAME_ARGS(orte_process_info.my_name));
    }

    /* add all ompi_proc_t's to PML */
    if (NULL == (procs = ompi_proc_world(&nprocs))) {
        error = "ompi_proc_world() failed";
        goto error;
    }
    if (OMPI_SUCCESS != (ret = mca_pml.pml_add_procs(procs, nprocs))) {
        free(procs);
        error = "PML add procs failed";
        goto error;
    }
    free(procs);

    /* start PTL's */
    param = 1;
    if (OMPI_SUCCESS != 
        (ret = mca_pml.pml_control(MCA_PTL_ENABLE, &param, sizeof(param)))) {
        error = "PML control failed";
        goto error;
    }

    /* save the resulting thread levels */

    ompi_mpi_thread_requested = requested;
    ompi_mpi_thread_provided = *provided;
    ompi_mpi_thread_multiple = (ompi_mpi_thread_provided == 
                                MPI_THREAD_MULTIPLE);
#if OMPI_HAVE_THREADS
    ompi_mpi_main_thread = ompi_thread_get_self();
#else
    ompi_mpi_main_thread = NULL;
#endif    

    /* Init coll for the comms */

    if (OMPI_SUCCESS != 
        (ret = mca_coll_base_comm_select(MPI_COMM_SELF, NULL))) {
        error = "mca_coll_base_comm_select(MPI_COMM_SELF) failed";
        goto error;
    }

    if (OMPI_SUCCESS !=
        (ret = mca_coll_base_comm_select(MPI_COMM_WORLD, NULL))) {
        error = "mca_coll_base_comm_select(MPI_COMM_WORLD) failed";
        goto error;
    }

#if OMPI_HAVE_THREADS == 0
    ompi_progress_events(OMPI_EVLOOP_NONBLOCK);
#endif

    /* Wait for everyone to initialize */
    mca_oob_barrier();

    /* new very last step: check whether we have been spawned or not.
       We introduce that at the very end, since we need collectives,
       datatypes, ptls etc. up and running here....
    */
    if (OMPI_SUCCESS != (ret = ompi_comm_dyn_init())) {
        error = "ompi_comm_dyn_init() failed";
        goto error;
    }

 error:
    if (ret != OMPI_SUCCESS) {
        ompi_show_help("help-mpi-runtime",
                       "mpi_init:startup:internal-failure", true,
                       "MPI_INIT", "MPI_INIT", error, ret);
        return ret;
    }

    /* All done */

    ompi_mpi_initialized = true;
    ompi_mpi_finalized = false;

    if (orte_debug_flag) {
	ompi_output(0, "[%d,%d,%d] ompi_mpi_init completed",
		    ORTE_NAME_ARGS(orte_process_info.my_name));
    }

    return MPI_SUCCESS;
}
