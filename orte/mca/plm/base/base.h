/*
 * Copyright (c) 2004-2006 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
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

#ifndef MCA_PLM_BASE_H
#define MCA_PLM_BASE_H

/*
 * includes
 */
#include "orte_config.h"

#include "opal/mca/mca.h"
#include "opal/class/opal_list.h"

#include "orte/mca/plm/plm.h"


BEGIN_C_DECLS

/**
 * Struct to hold data for public access
 */
typedef struct orte_plm_base_t {
    /** List of opened components */
    opal_list_t available_components;
    /** indicate a component has been selected */
    bool selected;
    /** selected component */
    orte_plm_base_component_t selected_component;
} orte_plm_base_t;

/**
 * Global instance of publicly-accessible PLM framework data
 */
ORTE_DECLSPEC extern orte_plm_base_t orte_plm_base;

/*
 * Global functions for MCA overall collective open and close
 */

/**
 * Open the plm framework
 */
ORTE_DECLSPEC int orte_plm_base_open(void);
/**
 * Select a plm module
 */
ORTE_DECLSPEC int orte_plm_base_select(void);

/**
 * Close the plm framework
 */
ORTE_DECLSPEC int orte_plm_base_finalize(void);
ORTE_DECLSPEC int orte_plm_base_close(void);

END_C_DECLS

#endif
