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

#include "oshmem/op/op.h"
#include "oshmem/mca/atomic/atomic.h"


/*
 * These routines perform an atomic fetch-and-add operation. 
 * The fetch and add routines retrieve the value at address target on PE pe, and update
 * target with the result of adding value to the retrieved value. The operation must be completed
 * without the possibility of another process updating target between the time of the
 * fetch and the update.
 */
#define SHMEM_TYPE_FADD(type_name, type)    \
    type shmem##type_name##_fadd(type *target, type value, int pe) \
    {                                                               \
        int rc;                                                     \
        size_t size;                                                \
        type out_value;                                             \
        oshmem_op_t* op = oshmem_op_sum##type_name;                 \
                                                                    \
        RUNTIME_CHECK_INIT();                                       \
        RUNTIME_CHECK_PE(pe);                                       \
        RUNTIME_CHECK_ADDR(target);                                 \
                                                                    \
        size = sizeof(out_value);                                   \
        rc = MCA_ATOMIC_CALL(fadd(                                  \
            (void*)target,                                          \
            (void*)&out_value,                                      \
            (const void*)&value,                                    \
            size,                                                   \
            pe,                                                     \
            op));                                                   \
                                                                    \
        return out_value;                                           \
    }


SHMEM_TYPE_FADD (_int, int)
SHMEM_TYPE_FADD (_long, long)
SHMEM_TYPE_FADD (_longlong, long long)
