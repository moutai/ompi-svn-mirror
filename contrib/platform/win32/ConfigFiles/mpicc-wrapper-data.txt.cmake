# There can be multiple blocks of configuration data, chosen by
# compiler flags (using the compiler_args key to chose which block
# should be activated.  This can be useful for multilib builds.  See the
# multilib page at:
#    https://svn.open-mpi.org/trac/ompi/wiki/compilerwrapper3264 
# for more information.

project=Open MPI
project_short=OMPI
version=@OMPI_VERSION_STRING@
language=C
compiler_env=CC
compiler_flags_env=CFLAGS
compiler=@CC@
extra_includes=@OMPI_WRAPPER_EXTRA_INCLUDES@
preprocessor_flags=@OMPI_WRAPPER_EXTRA_CPPFLAGS@
compiler_flags=@OMPI_WRAPPER_EXTRA_CFLAGS@
linker_flags=@OMPI_WRAPPER_EXTRA_LDFLAGS@
libs=libmpi.lib libopen-rte.lib libopen-pal.lib @OMPI_WRAPPER_EXTRA_LIBS@
required_file=
includedir=${includedir}
libdir=${libdir}
