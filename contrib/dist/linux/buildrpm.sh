#!/bin/sh -f
#
# Copyright (c) 2004-2006 The Trustees of Indiana University and Indiana
#                         University Research and Technology
#                         Corporation.  All rights reserved.
# Copyright (c) 2006      Cisco Systems, Inc.  All rights reserved.
# 

#
# General config vars
#

prefix="/opt/openmpi"
specfile="openmpi.spec"
rpmbuild_options="--define 'mflags -j4'"
# Another option: --target=i686-pc-gnu

# Some distro's will attempt to force using bizarre, custom compiler
# names (e.g., i386-redhat-linux-gnu-gcc).  So hardwire them to use
# "normal" names.
#export CC=gcc
#export CXX=g++
#export F77=f77
#export FC=

# Note that this script can build one or all of the following RPMs:
# SRPM, all-in-one, multiple.

# If you want to build the SRPM, put "yes" here
build_srpm=yes
# If you want to build the "all in one RPM", put "yes" here
build_single=no
# If you want to build the "multiple" RPMs, put "yes" here
build_multiple=yes

#########################################################################
# You should not need to change anything below this line
#########################################################################

#
# get the tarball name
#

tarball="$1"
if test "$tarball" = ""; then
    echo "Usage: buildrpm.sh <tarball>"
    exit 1
fi
if test ! -f $tarball; then
    echo "Can't find $tarball"
    exit 1
fi
echo "--> Found tarball: $tarball"

#
# get the extension from the tarball (gz or bz2)
#

extension=`echo $tarball | egrep '\.bz2'`
if test -n "$extension"; then
    extension=bz2
else
    extension=gz
fi

#
# Get the version number
#

first="`basename $tarball | cut -d- -f2`"
version="`echo $first | sed -e 's/\.tar\.'$extension'//'`"
unset first
echo "--> Found Open MPI version: $version"

#
# do we have the spec files?
#

if test ! -f $specfile; then
    echo "can't find $specfile"
    exit 1
fi
echo "--> Found specfile: $specfile"

#
# Find where the top RPM-building directory is
#

rpmtopdir="`grep %_topdir $HOME/.rpmmacros | awk '{ print $2 }'`"
if test "$rpmtopdir" != ""; then
    if test ! -d "$rpmtopdir"; then
	mkdir -p "$rpmtopdir"
	mkdir -p "$rpmtopdir/BUILD"
	mkdir -p "$rpmtopdir/RPMS"
	mkdir -p "$rpmtopdir/RPMS/i386"
	mkdir -p "$rpmtopdir/RPMS/i586"
	mkdir -p "$rpmtopdir/RPMS/i686"
	mkdir -p "$rpmtopdir/RPMS/noarch"
	mkdir -p "$rpmtopdir/RPMS/athlon"
	mkdir -p "$rpmtopdir/SOURCES"
	mkdir -p "$rpmtopdir/SPECS"
	mkdir -p "$rpmtopdir/SRPMS"
    fi
    need_root=0
elif test -d /usr/src/RPM; then
    need_root=1
    rpmtopdir="/usr/src/RPM"
else
    need_root=1
    rpmtopdir="/usr/src/redhat"
fi
echo "--> Found RPM top dir: $rpmtopdir"

#
# If we're not root, try to sudo
#

if test "$need_root" = "1" -a "`whoami`" != "root"; then
    echo "--> Trying to sudo: \"$0 $*\""
    echo "------------------------------------------------------------"
    sudo -u root sh -c "$0 $tarball"
    echo "------------------------------------------------------------"
    echo "--> sudo finished"
    exit 0
fi

#
# make sure we have write access to the directories we need
#

if test ! -w $rpmtopdir/SOURCES ; then
    echo "Problem creating rpms: You do not have a $rpmtopdir directory"
    echo "tree or you do not have write access to the $rpmtopdir directory"
    echo "tree.  Please remedy and try again."
    exit 1
fi
echo "--> Have write access to $rpmtopdir/SOURCES"

#
# move the tarball file to the rpm directory
#

cp $tarball $rpmtopdir/SOURCES

#
# Print out the compilers
#

cat <<EOF
--> Hard-wired for compilers:
    CC = $CC
    CXX = $CXX
    F77 = $F77
    FC = $FC
EOF

#
# what command should we use?
# RH 8.0 changed from using "rpm -ba" to "rpmbuild -ba".  ARRGGH!!!
#

which rpmbuild 2>&1 >/dev/null
if test "$?" = "0"; then
    rpm_cmd="rpmbuild"
else
    rpm_cmd="rpm"
fi


#
# from the specfile
#

specdest="$rpmtopdir/SPECS/openmpi-$version.spec"
sed -e 's/\$VERSION/'$version'/g' \
    -e 's/\$EXTENSION/'$extension'/g' \
    $specfile > "$specdest"
echo "--> Created destination specfile: $specdest"
release=`egrep -i release: $specdest | cut -d\  -f2`

#
# Setup compiler string
#

configure_options="CC=$CC CXX=$CXX FC=$FC F77=$F77"

#
# Make the SRPM
#

if test "$build_srpm" = "yes"; then
    echo "--> Building the Open MPI SRPM"
    cmd="$rpm_cmd -bs $specdest"
    echo "--> $cmd"
    eval $cmd

    if test $? != 0; then
        echo "*** FAILURE BUILDING SRPM!"
        echo "Aborting"
        exit 1
    fi
    echo "--> Done building the SRPM"
fi

#
# Make the single RPM
#

if test "$build_single" = "yes"; then
    echo "--> Building the single Open MPI RPM"
    cmd="$rpm_cmd -bb $rpmbuild_options --define 'build_all_in_one_rpm 1' --define 'configure_options $configure_options' $specdest"
    echo "--> $cmd"
    eval $cmd

    if test $? != 0; then
        echo "*** FAILURE BUILDING SINGLE RPM!"
        echo "Aborting"
        exit 1
    fi
    echo "--> Done building the single RPM"
fi

#
# Make the multi RPM
#

if test "$build_multiple" = "yes"; then
    echo "--> Building the multiple Open MPI RPM"
    cmd="$rpm_cmd -bb $rpmbuild_options --define 'build_all_in_one_rpm 0' --define 'configure_options $configure_options' $specdest"
    echo "--> $cmd"
    eval $cmd

    if test $? != 0; then
        echo "*** FAILURE BUILDING MULTIPLE RPM!"
        echo "Aborting"
        exit 1
    fi
    echo "--> Done building the multiple RPM"
fi

#
# Done
#

cat <<EOF

------------------------------------------------------------------------------
====                FINISHED BUILDING Open MPI RPM                        ====
------------------------------------------------------------------------------
A copy of the tarball is located in: $rpmtopdir/SOURCES/
The completed rpms are located in:   $rpmtopdir/RPMS/i<something>86/
The sources rpms are located in:     $rpmtopdir/SRPMS/
The spec files are located in:       $rpmtopdir/SPECS/
------------------------------------------------------------------------------

EOF
