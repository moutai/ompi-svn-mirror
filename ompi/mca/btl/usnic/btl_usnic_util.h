/*
 * Copyright (c) 2013 Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef BTL_USNIC_UTIL_H
#define BTL_USNIC_UTIL_H

#include "btl_usnic.h"
#include "btl_usnic_module.h"


/*
 * Safely (but abnornmally) exit this process without abort()'ing (and
 * leaving a corefile).
 */
void ompi_btl_usnic_exit(void);

void ompi_btl_usnic_sprintf_mac(char *out, const uint8_t mac[6]);

void ompi_btl_usnic_sprintf_gid_mac(char *out, union ibv_gid *gid);

int ompi_btl_usnic_find_ip(ompi_btl_usnic_module_t *module, uint8_t mac[6]);

void ompi_btl_usnic_gid_to_mac(union ibv_gid *gid, uint8_t mac[6]);

void ompi_btl_usnic_dump_hex(uint8_t *addr, int len);

#endif /* BTL_USNIC_UTIL_H */
