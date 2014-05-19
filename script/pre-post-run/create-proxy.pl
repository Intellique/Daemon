#! /usr/bin/perl

use strict;
use warnings;

use Digest::MD5 q/md5_hex/;
use JSON::PP;
use POSIX q/nice/;
use Sys::CPU q/cpu_count/;

my $nb_cpu     = cpu_count();
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

$nb_cpu = int( $nb_cpu * 3 / 4 ) if $nb_cpu >= 4;

my $archive = decode_json $data_in;

my %file_done;
my $nb_processes = 0;

sub process {
    my ( $input, $format ) = @_;

    my $pid = fork();
    if ( $pid == 0 ) {
        nice(10);

        my $filename = md5_hex($input) . '.mp4';
        exec $encoder, '-v', 'quiet', '-i', $input, '-acodec', 'libvorbis',
            '-ac', '2', '-ab', '96k', '-ar', '44100', '-b', '500k', '-s',
            '320x240', '-t', '0:0:30', "$output_dir/$filename";
    }

    $nb_processes++;

    if ( $nb_processes >= $nb_cpu ) {
        wait();
        $nb_processes--;
    }
}

foreach my $vol ( @{ $archive->{volumes} } ) {
    foreach my $file ( @{ $vol->{files} } ) {
        next if defined $file_done{ $file->{path} };
        $file_done{ $file->{path} } = $file;

        next unless $file->{'mime type'} =~ m{^video/};

        process( $file->{path}, '.mp4' );
        process( $file->{path}, '.ogv' );
    }
}

while ( $nb_processes > 0 ) {
    wait();
    $nb_processes--;
}

my $sent = {
    'data'    => {},
    'message' => "",
};
print encode_json($sent);

