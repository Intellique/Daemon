#! /usr/bin/perl

use strict;
use warnings;

open( my $fd, '<', 'test-link.txt' ) or die 'no link file';

while (<$fd>) {
	chomp;

    if (/^(.+) -> (.+)$/) {
		unlink $2 if -f $2;
		link $1, $2 unless -e $2;
    }
}

close $fd;

