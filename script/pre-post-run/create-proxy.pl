#! /usr/bin/perl

use strict;
use warnings;

use Digest::MD5 q/md5_hex/;
use Encode;
use JSON::PP;
use Mediainfo;
use POSIX q/nice/;
use Sys::CPU q/cpu_count/;

my $nb_cpu     = cpu_count();
my $output_dir = '/usr/share/storiqone/cache/movies/proxy';

$nb_cpu = int( $nb_cpu * 3 / 4 ) if $nb_cpu >= 4;

if ( open my $fd, '<', '/etc/storiq/storiqone.conf' ) {
    my $data = do { local $/; <$fd> };

    my $config = decode_json $data;

    $nb_cpu = $config->{'proxy'}->{'nb cpu'}
        if exists $config->{'proxy'}->{'nb cpu'};
    $output_dir = $config->{'proxy'}->{'path'}
        if exists $config->{'proxy'}->{'path'};

    close $fd;
}

my $param = shift;

my $data_in = do { local $/; <STDIN> };

my $convert = undef;
my $ffmpeg  = undef;
foreach my $dir ( split ':', $ENV{PATH} ) {
    unless ( defined $convert ) {
        my $command = "$dir/convert";
        if ( -f $command and -x $command ) {
            $convert = $command;
        }
    }

    unless ( defined $ffmpeg ) {
        foreach my $enc ( 'ffmpeg', 'avconv' ) {
            my $command = "$dir/$enc";
            if ( -f $command and -x $command ) {
                $ffmpeg = $command;
                last;
            }
        }
    }

    last if defined($convert) and defined($ffmpeg);
}

unless ( defined $convert ) {
    my $sent = {
        'finished' => JSON::PP::true,
        'data'     => {},
        'message'  => "Encoder not found ('convert')",
    };
    print encode_json($sent);
    exit 1;
}

unless ( defined $ffmpeg ) {
    my $sent = {
        'finished' => JSON::PP::true,
        'data'     => {},
        'message'  => "Encoder not found ('ffmpeg', 'avconv')",
    };
    print encode_json($sent);
    exit 1;
}

my $data = decode_json $data_in;

my %file_done;
my $nb_processes = 0;

my %codec = (
    'mp4' => {
        'audio' => {
            'codec'   => 'aac',
            'bitrate' => '64k',
            'extra'   => [ '-strict', '-2' ]
        },
        'video' => {
            'codec'   => 'libx264',
            'bitrate' => '500k',
            'extra'   => []
        }
    },
    'ogv' => {
        'audio' => {
            'codec'   => 'libvorbis',
            'bitrate' => '96k',
            'extra'   => []
        },
        'video' => {
            'codec'   => 'libtheora',
            'bitrate' => '500k',
            'extra'   => []
        }
    },
);

sub processImage {
    my ($input) = @_;

    my $pid = fork();

    return unless defined $pid;

    if ( $pid == 0 ) {
        nice(10);

        my $ext = '';
        $exit = $1 if ( $input =~ /\.([^.]+)$/ );
        my $filename = md5_hex( encode( 'UTF-8', $input ) ) . '.' . $ext;

        my @params = ( '-thumbnail', '320x240', $input, $filename );
        exec $convert, @params;

        exit 1;
    }

    $nb_processes++;

    if ( $nb_processes >= $nb_cpu ) {
        wait();
        $nb_processes--;
    }
}

sub processVideo {
    my ( $input, $format_name ) = @_;

    my $pid = fork();

    return unless defined $pid;

    if ( $pid == 0 ) {
        nice(10);

        my $format = $codec{$format_name};

        my @params =
            ( '-v', 'quiet', '-i', encode_utf8($input), '-t', '0:0:30' );

        push @params, '-vcodec', $format->{video}->{codec};
        push @params, '-b:v',    $format->{video}->{bitrate};
        push @params, '-s',      'cif';
        push @params, @{ $format->{video}->{extra} }
            if scalar( @{ $format->{video}->{extra} } ) > 0;

        push @params, '-acodec', $format->{audio}->{codec};
        push @params, '-b:a',    $format->{audio}->{bitrate};
        push @params, '-ac',     '2', '-ar', '44100';
        push @params, @{ $format->{audio}->{extra} }
            if scalar( @{ $format->{audio}->{extra} } ) > 0;

        my $filename =
            md5_hex( encode( 'UTF-8', $input ) ) . '.' . $format_name;
        push @params, "$output_dir/$filename";

        # print "run: " . join(" ", $ffmpeg, @params) . "\n";
        exec $ffmpeg, @params;

        exit 1;
    }

    $nb_processes++;

    if ( $nb_processes >= $nb_cpu ) {
        wait();
        $nb_processes--;
    }
}

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

my $nb_total_files = 0;
foreach my $vol ( @{ $archive->{volumes} } ) {
    foreach my $ptr_file ( @{ $vol->{files} } ) {
        my $file = $ptr_file->{file};

        next if defined $file_done{ $file->{path} };
        $file_done{ $file->{path} } = $file;

        $nb_total_files++;
    }
}
%file_done = ();

my $nb_file_done = 0;
foreach my $vol ( @{ $archive->{volumes} } ) {
    foreach my $ptr_file ( @{ $vol->{files} } ) {
        my $file = $ptr_file->{file};

        next if defined $file_done{ $file->{path} };
        $file_done{ $file->{path} } = $file;
        $nb_file_done++;

        next unless -e $file->{path};
        next if $file->{type} eq 'directory';

        if ( $file->{"mime type"} =~ m#^image/#i ) {
            processImage( $file->{path} );
        }
        else {
            my $info = new Mediainfo( "filename" => $file->{path} );
            next unless defined $info->{video_format};

            processVideo( $file->{path}, 'mp4' );
            processVideo( $file->{path}, 'ogv' );
        }

        my $sent = {
            'finished'    => JSON::PP::false,
            'data'        => {},
            'progression' => $nb_file_done / $nb_total_files,
            'message'     => ''
        };
        print encode_json($sent);
    }
}

while ( $nb_processes > 0 ) {
    wait();
    $nb_processes--;
}

my $sent = {
    'finished' => JSON::PP::true,
    'data'     => {},
    'message'  => ''
};
print encode_json($sent);

