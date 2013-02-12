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

#include "oshmem/shmem/shmem_api_logger.h"

#include "oshmem/runtime/runtime.h"
#include "oshmem/mca/memheap/memheap.h"


void* shmalloc(size_t size)
{
    int rc;
    void* pBuff = NULL;

    RUNTIME_CHECK_INIT();
    RUNTIME_CHECK_WITH_MEMHEAP_SIZE(size);

    rc = MCA_MEMHEAP_CALL(alloc(size, &pBuff));
    
    if(OSHMEM_SUCCESS != rc){
       SHMEM_API_VERBOSE(10, "Allocation with shmalloc(size=%lu) failed.", (unsigned long)size);
       return NULL;
    }
#if OSHMEM_SPEC_COMPAT == 1
    shmem_barrier_all();
#endif
    return pBuff;
}
