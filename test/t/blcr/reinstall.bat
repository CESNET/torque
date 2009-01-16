#!/usr/bin/perl

use strict;
use warnings;

use CRI::Test;

plan('no_plan');
setDesc("Reinstall 'Berkley Lab Checkpoint/Restart (BLCR) for Linux'");

my $testbase = $props->get_property('test.base') . "blcr";

execute_tests(
        "$testbase/uninstall/uninstall.bat",
        "$testbase/install/install.bat",
        );
