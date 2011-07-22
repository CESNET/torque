package CRI::Util::Mail;

use strict;
use warnings;

use TestLibFinder;
use lib test_lib_loc();

use CRI::Test;
use Data::Dumper;

use base 'Exporter';

our @EXPORT_OK = qw(
                    parse_mail_out
                    find_mail
                    grep_mail
                    delete_mail
                   );

my $mail_out = "/tmp/mail.out";

#------------------------------------------------------------------------------
# my @mail = parse_mail_out();
# my @mail = parse_mail_out({
#                            'file' => '/tmp/mail.txt'
#                          });
#------------------------------------------------------------------------------
#
# Reads the text file generated by contrig/mail.pl and returns an array of 
# hashrefs representing each mail message.
#
#------------------------------------------------------------------------------
sub parse_mail_out #($)
  {

  my ($params) = @_;

  # Parameters
  my $file = $params->{ 'file' } || $mail_out;

  # Variables
  my @rtn_array = ();
  my %tmp_hash  = ();
  my @lines     = ();

  # Open the file
  open(MAIL, "<$file")
    or die "Unable to open mail file '$file': $!";

  @lines = <MAIL>;

  close MAIL;

  foreach my $line (@lines)
    {

    next 
      if $line eq '';

    # Record the tmp_hash and reset it
    if ($line =~ /^Timestamp:/)
      {

      push @rtn_array, { %tmp_hash }
        if (scalar %tmp_hash); # Don't add empty hashes
 
      %tmp_hash = ();

      } # END  if ($line =~ /^TIMESTAMP:/) 

    # Check for a non-message attribute
    if ($line =~ /^(\S+)\:\s+(.*)/)
      {

      $tmp_hash{ $1 } = $2;

      } # END if ($line =~ /^(\S+)\:(.*)/)
    else
      {

      $tmp_hash{ "Message" } = ''
        unless defined $tmp_hash{ "Message" };

      $tmp_hash{ "Message" } .= $line;

      } # END 

    } # END foreach my $line (@lines)

  push @rtn_array, { %tmp_hash }
    if (scalar %tmp_hash); # Don't add empty hashes

  logMsg "Logging output of mail file $file:\n".Dumper(\@rtn_array);

  return @rtn_array;

  } # END sub parse_mail_out #($)

#------------------------------------------------------------------------------
# my $mail_href = find_mail({
#                            'field' => 'Subject',
#                            'value' => 'moab event - NOTIFY (node:notify)',
#                          });
# my $mail_href = find_mail({
#                            'field' => 'Subject',
#                            'value' => 'moab event - NOTIFY (node:notify)',
#                            'find_num' => -1,
#                            'mail_file' => '/tmp/mail.txt',
#                          });
#------------------------------------------------------------------------------
#
# Looks for mail_href in the array returned by parse_mail_out that matches the
# field and value returned.  Returns the hashref representing the mail message
#
# PARAMETERS
#  o field     - (REQUIRED) The field to look at when search for a certain
#                mail message.
#  o value     - (REQUIRED) The value of the field to match on
#  o find_num  - (OPTIONAL) Causes the subroutine to return the <find_num>th
#                mail message or last message matching.  If a negative number
#                is given search will begin from the back of the array.
#  o mail_file - (OPTIONAL) The file for parse_mail_out to parse.  
#
#------------------------------------------------------------------------------
sub find_mail #($)
  {

  my ($params) = @_;

  # Parameters
  my $field     = $params->{ 'field'     } || die "Please provide the search 'field'";
  my $value     = $params->{ 'value'     } || die "Please provide the search 'value'";
  my $find_num  = $params->{ 'find_num'  } || 1;
  my $mail_file = $params->{ 'mail_file' } || undef;

  die "'find_num' cannot be 0"
    if $find_num == 0;

  # Variables
  my @mail_hrefs    = parse_mail_out({ 'file' => $mail_file });

  my $rtn_mail_href = {};
  my $found         = 0;

  # Search backwards if find_num is negative
  @mail_hrefs = reverse @mail_hrefs
    if $find_num < 0;
  
  foreach my $mail_href (@mail_hrefs)
    {

    if ($mail_href->{ $field } eq $value)
      {
    
      $rtn_mail_href = $mail_href;
      $found++;

      } # END if ($mail_href->{ $field } eq $value)

    last 
      if $found == abs($find_num);

    } # END foreach my $mail_href (@mail_hrefs)

  return $rtn_mail_href;

  } # END sub find_mail #($)

#------------------------------------------------------------------------------
# my @mail = grep_mail({
#                          'grep_value' => 'moab event'
#                        });
# my @mail = grep_mail({
#                          'grep_field' => "To"
#                          'grep_value' => "testuser1",
#                          'mail_file' => "/tmp/mail.Tue_Jun_29_2010",
#                        });
#------------------------------------------------------------------------------
#
# Runs the parse_mail_out() subroutine and then greps through the resulting
# array for entries that match the given grep options.  Returns an array of 
# the results
#
# PARAMETERS:
#  o grep_field - (OPTIONAL) The mail file filed to use in the grep
#                            expression.  Defaults to 'Subject'.
#  o grep_value - (REQUIRED) The value we wish to look for in the given 
#                            grep_field.
#  o mail_file  - (OPTIONAL) The 'mail_file' parameter to pass to the 
#                            parse_mail_outs() subroutine. Defaults to the 
#                            file path returned by current_mail_file().
#
#------------------------------------------------------------------------------
sub grep_mail #()
  {

  my ($params) = @_;

  # Parameters
  my $mail_file  = $params->{ 'mail_file'  } || $mail_out;
  my $grep_field = $params->{ 'grep_field' } || 'Subject';
  my $grep_value = $params->{ 'grep_value' } || die "Please provide the 'grep_value'";

  # Variables
  my @mail     = parse_mail_out({ 'mail_file' => $mail_file });
  my @rtn_mail = ();

  @rtn_mail = grep { 
                       $_->{ $grep_field } eq $grep_value
                        if defined $_->{ $grep_field };
                     } @mail;

  return @rtn_mail;

  } # END sub grep_mail #()

#------------------------------------------------------------------------------
# delete_mail()
# delete_mail({
#              'file' => '/tmp/mail2.out' 
#            });
#------------------------------------------------------------------------------
#
# Creates a blank mail file
#
#------------------------------------------------------------------------------
sub delete_mail #($)
  {

  my ($params) = @_;

  my $file = $params->{ 'file' } || $mail_out;

  open(MAIL, ">$file")
    or die "Unable to open '$file': $!";

  print MAIL '';

  close MAIL;

  } # END sub delete_mail #($)

1;

__END__
