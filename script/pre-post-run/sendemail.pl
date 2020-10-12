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

my $jobname = $data->{job}{name};
my $jobtype = $data->{job}{type};
my $jobuser = $data->{job}{user};
my $jobemail = $data->{job}{email};

my $jobstatus = $data->{job}{status};

my $message = Email::Simple->create(
	header => [
		To => "$jobuser <$jobemail>",
		From => $senderemail,
		Subject => "Job '$jobname' done with status '$jobstatus'",
	],
	body => "The Storiq One job '$jobname' of type '$jobtype' is done.
	Status : $jobstatus\n"
	
);

sendmail($message);

my $sent = {
    'finished' => JSON::PP::true,
    'data'     => {},
    'message'  => ''
};
print encode_json($sent);
