/*
 * $HEADER$
 */

#include "mca/ptl/base/ptl_base_sendfrag.h"

static void mca_ptl_base_send_frag_construct(mca_ptl_base_send_frag_t* frag);
static void mca_ptl_base_send_frag_destruct(mca_ptl_base_send_frag_t* frag);


lam_class_t mca_ptl_base_send_frag_t_class = { 
    "mca_ptl_base_send_frag_t", 
    OBJ_CLASS(mca_ptl_base_frag_t),
    (lam_construct_t) mca_ptl_base_send_frag_construct, 
    (lam_destruct_t) mca_ptl_base_send_frag_destruct 
};
                                                                                                 

static void mca_ptl_base_send_frag_construct(mca_ptl_base_send_frag_t* frag)
{
}

static void mca_ptl_base_send_frag_destruct(mca_ptl_base_send_frag_t* frag)
{
}

