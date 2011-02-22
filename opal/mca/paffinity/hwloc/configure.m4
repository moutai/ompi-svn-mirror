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
# Copyright (c) 2007-2010 Cisco Systems, Inc. All rights reserved.
#
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

# MCA_paffinity_hwloc_CONFIG([action-if-found], [action-if-not-found])
# --------------------------------------------------------------------
AC_DEFUN([MCA_opal_paffinity_hwloc_CONFIG],[
    AC_CONFIG_FILES([opal/mca/paffinity/hwloc/Makefile])

    # All we check for is the results of opal/mca/common/hwloc's
    # configury
    AC_MSG_CHECKING([if common hwloc was happy])
    AC_MSG_RESULT([$opal_common_hwloc_support])

    AS_IF([test "$opal_common_hwloc_support" = "yes"], [$1], [$2])
])dnl
