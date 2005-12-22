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
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */


/*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/

#include "ompi_config.h"
#include "ompi/include/constants.h"
#include "orte/util/proc_info.h"
#include "mca/pml/pml.h"
#include "mca/btl/btl.h"
#include "mca/bml/bml.h"
#include "mca/ns/ns_types.h"
#include "mca/mpool/mpool.h" 
#include "pml_ob1.h"
#include "pml_ob1_rdma.h"

/*
 * Check to see if memory is registered or can be registered. Build a 
 * set of registrations on the request.
 */

size_t mca_pml_ob1_rdma_btls(
    mca_bml_base_endpoint_t* bml_endpoint,
    unsigned char* base,
    size_t size,
    mca_pml_ob1_rdma_btl_t* rdma_btls)
{
    size_t num_btls = mca_bml_base_btl_array_get_size(&bml_endpoint->btl_rdma);
    ompi_pointer_array_t regs;
    size_t num_btls_used = 0;
    size_t n;
    int rc;

    /* shortcut when there are no rdma capable btls */
    if(num_btls == 0) {
        return 0;
    }

    /* check to see if memory is registered */        
    OBJ_CONSTRUCT(&regs, ompi_pointer_array_t);
    for(n = 0; n < num_btls && num_btls_used < MCA_PML_OB1_MAX_RDMA_PER_REQUEST; n++) {

        mca_bml_base_btl_t* bml_btl = mca_bml_base_btl_array_get_index(&bml_endpoint->btl_rdma, n); 
        mca_mpool_base_registration_t* fit = NULL;
        mca_mpool_base_registration_t* largest = NULL;
        mca_mpool_base_module_t* btl_mpool = bml_btl->btl_mpool;
        uint32_t reg_cnt;
        size_t r;
 
        /* btl is rdma capable and registration is not required */
        if(NULL == btl_mpool) {
            rdma_btls[num_btls_used].bml_btl = bml_btl;
            rdma_btls[num_btls_used].btl_reg = NULL;
            num_btls_used++;
            continue;
        }
 
        /* look through existing registrations */
        ompi_pointer_array_remove_all(&regs);
        btl_mpool->mpool_find(btl_mpool, 
                              base,
                              size,
                              &regs, 
                              &reg_cnt); 
        assert(reg_cnt <= 1);
        /* shortcut for one entry - the typical case */
        if(reg_cnt == 1) {
            mca_mpool_base_registration_t* reg  = ompi_pointer_array_get_item(&regs, 0); 
            size_t reg_len = reg->bound - base + 1;
            if(reg->flags & MCA_MPOOL_FLAGS_CACHE) { 
                assert(reg->ref_count >= 3); 
            }

            /* is the existing registration the required size */
            if(reg->base <= base && reg_len >= size) {

                rdma_btls[num_btls_used].bml_btl = bml_btl;
                rdma_btls[num_btls_used].btl_reg = reg;
                num_btls_used++;

            /* otherwise if leave_pinned re-register */
            } else if( mca_pml_ob1.leave_pinned ) {

                btl_mpool->mpool_deregister(btl_mpool, reg); 
                rc = btl_mpool->mpool_register(btl_mpool, 
                    base, 
                    size, 
                    MCA_MPOOL_FLAGS_CACHE,
                    &reg); 
                if(OMPI_SUCCESS != rc || NULL == reg) {
                    opal_output(0, "[%s:%d] mpool_register(%p,%lu) failed, \n", __FILE__, __LINE__, base, size);
                    continue;
                }
                
                rdma_btls[num_btls_used].bml_btl = bml_btl;
                rdma_btls[num_btls_used].btl_reg = reg;
                num_btls_used++;

            /* existing registration cannot be used */
            } else {
                btl_mpool->mpool_release(btl_mpool, reg);
            }
            continue;
        }
        /*
         * find the best fit when there are multiple registrations
        */
        for(r = 0; r < reg_cnt; r++) { 
            mca_mpool_base_registration_t* reg  = ompi_pointer_array_get_item(&regs, r); 
            size_t reg_len = reg->bound - base + 1;

            if(reg->flags & MCA_MPOOL_FLAGS_CACHE) { 
                assert(reg->ref_count >= 3); 
            }
            if(reg->base <= base && reg_len >= size) {
                fit = reg;
                break;
            } else { 
                if(NULL == largest)
                    largest = reg;
                else if(reg->base <= base && (reg->bound - base) > (largest->bound - base)) { 
                    largest = reg;                         
                }
            }
        }

        /* if the leave pinned option is set - and there is not an existing
         * registration that satisfies this request, create one.
         */
        if(NULL == fit && mca_pml_ob1.leave_pinned) {
            if (NULL == largest) {
                /* register the memory */ 
                rc = btl_mpool->mpool_register(
                       btl_mpool, 
                       base,
                       size, 
                       MCA_MPOOL_FLAGS_CACHE,
                       &fit); 
                if(ORTE_SUCCESS != rc || NULL == fit) {
                    opal_output(0, "[%s:%d] mpool_register(%p,%lu) failed, \n", __FILE__, __LINE__, base, size);
                    continue;
                }

            /* a registration exists but is not large enough */
            } else {

                /* simplify cleanup - bump reference count as we decrement again below */
                btl_mpool->mpool_retain(btl_mpool,largest);
                btl_mpool->mpool_deregister(btl_mpool, largest);
                btl_mpool->mpool_register(btl_mpool, 
                    base, 
                    size, 
                    MCA_MPOOL_FLAGS_CACHE,
                    &fit); 
                if(ORTE_SUCCESS != rc || NULL == fit) {
                    opal_output(0, "[%s:%d] mpool_register(%p,%lu) failed, \n", __FILE__, __LINE__, base, size);
                    continue;
                }
            }
            assert(fit->ref_count == 3); 
        }

        /* decrement reference count on all unused entries */
        for(r = 0; r < reg_cnt; r++) { 
            mca_mpool_base_registration_t* reg  = ompi_pointer_array_get_item(&regs, r); 
            if(reg != fit) {
                btl_mpool->mpool_release(btl_mpool, reg);
            }
        }

        if(NULL != fit) {
            rdma_btls[num_btls_used].bml_btl = bml_btl;
            rdma_btls[num_btls_used].btl_reg = fit;
            num_btls_used++;
        }
    }
    return num_btls_used;
}


/*
 * For a given btl - find the best fit registration or
 * optionally create one for leave pinned.
 */

mca_mpool_base_registration_t* mca_pml_ob1_rdma_registration(
    mca_bml_base_btl_t* bml_btl,
    unsigned char* base,
    size_t size)
{
    ompi_pointer_array_t regs;
    mca_mpool_base_registration_t* fit = NULL;
    mca_mpool_base_registration_t* largest = NULL;
    mca_mpool_base_module_t* btl_mpool = bml_btl->btl_mpool;
    uint32_t reg_cnt;
    size_t r;
    int rc;

    /* btl is rdma capable and registration is not required */
    if(NULL == btl_mpool) {
        return NULL;
    }

    /* check to see if memory is registered */        
    OBJ_CONSTRUCT(&regs, ompi_pointer_array_t);
 
    /* look through existing registrations */
    btl_mpool->mpool_find(btl_mpool, 
        base,
        size,
        &regs, 
        &reg_cnt); 

    assert(reg_cnt <= 1);
    /* shortcut for one entry - the typical case */
    if(reg_cnt == 1) {
        mca_mpool_base_registration_t* reg  = ompi_pointer_array_get_item(&regs, 0); 
        size_t reg_len = reg->bound - base + 1;

        /* is the existing registration the required size */
        if(reg->base <= base && reg_len >= size) {
            return reg;

        /* otherwise if leave_pinned re-register */
        } else if ( mca_pml_ob1.leave_pinned ) {
          
            btl_mpool->mpool_deregister(btl_mpool, reg); 
            rc = btl_mpool->mpool_register(btl_mpool, 
                base, 
                size, 
                MCA_MPOOL_FLAGS_CACHE,
                &reg); 
            if(OMPI_SUCCESS != rc || NULL == reg) {
                opal_output(0, "[%s:%d] mpool_register(%p,%lu) failed, \n", __FILE__, __LINE__, base, size);
            }
            return reg;

        /* existing registration cannot be used */
        } else {
            btl_mpool->mpool_release(btl_mpool, reg);
            return NULL;
        }
    }

    for(r = 0; r < reg_cnt; r++) { 
        mca_mpool_base_registration_t* reg  = ompi_pointer_array_get_item(&regs, r); 
        size_t reg_len = reg->bound - base + 1;
        if(reg->flags & MCA_MPOOL_FLAGS_CACHE) { 
            assert(reg->ref_count >= 3); 
        }
                
        if(reg->base <= base && reg_len >= size) {
            fit = reg;
            break;
        } else { 
            if(NULL == largest)
                largest = reg;
            else if(reg->base <= base && (reg->bound - base) > (largest->bound - base)) { 
                largest = reg;                         
            }
        }
    }

    /* if the leave pinned option is set - and there is not an existing
     * registration that satisfies this request, create one.
     */
    if(NULL == fit && mca_pml_ob1.leave_pinned) {
       if (NULL == largest) {
           /* register the memory */ 
           rc = btl_mpool->mpool_register(
              btl_mpool, 
              base,
              size, 
              MCA_MPOOL_FLAGS_CACHE,
              &fit); 
           if(OMPI_SUCCESS != rc || NULL == fit) {
               opal_output(0, "[%s:%d] mpool_register(%p,%lu) failed, \n", __FILE__, __LINE__, base, size);
           }
       /* a registration exists but is not large enough */
       } else {
           
           btl_mpool->mpool_retain(btl_mpool, largest);
           btl_mpool->mpool_deregister(btl_mpool, largest);
           rc = btl_mpool->mpool_register(btl_mpool,
               base,
               size,
               MCA_MPOOL_FLAGS_CACHE,
               &fit);
           if(OMPI_SUCCESS != rc || NULL == fit) {
               opal_output(0, "[%s:%d] mpool_register(%p,%lu) failed, \n", __FILE__, __LINE__, base, size);
           }
        }
        assert(fit->ref_count >= 3);
    }

    /* release reference count */
    for(r = 0; r < reg_cnt; r++) { 
        mca_mpool_base_registration_t *reg  = ompi_pointer_array_get_item(&regs, r); 
        if(reg != fit) {
            btl_mpool->mpool_release(btl_mpool, reg);
        }
    }
    OBJ_DESTRUCT(&regs);
    return fit;
}
