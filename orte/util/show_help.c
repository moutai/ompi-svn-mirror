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
 * Copyright (c) 2008      Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */
#include "orte_config.h"
#include "orte/types.h"
#include "orte/constants.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "opal/util/show_help.h"
#include "opal/util/output.h"
#include "opal/dss/dss.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/rml/rml_types.h"
#include "orte/mca/routed/routed.h"
#include "orte/util/name_fns.h"
#include "orte/util/proc_info.h"
#include "orte/runtime/orte_globals.h"

#include "orte/util/show_help.h"

bool orte_help_want_aggregate;

/************************************************************************/

/* Section for systems without RML and/or HNP support (e.g., Cray) --
   just output directly; don't do any fancy RML sending to the HNP. */
#if ORTE_DISABLE_FULL_SUPPORT

int orte_show_help_init(void)
{
    return ORTE_SUCCESS;
}

void orte_show_help_finalize(void)
{
    return;
}

int orte_show_help(const char *filename, const char *topic, 
                   bool want_error_header, ...)
{
    va_list arglist;
    char *output;
    
    va_start(arglist, want_error_header);
    output = opal_show_help_vstring(filename, topic, want_error_header, 
                                    arglist);
    va_end(arglist);
    
    /* If nothing came back, there's nothing to do */
    if (NULL == output) {
        return ORTE_SUCCESS;
    }
    
    opal_output(0, output);
    return ORTE_SUCCESS;
}


#else

/************************************************************************/

/* Section for systems that have full RML/HNP support */

/* List items for holding (filename, topic) tuples */
typedef struct {
    opal_list_item_t super;
    /* The filename */
    char *tli_filename;
    /* The topic */
    char *tli_topic;
    /* List of process names that have displayed this (filename, topic) */
    opal_list_t tli_processes;
    /* Time this message was displayed */
    time_t tli_time_displayed;
    /* Count of processes since last display (i.e., "new" processes
       that have showed this message that have not yet been output) */
    int tli_count_since_last_display;
} tuple_list_item_t;

static void tuple_list_item_constructor(tuple_list_item_t *obj);
static void tuple_list_item_destructor(tuple_list_item_t *obj);
OBJ_CLASS_INSTANCE(tuple_list_item_t, opal_list_item_t,
                   tuple_list_item_constructor,
                   tuple_list_item_destructor);


/* List of (filename, topic) tuples that have already been displayed */
static opal_list_t abd_tuples;

/* How long to wait between displaying duplicate show_help notices */
static struct timeval show_help_interval = { 5, 0 };

/* Timer for displaying duplicate help message notices */
static time_t show_help_time_last_displayed = 0;
static bool show_help_timer_set = false;
static opal_event_t show_help_timer_event;
static bool ready;

static void tuple_list_item_constructor(tuple_list_item_t *obj)
{
    obj->tli_filename = NULL;
    obj->tli_topic = NULL;
    OBJ_CONSTRUCT(&(obj->tli_processes), opal_list_t);
    obj->tli_time_displayed = time(NULL);
    obj->tli_count_since_last_display = 0;
}

static void tuple_list_item_destructor(tuple_list_item_t *obj)
{
    opal_list_item_t *item, *next;

    if (NULL != obj->tli_filename) {
        free(obj->tli_filename);
    }
    if (NULL != obj->tli_topic) {
        free(obj->tli_topic);
    }
    for (item = opal_list_get_first(&(obj->tli_processes)); 
         opal_list_get_end(&(obj->tli_processes)) != item;
         item = next) {
        next = opal_list_get_next(item);
        opal_list_remove_item(&(obj->tli_processes), item);
        OBJ_RELEASE(item);
    }
}

/* dealing with special characters in xml output */
static char* xml_format(unsigned char *input)
{
    int i, j, k, len, outlen;
    char *output, qprint[10];
    char *endtag="</stderr>";
    char *starttag="<stderr>";
    int endtaglen, starttaglen;
    bool endtagged = false;
    
    len = strlen((char*)input);
    /* add some arbitrary size padding */
    output = (char*)malloc((len+1024)*sizeof(char));
    if (NULL == output) {
        ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
        return (char*)input; /* default to no xml formatting */
    }
    memset(output, 0, len+1024);
    outlen = len+1023;
    endtaglen = strlen(endtag);
    starttaglen = strlen(starttag);
    
    /* start at the beginning */
    k=0;
    
    /* start with the tag */
    for (j=0; j < starttaglen && k < outlen; j++) {
        output[k++] = starttag[j];
    }        

    for (i=0; i < len; i++) {
        if ('&' == input[i]) {
            if (k+5 >= outlen) {
                ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
                goto process;
            }
            snprintf(qprint, 10, "&amp;");
            for (j=0; j < (int)strlen(qprint) && k < outlen; j++) {
                output[k++] = qprint[j];
            }
        } else if ('<' == input[i]) {
            if (k+4 >= outlen) {
                ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
                goto process;
            }
            snprintf(qprint, 10, "&lt;");
            for (j=0; j < (int)strlen(qprint) && k < outlen; j++) {
                output[k++] = qprint[j];
            }
        } else if ('>' == input[i]) {
            if (k+4 >= outlen) {
                ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
                goto process;
            }
            snprintf(qprint, 10, "&gt;");
            for (j=0; j < (int)strlen(qprint) && k < outlen; j++) {
                output[k++] = qprint[j];
            }
        } else if (input[i] < 32 || input[i] > 127) {
            /* this is a non-printable character, so escape it too */
            if (k+7 >= outlen) {
                ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
                goto process;
            }
            snprintf(qprint, 10, "&#%03d;", (int)input[i]);
            for (j=0; j < (int)strlen(qprint) && k < outlen; j++) {
                output[k++] = qprint[j];
            }
            /* if this was a \n, then we also need to break the line with the end tag */
            if ('\n' == input[i] && (k+endtaglen+1) < outlen) {
                /* we need to break the line with the end tag */
                for (j=0; j < endtaglen && k < outlen-1; j++) {
                    output[k++] = endtag[j];
                }
                /* move the <cr> over */
                output[k++] = '\n';
                /* if this isn't the end of the input buffer, add a new start tag */
                if (i < len-1 && (k+starttaglen) < outlen) {
                    for (j=0; j < starttaglen && k < outlen; j++) {
                        output[k++] = starttag[j];
                        endtagged = false;
                    }
                } else {
                    endtagged = true;
                }
            }
        } else {
            output[k++] = input[i];
        }    
    }
    
process:
    if (!endtagged) {
        /* need to add an endtag */
        for (j=0; j < endtaglen && k < outlen-1; j++) {
            output[k++] = endtag[j];
        }
        output[k++] = '\n';
    }
    
    return output;
}


/*
 * Check to see if a given (filename, topic) tuple has been displayed
 * already.  Return ORTE_SUCCESS if so, or ORTE_ERR_NOT_FOUND if not.
 *
 * Always return a tuple_list_item_t representing this (filename,
 * topic) entry in the list of "already been displayed tuples" (if it
 * wasn't in the list already, this function will create a new entry
 * in the list and return it).
 *
 * Note that a list is not an overly-efficient mechanism for this kind
 * of data.  The assupmtion is that there will only be a small numebr
 * of (filename, topic) tuples displayed so the storage required will
 * be fairly small, and linear searches will be fast enough.
 */
static int get_tli(const char *filename, const char *topic,
                   tuple_list_item_t **tli)
{
    opal_list_item_t *item;

    /* Search the list for a duplicate. */
    for (item = opal_list_get_first(&abd_tuples); 
         opal_list_get_end(&abd_tuples) != item;
         item = opal_list_get_next(item)) {
        (*tli) = (tuple_list_item_t*) item;
        if (0 == strcmp((*tli)->tli_filename, filename) &&
            0 == strcmp((*tli)->tli_topic, topic)) {
            return ORTE_SUCCESS;
        }
    }

    /* Nope, we didn't find it -- make a new one */
    *tli = OBJ_NEW(tuple_list_item_t);
    if (NULL == *tli) {
        return ORTE_ERR_OUT_OF_RESOURCE;
    }
    (*tli)->tli_filename = strdup(filename);
    (*tli)->tli_topic = strdup(topic);
    opal_list_append(&abd_tuples, &((*tli)->super));
    return ORTE_ERR_NOT_FOUND;
}


static void show_accumulated_duplicates(int fd, short event, void *context)
{
    opal_list_item_t *item;
    time_t now = time(NULL);
    tuple_list_item_t *tli;
    char *tmp, *output;

    /* Loop through all the messages we've displayed and see if any
       processes have sent duplicates that have not yet been displayed
       yet */
    for (item = opal_list_get_first(&abd_tuples); 
         opal_list_get_end(&abd_tuples) != item;
         item = opal_list_get_next(item)) {
        tli = (tuple_list_item_t*) item;
        if (tli->tli_count_since_last_display > 0) {
            static bool first = true;
            if (orte_xml_output) {
                asprintf(&tmp, "%d more process%s sent help message %s / %s",
                         tli->tli_count_since_last_display,
                         (tli->tli_count_since_last_display > 1) ? "es have" : " has",
                         tli->tli_filename, tli->tli_topic);
                output = xml_format((unsigned char*)tmp);
                free(tmp);
                fprintf(stdout, "%s", output);
                free(output);
            } else {
                opal_output(0, "%d more process%s sent help message %s / %s",
                            tli->tli_count_since_last_display,
                            (tli->tli_count_since_last_display > 1) ? "es have" : " has",
                            tli->tli_filename, tli->tli_topic);
            }
            tli->tli_count_since_last_display = 0;

            if (first) {
                if (orte_xml_output) {
                    fprintf(stdout, "<stderr>Set MCA parameter \"orte_base_help_aggregate\" to 0 to see all help / error messages</stderr>\n");
                    fflush(stdout);
                } else {
                    opal_output(0, "Set MCA parameter \"orte_base_help_aggregate\" to 0 to see all help / error messages");
                }
                first = false;
            }
        }
    }

    show_help_time_last_displayed = now;
    show_help_timer_set = false;
}

static int show_help(const char *filename, const char *topic,
                     const char *output, orte_process_name_t *sender)
{
    int rc;
    tuple_list_item_t *tli = NULL;
    orte_namelist_t *pnli;
    time_t now = time(NULL);

    /* If we're aggregating, check for duplicates.  Otherwise, don't
       track duplicates at all and always display the message. */
    if (orte_help_want_aggregate) {
        rc = get_tli(filename, topic, &tli);
    } else {
        rc = ORTE_ERR_NOT_FOUND;
    }

    /* Was it already displayed? */
    if (ORTE_SUCCESS == rc) {
        /* Yes.  But do we want to print anything?  That's complicated.

           We always show the first message of a given (filename,
           topic) tuple as soon as it arrives.  But we don't want to
           show duplicate notices often, because we could get overrun
           with them.  So we want to gather them up and say "We got N
           duplicates" every once in a while.

           And keep in mind that at termination, we'll unconditionally
           show all accumulated duplicate notices.

           A simple scheme is as follows:
           - when the first of a (filename, topic) tuple arrives
             - print the message
             - if a timer is not set, set T=now
           - when a duplicate (filename, topic) tuple arrives
             - if now>(T+5) and timer is not set (due to
               non-pre-emptiveness of our libevent, a timer *could* be
               set!)
               - print all accumulated duplicates
               - reset T=now
             - else if a timer was not set, set the timer for T+5
             - else if a timer was set, do nothing (just wait)
           - set T=now when the timer expires
        */           
        ++tli->tli_count_since_last_display;
        if (now > show_help_time_last_displayed + 5 && !show_help_timer_set) {
            show_accumulated_duplicates(0, 0, NULL);
        } else if (!show_help_timer_set) {
            opal_evtimer_set(&show_help_timer_event,
                             show_accumulated_duplicates, NULL);
            opal_evtimer_add(&show_help_timer_event, &show_help_interval);
            show_help_timer_set = true;
        }
    } 
    /* Not already displayed */
    else if (ORTE_ERR_NOT_FOUND == rc) {
        if (orte_xml_output) {
            char *tmp;
            tmp = xml_format((unsigned char*)output);
            fprintf(stdout, "%s", tmp);
            fflush(stdout);
            free(tmp);
        } else {
            fprintf(stderr, "%s", output);
        }
        if (!show_help_timer_set) {
            show_help_time_last_displayed = now;
        }
    }
    /* Some other error occurred */
    else {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    /* If we're aggregating, add this process name to the list */
    if (orte_help_want_aggregate) {
        pnli = OBJ_NEW(orte_namelist_t);
        if (NULL == pnli) {
            rc = ORTE_ERR_OUT_OF_RESOURCE;
            ORTE_ERROR_LOG(rc);
            return rc;
        }
        pnli->name = *sender;
        opal_list_append(&(tli->tli_processes), &(pnli->item));
    }
    return ORTE_SUCCESS;
}


/* Note that this function is called from ess/hnp, so don't make it
   static */
void orte_show_help_recv(int status, orte_process_name_t* sender,
                         opal_buffer_t *buffer, orte_rml_tag_t tag,
                         void* cbdata)
{
    char *output=NULL;
    char *filename=NULL, *topic=NULL;
    int32_t n;
    int rc;
    
    OPAL_OUTPUT_VERBOSE((5, orte_debug_output,
                         "%s got show_help from %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(sender)));
    
    /* unpack the filename of the show_help text file */
    n = 1;
    if (ORTE_SUCCESS != (rc = opal_dss.unpack(buffer, &filename, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }
    /* unpack the topic tag */
    n = 1;
    if (ORTE_SUCCESS != (rc = opal_dss.unpack(buffer, &topic, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }
    /* unpack the resulting string */
    n = 1;
    if (ORTE_SUCCESS != (rc = opal_dss.unpack(buffer, &output, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        goto cleanup;
    }
    
    /* Send it to show_help */
    rc = show_help(filename, topic, output, sender);
    
cleanup:
    if (NULL != output) {
        free(output);
    }
    if (NULL != filename) {
        free(filename);
    }
    if (NULL != topic) {
        free(topic);
    }
    /* reissue the recv */
    rc = orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORTE_RML_TAG_SHOW_HELP,
                                 ORTE_RML_NON_PERSISTENT, orte_show_help_recv, NULL);
    if (rc != ORTE_SUCCESS && rc != ORTE_ERR_NOT_IMPLEMENTED) {
        ORTE_ERROR_LOG(rc);
    }
}

int orte_show_help_init(void)
{
    OPAL_OUTPUT_VERBOSE((5, orte_debug_output, "orte_show_help init"));

    if (ready) {
        return ORTE_SUCCESS;
    }
    ready = true;

    /* Show help duplicate detection */
    OBJ_CONSTRUCT(&abd_tuples, opal_list_t);
    
    return ORTE_SUCCESS;
}

void orte_show_help_finalize(void)
{
    if (!ready) {
        return;
    }
    ready = false;
    
    /* Shutdown show_help, showing final messages */
    if (ORTE_PROC_IS_HNP) {
        show_accumulated_duplicates(0, 0, NULL);
        OBJ_DESTRUCT(&abd_tuples);
        if (show_help_timer_set) {
            opal_evtimer_del(&show_help_timer_event);
        }
        
        /* cancel the recv */
        orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORTE_RML_TAG_SHOW_HELP);
        return;
    }

}

int orte_show_help(const char *filename, const char *topic, 
                   bool want_error_header, ...)
{
    int rc = ORTE_SUCCESS;
    va_list arglist;
    char *output;
    
    va_start(arglist, want_error_header);
    output = opal_show_help_vstring(filename, topic, want_error_header, 
                                    arglist);
    va_end(arglist);

    /* If nothing came back, there's nothing to do */
    if (NULL == output) {
        return ORTE_SUCCESS;
    }

    if (!ready) {
        /* if we are finalizing, then we have no way to process
         * this through the orte_show_help system - just drop it to
         * stderr; that's at least better than not showing it.
         *
         * If we are not finalizing, then this is probably a show_help
         * stemming from either a cmd-line request to display the usage
         * message, or a show_help related to a user error. In either case,
         * we can't do anything but just print to stderr.
         */
        fprintf(stderr, "%s", output);
        goto CLEANUP;
    }
    
    /* if we are the HNP, or the RML has not yet been setup,
     * or ROUTED has not been setup,
     * or we weren't given an HNP, then all we can do
     * is process this locally
     */
    if (ORTE_PROC_IS_HNP ||
        NULL == orte_rml.send_buffer ||
        NULL == orte_routed.get_route ||
        NULL == orte_process_info.my_hnp_uri) {
        rc = show_help(filename, topic, output, ORTE_PROC_MY_NAME);
    }
    
    /* otherwise, we relay the output message to
     * the HNP for processing
     */
    else {
        opal_buffer_t buf;
        static bool am_inside = false;

        /* JMS Note that we *may* have a recursion situation here where
           the RML could call show_help.  Need to think about this
           properly, but put a safeguard in here for sure for the time
           being. */
        if (am_inside) {
            rc = show_help(filename, topic, output, ORTE_PROC_MY_NAME);
        } else {
            am_inside = true;
        
            /* build the message to the HNP */
            OBJ_CONSTRUCT(&buf, opal_buffer_t);
            /* pack the filename of the show_help text file */
            opal_dss.pack(&buf, &filename, 1, OPAL_STRING);
            /* pack the topic tag */
            opal_dss.pack(&buf, &topic, 1, OPAL_STRING);
            /* pack the resulting string */
            opal_dss.pack(&buf, &output, 1, OPAL_STRING);
            /* send it to the HNP */
            if (0 > (rc = orte_rml.send_buffer(ORTE_PROC_MY_HNP, &buf, ORTE_RML_TAG_SHOW_HELP, 0))) {
                ORTE_ERROR_LOG(rc);
            }
            OBJ_DESTRUCT(&buf);
            am_inside = false;
        }
    }
    
CLEANUP:
    free(output);
    return rc;
}

#endif /* ORTE_DISABLE_FULL_SUPPORT */

