/*
 * $HEADER$
 * 
 * $Id: mpi.h,v 1.4 2004/01/07 07:53:25 jsquyres Exp $
 */

#ifndef LAM_MPI_H
#define LAM_MPI_H

#define MPI_SUCCESS 0
#define MPI_MAX_OBJECT_NAME 64

#define LAM_MPI 1

typedef struct _lam_communicator *MPI_Comm;
typedef struct _lam_group *MPI_Group;
typedef struct _lam_datatype *MPI_Datatype;

extern MPI_Comm MPI_COMM_NULL;
extern MPI_Comm MPI_COMM_WORLD;
extern MPI_Comm MPI_COMM_SELF;

extern MPI_Datatype MPI_TYPE_NULL;


#endif /* LAM_MPI_H */
