#! /usr/bin/perl

use strict;
use warnings;

my ( $symbol, $filename ) = @ARGV;
my $new_value = 0;

my ($version) = qx/git describe/;
chomp $version;
my ($branch) = grep {s/^\* (?:.*\/)?(\w+)/$1/} qx/git branch/;
chomp $branch;
$version .= '-' . $branch if $branch ne 'stable';

my ($git_commit) = qx/git log -1 --format=%H/;
chomp $git_commit;

my @lines;
if ( open my $fd, '<', $filename ) {
    @lines = <$fd>;
    close $fd;
}
else {
    $new_value = 1;
}

my ( $found_version, $found_commit ) = ( 0, 0 );
foreach (@lines) {
    if (/^#define ${symbol}_VERSION "(.+)"/) {
        $found_version = 1;

        if ( $version ne $1 ) {
            $new_value = 1;
            $_         = "#define ${symbol}_VERSION \"$version\"\n";
        }
    }
    elsif (/^#define ${symbol}_GIT_COMMIT "(\w+)"/) {
        $found_commit = 1;

        if ( $git_commit ne $1 ) {
            $new_value = 1;
            $_         = "#define ${symbol}_GIT_COMMIT \"$git_commit\"\n";
        }
    }
}

push @lines, "#define ${symbol}_VERSION \"$version\"\n" unless $found_version;
push @lines, "#define ${symbol}_GIT_COMMIT \"$git_commit\"\n"
    unless $found_commit;

if ($new_value) {
    open my $fd, '>', $filename;
    print $fd @lines;
    close $fd;
}

