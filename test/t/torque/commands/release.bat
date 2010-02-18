#!/usr/bin/perl 

use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin/../../../lib/";

use CRI::Test;

plan('no_plan');
setDesc('RELEASE Torque Compatability Tests');

my $testbase = $FindBin::Bin;

execute_tests(
              "$testbase/momctl/release.bat",
# RT7048              "$testbase/qalter/release.bat",
              "$testbase/qchkpt/release.bat",
              "$testbase/qdel/release.bat",
# RT7048              "$testbase/qhold/release.bat",
              "$testbase/qmgr/release.bat",
              "$testbase/qrerun/release.bat",
              "$testbase/qrls/release.bat",
              "$testbase/qrun/release.bat",
# RT7048              "$testbase/qsig/release.bat",
              "$testbase/qstat/release.bat",# RT7049
              "$testbase/qsub/release.bat",
# RT7048              "$testbase/qterm/release.bat",
              "$testbase/tracejob/release.bat",
             ); 

# Tests that can make the environment unstable if they fail.  We want to limit
# their impact on the test suite
execute_tests(
              "$testbase/pbsnodes/release.bat",
              "$testbase/pbs_sched/release.bat",# RT7049
              "$testbase/pbs_mom/release.bat",
              "$testbase/pbs_server/release.bat",# RT7049
             );
