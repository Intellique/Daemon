#! /usr/bin/perl

use strict;
use warnings;

use JSON::PP;
use File::Basename;
use Data::Dumper;
use LWP::UserAgent;

if ( @ARGV and $ARGV[0] eq 'config' ) {
    my $sent = {
        'name'        => 'ES Flow scan post restore',
        'description' => 'EditShare Flow Scan after restoring files',
        'type'        => 'post job'
    };
    print encode_json($sent);
    exit;
}

my $data_in       = do { local $/; <STDIN> };
my $data          = decode_json $data_in;
my $restored_path = $data->{'restore path'};

my $schema     = 'https';
my $hostname   = '192.168.0.154:8006';
my $path       = 'api/v2/scan/asset';
my $login      = 'storiqone';
my $password   = 'storiqone';
my $mediaspace = 'StoriqOne';

# debug stuff
# open( my $fd, '>', "/tmp/Rest$$.json" );
# print {$fd} $data_in;
# close $fd;

#log stuff
# open( $fd, '>', "/tmp/Rest$$.log" );
#

my $message = '';
my $restpaths;

if ( defined($restored_path) ) {
    for my $vol ( @{ $data->{archive}->{volumes} } ) {
        for my $file ( @{ $vol->{files} } ) {
            next unless defined $file->{file}{'restored to'};
			next if $file->{file}{type} eq 'directory';
			
            # my $parent = dirname( $file->{file}{'restored to'} );
            # $restpaths->{$parent}++;
            my $filename = basename( $file->{file}{'restored to'} );
            $restpaths->{$filename}++;
        }

#        foreach my $path ( keys %$restpaths ) {
#			my $parent = dirname $path ;
#            delete $restpaths->{$path}
#              if ( exists $restpaths->{$parent} );
#        }
    }

} else {
    $message = "Restore path NOT FOUND $restored_path\n";
}

# Mediaspace = last folder minus _1 if restore to flow
if ($restored_path =~ m#/mnt/flow/# ) {
	my @pathelems = split ('/', $restored_path);
	$mediaspace = pop @pathelems;
	$mediaspace =~ s/_1$//;
}


my $content_request = encode_json(
    {   'createproxy' => JSON::PP::true,
        'files'       =>  [ keys %$restpaths ],
        'fullscan'    => JSON::PP::false,
        'mediaspace'  => $mediaspace,
        'metadata'    => {},
        'update_pmr'  => JSON::PP::false
    }
);
 
my $user_agent = LWP::UserAgent->new;
$user_agent->ssl_opts( verify_hostname => 0 );
$user_agent->ssl_opts( SSL_verify_mode => 0 );
$user_agent->credentials( $hostname, 'Restricted Area', $login, $password );

my $request = HTTP::Request->new( POST => "$schema://$hostname/$path" );
$request->content_type('application/json');
$request->content($content_request);

my $result = $user_agent->request($request);
unless ( $result->is_success ) {
	$message = "Request to api ($schema://$hostname/$path) failed";
}


my $sent = {
    'finished' => JSON::PP::true,
    'data'     => {},
    'message'  => $message
};
print encode_json($sent);
