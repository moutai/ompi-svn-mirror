#!/usr/bin/env perl
#
# Copyright (c) 2008-2010 Cisco Systems, Inc.  All rights reserved.
#
# Dumb script to run through all the svn:ignore's in the tree and
# build build .gitignore files for Git.  

use strict;

# Sanity check
die "Not in Git+SVN repository top dir"
    if (! -d ".git" && ! -d ".svn");

# Put in some specials that we ignore everywhere
my @globals = qw/.libs
.deps
*.la
*.lo
*.o
*.so
*.a
.dirstamp
*.dSYM
*.S
*.loT
*.orig
*.rej
*.class
*.xcscheme
*.plist
*~
*\\\#/;
unshift(@globals, "# Automatically generated by build-gitignore.pl; edits may be lost!");

my $debug;
$debug = 1
    if ($ARGV[0]);

print "Thinking...\n"
    if (!$debug);

# Start at the top level
process(".");

# Done!
exit(0);

#######################################################################

# DFS-oriented recursive directory search
sub process {
    my $dir = shift;

    # Look at the svn:ignore property for this directory
    my $svn_ignore = `svn pg svn:ignore $dir 2> /dev/null`;
    # If svn failed, bail on this directory.
    return
        if ($? != 0);

    chomp($svn_ignore);
    if ($svn_ignore ne "") {
        print "Found svn:ignore in $dir\n"
            if ($debug);

        my @git = @globals;

        # See if there's an .gitignore_local file.  If so, add its
        # contents to the end.
        if (-f "$dir/.gitignore_local") {
            open(IN, "$dir/.gitignore_local") || die "Can't open .gitignore_local";
            while (<IN>) {
                chomp;
                push(@git, $_);
            }
            
            close(IN);
        }

        # Now read the svn:ignore value
        foreach my $line (split(/\n/, $svn_ignore)) {
            chomp($line);
            $line =~ s/^\.\///;
            next
                if ($line eq "");

            # Ensure not to ignore special git files
            next
                if ($line eq ".gitignore" || $line eq ".gitignore_local" ||
                    $line eq ".git" || $line eq ".svn");
            # We're globally ignoring some specials already; we can
            # skip those
            my $skip = 0;
            foreach my $g (@globals) {
                if ($g eq $line) {
                    $skip = 1;
                    last;
                }
            }
            next 
                if ($skip);

            push(@git, "$line");
        }

        # Write out a new .gitignore file
        unlink("$dir/.gitignore");
        open(OUT, ">$dir/.gitignore") || die "Can't open .gitignore file";
        foreach my $val (@git) {
            print OUT "$val\n";
        }

        # Ignore the .svn dir if that directory exists
        print OUT ".svn\n"
            if (-d "$dir/.svn");
        close(OUT);

        # Git add this .gitignore file
        system("git add $dir/.gitignore");
    }
        
    # Now find subdirectories in this directory
    my @entries;
    opendir(DIR, $dir) || die "Cannot open directory \"$dir\" for reading: $!";
    @entries = readdir(DIR);
    closedir DIR;

    foreach my $e (@entries) {
        # Skip special directories and sym links
        next
            if ($e eq "." || $e eq ".." || $e eq ".svn" || $e eq ".git" ||
                -l "$dir/$e");

        # If it's a directory, analyze it
        process("$dir/$e")
            if (-d "$dir/$e");
    }
}
