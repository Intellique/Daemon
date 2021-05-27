#! /usr/bin/perl

use strict;
use warnings;

my $ref;
my $directory = '.git';

if ( -f $directory ) {
	open my $fd_git, '<', $directory;
	while (<$fd_git>) {
		chomp;
		(undef, $directory) = split ' ';
	}
	close $fd_git;
} else {
	exit;
}

open my $fd, '<', "$directory/HEAD";
while (<$fd>) {
	if (/^ref: (.+)$/) {
		$ref = $1;
		last;
	}
}
close $fd;

my $sub_directory = "$directory/commondir";
if ( -f $sub_directory ) {
	open my $fd_git, '<', $sub_directory;
	while (<$fd_git>) {
		chomp;
		$directory = "$directory/$_";
	}
	close $fd_git;
}

my $filename = "$directory/$ref";

if (-e $filename) {
	print "$filename\n";
	exit 0;
}

$filename = "$directory/logs/$ref";

if (-e $filename) {
	print "$filename\n";
	exit 0;
}

