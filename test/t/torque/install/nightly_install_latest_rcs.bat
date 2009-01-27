#!/usr/bin/perl 

use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin/../../../lib/";

use CRI::Test;

plan('no_plan');
setDesc('Install TORQUE (NIGHTLY)');

my $testbase = $FindBin::Bin;

execute_tests(
"$testbase/configure.t",
"$testbase/make_clean.t",
"$testbase/make.t",
"$testbase/make_install.t",
"$testbase/setup.t",
"$testbase/config_mom.t",
"$testbase/nightly_remote_install_torques.t",
"$testbase/config_server.t",
"$testbase/create_torque_conf.t",
"$testbase/check_blcr.t",
"$testbase/startup.t",
);
