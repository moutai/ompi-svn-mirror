/*
 * Copyright (c) 2012      Mellanox Technologies, Inc.
 *                         All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */
#include "oshmem_config.h"

#include "oshmem/constants.h"
#include "oshmem/include/shmem.h"

#include "oshmem/runtime/runtime.h"

#include "oshmem/mca/atomic/atomic.h"


/*
 * shmem_swap performs an atomic swap operation. 
 * The atomic swap routines write value to address target on PE pe, and return the previous
 * contents of target. The operation must be completed without the possibility of another
 * process updating target between the time of the fetch and the update.
 */
#define SHMEM_TYPE_SWAP(type_name, type)    \
    type shmem##type_name##_swap(type *target, type value, int pe) \
    {                                                               \
        int rc;                                                     \
        size_t size;                                                \
        type out_value;                                             \
                                                                    \
        RUNTIME_CHECK_INIT();                                       \
        RUNTIME_CHECK_PE(pe);                                       \
        RUNTIME_CHECK_ADDR(target);                                 \
                                                                    \
        size = sizeof(out_value);                                   \
        rc = MCA_ATOMIC_CALL(cswap(                                 \
            (void*)target,                                          \
            (void*)&out_value,                                      \
            NULL,                                                   \
            (const void*)&value,                                    \
            size,                                                   \
            pe));                                                   \
                                                                    \
        return out_value;                                           \
    }


SHMEM_TYPE_SWAP ( , long)
SHMEM_TYPE_SWAP (_int, int)
SHMEM_TYPE_SWAP (_long, long)
SHMEM_TYPE_SWAP (_longlong, long long)
SHMEM_TYPE_SWAP (_float, float)
SHMEM_TYPE_SWAP (_double, double)
