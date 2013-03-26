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

AM_CPPFLAGS = $(btl_usnic_CPPFLAGS) 

dist_pkgdata_DATA = \
    help-mpi-btl-usnic.txt \
    help-ompi-common-verbs.txt

common_sources = \
    common_verbs_basics.c \
    common_verbs_devlist.c \
    common_verbs_find_ports.c \
    common_verbs.h \
    common_verbs_mca.c \
    common_verbs_port.c \
    common_verbs_qp_type.c

opal_sources = \
    opal_hotel.h \
    opal_hotel.c \
    btl_usnic_if.h \
    btl_usnic_if.c

sources = \
    btl_usnic_module.c \
    btl_usnic.h \
    btl_usnic_ack.c \
    btl_usnic_ack.h \
    btl_usnic_component.c \
    btl_usnic_endpoint.c \
    btl_usnic_endpoint.h \
    btl_usnic_frag.c \
    btl_usnic_frag.h \
    btl_usnic_mca.c \
    btl_usnic_proc.c \
    btl_usnic_proc.h \
    btl_usnic_recv.c \
    btl_usnic_recv.h \
    btl_usnic_send.c \
    btl_usnic_send.h \
    btl_usnic_util.c \
    btl_usnic_util.h \
    $(common_sources) \
    $(opal_sources)

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if OMPI_BUILD_btl_usnic_DSO
lib =
lib_sources =
component = mca_btl_usnic.la
component_sources = $(sources)
else
lib = libmca_btl_usnic.la
lib_sources = $(sources)
component =
component_sources =
endif

mcacomponentdir = $(pkglibdir)
mcacomponent_LTLIBRARIES = $(component)
mca_btl_usnic_la_SOURCES = $(component_sources)
mca_btl_usnic_la_LDFLAGS = -module -avoid-version $(btl_usnic_LDFLAGS) 
mca_btl_usnic_la_LIBADD = $(btl_usnic_LIBS)

noinst_LTLIBRARIES = $(lib)
libmca_btl_usnic_la_SOURCES = $(lib_sources)
libmca_btl_usnic_la_LDFLAGS= -module -avoid-version $(btl_usnic_LDFLAGS) 
libmca_btl_usnic_la_LIBADD = $(btl_usnic_LIBS)
