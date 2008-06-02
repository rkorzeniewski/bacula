#!/usr/bin/perl -w

=head1 DESCRIPTION

    check_bacula.pl -- check for bacula status

=head2 USAGE

    check_bacula.pl ...
      -C|--Client=ss       List of clients
      -g|--group=ss        List of groups

      -a|--age=i           Age in hours (default 10h)
      -w|--warning=i       warning threshold (jobs)
      -c|--critical=i      critical threshold (jobs)

      -S|--Storage=ss      List of SDs to test

      -s|--scratch=i       threshold scratch number
      -m|--mediatype=ss    Media type to check for scratch


=head3 EXAMPLES

    check_bacula.pl -C c1 -C c2 -w 10 -c 15 -S S1_LTO1 -S S1_LTO2 -m 2 -m S%

=head1 LICENSE

   Bweb - A Bacula web interface
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2008 Free Software Foundation Europe e.V.

   The main author of Bweb is Eric Bollengier.
   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version two of the GNU General Public
   License as published by the Free Software Foundation plus additions
   that are listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of John Walker.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zurich,
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

my $mediatype='%';		# check for all mediatype
my $nb_scratch=5;		# check for more than 5 scratch media
my $crit = 10;
my $warn = 5;
my $age;
my $res;
my $ret=0;
my $timeout=50;			# timeout for storage status command

GetOptions("Client=s@"  => \@client,
	   "group=s@"   => \@group,
	   "age=i"     => \$age,
	   "scratch=i" => \$nb_scratch,
	   "warning=i" => \$warn,
	   "critical=i"=> \$crit,
	   "verbose"   => \$verbose,
	   "timeout=i" => \$timeout,
	   "Storage=s@"=> \@storage,
	   "mediatype=s"=> \$mediatype,
	   "help"      => \$help) 
    || Pod::Usage::pod2usage(-exitval => 2, -verbose => 1) ;

Pod::Usage::pod2usage(-verbose => 1) if ( $help ) ;

if ($age) {
    $age *= 60*60;
} else {
    $age = 10*60*60;
}

my $since = time - $age;
my $trig = time - 2*60*60;

my $conf = new Bweb::Config(config_file => $config_file);
$conf->load();
my $bweb = new Bweb(info => $conf);

my $b = $bweb->get_bconsole();
$b->{timeout} = $timeout;

CGI::param(-name=> 'client',-value => \@client);
CGI::param(-name=> 'client_group', -value => \@group);

my ($where, undef) = $bweb->get_param(qw/clients client_groups/);

################################################################
# check if more than X jobs are running for too long (more than
# 2 hours) since Y ago

$query = "
SELECT count(1) AS nb
  FROM Job 
 WHERE JobStatus = 'R'
   AND Type = 'B'
   AND JobTDate > $since
   AND JobTDate < $trig
 $where
";

$res = $bweb->dbh_selectrow_hashref($query);
if ($res) {
    my $nb = $res->{nb};
    if ($nb > $crit) {
	push @msg, "$nb jobs are running";
	$ret = 2;
    } elsif ($nb > $warn) {
	push @msg, "$nb jobs are running";
	$ret = ($ret>1)?$ret:1;
    }
}

################################################################
# check failed jobs (more than X) since x time ago

$query = "
SELECT count(1) AS nb
  FROM Job 
 WHERE JobStatus IN ('E','e','f','A')
   AND Type = 'B'
   AND JobTDate > $since
 $where
";

$res = $bweb->dbh_selectrow_hashref($query);
if ($res) {
    my $nb = $res->{nb};
    if ($nb > $crit) {
	push @msg, "$nb jobs are in error";
	$ret = 2;
    } elsif ($nb > $warn) {
	push @msg, "$nb jobs are in error";
	$ret = ($ret>1)?$ret:1;
    }
}

################################################################
# check storage status command

foreach my $st (@storage) {

    my $out = $b->send_cmd("status storage=\"$st\"");
    if (!$out || $out !~ /Attr spooling/) {
	push @msg, "timeout ($timeout s) on status storage $st";
	$ret = 2;
    }
}

################################################################
# check for Scratch volume

$query = "
  SELECT MediaType AS mediatype, count(MediaId) AS nb
    FROM Media JOIN Pool USING (PoolId)
   WHERE Pool.Name = 'Scratch'
     AND Media.MediaType LIKE '$mediatype'
         
  GROUP BY MediaType
";

$res = $bweb->dbh_selectall_hashref($query, 'mediatype');
if ($res) {
    foreach my $k (keys %$res) {
	if ($res->{$k}->{nb} < $nb_scratch) {
	    push @msg, "no more scratch for $k ($res->{$k}->{nb})";
	    $ret = 2;
	}
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

