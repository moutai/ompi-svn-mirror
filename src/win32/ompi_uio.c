/*
 $HEADER$
 */

#include "ompi_config.h"
#include "win32/ompi_uio.h"
#include <errno.h>

/*
 Highly doubt if the windows sockets ever set errno to EAGAIN. There might
 be some weird conversion to map this or I might have to rewrite this piece
 of code to handle the windows error flags 
 */

int
writev(int fd,struct iovec * iov,int cnt)
{
    int err;
    DWORD sendlen;

	err = WSASend((SOCKET) fd, &(iov->data), cnt, &sendlen, 0, NULL, NULL);

	if (err < 0) {
        return err;
	} else {
	    return (int) sendlen;
	}
} 


int
readv(int fd,struct iovec * iov,int cnt)
{
   int err;
   DWORD recvlen = 0;
   DWORD flags = 0;
   err = WSARecv((SOCKET) fd, &(iov->data), cnt, &recvlen, &flags, NULL, NULL);

   if (err < 0) {
	   return err;
   } else {
	   return (int) recvlen;
   }
} 
