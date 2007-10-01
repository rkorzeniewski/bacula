#!/usr/bin/perl -w

my $bresto_enable = 1;
die "bresto is not enabled" if (not $bresto_enable);

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

package Bvfs;
use base qw/Bweb/;

sub get_root
{
    my ($self) = @_;
    return $self->get_pathid('');
}

sub ch_dir
{
    my ($self, $pathid) = @_;
    $self->{cwdid} = $pathid;
}

sub up_dir
{
    my ($self) = @_ ;
    my $query = "
  SELECT PPathId 
    FROM brestore_pathhierarchy 
   WHERE PathId IN ($self->{cwdid}) ";

    my $all = $self->dbh_selectall_arrayref($query);
    return unless ($all);	# already at root

    my $dir = join(',', map { $_->[0] } @$all);
    if ($dir) {
	$self->ch_dir($dir);
    }
}

sub pwd
{
    my ($self) = @_;
    return $self->get_path($self->{cwdid});
}

sub get_path
{
    my ($self, $pathid) = @_;
    $self->debug("Call with pathid = $pathid");
    my $query = 
	"SELECT Path FROM Path WHERE PathId IN (?)";

    my $sth = $self->dbh_prepare($query);
    $sth->execute($pathid);
    my $result = $sth->fetchrow_arrayref();
    $sth->finish();
    return $result->[0];    
}

sub set_curjobids
{
    my ($self, @jobids) = @_;
    $self->{curjobids} = join(',', @jobids);
#    $self->update_brestore_table(@jobids);
}

sub get_pathid
{
    my ($self, $dir) = @_;
    my $query = 
	"SELECT PathId FROM Path WHERE Path = ?";
    my $sth = $self->dbh_prepare($query);
    $sth->execute($dir);
    my $result = $sth->fetchall_arrayref();
    $sth->finish();
    
    return join(',', map { $_->[0] } @$result);
}

sub set_limits
{
    my ($self, $offset, $limit) = @_;
    $self->{limit}  = $limit  || 100;
    $self->{offset} = $offset || 0;
}

sub update_cache
{
    my ($self) = @_;

    $self->{dbh}->begin_work();

    my $query = "
  SELECT JobId from Job 
   WHERE JobId NOT IN (SELECT JobId FROM brestore_knownjobid) AND JobStatus IN ('T', 'f', 'A') ORDER BY JobId";
    my $jobs = $self->dbh_selectall_arrayref($query);

    $self->update_brestore_table(map { $_->[0] } @$jobs);

    print STDERR "Cleaning path visibility\n";
    
    my $nb = $self->dbh_do("
  DELETE FROM brestore_pathvisibility
      WHERE NOT EXISTS 
   (SELECT 1 FROM Job WHERE JobId=brestore_pathvisibility.JobId)");

    print STDERR "$nb rows affected\n";
    print STDERR "Cleaning known jobid\n";

    $nb = $self->dbh_do("
  DELETE FROM brestore_knownjobid
      WHERE NOT EXISTS 
   (SELECT 1 FROM Job WHERE JobId=brestore_knownjobid.JobId)");

    print STDERR "$nb rows affected\n";

    $self->{dbh}->commit();
}

sub update_brestore_table
{
    my ($self, @jobs) = @_;

    $self->debug(\@jobs);

    foreach my $job (sort {$a <=> $b} @jobs)
    {
	my $query = "SELECT 1 FROM brestore_knownjobid WHERE JobId = $job";
	my $retour = $self->dbh_selectrow_arrayref($query);
	next if ($retour and ($retour->[0] == 1)); # We have allready done this one ...

	print STDERR "Inserting path records for JobId $job\n";
	$query = "INSERT INTO brestore_pathvisibility (PathId, JobId) 
                   (SELECT DISTINCT PathId, JobId FROM File WHERE JobId = $job)";

	$self->dbh_do($query);

	# Now we have to do the directory recursion stuff to determine missing visibility
	# We try to avoid recursion, to be as fast as possible
	# We also only work on not allready hierarchised directories...

	print STDERR "Creating missing recursion paths for $job\n";

	$query = "SELECT brestore_pathvisibility.PathId, Path FROM brestore_pathvisibility 
		  JOIN Path ON( brestore_pathvisibility.PathId = Path.PathId)
		  LEFT JOIN brestore_pathhierarchy ON (brestore_pathvisibility.PathId = brestore_pathhierarchy.PathId)
		  WHERE brestore_pathvisibility.JobId = $job
		  AND brestore_pathhierarchy.PathId IS NULL
		  ORDER BY Path";

	my $sth = $self->dbh_prepare($query);
	$sth->execute();
	my $pathid; my $path;
	$sth->bind_columns(\$pathid,\$path);
	
	while ($sth->fetch)
	{
	    $self->build_path_hierarchy($path,$pathid);
	}
	$sth->finish();

	# Great. We have calculated all dependancies. We can use them to add the missing pathids ...
	# This query gives all parent pathids for a given jobid that aren't stored.
	# It has to be called until no record is updated ...
	$query = "
	INSERT INTO brestore_pathvisibility (PathId, JobId) (
	SELECT a.PathId,$job
	FROM
		(SELECT DISTINCT h.PPathId AS PathId
		FROM brestore_pathhierarchy AS h
		JOIN  brestore_pathvisibility AS p ON (h.PathId=p.PathId)
		WHERE p.JobId=$job) AS a
		LEFT JOIN
		(SELECT PathId
		FROM brestore_pathvisibility
		WHERE JobId=$job) AS b
		ON (a.PathId = b.PathId)
	WHERE b.PathId IS NULL)";

        my $rows_affected;
	while (($rows_affected = $self->dbh_do($query)) and ($rows_affected !~ /^0/))
	{
	    print STDERR "Recursively adding $rows_affected records from $job\n";
	}
	# Job's done
	$query = "INSERT INTO brestore_knownjobid (JobId) VALUES ($job)";
	$self->dbh_do($query);
    }
}

sub parent_dir
{
    my ($path) = @_;
    # Root Unix case :
    if ($path eq '/')
    {
        return '';
    }
    # Root Windows case :
    if ($path =~ /^[a-z]+:\/$/i)
    {
	return '';
    }
    # Split
    my @tmp = split('/',$path);
    # We remove the last ...
    pop @tmp;
    my $tmp = join ('/',@tmp) . '/';
    return $tmp;
}

sub build_path_hierarchy
{
    my ($self, $path,$pathid)=@_;
    # Does the ppathid exist for this ? we use a memory cache...
    # In order to avoid the full loop, we consider that if a dir is allready in the
    # brestore_pathhierarchy table, then there is no need to calculate all the hierarchy
    while ($path ne '')
    {
	if (! $self->{cache_ppathid}->{$pathid})
	{
	    my $query = "SELECT PPathId FROM brestore_pathhierarchy WHERE PathId = ?";
	    my $sth2 = $self->{dbh}->prepare_cached($query);
	    $sth2->execute($pathid);
	    # Do we have a result ?
	    if (my $refrow = $sth2->fetchrow_arrayref)
	    {
		$self->{cache_ppathid}->{$pathid}=$refrow->[0];
		$sth2->finish();
		# This dir was in the db ...
		# It means we can leave, the tree has allready been built for
		# this dir
		return 1;
	    } else {
		$sth2->finish();
		# We have to create the record ...
		# What's the current p_path ?
		my $ppath = parent_dir($path);
		my $ppathid = $self->return_pathid_from_path($ppath);
		$self->{cache_ppathid}->{$pathid}= $ppathid;
		
		$query = "INSERT INTO brestore_pathhierarchy (pathid, ppathid) VALUES (?,?)";
		$sth2 = $self->{dbh}->prepare_cached($query);
		$sth2->execute($pathid,$ppathid);
		$sth2->finish();
		$path = $ppath;
		$pathid = $ppathid;
	    }
	} else {
	   # It's allready in the cache.
	   # We can leave, no time to waste here, all the parent dirs have allready
	   # been done
	   return 1;
	}
    }
    return 1;
}

sub return_pathid_from_path
{
    my ($self, $path) = @_;
    my $query = "SELECT PathId FROM Path WHERE Path = ?";

    #print STDERR $query,"\n" if $debug;
    my $sth = $self->{dbh}->prepare_cached($query);
    $sth->execute($path);
    my $result =$sth->fetchrow_arrayref();
    $sth->finish();
    if (defined $result)
    {
	return $result->[0];

    } else {
        # A bit dirty : we insert into path, and we have to be sure
        # we aren't deleted by a purge. We still need to insert into path to get
        # the pathid, because of mysql
        $query = "INSERT INTO Path (Path) VALUES (?)";
        #print STDERR $query,"\n" if $debug;
	$sth = $self->{dbh}->prepare_cached($query);
	$sth->execute($path);
	$sth->finish();
        
	$query = "SELECT PathId FROM Path WHERE Path = ?";
	#print STDERR $query,"\n" if $debug;
	$sth = $self->{dbh}->prepare_cached($query);
	$sth->execute($path);
	$result = $sth->fetchrow_arrayref();
	$sth->finish();
	return $result->[0];
    }
}

sub ls_files
{
    my ($self) = @_;

    return undef unless ($self->{curjobids});

    my $inclause   = $self->{curjobids};
    my $inlistpath = $self->{cwdid};

    my $query = 
"SELECT File.FilenameId, listfiles.id, listfiles.Name, File.LStat, File.JobId
 FROM
	(SELECT Filename.Name, max(File.FileId) as id
	 FROM File, Filename
	 WHERE File.FilenameId = Filename.FilenameId
	   AND Filename.Name != ''
	   AND File.PathId IN ($inlistpath)
	   AND File.JobId IN ($inclause)
	 GROUP BY Filename.Name
	 ORDER BY Filename.Name) AS listfiles,
File
WHERE File.FileId = listfiles.id";
	
    $self->debug($query);
    my $result = $self->dbh_selectall_arrayref($query);
    $self->debug($result);
	
    return $result;
}


# return ($dirid,$dir_basename,$lstat,$jobid)
sub ls_dirs
{
    my ($self) = @_;

    return undef unless ($self->{curjobids});

    my $pathid = $self->{cwdid};
    my $jobclause = $self->{curjobids};

    # Let's retrieve the list of the visible dirs in this dir ...
    # First, I need the empty filenameid to locate efficiently the dirs in the file table
    my $query = "SELECT FilenameId FROM Filename WHERE Name = ''";
    my $sth = $self->dbh_prepare($query);
    $sth->execute();
    my $result = $sth->fetchrow_arrayref();
    $sth->finish();
    my $dir_filenameid = $result->[0];
     
    # Then we get all the dir entries from File ...
    $query = "
SELECT PathId, Path, JobId, Lstat FROM (
    
    SELECT Path1.PathId, Path1.Path, lower(Path1.Path),
           listfile1.JobId, listfile1.Lstat
    FROM (
        SELECT DISTINCT brestore_pathhierarchy1.PathId
        FROM brestore_pathhierarchy AS brestore_pathhierarchy1
        JOIN Path AS Path2
            ON (brestore_pathhierarchy1.PathId = Path2.PathId)
        JOIN brestore_pathvisibility AS brestore_pathvisibility1
            ON (brestore_pathhierarchy1.PathId = brestore_pathvisibility1.PathId)
        WHERE brestore_pathhierarchy1.PPathId = $pathid
        AND brestore_pathvisibility1.jobid IN ($jobclause)) AS listpath1
    JOIN Path AS Path1 ON (listpath1.PathId = Path1.PathId)
    LEFT JOIN (
        SELECT File1.PathId, File1.JobId, File1.Lstat FROM File AS File1
        WHERE File1.FilenameId = $dir_filenameid
        AND File1.JobId IN ($jobclause)) AS listfile1
        ON (listpath1.PathId = listfile1.PathId)
     ) AS A ORDER BY 2,3 DESC
";
    $self->debug($query);
    $sth=$self->dbh_prepare($query);
    $sth->execute();
    $result = $sth->fetchall_arrayref();
    my @return_list;
    my $prev_dir='';
    foreach my $refrow (@{$result})
    {
	my $dirid = $refrow->[0];
        my $dir = $refrow->[1];
        my $lstat = $refrow->[3];
        my $jobid = $refrow->[2] || 0;
        next if ($dirid eq $prev_dir);
        # We have to clean up this dirname ... we only want it's 'basename'
        my $return_value;
        if ($dir ne '/')
        {
            my @temp = split ('/',$dir);
            $return_value = pop @temp;
        }
        else
        {
            $return_value = '/';
        }
        my @return_array = ($dirid,$return_value,$lstat,$jobid);
        push @return_list,(\@return_array);
        $prev_dir = $dirid;
    }
    $self->debug(\@return_list);
    return \@return_list;    
}

# TODO : we want be able to restore files from a bad ended backup
# we have JobStatus IN ('T', 'A', 'E') and we must 

# Data acces subs from here. Interaction with SGBD and caching

# This sub retrieves the list of jobs corresponding to the jobs selected in the
# GUI and stores them in @CurrentJobIds.
# date must be quoted
sub set_job_ids_for_date
{
    my ($self, $client, $date)=@_;

    if (!$client or !$date) {
	return ();
    }
    	
    # The algorithm : for a client, we get all the backups for each
    # fileset, in reverse order Then, for each fileset, we store the 'good'
    # incrementals and differentials until we have found a full so it goes
    # like this : store all incrementals until we have found a differential
    # or a full, then find the full #

    my $query = "SELECT JobId, FileSet, Level, JobStatus
		FROM Job, Client, FileSet
		WHERE Job.ClientId = Client.ClientId
		AND FileSet.FileSetId = Job.FileSetId
		AND EndTime <= $date
		AND Client.Name = '$client'
		AND Type IN ('B')
		AND JobStatus IN ('T')
		ORDER BY FileSet, JobTDate DESC";
    
    my @CurrentJobIds;
    my $result = $self->dbh_selectall_arrayref($query);
    my %progress;
    foreach my $refrow (@$result)
    {
	my $jobid = $refrow->[0];
	my $fileset = $refrow->[1];
	my $level = $refrow->[2];
		
	defined $progress{$fileset} or $progress{$fileset}='U'; # U for unknown
		
	next if $progress{$fileset} eq 'F'; # It's over for this fileset...
		
	if ($level eq 'I')
	{
	    next unless ($progress{$fileset} eq 'U' or $progress{$fileset} eq 'I');
	    push @CurrentJobIds,($jobid);
	}
	elsif ($level eq 'D')
	{
	    next if $progress{$fileset} eq 'D'; # We allready have a differential
	    push @CurrentJobIds,($jobid);
	}
	elsif ($level eq 'F')
	{
	    push @CurrentJobIds,($jobid);
	}

	my $status = $refrow->[3] ;
	if ($status eq 'T') {	           # good end of job
	    $progress{$fileset} = $level;
	}
    }

    return @CurrentJobIds;
}

sub dbh_selectrow_arrayref
{
    my ($self, $query) = @_;
    $self->debug($query, up => 1);
    return $self->{dbh}->selectrow_arrayref($query);
}

# Returns list of versions of a file that could be restored
# returns an array of 
# (jobid,fileindex,mtime,size,inchanger,md5,volname,fileid)
# there will be only one jobid in the array of jobids...
sub get_all_file_versions
{
    my ($self,$pathid,$fileid,$client,$see_all)=@_;
    
    defined $see_all or $see_all=0;
    
    my @versions;
    my $query;
    $query = 
"SELECT File.JobId, File.FileId, File.Lstat, 
        File.Md5, Media.VolumeName, Media.InChanger
 FROM File, Job, Client, JobMedia, Media
 WHERE File.FilenameId = $fileid
   AND File.PathId=$pathid
   AND File.JobId = Job.JobId
   AND Job.ClientId = Client.ClientId
   AND Job.JobId = JobMedia.JobId
   AND File.FileIndex >= JobMedia.FirstIndex
   AND File.FileIndex <= JobMedia.LastIndex
   AND JobMedia.MediaId = Media.MediaId
   AND Client.Name = '$client'";

    $self->debug($query);
	
    my $result = $self->dbh_selectall_arrayref($query);
	
    foreach my $refrow (@$result)
    {
	my ($jobid, $fid, $lstat, $md5, $volname, $inchanger) = @$refrow;
	my @attribs = parse_lstat($lstat);
	my $mtime = array_attrib('st_mtime',\@attribs);
	my $size = array_attrib('st_size',\@attribs);
		
	my @list = ($pathid,$fileid,$jobid,
		    $fid, $mtime, $size, $inchanger,
		    $md5, $volname);
	push @versions, (\@list);
    }
	
    # We have the list of all versions of this file.
    # We'll sort it by mtime desc, size, md5, inchanger desc
    # the rest of the algorithm will be simpler
    # ('FILE:',filename,jobid,fileindex,mtime,size,inchanger,md5,volname)
    @versions = sort { $b->[4] <=> $a->[4] 
		    || $a->[5] <=> $b->[5] 
		    || $a->[7] cmp $a->[7] 
		    || $b->[6] <=> $a->[6]} @versions;

	
    my @good_versions;
    my %allready_seen_by_mtime;
    my %allready_seen_by_md5;
    # Now we should create a new array with only the interesting records
    foreach my $ref (@versions)
    {	
	if ($ref->[7])
	{
	    # The file has a md5. We compare his md5 to other known md5...
	    # We take size into account. It may happen that 2 files
	    # have the same md5sum and are different. size is a supplementary
	    # criterion
            
            # If we allready have a (better) version
	    next if ( (not $see_all) 
	              and $allready_seen_by_md5{$ref->[7] .'-'. $ref->[5]}); 

	    # we never met this one before...
	    $allready_seen_by_md5{$ref->[7] .'-'. $ref->[5]}=1;
	}
	# Even if it has a md5, we should also work with mtimes
        # We allready have a (better) version
	next if ( (not $see_all)
	          and $allready_seen_by_mtime{$ref->[4] .'-'. $ref->[5]}); 
	$allready_seen_by_mtime{$ref->[4] .'-'. $ref->[5] . '-' . $ref->[7]}=1;
	
	# We reached there. The file hasn't been seen.
	push @good_versions,($ref);
    }
	
    # To be nice with the user, we re-sort good_versions by
    # inchanger desc, mtime desc
    @good_versions = sort { $b->[4] <=> $a->[4] 
                         || $b->[2] <=> $a->[2]} @good_versions;
	
    return \@good_versions;
}
{
    my %attrib_name_id = ( 'st_dev' => 0,'st_ino' => 1,'st_mode' => 2,
			  'st_nlink' => 3,'st_uid' => 4,'st_gid' => 5,
			  'st_rdev' => 6,'st_size' => 7,'st_blksize' => 8,
			  'st_blocks' => 9,'st_atime' => 10,'st_mtime' => 11,
			  'st_ctime' => 12,'LinkFI' => 13,'st_flags' => 14,
			  'data_stream' => 15);;
    sub array_attrib
    {
	my ($attrib,$ref_attrib)=@_;
	return $ref_attrib->[$attrib_name_id{$attrib}];
    }
	
    sub file_attrib
    {   # $file = [filenameid,listfiles.id,listfiles.Name, File.LStat, File.JobId]

	my ($file, $attrib)=@_;
	
	if (defined $attrib_name_id{$attrib}) {

	    my @d = split(' ', $file->[3]) ; # TODO : cache this
	    
	    return from_base64($d[$attrib_name_id{$attrib}]);

	} elsif ($attrib eq 'jobid') {

	    return $file->[4];

        } elsif ($attrib eq 'name') {

	    return $file->[2];
	    
	} else	{
	    die "Attribute not known : $attrib.\n";
	}
    }
    
    sub lstat_attrib
    {
        my ($lstat,$attrib)=@_;
        if ($lstat and defined $attrib_name_id{$attrib}) 
        {
	    my @d = split(' ', $lstat) ; # TODO : cache this
	    return from_base64($d[$attrib_name_id{$attrib}]);
	}
	return 0;
    }
}

{
    # Base 64 functions, directly from recover.pl.
    # Thanks to
    # Karl Hakimian <hakimian@aha.com>
    # This section is also under GPL v2 or later.
    my @base64_digits;
    my @base64_map;
    my $is_init=0;
    sub init_base64
    {
	@base64_digits = (
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
	'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
	'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm',
	'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'
			  );
	@base64_map = (0) x 128;
	
	for (my $i=0; $i<64; $i++) {
	    $base64_map[ord($base64_digits[$i])] = $i;
	}
	$is_init = 1;
    }

    sub from_base64 {
	if(not $is_init)
	{
	    init_base64();
	}
	my $where = shift;
	my $val = 0;
	my $i = 0;
	my $neg = 0;
	
	if (substr($where, 0, 1) eq '-') {
	    $neg = 1;
	    $where = substr($where, 1);
	}
	
	while ($where ne '') {
	    $val *= 64;
	    my $d = substr($where, 0, 1);
	    $val += $base64_map[ord(substr($where, 0, 1))];
	    $where = substr($where, 1);
	}
	
	return $val;
    }

    sub parse_lstat {
	my ($lstat)=@_;
	my @attribs = split(' ',$lstat);
	foreach my $element (@attribs)
	{
	    $element = from_base64($element);
	}
	return @attribs;
    }
}


################################################################


package main;
use strict;
use Bweb;

my $conf = new Bweb::Config(config_file => $Bweb::config_file);
$conf->load();

my $bvfs = new Bvfs(info => $conf);
$bvfs->connect_db();

my $action = CGI::param('action') || '';

my $args = $bvfs->get_form('pathid', 'filenameid', 'fileid', 'qdate',
			   'limit', 'offset', 'client');

my @jobid = grep { /^\d+$/ } CGI::param('jobid');

if (!scalar(@jobid) and $args->{qdate} and $args->{client}) {
    @jobid = $bvfs->set_job_ids_for_date($args->{client}, $args->{qdate});    
}

$bvfs->set_curjobids(@jobid);

$bvfs->set_limits($args->{limit}, $args->{offset});

print CGI::header('application/x-javascript');

if ($action eq 'list_client') {

  my $q = 'SELECT Name FROM Client';
  my $ret = $bvfs->dbh_selectall_arrayref($q);

  print "[";
  print join(',', map { "['$_->[0]']" } @$ret);
  print "]\n";
  exit 0;

} elsif ($action eq 'list_job') {

    my $query = "
 SELECT Job.EndTime, FileSet.FileSet, Job.Level, Job.JobStatus, Job.JobId
  FROM Job,Client,FileSet
  WHERE Job.ClientId=Client.ClientId
  AND Client.Name = '$args->{client}'
  AND Job.Type = 'B'
  AND JobStatus IN ('f', 'T')
  AND Job.FileSetId = FileSet.FileSetId
  ORDER BY EndTime desc";
    my $result = $bvfs->dbh_selectall_arrayref($query);

    print "[";

    print join(',', map {
      "[$_->[4], '$_->[0]', '$_->[0] $_->[1] $_->[2] ($_->[3])']"
      } @$result);

    print "]\n";
}

if (CGI::param('init')) {
    $bvfs->update_brestore_table(@jobid);
}

my $pathid = CGI::param('node') || '';
my $path = CGI::param('path');

if ($pathid =~ /^(\d+)$/) {
    $pathid = $1;
} elsif ($path) {
    $pathid = $bvfs->get_pathid($path);
} else {
    $pathid = $bvfs->get_root();
}

$bvfs->ch_dir($pathid);

if ($action eq 'list_files') {
    print "[";
    my $files = $bvfs->ls_files();
#	[ 1, 2, 3, "Bill",  10, '2007-01-01 00:00:00'], 
#   File.FilenameId, listfiles.id, listfiles.Name, File.LStat, File.JobId 

    print join(',',
	       map { "[$_->[1], $_->[0], $pathid, \"$_->[2]\", 10, \"2007-01-01 00:00:00\"]" }
	       @$files);
    print "]\n";

} elsif ($action eq 'list_dirs') {

    print "[";
    my $dirs = $bvfs->ls_dirs();
	# return ($dirid,$dir_basename,$lstat,$jobid)

    print join(',', 
	       map { "{ 'id': '$_->[0]', 'text': '$_->[1]', 'cls':'folder'}" }
	       @$dirs);

    print "]\n";

} elsif ($action eq 'list_versions') {

    my $vafv = CGI::param('vafv') || 'false'; # view all file versions
    $vafv = ($vafv eq 'false')?0:1;

    print "[";
    #   0       1       2        3   4       5      6           7      8
    #($pathid,$fileid,$jobid, $fid, $mtime, $size, $inchanger, $md5, $volname);
    my $files = $bvfs->get_all_file_versions($args->{pathid}, $args->{filenameid}, $args->{client}, $vafv);
    print join(',', 
	       map { "[ $_->[3], $_->[1], $_->[0], $_->[2], '$_->[8]', $_->[6], '$_->[7]', $_->[5], $_->[4] ]" }
	       @$files);
    print "]\n";

}



