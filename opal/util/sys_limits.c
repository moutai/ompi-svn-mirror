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
 * Copyright (c) 2007      Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 *
 * This file is only here because some platforms have a broken strncpy
 * (e.g., Itanium with RedHat Advanced Server glibc).
 */

#include "opal_config.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <errno.h>
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#include "opal/constants.h"
#include "opal/runtime/opal_params.h"

#include "opal/util/sys_limits.h"
#include "opal/util/output.h"


/*
 * Create and initialize storage for the system limits
 */
OPAL_DECLSPEC opal_sys_limits_t opal_sys_limits = {
    /* initialized = */     false,
    /* num_files   = */     -1,
    /* num_procs   = */     -1,
    /* file_size   = */      0
};

int opal_util_init_sys_limits(void)
{
    struct rlimit rlim, rlim_set;

    /* get/set the system limits on number of files we can have open */
    if (0 <= getrlimit (RLIMIT_NOFILE, &rlim)) {
        if (opal_set_max_sys_limits) {
            rlim_set.rlim_cur = rlim.rlim_max;
            rlim_set.rlim_max = rlim.rlim_max;
            if (0 <= setrlimit (RLIMIT_NOFILE, &rlim_set)) {
                rlim.rlim_cur = rlim.rlim_max;
            }
        }
        opal_sys_limits.num_files = rlim.rlim_cur;
    }

#if HAVE_DECL_RLIMIT_NPROC
    /* get/set the system limits on number of child procs we can have open */
    if (0 <= getrlimit (RLIMIT_NPROC, &rlim)) {
        if (opal_set_max_sys_limits) {
            rlim_set.rlim_cur = rlim.rlim_max;
            rlim_set.rlim_max = rlim.rlim_max;
            if (0 <= setrlimit (RLIMIT_NPROC, &rlim_set)) {
                rlim.rlim_cur = rlim.rlim_max;
            }
        }
        opal_sys_limits.num_procs = rlim.rlim_cur;
    }
#endif
    
    /* get/set the system limits on max file size we can create */
    if (0 <= getrlimit (RLIMIT_FSIZE, &rlim)) {
        if (opal_set_max_sys_limits) {
            rlim_set.rlim_cur = rlim.rlim_max;
            rlim_set.rlim_max = rlim.rlim_max;
            if (0 <= setrlimit (RLIMIT_FSIZE, &rlim_set)) {
                rlim.rlim_cur = rlim.rlim_max;
            }
        }
        opal_sys_limits.file_size = rlim.rlim_cur;
    }
    
    /* indicate we initialized the limits structure */
    opal_sys_limits.initialized = true;

  return OPAL_SUCCESS;
}
