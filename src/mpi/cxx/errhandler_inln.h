// -*- c++ -*-
// 
// Copyright (c) 2004-2005 The Trustees of Indiana University.
//                         All rights reserved.
// Copyright (c) 2004-2005 The Trustees of the University of Tennessee.
//                         All rights reserved.
// $COPYRIGHT$
// 
// Additional copyrights may follow
// 
// $HEADER$
//

#if 0 /* OMPI_ENABLE_MPI_PROFILING */

inline PMPI::Errhandler::Errhandler(const PMPI::Errhandler& e)
  : handler_fn(e.handler_fn), mpi_errhandler(e.mpi_errhandler) { }

inline PMPI::Errhandler&
PMPI::Errhandler::operator=(const PMPI::Errhandler& e)
{
  handler_fn = e.handler_fn;
  mpi_errhandler = e.mpi_errhandler;
  return *this;
}

inline bool
PMPI::Errhandler::operator==(const PMPI::Errhandler &a)
{
  return (MPI2CPP_BOOL_T)(mpi_errhandler == a.mpi_errhandler);
}

#endif

inline void
MPI::Errhandler::Free()
{
  (void)MPI_Errhandler_free(&mpi_errhandler);
}




