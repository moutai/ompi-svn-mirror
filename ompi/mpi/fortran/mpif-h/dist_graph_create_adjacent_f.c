/*
 * Copyright (c) 2011      The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2011      INRIA.  All rights reserved.
 * Copyright (c) 2011      Université Bordeaux 1
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "ompi_config.h"

#include "ompi/mpi/fortran/mpif-h/bindings.h"
#include "ompi/mpi/fortran/base/constants.h"

#if OPAL_HAVE_WEAK_SYMBOLS && OMPI_PROFILE_LAYER
#pragma weak PMPI_DIST_GRAPH_CREATE_ADJACENT = ompi_dist_graph_create_adjacent_f
#pragma weak pmpi_dist_graph_create_adjacent = ompi_dist_graph_create_adjacent_f
#pragma weak pmpi_dist_graph_create_adjacent_ = ompi_dist_graph_create_adjacent_f
#pragma weak pmpi_dist_graph_create_adjacent__ = ompi_dist_graph_create_adjacent_f

#pragma weak PMPI_Dist_graph_create_adjacent_f = ompi_dist_graph_create_adjacent_f
#pragma weak PMPI_Dist_graph_create_adjacent_f08 = ompi_dist_graph_create_adjacent_f
#elif OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (PMPI_DIST_GRAPH_CREATE_ADJACENT,
                            pmpi_dist_graph_create_adjacent,
                            pmpi_dist_graph_create_adjacent_,
                            pmpi_dist_graph_create_adjacent__,
                            pompi_dist_graph_create_adjacent_f,
                            (MPI_Fint *comm_old, MPI_Fint *indegree,  MPI_Fint *sources, MPI_Fint *sourceweights, MPI_Fint *outdegree,  MPI_Fint *destinations, MPI_Fint *destweights, MPI_Fint *info, ompi_fortran_logical_t *reorder, MPI_Fint *comm_graph, MPI_Fint *ierr),
                            (comm_old, indegree, sources, sourceweights, outdegree, destinations, destweights, info, reorder, comm_graph, ierr) )
#endif

#if OPAL_HAVE_WEAK_SYMBOLS
#pragma weak MPI_DIST_GRAPH_CREATE_ADJACENT = ompi_dist_graph_create_adjacent_f
#pragma weak mpi_dist_graph_create_adjacent = ompi_dist_graph_create_adjacent_f
#pragma weak mpi_dist_graph_create_adjacent_ = ompi_dist_graph_create_adjacent_f
#pragma weak mpi_dist_graph_create_adjacent__ = ompi_dist_graph_create_adjacent_f

#pragma weak MPI_Dist_graph_create_adjacent_f = ompi_dist_graph_create_adjacent_f
#pragma weak MPI_Dist_graph_create_adjacent_f08 = ompi_dist_graph_create_adjacent_f
#endif

#if ! OPAL_HAVE_WEAK_SYMBOLS && ! OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (MPI_DIST_GRAPH_CREATE_ADJACENT,
                            mpi_dist_graph_create_adjacent,
                            mpi_dist_graph_create_adjacent_,
                            mpi_dist_graph_create_adjacent__,
                            ompi_dist_graph_create_adjacent_f,
                            (MPI_Fint *comm_old, MPI_Fint *indegree,  MPI_Fint *sources, MPI_Fint *sourceweights, MPI_Fint *outdegree,  MPI_Fint *destinations, MPI_Fint *destweights, MPI_Fint *info, ompi_fortran_logical_t *reorder, MPI_Fint *comm_graph, MPI_Fint *ierr),
                            (comm_old, indegree,  sources, sourceweights, outdegree, destinations, destweights, info, reorder, comm_graph, ierr) )
#endif

#if OMPI_PROFILE_LAYER && ! OPAL_HAVE_WEAK_SYMBOLS
#include "ompi/mpi/fortran/mpif-h/profile/defines.h"
#endif

static const char FUNC_NAME[] = "MPI_DIST_GRAPH_CREATE_ADJACENT";

void ompi_dist_graph_create_adjacent_f(MPI_Fint *comm_old, MPI_Fint *indegree,
                                       MPI_Fint *sources, MPI_Fint *sourceweights,
                                       MPI_Fint *outdegree,
                                       MPI_Fint *destinations, MPI_Fint *destweights, MPI_Fint *info,
                                       ompi_fortran_logical_t *reorder, MPI_Fint *comm_graph,
                                       MPI_Fint *ierr)
{
    MPI_Info c_info;
    MPI_Comm c_comm_old, c_comm_graph;
    OMPI_ARRAY_NAME_DECL(sources);
    OMPI_ARRAY_NAME_DECL(sourceweights);
    OMPI_ARRAY_NAME_DECL(destinations);
    OMPI_ARRAY_NAME_DECL(destweights);

    c_comm_old = MPI_Comm_f2c(*comm_old);
    c_info = MPI_Info_f2c(*info);

    OMPI_ARRAY_FINT_2_INT(sources, *indegree);
    if( !OMPI_IS_FORTRAN_UNWEIGHTED(sourceweights) ) {
        OMPI_ARRAY_FINT_2_INT(sourceweights, *indegree);
    }
    OMPI_ARRAY_FINT_2_INT(destinations, *outdegree);
    if( !OMPI_IS_FORTRAN_UNWEIGHTED(destweights) ) {
        OMPI_ARRAY_FINT_2_INT(destweights, *outdegree);
    }

    /* Number of edges is equal to the last entry in the index array */
    OMPI_ARRAY_FINT_2_INT(edges, index[*nnodes - 1]);

    *ierr = OMPI_INT_2_FINT(MPI_Dist_graph_create_adjacent(c_comm_old, OMPI_FINT_2_INT(*indegree),
                                                           OMPI_ARRAY_NAME_CONVERT(sources),
                                                           OMPI_IS_FORTRAN_UNWEIGHTED(sourceweights) ? MPI_UNWEIGHTED : OMPI_ARRAY_NAME_CONVERT(sourceweights),
                                                           OMPI_FINT_2_INT(*outdegree),
                                                           OMPI_ARRAY_NAME_CONVERT(destinations),
                                                           OMPI_IS_FORTRAN_UNWEIGHTED(destweights) ? MPI_UNWEIGHTED : OMPI_ARRAY_NAME_CONVERT(destweights),
                                                           c_info, 
                                                           OMPI_LOGICAL_2_INT(*reorder),
                                                           &c_comm_graph));
    if (OMPI_SUCCESS == OMPI_FINT_2_INT(*ierr)) {
        *comm_graph = MPI_Comm_c2f(c_comm_graph);
    }

    OMPI_ARRAY_FINT_2_INT_CLEANUP(sources);
    if( !OMPI_IS_FORTRAN_UNWEIGHTED(sourceweights) ) {
        OMPI_ARRAY_FINT_2_INT_CLEANUP(sourceweights);
    }
    OMPI_ARRAY_FINT_2_INT_CLEANUP(destinations);
    if( !OMPI_IS_FORTRAN_UNWEIGHTED(destweights) ) {
        OMPI_ARRAY_FINT_2_INT_CLEANUP(destweights);
    }
}
