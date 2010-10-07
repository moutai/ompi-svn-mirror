# There can be multiple blocks of configuration data, chosen by
# compiler flags (using the compiler_args key to chose which block
# should be activated.  This can be useful for multilib builds.  See the
# multilib page at:
#    https://svn.open-mpi.org/trac/ompi/wiki/compilerwrapper3264 
# for more information.

project=Open MPI
project_short=OMPI
version=@OMPI_VERSION_STRING@
language=C++
compiler_env=CXX
compiler_flags_env=CXXFLAGS
compiler=@CXX@
extra_includes=@OMPI_WRAPPER_EXTRA_INCLUDES@
preprocessor_flags=@OMPI_WRAPPER_EXTRA_CPPFLAGS@
compiler_flags=@OMPI_WRAPPER_EXTRA_CXXFLAGS@
linker_flags=@OMPI_WRAPPER_EXTRA_LDFLAGS@
libs=@OMPI_WRAPPER_CXX_LIB@ @OMPI_WRAPPER_EXTRA_LIBS@
required_file=@OMPI_WRAPPER_CXX_REQUIRED_FILE@
includedir=${includedir}
libdir=${libdir}
