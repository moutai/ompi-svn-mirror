/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart,
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2006      Sandia National Laboratories. All rights
 *                         reserved.
 * Copyright (c) 2007      The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2013 Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "ompi_config.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "opal/prefetch.h"

#include "orte/util/show_help.h"

#include "ompi/types.h"

#include "btl_usnic.h"
#include "btl_usnic_endpoint.h"
#include "btl_usnic_module.h"
#include "btl_usnic_frag.h"
#include "btl_usnic_proc.h"
#include "btl_usnic_util.h"
#include "btl_usnic_ack.h"
#include "btl_usnic_send.h"

/*
 * Construct/destruct an endpoint structure.
 */
static void endpoint_construct(mca_btl_base_endpoint_t* endpoint)
{
    int i;
    endpoint->endpoint_module = NULL;
    endpoint->endpoint_proc = NULL;

    for (i=0; i<USNIC_NUM_CHANNELS; ++i) {
        endpoint->endpoint_remote_addr.qp_num[i] = 0;
    }
    endpoint->endpoint_remote_addr.gid.global.subnet_prefix = 0;
    endpoint->endpoint_remote_addr.gid.global.interface_id = 0;
    endpoint->endpoint_remote_ah = NULL;

    endpoint->endpoint_send_credits = 8;

    /* list of fragments queued to be sent */
    OBJ_CONSTRUCT(&endpoint->endpoint_frag_send_queue, opal_list_t);

    endpoint->endpoint_next_seq_to_send = 10;
    endpoint->endpoint_ack_seq_rcvd = 9;
    endpoint->endpoint_next_contig_seq_to_recv = 10;
    endpoint->endpoint_highest_seq_rcvd = 9;

    endpoint->endpoint_sfstart = endpoint->endpoint_next_seq_to_send;
    endpoint->endpoint_rfstart = endpoint->endpoint_next_contig_seq_to_recv;

    /*
     * Make a new OPAL hotel for this module
     * "hotel" is a construct used for triggering segment retransmission
     * due to timeout
     */
    OBJ_CONSTRUCT(&endpoint->endpoint_hotel, opal_hotel_t);
    opal_hotel_init(&endpoint->endpoint_hotel, 
                    WINDOW_SIZE,
                    mca_btl_usnic_component.retrans_timeout,
                    0,
                    ompi_btl_usnic_ack_timeout);

    /* Setup this endpoint's ACK_needed list link */
    OBJ_CONSTRUCT(&(endpoint->endpoint_ack_li), opal_list_item_t);
    endpoint->endpoint_ack_needed = false;

    /* fragment reassembly info */
    endpoint->endpoint_rx_frag_info =
        calloc(sizeof(struct ompi_btl_usnic_rx_frag_info_t), MAX_ACTIVE_FRAGS);
    assert(NULL != endpoint->endpoint_rx_frag_info);
    if (OPAL_UNLIKELY(endpoint->endpoint_rx_frag_info == NULL)) {
        BTL_ERROR(("calloc returned NULL -- this should not happen!"));
        ompi_btl_usnic_exit();
        /* Does not return */
    }
}

static void endpoint_destruct(mca_btl_base_endpoint_t* endpoint)
{
    OBJ_DESTRUCT(&(endpoint->endpoint_ack_li));
    OBJ_DESTRUCT(&endpoint->endpoint_hotel);
#if 0
    /* JMS Restore when we keep the original calloc pointer */
    free(endpoint->endpoint_sent_frags);
#endif

    if (NULL != endpoint->endpoint_remote_ah) {
        ibv_destroy_ah(endpoint->endpoint_remote_ah);
    }
}

OBJ_CLASS_INSTANCE(ompi_btl_usnic_endpoint_t,
                   opal_list_item_t, 
                   endpoint_construct,
                   endpoint_destruct);
