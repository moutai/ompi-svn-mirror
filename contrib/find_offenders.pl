#!/usr/bin/perl

if (scalar(@ARGV) != 2) {
    print "Usage:
           find_offenders.pl <header_file_list> <source_tree_path>
           eg.,: running from top level source tree
           #contrib/find_offenders.pl contrib/results.txt .\n";
    exit(3);
}
$source_tree = @ARGV[1];
$header_file_list = @ARGV[0];

#first construct the danger list
open(FILE_LIST, "$header_file_list") || print "Could not open results.txt\n";
open(DANGER_FILES, "> contrib/headers.txt") || print "Could not open headers.txt\n";

while (<FILE_LIST>) {
#check if this file is a file in the source tree
    chomp($_);
    $file_name = $_;
    open(FILE, "find . -name $file_name |") || print "find failed\n";
    while(<FILE>) {
        #file is found 
        print DANGER_FILES "#include <$file_name>\n";
    }
    close (FILE);
}
close (DANGER_FILES);
close (FILE_LIST);

open(DANGER_FILES, "contrib/headers.txt") || print "Could not open headers.txt\n";
open(OFFENSIVE, "> contrib/offenders.list") || print "Could not open offenders list\n";

while (<DANGER_FILES>) {

    $header = $_;
    chomp($header);
    print;

    open(C_FILES, "find $source_tree -name *.c |") || print "Could not complete find command\n";

    while (<C_FILES>) {
        $c_file = $_;
        open(C_FILE, "$c_file") || print "Could not open $_\n";
        while (<C_FILE>) {
            if (/$header/) {
                print OFFENSIVE $header ." --> ". $c_file ;    
            }
        }
        close (C_FILE);
    }

    close (C_FILES);
    
    open(H_FILES, "find . -name *.h |") || print "Could not complete find command\n";

    while (<H_FILES>) {
        $h_file = $_;
        open(H_FILE, "$h_file") || print "Could not open $_\n";
        while (<H_FILE>) {
            if (/$header/) {
                print OFFENSIVE $header ." --> ". $h_file ;    
            }
        }
        close (H_FILE);
    }

    close (H_FILES);
}

close (DANGER_FILES);
close (OFFENSIVE);
