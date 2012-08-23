/*
 * Copyright (c) 2012      Los Alamos National Security, LLC.
 *                         All rights reserved
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

/*
 *
 */

#include "orte_config.h"
#include "orte/constants.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif  /* HAVE_DIRENT_H */
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include "opal/class/opal_list.h"
#include "opal/mca/event/event.h"

#include "orte/util/show_help.h"
#include "opal/util/argv.h"
#include "opal/util/output.h"
#include "opal/util/opal_environ.h"
#include "opal/util/os_dirpath.h"
#include "opal/util/os_path.h"
#include "opal/util/path.h"
#include "opal/util/basename.h"

#include "orte/util/name_fns.h"
#include "orte/util/proc_info.h"
#include "orte/util/session_dir.h"
#include "orte/runtime/orte_globals.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/grpcomm/grpcomm.h"
#include "orte/mca/rml/rml.h"

#include "orte/mca/filem/filem.h"
#include "orte/mca/filem/base/base.h"

#include "filem_raw.h"

static int raw_init(void);
static int raw_finalize(void);
static int raw_put(orte_filem_base_request_t *req);
static int raw_put_nb(orte_filem_base_request_t *req);
static int raw_get(orte_filem_base_request_t *req);
static int raw_get_nb(orte_filem_base_request_t *req);
static int raw_rm(orte_filem_base_request_t *req);
static int raw_rm_nb(orte_filem_base_request_t *req);
static int raw_wait(orte_filem_base_request_t *req);
static int raw_wait_all(opal_list_t *reqs);
static int raw_preposition_files(orte_job_t *jdata,
                                 orte_filem_completion_cbfunc_t cbfunc,
                                 void *cbdata);
static int raw_link_local_files(orte_job_t *jdata);

orte_filem_base_module_t mca_filem_raw_module = {
    raw_init,
    raw_finalize,
    /* we don't use any of the following */
    raw_put,
    raw_put_nb,
    raw_get,
    raw_get_nb,
    raw_rm,
    raw_rm_nb,
    raw_wait,
    raw_wait_all,
    /* now the APIs we *do* use */
    raw_preposition_files,
    raw_link_local_files
};

static opal_list_t outbound_files;
static opal_list_t incoming_files;

static void send_chunk(int fd, short argc, void *cbdata);
static void recv_files(int status, orte_process_name_t* sender,
                       opal_buffer_t* buffer, orte_rml_tag_t tag,
                       void* cbdata);
static void recv_ack(int status, orte_process_name_t* sender,
                     opal_buffer_t* buffer, orte_rml_tag_t tag,
                     void* cbdata);
static void write_handler(int fd, short event, void *cbdata);

static int raw_init(void)
{
    int rc;

    OBJ_CONSTRUCT(&incoming_files, opal_list_t);

    /* start a recv to catch any files sent to me */
    if (ORTE_SUCCESS != (rc = orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                                                      ORTE_RML_TAG_FILEM_BASE,
                                                      ORTE_RML_PERSISTENT,
                                                      recv_files,
                                                      NULL))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    /* if I'm the HNP, start a recv to catch acks sent to me */
    if (ORTE_PROC_IS_HNP) {
        OBJ_CONSTRUCT(&outbound_files, opal_list_t);
        if (ORTE_SUCCESS != (rc = orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                                                          ORTE_RML_TAG_FILEM_BASE_RESP,
                                                          ORTE_RML_PERSISTENT,
                                                          recv_ack,
                                                          NULL))) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }
    }

    return rc;
}

static int raw_finalize(void)
{
    opal_list_item_t *item;

    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORTE_RML_TAG_FILEM_BASE);
    while (NULL != (item = opal_list_remove_first(&incoming_files))) {
        OBJ_RELEASE(item);
    }
    OBJ_DESTRUCT(&incoming_files);

    if (ORTE_PROC_IS_HNP) {
        while (NULL != (item = opal_list_remove_first(&outbound_files))) {
            OBJ_RELEASE(item);
        }
        OBJ_DESTRUCT(&outbound_files);
        orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORTE_RML_TAG_FILEM_BASE_RESP);
    }

    return ORTE_SUCCESS;
}

static int raw_put(orte_filem_base_request_t *req)
{
    return ORTE_SUCCESS;
}

static int raw_put_nb(orte_filem_base_request_t *req)
{
    return ORTE_SUCCESS;
}

static int raw_get(orte_filem_base_request_t *req)
{
    return ORTE_SUCCESS;
}

static int raw_get_nb(orte_filem_base_request_t *req)
{
    return ORTE_SUCCESS;
}

static int raw_rm(orte_filem_base_request_t *req)
{
    return ORTE_SUCCESS;
}

static int raw_rm_nb(orte_filem_base_request_t *req)
{
    return ORTE_SUCCESS;
}

static int raw_wait(orte_filem_base_request_t *req)
{
    return ORTE_SUCCESS;
}

static int raw_wait_all(opal_list_t *reqs)
{
    return ORTE_SUCCESS;
}

static void xfer_complete(int status, orte_filem_raw_xfer_t *xfer)
{
    orte_filem_raw_outbound_t *outbound = xfer->outbound;

    /* transfer the status, if not success */
    if (ORTE_SUCCESS != status) {
        outbound->status = status;
    }

    /* this transfer is complete - remove it from list */
    opal_list_remove_item(&outbound->xfers, &xfer->super);
    OBJ_RELEASE(xfer);

    /* if the list is now empty, then the xfer is complete */
    if (0 == opal_list_get_size(&outbound->xfers)) {
        /* do the callback */
        if (NULL != outbound->cbfunc) {
            outbound->cbfunc(outbound->status, outbound->cbdata);
        }
        /* release the object */
        opal_list_remove_item(&outbound_files, &outbound->super);
        OBJ_RELEASE(outbound);
    }
}

static void recv_ack(int status, orte_process_name_t* sender,
                     opal_buffer_t* buffer, orte_rml_tag_t tag,
                     void* cbdata)
{
    opal_list_item_t *item, *itm;
    orte_filem_raw_outbound_t *outbound;
    orte_filem_raw_xfer_t *xfer;
    char *file;
    int st, n, rc;

    /* unpack the file */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &file, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    /* unpack the status */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &st, &n, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    OPAL_OUTPUT_VERBOSE((1, orte_filem_base_output,
                         "%s filem:raw: recvd ack from %s for file %s status %d",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(sender), file, st));

    /* find the corresponding outbound object */
    for (item = opal_list_get_first(&outbound_files);
         item != opal_list_get_end(&outbound_files);
         item = opal_list_get_next(item)) {
        outbound = (orte_filem_raw_outbound_t*)item;
        for (itm = opal_list_get_first(&outbound->xfers);
             itm != opal_list_get_end(&outbound->xfers);
             itm = opal_list_get_next(itm)) {
            xfer = (orte_filem_raw_xfer_t*)itm;
            if (0 == strcmp(file, xfer->file)) {
                /* if the status isn't success, record it */
                if (0 != st) {
                    xfer->status = st;
                }
                /* track number of respondents */
                xfer->nrecvd++;
                /* if all daemons have responded, then this is complete */
                if (xfer->nrecvd == orte_process_info.num_procs) {
                    OPAL_OUTPUT_VERBOSE((1, orte_filem_base_output,
                                         "%s filem:raw: xfer complete for file %s status %d",
                                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                         file, xfer->status));
                    xfer_complete(xfer->status, xfer);
                }
                free(file);
                return;
            }
        }
    }
}

static int raw_preposition_files(orte_job_t *jdata,
                                 orte_filem_completion_cbfunc_t cbfunc,
                                 void *cbdata)
{
#ifdef __WINDOWS__
    return ORTE_ERR_NOT_SUPPORTED;
#else
    orte_app_context_t *app;
    opal_list_item_t *item;
    orte_filem_base_file_set_t *fs;
    int fd, rc=ORTE_SUCCESS;
    orte_filem_raw_xfer_t *xfer;
    int flags, i;
    char **files=NULL;
    orte_filem_raw_outbound_t *outbound;
    char *cptr;
    opal_list_t fsets;

    /* cycle across the app_contexts looking for files or
     * binaries to be prepositioned
     */
    OBJ_CONSTRUCT(&fsets, opal_list_t);
    for (i=0; i < jdata->apps->size; i++) {
        if (NULL == (app = (orte_app_context_t*)opal_pointer_array_get_item(jdata->apps, i))) {
            continue;
        }
        if (app->preload_binary) {
            /* add the executable to our list */
            fs = OBJ_NEW(orte_filem_base_file_set_t);
            fs->local_target = strdup(app->app);
            fs->target_flag = ORTE_FILEM_TYPE_FILE;
            opal_list_append(&fsets, &fs->super);
            /* if we are preloading the binary, then the app must be in relative
             * syntax or we won't find it - the binary will be positioned in the
             * session dir
             */
            if (opal_path_is_absolute(app->app)) {
                cptr = opal_basename(app->app);
                free(app->app);
                app->app = cptr;
                free(app->argv[0]);
                app->argv[0] = strdup(cptr);
            }
        }
        if (NULL != app->preload_files) {
            files = opal_argv_split(app->preload_files, ',');
            for (i=0; NULL != files[i]; i++) {
                fs = OBJ_NEW(orte_filem_base_file_set_t);
                fs->local_target = strdup(files[i]);
                /* check any suffix for file type */
                if (NULL != (cptr = strchr(files[i], '.'))) {
                    if (0 == strncmp(cptr, ".tar", 4)) {
                        OPAL_OUTPUT_VERBOSE((1, orte_filem_base_output,
                                             "%s filem:raw: marking file %s as TAR",
                                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                             files[i]));
                        fs->target_flag = ORTE_FILEM_TYPE_TAR;
                    } else if (0 == strncmp(cptr, ".bz", 3)) {
                        OPAL_OUTPUT_VERBOSE((1, orte_filem_base_output,
                                             "%s filem:raw: marking file %s as BZIP",
                                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                             files[i]));
                        fs->target_flag = ORTE_FILEM_TYPE_BZIP;
                    } else if (0 == strncmp(cptr, ".gz", 3)) {
                        OPAL_OUTPUT_VERBOSE((1, orte_filem_base_output,
                                             "%s filem:raw: marking file %s as GZIP",
                                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                             files[i]));
                        fs->target_flag = ORTE_FILEM_TYPE_GZIP;
                    } else {
                        fs->target_flag = ORTE_FILEM_TYPE_FILE;
                    }
                } else {
                    fs->target_flag = ORTE_FILEM_TYPE_FILE;
                }
                if (NULL != app->preload_files_dest_dir) {
                    fs->remote_target = opal_os_path(false, app->preload_files_dest_dir, files[i], NULL);
                }
                opal_list_append(&fsets, &fs->super);
            }
            opal_argv_free(files);
        }
    }

    /* track the outbound file sets */
    outbound = OBJ_NEW(orte_filem_raw_outbound_t);
    outbound->cbfunc = cbfunc;
    outbound->cbdata = cbdata;
    opal_list_append(&outbound_files, &outbound->super);

    /* only the HNP should ever call this function - loop thru the
     * fileset and initiate xcast transfer of each file to every
     * daemon
     */
    while (NULL != (item = opal_list_remove_first(&fsets))) {
        fs = (orte_filem_base_file_set_t*)item;
        /* attempt to open the specified file */
        if (0 >= (fd = open(fs->local_target, O_RDONLY))) {
            opal_output(0, "%s CANNOT ACCESS FILE %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), fs->local_target);
            OBJ_RELEASE(item);
            rc = ORTE_ERROR;
            continue;
        }
        /* set the flags to non-blocking */
        if ((flags = fcntl(fd, F_GETFL, 0)) < 0) {
            opal_output(orte_filem_base_output, "[%s:%d]: fcntl(F_GETFL) failed with errno=%d\n", 
                        __FILE__, __LINE__, errno);
        } else {
            flags |= O_NONBLOCK;
            fcntl(fd, F_SETFL, flags);
        }            
        OPAL_OUTPUT_VERBOSE((1, orte_filem_base_output,
                             "%s filem:raw: setting up to position file %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), fs->local_target));
        xfer = OBJ_NEW(orte_filem_raw_xfer_t);
        xfer->file = strdup(fs->local_target);
        if (NULL != fs->remote_target) {
            xfer->target = strdup(fs->remote_target);
        }
        xfer->type = fs->target_flag;
        xfer->outbound = outbound;
        opal_list_append(&outbound->xfers, &xfer->super);
        opal_event_set(orte_event_base, &xfer->ev, fd, OPAL_EV_READ, send_chunk, xfer);
        opal_event_set_priority(&xfer->ev, ORTE_MSG_PRI);
        opal_event_add(&xfer->ev, 0);
        xfer->pending = true;
        OBJ_RELEASE(item);
    }
    OBJ_DESTRUCT(&fsets);

    return rc;
#endif
}

static int create_link(char *my_dir, char *path,
                       char *link_pt)
{
    char *mypath, *fullname;
    struct stat buf;
    int rc = ORTE_SUCCESS;

    /* form the full source path name */
    mypath = opal_os_path(false, my_dir, link_pt, NULL);
    /* form the full target path name */
    fullname = opal_os_path(false, path, link_pt, NULL);
    /* there may have been multiple files placed under the
     * same directory, so check for existence first
     */
    if (0 != stat(fullname, &buf)) {
        OPAL_OUTPUT_VERBOSE((1, orte_filem_base_output,
                             "%s filem:raw: creating symlink to %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), link_pt));
        /* do the symlink */
        if (0 != symlink(mypath, fullname)) {
            opal_output(0, "%s Failed to symlink %s to %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), mypath, fullname);
            rc = ORTE_ERROR;
        }
    }
    free(mypath);
    free(fullname);
    return rc;
}

static int raw_link_local_files(orte_job_t *jdata)
{
#ifdef __WINDOWS__
    return ORTE_ERR_NOT_SUPPORTED;
#else
    char *my_dir, *path=NULL;
    orte_proc_t *proc;
    char *prefix;
    int i, rc;
    orte_filem_raw_incoming_t *inbnd;
    opal_list_item_t *item;

    /* check my session directory for files I have received and
     * symlink them to the proc-level session directory of each
     * local process in the job
     */
    my_dir = opal_dirname(orte_process_info.job_session_dir);
    
    /* setup */
    if (NULL != orte_process_info.tmpdir_base) {
        prefix = strdup(orte_process_info.tmpdir_base);
    } else {
        prefix = NULL;
    }

    for (i=0; i < orte_local_children->size; i++) {
        if (NULL == (proc = (orte_proc_t*)opal_pointer_array_get_item(orte_local_children, i))) {
            continue;
        }
        if (proc->name.jobid != jdata->jobid) {
            continue;
        }

        OPAL_OUTPUT_VERBOSE((1, orte_filem_base_output,
                             "%s filem:raw: creating symlinks for %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             ORTE_NAME_PRINT(&proc->name)));

        /* get the session dir name in absolute form - create it
         * if it doesn't already exist
         */
        rc = orte_session_dir_get_name(&path, &prefix, NULL,
                                       orte_process_info.nodename,
                                       NULL, &proc->name);
        /* create it, if it doesn't already exist */
        if (OPAL_SUCCESS != (rc = opal_os_dirpath_create(path, S_IRWXU))) {
            ORTE_ERROR_LOG(rc);
            /* doesn't exist with correct permissions, and/or we can't
             * create it - either way, we are done
             */
            return rc;
        }

        /* cycle thru the incoming files */
        for (item = opal_list_get_first(&incoming_files);
             item != opal_list_get_end(&incoming_files);
             item = opal_list_get_next(item)) {
            inbnd = (orte_filem_raw_incoming_t*)item;
            OPAL_OUTPUT_VERBOSE((1, orte_filem_base_output,
                                 "%s filem:raw: checking file %s",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), inbnd->file));
            if (NULL != inbnd->link_pts) {
                /* cycle thru the link points and create symlinks to them */
                for (i=0; NULL != inbnd->link_pts[i]; i++) {
                    if (ORTE_SUCCESS != (rc = create_link(my_dir, path, inbnd->link_pts[i]))) {
                        ORTE_ERROR_LOG(rc);
                        free(my_dir);
                        free(path);
                        return rc;
                    }
                }
            }
        }
        free(path);
        if (NULL != prefix) {
            free(prefix);
        }
    }

    free(my_dir);
    return ORTE_SUCCESS;
#endif
}

static void send_chunk(int fd, short argc, void *cbdata)
{
    orte_filem_raw_xfer_t *rev = (orte_filem_raw_xfer_t*)cbdata;
    unsigned char data[ORTE_FILEM_RAW_CHUNK_MAX];
    int32_t numbytes;
    int rc;
    opal_buffer_t chunk;

    /* flag that event has fired */
    rev->pending = false;

    /* read up to the fragment size */
    numbytes = read(fd, data, sizeof(data));

    if (numbytes < 0) {
        /* either we have a connection error or it was a non-blocking read */
        
        /* non-blocking, retry */
        if (EAGAIN == errno || EINTR == errno) {
            opal_event_add(&rev->ev, 0);
            return;
        } 

        OPAL_OUTPUT_VERBOSE((1, orte_filem_base_output,
                             "%s filem:raw:read error on file %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), rev->file));

        /* Un-recoverable error. Allow the code to flow as usual in order to
         * to send the zero bytes message up the stream, and then close the
         * file descriptor and delete the event.
         */
        numbytes = 0;
    }
    
    /* if job termination has been ordered, just ignore the
     * data and delete the read event
     */
    if (orte_job_term_ordered) {
        OBJ_RELEASE(rev);
        return;
    }

    OPAL_OUTPUT_VERBOSE((1, orte_filem_base_output,
                         "%s filem:raw:read handler sending chunk %d of %d bytes for file %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         rev->nchunk, numbytes, rev->file));

    /* package it for transmission */
    OBJ_CONSTRUCT(&chunk, opal_buffer_t);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(&chunk, &rev->file, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        close(fd);
        return;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(&chunk, &rev->nchunk, 1, OPAL_INT32))) {
        ORTE_ERROR_LOG(rc);
        close(fd);
        return;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(&chunk, data, numbytes, OPAL_BYTE))) {
        ORTE_ERROR_LOG(rc);
        close(fd);
        return;
    }
    /* if it is the first chunk, then add file type and target path */
    if (0 == rev->nchunk) {
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&chunk, &rev->type, 1, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            close(fd);
            return;
        }
        if (OPAL_SUCCESS != (rc = opal_dss.pack(&chunk, &rev->target, 1, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            close(fd);
            return;
        }
    }

    /* xcast this chunk to all daemons */
    if (ORTE_SUCCESS != (rc = orte_grpcomm.xcast(ORTE_PROC_MY_NAME->jobid,
                                                 &chunk, ORTE_RML_TAG_FILEM_BASE))) {
        ORTE_ERROR_LOG(rc);
        close(fd);
        return;
    }
    OBJ_DESTRUCT(&chunk);
    rev->nchunk++;

    /* if num_bytes was zero, or we read the last piece of the file, then we
     * need to terminate the event and close the file descriptor
     */
    if (0 == numbytes || numbytes < (int)sizeof(data)) {
        /* if numbytes wasn't zero, then we need to send an "EOF" message
         * to notify everyone that the file is complete
         */
        if (0 < numbytes) {
            OBJ_CONSTRUCT(&chunk, opal_buffer_t);
            if (OPAL_SUCCESS != (rc = opal_dss.pack(&chunk, &rev->file, 1, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                close(fd);
                return;
            }
            numbytes = -1;
            if (OPAL_SUCCESS != (rc = opal_dss.pack(&chunk, &numbytes, 1, OPAL_INT32))) {
                ORTE_ERROR_LOG(rc);
                close(fd);
                return;
            }
            if (ORTE_SUCCESS != (rc = orte_grpcomm.xcast(ORTE_PROC_MY_NAME->jobid,
                                                         &chunk, ORTE_RML_TAG_FILEM_BASE))) {
                ORTE_ERROR_LOG(rc);
                close(fd);
                return;
            }
            OBJ_DESTRUCT(&chunk);
        }
        close(fd);
        return;
    } else {
        /* restart the read event */
        opal_event_add(&rev->ev, 0);
        rev->pending = true;
    }
}

static void send_complete(char *file, int status)
{
    opal_buffer_t *buf;
    int rc;

    buf = OBJ_NEW(opal_buffer_t);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &file, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        return;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &status, 1, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        return;
    }
    if (0 > (rc = orte_rml.send_buffer_nb(ORTE_PROC_MY_HNP, buf,
                                          ORTE_RML_TAG_FILEM_BASE_RESP, 0,
                                          orte_rml_send_callback, NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
    }
}

/* This is a little tricky as the name of the archive doesn't
 * necessarily have anything to do with the paths inside it -
 * so we have to first query the archive to retrieve that info
 */
static int link_archive(orte_filem_raw_incoming_t *inbnd)
{
    FILE *fp;
    char *cmd;
    char path[MAXPATHLEN];

    OPAL_OUTPUT_VERBOSE((1, orte_filem_base_output,
                         "%s filem:raw: identifying links for archive %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         inbnd->fullpath));

    asprintf(&cmd, "tar tf %s", inbnd->fullpath);
    fp = popen(cmd, "r");
    free(cmd);
    if (NULL == fp) {
        ORTE_ERROR_LOG(ORTE_ERR_FILE_OPEN_FAILURE);
        return ORTE_ERR_FILE_OPEN_FAILURE;
    }
    while (fgets(path, sizeof(path), fp) != NULL) {
        OPAL_OUTPUT_VERBOSE((1, orte_filem_base_output,
                             "%s filem:raw: path %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             path));
        /* if it is an absolute path, then do nothing - the user
         * is responsible for knowing where it went
         */
        if (opal_path_is_absolute(path)) {
            continue;
        }
        /* take the first element of the path as our
         * link point
         */
        if (NULL != (cmd = strchr(path, '/'))) {
            *cmd = '\0';
        }
        opal_argv_append_unique_nosize(&inbnd->link_pts, path, false);
    }
    /* close */
    pclose(fp);
    return ORTE_SUCCESS;
}

static void recv_files(int status, orte_process_name_t* sender,
                       opal_buffer_t* buffer, orte_rml_tag_t tag,
                       void* cbdata)
{
    char *file, *jobfam_dir;
    int32_t nchunk, n, nbytes;
    unsigned char data[ORTE_FILEM_RAW_CHUNK_MAX];
    int rc;
    orte_filem_raw_output_t *output;
    orte_filem_raw_incoming_t *ptr, *incoming;
    opal_list_item_t *item;
    int32_t type;
    char *target=NULL;
    char *tmp, *cptr;

    /* unpack the data */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &file, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        send_complete(NULL, rc);
        return;
    }
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &nchunk, &n, OPAL_INT32))) {
        ORTE_ERROR_LOG(rc);
        send_complete(file, rc);
        free(file);
        return;
    }
    /* if the chunk number is < 0, then this is an EOF message */
    if (nchunk < 0) {
        /* just set nbytes to zero so we close the fd */
        nbytes = 0;
    } else {
        nbytes=ORTE_FILEM_RAW_CHUNK_MAX;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, data, &nbytes, OPAL_BYTE))) {
            ORTE_ERROR_LOG(rc);
            send_complete(file, rc);
            free(file);
            return;
        }
    }
    /* if the chunk is 0, then additional info should be present */
    if (0 == nchunk) {
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &type, &n, OPAL_INT32))) {
            ORTE_ERROR_LOG(rc);
            send_complete(file, rc);
            free(file);
            return;
        }
        n=1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &target, &n, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            send_complete(file, rc);
            free(file);
            return;
        }
    }

    OPAL_OUTPUT_VERBOSE((1, orte_filem_base_output,
                         "%s filem:raw: received chunk %d for file %s containing %d bytes",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         nchunk, file, nbytes));

    /* do we already have this file on our list of incoming? */
    incoming = NULL;
    for (item = opal_list_get_first(&incoming_files);
         item != opal_list_get_end(&incoming_files);
         item = opal_list_get_next(item)) {
        ptr = (orte_filem_raw_incoming_t*)item;
        if (0 == strcmp(file, ptr->file)) {
            incoming = ptr;
            break;
        }
    }
    if (NULL == incoming) {
        /* better be first chunk! */
        if (0 != nchunk) {
            opal_output(0, "%s New file %s is missing first chunk",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), file);
            send_complete(file, ORTE_ERR_FILE_WRITE_FAILURE);
            free(file);
            if (NULL != target) {
                free(target);
            }
            return;
        }
        /* nope - add it */
        incoming = OBJ_NEW(orte_filem_raw_incoming_t);
        incoming->file = strdup(file);
        incoming->type = type;
        /* define the full filename to point to the absolute location */
        if (NULL == target) {
            /* if it starts with "./", then we need to remove
             * that prefix
             */
            if (0 == strncmp(file, "./", 2) ||
                0 == strncmp(file, "../", 3)) {
                cptr = strchr(file, '/');
                ++cptr;  /* step over the '/' */
                tmp = strdup(cptr);
            } else {
                tmp = strdup(file);
            }
            /* separate out the top-level directory of the target */
            if (NULL != (cptr = strchr(tmp, '/'))) {
                *cptr = '\0';
            }
            /* save it */
            incoming->top = strdup(tmp);
            free(tmp);
            /* define the full path to where we will put it */
            jobfam_dir = opal_dirname(orte_process_info.job_session_dir);
            incoming->fullpath = opal_os_path(false, jobfam_dir, file, NULL);
            free(jobfam_dir);
        } else if (opal_path_is_absolute(target)) {
            incoming->top = strdup(target);
            incoming->fullpath = strdup(target);
        } else {
            /* if it starts with "./", then we need to remove
             * that prefix
             */
            if (0 == strncmp(target, "./", 2) ||
                0 == strncmp(target, "../", 3)) {
                cptr = strchr(target, '/');
                ++cptr;  /* step over the '/' */
                tmp = strdup(cptr);
            } else {
                tmp = strdup(target);
            }
            /* separate out the top-level directory of the target */
            if (NULL != (cptr = strchr(tmp, '/'))) {
                *cptr = '\0';
            }
            /* save it */
            incoming->top = strdup(tmp);
            free(tmp);
            /* define the full path to where we will put it */
            jobfam_dir = opal_dirname(orte_process_info.job_session_dir);
            incoming->fullpath = opal_os_path(false, jobfam_dir, target, NULL);
            free(jobfam_dir);
        }

        OPAL_OUTPUT_VERBOSE((1, orte_filem_base_output,
                             "%s filem:raw: opening target file %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), incoming->fullpath));
        /* create the path to the target, if not already existing */
        tmp = opal_dirname(incoming->fullpath);
        if (OPAL_SUCCESS != (rc = opal_os_dirpath_create(tmp, S_IRWXU))) {
            ORTE_ERROR_LOG(rc);
            send_complete(file, ORTE_ERR_FILE_WRITE_FAILURE);
            free(file);
            free(tmp);
            OBJ_RELEASE(incoming);
            if (NULL != target) {
                free(target);
            }
            return;
        }
        /* open the file descriptor for writing */
        if (0 > (incoming->fd = open(incoming->fullpath, O_RDWR | O_CREAT, S_IRWXU))) {
            opal_output(0, "%s CANNOT CREATE FILE %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        incoming->fullpath);
            send_complete(file, ORTE_ERR_FILE_WRITE_FAILURE);
            free(file);
            if (NULL != target) {
                free(target);
            }
            return;
        }
        OPAL_OUTPUT_VERBOSE((1, orte_filem_base_output,
                             "%s filem:raw: adding file %s to incoming list",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), file));
        opal_list_append(&incoming_files, &incoming->super);
        opal_event_set(orte_event_base, &incoming->ev, incoming->fd, OPAL_EV_WRITE, write_handler, incoming);
        opal_event_set_priority(&incoming->ev, ORTE_MSG_PRI);
    }
    /* create an output object for this data */
    output = OBJ_NEW(orte_filem_raw_output_t);
    if (0 < nbytes) {
        /* don't copy 0 bytes - we just need to pass
         * the zero bytes so the fd can be closed
         * after it writes everything out
         */
        memcpy(output->data, data, nbytes);
    }
    output->numbytes = nbytes;

    /* add this data to the write list for this fd */
    opal_list_append(&incoming->outputs, &output->super);

    if (!incoming->pending) {
        /* add the event */
        opal_event_add(&incoming->ev, 0);
        incoming->pending = true;
    }

    /* cleanup */
    free(file);
    if (NULL != target) {
        free(target);
    }
}


static void write_handler(int fd, short event, void *cbdata)
{
    orte_filem_raw_incoming_t *sink = (orte_filem_raw_incoming_t*)cbdata;
    opal_list_item_t *item;
    orte_filem_raw_output_t *output;
    int num_written;
    char *dirname, *cmd;
    char homedir[MAXPATHLEN];
    int rc;

    OPAL_OUTPUT_VERBOSE((1, orte_filem_base_output,
                         "%s write:handler writing data to %d",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         sink->fd));

    /* note that the event is off */
    sink->pending = false;

    while (NULL != (item = opal_list_remove_first(&sink->outputs))) {
        output = (orte_filem_raw_output_t*)item;
        if (0 == output->numbytes) {
            /* indicates we are to close this stream */
            OPAL_OUTPUT_VERBOSE((1, orte_filem_base_output,
                                 "%s write:handler zero bytes - reporting complete for file %s",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                 sink->file));
            send_complete(sink->file, ORTE_SUCCESS);
            if (ORTE_FILEM_TYPE_FILE == sink->type) {
                /* if it is an absolute path, then no link is required - the
                 * user is responsible for correctly pointing at it
                 *
                 * if it is a file and not an absolute path,
                 * then just link to the top as this will be the
                 * name we will want in each proc's session dir
                 */
                if (!opal_path_is_absolute(sink->top)) {
                    opal_argv_append_nosize(&sink->link_pts, sink->top);
                }
            } else {
                /* unarchive the file */
                if (ORTE_FILEM_TYPE_TAR == sink->type) {
                    asprintf(&cmd, "tar xf %s", sink->file);
                } else if (ORTE_FILEM_TYPE_BZIP == sink->type) {
                    asprintf(&cmd, "tar xjf %s", sink->file);
                } else if (ORTE_FILEM_TYPE_GZIP == sink->type) {
                    asprintf(&cmd, "tar xzf %s", sink->file);
                }
                getcwd(homedir, sizeof(homedir));
                dirname = opal_dirname(sink->fullpath);
                chdir(dirname);
                OPAL_OUTPUT_VERBOSE((1, orte_filem_base_output,
                                     "%s write:handler unarchiving file %s with cmd: %s",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                     sink->file, cmd));
                system(cmd);
                chdir(homedir);
                free(dirname);
                free(cmd);
                /* setup the link points */
                if (ORTE_SUCCESS != (rc = link_archive(sink))) {
                    ORTE_ERROR_LOG(rc);
                    send_complete(sink->file, ORTE_ERR_FILE_WRITE_FAILURE);
                }
            }
            return;
        }
        num_written = write(sink->fd, output->data, output->numbytes);
        OPAL_OUTPUT_VERBOSE((1, orte_filem_base_output,
                             "%s write:handler wrote %d bytes to file %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             num_written, sink->file));
        if (num_written < 0) {
            if (EAGAIN == errno || EINTR == errno) {
                /* push this item back on the front of the list */
                opal_list_prepend(&sink->outputs, item);
                /* leave the write event running so it will call us again
                 * when the fd is ready.
                 */
                opal_event_add(&sink->ev, 0);
                sink->pending = true;
                return;
            }
            /* otherwise, something bad happened so all we can do is abort
             * this attempt
             */
            OPAL_OUTPUT_VERBOSE((1, orte_filem_base_output,
                                 "%s write:handler error on write for file %s: %s",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                 sink->file, strerror(errno)));
            OBJ_RELEASE(output);
            opal_list_remove_item(&incoming_files, &sink->super);
            send_complete(sink->file, OPAL_ERR_FILE_WRITE_FAILURE);
            OBJ_RELEASE(sink);
            return;
        } else if (num_written < output->numbytes) {
            /* incomplete write - adjust data to avoid duplicate output */
            memmove(output->data, &output->data[num_written], output->numbytes - num_written);
            /* push this item back on the front of the list */
            opal_list_prepend(&sink->outputs, item);
            /* leave the write event running so it will call us again
             * when the fd is ready
             */
            opal_event_add(&sink->ev, 0);
            sink->pending = true;
            return;
        }
        OBJ_RELEASE(output);
    }
}

static void xfer_construct(orte_filem_raw_xfer_t *ptr)
{
    ptr->pending = false;
    ptr->file = NULL;
    ptr->target = NULL;
    ptr->nchunk = 0;
    ptr->status = ORTE_SUCCESS;
    ptr->nrecvd = 0;
}
static void xfer_destruct(orte_filem_raw_xfer_t *ptr)
{
    if (ptr->pending) {
        opal_event_del(&ptr->ev);
    }
    if (NULL != ptr->file) {
        free(ptr->file);
    }
    if (NULL != ptr->target) {
        free(ptr->target);
    }
}
OBJ_CLASS_INSTANCE(orte_filem_raw_xfer_t,
                   opal_list_item_t,
                   xfer_construct, xfer_destruct);

static void out_construct(orte_filem_raw_outbound_t *ptr)
{
    OBJ_CONSTRUCT(&ptr->xfers, opal_list_t);
    ptr->status = ORTE_SUCCESS;
    ptr->cbfunc = NULL;
    ptr->cbdata = NULL;
}
static void out_destruct(orte_filem_raw_outbound_t *ptr)
{
    opal_list_item_t *item;

    while (NULL != (item = opal_list_remove_first(&ptr->xfers))) {
        OBJ_RELEASE(item);
    }
    OBJ_DESTRUCT(&ptr->xfers);
}
OBJ_CLASS_INSTANCE(orte_filem_raw_outbound_t,
                   opal_list_item_t,
                   out_construct, out_destruct);

static void in_construct(orte_filem_raw_incoming_t *ptr)
{
    ptr->pending = false;
    ptr->fd = -1;
    ptr->file = NULL;
    ptr->top = NULL;
    ptr->fullpath = NULL;
    ptr->link_pts = NULL;
    OBJ_CONSTRUCT(&ptr->outputs, opal_list_t);
}
static void in_destruct(orte_filem_raw_incoming_t *ptr)
{
    opal_list_item_t *item;

    if (ptr->pending) {
        opal_event_del(&ptr->ev);
    }
    if (0 <= ptr->fd) {
        close(ptr->fd);
    }
    if (NULL != ptr->file) {
        free(ptr->file);
    }
    if (NULL != ptr->top) {
        free(ptr->top);
    }
    if (NULL != ptr->fullpath) {
        free(ptr->fullpath);
    }
    opal_argv_free(ptr->link_pts);
    while (NULL != (item = opal_list_remove_first(&ptr->outputs))) {
        OBJ_RELEASE(item);
    }
    OBJ_DESTRUCT(&ptr->outputs);
}
OBJ_CLASS_INSTANCE(orte_filem_raw_incoming_t,
                   opal_list_item_t,
                   in_construct, in_destruct);

static void output_construct(orte_filem_raw_output_t *ptr)
{
    ptr->numbytes = 0;
}
OBJ_CLASS_INSTANCE(orte_filem_raw_output_t,
                   opal_list_item_t,
                   output_construct, NULL);
