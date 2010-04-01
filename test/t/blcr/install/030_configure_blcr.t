#!/usr/bin/perl
use strict;
use warnings;

use FindBin;
use lib "$FindBin::Bin/../../../lib/";

use CRI::Test;
plan('no_plan');
setDesc('Run the ./configure script for BLCR');

# Variables
my $blrc_prefix        = $props->get_property('blcr.home.dir');
my $blcr_extracted_dir = $props->get_property('blcr.build.dir');
my $configure_script   = "configure";

#diag $blcr_extracted_dir;
#ok(-e "$blcr_extracted_dir/Makefile","Check for the existence of the 'Makefile'")
#  or die 'Fatal Failure!';

# cd to the blcr directory
ok(chdir $blcr_extracted_dir, "Changing directory to '$blcr_extracted_dir'")
  or die "Unable to switch to directory '$blcr_extracted_dir'";

my $configure_cmd = "./$configure_script --prefix=$blrc_prefix";

my %configure     = runCommand($configure_cmd, test_success_die => 1);

ok(`grep "prefix='$blrc_prefix'" $blcr_extracted_dir/config.log`, "Check the prefix variable in the 'config.log'" );
