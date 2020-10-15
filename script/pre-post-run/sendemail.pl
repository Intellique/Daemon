#!/usr/bin/perl

use strict;
use warnings;

use JSON::PP;
use Data::Dumper;
use Email::Sender::Simple qw(sendmail);
use Email::Simple;

my $senderemail = 'Storiq One report <storiqone@intellique.com>';

if ( @ARGV and $ARGV[0] eq 'config' ) {
    my $sent = {
        'name' => 'Send email after job',
        'description' =>
          'Send an email report to the user when the job is complete',
        'type' => 'post job'
    };
    print encode_json($sent);
    exit;
}

my $data_in = do { local $/; <STDIN> };
my $data    = decode_json $data_in;

my $jobname  = $data->{job}{name};
my $jobtype  = $data->{job}{type};
my $jobuser  = $data->{job}{user};
my $jobemail = $data->{job}{email};

my $jobstatus = $data->{job}{status};

# for RESTORE and CHECK job, we must compute a report checksum status
my $report = '';

if ( exists $data->{archive}{volumes} ) {
    foreach my $vol ( @{ $data->{archive}{volumes} } ) {
        if ( $vol->{"checksum time"} > 0 and $vol->{"checksum ok"} == 0 ) {
            $report .=
              'Volume ' . $vol->{sequence} . " FAILED. Checksum mismatch.\n";
        }

        foreach my $ptrfile ( @{ $vol->{files} } ) {
            my $file = $ptrfile->{file};
            if (    $file->{"checksum time"} > 0
                and $file->{"checksum ok"} eq 'false' )
            {
                $report .=
                  "File '" . $file->{path} . "' FAILED. Checksum mismatch.\n";
            }
        }
    }
}

my $message = Email::Simple->create(
    header => [
        To      => "$jobuser <$jobemail>",
        From    => $senderemail,
        Subject => "Job '$jobname' done with status '$jobstatus'",
    ],
    body => "The Storiq One job '$jobname' of type '$jobtype' is done.
	Status : $jobstatus
	Report: \n$report"

);

sendmail($message);

my $sent = {
    'finished' => JSON::PP::true,
    'data'     => {},
    'message'  => $report
};
print encode_json($sent);
