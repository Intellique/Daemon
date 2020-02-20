#! /usr/bin/perl

use strict;
use warnings;

use JSON::PP;
use LWP::UserAgent;

if ( $ARGV[0] eq 'config' ) {
    my $sent = {
        'name'        => 'EditShare create asset',
        'description' => 'Create a new asset in EditShare Flow',
        'type'        => 'post job'
    };
    print encode_json($sent);
    exit;
}

use Data::Dumper;

my $schema     = 'https';
my $hostname   = '192.168.0.154:8006';
my $path       = 'api/v2/scan/asset';
my $login      = 'emmanuel';
my $password   = 'intellique';
# The name of the mediaspace in ES
my $mediaspace = 'StoriqOne';
# The local part of the shared space path
my $mspath     = '/mnt/raid';

open( STDERR, '>>', "/tmp/script$$.log" ) or die;

my $data_in = do { local $/; <STDIN> };
my $data    = decode_json $data_in;

my $archive;
if ( defined $data->{archive}->{main} ) {
    $archive = $data->{archive}->{main};
} elsif ( defined $data->{archive}->{source} ) {
    $archive = $data->{archive}->{source};
} else {
    my $sent = {
        'finished' => JSON::PP::true,
        'data'     => {},
        'message'  => "No archive found",
    };
    print encode_json($sent);
    warn "Arrrgh...\n";
    warn encode_json($sent);
    exit 2;
}

sub is_dpx {
    my ($file) = @_;
    return $file->{path} =~ m/\.dpx$/i;
}

my $file;
find_file: foreach my $vol ( reverse @{ $archive->{volumes} } ) {
    my $nb_total_file = scalar( @{ $vol->{files} } );
    my $middle        = int( $nb_total_file / 2 );

    for ( my $i = 0 ; $i < $middle ; $i++ ) {
        my $ptr_file = $vol->{files}->[ $middle + $i ];
        if ( is_dpx( $ptr_file->{file} ) ) {
            $file = $ptr_file->{file};
			warn "file01 :" . Dumper $file;
            $file->{path} =~ s(^$mspath)();
			warn "file02 :" . Dumper $file;
            last find_file;
        }

        $ptr_file = $vol->{files}->[ $middle - $i ];
        if ( is_dpx( $ptr_file->{file} ) ) {
            $file = $ptr_file->{file};
			warn "file1 :" . Dumper $file;
            $file->{path} =~ s(^$mspath)();
			warn "file2 :" . Dumper $file;
            last find_file;
        }
    }
}

unless ( defined $file ) {
    my $sent = {
        'finished' => JSON::PP::true,
        'data'     => {},
        'message'  => "No dpx file found",
    };
    warn "Arrrgh no DPX...\n";
    warn encode_json($sent);
    print encode_json($sent);
    exit 2;
}

my $content_request = encode_json(
    {
        'createproxy' => JSON::PP::true,
        'files'       => [ $file->{path} ],
        'fullscan'    => JSON::PP::false,
        'mediaspace'  => $mediaspace,
        'metadata'    => {},
        'update_pmr'  => JSON::PP::false
    }
);

warn "connect;  request:\n";
warn $content_request;

my $user_agent = LWP::UserAgent->new;
$user_agent->ssl_opts( verify_hostname => 0 );
$user_agent->ssl_opts( SSL_verify_mode => 0 );
$user_agent->credentials( $hostname, 'Restricted Area', $login, $password );

my $request = HTTP::Request->new( POST => "$schema://$hostname/$path" );
$request->content_type('application/json');
$request->content($content_request);

my $result = $user_agent->request($request);

unless ( $result->is_success ) {
    my $sent = {
        'finished' => JSON::PP::true,
        'data'     => {},
        'message'  => "Request to api ($schema://$hostname/$path) failed",
    };
    warn "connect failed\n";
    print encode_json($sent);
    exit 2;
}

my $sent = {
    'finished' => JSON::PP::true,
    'data'     => { 'job' => $result->content },
    'message'  => ''
};
warn "finish all right";
print encode_json($sent);
