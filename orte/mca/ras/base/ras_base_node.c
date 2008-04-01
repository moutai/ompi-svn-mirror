/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2008 The University of Tennessee and The University
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

#include "orte_config.h"
#include "orte/constants.h"

#include <string.h>

#include "opal/util/output.h"
#include "opal/util/argv.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"

#include "orte/mca/ras/base/ras_private.h"

static void orte_ras_base_proc_construct(orte_ras_proc_t* proc)
{
    proc->node_name = NULL;
    proc->cpu_list = NULL;
    proc->rank = ORTE_VPID_MAX;
}

static void orte_ras_base_proc_destruct(orte_ras_proc_t* proc)
{
    if (NULL != proc->node_name) {
        free(proc->node_name);
    }
    if (NULL != proc->cpu_list) {
        free(proc->cpu_list);
    }
}


OBJ_CLASS_INSTANCE(
    orte_ras_proc_t,
    opal_list_item_t,
    orte_ras_base_proc_construct,
    orte_ras_base_proc_destruct);


/*
 * Add the specified node definitions to the global data store
 * NOTE: this removes all items from the list!
 */
int orte_ras_base_node_insert(opal_list_t* nodes, orte_job_t *jdata)
{
    opal_list_item_t* item;
    orte_std_cntr_t num_nodes;
    int rc;
    orte_node_t *node, *hnp_node;

    /* get the number of nodes */
    num_nodes = (orte_std_cntr_t)opal_list_get_size(nodes);
    if (0 == num_nodes) {
        return ORTE_SUCCESS;  /* nothing to do */
    }
    
    OPAL_OUTPUT_VERBOSE((5, orte_ras_base.ras_output,
                         "%s ras:base:node_insert inserting %ld nodes",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         (long)num_nodes));
    
    /* set the size of the global array - this helps minimize time
     * spent doing realloc's
     */
    if (ORTE_SUCCESS != (rc = opal_pointer_array_set_size(orte_node_pool, num_nodes))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    
    /* get the hnp node's info */
    hnp_node = (orte_node_t*)(orte_node_pool->addr[0]);
    
    /* cycle through the list */
    while (NULL != (item = opal_list_remove_first(nodes))) {
        node = (orte_node_t*)item;
        
        /* if we are not keeping FQDN hostnames, abbreviate
         * the nodename as required
         */
        if (!orte_keep_fqdn_hostnames) {
            char *tmp, *ptr;
            tmp = strdup(node->name);
            if (NULL != (ptr = strchr(tmp, '.'))) {
                *ptr = '\0';
                free(node->name);
                node->name = strdup(tmp);
            }
            free(tmp);
        }
        
        /* the HNP had to already enter its node on the array - that entry is in the
         * first position since it is the first one entered. We need to check to see
         * if this node is the same as the HNP's node so we don't double-enter it
         */
        if (0 == strcmp(node->name, hnp_node->name)) {
            OPAL_OUTPUT_VERBOSE((5, orte_ras_base.ras_output,
                                 "%s ras:base:node_insert updating HNP info to %ld slots",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                 (long)node->slots));
            
            /* adjust the total slots in the job */
            jdata->total_slots_alloc -= hnp_node->slots;
            /* copy the allocation data to that node's info */
            hnp_node->slots = node->slots;
            hnp_node->slots_alloc = node->slots_alloc;
            hnp_node->slots_max = node->slots_max;
            hnp_node->launch_id = node->launch_id;
            /* set the node to available for use */
            hnp_node->allocate = true;
            /* update the total slots in the job */
            jdata->total_slots_alloc += hnp_node->slots;
            /* don't keep duplicate copy */
            OBJ_RELEASE(node);
        } else {
            /* insert the object onto the orte_nodes global array */
            OPAL_OUTPUT_VERBOSE((5, orte_ras_base.ras_output,
                                 "%s ras:base:node_insert node %s",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                 (NULL == node->name) ? "NULL" : node->name));
            /* set node to available for use */
            node->allocate = true;
            node->index = opal_pointer_array_add(orte_node_pool, (void*)node);
            if (ORTE_SUCCESS > (rc = node->index)) {
                ORTE_ERROR_LOG(rc);
                return rc;
            }
            /* update the total slots in the job */
            jdata->total_slots_alloc += node->slots;
        }
    }
    
    return ORTE_SUCCESS;
}
