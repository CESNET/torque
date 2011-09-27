#!/usr/bin/perl
use strict;
use warnings;

use FindBin;
use TestLibFinder;
use lib test_lib_loc();

my $testbase = $FindBin::Bin;

use CRI::Test;

plan('no_plan');
setDesc("RELEASE pbsnodes Compatibility Tests");

execute_tests("$testbase/setup.t") 
  or die("Couldn't setup for pbsnodes tests!");

execute_tests(
    "$testbase/pbsnodes.t",
    "$testbase/pbsnodes_a.t",
# JR-TRQ-404    "$testbase/pbsnodes_c.t",
#    "$testbase/pbsnodes_d.t", # Not implemented yet
# JR-TRQ-404    "$testbase/pbsnodes_l.t",
    "$testbase/pbsnodes_o.t",
#    "$testbase/pbsnodes_p.t", # Not implemented yet
    "$testbase/pbsnodes_q.t",
    "$testbase/pbsnodes_s.t",
    "$testbase/pbsnodes_x.t",
    "$testbase/pbsnodes_a_q.t",
    "$testbase/pbsnodes_c_q.t",
    "$testbase/pbsnodes_l_q.t",
    "$testbase/pbsnodes_s_q.t",
    "$testbase/pbsnodes_x_q.t",
    "$testbase/pbsnodes_l_n.t",
# JR-TRQ-404    "$testbase/pbsnodes_o_cN.t",
    "$testbase/pbsnodes_s_a.t",
# JR-TRQ-404    "$testbase/pbsnodes_s_c.t",
# JR-TRQ-404    "$testbase/pbsnodes_s_l.t",
    "$testbase/pbsnodes_s_x.t",
    "$testbase/pbsnodes_s_a_q.t",
    "$testbase/pbsnodes_s_c_q.t",
    "$testbase/pbsnodes_s_l_q.t",
    "$testbase/pbsnodes_s_x_q.t",
);

execute_tests("$testbase/cleanup.t"); 
