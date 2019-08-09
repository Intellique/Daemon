#! /usr/bin/perl

use strict;
use warnings;

my $dbconfig_file = '/etc/dbconfig-common/storiqone-plugin-db-postgresql.conf';
my $st1_config_file = '/etc/storiq/storiqone.conf';

my $password;

open my $fd_config, '<', $dbconfig_file;
while (<$fd_config>) {
	if (/dbc_dbpass='(.+)'$/) {
		$password = $1;
		last;
	}
}
close $fd_config;


open my $fd_config_in, '<', $st1_config_file;
open my $fd_config_out, '>', "$st1_config_file.new";
while (<$fd_config_in>) {
	$_ =~ s/<password>/$password/;
	print $fd_config_out $_;
}
close $fd_config_in;
close $fd_config_out;

rename $st1_config_file, "$st1_config_file.old";
rename "$st1_config_file.new", $st1_config_file;
unlink "$st1_config_file.old";

