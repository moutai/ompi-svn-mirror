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
 * Copyright (c) 2006-2007 Voltaire All rights reserved.
 * Copyright (c) 2007      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2010      Oracle and/or its affiliates.  All rights reserved.
 *
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "ompi_config.h"

#include <stdio.h>

#include "opal/util/output.h"


#include "ompi/mca/btl/btl.h"
#include "ompi/mca/btl/base/base.h"


int mca_btl_base_param_register(mca_base_component_t *version,
                                mca_btl_base_module_t *module)
{
    char *msg;

    /* If this is ever triggered change the uint32_ts in mca_btl_base_module_t to unsigned ints */
    assert(sizeof(unsigned int) == sizeof(uint32_t));

    (void) mca_base_component_var_register(version, "exclusivity",
                                           "BTL exclusivity (must be >= 0)",
                                           MCA_BASE_VAR_TYPE_UNSIGNED_INT, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &module->btl_exclusivity);

    asprintf(&msg, "BTL bit flags (general flags: SEND=%d, PUT=%d, GET=%d, SEND_INPLACE=%d, RDMA_MATCHED=%d, HETEROGENEOUS_RDMA=%d; flags only used by the \"dr\" PML (ignored by others): ACK=%d, CHECKSUM=%d, RDMA_COMPLETION=%d; flags only used by the \"bfo\" PML (ignored by others): FAILOVER_SUPPORT=%d)",
             MCA_BTL_FLAGS_SEND,
             MCA_BTL_FLAGS_PUT,
             MCA_BTL_FLAGS_GET,
             MCA_BTL_FLAGS_SEND_INPLACE,
             MCA_BTL_FLAGS_RDMA_MATCHED,
             MCA_BTL_FLAGS_HETEROGENEOUS_RDMA,
             MCA_BTL_FLAGS_NEED_ACK,
             MCA_BTL_FLAGS_NEED_CSUM,
             MCA_BTL_FLAGS_RDMA_COMPLETION,
             MCA_BTL_FLAGS_FAILOVER_SUPPORT);
    (void) mca_base_component_var_register(version, "flags", msg,
                                           MCA_BASE_VAR_TYPE_UNSIGNED_INT, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &module->btl_flags);
    free(msg);

    (void) mca_base_component_var_register(version, "rndv_eager_limit", "Size (in bytes, including header) of \"phase 1\" fragment sent for all large messages (must be >= 0 and <= eager_limit)",
                                           MCA_BASE_VAR_TYPE_SIZE_T, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &module->btl_rndv_eager_limit);

    (void) mca_base_component_var_register(version, "eager_limit", "Maximum size (in bytes, including header) of \"short\" messages (must be >= 1).",
                                           MCA_BASE_VAR_TYPE_SIZE_T, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &module->btl_eager_limit);

    (void) mca_base_component_var_register(version, "max_send_size", "Maximum size (in bytes) of a single \"phase 2\" fragment of a long message when using the pipeline protocol (must be >= 1)",
                                           MCA_BASE_VAR_TYPE_SIZE_T, NULL, 0, 0,
                                           OPAL_INFO_LVL_9,
                                           MCA_BASE_VAR_SCOPE_READONLY,
                                           &module->btl_max_send_size);

    if (NULL != module->btl_put) {
      (void) mca_base_component_var_register(version, "rdma_pipeline_send_length", "Length of the \"phase 2\" portion of a large message (in bytes) when using the pipeline protocol.  This part of the message will be split into fragments of size max_send_size and sent using send/receive semantics (must be >= 0; only relevant when the PUT flag is set)",
                                             MCA_BASE_VAR_TYPE_SIZE_T, NULL, 0, 0,
                                             OPAL_INFO_LVL_9,
                                             MCA_BASE_VAR_SCOPE_READONLY,
                                             &module->btl_rdma_pipeline_send_length);

      (void) mca_base_component_var_register(version, "rdma_pipeline_frag_size", "Maximum size (in bytes) of a single \"phase 3\" fragment from a long message when using the pipeline protocol.  These fragments will be sent using RDMA semantics (must be >= 1; only relevant when the PUT flag is set)",
                                             MCA_BASE_VAR_TYPE_SIZE_T, NULL, 0, 0,
                                             OPAL_INFO_LVL_9,
                                             MCA_BASE_VAR_SCOPE_READONLY,
                                             &module->btl_rdma_pipeline_frag_size);

      (void) mca_base_component_var_register(version, "min_rdma_pipeline_size", "Messages smaller than this size (in bytes) will not use the RDMA pipeline protocol.  Instead, they will be split into fragments of max_send_size and sent using send/receive semantics (must be >=0, and is automatically adjusted up to at least (eager_limit+btl_rdma_pipeline_send_length); only relevant when the PUT flag is set)",
                                             MCA_BASE_VAR_TYPE_SIZE_T, NULL, 0, 0,
                                             OPAL_INFO_LVL_9,
                                             MCA_BASE_VAR_SCOPE_READONLY,
                                             &module->btl_min_rdma_pipeline_size);

      (void) mca_base_component_var_register(version, "bandwidth", "Approximate maximum bandwidth of interconnect (0 = auto-detect value at run-time [not supported in all BTL modules], >= 1 = bandwidth in Mbps)",
                                             MCA_BASE_VAR_TYPE_UNSIGNED_INT, NULL, 0, 0,
                                             OPAL_INFO_LVL_9,
                                             MCA_BASE_VAR_SCOPE_READONLY,
                                             &module->btl_latency);
    }

    return mca_btl_base_param_verify(module);
}

/* Verify btl parameters make sense */
int mca_btl_base_param_verify(mca_btl_base_module_t *module)
{
    if (module->btl_min_rdma_pipeline_size < 
        (module->btl_eager_limit + module->btl_rdma_pipeline_send_length)) {
        module->btl_min_rdma_pipeline_size = 
            module->btl_eager_limit + module->btl_rdma_pipeline_send_length;
    }

    if (NULL == module->btl_put) {
        module->btl_flags &= ~MCA_BTL_FLAGS_PUT;
    }

    if (NULL == module->btl_get) {
        module->btl_flags &= ~MCA_BTL_FLAGS_GET;
    }

    return OMPI_SUCCESS;
}
