/*
 * Copyright (c) 2009-2011 Cisco Systems, Inc.  All rights reserved. 
 *
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */
/**
 * @file
 *
 * Process Resource Utilization sensor 
 */
#ifndef ORTE_SENSOR_FT_TESTER_H
#define ORTE_SENSOR_FT_TESTER_H

#include "orte_config.h"

#include "orte/mca/sensor/sensor.h"

BEGIN_C_DECLS

struct orte_sensor_ft_tester_component_t {
    orte_sensor_base_component_t super;
    float fail_prob;
    float daemon_fail_prob;
    bool multi_fail;
};
typedef struct orte_sensor_ft_tester_component_t orte_sensor_ft_tester_component_t;

ORTE_MODULE_DECLSPEC extern orte_sensor_ft_tester_component_t mca_sensor_ft_tester_component;
extern orte_sensor_base_module_t orte_sensor_ft_tester_module;


END_C_DECLS

#endif
