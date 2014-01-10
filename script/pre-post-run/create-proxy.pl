#! /usr/bin/perl

use strict;
use warnings;

use Digest::MD5 qw(md5_hex);
use JSON::PP;

my $output_dir = '/var/www/stone/cache/movies/proxy';

my $param = shift;

my $data_in = do { local $/; <STDIN> };

my $encoder = undef;
foreach my $dir ( split ':', $ENV{PATH} ) {
    foreach my $enc ( 'ffmpeg', 'avconv' ) {
        my $command = "$dir/$enc";
        if ( -f $command and -x $command ) {
            $encoder = $command;
            last;
        }
    }

    last if defined $encoder;
}

unless ($encoder) {
    my $sent = {
        'data'    => {},
        'message' => "Encoder not found ('ffmpeg', 'avconv')",
    };
    print encode_json($sent);
    exit 1;
}

my $archive = decode_json $data_in;

my %file_done;
foreach my $vol ( @{ $archive->{volumes} } ) {
    foreach my $file ( @{ $vol->{files} } ) {
        next if defined $file_done{ $file->{path} };
        $file_done{ $file->{path} } = $file;

        next unless $file->{'mime type'} =~ m{^video/};

        my $filename = md5_hex( $file->{path} ) . '.webm';
        my $status = system $encoder, '-v', 'quiet', '-i', $file->{path}, '-acodec',
            'libvorbis', '-ac', '2', '-ab', '96k', '-ar', '44100', '-b',
            '500k', '-s', '320x240', "$output_dir/$filename";
    }
}

my $sent = {
    'data'    => {},
    'message' => "",
};
print encode_json($sent);

