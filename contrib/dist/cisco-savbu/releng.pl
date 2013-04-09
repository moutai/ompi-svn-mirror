#!/usr/bin/env perl

use strict;

use Cwd;
use Data::Dumper;

#--------------------------------------------------------------------------

my $debug = 0;

#--------------------------------------------------------------------------

# run a command and save the stdout / stderr
sub do_command {
    my ($cmd) = @_;

    print "*** Running command: $cmd\n" if ($debug);
    pipe OUTread, OUTwrite;

    # Child

    my $pid;
    if (($pid = fork()) == 0) {
        close OUTread;

        close(STDERR);
        open STDERR, ">&OUTwrite"
            || die "Can't redirect stderr\n";
        select STDERR;
        $| = 1;

        close(STDOUT);
        open STDOUT, ">&OUTwrite"
            || die "Can't redirect stdout\n";
        select STDOUT;
        $| = 1;

        # Turn shell-quoted words ("foo bar baz") into individual tokens

        my @tokens;
        while ($cmd =~ /\".*\"/) {
            my $prefix;
            my $middle;
            my $suffix;
            
            $cmd =~ /(.*?)\"(.*?)\"(.*)/;
            $prefix = $1;
            $middle = $2;
            $suffix = $3;
            
            if ($prefix) {
                foreach my $token (split(' ', $prefix)) {
                    push(@tokens, $token);
                }
            }
            if ($middle) {
                push(@tokens, $middle);
            } else {
                push(@tokens, "");
            }
            $cmd = $suffix;
        }
        if ($cmd) {
            push(@tokens, split(' ', $cmd));
        }

        # Run it!

        exec(@tokens) ||
            die "Can't execute command: $cmd\n";
    }
    close OUTwrite;

    # Parent

    my (@out, @err);
    my ($rin, $rout);
    my $done = 1;

    # Keep watching over the pipe(s)

    $rin = '';
    vec($rin, fileno(OUTread), 1) = 1;

    while ($done > 0) {
        my $nfound = select($rout = $rin, undef, undef, undef);
        if (vec($rout, fileno(OUTread), 1) == 1) {
            my $data = <OUTread>;
            if (!defined($data)) {
                vec($rin, fileno(OUTread), 1) = 0;
                --$done;
            } else {
                push(@out, $data);
                print "OUT:$data" if ($debug);
            }
        }
    }
    close OUTerr;

    # The pipes are closed, so the process should be dead.  Reap it.

    waitpid($pid, 0);
    my $status = $?;
    print "*** Command complete, exit status: $status\n" if ($debug);

    # Return an anonymous hash containing the relevant data

    my $ret = {
        stdout => \@out,
        status => $status
        };


    # If we failed, just die
    if ($ret->{status} != 0) {
        print "=== Failed to $cmd\n";
        print "=== Last few lines of stdout/stderr:\n";
        my $i = $#{$ret->{stdout}} - 500;
        $i = 0
            if ($i < 0);
        while ($i <= $#{$ret->{stdout}}) {
            print $ret->{stdout}[$i];
            ++$i;
        }
        exit(1);
    }

    return $ret;
}

#--------------------------------------------------------------------------

# Sanity check
die "Must be run from the top-level Open MPI directory"
    if (! -d "ompi" || ! -d "orte" || ! -d "opal");
die "Must be from a Cisco/USNIC Open MPI checkout"
    if (! -d ".git" || ! -d "ompi/mca/btl/usnic");

# Ensure we don't have any whacky version of gcc in the path.  Unload
# all modules and re-run.
if (exists($ENV{LOADEDMODULES}) &&
    $ENV{LOADEDMODULES} =~ /gcc/) {
    print "=== Unloading a bunch of environment modules and re-running...\n";
    sleep(1);
    my $ret = system(". /etc/profile.d/modules.sh ; module unload gcc cisco/gcc cisco/intel-compilers cisco/pgi-compilers cisco/clang-compilers; $0");
    exit($ret);
}

#--------------------------------------------------------------------------

# Set the version number in VERSION
do_command("git checkout VERSION");
open(V, "VERSION")
    || die "Can't open VERSION";
my $version;
$version .= $_
    while (<V>);
close(V);
# We need to set want_svn to 0
$version =~ s/^want_svn=\d+$/want_svn=0/m;
# Set the build number
# JMS write me
my $build_id = 1;
$version =~ s/^greek=.*$/greek=cisco$build_id/m;
open(V, ">VERSION");
print V $version;
close(V);

# Get the version we're building
my $version = `./config/ompi_get_version.sh VERSION`;
chomp($version);
print "=== Making version $version\n";

# Autogen, configure, make dist.  Note that Open MPI changed from
# autogen.sh to autogen.pl after the v1.6 series; so future-proof this
# script a little and search for .pl first.
my $a = "autogen.sh";
$a = "autogen.pl"
    if (-x "autogen.pl");
print "=== Running $a...\n";
#do_command("./$a");

print "=== Running configure...\n";
#do_command("./configure --enable-dist");

print "=== Running make dist...\n";
#do_command("make dist");

# Make the SRPM.  Use our own $HOME/.rpmmacros file.
print "=== Creating RPMS:\n";
my $prev_rpmmacros;
if (-f "$ENV{HOME}/.rpmmacros") {
    $prev_rpmmacros = "$ENV{HOME}/.rpmmacros-ompi-usnic-build-save";
    unlink($prev_rpmmacros);
    do_command("mv $ENV{HOME}/.rpmmacros $prev_rpmmacros");
}
open(M, ">$ENV{HOME}/.rpmmacros") 
    || die "Can't open $ENV{HOME}/.rpmmacros";
print M "%_topdir $ENV{HOME}/RPMS
%_vendorinfo Cisco Systems, Inc.
%_packager Cisco Systems, Inc.
%_distribution www.cisco.com\n";
close(M);

# Make $HOME/RPMS tree if it doesn't already exist
mkdir("$ENV{HOME}/RPMS");
mkdir("$ENV{HOME}/RPMS/BUILD");
mkdir("$ENV{HOME}/RPMS/BUILDROOT");
mkdir("$ENV{HOME}/RPMS/RPMS");
mkdir("$ENV{HOME}/RPMS/SOURCES");
mkdir("$ENV{HOME}/RPMS/SPECS");
mkdir("$ENV{HOME}/RPMS/SRPMS");

# Copy the files to the right places
do_command("cp openmpi-$version.tar.bz2 $ENV{HOME}/RPMS/SOURCES");

# Preprocess the specfile and put it in the right place
open(SPEC, "contrib/dist/linux/openmpi.spec");
my $specfile;
$specfile .= $_
    while (<SPEC>);
close(SPEC);
$specfile =~ s/\$VERSION/$version/g;
$specfile =~ s/\$EXTENSION/bz2/g;
my $specdest = "$ENV{HOME}/RPMS/SPECS/openmpi.spec";
unlink($specdest);
open(SPEC, ">$specdest")
    || die "Can't open $specdest";
print SPEC $specfile;
close(SPEC);

# Build the SRPM
my $rpmbuild_options;
print "  = Building SRPM\n";
do_command("rpmbuild $rpmbuild_options -bs $specdest");

# Build the RPMs.  Ensure to fail if we don't have usnic support.
# Note: in Open MPI v1.6.x, we use --with-openib.  With later
# versions, it's --with-verbs.  So future-proof this script a little.
my $config_options = "--with-openib";
open(C, "./configure --help|")
    || die "Can't open configure --help";
my $out;
$out .= $_
    while (<C>);
close(C);
$config_options = "--with-verbs"
    if ($out =~ /--with-verbs/);

my $rpmbuild_options;
$rpmbuild_options = "--define \"install_in_opt 1\"";
$rpmbuild_options = "$rpmbuild_options --define \"mflags -j32\"";
$rpmbuild_options = "$rpmbuild_options --define \"configure_options $config_options --disable-vt --disable-mpi-fortran --disable-mpi-cxx --enable-mpi-ext=all LDFLAGS=-Wl,--build-id\"";
$rpmbuild_options = "$rpmbuild_options --define \"build_debuginfo_rpm 1\"";

print "  = Building RPM\n";
do_command("rpmbuild $rpmbuild_options -bb $specdest");

# Restore .rpmmacros
do_command("mv $prev_rpmmacros $ENV{HOME}/.rpmmacros")
    if (defined($prev_rpmmacros));

# Restore VERSION
do_command("git checkout VERSION");

# Success
print "=== Success:\n";
my $pwd = getcwd();
print "  = Tarballs in $pwd:\n";
system("ls -1 openmpi*$version*");

my $srpmdir = "$ENV{HOME}/RPMS/SRPMS";
print "  = SRPM in $srpmdir:\n";
chdir($srpmdir);
system("ls -1 openmpi-$version-*.src.rpm");

my $rpmdir = "$ENV{HOME}/RPMS/RPMS/x86_64";
print "  = RPMs in $rpmdir:\n";
chdir($rpmdir);
system("ls -1 openmpi*$version-*.rpm");

# All done
exit(0);
