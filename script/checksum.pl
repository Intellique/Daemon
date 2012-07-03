#! /usr/bin/perl

use strict;
use warnings;

use IPC::Open2;

# Try to read old checksum
my $symbol   = shift @ARGV;
my $filename = shift @ARGV;
my $oldChecksum = '';
if (open my $fd, '<', $filename) {
	my $line = <$fd>;
	$oldChecksum = $1 if ($line =~ /"(\w+)"/);
	close $fd;
}

my ($fdin, $fdout);
my $pid = open2($fdout, $fdin, 'sha1sum', '-') or die "Can't exec sha1sum";

print $fdin $_ while (<>);
close $fdin;

waitpid $pid, 0;

my $line = <$fdout>;
my $newCheckum = '';
$newCheckum = $1 if ($line =~ /^(\w+)\s/);

if ($oldChecksum ne $newCheckum) {
	open my $fd, '>', $filename;
	print $fd "#define ${symbol}_SRCSUM \"$newCheckum\"\n";
	close $fd;
} else {
	utime undef, undef, $filename;
}

