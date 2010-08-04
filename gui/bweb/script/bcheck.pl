#!/usr/bin/perl -w

=head1 DESCRIPTION


=head2 USAGE

    bcheck.pl [client=yes]

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
use Data::Dumper ;
use Bweb;
use CGI;
my $verbose;

my $conf = new Bweb::Config(config_file => $Bweb::config_file);
$conf->load();
my $bweb = new Bweb(info => $conf);

my $b = $bweb->get_bconsole();
my $ret = $b->director_get_sched(1);

if (CGI::param('client')) {

    print "
Check clients
-------------\n";

# check for next backup that client are online
    foreach my $elt (@$ret) {
	next unless ($elt->{name});
	
	my $job = $b->send_cmd("show job=\"$elt->{name}\"");
	my $obj = $bweb->run_parse_job($job);
	
#    print "I: test $obj->{client}\n" if $verbose;
	next unless ($obj->{client});
	my $out = $b->send_cmd("st client=\"$obj->{client}\"");
	if ($out !~ /Daemon started/m or $out !~ /^ Heap:/m) {
	    print "E: Can't connect to $obj->{client}\n";
	}
	exit 0;
    }
}

print "
Check for backup size
---------------------\n";

my $arg = $bweb->get_form('age', 'since');
my ($limit, $label) = $bweb->get_limit(age => $arg->{age},
				       since => $arg->{since});
my $query = "
SELECT Job.ClientId AS c0, Job.Name AS c1, 
       Client.Name AS c2
  FROM Job JOIN Client USING (ClientId)
 WHERE Type = 'B'
   AND JobStatus = 'T'
   AND Level = 'F'
   $limit
 GROUP BY Job.Name,Job.ClientId,Client.Name
";

my $old = $bweb->dbh_selectall_arrayref($query);

foreach my $elt (@$old) {
    $elt->[1] = $bweb->dbh_quote($elt->[1]);
    $query = "
SELECT JobId AS jobid, JobBytes AS jobbytes
  FROM Job
 WHERE Type = 'B'
   AND JobStatus = 'T'
   AND Level = 'F'
   AND Job.ClientId = $elt->[0]
   AND Job.Name = $elt->[1]
   $limit
 ORDER BY StartTime DESC
 LIMIT 1
";

    my $last_bkp = $bweb->dbh_selectrow_hashref($query);

    $query = "
SELECT COUNT(1) as nbjobs, AVG(JobBytes) as nbbytes
  FROM 
    (SELECT StartTime,JobBytes
       FROM Job
      WHERE JobId < $last_bkp->{jobid}
        AND Type = 'B'
        AND JobStatus = 'T'
        AND Level = 'F'
        AND ClientId = $elt->[0]
        AND Job.Name = $elt->[1]
      ORDER BY JobId DESC
      LIMIT 4
    ) AS T
";

    my $avg_bkp = $bweb->dbh_selectrow_hashref($query);

    if ($avg_bkp->{nbjobs} > 3) {
	if ($last_bkp->{jobbytes} > ($avg_bkp->{nbbytes} * 1.2)) {
	    print "W: Last backup $elt->[1] on $elt->[2] is greater than 20% (last=",
        	    &Bweb::human_size($last_bkp->{jobbytes}), " avg=",
		    &Bweb::human_size($avg_bkp->{nbbytes}), ")\n";
	}
	if ($last_bkp->{jobbytes} < ($avg_bkp->{nbbytes} * 0.8)) {
	    print "W: Last backup $elt->[1] on $elt->[2] is lower than 20% (last=",
        	    &Bweb::human_size($last_bkp->{jobbytes}), " avg=",
		    &Bweb::human_size($avg_bkp->{nbbytes}), ")\n";
	}
    }
}
print "
Check missing backup for 2 weeks
---------------------------------\n";

# we check that each job have been run one time for 2 weeks
my @jobs = $b->list_job();
($limit, $label) = $bweb->get_limit(age => 2*7*24*60*60);

foreach my $j (@jobs) {

    $j = $bweb->dbh_quote($j);
    $query = "
SELECT 1 as run
  FROM Job
 WHERE Type = 'B'
   AND JobStatus = 'T'
   AND Job.Name = $j
$limit
LIMIT 1
";
    my $row = $bweb->dbh_selectrow_hashref($query);

    if (!$row) {
	$query = "
SELECT StartTime AS starttime, Level AS level
  FROM Job
 WHERE Type = 'B'
   AND JobStatus = 'T'
   AND Job.Name = $j
LIMIT 1
";
	$row = $bweb->dbh_selectrow_hashref($query);
	print "W: No job for $j ";
	if ($row) {
	    print "($row->{level} $row->{starttime})";
	}
	print "\n";
    }
}

