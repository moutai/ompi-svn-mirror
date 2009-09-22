/*
 * Copyright (c) 2009      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2009      The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 *
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 *
 */

#include "orte_config.h"
#include "orte/constants.h"

#include <sys/types.h>
#include <stdio.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "opal/util/argv.h"
#include "opal/util/if.h"
#include "opal/mca/paffinity/paffinity.h"

#include "orte/mca/rmcast/base/base.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/util/show_help.h"
#include "orte/util/proc_info.h"
#include "orte/util/name_fns.h"
#include "orte/util/nidmap.h"
#include "orte/runtime/orte_wait.h"
#include "orte/runtime/orte_globals.h"

#include "orte/mca/ess/ess.h"
#include "orte/mca/ess/base/base.h"
#include "orte/mca/ess/cm/ess_cm.h"

static int rte_init(void);
static int rte_finalize(void);
static void rte_abort(int status, bool report) __opal_attribute_noreturn__;
static uint8_t proc_get_locality(orte_process_name_t *proc);
static orte_vpid_t proc_get_daemon(orte_process_name_t *proc);
static char* proc_get_hostname(orte_process_name_t *proc);
static orte_local_rank_t proc_get_local_rank(orte_process_name_t *proc);
static orte_node_rank_t proc_get_node_rank(orte_process_name_t *proc);
static int update_pidmap(opal_byte_object_t *bo);
static int update_nidmap(opal_byte_object_t *bo);


orte_ess_base_module_t orte_ess_cm_module = {
    rte_init,
    rte_finalize,
    rte_abort,
    proc_get_locality,
    proc_get_daemon,
    proc_get_hostname,
    proc_get_local_rank,
    proc_get_node_rank,
    update_pidmap,
    update_nidmap,
    NULL /* ft_event */
};

static int cm_set_name(void);

static int rte_init(void)
{
    int ret;
    char *error = NULL;
    char **hosts = NULL;
    char *nodelist;

    /* only daemons that are bootstrapping should
     * be calling this module
     */

    /* initialize the global list of local children and job data */
    OBJ_CONSTRUCT(&orte_local_children, opal_list_t);
    OBJ_CONSTRUCT(&orte_local_jobdata, opal_list_t);
    
    /* run the prolog */
    if (ORTE_SUCCESS != (ret = orte_ess_base_std_prolog())) {
        error = "orte_ess_base_std_prolog";
        goto error;
    }
    
    /* open the reliable multicast framework, just in
     * case we need it to query the HNP for a name
     */
    if (ORTE_SUCCESS != (ret = orte_rmcast_base_open())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_rmcast_base_open";
        goto error;
    }
    
    if (ORTE_SUCCESS != (ret = orte_rmcast_base_select())) {
        ORTE_ERROR_LOG(ret);
        error = "orte_rmcast_base_select";
        goto error;
    }
    
    /* get a name for ourselves */
    if (ORTE_SUCCESS != (ret = cm_set_name())) {
        error = "set_name";
        goto error;
    }
    
    /* get the list of nodes used for this job */
    nodelist = getenv("OMPI_MCA_orte_nodelist");
    
    if (NULL != nodelist) {
        /* split the node list into an argv array */
        hosts = opal_argv_split(nodelist, ',');
    }
    if (ORTE_SUCCESS != (ret = orte_ess_base_orted_setup(hosts))) {
        ORTE_ERROR_LOG(ret);
        error = "orte_ess_base_orted_setup";
        goto error;
    }
    opal_argv_free(hosts);
    return ORTE_SUCCESS;
    
error:
    orte_show_help("help-orte-runtime.txt",
                   "orte_init:startup:internal-failure",
                   true, error, ORTE_ERROR_NAME(ret), ret);
    
    return ret;
}

static int rte_finalize(void)
{
    int ret;
    
    if (ORTE_SUCCESS != (ret = orte_ess_base_orted_finalize())) {
        ORTE_ERROR_LOG(ret);
    }
    
    /* deconstruct the nidmap and jobmap arrays */
    orte_util_nidmap_finalize();
    
    return ret;    
}

/*
 * If we are a cm, it could be beneficial to get a core file, so
 * we call abort.
 */
static void rte_abort(int status, bool report)
{
    /* do NOT do a normal finalize as this will very likely
     * hang the process. We are aborting due to an abnormal condition
     * that precludes normal cleanup 
     *
     * We do need to do the following bits to make sure we leave a 
     * clean environment. Taken from orte_finalize():
     * - Assume errmgr cleans up child processes before we exit.
     */
    
    /* - Clean out the global structures 
     * (not really necessary, but good practice)
     */
    orte_proc_info_finalize();
    
    /* Now exit/abort */
    if (report) {
        abort();
    }
    
    /* otherwise, just exit */
    exit(status);
}

static uint8_t proc_get_locality(orte_process_name_t *proc)
{
    orte_nid_t *nid;
    
    if (NULL == (nid = orte_util_lookup_nid(proc))) {
        ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
        return OPAL_PROC_NON_LOCAL;
    }
    
    if (nid->daemon == ORTE_PROC_MY_DAEMON->vpid) {
        OPAL_OUTPUT_VERBOSE((2, orte_ess_base_output,
                             "%s ess:cm: proc %s on LOCAL NODE",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             ORTE_NAME_PRINT(proc)));
        return (OPAL_PROC_ON_NODE | OPAL_PROC_ON_CU | OPAL_PROC_ON_CLUSTER);
    }
    
    OPAL_OUTPUT_VERBOSE((2, orte_ess_base_output,
                         "%s ess:cm: proc %s is REMOTE",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(proc)));
    
    return OPAL_PROC_NON_LOCAL;
    
}

static orte_vpid_t proc_get_daemon(orte_process_name_t *proc)
{
    orte_nid_t *nid;
    
    if( ORTE_JOBID_IS_DAEMON(proc->jobid) ) {
        return proc->vpid;
    }
    
    if (NULL == (nid = orte_util_lookup_nid(proc))) {
        return ORTE_VPID_INVALID;
    }
    
    OPAL_OUTPUT_VERBOSE((2, orte_ess_base_output,
                         "%s ess:cm: proc %s is hosted by daemon %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(proc),
                         ORTE_VPID_PRINT(nid->daemon)));
    
    return nid->daemon;
}

static char* proc_get_hostname(orte_process_name_t *proc)
{
    orte_nid_t *nid;
    
    if (NULL == (nid = orte_util_lookup_nid(proc))) {
        ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
        return NULL;
    }
    
    OPAL_OUTPUT_VERBOSE((2, orte_ess_base_output,
                         "%s ess:cm: proc %s is on host %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(proc),
                         nid->name));
    
    return nid->name;
}

static orte_local_rank_t proc_get_local_rank(orte_process_name_t *proc)
{
    orte_pmap_t *pmap;
    
    if (NULL == (pmap = orte_util_lookup_pmap(proc))) {
        ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
        return ORTE_LOCAL_RANK_INVALID;
    }    
    
    OPAL_OUTPUT_VERBOSE((2, orte_ess_base_output,
                         "%s ess:cm: proc %s has local rank %d",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(proc),
                         (int)pmap->local_rank));
    
    return pmap->local_rank;
}

static orte_node_rank_t proc_get_node_rank(orte_process_name_t *proc)
{
    orte_pmap_t *pmap;
    
    /* is this me? */
    if (proc->jobid == ORTE_PROC_MY_NAME->jobid &&
        proc->vpid == ORTE_PROC_MY_NAME->vpid) {
        /* yes it is - since I am a daemon, it can only
         * be zero
         */
        return 0;
    }
    
    if (NULL == (pmap = orte_util_lookup_pmap(proc))) {
        return ORTE_NODE_RANK_INVALID;
    }    
    
    OPAL_OUTPUT_VERBOSE((2, orte_ess_base_output,
                         "%s ess:cm: proc %s has node rank %d",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(proc),
                         (int)pmap->node_rank));
    
    return pmap->node_rank;
}

static int update_pidmap(opal_byte_object_t *bo)
{
    int ret;
    
    OPAL_OUTPUT_VERBOSE((2, orte_ess_base_output,
                         "%s ess:cm: updating pidmap",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    /* build the pmap */
    if (ORTE_SUCCESS != (ret = orte_util_decode_pidmap(bo))) {
        ORTE_ERROR_LOG(ret);
    }
    
    return ret;
}

static int update_nidmap(opal_byte_object_t *bo)
{
    int rc;
    /* decode the nidmap - the util will know what to do */
    if (ORTE_SUCCESS != (rc = orte_util_decode_nodemap(bo))) {
        ORTE_ERROR_LOG(rc);
    }    
    return rc;
}

/* support for setting name */
static bool arrived = false;
static bool name_success = false;

static void cbfunc(int channel, opal_buffer_t *buf, void *cbdata)
{
    int32_t n;
    orte_process_name_t name, *nmptr;
    int rc;
    
    /* ensure we default to failure */
    name_success = false;

    /* unpack the response */
    nmptr = &name;
    n = 1;
    if (ORTE_SUCCESS != (rc = opal_dss.unpack(buf, &nmptr, &n, ORTE_NAME))) {
        ORTE_ERROR_LOG(rc);
        goto depart;
    }
    /* setup name */
    ORTE_PROC_MY_NAME->jobid = name.jobid;
    ORTE_PROC_MY_NAME->vpid = name.vpid;
    name_success = true;
    
depart:
    arrived = true;
}

static int cm_set_name(void)
{
    int i, rc;
    struct sockaddr_in if_addr;
    char *ifnames[] = {
        "ce",
        "eth0",
        "eth1",
        NULL
    };
    int32_t net, rack, slot, function;
    int32_t addr;
    opal_buffer_t buf;
    orte_daemon_cmd_flag_t cmd;
    
    /* try constructing the name from the IP address - first,
     * find an appropriate interface
     */
    for (i=0; NULL != ifnames[i]; i++) {
        if (ORTE_SUCCESS != (rc = opal_ifnametoaddr(ifnames[i],
                                                    (struct sockaddr*)&if_addr,
                                                    sizeof(struct sockaddr_in)))) {
            continue;
        }
        addr = htonl(if_addr.sin_addr.s_addr);
        opal_output(0, "IP address: %d.%d.%d.%d", OPAL_IF_FORMAT_ADDR(addr));

        /* break address into sections */
        net = 0x000000FF & ((0xFF000000 & addr) >> 24);
        rack = 0x000000FF & ((0x00FF0000 & addr) >> 16);
        slot = 0x000000FF & ((0x0000FF00 & addr) >> 8);
        function = 0x000000FF & addr;
        
        /* is this an appropriate interface to use */
        if (10 == net) {
            /* set our vpid - add 1 to ensure it cannot be zero */
            ORTE_PROC_MY_NAME->vpid = (rack * mca_ess_cm_component.max_slots) + slot + function + 1;
            /* set our jobid to 0 */
            ORTE_PROC_MY_NAME->jobid = 0;
            return ORTE_SUCCESS;
        } else if (192 == net && 168 == rack) {
            /* just use function */
            ORTE_PROC_MY_NAME->vpid = function + 1;
            /* set our jobid to 0 */
            ORTE_PROC_MY_NAME->jobid = 0;
            return ORTE_SUCCESS;
        }
    }
    
    /* if we get here, then we didn't find a usable interface.
     * use the reliable multicast system to contact the HNP and
     * get a name
     */
    OBJ_CONSTRUCT(&buf, opal_buffer_t);
    cmd = ORTE_DAEMON_NAME_REQ_CMD;
    opal_dss.pack(&buf, &cmd, 1, ORTE_DAEMON_CMD_T);
    
    /* set the recv to get the answer */
    if (ORTE_SUCCESS != (rc = orte_rmcast.recv_nb(ORTE_RMCAST_SYS_ADDR, cbfunc, NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&buf);
        return rc;
    }
    /* send the request */
    if (ORTE_SUCCESS != (rc = orte_rmcast.send(ORTE_RMCAST_SYS_ADDR, &buf))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&buf);
        return rc;
    }
    OBJ_DESTRUCT(&buf);

    /* wait for response */
    ORTE_PROGRESSED_WAIT(arrived, 0, 1);
    
    /* if we got a valid name, return success */
    if (name_success) {
        return ORTE_SUCCESS;
    }
    return ORTE_ERR_NOT_FOUND;
}

