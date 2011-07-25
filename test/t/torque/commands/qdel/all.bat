#!/usr/bin/perl

use strict;
use warnings;

use FindBin;
use TestLibFinder;
use lib test_lib_loc();

use CRI::Test;
plan('no_plan');
setDesc("ALL qdel Tests");

my $testbase = $FindBin::Bin;

execute_tests("$testbase/setup.t") 
  or die("Couldn't setup for qdel tests!");

execute_tests(
              "$testbase/qdel.t",
              "$testbase/qdel_m.t",
              "$testbase/qdel_p.t",
              "$testbase/qdel_t.t",
              "$testbase/qdel_cW.t",
);

execute_tests("$testbase/cleanup.t"); 
