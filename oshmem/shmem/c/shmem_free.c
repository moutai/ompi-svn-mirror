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


void shfree( void* ptr )
{
    int rc;

    RUNTIME_CHECK_INIT();
    RUNTIME_CHECK_ADDR(ptr);

#if OSHMEM_SPEC_COMPAT == 1
    shmem_barrier_all();
#endif

    rc = MCA_MEMHEAP_CALL(free(ptr));
    if(OSHMEM_SUCCESS != rc){
        SHMEM_API_VERBOSE(10, "shfree failure.");
    }
}

