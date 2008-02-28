/* Copyright (c) 2004-2007 The Trustees of Indiana University and Indiana
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
/** @file:
 */

#ifndef ORTE_MCA_RMAPS_TYPES_H
#define ORTE_MCA_RMAPS_TYPES_H

#include "orte_config.h"
#include "orte/constants.h"

#include "orte/class/orte_pointer_array.h"

/*
 * General MAP types - instanced in runtime/orte_globals_class_instances.h
 */

BEGIN_C_DECLS

#define ORTE_RMAPS_NOPOL    0x00
#define ORTE_RMAPS_BYNODE   0x01
#define ORTE_RMAPS_BYSLOT   0x02
#define ORTE_RMAPS_BYUSER   0x04

/*
 * Structure that represents the mapping of a job to an
 * allocated set of resources.
 */
struct orte_job_map_t {
    opal_object_t super;
    /* save the mapping configuration */
    uint8_t policy;
    bool no_use_local;
    bool pernode;
    orte_std_cntr_t npernode;
    bool oversubscribe;
    bool display_map;
    /* *** */
    /* number of new daemons required to be launched
     * to support this job map
     */
    orte_std_cntr_t num_new_daemons;
    /* starting vpid of the new daemons - they will
     * be sequential from that point
     */
    orte_vpid_t daemon_vpid_start;
    /* number of nodes participating in this job */
    orte_std_cntr_t num_nodes;
    /* array of pointers to nodes in this map for this job */
    orte_pointer_array_t *nodes;
};
typedef struct orte_job_map_t orte_job_map_t;
ORTE_DECLSPEC OBJ_CLASS_DECLARATION(orte_job_map_t);

END_C_DECLS

#endif
