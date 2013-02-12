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

#include "oshmem/mca/spml/spml.h"


/*
 * These routines copy strided data from the local PE to a strided data object on the destination PE. 
 * The shmem_iput() routines read the elements of a local array (source) and write them to
 * a remote array (target) on the PE indicated by pe. These routines return when the data has
 * been copied out of the source array on the local PE but not necessarily before the data has
 * been delivered to the remote data object.
 */
#define SHMEM_TYPE_IPUT(type_name, type)    \
    void shmem##type_name##_iput(type *target, const type *source, ptrdiff_t tst, ptrdiff_t sst, size_t nelems, int pe) \
    {                                                               \
        int rc;                                                     \
        size_t element_size = 0;                                    \
        size_t i = 0;                                               \
                                                                    \
        RUNTIME_CHECK_INIT();                                       \
        RUNTIME_CHECK_PE(pe);                                       \
        RUNTIME_CHECK_ADDR(target);                                 \
                                                                    \
        element_size = sizeof(type);                                \
        for (i = 0; i < nelems; i++)                                \
        {                                                           \
            rc = MCA_SPML_CALL(put(                                 \
                (void*)(target + i * tst),                          \
                element_size,                                       \
                (void*)(source + i * sst),                          \
                pe));                                               \
        }                                                           \
                                                                    \
        return ;                                                    \
    }


SHMEM_TYPE_IPUT (_short, short)
SHMEM_TYPE_IPUT (_int, int)
SHMEM_TYPE_IPUT (_long, long)
SHMEM_TYPE_IPUT (_longlong, long long)
SHMEM_TYPE_IPUT (_float, float)
SHMEM_TYPE_IPUT (_double, double)
SHMEM_TYPE_IPUT (_longdouble, long double)


#define SHMEM_TYPE_IPUTMEM(name, element_size)    \
    void shmem##name(void *target, const void *source, ptrdiff_t tst, ptrdiff_t sst, size_t nelems, int pe) \
    {                                                               \
        int rc;                                                     \
        size_t i = 0;                                               \
                                                                    \
        RUNTIME_CHECK_INIT();                                       \
        RUNTIME_CHECK_PE(pe);                                       \
        RUNTIME_CHECK_ADDR(target);                                 \
                                                                    \
        for (i = 0; i < nelems; i++)                                \
        {                                                           \
            rc = MCA_SPML_CALL(put(                                 \
                (void*)((char*)target + i * tst * element_size),    \
                element_size,                                       \
                (void*)((char*)source + i * sst * element_size),    \
                pe));                                               \
        }                                                           \
                                                                    \
        return ;                                                    \
    }


SHMEM_TYPE_IPUTMEM (_iput32, 4)
SHMEM_TYPE_IPUTMEM (_iput64, 8)
SHMEM_TYPE_IPUTMEM (_iput128, 16)
