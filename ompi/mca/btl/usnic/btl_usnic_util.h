/*
 * Copyright (c) 2012 Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef BTL_USNIC_UTIL_H
#define BTL_USNIC_UTIL_H

#include "ompi/mca/btl/usnic/btl_usnic.h"


void ompi_btl_usnic_sprintf_mac(char *out, const uint8_t mac[6]);

int ompi_btl_usnic_find_ip(ompi_btl_usnic_module_t *module, uint8_t mac[6]);

void ompi_btl_usnic_gid_to_mac(union ibv_gid *gid, uint8_t mac[6]);

#endif /* BTL_USNIC_UTIL_H */
