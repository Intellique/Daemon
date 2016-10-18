#! /usr/bin/perl

use strict;
use warnings;

use IPC::Run3 qw/run3/;

my %libs = (
    'libcrypto' => {
        'name' => 'openssl (lib crypto only)',

        'symbol' => 'LIB_CRYPTO_',

        'auto configure' => {
            'include dir' => [ 'pkg-config', '--variable=includedir', 'openssl' ],
            'lib'         => undef,
        },

        'dummy file' => 'script/dummy/crypto.c',

        'manual configure' => {
            'cflags'       => undef,
            'include dirs' => undef,
            'lib'          => ['crypto', 'ssl'],
        },

        'result' => {
            'include' => undef,
            'lib'     => undef,
        },
    },
    'libexpat' => {
        'name' => 'expat',

        'symbol' => 'LIB_EXPAT_',

        'auto configure' => {
            'include dir' => [ 'pkg-config', '--variable=includedir', 'expat' ],
            'lib'         => [ 'pkg-config', '--libs', 'expat' ],
        },

        'dummy file' => 'script/dummy/expat.c',

        'manual configure' => {
            'cflags'       => undef,
            'include dirs' => undef,
            'lib'          => ['expat'],
        },

        'result' => {
            'include' => undef,
            'lib'     => undef,
        },
    },
    'libmagic' => {
        'name' => 'lib magic',

        'symbol' => 'LIB_MAGIC_',

        'auto configure' => {
            'include dir' => undef,
            'lib'         => undef,
        },

        'dummy file' => 'script/dummy/magic.c',

        'manual configure' => {
            'cflags'       => undef,
            'include dirs' => undef,
            'lib'          => ['magic'],
        },

        'result' => {
            'include' => undef,
            'lib'     => undef,
        },
    },
    'libnettle' => {
        'name' => 'nettle',

        'symbol' => 'LIB_NETTLE_',

        'auto configure' => {
            'include dir' => [ 'pkg-config', '--variable=includedir', 'nettle' ],
            'lib'         => [ 'pkg-config', '--libs', 'nettle' ],
        },

        'dummy file' => 'script/dummy/nettle.c',

        'manual configure' => {
            'cflags'       => undef,
            'include dirs' => undef,
            'lib'          => ['nettle'],
        },

        'result' => {
            'include' => undef,
            'lib'     => undef,
        },
    },
    'libpq' => {
        'name' => 'postgresql',

        'symbol' => 'LIB_PQ_',

        'auto configure' => {
            'include dir' => [ 'pg_config', '--includedir' ],
            'lib'         => undef
        },

        'dummy file' => 'script/dummy/pq.c',

        'manual configure' => {
            'cflags'       => undef,
            'include dirs' => ['/usr/include/postgresql/'],
            'lib'          => ['pq'],
        },

        'result' => {
            'include' => undef,
            'lib'     => undef,
        },
    },
    'librt' => {
        'name' => 'rt',

        'symbol' => 'LIB_RT_',

        'auto configure' => {
            'include dir' => undef,
            'lib'         => undef,
        },

        'dummy file' => 'script/dummy/rt.c',

        'manual configure' => {
            'cflags'       => undef,
            'include dirs' => undef,
            'lib'          => ['rt'],
        },

        'result' => {
            'include' => undef,
            'lib'     => undef,
        },
    },
    'libuuid' => {
        'name' => 'uuid',

        'symbol' => 'LIB_UUID_',

        'auto configure' => {
            'include dir' => [ 'pkg-config', '--variable=includedir', 'uuid' ],
            'lib'         => [ 'pkg-config', '--libs', 'uuid' ],
        },

        'dummy file' => 'script/dummy/uuid.c',

        'manual configure' => {
            'cflags'       => undef,
            'include dirs' => undef,
            'lib'          => ['uuid'],
        },

        'result' => {
            'include' => undef,
            'lib'     => undef,
        },
    },
    'libz' => {
        'name' => 'zlib',

        'symbol' => 'LIB_Z_',

        'auto configure' => {
            'include dir' => [ 'pkg-config', '--variable=includedir', 'zlib' ],
            'lib'         => [ 'pkg-config', '--libs', 'zlib' ],
        },

        'dummy file' => 'script/dummy/z.c',

        'manual configure' => {
            'cflags'       => undef,
            'include dirs' => undef,
            'lib'          => ['z'],
        },

        'result' => {
            'include' => undef,
            'lib'     => undef,
        },
    }
);

$| = 1;

open my $config, '>', 'configure.vars';

foreach my $lib_name ( sort keys %libs ) {
    my $lib = $libs{$lib_name};

    print "checking $lib->{name}...";

    if ( defined $lib->{'auto configure'}->{'include dir'} ) {
        my @include;

        run3 $lib->{'auto configure'}->{'include dir'}, undef, \@include, \undef;

        if ( @include > 0 ) {
            chomp @include;
            ( $lib->{result}->{include} ) = @include;
        }
        elsif ( not $? ) {
            $lib->{result}->{include} = '';
        }
    }

    unless ( defined $lib->{result}->{include} ) {
        my @cmds = ( 'gcc', '-c', '-o', '/tmp/a.o' );
        push @cmds, $lib->{'manual configure'}->{'cflags'}
            if defined $lib->{'manual configure'}->{'cflags'};
        push @cmds, $lib->{'dummy file'};

        run3 \@cmds, undef, \undef, \undef;

        unless ($?) {
            $lib->{result}->{include} = '';
        }
    }

    unless ( defined $lib->{result}->{include} ) {
        foreach my $include ( @{ $lib->{'manual configure'}->{'include dirs'} } ) {
            my @cmds = ( 'gcc', '-c', '-o', '/tmp/a.o', "-I$include" );
            push @cmds, $lib->{'manual configure'}->{'cflags'}
                if defined $lib->{'manual configure'}->{'cflags'};
            push @cmds, $lib->{'dummy file'};

            run3 \@cmds, undef, \undef, \undef;

            unless ($?) {
                $lib->{result}->{include} = "-I$include";
                last;
            }
        }
    }

    die " failed" unless defined $lib->{result}->{include};

    if ( length($lib->{result}->{include}) > 0 and not $lib->{result}->{include} =~ /-I/ ) {
        $lib->{result}->{include} = '-I' . $lib->{result}->{include};
    }

    if ( defined $lib->{'auto configure'}->{lib} ) {
        my @library;

        run3 $lib->{'auto configure'}->{lib}, undef, \@library, \undef;

        if ( @library > 0 ) {
            chomp @library;
            ( $lib->{result}->{lib} ) = @library;
        }
        elsif ($?) {
            $lib->{result}->{lib} = '';
        }
    }

    unless ( defined $lib->{result}->{lib} ) {
        my @cmds = ( 'gcc', '-o', '/tmp/a.out' );
        push @cmds, $lib->{result}->{include}
            if defined $lib->{result}->{include} and length($lib->{result}->{include}) > 0;
        push @cmds, $lib->{'dummy file'};

        run3 \@cmds, undef, \undef, \undef;

        $lib->{result}->{lib} = '' unless $?;
    }

    unless ( defined $lib->{result}->{lib} ) {
        foreach my $lib_bin ( @{ $lib->{'manual configure'}->{lib} } ) {
            my @cmds = ( 'gcc', '-o', '/tmp/a.out' );
            push @cmds, $lib->{result}->{include}
                if defined $lib->{result}->{include} and length($lib->{result}->{include}) > 0;
            push @cmds, $lib->{'dummy file'}, "-l$lib_bin";

            run3 \@cmds, undef, \undef, \undef;

            unless ($?) {
                $lib->{result}->{lib} = "-l$lib_bin";
                last;
            }
        }
    }

    if ( defined $lib->{result}->{include} ) {
        print $config "$lib->{'symbol'}CFLAGS=";
        print $config $lib->{result}->{include}
            if defined $lib->{result}->{include};
        print $config "\n";
    }
    else {
        print $config "$lib->{'symbol'}CFLAGS=\n";
    }

    if ( defined $lib->{result}->{lib} ) {
        if ( length($lib->{result}->{lib}) and not $lib->{result}->{lib} =~ /-l/ ) {
            $lib->{result}->{lib} = '-l' . $lib->{result}->{lib};
        }

        print $config "$lib->{'symbol'}LD=$lib->{result}->{lib}\n";
    }
    else {
        print $config "$lib->{'symbol'}LD=\n";
    }

    print $config "\n";

    print "\n";
}

close $config;

