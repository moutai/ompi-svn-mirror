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
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */
#include "orte_config.h"
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include "include/orte_constants.h"
#include "ras_lsf_bproc.h"


static int orte_ras_lsf_bproc_allocate(orte_jobid_t jobid)
{
    return ORTE_SUCCESS;
}

static int orte_ras_lsf_bproc_node_insert(opal_list_t *nodes)
{
    return ORTE_ERROR;
}

static int orte_ras_lsf_bproc_node_query(opal_list_t *nodes)
{
    return ORTE_ERROR;
}

static int orte_ras_lsf_bproc_deallocate(orte_jobid_t jobid)
{
    return ORTE_SUCCESS;
}


static int orte_ras_lsf_bproc_finalize(void)
{
    return ORTE_SUCCESS;
}


orte_ras_base_module_t orte_ras_lsf_bproc_module = {
    orte_ras_lsf_bproc_allocate,
    orte_ras_lsf_bproc_node_insert,
    orte_ras_lsf_bproc_node_query,
    orte_ras_lsf_bproc_deallocate,
    orte_ras_lsf_bproc_finalize
};

