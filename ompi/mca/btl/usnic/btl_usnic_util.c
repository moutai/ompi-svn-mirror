/*
 * Copyright (c) 2012 Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ompi_config.h"

#include <stdio.h>
#include <infiniband/verbs.h>

#include "ompi/constants.h"

#include "btl_usnic_util.h"
#include "btl_usnic_if.h"


void ompi_btl_usnic_sprintf_mac(char *out, const uint8_t mac[6])
{
    snprintf(out, 32, "%02x:%02x:%02x:%02x:%02x:%02x", 
             mac[0],
             mac[1],
             mac[2],
             mac[3],
             mac[4],
             mac[5]);
}

