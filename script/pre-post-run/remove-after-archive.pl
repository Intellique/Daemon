#! /usr/bin/perl

use strict;
use warnings;

use File::Basename 'dirname';
use File::Path 'remove_tree';
use JSON::PP;

my $param = shift;

my $data_in = do { local $/; <STDIN> };
my $data = decode_json $data_in;

my $archive;
if ( defined $data->{archive}->{main} ) {
    $archive = $data->{archive}->{main};
}
elsif ( defined $data->{archive}->{source} ) {
    $archive = $data->{archive}->{source};
}
else {
    my $sent = {
        'finished' => JSON::PP::true,
        'data'     => {},
        'message'  => "No archive found",
    };
    print encode_json($sent);
    exit 2;
}

my %parents = ();
foreach my $vol ( @{ $archive->{volumes} } ) {
    foreach my $ptr_file ( @{ $vol->{files} } ) {
        my $file = $ptr_file->{file}->{path};
        next unless -e $file;

        $parents{ dirname($file) } = 1;

        remove_tree($file);
    }
}

my $command = '/var/www/nextcloud/occ';

# Don't put trailing '/'
my $next_cloud_data_dir = '/var/www/nextcloud/data';

unless ($<) {
    my $new_uid = getpwnam 'www-data';
    $< = $> = $new_uid;
}

if ( -x $command ) {
    foreach my $dir ( keys %parents ) {

        # $sub must start with '/'
        my $subdir = substr( $dir, length($next_cloud_data_dir) );

        system( $command, 'files:scan', "--path=$subdir" );
    }
}

my $sent = {
    'finished' => JSON::PP::true,
    'data'     => {},
    'message'  => ''
};
print encode_json($sent);
