#!/usr/bin/perl -w

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

use strict;
use POSIX qw/strftime/;
use Bweb;
use CCircle ;
use Digest::MD5 qw(md5_hex);
use File::Basename qw/basename dirname/;

my $conf = new Bweb::Config(config_file => $Bweb::config_file);
$conf->load();
my $bweb = new Bweb(info => $conf);
$bweb->connect_db();

my $arg = $bweb->get_form('where', 'jobid', 'pathid', 'filenameid');
my $where = $arg->{where} || '/';
my $jobid = $arg->{jobid};
my $pathid = $arg->{pathid};
my $fnid = $arg->{filenameid};
my $jobid_url = "jobid=$jobid";
my $opt_level = 2 ;
my $max_file = 20;
my $batch = CGI::param("mode") || '';

my $md5_rep = md5_hex("$where:$jobid:$pathid:$fnid") ;
my $base_url = '/bweb/fv' ;
my $base_fich = $conf->{fv_write_path};

if ($jobid and $batch eq 'batch') {
    my $root = fv_get_root_pathid($where);
    if ($root) {
	fv_compute_size($jobid, $root);
	exit 0;
    }
    exit 1;
}

print CGI::header('text/html');
$bweb->display_begin();
$bweb->display_job_zoom($jobid);

unless ($jobid) {
    $bweb->error("Can't get where or jobid");
    exit 0;
}

unless ($base_fich and -w $base_fich) {
    $bweb->error("fv_write_path ($base_fich) is not writable." . 
		 " See Bweb configuration.");
    exit 0;
}

if (-f "$base_fich/$md5_rep.png" and -f "$base_fich/$md5_rep.tpl")
{
    $bweb->display({}, "$base_fich/$md5_rep.tpl");
    $bweb->display_end();
    exit 0;
}


# if it's a file, display it
if ($fnid and $pathid)
{
    my $attribs = fv_get_file_attribute_from_id($jobid, $pathid, $fnid);
    if ($attribs->{found}) {
	$bweb->display($attribs, 'fv_file_attribs.tpl');
	$bweb->display_end();
	exit 0;
    }

} else {

    my $attribs = fv_get_file_attribute($jobid, $where);
    if ($attribs->{found}) {
	$bweb->display($attribs, 'fv_file_attribs.tpl');
	$bweb->display_end();
	exit 0;
    }
}

my $root;

if ($pathid) {
    $root = $pathid;
    $where = fv_get_root_path($pathid);

} else {
    if ($where !~ m!/$!) {
	$where = $where . "/" ;
    }
    
    $root = fv_get_root_pathid($where);
}

if (!$root) {
    $bweb->error("Can't find $where in catalog");
    $bweb->display_end();
    exit 0;
}

my $total = fv_compute_size($jobid, $root);

my $url_action = "bfileview.pl?opt_level=$opt_level" ;
my $top = new CCircle(
		      display_other => 1,
		      base_url => "$url_action;pathid=$root;$jobid_url;here=$where",
		      ) ;

fv_display_rep($top, $total, $root, $opt_level) ;

$top->draw_labels() ;
$top->set_title(Bweb::human_size($total)) ;

open(OUT, ">$base_fich/$md5_rep.png") or die "$base_fich/$md5_rep.png $!";
# make sure we are writing to a binary stream
binmode OUT;
# Convert the image to PNG and print it on standard output
print OUT $CCircle::gd->png;
close(OUT) ;

open(OUT, ">$base_fich/$md5_rep.tpl") or die "$base_fich/$md5_rep.tpl $!";
print OUT "
 <form action='$url_action' method='get'>
  <div align='right'>
   <input title='jobids' type='hidden' name='jobid' value='$jobid'>
   <input title='repertoire' type='text' name='where' value='$where'/>
   <input type='submit' size='256' name='go' value='go'/>
  </div>
 </form>
 <br/>
" ;

print OUT $top->get_imagemap($where, "$base_url/$md5_rep.png") ;
close(OUT) ;

$bweb->display({}, "$base_fich/$md5_rep.tpl");
$bweb->display_end();

sub fv_display_rep
{
    my ($ccircle, $max, $rep, $level) = @_ ;
    return if ($max < 1);

    my $sum = 0;
    my $dirs = fv_list_dirs($jobid, $rep);	# 0: pathid, 1: pathname

    foreach my $dir (@{$dirs})
    {
	my $size = fv_compute_size($jobid, $dir->[0]);
	$sum += $size;

	my $per = $size * 100 / $max;
	# can't use pathname when using utf or accent
	# a bit ugly
	$ccircle->{base_url} =~ s/pathid=\d+;/pathid=$dir->[0];/;

	my $chld = $ccircle->add_part($per, 
				      basename($dir->[1]) . '/',
				      basename($dir->[1]) 
				       . sprintf(' %.0f%% ', $per)
				       . Bweb::human_size($size)
				      ) ;

	if ($chld && $level > 0) {
	    fv_display_rep($chld, $size, $dir->[0], $level - 1) ;
	}
    }

    # 0: filenameid, 1: filename, 2: size
    my $files = fv_get_big_files($jobid, $rep, 3*100/$max, $max_file/($level+1));
    foreach my $f (@{$files}) {
	$ccircle->{base_url} =~ s/pathid=\d+;(filenameid=\d+)?/pathid=$rep;filenameid=$f->[0];/;

	$ccircle->add_part($f->[2] * 100 / $max, 
			   $f->[1],
			   $f->[1] . "\n" . Bweb::human_size($f->[2]));
	$sum += $f->[2];
    }

    if ($sum < $max) {
	$ccircle->add_part(($max - $sum) * 100 / $max, 
			   "other files < 3%",
			   "other\n" . Bweb::human_size($max - $sum));
    }

    $ccircle->finalize() ;
}

sub fv_compute_size
{
    my ($jobid, $rep) = @_;

    my $size = fv_get_size($jobid, $rep);
    if ($size) {
	return $size;
    }

    $size = fv_get_files_size($jobid, $rep);

    my $dirs = fv_list_dirs($jobid, $rep);
    foreach my $dir (@{$dirs}) {
	$size += fv_compute_size($jobid, $dir->[0]);
    }
    
    fv_update_size($jobid, $rep, $size);
    return $size;
}

sub fv_list_dirs
{
    my ($jobid, $rep) = @_;

    my $ret = $bweb->dbh_selectall_arrayref("
      SELECT P.PathId,
             ( SELECT Path FROM Path WHERE PathId = P.PathId) AS Path
        FROM (
          SELECT PathId
            FROM brestore_pathvisibility 
      INNER JOIN brestore_pathhierarchy USING (PathId)
           WHERE PPathId  = $rep
             AND JobId = $jobid
             ) AS P
");

    return $ret;
}

sub fv_get_file_attribute
{
    my ($jobid, $full_name) = @_;
    
    my $filename = $bweb->dbh_quote(basename($full_name));
    my $path     = $bweb->dbh_quote(dirname($full_name) . "/");

    my $attr = $bweb->dbh_selectrow_hashref("
 SELECT 1    AS found,
        MD5  AS md5,
        base64_decode_lstat(8,  LStat) AS size,
        base64_decode_lstat(11, LStat) AS atime,
        base64_decode_lstat(12, LStat) AS mtime,
        base64_decode_lstat(13, LStat) AS ctime

   FROM File INNER JOIN Filename USING (FilenameId)
             INNER JOIN Path     USING (PathId)
  WHERE Name  = $filename
   AND  Path  = $path
   AND  JobId = $jobid
");

    $attr->{filename} = $full_name;
    $attr->{size} = Bweb::human_size($attr->{size});
    foreach my $d (qw/atime ctime mtime/) {
	$attr->{$d} = strftime('%F %H:%M', localtime($attr->{$d}));
    }
    return $attr;
}


sub fv_get_file_attribute_from_id
{
    my ($jobid, $pathid, $filenameid) = @_;
    
    my $attr = $bweb->dbh_selectrow_hashref("
 SELECT 1    AS found,
        MD5  AS md5,
        base64_decode_lstat(8,  LStat) AS size,
        base64_decode_lstat(11, LStat) AS atime,
        base64_decode_lstat(12, LStat) AS mtime,
        base64_decode_lstat(13, LStat) AS ctime,
        Path.Path ||  Filename.Name AS filename

   FROM File INNER JOIN Filename USING (FilenameId)
             INNER JOIN Path     USING (PathId)
  WHERE FilenameId  = $filenameid
   AND  PathId  = $pathid
   AND  JobId = $jobid
");

    $attr->{size} = Bweb::human_size($attr->{size});
    foreach my $d (qw/atime ctime mtime/) {
	$attr->{$d} = strftime('%F %H:%M', localtime($attr->{$d}));
    }
    return $attr;
}

sub fv_get_size
{
    my ($jobid, $rep) = @_;

    my $ret = $bweb->dbh_selectrow_hashref("
 SELECT Size AS size
   FROM brestore_pathvisibility
  WHERE PathId = $rep
    AND JobId = $jobid
");

    return $ret->{size};
}

sub fv_get_files_size
{
    my ($jobid, $rep) = @_;

    my $ret = $bweb->dbh_selectrow_hashref("
 SELECT sum(base64_decode_lstat(8,LStat)) AS size
   FROM File
  WHERE PathId  = $rep
    AND JobId = $jobid
");

    return $ret->{size};
}

sub fv_get_big_files
{
    my ($jobid, $rep, $min, $limit) = @_;

    my $ret = $bweb->dbh_selectall_arrayref("
   SELECT FilenameId AS filenameid, Name AS name, size
   FROM (
         SELECT FilenameId, base64_decode_lstat(8,LStat) AS size
           FROM File
          WHERE PathId  = $rep
            AND JobId = $jobid
        ) AS S INNER JOIN Filename USING (FilenameId)
   WHERE S.size > $min
   ORDER BY S.size DESC
   LIMIT $limit
");

    return $ret;
}

sub fv_update_size
{
    my ($jobid, $rep, $size) = @_;

    my $nb = $bweb->dbh_do("
 UPDATE brestore_pathvisibility SET Size = $size 
  WHERE JobId = $jobid 
    AND PathId = $rep 
");

    return $nb;
}

sub fv_get_root_pathid
{
    my ($path) = @_;
    $path = $bweb->dbh_quote($path);
    my $ret = $bweb->dbh_selectrow_hashref("SELECT PathId FROM Path WHERE Path = $path");
    return $ret->{pathid};
}

sub fv_get_root_path
{
    my ($pathid) = @_;
    my $ret = $bweb->dbh_selectrow_hashref("SELECT Path FROM Path WHERE PathId = $pathid");
    return $ret->{path};
}


__END__

CREATE OR REPLACE FUNCTION base64_decode_lstat(int4, varchar) RETURNS int8 AS $$
DECLARE
val int8;
b64 varchar(64);
size varchar(64);
i int;
BEGIN
size := split_part($2, ' ', $1);
b64 := 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
val := 0;
FOR i IN 1..length(size) LOOP
val := val + (strpos(b64, substr(size, i, 1))-1) * (64^(length(size)-i));
END LOOP;
RETURN val;
END;
$$ language 'plpgsql';

ALTER TABLE brestore_pathvisibility ADD Size  int8;



ALTER TABLE brestore_pathvisibility ADD Files int4;
