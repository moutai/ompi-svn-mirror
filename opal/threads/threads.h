/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef OPAL_THREAD_H
#define OPAL_THREAD_H 1

#include "opal_config.h"

#if OPAL_HAVE_POSIX_THREADS
#include <pthread.h>
#elif OPAL_HAVE_SOLARIS_THREADS
#include <thread.h>
#endif

#include "opal/class/opal_object.h"

BEGIN_C_DECLS

typedef void *(*opal_thread_fn_t) (opal_object_t *);

#ifdef __WINDOWS__
#define OPAL_THREAD_CANCELLED   ((void*)1);
#elif OPAL_HAVE_POSIX_THREADS
#define OPAL_THREAD_CANCELLED   ((void*)1);
#elif OPAL_HAVE_SOLARIS_THREADS
#define OPAL_THREAD_CANCELLED   ((void*)1);
#endif

struct opal_thread_t {
    opal_object_t super;
    opal_thread_fn_t t_run;
    void* t_arg;
#ifdef __WINDOWS__
    HANDLE t_handle;
#elif OPAL_HAVE_POSIX_THREADS
    pthread_t t_handle;
#elif OPAL_HAVE_SOLARIS_THREADS
    thread_t t_handle;
#endif
};

typedef struct opal_thread_t opal_thread_t;

OPAL_DECLSPEC OBJ_CLASS_DECLARATION(opal_thread_t);

OPAL_DECLSPEC int  opal_thread_start(opal_thread_t *);
OPAL_DECLSPEC int  opal_thread_join(opal_thread_t *, void **thread_return);
OPAL_DECLSPEC bool opal_thread_self_compare(opal_thread_t*);
OPAL_DECLSPEC opal_thread_t *opal_thread_get_self(void);

END_C_DECLS

#endif /* OPAL_THREAD_H */
