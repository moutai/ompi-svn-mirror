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
/** @file 
 * @page gpr_api
 */

/** 
 *  \brief General Purpose Registry (GPR) API
 *
 * The Open MPI General Purpose Registry (GPR) 
 */

#ifndef ORTE_GPR_H_
#define ORTE_GPR_H_

/*
 * includes
 */

#include "orte_config.h"

#include <sys/types.h>

#include "include/orte_types.h"
#include "include/orte_constants.h"
#include "class/ompi_list.h"

#include "mca/mca.h"
#include "mca/ns/ns_types.h"
#include "mca/rml/rml_types.h"

#include "dps/dps_types.h"
#include "mca/gpr/gpr_types.h"
#include "mca/rml/rml_types.h"


#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif

/*
 * Component functions that MUST be provided
 */

/*
 * Perform any one-time initialization required by the module
 * after RML/NS are available.
 */
typedef int (*orte_gpr_base_module_init_fn_t)(void);

/*
 * Begin recording a compound command.
 * Normally, the registry executes each command as it is called. This, however, can result
 * in an undesirable amount of network traffic. To reduce the traffic, this command allows
 * the user to aggregate a set of registry commands - in any combination of put, get, index,
 * or any other command - to be executed via a single communication to the registry.
 *
 * While recording, all registry commands are stored in a buffer instead of being immediately
 * executed. Thus, commands that retrieve information (e.g., "get") will return a NULL
 * during recording. Values from these commands will be returned when the compound
 * command is actually executed.
 *
 * The process of recording a compound command is thread safe. Threads attempting to
 * record commands are held on a lock until given access in their turn.
 *
 * @param None
 * @retval ORTE_SUCCESS Compound command recorder is active.
 * @retval ORTE_ERROR Compound command recorder did not activate.
 *
 * @code
 * ompi_gpr.begin_compound_cmd();
 * @endcode
 *
 */
typedef int (*orte_gpr_base_module_begin_compound_cmd_fn_t)(void);

/*
 * Stop recording a compound command
 * Terminates the recording process and clears the buffer of any previous commands
 *
 * @param None
 * @retval ORTE_SUCCESS Recording stopped and buffer successfully cleared
 * @retval ORTE_ERROR Didn't work - no idea why it wouldn't
 *
 * @code
 * orte_gpr.stop_compound_cmd();
 * @endcode
 *
 */
typedef int (*orte_gpr_base_module_stop_compound_cmd_fn_t)(void);

/*
 * Execute the compound command (BLOCKING)
 * Execute the compound command that has been recorded. The function returns a status
 * code that indicates whether or not all the included commands were successfully
 * executed. Failure of any command contained in the compound command will terminate
 * execution of the compound command list and return an error to the caller.
 *
 * @retval ORTE_SUCCESS All commands in the list were successfully executed.
 * @retval ORTE_ERROR(s) A command in the list failed, returning the indicated
 * error code.
 *
 * @code
 * status_code = orte_gpr.exec_compound_cmd();
 * @endcode
 *
 */
typedef int (*orte_gpr_base_module_exec_compound_cmd_fn_t)(void);

/* Turn off subscriptions for this process
 * Temporarily turn off subscriptions for this process on the registry. Until restored,
 * the specified subscription will be ignored - no message will be sent. Providing a
 * value of ORTE_REGISTRY_NOTIFY_ID_MAX for the subscription number will turn off ALL
 * subscriptions with this process as the subscriber.
 *
 * Note: synchro messages will continue to be sent - only messages from subscriptions
 * are affected.
 *
 * @param sub_number Notify id number of the subscription to be turned "off". A value
 * of ORTE_REGISTRY_NOTIFY_ID_MAX indicates that ALL subscriptions with this process as the subscriber are to be
 * turned "off" until further notice.
 *
 * @retval ORTE_SUCCESS Operation was successfully completed.
 * @retval ORTE_ERROR(s) Operation failed, returning the provided error code.
 * 
 * @code
 * status_code = orte_gpr.notify_off(subscription_number);
 * @endcode
 */
typedef int (*orte_gpr_base_module_notify_off_fn_t)(orte_gpr_notify_id_t sub_number);

/* Turn on subscriptions for this process
 * Turn on subscriptions for this process on the registry. This is the default condition
 * for subscriptions, indicating that messages generated by triggered subscriptions are to
 * be sent to the subscribing process.
 *
 * @param sub_number Notify id number of the subscription to be turned "on". A value
 * of ORTE_REGISTRY_NOTIFY_ID_MAX indicates that ALL subscriptions with this process as the subscriber are to be
 * turned "on" until further notice.
 *
 * @retval ORTE_SUCCESS Operation was successfully completed.
 * @retval ORTE_ERROR(s) Operation failed, returning the provided error code.
 * 
 * @code
 * status_code = orte_gpr.notify_on(subscription_number);
 * @endcode
 */
typedef int (*orte_gpr_base_module_notify_on_fn_t)(orte_gpr_notify_id_t sub_number);

/* Cleanup a job from the registry
 * Remove all references to a given job from the registry. This includes removing
 * all segments "owned" by the job, and removing all process names from dictionaries
 * in the registry.
 *
 * @param jobid The jobid to be cleaned up.
 *
 * @retval ORTE_SUCCESS Operation was successfully completed.
 * @retval ORTE_ERROR(s) Operation failed, returning the provided error code.
 * 
 * @code
 * status_code = orte_gpr.cleanup_job(jobid);
 * @endcode
 *
 */
typedef int (*orte_gpr_base_module_cleanup_job_fn_t)(orte_jobid_t jobid);

/* Cleanup a process from the registry
 * Remove all references to a given process from the registry. This includes removing
 * the process name from all dictionaries in the registry, all subscriptions, etc.
 * It also includes reducing any synchros on the job segment.
 *
 * @param proc A pointer to the process name to be cleaned up.
 *
 * @retval ORTE_SUCCESS Operation was successfully completed.
 * @retval ORTE_ERROR(s) Operation failed, returning the provided error code.
 * 
 * @code
 * status_code = orte_gpr.cleanup_process(&proc);
 * @endcode
 *
 */
typedef int (*orte_gpr_base_module_cleanup_proc_fn_t)(orte_process_name_t *proc);

/*
 * Define and initialize a job segment
 * The registry contains a segment for each job that stores data on each
 * process within that job. Although the registry can create this segment
 * "on-the-fly", it is more efficient to initialize the segment via a separate
 * command - thus allowing the registry to allocate the base storage for all
 * the processes in a single malloc.
 * 
 * @param name A character string indicating the name of the segment.
 * @param num_slots The number of containers expected in this segment. This
 * is just the starting number requested by the user - the registry will
 * dynamically expand the segment as required.
 * 
 * @retval ORTE_SUCCESS The operation was successfully executed.
 * @retval ORTE_ERROR(s) An appropriate error code is returned.
 * 
 * @code
 * status_code = orte_gpr.preallocate_segment("MY_SEGMENT", num_slots);
 * @endcode
 */
typedef int (*orte_gpr_base_module_preallocate_segment_fn_t)(char *name, int num_slots);

/*
 * Delete a segment from the registry (BLOCKING)
 * This command removes an entire segment from the registry, including all data objects,
 * associated subscriptions, and synchros. This is a non-reversible process, so it should
 * be used with care.
 *
 * @param segment Character string specifying the name of the segment to be removed.
 *
 * @retval ORTE_SUCCESS Segment successfully removed.
 * @retval ORTE_ERROR(s) Segment could not be removed for some reason - most
 * likely, the segment name provided was not found in the registry.
 *
 * @code
 * status_code = orte_gpr.delete_segment(segment);
 * @endcode
 */
typedef int (*orte_gpr_base_module_delete_segment_fn_t)(char *segment);

/*
 * Delete a segment from the registry (NON-BLOCKING)
 * A non-blocking version of delete segment.
 */
typedef int (*orte_gpr_base_module_delete_segment_nb_fn_t)(char *segment,
                                orte_gpr_notify_cb_fn_t cbfunc, void *user_tag);


/*
 * Put a data object on the registry (BLOCKING)
 * Place a data item on the registry using a blocking operation - i.e., the calling
 * program will be blocked until the operation completes.
 * 
 * @param addr_mode The addressing mode to be used. Addresses are defined by the tokens provided
 * that describe the object being stored. The caller has the option of specifying how
 * those tokens are to be combined in describing the object. Passing a value of
 * "ORTE_REGISTRY_AND", for example, indicates that all provided tokens are to be used.
 * In contrast, a value of "ORTE_REGISTRY_OR" indicates that any of the provided tokens
 * can adequately describe the object. For the "put" command, only "ORTE_REGISTRY_XAND"
 * is accepted - in other words, the tokens must exactly match those of any existing
 * object in order for the object to be updated. In addition, the "ORTE_REGISTRY_OVERWRITE"
 * flag must be or'd into the mode to enable update of the data object. If a data object
 * is found with the identical token description, but ORTE_REGISTRY_OVERWRITE is NOT specified,
 * then an error will be generated - the data object will NOT be overwritten in this
 * situation.
 *
 * Upon completing the "put", all subscription and synchro requests registered on the
 * specified segment are checked and appropriately processed.
 *
 * @param *segment A character string specifying the name of the segment upon which
 * the data is to be placed.
 *
 * @param **tokens A **char list of tokens describing the data.
 *
 * @param cnt The number of key-value pair structures to be stored.
 * 
 * @param **keyval A pointer to the start of a contiguous array of one or more
 * pointers to key_value pair
 * objects to be stored. The registry will copy this data onto the specified segment - the
 * calling program is responsible for freeing any memory, if appropriate.
 *
 * @retval ORTE_SUCCESS The data has been stored on the specified segment, or the
 * corresponding existing data has been updated.
 *
 * @retval ORTE_ERROR(s) The data was not stored on the specified segment, or the
 * corresponding existing data was not found, or the data was found but the overwrite
 * flag was not set.
 *
 * @code
 * orte_gpr_keyval_t keyval;
 * 
 * status_code = orte_gpr.put(mode, segment, tokens, 1, &keyval);
 * @endcode
 */
typedef int (*orte_gpr_base_module_put_fn_t)(orte_gpr_addr_mode_t addr_mode,
                            int cnt, orte_gpr_value_t **values);

/*
 * Put data on the registry (NON-BLOCKING)
 * A non-blocking version of put.
 */
typedef int (*orte_gpr_base_module_put_nb_fn_t)(orte_gpr_addr_mode_t addr_mode,
                      int cnt, orte_gpr_value_t **values,
                      orte_gpr_notify_cb_fn_t cbfunc, void *user_tag);


/*
 * Get data from the registry (BLOCKING)
 * Returns data from the registry. Given an addressing mode, segment name, and a set
 * of tokens describing the data to be retrieved, the "get" function will search the specified
 * registry segment and return all data items that "match" the description. Addressing
 * modes specify how the provided tokens are to be combined to determine the match -
 * a value of "ORTE_REGISTRY_AND", for example, indictates that all the tokens must be
 * included in the object's description, but allows for other tokens to also be present.
 * A value of "ORTE_REGISTRY_XAND", in contrast, requires that all the tokens be present,
 * and that ONLY those tokens be present.
 *
 * The data is returned as a list of orte_gpr_value_t objects. The caller is
 * responsible for freeing this data storage. Only copies of the registry data are
 * returned - thus, any actions taken by the caller will NOT impact data stored on the
 * registry.
 *
 * @param addr_mode (IN) The addressing mode to be used in the search.
 * @param *segment (IN) A character string indicating the name of the segment to be searched.
 * @param **tokens (IN) A NULL-terminated **char list of tokens describing the objects to be
 * returned. A value of NULL indicates that ALL data on the segment is to be returned.
 * @param **keys (IN) A NULL-terminated **char array of keys describing the specific
 * key-value data to be returned. A value of NULL indicates that ALL key-value pairs
 * described by the segment/token combination are to be returned.
 * 
 * @param *cnt (OUT) A pointer to the number of objects returned by the request.
 * @param **values (OUT) A pointer to an array of orte_gpr_value_t object pointers
 * containing the data
 * returned by the specified search, including the segment and container id info
 * for each keyval pair.
 *
 * @retval ORTE_SUCCESS Operation was successfully completed.
 * @retval ORTE_ERROR(s) Operation failed, returning the provided error code.
 * 
 * @code
 * ompi_list_t *keyval_list;
 * 
 * status_code = orte_gpr.get(addr_mode, segment, tokens, keyval_list);
 * @endcode
 */
typedef int (*orte_gpr_base_module_get_fn_t)(orte_gpr_addr_mode_t addr_mode,
                                char *segment, char **tokens, char **keys,
                                int *cnt, orte_gpr_value_t ***values);

/*
 * Get data from the registry (NON-BLOCKING)
 * A non-blocking version of get. Data is returned to the callback function in the
 * notify message format.
 */
typedef int (*orte_gpr_base_module_get_nb_fn_t)(orte_gpr_addr_mode_t addr_mode,
                                char *segment, char **tokens, char **keys,
                                orte_gpr_notify_cb_fn_t cbfunc, void *user_tag);


/*
 * Delete an object from the registry (BLOCKING)
 * Remove an object from the registry. Given an addressing mode, segment name, and a set
 * of tokens describing the data object, the function will search the specified
 * registry segment and delete all data items that "match" the description. Addressing
 * modes specify how the provided tokens are to be combined to determine the match -
 * a value of "ORTE_REGISTRY_AND", for example, indictates that all the tokens must be
 * included in the object's description, but allows for other tokens to also be present.
 * A value of "ORTE_REGISTRY_XAND", in contrast, requires that all the tokens be present,
 * and that ONLY those tokens be present.
 *
 * Note: A value of NULL for the tokens will delete ALL data items from the specified
 * segment. 
 *
 * @param addr_mode The addressing mode to be used in the search.
 * @param *segment A character string indicating the name of the segment to be searched.
 * @param **tokens A NULL-terminated **char list of tokens describing the objects to be
 * returned. A value of NULL indicates that ALL data on the segment is to be removed.
 *
 * @retval ORTE_SUCCESS Operation was successfully completed.
 * @retval ORTE_ERROR(s) Operation failed, returning the provided error code.
 * 
 * @code
 * status_code = orte_gpr.delete_object(mode, segment, tokens);
 * @endcode
 */
typedef int (*orte_gpr_base_module_delete_entries_fn_t)(orte_gpr_addr_mode_t addr_mode,
						      char *segment, char **tokens, char **keys);

/*
 * Delete an object from the registry (NON-BLOCKING)
 * A non-blocking version of delete object. Result of the command is returned
 * to the callback function in the notify msg format.
 */
typedef int (*orte_gpr_base_module_delete_entries_nb_fn_t)(
                            orte_gpr_addr_mode_t addr_mode,
                            char *segment, char **tokens, char **keys,
                            orte_gpr_notify_cb_fn_t cbfunc, void *user_tag);
/*
 * Obtain an index of a specified dictionary (BLOCKING)
 * The registry contains a dictionary at the global level (containing names of all the
 * segments) and a dictionary for each segment (containing the names of all tokens used
 * in that segment). This command allows the caller to obtain a list of all entries
 * in the specified dictionary.
 *
 * @param *segment (IN) A character string indicating the segment whose dictionary is to be
 * indexed. A value of NULL indicates that the global level dictionary is to be used.
 *
 * @param *cnt (OUT) A pointer to the number of tokens in the index.
 * @param **index (OUT) A char** array of strings containing an index of the
 * specified dictionary.
 *
 * @retval ORTE_SUCCESS Operation was successfully completed.
 * @retval ORTE_ERROR(s) Operation failed, returning the provided error code.
 * 
 * @code
 * int32_t cnt;
 * char *index;
 * char *segment;
 * 
 * status_code = orte_gpr.index(segment, &cnt, &index);
 * @endcode
 */
typedef int (*orte_gpr_base_module_index_fn_t)(char *segment, size_t *cnt, char **index);

/*
 * Obtain an index of a specified dictionary (NON-BLOCKING)
 * A non-blocking version of index. Result of the command is returned to the
 * callback function in the notify msg format.
 */
typedef int (*orte_gpr_base_module_index_nb_fn_t)(char *segment,
                        orte_gpr_notify_cb_fn_t cbfunc, void *user_tag);
                        
/*
 * Subscribe to be notified upon a specified action
 * The registry includes a publish/subscribe mechanism by which callers can be notified
 * upon certain actions occuring to data objects stored on the registry. This function
 * allows the caller to register for such notifications. The registry allows a subscription
 * to be placed upon any segment, and upon the entire registry if desired.
 *
 * @param addr_mode (IN) The addressing mode to be used in specifying the objects to be
 * monitored by this subscription.
 * 
 * @param actions (IN) The actions which are to trigger the notification message. These can
 * be OR'd together from the defined registry action flags.
 * 
 * @param *value (IN) A pointer to an orte_gpr_value_t object that describes the segment,
 * tokens, and keys/values to which a subscription is requested.
 * 
 * @param *trig_value (IN) For subscriptions that begin when reaching a specified level,
 * this registry value (which will be stored on the indicated segment/container)
 * provides the trigger level at which to begin notification. A notification message
 * will be sent upon attaining the specified level, and for each event thereafter, as
 * specified by the caller.
 * 
 * @param *sub_number (OUT) A notify id for the resulting subscription is returned in
 * the provided memory location. Callers should save this
 * number for later use if (for example) it is desired to temporarily turn "off" the
 * subscription or to permanently remove the subscription from the registry
 * 
 * @param cb_func (IN) The orte_registry_notify_cb_fn_t callback function to be called when
 * a subscription is triggered. The data from each monitored object will be returned
 * to the callback function in an orte_gpr_notify_message_t structure.
 * 
 * @param user_tag (IN) A void* user-provided storage location that the caller can
 * use for its own purposes. A NULL value is acceptable.
 *
 * @retval ORTE_SUCCESS Operation was successfully completed.
 * @retval ORTE_ERROR(s) Operation failed, returning the provided error code.
 *
 * @code
 * orte_gpr_notify_id_t sub_number;
 * orte_gpr_value_t value, trig_value;
 * 
 * status_code = orte_gpr.subscribe(addr_mode, action, &value, &trig_value,
 *                                       &sub_number, cb_func, user_tag);
 * @endcode
 */
typedef int (*orte_gpr_base_module_subscribe_fn_t)(orte_gpr_addr_mode_t addr_mode,
                            orte_gpr_notify_action_t actions,
                            orte_gpr_value_t *value,
                            orte_gpr_value_t *trig_value,
                            orte_gpr_notify_id_t *sub_number,
                            orte_gpr_notify_cb_fn_t cb_func, void *user_tag);

/*
 * Cancel a subscription.
 * Once a subscription has been entered on the registry, a caller may choose to permanently
 * remove it at a later time. This function supports that request.
 *
 * @param sub_number The orte_gpr_notify_id_t value returned by the original subscribe
 * command.
 *
 * @retval ORTE_SUCCESS The subscription was removed.
 * @retval ORTE_ERROR The subscription could not be removed - most likely caused by specifying
 * a non-existent (or previously removed) subscription number.
 *
 * @code
 * status_code = orte_gpr.unsubscribe(sub_number);
 * @endcode
 */
typedef int (*orte_gpr_base_module_unsubscribe_fn_t)(orte_gpr_notify_id_t sub_number);

/* Output the registry's contents to an output stream
 * For debugging purposes, it is helpful to be able to obtain a complete formatted printout
 * of the registry's contents. This function provides that ability.
 *
 * @param output_id The output stream id to which the registry's contents are to be
 * printed.
 *
 * @retval ORTE_SUCCESS Operation was successfully completed.
 * @retval ORTE_ERROR(s) Operation failed, returning the provided error code.
 *
 * @code
 * orte_gpr.dump(output_id);
 * @endcode
 */
typedef int (*orte_gpr_base_module_dump_fn_t)(int output_id);

/* Deliver a notify message.
 * The registry generates notify messages whenever a subscription is fired. Normally,
 * this happens completely "under the covers" - i.e., the notification process is transparent
 * to the rest of the system, with the message simply delivered to the specified callback function.
 * However, there are two circumstances when the system needs to explicitly deliver a notify
 * message - namely, during startup and shutdown. In these two cases, a special message is
 * "xcast" to all processes, with each process receiving the identical message. In order to
 * ensure that the correct data gets to each subsystem, the message must be disassembled and
 * the appropriate callback function called.
 *
 * This, unfortunately, means that the decoder must explicitly call the message notification
 * subsystem in order to find the callback function. Alternatively, the entire startup/shutdown
 * logic could be buried in the registry, but this violates the design philosophy of the registry
 * acting solely as a publish/subscribe-based cache memory - it should not contain logic pertinent
 * to any usage of that memory.
 *
 * This function provides the necessary "hook" for an external program to request delivery of
 * a message via the publish/subscribe's notify mechanism.
 *
 * @param message The message to be delivered.
 *
 * @retval ORTE_SUCCESS Operation was successfully completed.
 * @retval ORTE_ERROR(s) Operation failed, returning the provided error code.
 *
 * @code
 * status_code = orte_gpr.deliver_notify_msg(message);
 * @endcode
 *
 */
typedef int (*orte_gpr_base_module_deliver_notify_msg_fn_t)(orte_gpr_notify_message_t *message);

/*
 * Increment value
 * This function increments the stored value of an existing registry entry by one. Failure
 * to find the entry on the registry will result in an error.
 */
typedef int (*orte_gpr_base_module_increment_value_fn_t)(orte_gpr_addr_mode_t addr_mode,
                                                         orte_gpr_value_t *value);

/*
 * Decrement value
 * This function decrements the stored value of an existing registry entry by one. Failure
 * to find the entry on the registry will result in an error.
 */
typedef int (*orte_gpr_base_module_decrement_value_fn_t)(orte_gpr_addr_mode_t addr_mode,
                                                         orte_gpr_value_t *value);

/*
 * test interface for internal functions - optional to provide
 */
typedef int (*orte_gpr_base_module_test_internals_fn_t)(int level, ompi_list_t **results);


/*
 * Ver 1.0.0
 */
struct orte_gpr_base_module_1_0_0_t {
    /* INIT */
    orte_gpr_base_module_init_fn_t init;
    /* BLOCKING OPERATIONS */
    orte_gpr_base_module_get_fn_t get;
    orte_gpr_base_module_put_fn_t put;
    orte_gpr_base_module_delete_entries_fn_t delete_entries;
    orte_gpr_base_module_delete_segment_fn_t delete_segment;
    orte_gpr_base_module_index_fn_t index;
    /* NON-BLOCKING OPERATIONS */
    orte_gpr_base_module_get_nb_fn_t get_nb;
    orte_gpr_base_module_put_nb_fn_t put_nb;
    orte_gpr_base_module_delete_entries_nb_fn_t delete_entries_nb;
    orte_gpr_base_module_delete_segment_nb_fn_t delete_segment_nb;
    orte_gpr_base_module_index_nb_fn_t index_nb;
    /* GENERAL OPERATIONS */
    orte_gpr_base_module_preallocate_segment_fn_t preallocate_segment;
    orte_gpr_base_module_deliver_notify_msg_fn_t deliver_notify_msg;
    /* ARITHMETIC OPERATIONS */
    orte_gpr_base_module_increment_value_fn_t increment_value;
    orte_gpr_base_module_decrement_value_fn_t decrement_value;
    /* SUBSCRIBE OPERATIONS */
    orte_gpr_base_module_subscribe_fn_t subscribe;
    orte_gpr_base_module_unsubscribe_fn_t unsubscribe;
    /* COMPOUND COMMANDS */
    orte_gpr_base_module_begin_compound_cmd_fn_t begin_compound_cmd;
    orte_gpr_base_module_stop_compound_cmd_fn_t stop_compound_cmd;
    orte_gpr_base_module_exec_compound_cmd_fn_t exec_compound_cmd;
    /* DUMP */
    orte_gpr_base_module_dump_fn_t dump;
    /* MODE OPERATIONS */
    orte_gpr_base_module_notify_on_fn_t notify_on;
    orte_gpr_base_module_notify_off_fn_t notify_off;
    /* CLEANUP OPERATIONS */
    orte_gpr_base_module_cleanup_job_fn_t cleanup_job;
    orte_gpr_base_module_cleanup_proc_fn_t cleanup_process;
    /* TEST INTERFACE */
    orte_gpr_base_module_test_internals_fn_t test_internals;
};
typedef struct orte_gpr_base_module_1_0_0_t orte_gpr_base_module_1_0_0_t;
typedef orte_gpr_base_module_1_0_0_t orte_gpr_base_module_t;

/*
 * GPR Component
 */

typedef orte_gpr_base_module_t* (*orte_gpr_base_component_init_fn_t)(
								   bool *allow_multi_user_threads,
								   bool *have_hidden_threads,
								   int *priority);

typedef int (*orte_gpr_base_component_finalize_fn_t)(void);
 
/*
 * the standard component data structure
 */


struct mca_gpr_base_component_1_0_0_t {
    mca_base_component_t gpr_version;
    mca_base_component_data_1_0_0_t gpr_data;

    orte_gpr_base_component_init_fn_t gpr_init;
    orte_gpr_base_component_finalize_fn_t gpr_finalize;
};
typedef struct mca_gpr_base_component_1_0_0_t mca_gpr_base_component_1_0_0_t;
typedef mca_gpr_base_component_1_0_0_t mca_gpr_base_component_t;

#if defined(c_plusplus) || defined(__cplusplus)
}
#endif

/*
 * Macro for use in modules that are of type gpr v1.0.0
 */
#define MCA_GPR_BASE_VERSION_1_0_0		\
    /* gpr v1.0 is chained to MCA v1.0 */	\
    MCA_BASE_VERSION_1_0_0,			\
	/* gpr v1.0 */				\
	"gpr", 1, 0, 0

/*
 * global module that holds function pointers
 */
extern orte_gpr_base_module_t orte_gpr; /* holds selected module's function pointers */

#endif
