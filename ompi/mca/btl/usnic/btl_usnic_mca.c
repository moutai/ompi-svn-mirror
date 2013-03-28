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

#include "ompi/constants.h"
#include "ompi/mca/btl/btl.h"
#include "ompi/mca/btl/base/base.h"
#include "common_verbs.h"

#include "btl_usnic.h"
#include "btl_usnic_frag.h"
#include "btl_usnic_endpoint.h"


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

    CHECK(reg_int("rc_devices_ok",
                  "Whether it is ok to use RC-capable OpenFabrics devices or not.  Set to 0 to only allow UD-capable-only devices, 1 to allow UD-and-RC-capable devices (default: 0)",
                  0, &val, 0));
    mca_btl_usnic_component.rc_devices_ok = (bool) val;

    CHECK(reg_int("stats",
                  "Whether you want stats emitted periodically not (default: 0)",
                  0, &val, 0));
    mca_btl_usnic_component.stats_enabled = (bool) val;

    CHECK(reg_int("stats_frequency",
                  "If stats are enabled, the frequency (in seconds) in which stats are output",
                  60, &mca_btl_usnic_component.stats_frequency, 0));

    CHECK(reg_int("stats_relative",
                  "If stats are enabled, output relative stats between the timestemps (vs. cumulative stats since the beginning of the job) (default: 0 -- i.e., absolute)",
                  0, &val, 0));
    mca_btl_usnic_component.stats_relative = (bool) val;

    CHECK(reg_string("mpool", "Name of the memory pool to be used",
                     "rdma", &mca_btl_usnic_component.usnic_mpool_name, 0));

    CHECK(reg_int("ib_pkey_index", "Verbs pkey index",
                  0, &val, REGINT_GE_ZERO));
    mca_btl_usnic_component.verbs_pkey_index = (uint32_t) val;
    CHECK(reg_int("ib_qkey", "Verbs qkey",
                  0x01330133, &val, REGINT_GE_ZERO));
    mca_btl_usnic_component.verbs_qkey = (uint32_t) val;
    CHECK(reg_int("ib_service_level", "Service level",
                  0, &val, REGINT_GE_ZERO));
    mca_btl_usnic_component.verbs_service_level = (uint32_t) val;

    CHECK(reg_int("gid_index",
                  "GID index to use on verbs device ports",
                  0, &mca_btl_usnic_component.gid_index, REGINT_GE_ZERO));

    CHECK(reg_int("sd_num", "maximum send descriptors to post (-1 = max supported by device)",
                  -1, &val, REGINT_NEG_ONE_OK));
    mca_btl_usnic_component.sd_num = (int32_t) val;

    CHECK(reg_int("rd_num", "number of pre-posted receive buffers (-1 = max supported by device)",
                  -1, &val, REGINT_NEG_ONE_OK));
    mca_btl_usnic_component.rd_num = (int32_t) val;

    CHECK(reg_int("cq_num", "number of completion queue entries (-1 = max supported by the device; will error if (sd_num+rd_num)>cq_num)",
                  -1, &val, REGINT_NEG_ONE_OK));
    mca_btl_usnic_component.cq_num = (int32_t) val;
    if (0 == mca_btl_usnic_component.cq_num) {
        mca_btl_usnic_component.cq_num = 
            mca_btl_usnic_component.rd_num + mca_btl_usnic_component.sd_num;
    }

    CHECK(reg_int("ethertype", "Ethertype",
                  0x5000, &val, REGINT_GE_ONE));
    mca_btl_usnic_component.ethertype = (uint32_t) val;

#if RELIABILITY
    CHECK(reg_int("retrans_timeout", "number of miliseconds before retransmitting a frame",
                  /* JMS: Was 250,000 -- changed to 100,000 */
                  100000, &val, REGINT_GE_ONE));
    mca_btl_usnic_component.retrans_timeout = val;
#endif

    CHECK(reg_int("eager_limit", "Eager send limit.  If 0, use the device's current MTU size (minus OMPI protocol overhead).  In the usnic BTL, the eager send limit is also the same as the max send size.",
                  0, &val, REGINT_GE_ZERO));
    ompi_btl_usnic_module_template.super.btl_eager_limit = 
        ompi_btl_usnic_module_template.super.btl_rndv_eager_limit =
        ompi_btl_usnic_module_template.super.btl_max_send_size = val;

    /* Default to bandwidth auto-detection */
    ompi_btl_usnic_module_template.super.btl_bandwidth = 0;
    ompi_btl_usnic_module_template.super.btl_latency = 4;

    /* Register some synonyms to the ompi common verbs component */
    ompi_common_verbs_mca_register(&mca_btl_usnic_component.super.btl_version);

    return ret;
}
