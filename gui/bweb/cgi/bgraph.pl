#!/usr/bin/perl -w
use strict;

=head1 LICENSE

   Bweb - A Bacula web interface
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2006 Free Software Foundation Europe e.V.

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

=head1 VERSION

    $Id$

=cut

use Bweb;

use Data::Dumper;
use CGI;

use POSIX qw/strftime/;
use File::Basename qw/basename dirname/;

my $conf = new Bweb::Config(config_file => $Bweb::config_file);
$conf->load();
my $bweb = new Bweb(info => $conf);
$bweb->connect_db();
my $dbh = $bweb->{dbh};
my $debug = $bweb->{debug};

# Job table keep use Media or Job retention, so it's quite enought
# for good statistics
# CREATE TABLE job_old (LIKE Job);
# INSERT INTO job_old
#    (SELECT * FROM Job WHERE JobId NOT IN (SELECT JobId FROM job_old) );
my $jobt = $conf->{stat_job_table} || 'Job';

my $graph = CGI::param('graph') || 'job_size';
my $legend = CGI::param('legend') || 'on' ;
$legend = ($legend eq 'on')?1:0;

my $arg = $bweb->get_form(qw/width height limit offset age where jobid
			     jfilesets level status jjobnames jclients jclient_groups/);

my ($limitq, $label) = $bweb->get_limit(age   => $arg->{age},
					limit => $arg->{limit},
					offset=> $arg->{offset},
					order => "$jobt.StartTime ASC",
					);

my $statusq='';
if ($arg->{status} and $arg->{status} ne 'Any') {
    $statusq = " AND $jobt.JobStatus = '$arg->{status}' ";
}
    
my $levelq='';
if ($arg->{level} and $arg->{level} ne 'Any') {
    $levelq = " AND $jobt.Level = '$arg->{level}' ";
} 

my $filesetq='';
if ($arg->{jfilesets}) {
    $filesetq = " AND FileSet.FileSet IN ($arg->{qfilesets}) ";
} 

my $jobnameq='';
if ($arg->{jjobnames}) {
    $jobnameq = " AND $jobt.Name IN ($arg->{jjobnames}) ";
} else {
    $arg->{jjobnames} = 'all';	# skip warning
} 

my $clientq='';
if ($arg->{jclients}) {
    $clientq = " AND Client.Name IN ($arg->{jclients}) ";
} else {
    $arg->{jclients} = 'all';	# skip warning
}

my $groupf='';			# from clause
my $groupq='';			# whre clause
if ($arg->{jclient_groups}) {
    $groupf = " JOIN client_group_member ON (Client.ClientId = client_group_member.clientid) 
                JOIN client_group USING (client_group_id)";
    $groupq = " AND client_group_name IN ($arg->{jclient_groups}) ";
}

$bweb->can_do('r_view_job');
my $filter = $bweb->get_client_filter();

my $gtype = CGI::param('gtype') || 'bars';

print CGI::header('image/png');

sub get_graph
{
    my (@options) = @_;
    my $graph;
    if ($gtype eq 'lines') {
	use GD::Graph::lines;
	$graph = GD::Graph::lines->new ( $arg->{width}, $arg->{height} );

    } elsif ($gtype eq 'bars') {
	use GD::Graph::bars;
	$graph = GD::Graph::bars->new ( $arg->{width}, $arg->{height} );

    } elsif ($gtype eq 'linespoints') {
	use GD::Graph::linespoints;
	$graph = GD::Graph::linespoints->new ( $arg->{width}, $arg->{height} );

#   this doesnt works at this time
#    } elsif ($gtype eq 'bars3d') {
#	use GD::Graph::bars3d;
#	$graph = GD::Graph::bars3d->new ( $arg->{width}, $arg->{height} );

    } else {
	return undef;
    }

    $graph->set('x_label' => 'Time',
		'x_number_format' => sub { strftime('%D', localtime($_[0])) },
		'x_tick_number' => 1,
		@options,
		);

    return $graph;
}

sub make_tab
{
    my ($all_row) = @_;

    my $i=0;
    my $last_date=0;

    my $ret = {};
    
    foreach my $row (@$all_row) {
	my $label = $row->[1] . "/" . $row->[2] ; # client/backup name

	$ret->{date}->[$i]   = $row->[0];	
	$ret->{$label}->[$i] = $row->[3];
	$i++;
	$last_date = $row->[0];
    }

    # insert a fake element
    foreach my $elt ( keys %{$ret}) {
	$ret->{$elt}->[$i] =  undef;
    }

    $ret->{date}->[$i] = $last_date + 1;

    my $date = $ret->{date} ;
    delete $ret->{date};

    return ($date, $ret);
}

sub make_tab_sum
{
    my ($all_row) = @_;

    my $i=0;
    my $last_date=0;

    my $ret = {};
    
    foreach my $row (@$all_row) {
	$ret->{date}->[$i]   = $row->[0];	
	$ret->{nb}->[$i] = $row->[1];
	$i++;
    }

    return ($ret);
}

if ($graph eq 'job_size') {

    my $query = "
SELECT 
       UNIX_TIMESTAMP($jobt.StartTime)  AS starttime,
       Client.Name                      AS clientname,
       $jobt.Name                       AS jobname,
       $jobt.JobBytes                   AS jobbytes
FROM $jobt, FileSet, Client $filter $groupf
WHERE $jobt.ClientId = Client.ClientId
  AND $jobt.FileSetId = FileSet.FileSetId
  AND $jobt.Type = 'B'
  $clientq
  $statusq
  $filesetq
  $levelq
  $jobnameq
  $groupq
$limitq
";

    print STDERR $query if ($debug);

    my $obj = get_graph('title' => "Job Size : $arg->{jclients}/$arg->{jjobnames}",
			'y_label' => 'Size',
			'y_min_value' => 0,
			'y_number_format' => \&Bweb::human_size,
			);

    my $all = $dbh->selectall_arrayref($query) ;

    my ($d, $ret) = make_tab($all);
    if ($legend) {
	$obj->set_legend(keys %$ret);
    }
    print $obj->plot([$d, values %$ret])->png;
}

if ($graph eq 'job_file') {

    my $query = "
SELECT 
       UNIX_TIMESTAMP($jobt.StartTime)  AS starttime,
       Client.Name                      AS clientname,
       $jobt.Name                       AS jobname,
       $jobt.JobFiles                   AS jobfiles
FROM $jobt, FileSet, Client $filter $groupf
WHERE $jobt.ClientId = Client.ClientId
  AND $jobt.FileSetId = FileSet.FileSetId
  AND $jobt.Type = 'B'
  $clientq
  $statusq
  $filesetq
  $levelq
  $jobnameq
  $groupq
$limitq
";

    print STDERR $query if ($debug);

    my $obj = get_graph('title' => "Job Files : $arg->{jclients}/$arg->{jjobnames}",
			'y_label' => 'Number Files',
			'y_min_value' => 0,
			);

    my $all = $dbh->selectall_arrayref($query) ;

    my ($d, $ret) = make_tab($all);
    if ($legend) {
	$obj->set_legend(keys %$ret);
    }
    print $obj->plot([$d, values %$ret])->png;
}

# it works only with postgresql at this time
# we dont use $jobt because we use File, so job is in Job table
elsif ($graph eq 'file_histo' and $arg->{where}) {
    
    my $dir  = $dbh->quote(dirname($arg->{where}) . '/');
    my $file = $dbh->quote(basename($arg->{where}));

    my $query = "
SELECT UNIX_TIMESTAMP(Job.StartTime)    AS starttime,
       Client.Name                      AS client,
       Job.Name                         AS jobname,
       base64_decode_lstat(8,LStat)     AS lstat

FROM Job, FileSet, Filename, Path, File, Client $filter
WHERE Job.ClientId = Client.ClientId
  AND Job.FileSetId = FileSet.FileSetId
  AND Job.Type = 'B'
  AND File.JobId = Job.JobId
  AND File.FilenameId = Filename.FilenameId
  AND File.PathId = Path.PathId
  AND Path.Path = $dir
  AND Filename.Name = $file
  $clientq
  $statusq
  $filesetq
  $levelq
  $jobnameq
$limitq
";

    print STDERR $query if ($debug);

    my $all = $dbh->selectall_arrayref($query) ;

    my $obj = get_graph('title' => "File size : $arg->{where}",
			'y_label' => 'File size',
			'y_min_value' => 0,
			'y_min_value' => 0,
			'y_number_format' => \&Bweb::human_size,
			);


    my ($d, $ret) = make_tab($all);
    if ($legend) {
	$obj->set_legend(keys %$ret);
    }
    print $obj->plot([$d, values %$ret])->png;
}

# it works only with postgresql at this time
# TODO: use brestore_missing_path
elsif ($graph eq 'rep_histo' and $arg->{where}) {
    
    my $dir  = $arg->{where};
    $dir .= '/' if ($dir !~ m!/$!);
    $dir = $dbh->quote($dir);

    my $query = "
SELECT UNIX_TIMESTAMP(Job.StartTime) AS starttime,
       Client.Name                   AS client,
       Job.Name                      AS jobname,
       brestore_pathvisibility.size  AS size

FROM Job, Client $filter, FileSet, Path, brestore_pathvisibility
WHERE Job.ClientId = Client.ClientId
  AND Job.FileSetId = FileSet.FileSetId
  AND Job.Type = 'B'
  AND Job.JobId = brestore_pathvisibility.JobId
  AND Path.PathId = brestore_pathvisibility.PathId
  AND Path.Path = $dir
  $clientq
  $statusq
  $filesetq
  $levelq
  $jobnameq
$limitq
";

    print STDERR $query if ($debug);

    my $all = $dbh->selectall_arrayref($query) ;

    my $obj = get_graph('title' => "Directory size : $arg->{where}",
			'y_label' => 'Directory size',
			'y_min_value' => 0,
			'y_min_value' => 0,
			'y_number_format' => \&Bweb::human_size,
			);


    my ($d, $ret) = make_tab($all);
    if ($legend) {
	$obj->set_legend(keys %$ret);
    }
    print $obj->plot([$d, values %$ret])->png;
}

elsif ($graph eq 'job_rate') {

    my $query = "
SELECT 
       UNIX_TIMESTAMP($jobt.StartTime)  AS starttime,
       Client.Name                      AS clientname,
       $jobt.Name                       AS jobname,
       $jobt.JobBytes /
       ($bweb->{sql}->{SEC_TO_INT}(
                          $bweb->{sql}->{UNIX_TIMESTAMP}(EndTime)  
                        - $bweb->{sql}->{UNIX_TIMESTAMP}(StartTime)) + 0.01) 
         AS rate

FROM $jobt, FileSet, Client $filter $groupf
WHERE $jobt.ClientId = Client.ClientId
  AND $jobt.FileSetId = FileSet.FileSetId
  AND $jobt.Type = 'B'
  $clientq
  $statusq
  $filesetq
  $levelq
  $jobnameq
  $groupq
$limitq
";

    print STDERR $query if ($debug);

    my $obj = get_graph('title' => "Job Rate : $arg->{jclients}/$arg->{jjobnames}",
			'y_label' => 'Rate b/s',
			'y_min_value' => 0,
			'y_number_format' => \&Bweb::human_size,
			);

    my $all = $dbh->selectall_arrayref($query) ;

    my ($d, $ret) = make_tab($all);    
    if ($legend) {
	$obj->set_legend(keys %$ret);
    }
    print $obj->plot([$d, values %$ret])->png;
}



elsif ($graph eq 'job_duration') {

    my $query = "
SELECT 
       UNIX_TIMESTAMP($jobt.StartTime)                         AS starttime,
       Client.Name                                             AS clientname,
       $jobt.Name                                              AS jobname,
  $bweb->{sql}->{SEC_TO_INT}(  $bweb->{sql}->{UNIX_TIMESTAMP}(EndTime)  
                             - $bweb->{sql}->{UNIX_TIMESTAMP}(StartTime)) 
         AS duration
FROM $jobt, FileSet, Client $filter $groupf
WHERE $jobt.ClientId = Client.ClientId
  AND $jobt.FileSetId = FileSet.FileSetId
  AND $jobt.Type = 'B'
  $clientq
  $statusq
  $filesetq
  $levelq
  $jobnameq
  $groupq
$limitq
";

    print STDERR $query if ($debug);

    my $obj = get_graph('title' => "Job Duration : $arg->{jclients}/$arg->{jjobnames}",
			'y_label' => 'Duration',
			'y_min_value' => 0,
			'y_number_format' => \&Bweb::human_sec,
			);
    my $all = $dbh->selectall_arrayref($query) ;

    my ($d, $ret) = make_tab($all);
    if ($legend) {
	$obj->set_legend(keys %$ret);
    }
    print $obj->plot([$d, values %$ret])->png;


# number of job per day/hour
} elsif ($graph =~ /^job_(count|sum|avg)_((p?)(day|hour|month))$/) {
    my $t = $1;
    my $d = uc($2);
    my $per_t = $3;
    my ($limit, $label) = $bweb->get_limit(age   => $arg->{age},
					   limit => $arg->{limit},
					   offset=> $arg->{offset},
					   groupby => "A",
					   order => "A",
					   );
    my @arg;			# arg for plotting

    if (!$per_t) {		# much better aspect
	#$gtype = 'lines';
    } else {
	push @arg, ("x_number_format" => undef,
		    "x_min_value" => 0,
		    );
    }

    if ($t eq 'sum' or $t eq 'avg') {
	push @arg, ('y_number_format' => \&Bweb::human_size);
    }
    
    my $stime = $bweb->{sql}->{"STARTTIME_$d"};
    $stime =~ s/Job\./$jobt\./;

    my $query = "
SELECT
     " . ($per_t?"":"UNIX_TIMESTAMP") . "($stime) AS A,
     $t(JobBytes)                  AS nb
FROM $jobt, FileSet, Client $filter $groupf
WHERE $jobt.ClientId = Client.ClientId
  AND $jobt.FileSetId = FileSet.FileSetId
  AND $jobt.Type = 'B'
  $clientq
  $statusq
  $filesetq
  $levelq
  $jobnameq
  $groupq
$limit
";

    print STDERR $query  if ($debug);

    my $obj = get_graph('title' => "Job $t : $arg->{jclients}/$arg->{jjobnames}",
			'y_label' => $t,
			'y_min_value' => 0,
			@arg,
			);

    my $all = $dbh->selectall_arrayref($query) ;
#    print STDERR Data::Dumper::Dumper($all);
    my ($ret) = make_tab_sum($all);

    print $obj->plot([$ret->{date}, $ret->{nb}])->png;    
}

$dbh->disconnect();
