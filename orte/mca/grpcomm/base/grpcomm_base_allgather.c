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
 * Copyright (c) 2007      Sun Microsystems, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orte_config.h"
#include "orte/constants.h"
#include "orte/types.h"

#include <string.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif  /* HAVE_SYS_TIME_H */

#include "opal/threads/condition.h"
#include "opal/util/output.h"

#include "orte/util/proc_info.h"
#include "opal/dss/dss.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/odls/odls_types.h"
#include "orte/mca/rml/rml.h"
#include "orte/runtime/orte_globals.h"
#include "orte/util/name_fns.h"
#include "orte/orted/orted.h"
#include "orte/runtime/orte_wait.h"

#include "orte/mca/grpcomm/base/base.h"

static bool allgather_failed;
static orte_std_cntr_t allgather_num_recvd;
static opal_buffer_t *allgather_buf;

static void allgather_server_recv(int status, orte_process_name_t* sender,
                                  opal_buffer_t *buffer,
                                  orte_rml_tag_t tag, void *cbdata)
{
    int rc;
    
    OPAL_OUTPUT_VERBOSE((2, orte_grpcomm_base_output,
                         "%s allgather buffer received from %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(sender)));
    
    /* append this data to the allgather_buf */
    if (ORTE_SUCCESS != (rc = opal_dss.copy_payload(allgather_buf, buffer))) {
        ORTE_ERROR_LOG(rc);
        allgather_failed = true;
        return;
    }

    /* bump the counter */
    ++allgather_num_recvd;
    
    /* reissue the recv */
    rc = orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORTE_RML_TAG_ALLGATHER_SERVER,
                                 ORTE_RML_NON_PERSISTENT, allgather_server_recv, NULL);
    if (rc != ORTE_SUCCESS && rc != ORTE_ERR_NOT_IMPLEMENTED) {
        ORTE_ERROR_LOG(rc);
        allgather_failed = true;
    }
}

static void allgather_client_recv(int status, orte_process_name_t* sender,
                                  opal_buffer_t *buffer,
                                  orte_rml_tag_t tag, void *cbdata)
{
    int rc;
    
    OPAL_OUTPUT_VERBOSE((2, orte_grpcomm_base_output,
                         "%s grpcomm:base: allgather buffer received",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    /* transfer the buffer */
    if (ORTE_SUCCESS != (rc = opal_dss.copy_payload(allgather_buf, buffer))) {
        ORTE_ERROR_LOG(rc);
        allgather_failed = true;
    }
    
    /* bump the counter */
    ++allgather_num_recvd;
}

int orte_grpcomm_base_allgather(opal_buffer_t *sbuf, opal_buffer_t *rbuf)
{
    orte_process_name_t name;
    int rc;
    
    OPAL_OUTPUT_VERBOSE((1, orte_grpcomm_base_output,
                         "%s grpcomm: entering allgather",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    /* everything happens within my jobid */
    name.jobid = ORTE_PROC_MY_NAME->jobid;
    
    /***   RANK != 0   ***/
    if (0 != ORTE_PROC_MY_NAME->vpid) {
        /* everyone but rank=0 sends data */
        name.vpid = 0;
        if (0 > orte_rml.send_buffer(&name, sbuf, ORTE_RML_TAG_ALLGATHER_SERVER, 0)) {
            ORTE_ERROR_LOG(ORTE_ERR_COMM_FAILURE);
            return ORTE_ERR_COMM_FAILURE;
        }
        OPAL_OUTPUT_VERBOSE((2, orte_grpcomm_base_output,
                             "%s allgather buffer sent",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        
        /* setup the buffer that will recv the results */
        allgather_buf = OBJ_NEW(opal_buffer_t);
        
        /* now receive the final result from rank=0. Be sure to do this in
         * a manner that allows us to return without being in a recv!
         */
        allgather_num_recvd = 0;
        allgather_failed = false;
        rc = orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORTE_RML_TAG_ALLGATHER_CLIENT,
                                     ORTE_RML_NON_PERSISTENT, allgather_client_recv, NULL);
        if (rc != ORTE_SUCCESS && rc != ORTE_ERR_NOT_IMPLEMENTED) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }
        
        ORTE_PROGRESSED_WAIT(allgather_failed, allgather_num_recvd, 1);
        
        /* if the allgather failed, return an error */
        if (allgather_failed) {
            ORTE_ERROR_LOG(ORTE_ERR_COMM_FAILURE);
            OBJ_RELEASE(allgather_buf);
            return ORTE_ERR_COMM_FAILURE;
        }
        
        /* copy payload to the caller's buffer */
        if (ORTE_SUCCESS != (rc = opal_dss.copy_payload(rbuf, allgather_buf))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(allgather_buf);
            return rc;
        }
        OBJ_RELEASE(allgather_buf);

        OPAL_OUTPUT_VERBOSE((2, orte_grpcomm_base_output,
                             "%s allgather buffer received",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        
        return ORTE_SUCCESS;
    }
    
    
    /***   RANK = 0   ***/
    /* seed the outgoing buffer with the num_procs so it can be unpacked */
    if (ORTE_SUCCESS != (rc = opal_dss.pack(rbuf, &orte_process_info.num_procs, 1, ORTE_STD_CNTR))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    
    /* put my own information into the outgoing buffer */
    if (ORTE_SUCCESS != (rc = opal_dss.copy_payload(rbuf, sbuf))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    
    OPAL_OUTPUT_VERBOSE((2, orte_grpcomm_base_output,
                         "%s allgather collecting buffers",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    /* setup the recv conditions */
    allgather_failed = false;
    allgather_num_recvd = 0;
    
    /* setup the buffer that will recv the results */
    allgather_buf = OBJ_NEW(opal_buffer_t);
    
    /* post the non-blocking recv */
    rc = orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORTE_RML_TAG_ALLGATHER_SERVER,
                                 ORTE_RML_NON_PERSISTENT, allgather_server_recv, NULL);
    if (rc != ORTE_SUCCESS && rc != ORTE_ERR_NOT_IMPLEMENTED) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    
    ORTE_PROGRESSED_WAIT(allgather_failed, allgather_num_recvd, (orte_std_cntr_t)orte_process_info.num_procs-1);
    
    /* cancel the lingering recv */
    if (ORTE_SUCCESS != (rc = orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORTE_RML_TAG_ALLGATHER_SERVER))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(allgather_buf);
        return rc;
    }
    
    /* if the allgather failed, say so */
    if (allgather_failed) {
        OPAL_OUTPUT_VERBOSE((2, orte_grpcomm_base_output,
                             "%s allgather failed!",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        OBJ_RELEASE(allgather_buf);
        return ORTE_ERROR;
    }
    
    OPAL_OUTPUT_VERBOSE((2, orte_grpcomm_base_output,
                         "%s allgather xcasting collected data",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    /* copy the received info to the caller's buffer */
    if (ORTE_SUCCESS != (rc = opal_dss.copy_payload(rbuf, allgather_buf))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(allgather_buf);
        return rc;
    }
    OBJ_RELEASE(allgather_buf);
    
    /* xcast the results */
    orte_grpcomm.xcast(ORTE_PROC_MY_NAME->jobid, rbuf, ORTE_RML_TAG_ALLGATHER_CLIENT);
    
    /* xcast automatically ensures that the sender -always- gets a copy
     * of the message. This is required to ensure proper operation of the
     * launch system as the HNP -must- get a copy itself. So we have to
     * post our own receive here so that we don't leave a message rattling
     * around in our RML
     */
    /* setup the buffer that will recv the results */
    allgather_buf = OBJ_NEW(opal_buffer_t);
    
    /* receive the echo'd message. Be sure to do this in
     * a manner that allows us to return without being in a recv!
     */
    allgather_num_recvd = 0;
    allgather_failed = false;
    rc = orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORTE_RML_TAG_ALLGATHER_CLIENT,
                                 ORTE_RML_NON_PERSISTENT, allgather_client_recv, NULL);
    if (rc != ORTE_SUCCESS && rc != ORTE_ERR_NOT_IMPLEMENTED) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    
    ORTE_PROGRESSED_WAIT(allgather_failed, allgather_num_recvd, 1);
    
    /* if the allgather failed, return an error */
    if (allgather_failed) {
        ORTE_ERROR_LOG(ORTE_ERR_COMM_FAILURE);
        OBJ_RELEASE(allgather_buf);
        return ORTE_ERR_COMM_FAILURE;
    }

    /* don't need the received buffer - we already have what we need in rbuf */
    OBJ_DESTRUCT(allgather_buf);
    
    OPAL_OUTPUT_VERBOSE((1, orte_grpcomm_base_output,
                         "%s grpcomm: allgather completed",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    return ORTE_SUCCESS;
}



static orte_std_cntr_t allgather_num_sent;
static void allgather_send_cb(int status, orte_process_name_t* sender,
                              opal_buffer_t *buffer,
                              orte_rml_tag_t tag, void *cbdata)
{
    /* increment the count */
    ++allgather_num_sent;
}


int orte_grpcomm_base_allgather_list(opal_list_t *names, opal_buffer_t *sbuf, opal_buffer_t *rbuf)
{
    opal_list_item_t *item;
    orte_namelist_t *peer, *root;
    orte_std_cntr_t num_peers;
    int rc;
    
    OPAL_OUTPUT_VERBOSE((1, orte_grpcomm_base_output,
                         "%s grpcomm: entering allgather_list",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    /* the first entry on the list is the "root" that collects
     * all the data - everyone else just sends and gets back
     * the results
     */
    root = (orte_namelist_t*)opal_list_get_first(names);
    
    /***   NON-ROOT   ***/
    if (OPAL_EQUAL != opal_dss.compare(&root->name, ORTE_PROC_MY_NAME, ORTE_NAME)) {
        /* everyone but root sends data */
        OPAL_OUTPUT_VERBOSE((2, orte_grpcomm_base_output,
                             "%s allgather_list: sending my data to %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             ORTE_NAME_PRINT(&root->name)));
        
        if (0 > orte_rml.send_buffer(&root->name, sbuf, ORTE_RML_TAG_ALLGATHER_SERVER, 0)) {
            ORTE_ERROR_LOG(ORTE_ERR_COMM_FAILURE);
            return ORTE_ERR_COMM_FAILURE;
        }
        
        OPAL_OUTPUT_VERBOSE((2, orte_grpcomm_base_output,
                             "%s allgather_list: buffer sent",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        
        /* setup the buffer that will recv the results */
        allgather_buf = OBJ_NEW(opal_buffer_t);
        
        /* now receive the final result from rank=0. Be sure to do this in
         * a manner that allows us to return without being in a recv!
         */
        allgather_num_recvd = 0;
        allgather_failed = false;
        rc = orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORTE_RML_TAG_ALLGATHER_CLIENT,
                                     ORTE_RML_NON_PERSISTENT, allgather_client_recv, NULL);
        if (rc != ORTE_SUCCESS && rc != ORTE_ERR_NOT_IMPLEMENTED) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }
        
        ORTE_PROGRESSED_WAIT(allgather_failed, allgather_num_recvd, 1);
        
        /* if the allgather failed, return an error */
        if (allgather_failed) {
            ORTE_ERROR_LOG(ORTE_ERR_COMM_FAILURE);
            OBJ_RELEASE(allgather_buf);
            return ORTE_ERR_COMM_FAILURE;
        }
        
        /* copy payload to the caller's buffer */
        if (ORTE_SUCCESS != (rc = opal_dss.copy_payload(rbuf, allgather_buf))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(allgather_buf);
            return rc;
        }
        OBJ_RELEASE(allgather_buf);
        
        OPAL_OUTPUT_VERBOSE((2, orte_grpcomm_base_output,
                             "%s allgather_list: buffer received",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
        
        return ORTE_SUCCESS;
    }
    
    
    /***   ROOT   ***/
    /* count how many peers are participating, including myself */
    num_peers = (orte_std_cntr_t)opal_list_get_size(names);

    /* seed the outgoing buffer with the num_procs so it can be unpacked */
    if (ORTE_SUCCESS != (rc = opal_dss.pack(rbuf, &num_peers, 1, ORTE_STD_CNTR))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    
    /* put my own information into the outgoing buffer */
    if (ORTE_SUCCESS != (rc = opal_dss.copy_payload(rbuf, sbuf))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    
    /* setup the recv conditions */
    allgather_failed = false;
    allgather_num_recvd = 0;
    
    /* setup the buffer that will recv the results */
    allgather_buf = OBJ_NEW(opal_buffer_t);
    
    OPAL_OUTPUT_VERBOSE((2, orte_grpcomm_base_output,
                         "%s allgather_list: waiting to recv %ld inputs",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         (long)num_peers-1));
    
    /* post the non-blocking recv */
    rc = orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORTE_RML_TAG_ALLGATHER_SERVER,
                                 ORTE_RML_NON_PERSISTENT, allgather_server_recv, NULL);
    if (rc != ORTE_SUCCESS && rc != ORTE_ERR_NOT_IMPLEMENTED) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    
    ORTE_PROGRESSED_WAIT(allgather_failed, allgather_num_recvd, num_peers-1);
    
    /* cancel the lingering recv */
    if (ORTE_SUCCESS != (rc = orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORTE_RML_TAG_ALLGATHER_SERVER))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(allgather_buf);
        return rc;
    }
    
    OPAL_OUTPUT_VERBOSE((2, orte_grpcomm_base_output,
                         "%s allgather_list: received all data",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    /* copy the received info to the caller's buffer */
    if (ORTE_SUCCESS != (rc = opal_dss.copy_payload(rbuf, allgather_buf))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(allgather_buf);
        return rc;
    }
    OBJ_RELEASE(allgather_buf);
    
    /* broadcast the results */
    allgather_num_sent = 0;
    for (item = opal_list_get_first(names);
         item != opal_list_get_end(names);
         item = opal_list_get_next(item)) {
        peer = (orte_namelist_t*)item;
        
        /* skip myself */
        if (OPAL_EQUAL == opal_dss.compare(&root->name, &peer->name, ORTE_NAME)) {
            continue;
        }
        
        /* transmit the buffer to this process */
        if (0 > orte_rml.send_buffer_nb(&peer->name, rbuf, ORTE_RML_TAG_ALLGATHER_CLIENT,
                                        0, allgather_send_cb, 0)) {
            ORTE_ERROR_LOG(ORTE_ERR_COMM_FAILURE);
            return ORTE_ERR_COMM_FAILURE;
        }
    }
    
    ORTE_PROGRESSED_WAIT(false, allgather_num_sent, num_peers-1);
    
    OPAL_OUTPUT_VERBOSE((1, orte_grpcomm_base_output,
                         "%s grpcomm: allgather_list completed",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    return ORTE_SUCCESS;
}
