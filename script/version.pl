#! /usr/bin/perl

use strict;
use warnings;

my ( $symbol, $filename ) = @ARGV;

my $version;
if ( open my $dcl, '<', 'debian/changelog' ) {
    $version = <$dcl>;
    chomp $version;
    $version =~ s/^.+\((.+)\).+$/$1/;

    close($dcl);
}
else {
    ($version) = qx/git describe/;
    chomp $version;
}

my ($branch) = grep { s/^\*\s+(.*)$/$1/ } qx/git branch/;
chomp $branch;
$version .= '~' . $branch if $branch ne 'master';

my ($git_commit) = qx/git log -1 --format=%H/;
chomp $git_commit;

open my $fd, '>', $filename;
print $fd "#define ${symbol}_VERSION \"$version\"\n";
print $fd "#define ${symbol}_GIT_COMMIT \"$git_commit\"\n";
close $fd;

