/* -*- C -*-
 *
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
/** @file:
 *
 * The Open MPI General Purpose Registry - Proxy component
 *
 */

/*
 * includes
 */
#include "orte_config.h"

#include "include/orte_constants.h"
#include "include/orte_types.h"
#include "dps/dps.h"
#include "util/output.h"
#include "util/proc_info.h"

#include "mca/ns/ns.h"
#include "mca/oob/oob_types.h"
#include "mca/rml/rml.h"

#include "gpr_proxy.h"


/*
 * Struct of function pointers that need to be initialized
 */
OMPI_COMP_EXPORT mca_gpr_base_component_t orte_gpr_proxy_component = {
    {
	MCA_GPR_BASE_VERSION_1_0_0,

	"orte_gpr_proxy", /* MCA module name */
	1,  /* MCA module major version */
	0,  /* MCA module minor version */
	0,  /* MCA module release version */
	orte_gpr_proxy_open,  /* module open */
	orte_gpr_proxy_close /* module close */
    },
    {
	false /* checkpoint / restart */
    },
    orte_gpr_proxy_init,    /* module init */
    orte_gpr_proxy_finalize /* module shutdown */
};

/*
 * setup the function pointers for the module
 */
static orte_gpr_base_module_t orte_gpr_proxy = {
   /* BLOCKING OPERATIONS */
    orte_gpr_proxy_get,
    orte_gpr_proxy_put,
    orte_gpr_proxy_delete_entries,
    orte_gpr_proxy_delete_segment,
    orte_gpr_proxy_index,
    /* NON-BLOCKING OPERATIONS */
    orte_gpr_proxy_get_nb,
    orte_gpr_proxy_put_nb,
    orte_gpr_proxy_delete_entries_nb,
    orte_gpr_proxy_delete_segment_nb,
    orte_gpr_proxy_index_nb,
    /* JOB-RELATED OPERATIONS */
    orte_gpr_proxy_preallocate_segment,
    orte_gpr_proxy_get_startup_msg,
    orte_gpr_proxy_decode_startup_msg,
    /* SUBSCRIBE OPERATIONS */
    orte_gpr_proxy_subscribe,
    orte_gpr_proxy_unsubscribe,
    /* SYNCHRO OPERATIONS */
    orte_gpr_proxy_synchro,
    orte_gpr_proxy_cancel_synchro,
    /* COMPOUND COMMANDS */
    orte_gpr_proxy_begin_compound_cmd,
    orte_gpr_proxy_stop_compound_cmd,
    orte_gpr_proxy_exec_compound_cmd,
    /* DUMP */
    orte_gpr_proxy_dump,
    /* MODE OPERATIONS */
    orte_gpr_proxy_notify_on,
    orte_gpr_proxy_notify_off,
    orte_gpr_proxy_triggers_active,
    orte_gpr_proxy_triggers_inactive,
    /* CLEANUP OPERATIONS */
    orte_gpr_proxy_cleanup_job,
    orte_gpr_proxy_cleanup_proc,
    /* TEST INTERFACE */
    orte_gpr_proxy_test_internals
};


/*
 * Whether or not we allowed this component to be selected
 */
static bool initialized = false;

/*
 * globals needed within proxy component
 */
orte_process_name_t *orte_gpr_my_replica;
ompi_list_t orte_gpr_proxy_notify_request_tracker;
orte_gpr_notify_id_t orte_gpr_proxy_next_notify_id_tag;
int orte_gpr_proxy_debug;
ompi_mutex_t orte_gpr_proxy_mutex;
bool orte_gpr_proxy_compound_cmd_mode;
orte_buffer_t *orte_gpr_proxy_compound_cmd;
ompi_mutex_t orte_gpr_proxy_wait_for_compound_mutex;
ompi_condition_t orte_gpr_proxy_compound_cmd_condition;
int orte_gpr_proxy_compound_cmd_waiting;
bool orte_gpr_proxy_silent_mode;


/* constructor - used to initialize notify message instance */
static void orte_gpr_proxy_notify_request_tracker_construct(orte_gpr_proxy_notify_request_tracker_t* req)
{
    req->callback = NULL;
    req->user_tag = NULL;
    req->local_idtag = ORTE_GPR_NOTIFY_ID_MAX;
    req->remote_idtag = ORTE_GPR_NOTIFY_ID_MAX;
    req->segment = NULL;
    req->action = 0;
}

/* destructor - used to free any resources held by instance */
static void orte_gpr_proxy_notify_request_tracker_destructor(orte_gpr_proxy_notify_request_tracker_t* req)
{
    if (NULL != req->segment) {
	free(req->segment);
    }
}

/* define instance of ompi_class_t */
OBJ_CLASS_INSTANCE(
		   orte_gpr_proxy_notify_request_tracker_t,            /* type name */
		   ompi_list_item_t,                          /* parent "class" name */
		   orte_gpr_proxy_notify_request_tracker_construct,    /* constructor */
		   orte_gpr_proxy_notify_request_tracker_destructor);  /* destructor */


/*
 * Open the component
 */
int orte_gpr_proxy_open(void)
{
    int id;

    id = mca_base_param_register_int("gpr", "proxy", "debug", NULL, 0);
    mca_base_param_lookup_int(id, &orte_gpr_proxy_debug);

    return ORTE_SUCCESS;
}

/*
 * Close the component
 */
int orte_gpr_proxy_close(void)
{
    return ORTE_SUCCESS;
}

orte_gpr_base_module_t* orte_gpr_proxy_init(bool *allow_multi_user_threads, bool *have_hidden_threads, int *priority)
{
    int rc;

    if (orte_gpr_proxy_debug) {
	ompi_output(0, "gpr_proxy_init called");
    }

    /* If we are NOT to host a replica, then we want to be selected, so do all
       the setup and return the module */
    if (NULL != orte_process_info.gpr_replica) {

	if (orte_gpr_proxy_debug) {
	    ompi_output(0, "gpr_proxy_init: proxy selected");
	}

	/* Return a module (choose an arbitrary, positive priority --
	   it's only relevant compared to other ns components).  If
	   we're not the seed, then we don't want to be selected, so
	   return NULL. */

	*priority = 10;

	/* We allow multi user threads but don't have any hidden threads */

	*allow_multi_user_threads = true;
	*have_hidden_threads = false;

	/* setup thread locks and condition variable */
	OBJ_CONSTRUCT(&orte_gpr_proxy_mutex, ompi_mutex_t);
	OBJ_CONSTRUCT(&orte_gpr_proxy_wait_for_compound_mutex, ompi_mutex_t);
	OBJ_CONSTRUCT(&orte_gpr_proxy_compound_cmd_condition, ompi_condition_t);

	/* initialize the registry compound mode */
	orte_gpr_proxy_compound_cmd_mode = false;
	orte_gpr_proxy_compound_cmd_waiting = 0;
	orte_gpr_proxy_compound_cmd = NULL;

	/* define the replica for us to use - get it from process_info */
    if (ORTE_SUCCESS != orte_ns.copy_process_name(&orte_gpr_my_replica, orte_process_info.gpr_replica)) {
        return NULL;
    }
    
	if (NULL == orte_gpr_my_replica) { /* can't function */
	    return NULL;
	}

	/* initialize the notify request tracker */
	OBJ_CONSTRUCT(&orte_gpr_proxy_notify_request_tracker, ompi_list_t);
	orte_gpr_proxy_next_notify_id_tag = 0;

	/* initialize any local variables */
	orte_gpr_proxy_silent_mode = false;

	/* issue the non-blocking receive */
	rc = orte_rml.recv_buffer_nb(ORTE_RML_NAME_ANY, MCA_OOB_TAG_GPR_NOTIFY, 0, orte_gpr_proxy_notify_recv, NULL);
	if(rc != ORTE_SUCCESS && rc != ORTE_ERR_NOT_IMPLEMENTED) {
	    return NULL;
	}

	/* Return the module */

	initialized = true;
	return &orte_gpr_proxy;
    } else {
	return NULL;
    }
}

/*
 * finalize routine
 */
int orte_gpr_proxy_finalize(void)
{

    if (orte_gpr_proxy_debug) {
	ompi_output(0, "finalizing gpr proxy");
    }

    if (initialized) {
	initialized = false;
    }

    /* All done */
    orte_rml.recv_cancel(ORTE_RML_NAME_ANY, MCA_OOB_TAG_GPR_NOTIFY);
    return ORTE_SUCCESS;
}

/* 
 * handle notify messages from replicas
 */

void orte_gpr_proxy_notify_recv(int status, orte_process_name_t* sender,
			       orte_buffer_t *buffer, orte_rml_tag_t tag,
			       void* cbdata)
{
    orte_gpr_cmd_flag_t command;
    orte_gpr_notify_id_t id_tag;
    orte_gpr_notify_message_t *message;
    bool found;
    orte_gpr_proxy_notify_request_tracker_t *trackptr;
    size_t n;
    int rc;
    uint32_t cnt;
    orte_data_type_t type;

    if (orte_gpr_proxy_debug) {
	ompi_output(0, "[%d,%d,%d] gpr proxy: received trigger message",
				ORTE_NAME_ARGS(*(orte_process_info.my_name)));
    }

    message = OBJ_NEW(orte_gpr_notify_message_t);

    n = 1;
    if (ORTE_SUCCESS != (rc = orte_dps.unpack(buffer, &command, &n, ORTE_GPR_PACK_CMD))) {
        if (orte_gpr_proxy_debug) {
            ompi_output(0, "[%d,%d,%d] gpr_proxy_notify_recv: failure %d",
               ORTE_NAME_ARGS(*(orte_process_info.my_name)), rc);
        }
        goto RETURN_ERROR;
    }

	if (ORTE_GPR_NOTIFY_CMD != command) {
        if (orte_gpr_proxy_debug) {
            ompi_output(0, "[%d,%d,%d] gpr_proxy_notify_recv: communication failure",
               ORTE_NAME_ARGS(*(orte_process_info.my_name)));
        }
	   goto RETURN_ERROR;
    }

    n = 1;
    if (0 > (rc = orte_dps.unpack(buffer, message->segment, &n, ORTE_STRING))) {
        if (orte_gpr_proxy_debug) {
            ompi_output(0, "[%d,%d,%d] gpr_proxy_notify_recv: failure %d",
               ORTE_NAME_ARGS(*(orte_process_info.my_name)), rc);
        }
	    goto RETURN_ERROR;
    }

    n = 1;
    if (ORTE_SUCCESS != (rc = orte_dps.unpack(buffer, &id_tag, &n, ORTE_GPR_PACK_NOTIFY_ID))) {
        if (orte_gpr_proxy_debug) {
            ompi_output(0, "[%d,%d,%d] gpr_proxy_notify_recv: failure %d",
               ORTE_NAME_ARGS(*(orte_process_info.my_name)), rc);
        }
	   goto RETURN_ERROR;
    }
	
    n = 1;
    if (ORTE_SUCCESS != (rc = orte_dps.unpack(buffer, &(message->cnt), &n, ORTE_UINT32))) {
        if (orte_gpr_proxy_debug) {
            ompi_output(0, "[%d,%d,%d] gpr_proxy_notify_recv: failure %d",
               ORTE_NAME_ARGS(*(orte_process_info.my_name)), rc);
        }
    goto RETURN_ERROR;
    }

    message->values = (orte_gpr_value_t*)malloc(cnt*sizeof(orte_gpr_value_t*));
    if (NULL == message->values) {
        if (orte_gpr_proxy_debug) {
            ompi_output(0, "[%d,%d,%d] gpr_proxy_notify_recv: malloc failure",
               ORTE_NAME_ARGS(*(orte_process_info.my_name)));
        }
        goto RETURN_ERROR;
    }
    
    if (ORTE_SUCCESS != (rc = orte_dps.unpack(buffer, message->values, &(message->cnt), ORTE_KEYVAL))) {
        if (orte_gpr_proxy_debug) {
            ompi_output(0, "[%d,%d,%d] gpr_proxy_notify_recv: failure %d",
               ORTE_NAME_ARGS((*orte_process_info.my_name)), rc);
        }
    goto RETURN_ERROR;
    }

    n = 1;
    if (ORTE_SUCCESS != (rc = orte_dps.unpack(buffer, &(message->cmd), &n, ORTE_GPR_PACK_CMD))) {
        if (orte_gpr_proxy_debug) {
            ompi_output(0, "[%d,%d,%d] gpr_proxy_notify_recv: failure %d",
               ORTE_NAME_ARGS(*(orte_process_info.my_name)), rc);
        }
    goto RETURN_ERROR;
    }

    if (ORTE_SUCCESS != (rc = orte_dps.peek(buffer, &type, &n))) {
        if (orte_gpr_proxy_debug) {
            ompi_output(0, "[%d,%d,%d] gpr_proxy_notify_recv: failure %d",
               ORTE_NAME_ARGS(*(orte_process_info.my_name)), rc);
        }
           goto RETURN_ERROR;
    }

    n = 1;
    if (ORTE_NOTIFY_ACTION == type) {
        if (ORTE_SUCCESS != (rc = orte_dps.unpack(buffer, &(message->flag.trig_action), &n, ORTE_NOTIFY_ACTION))) {
            if (orte_gpr_proxy_debug) {
                ompi_output(0, "[%d,%d,%d] gpr_proxy_notify_recv: failure %d",
                   ORTE_NAME_ARGS(*(orte_process_info.my_name)), rc);
            }
    	        goto RETURN_ERROR;
        }
    } else if (ORTE_SYNCHRO_MODE == message->cmd) {
        if (ORTE_SUCCESS != (rc = orte_dps.unpack(buffer, &(message->flag.trig_synchro), &n, ORTE_SYNCHRO_MODE))) {
            if (orte_gpr_proxy_debug) {
                ompi_output(0, "[%d,%d,%d] gpr_proxy_notify_recv: failure %d",
                   ORTE_NAME_ARGS(*(orte_process_info.my_name)), rc);
            }
    	        goto RETURN_ERROR;
        }
    } else if (ORTE_EXIT_CODE == type) {
        if (ORTE_SUCCESS != (rc = orte_dps.unpack(buffer, &(message->flag.exit_code), &n, ORTE_EXIT_CODE))) {
            if (orte_gpr_proxy_debug) {
                ompi_output(0, "[%d,%d,%d] gpr_proxy_notify_recv: failure %d",
                   ORTE_NAME_ARGS(*(orte_process_info.my_name)), rc);
            }
            goto RETURN_ERROR;
        }
    } else if (ORTE_GPR_CMD == type) {
        if (ORTE_SUCCESS != (rc = orte_dps.unpack(buffer, &(message->flag.cmd_return), &n, ORTE_INT32))) {
            if (orte_gpr_proxy_debug) {
                ompi_output(0, "[%d,%d,%d] gpr_proxy_notify_recv: failure %d",
                   ORTE_NAME_ARGS(*(orte_process_info.my_name)), rc);
            }
            goto RETURN_ERROR;
        }
    } else {
        if (orte_gpr_proxy_debug) {
            ompi_output(0, "[%d,%d,%d] gpr_proxy_notify_recv: comm failure",
               ORTE_NAME_ARGS(*(orte_process_info.my_name)));
        }
            goto RETURN_ERROR;
    }
    
    OMPI_THREAD_LOCK(&orte_gpr_proxy_mutex);

    /* find the request corresponding to this notify */
    found = false;
    for (trackptr = (orte_gpr_proxy_notify_request_tracker_t*)ompi_list_get_first(&orte_gpr_proxy_notify_request_tracker);
         trackptr != (orte_gpr_proxy_notify_request_tracker_t*)ompi_list_get_end(&orte_gpr_proxy_notify_request_tracker);
         trackptr = (orte_gpr_proxy_notify_request_tracker_t*)ompi_list_get_next(trackptr)) {
     	if (orte_gpr_proxy_debug) {
     		ompi_output(0, "\tchecking idtag %d for segment %s\n", trackptr->local_idtag, trackptr->segment);
     	}
		if (trackptr->local_idtag == id_tag) {
            found = true;
            break;
        }
    }

    OMPI_THREAD_UNLOCK(&orte_gpr_proxy_mutex);

    if (!found) {  /* didn't find request */
        	ompi_output(0, "[%d,%d,%d] Proxy notification error - received notify request not found",
                        ORTE_NAME_ARGS(*(orte_process_info.my_name)));
        	goto RETURN_ERROR;
    }

    /* process request */
    trackptr->callback(message, trackptr->user_tag);

    /* dismantle message and free memory */

 RETURN_ERROR:
    OBJ_RELEASE(message);

    /* reissue non-blocking receive */
    orte_rml.recv_buffer_nb(ORTE_RML_NAME_ANY, MCA_OOB_TAG_GPR_NOTIFY, 0, orte_gpr_proxy_notify_recv, NULL);

}

