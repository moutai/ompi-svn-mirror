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
 * The strided get routines copy strided data located on a remote PE to a local strided data object. 
 * The strided get routines retrieve array data available at address source on remote PE (pe).
 * The elements of the source array are separated by a stride sst. Once the data is received,
 * it is stored at the local memory address target, separated by stride tst. The routines return
 * when the data has been copied into the local target array.
 */
#define SHMEM_TYPE_IGET(type_name, type)    \
    void shmem##type_name##_iget(type *target, const type *source, ptrdiff_t tst, ptrdiff_t sst, size_t nelems, int pe) \
    {                                                               \
        int rc;                                                     \
        size_t element_size = 0;                                    \
        size_t i = 0;                                               \
                                                                    \
        RUNTIME_CHECK_INIT();                                       \
        RUNTIME_CHECK_PE(pe);                                       \
        RUNTIME_CHECK_ADDR(source);                                 \
                                                                    \
        element_size = sizeof(type);                                \
        for (i = 0; i < nelems; i++)                                \
        {                                                           \
            rc = MCA_SPML_CALL(get(                                 \
                (void*)(source + i * sst),                          \
                element_size,                                       \
                (void*)(target + i * tst),                          \
                pe));                                               \
        }                                                           \
                                                                    \
        return ;                                                    \
    }


SHMEM_TYPE_IGET (_short, short)
SHMEM_TYPE_IGET (_int, int)
SHMEM_TYPE_IGET (_long, long)
SHMEM_TYPE_IGET (_longlong, long long)
SHMEM_TYPE_IGET (_float, float)
SHMEM_TYPE_IGET (_double, double)
SHMEM_TYPE_IGET (_longdouble, long double)


#define SHMEM_TYPE_IGETMEM(name, element_size)    \
    void shmem##name(void *target, const void *source, ptrdiff_t tst, ptrdiff_t sst, size_t nelems, int pe) \
    {                                                               \
        int rc;                                                     \
        size_t i = 0;                                               \
                                                                    \
        RUNTIME_CHECK_INIT();                                       \
        RUNTIME_CHECK_PE(pe);                                       \
        RUNTIME_CHECK_ADDR(source);                                 \
                                                                    \
        for (i = 0; i < nelems; i++)                                \
        {                                                           \
            rc = MCA_SPML_CALL(get(                                 \
                (void*)((char*)source + i * sst * element_size),    \
                element_size,                                       \
                (void*)((char*)target + i * tst * element_size),    \
                pe));                                               \
        }                                                           \
                                                                    \
        return ;                                                    \
    }


SHMEM_TYPE_IGETMEM (_iget32, 4)
SHMEM_TYPE_IGETMEM (_iget64, 8)
SHMEM_TYPE_IGETMEM (_iget128, 16)
