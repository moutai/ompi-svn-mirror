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
/** @file:
 *
 */

#include "orte_config.h"
#include "include/orte_constants.h"
#include "include/orte_types.h"
#include "mca/mca.h"
#include "util/bufpack.h"
#include "mca/oob/base/base.h"

#include "ns_proxy.h"

/**
 * globals
 */

/*
 * functions
 */

int orte_ns_proxy_create_cellid(orte_cellid_t *cellid)
{
    ompi_buffer_t cmd;
    ompi_buffer_t answer;
    orte_ns_cmd_flag_t command;
    int recv_tag;

    /* set the default value of error */
    *cellid = ORTE_CELLID_MAX;
    
    command = ORTE_NS_CREATE_CELLID_CMD;
    recv_tag = MCA_OOB_TAG_NS;

    if (OMPI_SUCCESS != ompi_buffer_init(&cmd, 0)) { /* got a problem */
	   return ORTE_ERR_OUT_OF_RESOURCE;
    }

    if (OMPI_SUCCESS != ompi_pack(cmd, (void*)&command, 1, ORTE_NS_OOB_PACK_CMD)) {
	   return ORTE_ERR_PACK_FAILURE;
    }

    if (0 > mca_oob_send_packed(orte_ns_my_replica, cmd, MCA_OOB_TAG_NS, 0)) {
	   return ORTE_ERR_COMM_FAILURE;
    }

    if (0 > mca_oob_recv_packed(orte_ns_my_replica, &answer, &recv_tag)) {
	   return ORTE_ERR_COMM_FAILURE;
    }

    if ((OMPI_SUCCESS != ompi_unpack(answer, &command, 1, ORTE_NS_OOB_PACK_CMD))
	|| (ORTE_NS_CREATE_CELLID_CMD != command)) {
	   ompi_buffer_free(answer);
	   return ORTE_ERR_UNPACK_FAILURE;
    }

    if (OMPI_SUCCESS != ompi_unpack(answer, cellid, 1, ORTE_NS_OOB_PACK_CELLID)) {
	   ompi_buffer_free(answer);
	   return ORTE_ERR_UNPACK_FAILURE;
    }
    
	ompi_buffer_free(answer);
	return ORTE_SUCCESS;
}


int orte_ns_proxy_create_jobid(orte_jobid_t *job)
{
    ompi_buffer_t cmd;
    ompi_buffer_t answer;
    orte_ns_cmd_flag_t command;
    int recv_tag;

    command = ORTE_NS_CREATE_JOBID_CMD;
    recv_tag = MCA_OOB_TAG_NS;

    /* set default value */
    *job = ORTE_JOBID_MAX;
    
    if (OMPI_SUCCESS != ompi_buffer_init(&cmd, 0)) { /* got a problem */
	   return ORTE_ERR_OUT_OF_RESOURCE;
    }

    if (OMPI_SUCCESS != ompi_pack(cmd, (void*)&command, 1, ORTE_NS_OOB_PACK_CMD)) { /* got a problem */
	   return OMPI_ERR_PACK_FAILURE;
    }

    if (0 > mca_oob_send_packed(orte_ns_my_replica, cmd, MCA_OOB_TAG_NS, 0)) {
	   return ORTE_ERR_COMM_FAILURE;
    }

    if (0 > mca_oob_recv_packed(orte_ns_my_replica, &answer, &recv_tag)) {
	   return ORTE_ERR_COMM_FAILURE;
    }

    if ((OMPI_SUCCESS != ompi_unpack(answer, &command, 1, ORTE_NS_OOB_PACK_CMD))
	|| (ORTE_NS_CREATE_JOBID_CMD != command)) {
	   ompi_buffer_free(answer);
	   return ORTE_ERR_UNPACK_FAILURE;
    }

    if (OMPI_SUCCESS != ompi_unpack(answer, job, 1, ORTE_NS_OOB_PACK_JOBID)) {
	   ompi_buffer_free(answer);
	   return ORTE_ERR_UNPACK_FAILURE;
    }
    
	ompi_buffer_free(answer);
	return ORTE_SUCCESS;
}


int orte_ns_proxy_reserve_range(orte_jobid_t job, orte_vpid_t range, orte_vpid_t *starting_vpid)
{
    ompi_buffer_t cmd;
    ompi_buffer_t answer;
    orte_ns_cmd_flag_t command;
    int recv_tag;

    command = ORTE_NS_RESERVE_RANGE_CMD;
    recv_tag = MCA_OOB_TAG_NS;

    /* set default return value */
    *starting_vpid = ORTE_VPID_MAX;
    
    if (OMPI_SUCCESS != ompi_buffer_init(&cmd, 0)) { /* got a problem */
	   return ORTE_ERR_OUT_OF_RESOURCE;
    }

    if (OMPI_SUCCESS != ompi_pack(cmd, (void*)&command, 1, ORTE_NS_OOB_PACK_CMD)) { /* got a problem */
	   return ORTE_ERR_PACK_FAILURE;
    }

    if (OMPI_SUCCESS != ompi_pack(cmd, (void*)&job, 1, ORTE_NS_OOB_PACK_JOBID)) { /* got a problem */
	   return ORTE_ERR_PACK_FAILURE;
    }

    if (OMPI_SUCCESS != ompi_pack(cmd, (void*)&range, 1, ORTE_NS_OOB_PACK_VPID)) { /* got a problem */
	   return ORTE_ERR_PACK_FAILURE;
    }

    if (0 > mca_oob_send_packed(orte_ns_my_replica, cmd, MCA_OOB_TAG_NS, 0)) {
	   return ORTE_ERR_COMM_FAILURE;
    }

    if (0 > mca_oob_recv_packed(orte_ns_my_replica, &answer, &recv_tag)) {
	   return ORTE_ERR_COMM_FAILURE;
    }

    if ((OMPI_SUCCESS != ompi_unpack(answer, &command, 1, ORTE_NS_OOB_PACK_CMD))
	|| (ORTE_NS_RESERVE_RANGE_CMD != command)) {
	   ompi_buffer_free(answer);
	   return ORTE_ERR_UNPACK_FAILURE;
    }

    if (OMPI_SUCCESS != ompi_unpack(answer, starting_vpid, 1, ORTE_NS_OOB_PACK_VPID)) {
	   ompi_buffer_free(answer);
	   return ORTE_ERR_UNPACK_FAILURE;
    }
    
	ompi_buffer_free(answer);
	return ORTE_SUCCESS;
}


int orte_ns_proxy_assign_rml_tag(orte_rml_tag_t *tag,
                                 char *name)
{
    ompi_buffer_t cmd;
    ompi_buffer_t answer;
    orte_ns_cmd_flag_t command;
    int recv_tag;
    orte_ns_proxy_tagitem_t* tagitem;

    OMPI_THREAD_LOCK(&orte_ns_proxy_mutex);

    if (NULL != name) {
        /* first, check to see if name is already on local list
         * if so, return tag
         */
        for (tagitem = (orte_ns_proxy_tagitem_t*)ompi_list_get_first(&orte_ns_proxy_taglist);
             tagitem != (orte_ns_proxy_tagitem_t*)ompi_list_get_end(&orte_ns_proxy_taglist);
             tagitem = (orte_ns_proxy_tagitem_t*)ompi_list_get_next(tagitem)) {
            if (0 == strcmp(name, tagitem->name)) { /* found name on list */
               *tag = tagitem->tag;
               return ORTE_SUCCESS;
            }
        }
    }   
    /* okay, not on local list - so go get one from tag server */
    command = ORTE_NS_ASSIGN_OOB_TAG_CMD;
    recv_tag = MCA_OOB_TAG_NS;

    *tag = ORTE_RML_TAG_MAX;  /* set the default error value */
    
    if (OMPI_SUCCESS != ompi_buffer_init(&cmd, 0)) { /* got a problem */
        return ORTE_ERR_OUT_OF_RESOURCE;
    }

    if (OMPI_SUCCESS != ompi_pack(cmd, (void*)&command, 1, ORTE_NS_OOB_PACK_CMD)) {
        return ORTE_ERR_PACK_FAILURE;
    }

    if (NULL != name) {
        if (0 > ompi_pack_string(cmd, (void*)name)) {
            return ORTE_ERR_PACK_FAILURE;
        }
    } else {
        if (0 > ompi_pack_string(cmd, "NULL")) {
            return ORTE_ERR_PACK_FAILURE;
        }
    }
    
    if (0 > mca_oob_send_packed(orte_ns_my_replica, cmd, MCA_OOB_TAG_NS, 0)) {
        return ORTE_ERR_COMM_FAILURE;
    }

    if (0 > mca_oob_recv_packed(orte_ns_my_replica, &answer, &recv_tag)) {
        return ORTE_ERR_COMM_FAILURE;
    }

    if ((OMPI_SUCCESS != ompi_unpack(answer, &command, 1, ORTE_NS_OOB_PACK_CMD))
        || (ORTE_NS_CREATE_CELLID_CMD != command)) {
            ompi_buffer_free(answer);
            return ORTE_ERR_UNPACK_FAILURE;
    }

    if (OMPI_SUCCESS != ompi_unpack(answer, tag, 1, ORTE_NS_OOB_PACK_OOB_TAG)) {
        ompi_buffer_free(answer);
        return ORTE_ERR_UNPACK_FAILURE;
    }
    
    ompi_buffer_free(answer);
        
    /* add the new tag to the local list so we don't have to get it again */
    tagitem = OBJ_NEW(orte_ns_proxy_tagitem_t);
    if (NULL == tagitem) { /* out of memory */
        *tag = ORTE_RML_TAG_MAX;
        OMPI_THREAD_UNLOCK(&orte_ns_proxy_mutex);
        return ORTE_ERR_OUT_OF_RESOURCE;
    }
    tagitem->tag = *tag;
    if (NULL != name) {
        tagitem->name = strdup(name);
    } else {
        tagitem->name = NULL;
    }
    ompi_list_append(&orte_ns_proxy_taglist, &tagitem->item);
    
    OMPI_THREAD_UNLOCK(&orte_ns_proxy_mutex);
    
    /* all done */
    return ORTE_SUCCESS;
}


