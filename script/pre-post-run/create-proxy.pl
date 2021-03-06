#! /usr/bin/perl

use strict;
use warnings;

use JSON::PP;

if ( $ARGV[0] eq 'config' ) {
    my $sent = {
        'name'        => 'create proxies',
        'description' => 'Create a proxy for audio, video and images files',
        'type'        => 'post job'
    };
    print encode_json($sent);
    exit;
}

use Digest::MD5 q/md5_hex/;
use Encode;
use Mediainfo;
use POSIX q/nice/;
use Sys::CPU q/cpu_count/;

my $nb_cpu             = cpu_count();
my $output_picture_dir = '/usr/share/storiqone-www/cache/pictures/proxy';
my $output_movie_dir   = '/usr/share/storiqone-www/cache/movies/proxy';
my $output_sound_dir   = '/usr/share/storiqone-www/cache/sound/proxy';
my $config;

$nb_cpu = int( $nb_cpu * 3 / 4 ) if $nb_cpu >= 4;

if ( open my $fd, '<', '/etc/storiq/storiqone.conf' ) {
    my $data = do { local $/; <$fd> };

    $config = decode_json $data;

    $nb_cpu = $config->{'proxy'}{'nb cpu'}
        if exists $config->{'proxy'}{'nb cpu'};
    $output_picture_dir = $config->{'proxy'}{'picture'}{'path'}
        if exists $config->{'proxy'}{'picture'}{'path'};
    $output_movie_dir = $config->{'proxy'}{'movie'}{'path'}
        if exists $config->{'proxy'}{'movie'}{'path'};
    $output_sound_dir = $config->{'proxy'}{'sound'}{'path'}
        if exists $config->{'proxy'}{'sound'}{'path'};

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

sub processAudio {
    return
        if ( defined $config->{'proxy'}{'sound'}{'enabled'}
        and not $config->{'proxy'}{'sound'}{'enabled'} );
    my ($input) = @_;

    my $pid = fork();

    return unless defined $pid;

    if ( $pid == 0 ) {
        nice(10);

        my $format = $codec{'mp4'}{'audio'};

        my @params = ( '-v', 'quiet', '-i', encode_utf8($input) );

        if ( defined $config->{'proxy'}{'sound'}{'duration'} ) {
            push @params, '-t', $config->{'proxy'}{'sound'}{'duration'};
        }

        push @params, '-acodec', $format->{codec};
        push @params, '-b:a',    $format->{bitrate};
        push @params, @{ $format->{extra} }
            if scalar( @{ $format->{extra} } ) > 0;

        my $filename = md5_hex( encode( 'UTF-8', $input, '-vn' ) ) . '.m4a';
        push @params, "$output_sound_dir/$filename";

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

sub processImage {
    return
        if ( defined $config->{'proxy'}{'picture'}{'enabled'}
        and not $config->{'proxy'}{'picture'}{'enabled'} );
    my ($input) = @_;

    my $pid = fork();

    return unless defined $pid;

    if ( $pid == 0 ) {
        nice(10);

        my $image_size =
            defined $config->{'proxy'}{'picture'}{'image size'}
            ? $config->{'proxy'}{'picture'}{'image size'}
            : '320x240';
        my $filename = $output_picture_dir . '/'
            . md5_hex( encode( 'UTF-8', $input ) ) . '.jpg';
        my @params = ( '-thumbnail', $image_size, $input, $filename );

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
    return
        if ( defined $config->{'proxy'}{'movie'}{'enabled'}
        and not $config->{'proxy'}{'movie'}{'enabled'} );
    my ( $input, $format_name ) = @_;

    my $pid = fork();

    return unless defined $pid;

    if ( $pid == 0 ) {
        nice(10);

        my $video_size =
            defined $config->{'proxy'}{'movie'}{'video size'}
            ? $config->{'proxy'}{'movie'}{'video size'}
            : 'cif';
        my $format = $codec{$format_name};

        my @params = ( '-v', 'quiet', '-i', encode_utf8($input) );

        if ( defined $config->{'proxy'}{'movie'}{'duration'} ) {
            push @params, '-t', $config->{'proxy'}{'movie'}{'duration'};
        }

        push @params, '-vcodec', $format->{video}->{codec};
        push @params, '-b:v',    $format->{video}->{bitrate};
        push @params, '-s',      $video_size;
        push @params, @{ $format->{video}->{extra} }
            if scalar( @{ $format->{video}->{extra} } ) > 0;

        push @params, '-acodec', $format->{audio}->{codec};
        push @params, '-b:a',    $format->{audio}->{bitrate};
        push @params, '-ac',     '2', '-ar', '44100';
        push @params, @{ $format->{audio}->{extra} }
            if scalar( @{ $format->{audio}->{extra} } ) > 0;

        my $filename =
            md5_hex( encode( 'UTF-8', $input ) ) . '.' . $format_name;
        push @params, "$output_movie_dir/$filename";

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

            if ( defined $info->{video_format} ) {
                processVideo( $file->{path}, 'mp4' );
                processVideo( $file->{path}, 'ogv' );
            }
            elsif ( defined $info->{audio_format} ) {
                processAudio( $file->{path} );
            }
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

