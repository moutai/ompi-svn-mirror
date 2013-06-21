Open MPI 1.6.x with the Cisco usNIC BTL MPI Transport
=====================================================

Overview:
=========

This package is based on the community Open MPI 1.6.x series from
www.open-mpi.org.  It has been augmented to include support for the
Cisco usNIC Linux OS bypass software stack (a specific list of
differences from the community Open MPI release is included below).

Cisco is releasing this package because the Open MPI 1.6.x series is
"stable" and does not accept new features (such as usNIC support).
Cisco anticipates upstreaming the usNIC Open MPI support in future
Open MPI "feature" releases.

See the KNOWN_ISSUES-Cisco.txt file for the known issues with this
release.

Prerequisites:
==============

The Cisco usNIC Open MPI RPM requires that the following RPMs are
installed:

Userspace Libraries:
  1) libusnic_verbs: plugin for the verbs API library
  2) gcc-g++: the GNU G++ compiler (RHEL RPM)
  3) gcc-gfortran: the GNU Fortran compiler (RHEL RPM)

Installing the binary RPM:
==========================

Install the openmpi RPM (requires administrative privileges):

   Red Hat:
   # rpm -Uvh openmpi-<version>-1.x86_64.rpm

Rebuilding the source RPM:
==========================

The source RPM ("SRPM") for the Open MPI distribution is included in
this distribution for those who wish to see, examine, or rebuild the
source themselves.

While only certain Linux distributions are supported, you can
generally rebuild the SRPM via:

    Red Hat:
    # rpmbuild --rebuild openmpi-<version>.src.rpm

Note that the Open MPI SRPM supports several rpmbuild --define options
for altering its build / installation behavior.  For example, you can
choose a different installation prefix instead of the default
/opt/cisco/openmpi location via the "prefix" variable, and to use a
32-way parallel build:

    Red Hat:
    (note the use of single quotes)
    # rpmbuild --rebuild openmpi-<version>.src.rpm \
          --define 'prefix /shared/apps/openmpi' \
          --define 'mflags -j32'

The resulting binary RPM can be installed via the instructions listed
above.

Uninstalling the binary RPM:
============================

Uninstall using (requires administrative privileges):

    Red Hat:
    # rpm -e openmpi

Using Open MPI:
===============

Open MPI has much documentation available online (www.open-mpi.org and
google.com), in man pages, and in its own README file.  

This package is different from the upstream Open MPI 1.6.x releases in
the following ways:

   1) The version number of this package clearly delineates it from an
      upstream community release, and is of the following form:

          <upstream release number>cisco<Cisco release number>

      For example:

          1.6.5cisco1.0.0

      The above version number indicates that this is Cisco version
      1.0.0 of the usNIC Open MPI distribution, and is based on Open
      MPI v1.6.5.  The ompi_info command can be used to show what
      version is being used.

   2) This package includes the "usnic" BTL (MPI point-to-point
      transport).

   3) This package includes two shell scripts that are suitable to be
      sourced from individual user shell startup files, or even placed
      in /etc/profile.d for system-wide usage.  For example, an
      individual user may wish to add the following line to their
      $HOME/.bashrc to add Open MPI to their PATH, LD_LIBRARY_PATH,
      and MANPATH:

          . /opt/cisco/openmpi-vars.sh

      Or, if the user uses a csh-flavored shell, they may wish to add
      the following line to their $HOME/.cshrc:

          source /opt/cisco/openmpi-vars.csh

      Open MPI to the current PATH, LD_LIBRARY_PATH, and MANPATH.
 
   4) This package defaults to setting the "btl" MCA parameter to
      "usnic,sm,self" in the site-wide
      /opt/cisco/openmpi/etc/openmpi-mca-params.conf file.  Hence, all
      MPI runs should default to using usNIC and shared memory
      transports.  Open MPI's usNIC support will use all usNIC devices
      that are reported as PORT_ACTIVE by ibv_devinfo on a given
      server, and will automatically determine which interfaces are
      suitable for reaching remote MPI process peers.  Shared memory
      will be used for same-server MPI communication.

      Note that because of this setting, if the usNIC transport is not
      working for some reason, Open MPI will *not* failover to the TCP
      transport unless specifically directed to (by resetting the btl
      MCA parameter value).

Otherwise, this Open MPI installation should behave just like the
upstream Open MPI releases.

If you need instructions on how to compile and run Open MPI
applications, you should read the Open MPI README file and/or the FAQ
on the Open MPI web site (www.open-mpi.org/faq/).
