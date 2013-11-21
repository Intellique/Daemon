#! /usr/bin/perl

use strict;
use warnings;

my $param = shift;

my $data_in = do { local $/; <STDIN> };

if (open my $fd, '>', 'script.json') {
	print { $fd } $data_in;
	close $fd;
}

# sleep 300;

print "{ \"data\": {}, \"message\": \"foo bar\", \"should run\": true, \"param\": \"$param\" }\n";

