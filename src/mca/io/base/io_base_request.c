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

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "class/ompi_object.h"
#include "file/file.h"
#include "mca/base/base.h"
#include "mca/base/mca_base_param.h"
#include "mca/io/base/base.h"
#include "mca/io/base/io_base_request.h"


/*
 * Public variables
 */
bool mca_io_base_requests_valid = false;
ompi_free_list_t mca_io_base_requests;
volatile int32_t mca_io_base_request_num_pending = 0;


/*
 * Private functions
 */
static void io_base_request_constructor(mca_io_base_request_t *req);


OBJ_CLASS_INSTANCE(mca_io_base_request_t,
                   ompi_request_t,
                   io_base_request_constructor,
                   NULL);


static void io_base_request_constructor(mca_io_base_request_t *req)
{
    req->super.req_type = OMPI_REQUEST_IO;
    req->free_called = false;
}


/*
 * Setup the freelist of IO requests.  This does not need to be
 * protected with a lock because it's called during MPI_INIT.
 */
int mca_io_base_request_create_freelist(void)
{
    ompi_list_item_t *p;
    const mca_base_component_t *component;
    const mca_io_base_component_1_0_0_t *v100;
    size_t size = 0;
    int i, init, incr;

    /* Find the maximum additional number of bytes required by all io
       components for requests and make that the request size */

    for (p = ompi_list_get_first(&mca_io_base_components_available); 
         p != ompi_list_get_end(&mca_io_base_components_available); 
         p = ompi_list_get_next(p)) {
        component = ((mca_base_component_priority_list_item_t *) 
                     p)->super.cli_component;

        /* Only know how to handle v1.0.0 components for now */

        if (component->mca_type_major_version == 1 &&
            component->mca_type_minor_version == 0 &&
            component->mca_type_release_version == 0) {
            v100 = (mca_io_base_component_1_0_0_t *) component;
            if (v100->io_request_bytes > size) {
                size = v100->io_request_bytes;
            }
        }
    }

    /* Construct and initialized the freelist of IO requests. */

    OBJ_CONSTRUCT(&mca_io_base_requests, ompi_free_list_t);
    mca_io_base_requests_valid = true;
    i = mca_base_param_find("io", "base", "freelist_initial_size");
    mca_base_param_lookup_int(i, &init);
    i = mca_base_param_find("io", "base", "freelist_increment");
    mca_base_param_lookup_int(i, &incr);

    ompi_free_list_init(&mca_io_base_requests,
                        sizeof(mca_io_base_request_t) + size,
                        OBJ_CLASS(mca_io_base_request_t),
                        init, -1, incr,
                        NULL);

    /* All done */

    return OMPI_SUCCESS;
}


/*
 * Return a module-specific IO MPI_Request
 */
int mca_io_base_request_alloc(ompi_file_t *file, 
                              mca_io_base_request_t **req)
{
    int err;
    mca_io_base_module_request_once_init_fn_t func;
    ompi_list_item_t *item;

    /* See if we've got a request on the module's freelist (which is
       cached on the file, since there's only one module per
       MPI_File).  Use a quick-but-not-entirely-accurate (but good
       enough) check as a slight optimization to potentially having to
       avoid locking and unlocking. */

    if (ompi_list_get_size(&file->f_io_requests) > 0) {
        OMPI_THREAD_LOCK(&file->f_io_requests_lock);
        if (ompi_list_get_size(&file->f_io_requests) > 0) {
            *req = (mca_io_base_request_t*) 
                ompi_list_remove_first(&file->f_io_requests);
        } else {
            *req = NULL;
        }
        OMPI_THREAD_UNLOCK(&file->f_io_requests_lock);
    } else {
        *req = NULL;
    }
        
    /* Nope, we didn't have one on the file freelist, so let's get one
       off the global freelist */

    if (NULL == *req) {
        OMPI_FREE_LIST_GET(&mca_io_base_requests, item, err);
        *req = (mca_io_base_request_t*) item;

        /* Call the per-use init function, if it exists */

        switch (file->f_io_version) {
        case MCA_IO_BASE_V_1_0_0:

            /* These can be set once for this request since this
               request will always be used with the same module (and
               therefore, the same MPI_File).  Note that
               (*req)->req_ompi.rq_type is already set by the
               constructor. */

            (*req)->req_file = file;
            (*req)->req_ver = file->f_io_version;
            (*req)->free_called = false;
            (*req)->super.req_fini =
                file->f_io_selected_module.v1_0_0.io_module_request_fini;
            (*req)->super.req_free = 
                file->f_io_selected_module.v1_0_0.io_module_request_free;
            (*req)->super.req_cancel =
                file->f_io_selected_module.v1_0_0.io_module_request_cancel;

            /* Call the module's once-per process init, if it
               exists */

            func = 
                file->f_io_selected_module.v1_0_0.io_module_request_once_init;
            if (NULL != func) {
                if (OMPI_SUCCESS != 
                    (err = func(&file->f_io_selected_module, *req))) {
                    OMPI_FREE_LIST_RETURN(&mca_io_base_requests, item);
                    return err;
                }
            }

            break;
            
        default:
            OMPI_FREE_LIST_RETURN(&mca_io_base_requests, item);
            return OMPI_ERR_NOT_IMPLEMENTED;
            break;
        }
    }

    /* Initialize the request */

    OMPI_REQUEST_INIT(&((*req)->super));

    /* All done */

    return OMPI_SUCCESS;
}


/*
 * Free a module-specific IO MPI_Request
 */
void mca_io_base_request_free(ompi_file_t *file,
                              mca_io_base_request_t *req)
{
    /* Put the request back on the per-module freelist, since it's
       been initialized for that module */

    OMPI_THREAD_LOCK(&file->f_io_requests_lock);
    ompi_list_prepend(&file->f_io_requests, (ompi_list_item_t*) req);
    OMPI_THREAD_UNLOCK(&file->f_io_requests_lock);
}


/*
 * Return all the requests in the per-file freelist to the global list
 */
void mca_io_base_request_return(ompi_file_t *file)
{
    ompi_list_item_t *p, *next;

    OMPI_THREAD_LOCK(&file->f_io_requests_lock);
    for (p = ompi_list_get_first(&file->f_io_requests);
         p != ompi_list_get_end(&file->f_io_requests);
         p = next) {
        next = ompi_list_get_next(p);
        OMPI_FREE_LIST_RETURN(&mca_io_base_requests, p);
    }
    OMPI_THREAD_UNLOCK(&file->f_io_requests_lock);
}

#if OMPI_HAVE_THREADS
static volatile bool thread_running = false;
static volatile bool thread_done = false;
static ompi_thread_t progress_thread;
static ompi_mutex_t progress_mutex;
static ompi_condition_t progress_cond;

static void*
request_progress_thread(ompi_object_t *arg)
{
    struct timespec abstime;
    struct timeval tv;

    while (! thread_done) {
        gettimeofday(&tv, NULL);
        abstime.tv_sec = tv.tv_sec + 1;
        abstime.tv_nsec = tv.tv_usec * 1000;
        while (mca_io_base_request_num_pending > 0) {
            /* do some progress, sleep, repeat */
            mca_io_base_component_run_progress();
            sleep(2);
        }
        ompi_condition_timedwait(&progress_cond, &progress_mutex, &abstime);
    }

    return NULL;
}
#endif /* OMPI_HAVE_THREADS */

void
mca_io_base_request_progress_init()
{
    mca_io_base_request_num_pending = 0;

#if OMPI_HAVE_THREADS
    thread_running = false;
    thread_done = false;

    OBJ_CONSTRUCT(&progress_mutex, ompi_mutex_t);
    OBJ_CONSTRUCT(&progress_cond, ompi_condition_t);
    OBJ_CONSTRUCT(&progress_thread, ompi_thread_t);

    progress_thread.t_run = request_progress_thread;
    progress_thread.t_arg = NULL;
#endif /* OMPI_HAVE_THREADS */
}


void
mca_io_base_request_progress_add()
{
#if OMPI_HAVE_THREADS
    /* if we don't have a progress thread, make us have a progress
       thread */
    if (! thread_running) {
        OMPI_THREAD_LOCK(&progress_mutex);
        if (! thread_running) {
            thread_running = true;
            ompi_thread_start(&progress_thread);
        }
        OMPI_THREAD_UNLOCK(&progress_mutex);
    }
#endif /* OMPI_HAVE_THREADS */

    OMPI_THREAD_ADD32(&mca_io_base_request_num_pending, 1);

#if OMPI_HAVE_THREADS
    ompi_condition_signal(&progress_cond);
#endif /* OMPI_HAVE_THREADS */
}


void
mca_io_base_request_progress_del()
{
    OMPI_THREAD_ADD32(&mca_io_base_request_num_pending, -1);
}


void
mca_io_base_request_progress_fini()
{
#if OMPI_HAVE_THREADS
    void *ret;

    /* make the helper thread die */
    thread_done = true;
    if (thread_running) {
        ompi_condition_signal(&progress_cond);
        ompi_thread_join(&progress_thread, &ret);
    }

    /* clean up */
    OBJ_DESTRUCT(&progress_thread);
    OBJ_DESTRUCT(&progress_cond);
    OBJ_DESTRUCT(&progress_mutex);
#endif /* OMPI_HAVE_THREADS */
}
