/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 The Trustees of the University of Tennessee.
 *                         All rights reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "ompi_config.h"

#include <errno.h>

#include "include/constants.h"
#include "class/ompi_pointer_array.h"
#include "runtime/runtime.h"
#include "runtime/runtime_types.h"
#include "runtime/runtime_internal.h"
#include "mca/pcm/pcm.h"
#include "mca/pcm/base/base.h"
#include "mca/pcm/base/base_kill_track.h"
#include "mca/pcmclient/pcmclient.h"
#include "mca/pcmclient/base/base.h"

static void ompi_rte_spawn_handle_construct(ompi_object_t *);
static void ompi_rte_spawn_handle_destruct(ompi_object_t *);

static ompi_pointer_array_t avail_handles;
static ompi_mutex_t avail_handles_mutex;

OBJ_CLASS_INSTANCE(ompi_rte_spawn_handle_t, ompi_object_t, 
                   ompi_rte_spawn_handle_construct, ompi_rte_spawn_handle_destruct);


int
ompi_rte_internal_init_spawn(void)
{
    OBJ_CONSTRUCT(&avail_handles, ompi_pointer_array_t);
    OBJ_CONSTRUCT(&avail_handles_mutex, ompi_mutex_t);

    return OMPI_SUCCESS;
}

int
ompi_rte_internal_fini_spawn(void)
{
    int i;
    ompi_rte_spawn_handle_t *ptr;

    OMPI_THREAD_LOCK(&avail_handles_mutex);
    for (i = 0 ; i < ompi_pointer_array_get_size(&avail_handles) ; ++i) {
        ptr = (ompi_rte_spawn_handle_t*) ompi_pointer_array_get_item(&avail_handles, i);
        if (NULL == ptr) continue;

        OBJ_RELEASE(ptr);

        ompi_pointer_array_set_item(&avail_handles, i, NULL);
    }
    OMPI_THREAD_UNLOCK(&avail_handles_mutex);

    OBJ_DESTRUCT(&avail_handles);
    OBJ_DESTRUCT(&avail_handles_mutex);

    return OMPI_SUCCESS;
}


ompi_rte_spawn_handle_t *
ompi_rte_get_spawn_handle(int criteria, bool have_threads)
{
    int i;
    ompi_rte_spawn_handle_t *ptr;
    int ret;

    /* BWB - long term, this has to go.  But for now, here we are */
    if (0 != (OMPI_RTE_SPAWN_MULTI_CELL & criteria)) {
        printf("ompi_rte_get_spawn_handle: warning: multi-cell support "
               "not implemented.  Removing criteria.\n");
        criteria ^= OMPI_RTE_SPAWN_MULTI_CELL;
    }

    OMPI_THREAD_LOCK(&avail_handles_mutex);
    
    /* make sure we don't already have a matching criteria */
    for (i = 0 ; i < ompi_pointer_array_get_size(&avail_handles) ; ++i) {
        ptr = (ompi_rte_spawn_handle_t*) ompi_pointer_array_get_item(&avail_handles, i);
        if (NULL == ptr) continue;

        if (ptr->criteria == criteria) {
            OBJ_RETAIN(ptr);
            goto cleanup;
        }
    }

    /* no matching criteria.  create a new set of pcms and we're good
       to go */
    ptr = OBJ_NEW(ompi_rte_spawn_handle_t);
    if (NULL == ptr) return NULL;

    ret = mca_pcm_base_select(have_threads, criteria,
                              &(ptr->modules), &(ptr->modules_len));
    if (ret != OMPI_SUCCESS) {
        errno = ret;
        return NULL;
    }

    /* remove for multi-cell */
    if (ptr->modules_len != 1) {
        OBJ_RELEASE(ptr);
        return NULL;
    }

    ompi_pointer_array_add(&avail_handles, ptr);
    /* retain for the array */
    OBJ_RETAIN(ptr);

 cleanup:
    OMPI_THREAD_UNLOCK(&avail_handles_mutex);

    return ptr;
}


int
ompi_rte_spawn_procs(ompi_rte_spawn_handle_t *handle,
                     mca_ns_base_jobid_t jobid, 
                     ompi_list_t *schedule_list)
{
    mca_pcm_base_module_t *active;

    if (NULL == handle) return OMPI_ERR_BAD_PARAM;
    if (MCA_NS_BASE_JOBID_MAX == jobid) return OMPI_ERR_BAD_PARAM;
    if (NULL == schedule_list) return OMPI_ERR_BAD_PARAM;

    /* remove for multi-cell */
    assert(1 == handle->modules_len);

    active = handle->modules[0];

    if (NULL == active->pcm_spawn_procs) {
        return OMPI_ERR_NOT_IMPLEMENTED;
    }

    return active->pcm_spawn_procs(active, jobid, schedule_list);
}


int
ompi_rte_kill_proc(ompi_process_name_t *name, int signal, int flags)
{
    if (NULL == name) return OMPI_ERR_BAD_PARAM;

    return mca_pcm_base_kill(MCA_PCM_BASE_KILL_PROC, name, signal, flags);
}


int
ompi_rte_kill_job(mca_ns_base_jobid_t jobid, int signal, int flags)
{
    int ret;
    ompi_process_name_t *job_name;

    if (jobid < 0 || jobid == MCA_NS_BASE_JOBID_MAX) {
        return OMPI_ERR_BAD_PARAM;
    }

    job_name = ompi_name_server.create_process_name(MCA_NS_BASE_CELLID_MAX,
                                                    jobid,
                                                    MCA_NS_BASE_VPID_MAX);
    
    ret = mca_pcm_base_kill(MCA_PCM_BASE_KILL_JOB, job_name, signal, flags);

    ompi_name_server.free_name(job_name);

    return ret;
}


int
ompi_rte_terminate_proc(ompi_process_name_t *name, int flags)
{
    if (NULL == name) return OMPI_ERR_BAD_PARAM;

    return mca_pcm_base_kill(MCA_PCM_BASE_TERM_PROC, name, 0, flags);
}


int
ompi_rte_terminate_job(mca_ns_base_jobid_t jobid, int flags)
{
    int ret;
    ompi_process_name_t *job_name;

    if (jobid < 0 || jobid == MCA_NS_BASE_JOBID_MAX) {
        return OMPI_ERR_BAD_PARAM;
    }

    job_name = ompi_name_server.create_process_name(MCA_NS_BASE_CELLID_MAX,
                                                    jobid,
                                                    MCA_NS_BASE_VPID_MAX);
    
    ret = mca_pcm_base_kill(MCA_PCM_BASE_TERM_JOB, job_name, 0, flags);

    ompi_name_server.free_name(job_name);

    return ret;
}


static void
ompi_rte_spawn_handle_construct(ompi_object_t *obj)
{
    ompi_rte_spawn_handle_t *handle = (ompi_rte_spawn_handle_t*) obj;
    handle->criteria = 0;
    handle->modules = NULL;
    handle->modules_len = 0;
}


static void
ompi_rte_spawn_handle_destruct(ompi_object_t *obj)
{
    ompi_rte_spawn_handle_t *handle = (ompi_rte_spawn_handle_t*) obj;
    size_t i;

    handle->criteria = 0;
    for (i = 0 ; i < handle->modules_len ; ++i) {
        mca_pcm_base_module_t *pcm = handle->modules[i];
        if (NULL == pcm) continue;
        if (NULL == pcm->pcm_finalize) continue;
        pcm->pcm_finalize(pcm);
    }

    if (NULL != handle->modules) free(handle->modules);
    
    handle->modules_len = 0;
}
