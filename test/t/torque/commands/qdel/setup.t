#!/usr/bin/perl

use strict;
use warnings;

use FindBin;
use TestLibFinder;
use lib test_lib_loc();


use CRI::Test;
use Torque::Ctrl        qw( startTorqueClean
                            stopTorque 
                            stopPbssched
                          );
use Torque::Util qw( 
                            list2array             
                          );

# Describe Test
plan('no_plan');
setDesc('Qdel Setup');

# Torque params
my @remote_moms    = list2array($props->get_property('Torque.Remote.Nodes'));
my $torque_params  = {
                     'remote_moms' => \@remote_moms
                     };

###############################################################################
# Stop Torque
###############################################################################
stopTorque($torque_params) 
  or die 'Unable to stop Torque';

###############################################################################
# Stop pbs_sched
###############################################################################
stopPbssched() 
  or die 'Unable to stop pbs_sched';

###############################################################################
# Restart Torque
###############################################################################
startTorqueClean($torque_params)
  or die 'Unable to start Torque';
