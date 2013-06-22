#!/usr/bin/env perl

use strict;

use Getopt::Long;
use Data::Dumper;

my $tmpdir = "/tmp/usnic-qa-git-check.$$";

#--------------------------------------------------------------------------

my $build_arg;
my $help_arg;
my $debug_arg;

&Getopt::Long::Configure("bundling");
my $ok = Getopt::Long::GetOptions("build=s" => \$build_arg,
                                  "debug!" => \$debug_arg,
                                  "help|h" => \$help_arg);

my $ret = 0;
if (!$ok || 
    (!defined($build_arg) && !$help_arg)) {
    $help_arg = 1;
    $ret = 1;
}
if ($help_arg) {
    print "$0 --build=BUILD_ID

--build=BUILD_ID Build ID to check (e.g., 30)
--help           This message\n";
    exit($ret);
}

#--------------------------------------------------------------------------

sub cleanup_and_exit {
    my ($status, $msg) = @_;

    print $msg
        if (defined($msg));
    system("rm -rf $tmpdir")
        if (-d $tmpdir);
    exit($status);
}

#--------------------------------------------------------------------------

# run a command and save the stdout / stderr
sub do_command {
    my ($cmd) = @_;

    print "*** Running command: $cmd\n" if ($debug_arg);
    pipe OUTread, OUTwrite;

    # Child

    my $pid;
    if (($pid = fork()) == 0) {
        close OUTread;

        close(STDERR);
        open STDERR, ">&OUTwrite"
            || cleanup_and_exit(1, "Can't redirect stderr\n");
        select STDERR;
        $| = 1;

        close(STDOUT);
        open STDOUT, ">&OUTwrite"
            || cleanup_and_exit(1, "Can't redirect stdout\n");
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
            cleanup_and_exit(1, "Can't execute command: $cmd\n");
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
                print "OUT:$data" if ($debug_arg);
            }
        }
    }
    close OUTerr;

    # The pipes are closed, so the process should be dead.  Reap it.

    waitpid($pid, 0);
    my $status = $?;
    print "*** Command complete, exit status: $status\n" if ($debug_arg);

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
        cleanup_and_exit(1);
    }

    return $ret;
}

#--------------------------------------------------------------------------

# Run ompi_info; extract its git hash.  
my $ompi_git_hash;
$ret = do_command("ompi_info --parsable");
foreach my $line (@{$ret->{stdout}}) {
    chomp($line);
    # Future proof the script a bit: handle both the v1.6 series
    # output and the v1.7 series/beyond output.
    if ($line =~ m/^ompi:version:svn:git_(.+)$/ ||
        $line =~ m/^ompi:version:repo:git_(.+)$/) {
        $ompi_git_hash = $1;
        last;
    }
}
cleanup_and_exit(1, "Could not find ompi_info git version -- is the right OMPI install in your PATH?\n")
    if (!defined($ompi_git_hash));
print "==> Found OMPI git hash: $ompi_git_hash\n";

# Get a git checkout
print "==> Getting a git checkout of ompi-usnic...\n";
do_command("mkdir $tmpdir");

chdir($tmpdir);
do_command("git clone ssh://savbugit\@savbu-sjc-svn.cisco.com/ompi-usnic");

# Find the git tag correspondin to the build ID we want
chdir("ompi-usnic");
open(IN, "git tag|");
my $tag;
while (<IN>) {
    if (/^usNIC_1_0_0_${build_arg}_/) {
        chomp();
        $tag = $_;
        last;
    }
}
close(IN);
cleanup_and_exit(1, "Could not find a git tag corresopnding to build ID $build_arg\n")
    if (!defined($tag));
print "==> Found git tag for build ID $build_arg: $tag\n";

# Find the git hash corresponding to the commit corresponding to this
# git tag
$ret = do_command("git cat-file -p $tag");
$ret->{stdout}[0] =~ m/^object ([0-9a-f]{40})$/;
my $git_hash = $1;
print "==> Found git hash: $git_hash\n";

# Compare the 2 git hashes
print "==> Comparing:
     Git:  $git_hash
     OMPI: $ompi_git_hash\n";
cleanup_and_exit(0, "Git hashes match: happiness!\n")
    if (substr($git_hash, 0, length($ompi_git_hash)) eq $ompi_git_hash);
cleanup_and_exit(1, "Git hashes do NOT match: sadness\n")
