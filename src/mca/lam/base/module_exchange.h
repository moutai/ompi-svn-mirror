/*
 * $HEADER$
 */

/** @file **/

#ifndef MCA_LAM_MODULE_EXCHANGE_H
#define MCA_LAM_MODULE_EXCHANGE_H

#include "lam_config.h"

#include <sys/types.h>

#include "mpi/proc/proc.h"
#include "mca/mca.h"


#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif
  /**
   * Send a module-specific buffer to all other corresponding MCA
   * modules in peer processes.
   *
   * @param source_module A pointer to this module's mca_base_module_t
   * instance.
   * @param buffer A pointer to the beginning of the buffer to send.
   * @param size Number of bytes of each instance in the buffer.
   * @param count Number of instances in the buffer.
   *
   * @retval LAM_SUCCESS On success
   * @retval LAM_ERROR On failure
   * 
   * This function takes a contiguous buffer of network-ordered data
   * and makes it available to all other MCA processes during the
   * selection process.  Modules sent by one source_module can only be
   * received by a corresponding module in peer processes.
   *
   * Two modules are "corresponding" if:
   *
   * - they share the same major and minor MCA version number
   * - they have the same type name string
   * - they share the same major and minor type version number
   * - they have the same module name string
   * - they share the same major and minor module version number
   *
   * This function is indended to be used during MCA module
   * initialization \em before \em selection (the selection process is
   * defined differently for each component type).  Each module will
   * provide a buffer containing meta information and/or parameters
   * that it wants to share with its corresponding modules in peer
   * processes.  This information typically contains location /
   * contact information for establishing communication between
   * processes (in a manner that is specific to that module).  For
   * example, a TCP-based module could provide its IP address and TCP
   * port where it is waiting on listen().  The peer process receiving
   * this buffer can therefore open a socket to the indicated IP
   * address and TCP port.
   *
   * The total number of bytes sent is (size * count).  The count
   * argument is currently superfulous, since corresponding modules,
   * by definition, should be able to deduce how many instances of
   * meta data are in the received buffer either by dividing the total
   * bytes received by the size of the module's meta data struct, or
   * by analyzing the content in the received buffer (if it is
   * self-describing data).  It is included in the interface to
   * provide flexability for modules that may require sending
   * multiple, logically separate instances of meta data (e.g., a PTL
   * module type that sends one meta data struct for each of the three
   * TCP NICs on its node).
   *
   * During the selection process, the MCA framework will effectively
   * perform an "allgather" operation of all modex buffers; every
   * buffer will be available to every peer process (see
   * mca_base_modex_recv()).
   *
   * Note that the buffer should not be modified after invoking this
   * fuction; the MCA framework may asynchronously send it to a
   * process peer at any time.
   *
   * Note again that the buffer contents is transparent to the MCA
   * framework -- it \em must already either be in network order or be
   * in some format that peer processes will be able to read it,
   * regardless of pointer sizes or endian bias.
   */
  int mca_base_modex_send(mca_base_module_t *source_module, 
                          void *buffer, size_t size, int count); 

  /**
   * Receive a module-specific buffer from a corresponding MCA module
   * in a specific peer process.
   *
   * @param dest_module A pointer to this module's mca_base_module_t
   * instance.
   * @param source_proc Peer process to receive from.
   * @param buffer A pointer to a (void*) that will be filled with a
   * pointer to the received buffer.
   * @param size Pointer to a size_t that will be filled with the
   * number of bytes of each instance in the buffer.
   * @param count Pointer to an int that will be filled with the
   * number of instances in the buffer.
   *
   * @retval LAM_SUCCESS If a corresponding module buffer is found and
   * is successfully returned to the caller.
   * @retval LAM_FAILURE If no corresponding module buffer is found,
   * or if an error occurs wil returning the buffer to the caller.
   *
   * This is the corresponding "get" call to mca_base_modex_send().
   * After selection, modules can call this function to receive the
   * buffer sent by their corresponding module on the process
   * source_proc.
   *
   * If a buffer from a corresponding module is found, buffer will be
   * filled with a pointer to a copy of the buffer that was sent by
   * the peer process.  It is the caller's responsibility to free this
   * buffer.  size will be filled in with the number of instances in
   * the buffer, and count will be filled in with the number of
   * instances.  The total number of bytes in the buffer is (size *
   * count).  See the explanation in mca_base_modex_send() for why the
   * number of bytes is split into two parts.
   */
  int mca_base_modex_recv(mca_base_module_t *dest_module,
                          lam_proc_t *source_proc,
                          void **buffer, size_t *size, int *count); 

#if defined(c_plusplus) || defined(__cplusplus)
}
#endif

#endif /* MCA_LAM_MODULE_EXCHANGE_H */
