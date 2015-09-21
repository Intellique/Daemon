#! /usr/bin/perl

use strict;
use warnings;

my ($dir) = @ARGV;

my ($version) = qx/git describe/;
chomp $version;

my ($ver) = $version =~ /^v([^-]+)-/;

if (defined $ver) {
	print "${dir}_$ver";
} else {
	print $dir;
}

