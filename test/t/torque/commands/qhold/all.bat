#!/usr/bin/perl

use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin/../../../../lib/";

use CRI::Test;

plan('no_plan');
setDesc("ALL qhold tests");

my $testbase = $FindBin::Bin;

# Setup
execute_tests(
               "$testbase/setup.t"
             );

# Run the tests
execute_tests(
               "$testbase/qhold.t",
               "$testbase/qhold_h_u.t",
               "$testbase/qhold_h_o.t",
               "$testbase/qhold_h_s.t",
               "$testbase/qhold_h_sou.t",
             );

# Cleanup
execute_tests(
               "$testbase/cleanup.t"
             );
