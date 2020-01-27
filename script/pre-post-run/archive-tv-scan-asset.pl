#! /usr/bin/perl

use strict;
use warnings;

use JSON::PP;
use LWP::UserAgent;

if ( $ARGV[0] eq 'config' ) {
    my $sent = {
        'name'        => 'archive tv create asset',
        'description' => 'Create an asset for ArchiveTV',
        'type'        => 'post job'
    };
    print encode_json($sent);
    exit;
}

my $schema     = 'https';
my $hostname   = '';
my $path       = '';
my $login      = '';
my $password   = '';
my $mediaspace = 'StoriqOne';

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

sub is_dpx {
    my ($file) = @_;
    return $file->{path} =~ m/\.dpx$/i;
}

my $file;
find_file: foreach my $vol ( @{ $archive->{volumes} } ) {
    my $nb_total_file = scalar( @{ $vol->{files} } );
    my $middle        = int( $nb_total_file / 2 );

    for ( my $i = 0; $i < $middle; $i++ ) {
        my $ptr_file = $vol->{files}->[ $middle + $i ];
        if ( is_dpx( $ptr_file->{file} ) ) {
            $file = $l_file;
            break find_file;
        }

        $ptr_file = $vol->{files}->[ $middle - $i ];
        if ( is_dpx( $ptr_file->{file} ) ) {
            $file = $l_file;
            break find_file;
        }
    }
}

unless ( defined $file ) {
    my $sent = {
        'finished' => JSON::PP::true,
        'data'     => {},
        'message'  => "No dpx file found",
    };
    print encode_json($sent);
    exit 2;
}

$content_request = encode_json(
    {   'createproxy' => JSON::PP::true,
        'files'       => [ $file->{path} ],
        'fullscan'    => JSON::PP::false,
        'mediaspace'  => $mediaspace,
        'metadata'    => {},
        'update_pmr'  => JSON::PP::false
    }
);

my $user_agent = LWP::UserAgent->new;
$user_agent->credentials( $hostname, 'default', $login, $password );
$user_agent->ssl_opts( verify_hostname => 0 );

my $request = HTTP::Request->new( POST => "$schema://$hostname/$path" );
$request->content_type('application/json');
$request->content($content_request);

my $result = $ua->request($request);
unless ( $result->is_success ) {
    my $sent = {
        'finished' => JSON::PP::true,
        'data'     => {},
        'message'  => "Request to api ($schema://$hostname/$path) failed",
    };
    print encode_json($sent);
    exit 2;
}

my $sent = {
    'finished' => JSON::PP::true,
    'data'     => {},
    'message'  => ''
};
print encode_json($sent);
