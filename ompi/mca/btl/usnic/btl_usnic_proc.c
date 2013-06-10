/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2011 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2006      Sandia National Laboratories. All rights
 *                         reserved.
 * Copyright (c) 2013 Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ompi_config.h"

#include "opal_stdint.h"
#include "opal/util/arch.h"

#include "orte/mca/errmgr/errmgr.h"

#include "ompi/runtime/ompi_module_exchange.h"
#include "ompi/constants.h"

#include "btl_usnic.h"
#include "btl_usnic_proc.h"
#include "btl_usnic_endpoint.h"
#include "btl_usnic_module.h"


static void proc_construct(ompi_btl_usnic_proc_t* proc)
{
    proc->proc_ompi = 0;
    proc->proc_modex = NULL;
    proc->proc_modex_count = 0;
    proc->proc_modex_claimed = NULL;
    proc->proc_endpoints = 0;
    proc->proc_endpoint_count = 0;
    OBJ_CONSTRUCT(&proc->proc_lock, opal_mutex_t);

    /* add to list of all proc instance */
    opal_list_append(&mca_btl_usnic_component.usnic_procs, &proc->super);
}


static void proc_destruct(ompi_btl_usnic_proc_t* proc)
{
    size_t i;

    /* remove from list of all proc instances */
    opal_list_remove_item(&mca_btl_usnic_component.usnic_procs, &proc->super);

    /* release resources */
    if (NULL != proc->proc_modex) {
        free(proc->proc_modex);
        proc->proc_modex = NULL;
    }

    if (NULL != proc->proc_modex_claimed) {
        free(proc->proc_modex_claimed);
        proc->proc_modex_claimed = NULL;
    }

    if (NULL != proc->proc_endpoints) {
        for (i = 0; i < proc->proc_endpoint_count; ++i) {
            if (NULL != proc->proc_endpoints[i]) {
                OBJ_RELEASE(proc->proc_endpoints[i]);
            }
        }
        free(proc->proc_endpoints);
        proc->proc_endpoints = NULL;
    }

    OBJ_DESTRUCT(&proc->proc_lock);
}


OBJ_CLASS_INSTANCE(ompi_btl_usnic_proc_t,
                   opal_list_item_t, 
                   proc_construct,
                   proc_destruct);

/*
 * Finalize the procs resources
 */
void ompi_btl_usnic_proc_finalize(void)
{
    ompi_btl_usnic_proc_t *usnic_proc, *next;

    for (usnic_proc = (ompi_btl_usnic_proc_t*)
             opal_list_get_first(&mca_btl_usnic_component.usnic_procs);
         usnic_proc != (ompi_btl_usnic_proc_t*)
             opal_list_get_end(&mca_btl_usnic_component.usnic_procs); ) {
        next  = (ompi_btl_usnic_proc_t*) opal_list_get_next(usnic_proc);
        OBJ_RELEASE(usnic_proc);
        usnic_proc = next;
    }
}

/*
 * Look for an existing usnic process instance based on the
 * associated ompi_proc_t instance.
 */
ompi_btl_usnic_proc_t *
ompi_btl_usnic_proc_lookup_ompi(ompi_proc_t* ompi_proc)
{
    ompi_btl_usnic_proc_t* usnic_proc;

    for (usnic_proc = (ompi_btl_usnic_proc_t*)
             opal_list_get_first(&mca_btl_usnic_component.usnic_procs);
         usnic_proc != (ompi_btl_usnic_proc_t*)
             opal_list_get_end(&mca_btl_usnic_component.usnic_procs);
         usnic_proc  = (ompi_btl_usnic_proc_t*)
             opal_list_get_next(usnic_proc)) {
        if (usnic_proc->proc_ompi == ompi_proc) {
            return usnic_proc;
        }
    }

    return NULL;
}


/*
 * Look for an existing usnic proc based on a hashed ORTE process
 * name.
 */
ompi_btl_usnic_endpoint_t *
ompi_btl_usnic_proc_lookup_endpoint(ompi_btl_usnic_module_t *receiver,
                                      uint64_t sender_hashed_orte_name)
{
    size_t i, j;
    uint32_t mask, mynet, peernet;
    ompi_btl_usnic_proc_t *proc;
    ompi_btl_usnic_endpoint_t *endpoint;
    
    for (proc = (ompi_btl_usnic_proc_t*)
             opal_list_get_first(&mca_btl_usnic_component.usnic_procs);
         proc != (ompi_btl_usnic_proc_t*)
             opal_list_get_end(&mca_btl_usnic_component.usnic_procs);
         proc  = (ompi_btl_usnic_proc_t*)
             opal_list_get_next(proc)) {
        if (orte_util_hash_name(&proc->proc_ompi->proc_name) ==
            sender_hashed_orte_name) {
            break;
        }
    }

    /* If we didn't find the sending proc (!), return NULL */
    if (opal_list_get_end(&mca_btl_usnic_component.usnic_procs) == 
        (opal_list_item_t*) proc) {
        return NULL;
    }

    /* Look through all the endpoints on sender's proc and find one
       that we can reach.  For the moment, do the same test as in
       proc_insert: check to see if we have compatible IPv4
       networks. */
    /* JMS This is essentially duplicated code (see proc_insert,
       below) -- should figure out how subroutine-ize this
       functionality. */
    mynet = receiver->if_ipv4_addr;
    for (i = 0, mask = ~0; i < (32 - receiver->if_cidrmask); ++i, mask >>= 1) {
        continue;
    }
    mynet &= mask;

    for (i = 0; i < proc->proc_endpoint_count; ++i) {
        endpoint = proc->proc_endpoints[i];
        peernet = endpoint->endpoint_remote_addr.ipv4_addr;
        for (j = 0, mask = ~0; j < (32 - endpoint->endpoint_remote_addr.cidrmask);
             ++j, mask >>= 1) {
            continue;
        }
        peernet &= mask;

        /* If we match, we're done */
        if (mynet == peernet) {
            return endpoint;
        }
    }

    /* Didn't find it */
    return NULL;
}


/*
 * Create a usnic process structure. There is a one-to-one
 * correspondence between a ompi_proc_t and a ompi_btl_usnic_proc_t
 * instance. We cache additional data (specifically the list of
 * ompi_btl_usnic_endpoint_t instances, and published addresses)
 * associated with a given destination on this data structure.
 */
ompi_btl_usnic_proc_t* ompi_btl_usnic_proc_create(ompi_proc_t* ompi_proc)
{
    ompi_btl_usnic_proc_t *proc = NULL;
    size_t size;
    int rc;

    /* Check if we have already created a module proc structure for
       this ompi process */
    proc = ompi_btl_usnic_proc_lookup_ompi(ompi_proc);
    if (proc != NULL) {
        OBJ_RETAIN(proc);
        return proc;
    }

    /* Create the proc if it doesn't already exist */
    proc = OBJ_NEW(ompi_btl_usnic_proc_t);
    /* Initialize number of peer */
    proc->proc_endpoint_count = 0;
    proc->proc_ompi = ompi_proc;

    /* query for the peer address info */
    rc = ompi_modex_recv(&mca_btl_usnic_component.super.btl_version,
                         ompi_proc, (void*)&proc->proc_modex,
                         &size);

    if (OMPI_SUCCESS != rc) {
        /* JMS better error message */
        opal_output(0,
                "[%s:%d] ompi_modex_recv failed for peer %s",
                __FILE__, __LINE__, ORTE_NAME_PRINT(&ompi_proc->proc_name));
        OBJ_RELEASE(proc);
        return NULL;
    }

    if ((size % sizeof(ompi_btl_usnic_addr_t)) != 0) {
        /* JMS better error message */
        opal_output(0, "[%s:%d] invalid module address for peer %s",
                __FILE__, __LINE__, ORTE_NAME_PRINT(&ompi_proc->proc_name));
        OBJ_RELEASE(proc);
        return NULL;
    }

    proc->proc_modex_count = size / sizeof(ompi_btl_usnic_addr_t);
    if (0 == proc->proc_modex_count) {
        proc->proc_endpoints = NULL;
        OBJ_RELEASE(proc);
        return NULL;
    }

    proc->proc_modex_claimed = (bool*) calloc(proc->proc_modex_count, sizeof(bool));
    if (NULL == proc->proc_modex_claimed) {
        ORTE_ERROR_LOG(OMPI_ERR_OUT_OF_RESOURCE);
        OBJ_RELEASE(proc);
        return NULL;
    }

    proc->proc_endpoints = (mca_btl_base_endpoint_t**)
        calloc(proc->proc_modex_count, sizeof(mca_btl_base_endpoint_t*));
    if (NULL == proc->proc_endpoints) {
        ORTE_ERROR_LOG(OMPI_ERR_OUT_OF_RESOURCE);
        OBJ_RELEASE(proc);
        return NULL;
    }

    return proc;
}


/*
 * Insert an endpoint into the proc array and assign it an address.
 *
 * MUST be called with the proc lock held!
 */
int ompi_btl_usnic_proc_insert(ompi_btl_usnic_module_t *module,
                                 ompi_btl_usnic_proc_t *proc,
                                 mca_btl_base_endpoint_t *endpoint)
{
    size_t i;

#ifndef WORDS_BIGENDIAN
    /* If we are little endian and our peer is not so lucky, then we
       need to put all information sent to him in big endian (aka
       Network Byte Order) and expect all information received to
       be in NBO.  Since big endian machines always send and receive
       in NBO, we don't care so much about that case. */
    if (proc->proc_ompi->proc_arch & OPAL_ARCH_ISBIGENDIAN) {
        endpoint->endpoint_nbo = true;
    }
#endif

    /* Each module can claim an address in the proc's modex info that
       no other local module is using.  See if we can find an unused
       address that's on this module's subnet. */
    for (i = 0; i < proc->proc_modex_count; ++i) {
        if (!proc->proc_modex_claimed[i]) {
            char my_ip_string[32], peer_ip_string[32];
            uint32_t j, mask, mynet, peernet;

            inet_ntop(AF_INET, &module->if_ipv4_addr,
                      my_ip_string, sizeof(my_ip_string));
            inet_ntop(AF_INET, &proc->proc_modex[i].ipv4_addr,
                      peer_ip_string, sizeof(peer_ip_string));
            opal_output_verbose(5, mca_btl_base_output, "btl:usnic:proc_insert: checking my IP address/subnet (%s/%d) vs. peer (%s/%d)\n",
                        my_ip_string, module->if_cidrmask,
                        peer_ip_string, proc->proc_modex[i].cidrmask);

            /* JMS For the moment, do an abbreviated comparison.  Just
               compare the CIDR-masked IP address to see if they're on
               the same network.  If so, we're good.  Need to
               eventually replace this with the same type of IP
               address matching that is in the TCP BTL (probably want
               to move that routine down to opal/util/if.c...?) */
            /* JMS mynet is loop invariant */
            mynet = module->if_ipv4_addr;
            for (j = 0, mask = ~0; j < (32 - module->if_cidrmask); 
                 ++j, mask >>= 1) {
                continue;
            }
            mynet &= mask;

            peernet = proc->proc_modex[i].ipv4_addr;
            for (j = 0, mask = ~0; j < (32 - proc->proc_modex[i].cidrmask);
                 ++j, mask >>= 1) {
                continue;
            }
            peernet &= mask;

            /* If we match, we're done */
            if (mynet == peernet) {
                opal_output_verbose(5, mca_btl_base_output, "btl:usnic:proc_insert: IP networks match -- yay!\n");
                break;
            }
        }
    }

    /* If we didn't find one, return failure */
    if (i >= proc->proc_modex_count) {
        /* JMS better error message */
        BTL_VERBOSE(("--- did not find matching address for this peer"));
        return OMPI_ERR_NOT_FOUND;
    }

    proc->proc_modex_claimed[i] = true;
    endpoint->endpoint_remote_addr = 
        proc->proc_modex[proc->proc_endpoint_count];
    proc->proc_endpoints[proc->proc_endpoint_count] = endpoint;
    OBJ_RETAIN(endpoint);
    ++proc->proc_endpoint_count;

    return OMPI_SUCCESS;
}


/*
 * Remove an endpoint from the proc array.
 */
int ompi_btl_usnic_proc_remove(ompi_btl_usnic_proc_t* proc,
                                 mca_btl_base_endpoint_t* endpoint)
{
    size_t i;

    OPAL_THREAD_LOCK(&proc->proc_lock);
    for (i = 0; i < proc->proc_endpoint_count; i++) {
        if (proc->proc_endpoints[i] == endpoint) {
            memmove(proc->proc_endpoints + i, proc->proc_endpoints + i + 1,
                    (proc->proc_endpoint_count -i - 1) *
                            sizeof(mca_btl_base_endpoint_t*));
            if (--proc->proc_endpoint_count == 0) {
                OPAL_THREAD_UNLOCK(&proc->proc_lock);
                OBJ_RELEASE(proc);
                return OMPI_SUCCESS;
            }

            break;
        }
    }

    OPAL_THREAD_UNLOCK(&proc->proc_lock);
    return OMPI_SUCCESS;
}

