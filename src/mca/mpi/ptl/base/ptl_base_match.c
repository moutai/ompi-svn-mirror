/** @file */

/*
 * $HEADER$
 */

#include <stdio.h>

#include "mca/mpi/pml/pml.h"
#include "mca/mpi/pml/base/header.h"
#include "mca/mpi/pml/base/recvfrag.h"
#include "mca/mpi/pml/base/recvreq.h"
#include "lam/threads/mutex.h"
#include "lam/constants.h"
#include "mpi/communicator/communicator.h"
#include "mca/mpi/ptl/base/match.h"
#include "mca/mpi/pml/teg/comm.h"
#include "lam/lfc/list.h"

/**
 * RCS/CTS receive side matching
 *
 * @param frag_header list of parameters needed for matching
 *                    This list is also embeded in frag_desc,
 *                    but this allows to save a memory copy when
 *                    a match is made in this routine. (IN)
 * @param frag_desc   pointer to receive fragment which we want
 *                    to match (IN/OUT).  If a match is not made,
 *                    frag_header is copied to frag_desc.
 * @param match_made  parameter indicating if we matched frag_desc/
 *                    frag_header (OUT)
 * @param additional_matches  if a match is made with frag_desc, we
 *                    may be able to match fragments that previously
 *                    have arrived out-of-order.  If this is the
 *                    case, the associted fratment descriptors are
 *                    put on this list for further processing. (OUT)
 *
 * @return LAM error code
 *
 * This routine is used to try and match a newly arrived message fragment
 *   to pre-posted receives.  The following assumptions are made
 *   - fragments are received out of order
 *   - for long messages, e.g. more than one fragment, a RTS/CTS algorithm
 *       is used.
 *   - 2nd and greater fragments include a receive descriptor pointer
 *   - fragments may be dropped
 *   - fragments may be corrupt
 *   - this routine may be called simoultaneously by more than one thread
 */
int mca_ptl_base_match(mca_pml_base_reliable_hdr_t *frag_header,
        mca_pml_base_recv_frag_t *frag_desc, int *match_made, 
        lam_list_t *additional_matches)
{
	/* local variables */
	mca_pml_base_sequence_t frag_msg_seq_num,next_msg_seq_num_expected;
	lam_communicator_t *comm_ptr;
	mca_pml_base_recv_request_t *matched_receive;
    mca_pml_comm_t *pml_comm;
	int frag_src;

    /* initialization */
    *match_made=0;

	/* communicator pointer */
	comm_ptr=get_comm_ptr(frag_header->hdr_base.hdr_contextid);
    pml_comm=(mca_pml_comm_t *)comm_ptr->c_pml_comm;

	/* source sequence number */
	frag_msg_seq_num = frag_header->hdr_msg_seq_num;

	/* get fragment communicator source rank */
	frag_src = frag_header->hdr_frag_seq_num;

	/* get next expected message sequence number - if threaded
	 * run, lock to make sure that if another thread is processing 
	 * a frag from the same message a match is made only once.
	 * Also, this prevents other posted receives (for a pair of
	 * end points) from being processed, and potentially "loosing"
	 * the fragment.
	 */
    THREAD_LOCK((pml_comm->c_matching_lock)+frag_src);

	/* get sequence number of next message that can be processed */
	next_msg_seq_num_expected = *((pml_comm->c_next_msg_seq_num)+frag_src);

	if (frag_msg_seq_num == next_msg_seq_num_expected) {

		/*
		 * This is the sequence number we were expecting,
		 * so we can try matching it to already posted
		 * receives.
		 */

		/* We're now expecting the next sequence number. */
		(pml_comm->c_next_msg_seq_num[frag_src])++;

		/* see if receive has already been posted */
		matched_receive = lam_check_recieves_for_match(frag_header);

		/* if match found, process data */
		if (matched_receive) {
			/* 
			 * if threaded, ok to release lock, since the posted
			 * receive is not on any queue, so it won't be
			 * matched again, and the fragment can be processed
			 * w/o any conflict from other threads - locks will
			 * be used where concurent access needs to be managed.
			 */

            /* set flag indicating the input fragment was matched */
            *match_made=1;
            /* associate the receive descriptor with the fragment
             * descriptor */
            frag_desc->matched_recv=matched_receive;

			/*
			 * update deliverd sequence number information,
			 *   if need be.
			 */

		} else {
			/* if no match found, place on unexpected queue - need to
             * lock to prevent probe from interfering with updating
             * the list */
            THREAD_LOCK((pml_comm->unexpected_frags_lock)+frag_src);
            lam_list_append( ((pml_comm->unexpected_frags)+frag_src),
                    (lam_list_item_t *)frag_desc);
            THREAD_UNLOCK((pml_comm->unexpected_frags_lock)+frag_src);

			/* now that the fragment is on the list, ok to
			 * release match - other matches may be attempted */
            THREAD_UNLOCK((pml_comm->c_matching_lock)+frag_src);
		}

		/* 
		 * Now that new message has arrived, check to see if
		 *   any fragments on the c_frags_cant_match list
		 *   may now be used to form new matchs
		 */
		if (lam_list_get_size((pml_comm->frags_cant_match)+frag_src)) {
            /* initialize list to empty */
            lam_list_set_size(additional_matches,0);
			/* need to handle this -- lam_check_cantmatch_for_match();
             * */
		}

		/* 
		 * mark message as done, if it has completed - need to mark
		 *   it this late to avoid a race condition with another thread
		 *   waiting to complete a recv, completing, and try to free the
		 *   communicator before the current thread is done referencing
		 *   this communicator - is this true ?
		 */
    } else {
        /*
         * This message comes after the next expected, so it
         * is ahead of sequence.  Save it for later.
         */
        lam_list_append( ((pml_comm->frags_cant_match)+frag_src),
                    (lam_list_item_t *)frag_desc);

        /* now that the fragment is on the list, ok to
         * release match - other matches may be attempted */
        THREAD_UNLOCK((pml_comm->c_matching_lock)+frag_src);
    }

    return LAM_SUCCESS;
}
