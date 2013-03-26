/*
 * Copyright (c) 2012 Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef BTL_USNIC_ACK_H
#define BTL_USNIC_ACK_H

#include "ompi_config.h"

#include "opal_hotel.h"

#include "btl_usnic.h"
#include "btl_usnic_frag.h"
#include "btl_usnic_endpoint.h"

#if RELIABILITY

/*
 * Reap an ACK send that is complete 
 */
void ompi_btl_usnic_ack_complete(ompi_btl_usnic_module_t *module,
                                   ompi_btl_usnic_frag_t *frag);


/*
 * Send an ACK
 */
void ompi_btl_usnic_ack_send(ompi_btl_usnic_module_t *module,
                               ompi_btl_usnic_endpoint_t *endpoint);

/*
 * Callback for when a send times out without receiving a
 * corresponding ACK
 */
void ompi_btl_usnic_ack_timeout(opal_hotel_t *hotel, int room_num, 
                                  void *occupant);


/*
 * Do the actual send from _ack_timeout(), and also for if the resend
 * was queued up from _ack_timeout() due to lack of resources.
 */
void ompi_btl_usnic_ack_timeout_part2(ompi_btl_usnic_module_t *module,
                                        ompi_btl_usnic_frag_t *frag,
                                        bool direct);

#endif

#endif /* BTL_USNIC_ACK_H */
