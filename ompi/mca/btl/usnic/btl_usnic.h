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

/* Set to 1 to turn out a LOT of debugging ouput */
#define MSGDEBUG 0
/* Set to 1 to run off reliability */
#define RELIABILITY 1
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

#include "btl_usnic_endpoint.h"

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

/*
 * Have the window size as a compile-time constant that is a power of
 * two so that we can take advantage of fast bit operations.
 */
#if RELIABILITY
#define WINDOW_SIZE 4096
#define WINDOW_SIZE_MOD(a) (((a) & (WINDOW_SIZE - 1)))
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

    /** max completion queue entries per module */
    int32_t cq_num;

#if RELIABILITY
    /** retrans characteristics */
    int retrans_timeout;
#endif
} ompi_btl_usnic_component_t;

OMPI_MODULE_DECLSPEC extern ompi_btl_usnic_component_t mca_btl_usnic_component;

typedef mca_btl_base_recv_reg_t ompi_btl_usnic_recv_reg_t;


/**
 * UD verbs BTL interface
 */
typedef struct ompi_btl_usnic_module_t {
    mca_btl_base_module_t super;

    uint8_t port_num;
    struct ibv_device *device;
    struct ibv_context *device_context;
    struct ibv_pd *pd;
    struct ibv_cq *cq;

    /* Information about the IP interface corresponding to this USNIC
       interface */
    char if_name[64];
    uint32_t if_ipv4_addr;
    uint32_t if_cidrmask;
    uint8_t if_mac[6];
    int if_mtu;

    /** desired send, receive, and completion queue entries (from MCA
        params; cached here on the component because the MCA param
        might == 0, which means "max supported on that device") */
    int sd_num;
    int rd_num;
    int cq_num;

    /** Hash table to keep track of senders */
    opal_hash_table_t senders;

    /** local address information */
    struct ompi_btl_usnic_addr_t addr;

    /** send fragments & buffers */
    ompi_free_list_t send_frags;

    /** receive fragments & buffers */
    ompi_free_list_t recv_frags;

#if RELIABILITY
    /** list of send frags that are waiting to be resent (they
        previously deferred because of lack of resources) */
    opal_list_t pending_resend_frags;

    /** ack frags */
    ompi_free_list_t ack_frags;

    /** list of endpoints to which we need to send ACKs */
    opal_list_t endpoints_that_need_acks;
#endif

    /** available send WQ entries */
    int32_t sd_wqe;

    /** queue pair */
    struct ibv_qp* qp;
    uint32_t qp_max_inline;

    /* Debugging statistics */
    bool final_stats;
    uint64_t stats_report_num;

    uint64_t num_total_sends;
    uint64_t num_resends;
    uint64_t num_frag_sends;
    uint64_t num_ack_sends;

    uint64_t num_total_recvs;
    uint64_t num_unk_recvs;
    uint64_t num_dup_recvs;
    uint64_t num_oow_recvs;
    uint64_t num_frag_recvs;
    uint64_t num_ack_recvs;
    uint64_t num_recv_reposts;

    uint64_t num_deferred_sends_no_wqe;
    uint64_t num_deferred_sends_outside_window;
    uint64_t num_deferred_sends_hotel_full;

    uint64_t max_sent_window_size;
    uint64_t max_rcvd_window_size;

    uint64_t pml_module_sends;
    uint64_t pml_module_sends_deferred;
    uint64_t pml_send_callbacks;

    ompi_btl_usnic_endpoint_t **all_endpoints;
    int num_endpoints;

    opal_event_t stats_timer_event;
    struct timeval stats_timeout;
} ompi_btl_usnic_module_t;

struct ompi_btl_usnic_frag_t;
extern ompi_btl_usnic_module_t ompi_btl_usnic_module_template;


/*
 * Initialize a module
 */
int ompi_btl_usnic_module_init(ompi_btl_usnic_module_t* module);


/**
 * Register the usnic BTL MCA params
 */
int ompi_btl_usnic_component_register(void); 


END_C_DECLS
#endif
