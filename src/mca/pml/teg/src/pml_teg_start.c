/*
 * $HEADER$
 */

#include "ompi_config.h"

#include "pml_teg.h"
#include "pml_teg_recvreq.h"
#include "pml_teg_sendreq.h"


int mca_pml_teg_start(size_t count, ompi_request_t** requests)
{
    int rc;
    size_t i;
    for(i=0; i<count; i++) {
        mca_pml_base_request_t *pml_request = (mca_pml_base_request_t*)requests[i];
        if(NULL == pml_request)
            continue;

        /* If the persistent request is currently active - obtain the
         * request lock and verify the status is incomplete. if the
         * pml layer has not completed the request - mark the request
         * as free called - so that it will be freed when the request
         * completes - and create a new request.
         */

        switch(pml_request->req_ompi.req_state) {
            case OMPI_REQUEST_INVALID: 
                return OMPI_ERR_REQUEST;
            case OMPI_REQUEST_INACTIVE:
                break;
            case OMPI_REQUEST_ACTIVE: {
            
                ompi_request_t *request;
                OMPI_THREAD_LOCK(&mca_pml_teg.teg_request_lock);
                if (pml_request->req_pml_done == false) {
                    /* free request after it completes */
                    pml_request->req_free_called = true;
                } else {
                    /* can reuse the existing request */
                    OMPI_THREAD_UNLOCK(&mca_pml_teg.teg_request_lock);
                    break;
                }
                OMPI_THREAD_UNLOCK(&mca_pml_teg.teg_request_lock);

                /* allocate a new request */
                switch(pml_request->req_type) {
                    case MCA_PML_REQUEST_SEND: {
                         mca_pml_base_send_mode_t sendmode = 
                             ((mca_pml_base_send_request_t*)pml_request)->req_send_mode;
                         rc = mca_pml_teg_isend_init(
                              pml_request->req_addr,
                              pml_request->req_count,
                              pml_request->req_datatype,
                              pml_request->req_peer,
                              pml_request->req_tag,
                              sendmode,
                              pml_request->req_comm,
                              &request);
                         if (sendmode == MCA_PML_BASE_SEND_BUFFERED) {
                             mca_pml_base_bsend_request_init(request, true);
                         }
                         break;
                    }
                    case MCA_PML_REQUEST_RECV:
                         rc = mca_pml_teg_irecv_init(
                              pml_request->req_addr,
                              pml_request->req_count,
                              pml_request->req_datatype,
                              pml_request->req_peer,
                              pml_request->req_tag,
                              pml_request->req_comm,
                              &request);
                         break;
                    default:
                         rc = OMPI_ERR_REQUEST;
                         break;
                }
                if(OMPI_SUCCESS != rc)
                    return rc;
                pml_request = (mca_pml_base_request_t*)request;
                requests[i] = request;
                break;
            }
        }

        /* start the request */
        switch(pml_request->req_type) {
            case MCA_PML_REQUEST_SEND:
                if((rc = mca_pml_teg_send_request_start((mca_pml_base_send_request_t*)pml_request)) 
                    != OMPI_SUCCESS)
                    return rc;
                break;
            case MCA_PML_REQUEST_RECV:
                if((rc = mca_pml_teg_recv_request_start((mca_pml_base_recv_request_t*)pml_request)) 
                    != OMPI_SUCCESS)
                    return rc;
                break;
            default:
                return OMPI_ERR_REQUEST;
        }
    }
    return OMPI_SUCCESS;
}

