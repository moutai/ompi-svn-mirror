/*
 * Copyright (c) 2004-2007 The Trustees of the University of Tennessee.
 *                         All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "vprotocol_base_request.h"

int mca_vprotocol_base_request_parasite(void) 
{
    int ret;
    
    if(mca_vprotocol.req_recv_class)
    {
        ompi_free_list_t pml_fl_save = mca_pml_base_recv_requests;
        mca_pml_v.host_pml_req_recv_size = 
            pml_fl_save.fl_elem_class->cls_sizeof;
        V_OUTPUT_VERBOSE(300, "req_rebuild: recv\tsize %lu+%lu\talignment=%lu", (unsigned long) mca_pml_v.host_pml_req_recv_size, (unsigned long) mca_vprotocol.req_recv_class->cls_sizeof, (unsigned long) pml_fl_save.fl_alignment);
        mca_vprotocol.req_recv_class->cls_parent = 
            pml_fl_save.fl_elem_class; 
        mca_vprotocol.req_recv_class->cls_sizeof += 
            pml_fl_save.fl_elem_class->cls_sizeof;
        /* rebuild the requests free list with the right size */
        OBJ_DESTRUCT(&mca_pml_base_recv_requests);
        OBJ_CONSTRUCT(&mca_pml_base_recv_requests, ompi_free_list_t);
        ret = ompi_free_list_init_ex(&mca_pml_base_recv_requests,
                                     mca_vprotocol.req_recv_class->cls_sizeof,
                                     pml_fl_save.fl_alignment,
                                     mca_vprotocol.req_recv_class,
                                     pml_fl_save.fl_num_allocated,
                                     pml_fl_save.fl_max_to_alloc,
                                     pml_fl_save.fl_num_per_alloc,
                                     pml_fl_save.fl_mpool,
                                     pml_fl_save.item_init,
                                     pml_fl_save.ctx);
        if(OMPI_SUCCESS != ret) return ret;
    }
    if(mca_vprotocol.req_send_class)
    {
        ompi_free_list_t pml_fl_save = mca_pml_base_send_requests;
        mca_pml_v.host_pml_req_send_size = 
            pml_fl_save.fl_elem_class->cls_sizeof;
        V_OUTPUT_VERBOSE(300, "req_rebuild: send\tsize %lu+%lu\talignment=%lu", (unsigned long) mca_pml_v.host_pml_req_send_size, (unsigned long) mca_vprotocol.req_send_class->cls_sizeof, (unsigned long) pml_fl_save.fl_alignment);
        mca_vprotocol.req_send_class->cls_parent = 
            pml_fl_save.fl_elem_class; 
        mca_vprotocol.req_send_class->cls_sizeof += 
            pml_fl_save.fl_elem_class->cls_sizeof;
        /* rebuild the requests free list with the right size */
        OBJ_DESTRUCT(&mca_pml_base_send_requests);
        OBJ_CONSTRUCT(&mca_pml_base_send_requests, ompi_free_list_t);
        ret = ompi_free_list_init_ex(&mca_pml_base_send_requests,
                                     mca_vprotocol.req_send_class->cls_sizeof,
                                     pml_fl_save.fl_alignment,
                                     mca_vprotocol.req_send_class,
                                     pml_fl_save.fl_num_allocated,
                                     pml_fl_save.fl_max_to_alloc,
                                     pml_fl_save.fl_num_per_alloc,
                                     pml_fl_save.fl_mpool,
                                     pml_fl_save.item_init,
                                     pml_fl_save.ctx);                             
        if(OMPI_SUCCESS != ret) return ret;
    }
    return OMPI_SUCCESS;
    
}
