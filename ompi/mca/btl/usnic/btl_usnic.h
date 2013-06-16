/*
 * Copyright (c) 2004-2008 The Trustees of Indiana University and Indiana
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
 * Copyright (c) 2011-2013 Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/**
 * @file
 */
#ifndef OMPI_BTL_USNIC_H
#define OMPI_BTL_USNIC_H

#include "ompi_config.h"
#include <sys/types.h>
#include <infiniband/verbs.h>

#include "opal/class/opal_hash_table.h"
#include "opal/class/opal_hash_table.h"
#include "opal/event/event.h"

#include "ompi/class/ompi_free_list.h"
#include "ompi/mca/btl/btl.h"
#include "ompi/mca/btl/base/btl_base_error.h"
#include "ompi/mca/btl/base/base.h"
#include "ompi/mca/mpool/rdma/mpool_rdma.h"

static inline uint64_t
get_nsec(void)
{
    static unsigned long isec, insec;
    struct timespec ts;
    uint64_t ns;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    if (insec == 0) {
        isec = ts.tv_sec;
        insec = ts.tv_nsec;
    }
    ns = (ts.tv_sec - isec) * 1000000000LL + (long)(ts.tv_nsec - insec);
    return ns;
}

#define container_of(ptr, type, member) ( \
        (type *)( ((char *)(ptr)) - offsetof(type,member) ))

/* Set to 1 to turn out a LOT of debugging ouput */
#define MSGDEBUG2 (MSGDEBUG1||0)     /* temp */
#define MSGDEBUG1 (MSGDEBUG||0)     /* temp */
#define MSGDEBUG 0
/* Set to 1 to get frag history */
#define HISTORY 0
/* Set to >0 to randomly drop received frags.  The higher the number,
   the more frequent the drops. */
#define WANT_RECV_FRAG_DROPS 0
/* Set to >0 to randomly fail to send an ACK, mimicing a lost ACK.
   The higher the number, the more frequent the failed-to-send-ACK. */
#define WANT_FAIL_TO_SEND_ACK 0
/* Set to >0 to randomly fail to resend a frag (causing it to be
   requed to be sent later).  The higher the number, the more frequent
   the failed-to-resend-frag. */
#define WANT_FAIL_TO_RESEND_FRAG 0

BEGIN_C_DECLS


#if WANT_RECV_FRAG_DROPS > 0
#define FAKE_RECV_FRAG_DROP (rand() < WANT_RECV_FRAG_DROPS)
#else
#define FAKE_RECV_FRAG_DROP 0
#endif

#if WANT_FAIL_TO_SEND_ACK > 0
#define FAKE_FAIL_TO_SEND_ACK (rand() < WANT_FAIL_TO_SEND_ACK)
#else
#define FAKE_FAIL_TO_SEND_ACK 0
#endif

#if WANT_FAIL_TO_RESEND_FRAG > 0
#define FAKE_FAIL_TO_RESEND_FRAG (rand() < WANT_FAIL_TO_RESEND_FRAG)
#else
#define FAKE_FAIL_TO_RESEND_FRAG 0
#endif


/**
 * Verbs UD BTL component.
 */
typedef struct ompi_btl_usnic_component_t {
    /** base BTL component */
    mca_btl_base_component_2_0_0_t super;

    /** Maximum number of BTL modules */
    uint32_t max_modules;
    /** Number of available/initialized BTL modules */
    uint32_t num_modules;

    char *if_include;
    char *if_exclude;
    uint32_t *vendor_part_ids;

    /* Cached hashed version of my ORTE proc name (to stuff in
       protocol headers) */
    uint64_t my_hashed_orte_name;

    /** array of available BTLs */
    struct ompi_btl_usnic_module_t* usnic_modules;

    /** list of ib proc structures */
    opal_list_t usnic_procs;

    /** name of memory pool */
    char* usnic_mpool_name;

    /** Want stats? */
    bool stats_enabled;
    bool stats_relative;
    int stats_frequency;

    /** GID index to use */
    int gid_index;

    /** max send descriptors to post per module */
    int32_t sd_num;

    /** max receive descriptors per module */
    int32_t rd_num;

    /** max send/receive desriptors for priority channel */
    int32_t prio_sd_num;
    int32_t prio_rd_num;

    /** max completion queue entries per module */
    int32_t cq_num;

    /** retrans characteristics */
    int retrans_timeout;
} ompi_btl_usnic_component_t;

OMPI_MODULE_DECLSPEC extern ompi_btl_usnic_component_t mca_btl_usnic_component;

typedef mca_btl_base_recv_reg_t ompi_btl_usnic_recv_reg_t;

/**
 * Size for sequence numbers (just to ensure we use the same size
 * everywhere)
 */
typedef uint64_t ompi_btl_usnic_seq_t;
#define UDSEQ "lu"

/**
 * Register the usnic BTL MCA params
 */
int ompi_btl_usnic_component_register(void); 


END_C_DECLS
#endif
