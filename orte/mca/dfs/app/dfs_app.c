/*
 * Copyright (c) 2012      Los Alamos National Security, LLC.
 *                         All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orte_config.h"

#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <sys/stat.h>

#include "opal/util/if.h"
#include "opal/util/output.h"
#include "opal/util/uri.h"
#include "opal/dss/dss.h"

#include "orte/util/error_strings.h"
#include "orte/util/name_fns.h"
#include "orte/util/show_help.h"
#include "orte/runtime/orte_globals.h"
#include "orte/mca/db/db.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"

#include "orte/mca/dfs/base/base.h"
#include "dfs_app.h"

/*
 * Module functions: Global
 */
static int init(void);
static int finalize(void);

static void dfs_open(char *uri,
                     orte_dfs_open_callback_fn_t cbfunc,
                     void *cbdata);
static void dfs_close(int fd,
                      orte_dfs_close_callback_fn_t cbfunc,
                      void *cbdata);
static void dfs_get_file_size(int fd,
                              orte_dfs_size_callback_fn_t cbfunc,
                              void *cbdata);
static void dfs_seek(int fd, long offset, int whence,
                     orte_dfs_seek_callback_fn_t cbfunc,
                     void *cbdata);
static void dfs_read(int fd, uint8_t *buffer,
                     long length,
                     orte_dfs_read_callback_fn_t cbfunc,
                     void *cbdata);

/******************
 * APP module
 ******************/
orte_dfs_base_module_t orte_dfs_app_module = {
    init,
    finalize,
    dfs_open,
    dfs_close,
    dfs_get_file_size,
    dfs_seek,
    dfs_read
};

static opal_list_t requests, active_files;
static int local_fd = 0;
static uint64_t req_id = 0;
static void recv_dfs(int status, orte_process_name_t* sender,
                     opal_buffer_t* buffer, orte_rml_tag_t tag,
                     void* cbdata);

static int init(void)
{
    int rc;

    OBJ_CONSTRUCT(&requests, opal_list_t);
    OBJ_CONSTRUCT(&active_files, opal_list_t);
    if (ORTE_SUCCESS != (rc = orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                                                      ORTE_RML_TAG_DFS_DATA,
                                                      ORTE_RML_PERSISTENT,
                                                      recv_dfs,
                                                      NULL))) {
        ORTE_ERROR_LOG(rc);
    }
    return rc;
}

static int finalize(void)
{
    opal_list_item_t *item;

    orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORTE_RML_TAG_DFS_DATA);
    while (NULL != (item = opal_list_remove_first(&requests))) {
        OBJ_RELEASE(item);
    }
    OBJ_DESTRUCT(&requests);
    while (NULL != (item = opal_list_remove_first(&active_files))) {
        OBJ_RELEASE(item);
    }
    OBJ_DESTRUCT(&active_files);
    return ORTE_SUCCESS;
}

/* receives take place in an event, so we are free to process
 * the request list without fear of getting things out-of-order
 */
static void recv_dfs(int status, orte_process_name_t* sender,
                     opal_buffer_t* buffer, orte_rml_tag_t tag,
                     void* cbdata)
{
    orte_dfs_cmd_t cmd;
    int32_t cnt;
    orte_dfs_request_t *dfs, *dptr;
    opal_list_item_t *item;
    int remote_fd, rc;
    int64_t i64;
    uint64_t rid;
    orte_dfs_tracker_t *trk;

    /* unpack the command this message is responding to */
    cnt = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &cmd, &cnt, ORTE_DFS_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    opal_output_verbose(1, orte_dfs_base.output,
                        "%s recvd cmd %d from sender %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), (int)cmd,
                        ORTE_NAME_PRINT(sender));

    switch (cmd) {
    case ORTE_DFS_OPEN_CMD:
        /* unpack the request id */
        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &rid, &cnt, OPAL_UINT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        /* unpack the remote fd */
        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &remote_fd, &cnt, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        /* search our list of requests to find the matching one */
        dfs = NULL;
        for (item = opal_list_get_first(&requests);
             item != opal_list_get_end(&requests);
             item = opal_list_get_next(item)) {
            dptr = (orte_dfs_request_t*)item;
            if (dptr->id == rid) {
                /* as the request has been fulfilled, remove it */
                opal_list_remove_item(&requests, item);
                dfs = dptr;
                break;
            }
        }
        if (NULL == dfs) {
            opal_output_verbose(1, orte_dfs_base.output,
                                "%s recvd open file - no corresponding request found for local fd %d",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), local_fd);
            ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
            return;
        }

        /* if the remote_fd < 0, then we had an error, so return
         * the error value to the caller
         */
        if (remote_fd < 0) {
            opal_output_verbose(1, orte_dfs_base.output,
                                "%s recvd open file response error file %s [error: %d]",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                dfs->uri, remote_fd);
            if (NULL != dfs->open_cbfunc) {
                dfs->open_cbfunc(remote_fd, dfs->cbdata);
            }
            /* release the request */
            OBJ_RELEASE(dfs);
            return;
        }
        /* otherwise, create a tracker for this file */
        trk = OBJ_NEW(orte_dfs_tracker_t);
        trk->requestor.jobid = ORTE_PROC_MY_NAME->jobid;
        trk->requestor.vpid = ORTE_PROC_MY_NAME->vpid;
        trk->host_daemon.jobid = sender->jobid;
        trk->host_daemon.vpid = sender->vpid;
        trk->filename = strdup(dfs->uri);
        /* define the local fd */
        trk->local_fd = local_fd++;
        /* record the remote file descriptor */
        trk->remote_fd = remote_fd;
        /* add it to our list of active files */
        opal_list_append(&active_files, &trk->super);
        /* return the local_fd to the caller for
         * subsequent operations
         */
        opal_output_verbose(1, orte_dfs_base.output,
                            "%s recvd open file completed for file %s [local fd: %d remote fd: %d]",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                            dfs->uri, trk->local_fd, remote_fd);
        if (NULL != dfs->open_cbfunc) {
            dfs->open_cbfunc(trk->local_fd, dfs->cbdata);
        }
        /* release the request */
        OBJ_RELEASE(dfs);
        break;

    case ORTE_DFS_SIZE_CMD:
        /* unpack the request id for this request */
        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &rid, &cnt, OPAL_UINT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        /* search our list of requests to find the matching one */
        dfs = NULL;
        for (item = opal_list_get_first(&requests);
             item != opal_list_get_end(&requests);
             item = opal_list_get_next(item)) {
            dptr = (orte_dfs_request_t*)item;
            if (dptr->id == rid) {
                /* request was fulfilled, so remove it */
                opal_list_remove_item(&requests, item);
                dfs = dptr;
                break;
            }
        }
        if (NULL == dfs) {
            opal_output_verbose(1, orte_dfs_base.output,
                                "%s recvd size - no corresponding request found for local fd %d",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), local_fd);
            ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
            return;
        }
        /* get the size */
        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &i64, &cnt, OPAL_INT64))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(dfs);
            return;
        }
        /* pass it back to the original caller */
        if (NULL != dfs->size_cbfunc) {
            dfs->size_cbfunc(i64, dfs->cbdata);
        }
        /* release the request */
        OBJ_RELEASE(dfs);
        break;

    case ORTE_DFS_SEEK_CMD:
        /* unpack the request id for this read */
        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &rid, &cnt, OPAL_UINT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        /* search our list of requests to find the matching one */
        dfs = NULL;
        for (item = opal_list_get_first(&requests);
             item != opal_list_get_end(&requests);
             item = opal_list_get_next(item)) {
            dptr = (orte_dfs_request_t*)item;
            if (dptr->id == rid) {
                /* request was fulfilled, so remove it */
                opal_list_remove_item(&requests, item);
                dfs = dptr;
                break;
            }
        }
        if (NULL == dfs) {
            opal_output_verbose(1, orte_dfs_base.output,
                                "%s recvd seek - no corresponding request found for local fd %d",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), local_fd);
            ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
            return;
        }
        /* get the returned offset/status */
        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &i64, &cnt, OPAL_INT64))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(dfs);
            return;
        }
        /* pass it back to the original caller */
        if (NULL != dfs->seek_cbfunc) {
            dfs->seek_cbfunc(i64, dfs->cbdata);
        }
        /* release the request */
        OBJ_RELEASE(dfs);
        break;

    case ORTE_DFS_READ_CMD:
        /* unpack the request id for this read */
        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &rid, &cnt, OPAL_UINT64))) {
            ORTE_ERROR_LOG(rc);
            return;
        }
        /* search our list of requests to find the matching one */
        dfs = NULL;
        for (item = opal_list_get_first(&requests);
             item != opal_list_get_end(&requests);
             item = opal_list_get_next(item)) {
            dptr = (orte_dfs_request_t*)item;
            if (dptr->id == rid) {
                /* request was fulfilled, so remove it */
                opal_list_remove_item(&requests, item);
                dfs = dptr;
                break;
            }
        }
        if (NULL == dfs) {
            opal_output_verbose(1, orte_dfs_base.output,
                                "%s recvd read - no corresponding request found for local fd %d",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), local_fd);
            ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
            return;
        }
        /* get the bytes read */
        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &i64, &cnt, OPAL_INT64))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(dfs);
            return;
        }
        if (0 < i64) {
            cnt = i64;
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, dfs->read_buffer, &cnt, OPAL_UINT8))) {
                ORTE_ERROR_LOG(rc);
                OBJ_RELEASE(dfs);
                return;
            }
        }
        /* pass them back to the original caller */
        if (NULL != dfs->read_cbfunc) {
            dfs->read_cbfunc(i64, dfs->read_buffer, dfs->cbdata);
        }
        /* release the request */
        OBJ_RELEASE(dfs);
        break;

    default:
        opal_output(0, "APP:DFS:RECV WTF");
        break;
    }
}

static void open_local_file(orte_dfs_request_t *dfs)
{
    char *filename;
    orte_dfs_tracker_t *trk;

    /* extract the filename from the uri */
    if (NULL == (filename = opal_filename_from_uri(dfs->uri, NULL))) {
        /* something wrong - error was reported, so just get out */
        if (NULL != dfs->open_cbfunc) {
            dfs->open_cbfunc(-1, dfs->cbdata);
        }
        OBJ_RELEASE(dfs);
        return;
    }
    opal_output_verbose(1, orte_dfs_base.output,
                        "%s opening local file %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        filename);
    /* attempt to open the file */
    if (0 > (dfs->remote_fd = open(filename, O_RDONLY))) {
        ORTE_ERROR_LOG(ORTE_ERR_FILE_OPEN_FAILURE);
        if (NULL != dfs->open_cbfunc) {
            dfs->open_cbfunc(dfs->remote_fd, dfs->cbdata);
        }
        OBJ_RELEASE(dfs);
        return;
    }
    /* otherwise, create a tracker for this file */
    trk = OBJ_NEW(orte_dfs_tracker_t);
    trk->requestor.jobid = ORTE_PROC_MY_NAME->jobid;
    trk->requestor.vpid = ORTE_PROC_MY_NAME->vpid;
    trk->filename = strdup(filename);
    /* define the local fd */
    trk->local_fd = local_fd++;
    /* record the remote file descriptor */
    trk->remote_fd = dfs->remote_fd;
    /* add it to our list of active files */
    opal_list_append(&active_files, &trk->super);
    /* the file is locally hosted */
    trk->host_daemon.jobid = ORTE_PROC_MY_DAEMON->jobid;
    trk->host_daemon.vpid = ORTE_PROC_MY_DAEMON->vpid;
    opal_output_verbose(1, orte_dfs_base.output,
                        "%s local file %s mapped localfd %d to remotefd %d",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        filename, trk->local_fd, trk->remote_fd);
    /* let the caller know */
    if (NULL != dfs->open_cbfunc) {
        dfs->open_cbfunc(trk->local_fd, dfs->cbdata);
    }
    /* request will be released by the calling routing */
}

static void process_opens(int fd, short args, void *cbdata)
{
    orte_dfs_request_t *dfs = (orte_dfs_request_t*)cbdata;
    int rc;
    opal_buffer_t *buffer;
    char *scheme, *host, *filename, *hostname;
    orte_process_name_t daemon;
    bool found;
    orte_vpid_t v;

    /* get the scheme to determine if we can process locally or not */
    if (NULL == (scheme = opal_uri_get_scheme(dfs->uri))) {
        OBJ_RELEASE(dfs);
        return;
    }

    if (0 == strcmp(scheme, "nfs")) {
        open_local_file(dfs);
        goto complete;
    }

    if (0 != strcmp(scheme, "file")) {
        /* not yet supported */
        orte_show_help("orte_dfs_help.txt", "unsupported-filesystem",
                       true, dfs->uri);
        if (NULL != dfs->open_cbfunc) {
            dfs->open_cbfunc(-1, dfs->cbdata);
        }
        goto complete;
    }

    /* dissect the uri to extract host and filename/path */
    if (NULL == (filename = opal_filename_from_uri(dfs->uri, &host))) {
        goto complete;
    }
    /* if the host is our own, then treat it as a local file */
    if (NULL == host ||
        0 == strcmp(host, orte_process_info.nodename) ||
        0 == strcmp(host, "localhost") ||
        opal_ifislocal(host)) {
        opal_output_verbose(1, orte_dfs_base.output,
                            "%s file %s on local host",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                            filename);
        open_local_file(dfs);
        goto complete;
    }

    /* ident the daemon on that host */
    daemon.jobid = ORTE_PROC_MY_DAEMON->jobid;
    found = false;
    for (v=0; v < orte_process_info.num_daemons; v++) {
        daemon.vpid = v;
        /* fetch the hostname where this daemon is located */
        if (ORTE_SUCCESS != (rc = orte_db.fetch_pointer(&daemon, ORTE_DB_HOSTNAME, (void**)&hostname, OPAL_STRING))) {
            ORTE_ERROR_LOG(rc);
            goto complete;
        }
        if (0 == strcmp(host, hostname)) {
            found = true;
            break;
        }
    }
    if (!found) {
        ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
        goto complete;
    }
    opal_output_verbose(1, orte_dfs_base.output,
                        "%s file %s on host %s daemon %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        filename, host, ORTE_NAME_PRINT(&daemon));
    /* double-check: if it is our local daemon, then we
     * treat this as local
     */
    if (daemon.vpid == ORTE_PROC_MY_DAEMON->vpid) {
        opal_output_verbose(1, orte_dfs_base.output,
                            "%s local file %s on same daemon",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                            filename);
        open_local_file(dfs);
        goto complete;
    }
    /* add this request to our local list so we can
     * match it with the returned response when it comes
     */
    dfs->id = req_id++;
    opal_list_append(&requests, &dfs->super);

    /* setup a message for the daemon telling
     * them what file we want to access
     */
    buffer = OBJ_NEW(opal_buffer_t);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &dfs->cmd, 1, ORTE_DFS_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        opal_list_remove_item(&requests, &dfs->super);
        goto complete;
    }
    /* pass the request id */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &dfs->id, 1, OPAL_UINT64))) {
        ORTE_ERROR_LOG(rc);
        opal_list_remove_item(&requests, &dfs->super);
        goto complete;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &filename, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        opal_list_remove_item(&requests, &dfs->super);
        goto complete;
    }
    
    opal_output_verbose(1, orte_dfs_base.output,
                        "%s sending open file request to %s file %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        ORTE_NAME_PRINT(&daemon),
                        filename);
    /* send it */
    if (0 > (rc = orte_rml.send_buffer_nb(&daemon, buffer,
                                          ORTE_RML_TAG_DFS_CMD, 0,
                                          orte_rml_send_callback, NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buffer);
        opal_list_remove_item(&requests, &dfs->super);
        goto complete;
    }
    /* don't release it */
    return;

 complete:
    OBJ_RELEASE(dfs);
}


/* in order to handle the possible opening/reading of files by
 * multiple threads, we have to ensure that all operations are
 * carried out in events - so the "open" cmd simply posts an
 * event containing the required info, and then returns
 */
static void dfs_open(char *uri,
                     orte_dfs_open_callback_fn_t cbfunc,
                     void *cbdata)
{
    orte_dfs_request_t *dfs;

    opal_output_verbose(1, orte_dfs_base.output,
                        "%s opening file %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), uri);

    /* setup the request */
    dfs = OBJ_NEW(orte_dfs_request_t);
    dfs->cmd = ORTE_DFS_OPEN_CMD;
    dfs->uri = strdup(uri);
    dfs->open_cbfunc = cbfunc;
    dfs->cbdata = cbdata;

    /* post it for processing */
    ORTE_DFS_POST_REQUEST(dfs, process_opens);
}

static void process_close(int fd, short args, void *cbdata)
{
    orte_dfs_request_t *close_dfs = (orte_dfs_request_t*)cbdata;
    orte_dfs_tracker_t *tptr, *trk;
    opal_list_item_t *item;
    opal_buffer_t *buffer;
    int rc;

    opal_output_verbose(1, orte_dfs_base.output,
                        "%s closing fd %d",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        close_dfs->local_fd);

    /* look in our local records for this fd */
    trk = NULL;
    for (item = opal_list_get_first(&active_files);
         item != opal_list_get_end(&active_files);
         item = opal_list_get_next(item)) {
        tptr = (orte_dfs_tracker_t*)item;
        if (tptr->local_fd == close_dfs->local_fd) {
            trk = tptr;
            break;
        }
    }
    if (NULL == trk) {
        ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
        if (NULL != close_dfs->close_cbfunc) {
            close_dfs->close_cbfunc(close_dfs->local_fd, close_dfs->cbdata);
        }
        OBJ_RELEASE(close_dfs);
        return;
    }

    /* if the file is local, close it */
    if (trk->host_daemon.vpid == ORTE_PROC_MY_DAEMON->vpid) {
        close(trk->remote_fd);
        goto complete;
    }

    /* setup a message for the daemon telling
     * them what file to close
     */
    buffer = OBJ_NEW(opal_buffer_t);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &close_dfs->cmd, 1, ORTE_DFS_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        goto complete;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &trk->remote_fd, 1, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        goto complete;
    }
    
    opal_output_verbose(1, orte_dfs_base.output,
                        "%s sending close file request to %s for fd %d",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        ORTE_NAME_PRINT(&trk->host_daemon),
                        trk->local_fd);
    /* send it */
    if (0 > (rc = orte_rml.send_buffer_nb(&trk->host_daemon, buffer,
                                          ORTE_RML_TAG_DFS_CMD, 0,
                                          orte_rml_send_callback, NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buffer);
        goto complete;
    }

 complete:
    opal_list_remove_item(&active_files, &trk->super);
    OBJ_RELEASE(trk);
    if (NULL != close_dfs->close_cbfunc) {
        close_dfs->close_cbfunc(close_dfs->local_fd, close_dfs->cbdata);
    }
    OBJ_RELEASE(close_dfs);
}

static void dfs_close(int fd,
                      orte_dfs_close_callback_fn_t cbfunc,
                      void *cbdata)
{
    orte_dfs_request_t *dfs;

    opal_output_verbose(1, orte_dfs_base.output,
                        "%s close called on fd %d",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), fd);

    dfs = OBJ_NEW(orte_dfs_request_t);
    dfs->cmd = ORTE_DFS_CLOSE_CMD;
    dfs->local_fd = fd;
    dfs->close_cbfunc = cbfunc;
    dfs->cbdata = cbdata;

    /* post it for processing */
    ORTE_DFS_POST_REQUEST(dfs, process_close);
}

static void process_sizes(int fd, short args, void *cbdata)
{
    orte_dfs_request_t *size_dfs = (orte_dfs_request_t*)cbdata;
    orte_dfs_tracker_t *tptr, *trk;
    opal_list_item_t *item;
    opal_buffer_t *buffer;
    int rc;
    struct stat buf;

    opal_output_verbose(1, orte_dfs_base.output,
                        "%s processing get_size on fd %d",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        size_dfs->local_fd);

    /* look in our local records for this fd */
    trk = NULL;
    for (item = opal_list_get_first(&active_files);
         item != opal_list_get_end(&active_files);
         item = opal_list_get_next(item)) {
        tptr = (orte_dfs_tracker_t*)item;
        if (tptr->local_fd == size_dfs->local_fd) {
            trk = tptr;
            break;
        }
    }
    if (NULL == trk) {
        ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
        OBJ_RELEASE(size_dfs);
        return;
    }

    /* if the file is local, execute the seek on it - we
     * stuck the "whence" value in the remote_fd
     */
    if (trk->host_daemon.vpid == ORTE_PROC_MY_DAEMON->vpid) {
        /* stat the file and get its size */
        if (0 > stat(trk->filename, &buf)) {
            /* cannot stat file */
            opal_output_verbose(1, orte_dfs_base.output,
                                "%s could not stat %s",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                trk->filename);
            if (NULL != size_dfs->size_cbfunc) {
                size_dfs->size_cbfunc(-1, size_dfs->cbdata);
            }
        } else {
            if (NULL != size_dfs->size_cbfunc) {
                size_dfs->size_cbfunc(buf.st_size, size_dfs->cbdata);
            }
        }
        goto complete;
    }
    /* add this request to our local list so we can
     * match it with the returned response when it comes
     */
    size_dfs->id = req_id++;
    opal_list_append(&requests, &size_dfs->super);

    /* setup a message for the daemon telling
     * them what file we want to access
     */
    buffer = OBJ_NEW(opal_buffer_t);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &size_dfs->cmd, 1, ORTE_DFS_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        opal_list_remove_item(&requests, &size_dfs->super);
        goto complete;
    }
    /* pass the request id */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &size_dfs->id, 1, OPAL_UINT64))) {
        ORTE_ERROR_LOG(rc);
        opal_list_remove_item(&requests, &size_dfs->super);
        goto complete;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &trk->remote_fd, 1, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        opal_list_remove_item(&requests, &size_dfs->super);
        goto complete;
    }

    opal_output_verbose(1, orte_dfs_base.output,
                        "%s sending get_size request to %s for fd %d",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        ORTE_NAME_PRINT(&trk->host_daemon),
                        trk->local_fd);
    /* send it */
    if (0 > (rc = orte_rml.send_buffer_nb(&trk->host_daemon, buffer,
                                          ORTE_RML_TAG_DFS_CMD, 0,
                                          orte_rml_send_callback, NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buffer);
        opal_list_remove_item(&requests, &size_dfs->super);
        if (NULL != size_dfs->size_cbfunc) {
            size_dfs->size_cbfunc(-1, size_dfs->cbdata);
        }
        goto complete;
    }
    /* leave the request there */
    return;

 complete:
    OBJ_RELEASE(size_dfs);
}

static void dfs_get_file_size(int fd,
                              orte_dfs_size_callback_fn_t cbfunc,
                              void *cbdata)
{
    orte_dfs_request_t *dfs;

    opal_output_verbose(1, orte_dfs_base.output,
                        "%s get_size called on fd %d",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), fd);

    dfs = OBJ_NEW(orte_dfs_request_t);
    dfs->cmd = ORTE_DFS_SIZE_CMD;
    dfs->local_fd = fd;
    dfs->size_cbfunc = cbfunc;
    dfs->cbdata = cbdata;

    /* post it for processing */
    ORTE_DFS_POST_REQUEST(dfs, process_sizes);
}


static void process_seeks(int fd, short args, void *cbdata)
{
    orte_dfs_request_t *seek_dfs = (orte_dfs_request_t*)cbdata;
    orte_dfs_tracker_t *tptr, *trk;
    opal_list_item_t *item;
    opal_buffer_t *buffer;
    int64_t i64;
    int rc;
    struct stat buf;

    opal_output_verbose(1, orte_dfs_base.output,
                        "%s processing seek on fd %d",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        seek_dfs->local_fd);

    /* look in our local records for this fd */
    trk = NULL;
    for (item = opal_list_get_first(&active_files);
         item != opal_list_get_end(&active_files);
         item = opal_list_get_next(item)) {
        tptr = (orte_dfs_tracker_t*)item;
        if (tptr->local_fd == seek_dfs->local_fd) {
            trk = tptr;
            break;
        }
    }
    if (NULL == trk) {
        ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
        OBJ_RELEASE(seek_dfs);
        return;
    }

    /* if the file is local, execute the seek on it - we
     * stuck the "whence" value in the remote_fd
     */
    if (trk->host_daemon.vpid == ORTE_PROC_MY_DAEMON->vpid) {
        opal_output_verbose(1, orte_dfs_base.output,
                            "%s local seek on fd %d",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                            seek_dfs->local_fd);
        /* stat the file and get its size */
        if (0 > stat(trk->filename, &buf)) {
            /* cannot stat file */
            opal_output_verbose(1, orte_dfs_base.output,
                                "%s could not stat %s",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                trk->filename);
            if (NULL != seek_dfs->seek_cbfunc) {
                seek_dfs->seek_cbfunc(-1, seek_dfs->cbdata);
            }
        } else if (buf.st_size < seek_dfs->read_length &&
                   SEEK_SET == seek_dfs->remote_fd) {
            /* seek would take us past EOF */
            if (NULL != seek_dfs->seek_cbfunc) {
                seek_dfs->seek_cbfunc(-1, seek_dfs->cbdata);
            }
        } else if (buf.st_size < (off_t)(trk->location + seek_dfs->read_length) &&
                   SEEK_CUR == seek_dfs->remote_fd) {
            /* seek would take us past EOF */
            if (NULL != seek_dfs->seek_cbfunc) {
                seek_dfs->seek_cbfunc(-1, seek_dfs->cbdata);
            }
        } else {
            lseek(trk->remote_fd, seek_dfs->read_length, seek_dfs->remote_fd);
            if (SEEK_SET == seek_dfs->remote_fd) {
                trk->location = seek_dfs->read_length;
            } else {
                trk->location += seek_dfs->read_length;
            }
            if (NULL != seek_dfs->seek_cbfunc) {
                seek_dfs->seek_cbfunc(seek_dfs->read_length, seek_dfs->cbdata);
            }
        }
        goto complete;
    }
    /* add this request to our local list so we can
     * match it with the returned response when it comes
     */
    seek_dfs->id = req_id++;
    opal_list_append(&requests, &seek_dfs->super);

    /* setup a message for the daemon telling
     * them what file to seek
     */
    buffer = OBJ_NEW(opal_buffer_t);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &seek_dfs->cmd, 1, ORTE_DFS_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        goto complete;
    }
    /* pass the request id */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &seek_dfs->id, 1, OPAL_UINT64))) {
        ORTE_ERROR_LOG(rc);
        opal_list_remove_item(&requests, &seek_dfs->super);
        goto complete;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &trk->remote_fd, 1, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        goto complete;
    }
    i64 = (int64_t)seek_dfs->read_length;
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &i64, 1, OPAL_INT64))) {
        ORTE_ERROR_LOG(rc);
        goto complete;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &seek_dfs->remote_fd, 1, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        goto complete;
    }

    opal_output_verbose(1, orte_dfs_base.output,
                        "%s sending seek file request to %s for fd %d",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        ORTE_NAME_PRINT(&trk->host_daemon),
                        trk->local_fd);
    /* send it */
    if (0 > (rc = orte_rml.send_buffer_nb(&trk->host_daemon, buffer,
                                          ORTE_RML_TAG_DFS_CMD, 0,
                                          orte_rml_send_callback, NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buffer);
        goto complete;
    }
    /* leave the request */
    return;

 complete:
    OBJ_RELEASE(seek_dfs);
}


static void dfs_seek(int fd, long offset, int whence,
                     orte_dfs_seek_callback_fn_t cbfunc,
                     void *cbdata)
{
    orte_dfs_request_t *dfs;

    opal_output_verbose(1, orte_dfs_base.output,
                        "%s seek called on fd %d",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), fd);

    dfs = OBJ_NEW(orte_dfs_request_t);
    dfs->cmd = ORTE_DFS_SEEK_CMD;
    dfs->local_fd = fd;
    dfs->read_length = offset;
    dfs->remote_fd = whence;
    dfs->seek_cbfunc = cbfunc;
    dfs->cbdata = cbdata;

    /* post it for processing */
    ORTE_DFS_POST_REQUEST(dfs, process_seeks);
}

static void process_reads(int fd, short args, void *cbdata)
{
    orte_dfs_request_t *read_dfs = (orte_dfs_request_t*)cbdata;
    orte_dfs_tracker_t *tptr, *trk;
    long nbytes;
    opal_list_item_t *item;
    opal_buffer_t *buffer;
    int64_t i64;
    int rc;

    /* look in our local records for this fd */
    trk = NULL;
    for (item = opal_list_get_first(&active_files);
         item != opal_list_get_end(&active_files);
         item = opal_list_get_next(item)) {
        tptr = (orte_dfs_tracker_t*)item;
        if (tptr->local_fd == read_dfs->local_fd) {
            trk = tptr;
            break;
        }
    }
    if (NULL == trk) {
        ORTE_ERROR_LOG(ORTE_ERR_NOT_FOUND);
        OBJ_RELEASE(read_dfs);
        return;
    }

    /* if the file is local, read the desired bytes */
    if (trk->host_daemon.vpid == ORTE_PROC_MY_DAEMON->vpid) {
        nbytes = read(trk->remote_fd, read_dfs->read_buffer, read_dfs->read_length);
        if (0 < nbytes) {
            /* update our location */
            trk->location += nbytes;
        }
        /* pass them back to the caller */
        if (NULL != read_dfs->read_cbfunc) {
            read_dfs->read_cbfunc(nbytes, read_dfs->read_buffer, read_dfs->cbdata);
        }
        /* request is complete */
        OBJ_RELEASE(read_dfs);
        return;
    }
    /* add this request to our pending list */
    read_dfs->id = req_id++;
    opal_list_append(&requests, &read_dfs->super);

    /* setup a message for the daemon telling
     * them what file to read
     */
    buffer = OBJ_NEW(opal_buffer_t);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &read_dfs->cmd, 1, ORTE_DFS_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        goto complete;
    }
    /* include the request id */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &read_dfs->id, 1, OPAL_UINT64))) {
        ORTE_ERROR_LOG(rc);
        goto complete;
    }
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &trk->remote_fd, 1, OPAL_INT))) {
        ORTE_ERROR_LOG(rc);
        goto complete;
    }
    i64 = (int64_t)read_dfs->read_length;
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &i64, 1, OPAL_INT64))) {
        ORTE_ERROR_LOG(rc);
        goto complete;
    }
    
    opal_output_verbose(1, orte_dfs_base.output,
                        "%s sending read file request to %s for fd %d",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        ORTE_NAME_PRINT(&trk->host_daemon),
                        trk->local_fd);
    /* send it */
    if (0 > (rc = orte_rml.send_buffer_nb(&trk->host_daemon, buffer,
                                          ORTE_RML_TAG_DFS_CMD, 0,
                                          orte_rml_send_callback, NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buffer);
    }
    /* don't release the request */
    return;

 complete:
    /* don't need to hang on to this request */
    opal_list_remove_item(&requests, &read_dfs->super);
    OBJ_RELEASE(read_dfs);
}

static void dfs_read(int fd, uint8_t *buffer,
                     long length,
                     orte_dfs_read_callback_fn_t cbfunc,
                     void *cbdata)
{
    orte_dfs_request_t *dfs;

    dfs = OBJ_NEW(orte_dfs_request_t);
    dfs->cmd = ORTE_DFS_READ_CMD;
    dfs->local_fd = fd;
    dfs->read_buffer = buffer;
    dfs->read_length = length;
    dfs->read_cbfunc = cbfunc;
    dfs->cbdata = cbdata;

    /* post it for processing */
    ORTE_DFS_POST_REQUEST(dfs, process_reads);
}
