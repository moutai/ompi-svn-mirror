dnl -*- shell-script -*-
dnl
dnl $HEADER$
dnl

define(LAM_CXX_FIND_TEMPLATE_PARAMETERS,[
#
# Arguments: none
#
# Dependencies: None
#
# Get the C++ compiler template parameters.
#
# Adds to CXXFLAGS

AC_MSG_CHECKING([for C++ compiler template parameters])
if test "$BASECXX" = "KCC"; then                              
  new_flags="--one_instantiation_per_object"
  CXXFLAGS="$CXXFLAGS $new_flags" 
else
  new_flags="none needed"
fi
AC_MSG_RESULT([$new_flags])

#
# Clean up
#
unset new_flags
])
