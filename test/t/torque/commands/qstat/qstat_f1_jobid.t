#!/usr//bin/perl

use strict;
use warnings;

use FindBin;
use TestLibFinder;
use lib test_lib_loc();


# Test Modules
use CRI::Test;


use Torque::Job::Ctrl          qw(
                                   submitSleepJob
                                   runJobs
                                   delJobs 
                                 );
use Torque::Util::Regexp       qw(
                                   QSTAT_F_REGEXP
                                 );
use Torque::Util        qw(
                                   run_and_check_cmd
                                   job_info
                                 );
use Torque::Test::Qstat::Utils qw(
                                   parse_qstat_f1
                                 );


# Test Description
plan('no_plan');
setDesc("qstat -f -1 job_identifier");

# Variables
my $cmd;
my %qstat;
my @job_ids;
my %job_info;
my $job_params;

my $user       = $props->get_property( 'User.1' );
my $torque_bin = $props->get_property( 'Torque.Home.Dir' ) . '/bin/';

my @attributes = qw(
                     job_name
                     job_owner
                     job_state
                     queue
                     server
                     checkpoint
                     ctime
                     error_path
                     hold_types
                     join_path
                     keep_files
                     mail_points
                     mtime
                     output_path
                     priority
                     qtime
                     rerunable
                     resource_list.neednodes
                     resource_list.nodect
                     resource_list.nodes
                     resource_list.walltime
                     substate
                     variable_list
                     euser
                     egroup
                     queue_rank
                     queue_type
                     etime
                   );

# Submit a job
$job_params = {
                'user'       => $user,
                'torque_bin' => $torque_bin
              };

# submit a couple of jobs
push(@job_ids, submitSleepJob($job_params));
push(@job_ids, submitSleepJob($job_params));

foreach my $job_id (@job_ids)
  {

  my $msg = "Checking job '$job_id'";
  diag($msg);
  logMsg($msg);

  # Test qstat
  $cmd   = "qstat -f -1 $job_id";
  %qstat = run_and_check_cmd($cmd);

  %job_info = parse_qstat_f1( $qstat{ 'STDOUT' } );

  foreach my $attribute (@attributes)
    {

    my $reg_exp = &QSTAT_F_REGEXP->{ $attribute };
    ok($job_info{ $job_id }{ $attribute } =~ /${reg_exp}/, "Checking '$job_id' $attribute attribute"); 

    } # END foreach my $attribute (@attributes)

  } # END foreach my $job_id (@job_ids)

# Delete the job
delJobs(@job_ids);
