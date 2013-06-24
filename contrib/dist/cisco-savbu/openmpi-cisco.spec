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
# Copyright (c) 2006-2013 Cisco Systems, Inc.  All rights reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#
############################################################################
#
# Copyright (c) 2003, The Regents of the University of California, through
# Lawrence Berkeley National Laboratory (subject to receipt of any
# required approvals from the U.S. Dept. of Energy).  All rights reserved.
#
# Initially written by:
#       Greg Kurtzer, <gmkurtzer@lbl.gov>
#
############################################################################

#############################################################################
#
# Configuration Options
#
# Options that can be passed in via rpmbuild's --define option.  Note
# that --define takes *1* argument: a multi-token string where the first
# token is the name of the variable to define, and all remaining tokens
# are the value.  For example:
#
# shell$ rpmbuild ... --define 'prefix /shared/apps/openmpi' ...
#
# Or (a multi-token example):
#
# shell$ rpmbuild ... \
#    --define 'configure_options CFLAGS=-g --with-openib=/usr/local/ofed' \
#    --define 'mflags -j32' ...
#
#############################################################################
#
# This spec file specifically exports the following configurable options:
#
# configure_options: options passed to Open MPI's configure script.
#     Defaults to LDFLAGS=-Wl,--build-id.
#
# prefix: location to install Open MPI.
#     Defaults to /opt/cisco/openmpi.
#
#############################################################################

%{!?configure_options: %define configure_options LDFLAGS=-Wl,--build-id}

# If the user doesn't --define override "prefix", then use our default
# prefix
%{!?prefix:
%define cisco_dir /opt/cisco
%define ompi_script_dir %{cisco_dir}
%define _prefix %{cisco_dir}/openmpi
}
# If the user *does* --define override "prefix", then use that value
%{?prefix:
%define cisco_dir %{prefix}
%define ompi_script_dir %{_datadir}
%define _prefix %{prefix}
}

# If the builder did not define a version, then default to adding the
# username/time/datestamp.  This identifies customer-made builds.
%{!?ompi_version: %define ompi_version @OMPI_VERSION@.%{expand:%(echo $USER)}_%{expand:%(date +%Y%m%d_%H%M)}}

#############################################################################
#
# These values are based on the prefix (and are not intended to be
# changed by the build user).
#
#############################################################################

# Override libdir so that we end up in lib, not lib64
%define _libdir %{_prefix}/lib

# Override sysconfdir, mandir, and defaultdocdir; otherwise, they end
# up outside the prefix
%define _sysconfdir %{_prefix}/etc
%define _mandir %{_datadir}/man
%define _defaultdocdir %{_prefix}/doc

#############################################################################
#
# Preamble Section
#
#############################################################################

Summary: A powerful implementation of MPI
Name: openmpi
Version: %{ompi_version}
Release: 1
License: BSD
Group: Development/Libraries
Source: openmpi-@OMPI_VERSION@.tar.bz2
Packager: Cisco Systems, Inc.
Vendor: Cisco Systems, Inc.
Distribution: www.cisco.com
Prefix: %{_prefix}
Provides: mpi
Requires: libusnic_verbs gcc-c++ gcc-gfortran

%description
Open MPI is a project combining technologies and resources from
several other projects (FT-MPI, LA-MPI, LAM/MPI, and PACX-MPI) in
order to build the best MPI library available.

This RPM contains the base Open MPI package plus support for the Cisco
usNIC network stack (i.e., the "usnic" BTL plugin).  It is distributed
from www.cisco.com as a convenience until the "usnic" BTL plugin is
incorporated in upstream Open MPI community releases.

#############################################################################
#
# Prepatory Section
#
#############################################################################
%prep
# Unbelievably, some versions of RPM do not first delete the previous
# installation root (e.g., it may have been left over from a prior
# failed build).  This can lead to Badness later if there's files in
# there that are not meant to be packaged.
rm -rf $RPM_BUILD_ROOT

%setup -q -n openmpi-@OMPI_VERSION@

#############################################################################
#
# Build Section
#
#############################################################################

%build
%configure %{configure_options}
%{__make} %{?mflags}


#############################################################################
#
# Install Section
#
#############################################################################

%install
rm -rf $RPM_BUILD_ROOT
%{__make} install DESTDIR=$RPM_BUILD_ROOT %{?mflags_install}

# We've had cases of config.log being left in the installation tree.
# We don't need that in an RPM.
find $RPM_BUILD_ROOT -name config.log -exec rm -f {} \;

cat <<EOF > $RPM_BUILD_ROOT/%{ompi_script_dir}/openmpi-vars.sh
# NOTE: This is an automatically-generated file!  (generated by the
# Cisco Open MPI+usNIC RPM).  Any changes made here will be lost if
# the RPM is uninstalled or upgraded.

# PATH
if test -z "\`echo \$PATH | grep %{_bindir}\`"; then
    PATH=%{_bindir}:\${PATH}
    export PATH
fi

# LD_LIBRARY_PATH
if test -z "\`echo \$LD_LIBRARY_PATH | grep %{_libdir}\`"; then
    LD_LIBRARY_PATH=%{_libdir}\${LD_LIBRARY_PATH:+:}\${LD_LIBRARY_PATH}
    export LD_LIBRARY_PATH
fi

# MANPATH
if test -z "\`echo \$MANPATH | grep %{_mandir}\`"; then
    MANPATH=%{_mandir}:\${MANPATH}
    export MANPATH
fi

# MPI_ROOT
MPI_ROOT=%{_prefix}
export MPI_ROOT
EOF

cat <<EOF > $RPM_BUILD_ROOT/%{ompi_script_dir}/openmpi-vars.csh
# NOTE: This is an automatically-generated file!  (generated by the
# Cisco Open MPI+usNIC RPM).  Any changes made here will be lost if
# the RPM is uninstalled or upgraded.

# path
if ("" == "\`echo \$path | grep %{_bindir}\`") then
    set path=(%{_bindir} \$path)
endif

# LD_LIBRARY_PATH
if ("1" == "\$?LD_LIBRARY_PATH") then
    if ("\$LD_LIBRARY_PATH" !~ *%{_libdir}*) then
        setenv LD_LIBRARY_PATH %{_libdir}:\${LD_LIBRARY_PATH}
    endif
else
    setenv LD_LIBRARY_PATH %{_libdir}
endif

# MANPATH
if ("1" == "\$?MANPATH") then
    if ("\$MANPATH" !~ *%{_mandir}*) then
        setenv MANPATH %{_mandir}:\${MANPATH}
    endif
else
    setenv MANPATH %{_mandir}:
endif

# MPI_ROOT
setenv MPI_ROOT %{_prefix}
EOF

#############################################################################
#
# Clean Section
#
#############################################################################
%clean
# We may be in the directory that we're about to remove, so cd out of
# there before we remove it
cd /tmp

# Remove installed driver after rpm build finished
rm -rf $RPM_BUILD_DIR/openmpi-@OMPI_VERSION@

test "x$RPM_BUILD_ROOT" != "x" && rm -rf $RPM_BUILD_ROOT

#############################################################################
#
# Files Section
#
#############################################################################

#
# Easy; just list the prefix and then specifically call out the doc
# files.
#

%files
%defattr(-, root, root, -)
%{cisco_dir}
%doc README INSTALL LICENSE


#############################################################################
#
# Changelog
#
#############################################################################
%changelog
* Wed May 29 2013 Jeff Squyres <jsquyres@cisco.com>
- Fork from the upstream community specfile to have a simplified Cisco
  Open MPI+usNIC specfile
