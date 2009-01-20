#!/usr/bin/perl 

use CRI::Test;
plan('no_plan');
use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin/../../../../lib/";

setDesc('RELEASE tracejob Compatibility Tests');

my $testbase = $FindBin::Bin;


execute_tests("$testbase/setup.t") 
  or die('Could not setup tracejob tests');

execute_tests(
              "$testbase/tracejob.t",
             ); 

execute_tests("$testbase/cleanup.t"); 
