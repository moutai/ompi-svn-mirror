# -*- shell-script -*-
#
# Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
#                         University Research and Technology
#                         Corporation.  All rights reserved.
# Copyright (c) 2004-2005 The University of Tennessee and The University
#                         of Tennessee Research Foundation.  All rights
#                         reserved.
# Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
#                         University of Stuttgart.  All rights reserved.
# Copyright (c) 2004-2005 The Regents of the University of California.
#                         All rights reserved.
# Copyright (c) 2006      Sandia National Laboratories. All rights
#                         reserved.
# Copyright (c) 2010-2012 Cisco Systems, Inc.  All rights reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#


# MCA_btl_usnic_CONFIG([action-if-can-compile], 
#                      [action-if-cant-compile])
# ------------------------------------------------
AC_DEFUN([MCA_btl_usnic_CONFIG],[
    OMPI_CHECK_OPENIB([btl_usnic],
                        [btl_usnic_happy="yes"],
                        [btl_usnic_happy="no"])

    # Do we have the IBV_TRANSPORT_USNIC / IBV_NODE_USNIC defines?
    # (note: if we have one, we have both)
    btl_usnic_have_ibv_usnic=0
    AS_IF([test "$btl_usnic_happy" = "yes"],
          [AC_MSG_CHECKING([for IBV_NODE_USNIC/IBV_TRANSPORT_USNIC])
           AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[ #include <infiniband/verbs.h>
]],
[[int i = IBV_NODE_USNIC;]])],
                             [msg=yes btl_usnic_have_ibv_usnic=1],
                             [msg=no])
           AC_MSG_RESULT([$msg])]
    )
    AC_DEFINE_UNQUOTED([BTL_USNIC_HAVE_IBV_USNIC], 
                       [$btl_usnic_have_ibv_usnic],
                       [Whether we have IBV_NODE_USNIC / IBV_TRANSPORT_USNIC or not])

    AS_IF([test "$btl_usnic_happy" = "yes"],
          [btl_usnic_WRAPPER_EXTRA_LDFLAGS="$btl_usnic_LDFLAGS"
           btl_usnic_WRAPPER_EXTRA_LIBS="$btl_usnic_LIBS"
           $1],
          [$2])

    # Substitute in the things needed to build USNIC
    AC_SUBST([btl_usnic_CPPFLAGS])
    AC_SUBST([btl_usnic_LDFLAGS])
    AC_SUBST([btl_usnic_LIBS])
])dnl