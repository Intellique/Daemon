#! /usr/bin/perl

use strict;
use warnings;

my ( $symbol, $filename ) = @ARGV;

my $git_commit;
my $version;
if ( open my $dcl, '<', 'debian/changelog' ) {
    $version = <$dcl>;
    chomp $version;
    $version =~ s/^.+\((.+)\).+$/$1/;
    $git_commit = "v$version";

    close($dcl);
}
else {
    ($version) = qx/git describe/;
    chomp $version;
}

if ( -e '.git' ) {
    ($git_commit) = qx/git log -1 --format=%H/;
    chomp $git_commit;
}

open my $fd, '>', $filename;
print $fd "#define ${symbol}_VERSION \"$version\"\n";
print $fd "#define ${symbol}_GIT_COMMIT \"$git_commit\"\n";
close $fd;

