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
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

# MCA_pls_cnos_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_pls_cnos_CONFIG],[
    pls_cnos_happy="no"
    # see if we should enable super secret utcp support
    if test "$with_pls_cnos" = "utcp" ; then
        pls_cnos_happy="yes"
    else
        # check for cnos functions
        AC_CHECK_FUNC([cnos_barrier], 
                      [pls_cnos_happy="yes"],
                      [pls_cnos_happy="no"])
    fi

    AC_CHECK_FUNCS([killrank cnos_pm_barrier])

    AS_IF([test "$pls_cnos_happy" = "yes"], [$1], [$2])
])dnl
