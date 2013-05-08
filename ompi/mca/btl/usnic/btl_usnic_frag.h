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

#ifndef OMPI_BTL_USNIC_FRAG_H
#define OMPI_BTL_USNIC_FRAG_H

#define OMPI_BTL_USNIC_FRAG_ALIGN (8)

#include <infiniband/verbs.h>

#include "btl_usnic.h"


BEGIN_C_DECLS


/*
 * The module struct
 */    
struct ompi_btl_usnic_module_t;

/**
 * Fragment types
 */
typedef enum {
#if RELIABILITY
    OMPI_BTL_USNIC_FRAG_ACK,
#endif
    OMPI_BTL_USNIC_FRAG_SEND,
    OMPI_BTL_USNIC_FRAG_RECV
} ompi_btl_usnic_frag_type_t;


typedef struct ompi_btl_usnic_reg_t {
    mca_mpool_base_registration_t base;
    struct ibv_mr* mr;
} ompi_btl_usnic_reg_t;

/* 
 * Header that is the beginning of every usnic packet buffer.
 * Depends on whether we are IB UD or USNIC UD.
 */
typedef struct {
    /* IB UD global resource header (GRH), which appears on the
       receiving side only. */
    struct ibv_grh grh;
} ompi_btl_usnic_protocol_header_t;

/**
 * usnic header type
 */
typedef enum {
#if RELIABILITY
    OMPI_BTL_USNIC_PAYLOAD_TYPE_SEQ_ACK = 1,
#endif
    OMPI_BTL_USNIC_PAYLOAD_TYPE_MSG = 2
} ompi_btl_usnic_payload_type_t;

/**
 * Size for sequence numbers (just to ensure we use the same size
 * everywhere)
 */
typedef uint64_t ompi_btl_usnic_seq_t;
#define UDSEQ "lu"

/**
 * BTL header that goes after the protocol header.  Since this is not
 * a stream, we can put the fields in whatever order make the least
 * holes.
 */
typedef struct {
    /* Hashed ORTE process name of the sender */
    /* JMS Can this be replaced with a pointer?  Requires an initial
       handshake somehow... */
    uint64_t sender;
#if RELIABILITY
    /* Sliding window sequence number (echoed back in an ACK).  This
       is 64 bits. */
    ompi_btl_usnic_seq_t seq;
#endif
    /* Type of BTL header (see enum, above) */
    uint8_t payload_type;
    /* Yuck */
    uint8_t padding;
    /* payload legnth (in bytes).  We unfortunately have to include
       this in our header because the L2 layer may artifically inflate
       the length of the packet to meet a minimum size */
    uint16_t payload_len; 
} ompi_btl_usnic_btl_header_t; 

/* JMS fix the seq endian flipping */
#define OMPI_BTL_USNIC_FRAME_HTON(hdr)                \
    do {                                                \
        hdr.seq = htons(hdr.seq);                       \
        hdr.payload_size = htons(hdr.payload_size);     \
    } while (0)

#define OMPI_BTL_USNIC_FRAME_NTOH(hdr)                \
    do {                                                \
        hdr.seq = ntohs(hdr.seq);                       \
        hdr.payload_size = ntohs(hdr.payload_size);     \
    } while (0)

#if HISTORY
/* JMS debugging support */
typedef struct {
    char file[256];
    int line;
    char message[256];
} ompi_btl_usnic_frag_history_t;
#define NUM_FRAG_HISTORY 32
#endif

/*
 * Enums for the states of frags
 */
typedef enum {
    /* Frag states: all frags */
    FRAG_ALLOCED = 0x01,

    /* Frag states: send frags */
    FRAG_SEND_ACKED = 0x02,
    FRAG_SEND_ENQUEUED = 0x04,
    FRAG_PML_CALLED_BACK = 0x08,
    FRAG_PML_FREED = 0x10,
    FRAG_IN_HOTEL = 0x20,

    /* Frag states: receive frags */
    FRAG_RECV_WR_POSTED = 0x40,

    FRAG_MAX = 0xff
} ompi_btl_usnic_frag_state_flags_t;


/* 
 * Convenience macros for states
 */
#define FRAG_STATE_SET(frag, state) (frag)->state_flags |= (state)
#define FRAG_STATE_CLR(frag, state) (frag)->state_flags &= ~(state)
#define FRAG_STATE_GET(frag, state) ((frag)->state_flags & (state))
#define FRAG_STATE_ISSET(frag, state) (((frag)->state_flags & (state)) != 0)


/**
 * send fragment derived type.
 */
typedef struct ompi_btl_usnic_frag_t {
    mca_btl_base_descriptor_t base;
    mca_btl_base_segment_t segment;

    struct mca_btl_base_endpoint_t *endpoint;

    ompi_btl_usnic_frag_type_t type;
    
    union {
        struct ibv_recv_wr rd_desc;
        struct ibv_send_wr sr_desc;
    } wr_desc;
    struct ibv_sge sg_entry;

    ompi_btl_usnic_protocol_header_t *protocol_header;
    ompi_btl_usnic_btl_header_t *btl_header;
    union {
        uint8_t *raw;
        mca_btl_base_header_t *pml_header;
#if RELIABILITY
        ompi_btl_usnic_seq_t *ack;
#endif
    } payload;

    ompi_btl_usnic_reg_t* ud_reg;

    /* Bit flags for states */
    uint32_t state_flags;

    /* How many times is this frag on a hardware queue? */
    int send_wr_posted;

#if HISTORY
    /* Debugging support */
    ompi_btl_usnic_frag_history_t history[NUM_FRAG_HISTORY];
    int history_start;
    int history_next;
#endif
} ompi_btl_usnic_frag_t;
OBJ_CLASS_DECLARATION(ompi_btl_usnic_frag_t);

typedef struct ompi_btl_usnic_frag_t ompi_btl_usnic_send_frag_t;
OBJ_CLASS_DECLARATION(ompi_btl_usnic_send_frag_t);

typedef struct ompi_btl_usnic_frag_t ompi_btl_usnic_recv_frag_t;
OBJ_CLASS_DECLARATION(ompi_btl_usnic_recv_frag_t);

#if RELIABILITY
typedef struct ompi_btl_usnic_frag_t ompi_btl_usnic_ack_frag_t;
OBJ_CLASS_DECLARATION(ompi_btl_usnic_ack_frag_t);
#endif

/*
 * Alloc a send frag from the send pool
 */
ompi_btl_usnic_frag_t *
ompi_btl_usnic_frag_send_alloc(struct ompi_btl_usnic_module_t *module);

/*
 * Is a send frag ok to return?
 */
bool 
ompi_btl_usnic_frag_send_ok_to_return(struct ompi_btl_usnic_module_t *module,
                                      ompi_btl_usnic_frag_t *frag);

/* 
 * Return a send frag
 */
void ompi_btl_usnic_frag_send_return(struct ompi_btl_usnic_module_t *module,
                                     ompi_btl_usnic_frag_t *frag);

/* 
 * Return a send frag conditionally
 */
void 
ompi_btl_usnic_frag_send_return_cond(struct ompi_btl_usnic_module_t *module,
                                     ompi_btl_usnic_frag_t *frag);


#if RELIABILITY
/*
 * Alloc an ACK frag
 */
ompi_btl_usnic_frag_t *
ompi_btl_usnic_frag_ack_alloc(struct ompi_btl_usnic_module_t *module);

/* 
 * Return an ACK frag
 */
void ompi_btl_usnic_frag_ack_return(struct ompi_btl_usnic_module_t *module,
                                      ompi_btl_usnic_frag_t *frag);

/*
 * See if we can advance the pending send queue.
 */
void ompi_btl_usnic_frag_progress_pending_resends(struct ompi_btl_usnic_module_t *module);
#endif

/*
 * Debugging: dump a frag
 */
void ompi_btl_usnic_frag_dump(ompi_btl_usnic_frag_t *frag);

#if HISTORY
/*
 * Debugging: history
 */
void ompi_btl_usnic_frag_history(ompi_btl_usnic_frag_t *frag,
                                   char *file, int line,
                                   const char *msg);

#define FRAG_HISTORY(frag, msg) \
    ompi_btl_usnic_frag_history((frag), __FILE__, __LINE__, (msg));
#else
#define FRAG_HISTORY(frag, msg)
#endif /* HISTORY */

END_C_DECLS

#endif
