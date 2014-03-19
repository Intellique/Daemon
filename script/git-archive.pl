#! /usr/bin/perl

use strict;
use warnings;

my ($version) = qx/git describe/;
chomp $version;
$version =~ s/^v([^-+]+).*$/$1/;

print "$version\n";

