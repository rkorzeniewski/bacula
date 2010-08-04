#!/usr/bin/perl -w

=head1 DESCRIPTION

    check_bacula.pl -- check for bacula status

=head2 USAGE

    check_bacula.pl ...
      -C|--Client=ss       List of clients
      -g|--group=ss        List of groups

      -l|--level=F/I/D     Specify job level

      -w|--warning=i       warning threshold (jobs)
      -c|--critical=i      critical threshold (jobs)

      -S|--Storage=ss      List of SDs to test

      -s|--scratch=i       threshold scratch number
      -m|--mediatype=ss    Media type to check for scratch

      -R|--Running=i       Test for maximum running jobs for a period (in hours)
      -F|--Failed=i        Test for failed jobs after a period (in mins)

=head3 EXAMPLES

   Check :
      - if more than 10 jobs are running for client c1 and c2 for 1 hour
    check_bacula.pl -R 1 -C c1 -C c2 -w 10 -c 15

      - if more than 10 jobs are running for group g1 for 2 hours
    check_bacula.pl -R 2 -g g1 -w 10 -c 15

      - if more than 10 jobs are failed of canceled for 2 hours for group g1
    check_bacula.pl -F 120 -g g1 -w 10 -c 15

      - if S1_LTO1 and S1_LTO2 storage deamon are responding to status cmd
    check_bacula.pl -S S1_LTO1 -S S1_LTO2

      - if the scratch pool contains 5 volumes with mediatype Tape% at minimum
    check_bacula.pl -s 2 -m Tape%

   You can mix all options

   check_bacula.pl -g g1 -w 10 -c 15 -S S1_LTO1 -s 2 -m Tape%

      - if we have more than 10 jobs in error or already running for 2 hours
   check_bacula.pl -R 2 -F 20 -w 10 -c 15

=head1 LICENSE

   Bweb - A Bacula web interface
   BaculaÂ® - The Network Backup Solution

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.

   The main author of Bweb is Eric Bollengier.
   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.

=cut

use strict ;
use Getopt::Long ;
use Bweb;
use CGI;
use POSIX qw/strftime/;
use Getopt::Long qw/:config no_ignore_case/;
use Pod::Usage;

my $config_file = $Bweb::config_file;
my (@client, @group, $help, $query, $verbose, @msg, @storage);

my $max_run;
my $test_failed;
my $mediatype='%';		# check for all mediatype
my $nb_scratch;                 # check for scratch media
my $crit = 10;
my $warn = 5;
my $res;
my $ret=0;
my $level;
my $timeout=50;			# timeout for storage status command

GetOptions("Client=s@"  => \@client,
	   "group=s@"   => \@group,
	   "scratch=i" => \$nb_scratch,
	   "warning=i" => \$warn,
	   "critical=i"=> \$crit,
	   "verbose"   => \$verbose,
	   "timeout=i" => \$timeout,
	   "Storage=s@"=> \@storage,
	   "mediatype=s"=> \$mediatype,
           "Runing:45"  => \$max_run,
           "Failed:12"  => \$test_failed,
           "level=s"   => \$level,
	   "help"      => \$help) 
    || Pod::Usage::pod2usage(-exitval => 2, -verbose => 1) ;

Pod::Usage::pod2usage(-verbose => 1) if ( $help ) ;

my $conf = new Bweb::Config(config_file => $config_file);
$conf->load();
my $bweb = new Bweb(info => $conf);

my $b = $bweb->get_bconsole();
$b->{timeout} = $timeout;

CGI::param(-name=> 'client',-value => \@client);
CGI::param(-name=> 'client_group', -value => \@group);
CGI::param(-name=> 'level', -value => $level);

my ($where, undef) = $bweb->get_param(qw/clients client_groups level/);

my $c_filter ="";
my $g_filter = "";

if (@client) {
    $c_filter = " JOIN Client USING (ClientId) ";
}

if (@group) {
    $g_filter = " JOIN client_group_member USING (ClientId) " .
                " JOIN client_group USING (client_group_id) ";
}

################################################################
# check if more than X jobs are running or just created
# for too long (more than 2 hours) since Y ago

if ($max_run) {
    my $trig = time - $max_run*60;

    $query = "
SELECT count(1) AS nb
  FROM Job $c_filter $g_filter

 WHERE JobStatus IN ('R', 'C')
   AND Type = 'B'
   AND JobTDate < $trig
 $where
";
    $res = $bweb->dbh_selectrow_hashref($query);
    if ($res) {
        my $nb = $res->{nb};
        if ($nb >= $crit) {
            push @msg, "$nb jobs are running (${max_run}m)";
            $ret = 2;
        } elsif ($nb >= $warn) {
            push @msg, "$nb jobs are running (${max_run}m)";
            $ret = ($ret>1)?$ret:1;
        }
    }
}

################################################################
# check failed jobs (more than X) since x time ago

if ($test_failed) {
    my $since = time - $test_failed*60*60;

    $query = "
SELECT count(1) AS nb
  FROM Job $c_filter $g_filter

 WHERE JobStatus IN ('E','e','f','A')
   AND Type = 'B'
   AND JobTDate > $since
 $where
";
    $res = $bweb->dbh_selectrow_hashref($query);
    if ($res) {
        my $nb = $res->{nb};
        if ($nb >= $crit) {
            push @msg, "$nb jobs are in error (${test_failed}h)";
            $ret = 2;
        } elsif ($nb >= $warn) {
            push @msg, "$nb jobs are in error (${test_failed}h)";
            $ret = ($ret>1)?$ret:1;
        }
    }
}

################################################################
# check storage status command

foreach my $st (@storage) {

    my $out = $b->send_cmd("status storage=\"$st\"");
    if (!$out || $out !~ /Attr spooling|JobId/) {
	push @msg, "timeout ($timeout s) or bad response on status storage $st";
	$ret = 2;
    }
}

################################################################
# check for Scratch volume

if ($nb_scratch) {
    $query = "
  SELECT MediaType AS mediatype, count(MediaId) AS nb
    FROM Media JOIN Pool USING (PoolId)
   WHERE Pool.Name = 'Scratch'
     AND Media.MediaType LIKE '$mediatype'
         
  GROUP BY MediaType
";

    $res = $bweb->dbh_selectall_hashref($query, 'mediatype');
    if ($res && keys %$res) {
        foreach my $k (keys %$res) {
            if ($res->{$k}->{nb} < $nb_scratch) {
                push @msg, "no more scratch for $k ($res->{$k}->{nb})";
                $ret = 2;
            }
        }
    } else { # query doesn't report anything...
        push @msg, "no more scratch for $mediatype";
        $ret = 2;
    }
}

################################################################
# print result

if (!$ret) {
    print "OK - All checks ok\n";
} elsif ($ret == 1) {
    print "WARNING - ", join(", ", @msg), "\n";
} else {
    print "CRITICAL - ", join(", ", @msg), "\n";
}
exit $ret;

