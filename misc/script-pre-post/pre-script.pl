#! /usr/bin/perl

use strict;
use warnings;

my $param = shift;

my $data_in = join('\n', <>);

# sleep 300;

print "{ \"data\": {}, \"message\": \"foo bar\", \"should run\": true, \"param\": \"$param\" }\n";

