#!/usr/bin/perl 

use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin/../../../../lib/";

use CRI::Test;

plan('no_plan');
setDesc('RELEASE qterm Compatibility Tests');

my $testbase = $FindBin::Bin;

execute_tests("$testbase/setup.t") 
  or die('Could not setup qterm tests');

execute_tests(
              "$testbase/qterm.t",
              "$testbase/qterm_t_quick.t",
              "$testbase/checkpoint/qterm_t_immediate.t",
              "$testbase/checkpoint/qterm_t_delay.t",
              "$testbase/rerunable/qterm_t_immediate.t",
              "$testbase/rerunable/qterm_t_delay.t",
             ); 

execute_tests("$testbase/cleanup.t"); 
