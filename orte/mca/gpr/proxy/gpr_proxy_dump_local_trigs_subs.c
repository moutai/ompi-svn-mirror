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
/** @file:
 *
 * The Open MPI general purpose registry - implementation.
 *
 */

/*
 * includes
 */

#include "orte_config.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "orte/orte_constants.h"

#include "orte/mca/errmgr/errmgr.h"

#include "opal/util/output.h"

#include "gpr_proxy.h"

int orte_gpr_proxy_dump_local_triggers(void)
{
    orte_gpr_proxy_trigger_t **trigs;
    orte_std_cntr_t j, k;
    
    opal_output(orte_gpr_base_output, "DUMP OF LOCAL TRIGGERS for [%ld,%ld]\n",
            ORTE_NAME_ARGS(orte_process_info.my_name));
    opal_output(orte_gpr_base_output, "Number of triggers: %lu\n", (unsigned long) orte_gpr_proxy_globals.num_trigs);

    trigs = (orte_gpr_proxy_trigger_t**)(orte_gpr_proxy_globals.triggers)->addr;
    for (j=0, k=0; k < orte_gpr_proxy_globals.num_trigs &&
                   j < (orte_gpr_proxy_globals.triggers)->size; j++) {
        if (NULL != trigs[j]) {
            k++;
            opal_output(orte_gpr_base_output, "Data for trigger %lu", (unsigned long) trigs[j]->id);
            if (NULL == trigs[j]->name) {
                opal_output(orte_gpr_base_output, "\tNOT a named trigger");
            } else {
                opal_output(orte_gpr_base_output, "\ttrigger name: %s", trigs[j]->name);
            }
        }
    }
    return ORTE_SUCCESS;    
}

int orte_gpr_proxy_dump_local_subscriptions(void)
{
    orte_gpr_proxy_subscriber_t **subs;
    orte_std_cntr_t j, k;
    
    opal_output(orte_gpr_base_output, "DUMP OF LOCAL SUBSCRIPTIONS for [%ld,%ld]\n",
            ORTE_NAME_ARGS(orte_process_info.my_name));
    opal_output(orte_gpr_base_output, "Number of subscriptions: %lu\n", (unsigned long) orte_gpr_proxy_globals.num_subs);

    subs = (orte_gpr_proxy_subscriber_t**)(orte_gpr_proxy_globals.subscriptions)->addr;
    for (j=0, k=0; k < orte_gpr_proxy_globals.num_subs &&
                   j < (orte_gpr_proxy_globals.subscriptions)->size; j++) {
        if (NULL != subs[j]) {
            k++;
            opal_output(orte_gpr_base_output, "Data for subscription %lu", (unsigned long) subs[j]->id);
            if (NULL == subs[j]->name) {
                opal_output(orte_gpr_base_output, "\tNOT a named subscription");
            } else {
                opal_output(orte_gpr_base_output, "\tsubscription name: %s", subs[j]->name);
            }
        }
    }
    return ORTE_SUCCESS;    
}
