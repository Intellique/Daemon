#! /usr/bin/perl

use strict;
use warnings;

my ($dir) = @ARGV;

my $version;
if ( -f 'debian/changelog' ) {
	open my $fd, '<', 'debian/changelog';
	my $line = <$fd>;
	close $fd;

	if ( $line =~ /\((.*)\)/ ) {
		$version = $1;
		($version) = $version =~ /^([^-]+)-/;
	} else {
		print $dir;
		exit;
	}
} else {
	($version) = qx/git describe/;
	chomp $version;

	($version) = $version =~ /^v([^-]+)-/;
}

if (defined $version) {
	print "${dir}_$version";
} else {
	print $dir;
}

