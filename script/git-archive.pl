#! /usr/bin/perl

use strict;
use warnings;

my ($dirname) = @ARGV;

my ($dir, $ver) = $dirname =~ /^(.+?).(\d+.*)/;

if (defined $ver) {
	print "${dir}_$ver";
} else {
	print $dirname;
}

