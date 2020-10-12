#!/usr/bin/perl

use strict;
use warnings;

use JSON::PP;
use Data::Dumper;

if ( $ARGV[0] eq 'config' ) {
    my $sent = {
        'name' => 'Generic Test Script',
        'description' =>
          'Grab output from Storiq One job in /tmp/test-$$.json',
        'type' => 'post job'
    };
    print encode_json($sent);
    exit;
}

my $data_in = do { local $/; <STDIN> };
#my $data    = decode_json $data_in;

open my $file, '>', "/tmp/test-$$.json";
print $file $data_in;
close $file;

my $sent = {
    'finished' => JSON::PP::true,
    'data'     => {},
    'message'  => ''
};
print encode_json($sent);
