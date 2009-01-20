#!/usr/bin/perl

use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin/../../../../lib/";


# Test Modules
use CRI::Test;

use Torque::Job::Ctrl           qw(
                                    submitSleepJob
                                    runJobs
                                    delJobs
                                  );
use Torque::Test::Utils         qw( run_and_check_cmd 
                                    list2array        );
use Torque::Test::Qstat::Utils  qw( parse_qstat_fx    );

# Test Description
plan('no_plan');
setDesc("qalter -m");

# Variables
my $m_cmd;
my $fx_cmd;
my %qstat;
my %qstat_fx;
my %qalter;
my $job_id;
my $mail_points;

# Submit the jobs
my $job_params = {
                   'user'       => $props->get_property('torque.user.one'),
                   'torque_bin' => $props->get_property('torque.home.dir') . '/bin/'
                 };

$job_id = submitSleepJob($job_params);

# Run a job
runJobs($job_id);

# Check qalter -m a
$mail_points = 'a';
$m_cmd       = "qalter -m $mail_points $job_id";
%qalter      = run_and_check_cmd($m_cmd);
 
$fx_cmd      = "qstat -f -x $job_id";
%qstat       = run_and_check_cmd($fx_cmd);
%qstat_fx    = parse_qstat_fx($qstat{ 'STDOUT' });
ok($qstat_fx{ $job_id }{ 'Mail_Points' } eq $mail_points, "Checking if '$m_cmd' was successful");

# Check qalter -m b
$mail_points = 'b';
$m_cmd       = "qalter -m $mail_points $job_id";
%qalter      = run_and_check_cmd($m_cmd);

$fx_cmd      = "qstat -f -x $job_id";
%qstat       = run_and_check_cmd($fx_cmd);
%qstat_fx    = parse_qstat_fx($qstat{ 'STDOUT' });
ok($qstat_fx{ $job_id }{ 'Mail_Points' } eq $mail_points, "Checking if '$m_cmd' was successful");

# Check qalter -m e
$mail_points = 'e';
$m_cmd       = "qalter -m $mail_points $job_id";
%qalter      = run_and_check_cmd($m_cmd);

$fx_cmd      = "qstat -f -x $job_id";
%qstat       = run_and_check_cmd($fx_cmd);
%qstat_fx    = parse_qstat_fx($qstat{ 'STDOUT' });
ok($qstat_fx{ $job_id }{ 'Mail_Points' } eq $mail_points, "Checking if '$m_cmd' was successful");

# Check qalter -m abe
$mail_points = 'abe';
$m_cmd       = "qalter -m $mail_points $job_id";
%qalter      = run_and_check_cmd($m_cmd);

$fx_cmd      = "qstat -f -x $job_id";
%qstat       = run_and_check_cmd($fx_cmd);
%qstat_fx    = parse_qstat_fx($qstat{ 'STDOUT' });
ok($qstat_fx{ $job_id }{ 'Mail_Points' } eq $mail_points, "Checking if '$m_cmd' was successful");


# Check qalter -m n
$mail_points = 'n';
$m_cmd     = "qalter -m $mail_points $job_id";
%qalter    = run_and_check_cmd($m_cmd);

$fx_cmd   = "qstat -f -x $job_id";
%qstat    = run_and_check_cmd($fx_cmd);
%qstat_fx = parse_qstat_fx($qstat{ 'STDOUT' });
ok($qstat_fx{ $job_id }{ 'Mail_Points' } eq $mail_points, "Checking if '$m_cmd' was successful");
# Delete the jobs
delJobs($job_id);
