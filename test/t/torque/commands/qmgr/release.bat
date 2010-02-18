#!/usr/bin/perl
use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin/../../../../lib/";

use CRI::Test;
plan('no_plan');
setDesc("RELEASE qmgr Compatibility Tests");

my $testbase = $FindBin::Bin;

execute_tests("$testbase/setup.t") 
  or die("Couldn't setup for qmgr tests!");

execute_tests(
    "$testbase/qmgr_c_scheduling.t",# RT7049
    "$testbase/qmgr_c_queue.t",
);

execute_tests("$testbase/cleanup.t"); 
