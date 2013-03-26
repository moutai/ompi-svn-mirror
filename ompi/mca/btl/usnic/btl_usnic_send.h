/*
 * Copyright (c) 2012 Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef BTL_USNIC_SEND_H
#define BTL_USNIC_SEND_H

#include <infiniband/verbs.h>

#include "btl_usnic.h"
#include "btl_usnic_frag.h"


void ompi_btl_usnic_send_complete(ompi_btl_usnic_module_t *module,
                                    ompi_btl_usnic_frag_t *frag);

#endif /* BTL_USNIC_SEND_H */
