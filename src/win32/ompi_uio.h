/*
 * $HEADER$
 */

#ifndef OMPI_UIO_H
#define OMPI_UIO_H

#include "ompi_declspec.h"
#include <winsock2.h>
#include <ws2tcpip.h>

/* define the iovec structure */
struct iovec {
  WSABUF data;
};
#define iov_base data.buf
#define iov_len data.len

#if defined(c_plusplus) || defined (__cplusplus)
extern "C" {
#endif
/*
 * writev:
   writev  writes  data  to  file  descriptor  fd,  and  from  the buffers
   described by iov. The number of buffers is specified by  cnt.  The
   buffers  are  used  in  the  order specified.  Operates just like write
   except that data is taken from iov instead of a contiguous buffer.
 */
OMPI_DECLSPEC int writev (int fd, struct iovec *iov, int cnt);

/* 
   readv  reads  data  from file descriptor fd, and puts the result in the
   buffers described by iov. The number  of  buffers  is  specified  by
   cnt.  The  buffers  are filled in the order specified.  Operates just
   like read except that data is put in iov  instead  of  a  contiguous
   buffer.
 */
OMPI_DECLSPEC int readv (int fd, struct iovec *iov, int cnt);
   
#if defined(c_plusplus) || defined (__cplusplus)
}
#endif

#endif /* OMPI_UIO_H */
