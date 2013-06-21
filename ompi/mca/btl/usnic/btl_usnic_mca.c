/*
 * Copyright (c) 2004-2008 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2011 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2006      Sandia National Laboratories. All rights
 *                         reserved.
 * Copyright (c) 2008-2013 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2012      Los Alamos National Security, LLC.  All rights
 *                         reserved. 
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ompi_config.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <errno.h>
#include <infiniband/verbs.h>

#include "opal/mca/base/mca_base_param.h"
#include "opal/util/argv.h"

#include "ompi/constants.h"
#include "ompi/mca/btl/btl.h"
#include "ompi/mca/btl/base/base.h"
#include "common_verbs.h"

#include "btl_usnic.h"
#include "btl_usnic_frag.h"
#include "btl_usnic_endpoint.h"
#include "btl_usnic_module.h"


/*
 * Local flags
 */
enum {
    REGINT_NEG_ONE_OK = 0x01,
    REGINT_GE_ZERO = 0x02,
    REGINT_GE_ONE = 0x04,
    REGINT_NONZERO = 0x08,

    REGINT_MAX = 0x88
};


enum {
    REGSTR_EMPTY_OK = 0x01,

    REGSTR_MAX = 0x88
};


/*
 * utility routine for string parameter registration
 */
static int reg_string(const char* param_name,
                      const char* param_desc,
                      const char* default_value, char **out_value,
                      int flags)
{
    int index;
    char *value;
    index = mca_base_param_reg_string(&mca_btl_usnic_component.super.btl_version,
                                      param_name, param_desc, false, false,
                                      default_value, &value);
    mca_base_param_lookup_string(index, &value);

    if (0 == (flags & REGSTR_EMPTY_OK) && 
        (NULL == value || 0 == strlen(value))) {
        opal_output(0, "Bad parameter value for parameter \"%s\"",
                    param_name);
        return OMPI_ERR_BAD_PARAM;
    }
    *out_value = value;
    return OMPI_SUCCESS;
}


/*
 * utility routine for integer parameter registration
 */
static int reg_int(const char* param_name,
                   const char* param_desc,
                   int default_value, int *out_value, int flags)
{
    int index, value;
    index = mca_base_param_reg_int(&mca_btl_usnic_component.super.btl_version,
                                   param_name, param_desc, false, false,
                                   default_value, NULL);
    mca_base_param_lookup_int(index, &value);

    if (0 != (flags & REGINT_NEG_ONE_OK) && -1 == value) {
        *out_value = value;
        return OMPI_SUCCESS;
    }
    if ((0 != (flags & REGINT_GE_ZERO) && value < 0) ||
        (0 != (flags & REGINT_GE_ONE) && value < 1) ||
        (0 != (flags & REGINT_NONZERO) && 0 == value)) {
        opal_output(0, "Bad parameter value for parameter \"%s\"",
                param_name);
        return OMPI_ERR_BAD_PARAM;
    }
    *out_value = value;
    return OMPI_SUCCESS;
}


int ompi_btl_usnic_component_register(void)
{
    int val = 0, tmp, ret = 0;
    char *str, **parts;

#define CHECK(expr) do {\
        tmp = (expr); \
        if (OMPI_SUCCESS != tmp) ret = tmp; \
     } while (0)

    CHECK(reg_int("max_btls",
                  "Maximum number of OpenFabrics ports to use (default: 0 = as many as are available)",
                  0, &val, REGINT_GE_ZERO));
    mca_btl_usnic_component.max_modules = (uint32_t) val;

    CHECK(reg_string("if_include",
                     "Comma-delimited list of devices/ports to be used (e.g. \"mthca0,mthca1:2\"; empty value means to use all ports found).  Mutually exclusive with btl_usnic_if_exclude.",
                     NULL, &mca_btl_usnic_component.if_include, 
                     REGSTR_EMPTY_OK));
    
    CHECK(reg_string("if_exclude",
                     "Comma-delimited list of device/ports to be excluded (empty value means to not exclude any ports).  Mutually exclusive with btl_usnic_if_include.",
                     NULL, &mca_btl_usnic_component.if_exclude,
                     REGSTR_EMPTY_OK));

    /* Cisco Sereno-based VICs are part ID 207 */
    str = NULL;
    CHECK(reg_string("vendor_part_ids",
                     "Comma-delimited list verbs vendor part IDs to search for/use",
                     "207", &str, 0));
    parts = opal_argv_split(str, ',');
    free(str);
    mca_btl_usnic_component.vendor_part_ids = 
        calloc(sizeof(uint32_t), opal_argv_count(parts) + 1);
    if (NULL == mca_btl_usnic_component.vendor_part_ids) {
        return OPAL_ERR_OUT_OF_RESOURCE;
    }
    for (val = 0, str = parts[0]; NULL != str; str = parts[++val]) {
        mca_btl_usnic_component.vendor_part_ids[val] = (uint32_t) atoi(str);
    }
    opal_argv_free(parts);

    CHECK(reg_int("stats",
                  "A non-negative integer specifying the frequency at which each USNIC BTL will output statistics (default: 0 seconds, meaning that statistics are disabled)",
                  0, &mca_btl_usnic_component.stats_frequency, 0));
    mca_btl_usnic_component.stats_enabled = 
        (bool) (mca_btl_usnic_component.stats_frequency > 0);

    CHECK(reg_int("stats_relative",
                  "If stats are enabled, output relative stats between the timestemps (vs. cumulative stats since the beginning of the job) (default: 0 -- i.e., absolute)",
                  0, &val, 0));
    mca_btl_usnic_component.stats_relative = (bool) val;

    CHECK(reg_string("mpool", "Name of the memory pool to be used",
                     "rdma", &mca_btl_usnic_component.usnic_mpool_name, 0));

    CHECK(reg_int("gid_index",
                  "GID index to use on verbs device ports",
                  0, &mca_btl_usnic_component.gid_index, REGINT_GE_ZERO));

    CHECK(reg_int("sd_num", "maximum send descriptors to post (-1 = max supported by device)",
                  -1, &val, REGINT_NEG_ONE_OK));
    mca_btl_usnic_component.sd_num = (int32_t) val;

    CHECK(reg_int("rd_num", "number of pre-posted receive buffers (-1 = max supported by device)",
                  -1, &val, REGINT_NEG_ONE_OK));
    mca_btl_usnic_component.rd_num = (int32_t) val;

    CHECK(reg_int("prio_sd_num", "maximum priority send descriptors to post (-1 = use default)",
                  -1, &val, REGINT_NEG_ONE_OK));
    mca_btl_usnic_component.prio_sd_num = (int32_t) val;

    CHECK(reg_int("prio_rd_num", "number of pre-posted priority receive buffers (-1 = use default)",
                  -1, &val, REGINT_NEG_ONE_OK));
    mca_btl_usnic_component.prio_rd_num = (int32_t) val;

    CHECK(reg_int("cq_num", "number of completion queue entries (-1 = max supported by the device; will error if (sd_num+rd_num)>cq_num)",
                  -1, &val, REGINT_NEG_ONE_OK));
    mca_btl_usnic_component.cq_num = (int32_t) val;

    CHECK(reg_int("retrans_timeout", "number of microseconds before retransmitting a frame",
                  /* JMS: Was 250,000 -- changed to 100,000 */
                  100000, &val, REGINT_GE_ONE));
    mca_btl_usnic_component.retrans_timeout = val;

    CHECK(reg_int("eager_limit", "Eager send limit.  If 0, use the device's default.",
                  USNIC_DFLT_EAGER_LIMIT, &val, REGINT_GE_ZERO));
    ompi_btl_usnic_module_template.super.btl_eager_limit = val;

    CHECK(reg_int("rndv_eager_limit", "Eager rendezvous limit.  If 0, use the device MTU.",
                  0, &val, REGINT_GE_ZERO));
    ompi_btl_usnic_module_template.super.btl_rndv_eager_limit = val;

    CHECK(reg_int("max_send_size", "Max send size.  If 0, use device default.",
                  USNIC_DFLT_MAX_SEND, &val, REGINT_GE_ZERO));
    ompi_btl_usnic_module_template.super.btl_max_send_size = val;

    /* Default to bandwidth auto-detection */
    ompi_btl_usnic_module_template.super.btl_bandwidth = 0;
    ompi_btl_usnic_module_template.super.btl_latency = 4;

    /* Register some synonyms to the ompi common verbs component */
    ompi_common_verbs_mca_register(&mca_btl_usnic_component.super.btl_version);

    return ret;
}
