#!/usr/bin/perl 

use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin/../../../lib/";

use CRI::Test;

plan('no_plan');
setDesc('Install Latest Torque');

my $testbase = $FindBin::Bin;

execute_tests(
"$testbase/configure.t",
"$testbase/make_clean.t",
"$testbase/make.t",
"$testbase/make_install.t",
"$testbase/setup.t",
"$testbase/config_mom.t",
"$testbase/remote_install_torques.t",
"$testbase/config_server.t",
"$testbase/create_torque_conf.t",
"$testbase/startup.t",
);
