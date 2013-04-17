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
 * Copyright (c) 2006      Sandia National Laboratories. All rights
 *                         reserved.
 * Copyright (c) 2013 Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef OMPI_BTL_USNIC_ENDPOINT_H
#define OMPI_BTL_USNIC_ENDPOINT_H

#include <infiniband/verbs.h>

#include "opal/class/opal_list.h"
#include "opal/event/event.h"
#include "opal_hotel.h"

#include "btl_usnic.h"
#include "btl_usnic_frag.h"

BEGIN_C_DECLS

typedef struct ompi_btl_usnic_addr_t {
    uint32_t qp_num;
    union ibv_gid gid;
    uint32_t ipv4_addr;
    uint32_t cidrmask;

    /* JMS For now, we also need the MAC */
    uint8_t mac[6];
    int mtu;
} ompi_btl_usnic_addr_t;


/*
 * Pending frags that we store in send/receive windows
 */
typedef struct ompi_btl_usnic_pending_frag_t {
    struct ompi_btl_usnic_frag_t *frag;
    int hotel_room;
    /* JMS could space optimize this -- perhaps remove the bool (and
       just check for frag==NULL)?  Probably not a big deal because
       it's going to be 64 bit aligned, but... */
    bool occupied;
} ompi_btl_usnic_pending_frag_t;

/*
 * Item for the list of endpoints that received something (and
 * therefore need an ACK)
 */
struct mca_btl_base_endpoint_t;
typedef struct {
    opal_list_item_t super;
    struct mca_btl_base_endpoint_t *endpoint;
    bool enqueued;
} ompi_btl_usnic_endpoint_list_item_t;

/**
 * An abstraction that represents a connection to a remote process.
 * An instance of mca_btl_base_endpoint_t is associated with each
 * (btl_usnic_proc_t, btl_usnic_module_t) tuple and address
 * information is exchanged at startup.  The usnic BTL is
 * connectionless, so no connection is ever established.
 */
typedef struct mca_btl_base_endpoint_t {
    opal_list_item_t            super;

    /** BTL module that created this connection */
    struct ompi_btl_usnic_module_t *endpoint_module;
    /** Usnic proc structure corresponding to endpoint */
    struct ompi_btl_usnic_proc_t   *endpoint_proc;
#if RELIABILITY
    /** List item pointing to this endpoint */
    ompi_btl_usnic_endpoint_list_item_t endpoint_li;
#endif

    /** Remote address information */
    ompi_btl_usnic_addr_t          endpoint_remote_addr;

    /** Remote address handle */
    struct ibv_ah*                   endpoint_remote_ah;

#if RELIABILITY
    /** OPAL hotel to track outstanding stends */
    opal_hotel_t                     endpoint_hotel;

    /** Sliding window parameters for this peer */
    /* Values for the current proc to send to this endpoint on the
       peer proc */
    ompi_btl_usnic_seq_t           endpoint_next_seq_to_send; /* n_t */
    ompi_btl_usnic_seq_t           endpoint_ack_seq_rcvd; /* n_a */

    ompi_btl_usnic_pending_frag_t *endpoint_sent_frags;
    uint32_t                         endpoint_sfstart;

    /* Values for the current proc to receive from this endpoint on
       the peer proc */
    ompi_btl_usnic_seq_t           endpoint_next_contig_seq_to_recv; /* n_r */
    ompi_btl_usnic_seq_t           endpoint_highest_seq_rcvd; /* n_s */

    ompi_btl_usnic_pending_frag_t *endpoint_rcvd_frags;
    uint32_t                         endpoint_rfstart;
#endif

    /** Do we need to convert headers to network byte order? */
    bool                             endpoint_nbo;
} mca_btl_base_endpoint_t;

typedef mca_btl_base_endpoint_t ompi_btl_usnic_endpoint_t;
OBJ_CLASS_DECLARATION(ompi_btl_usnic_endpoint_t);

int ompi_btl_usnic_endpoint_post_send(struct ompi_btl_usnic_module_t* ud_btl,
                                       struct ompi_btl_usnic_frag_t * frag);

END_C_DECLS
#endif
