#!/usr/bin/perl
##
# bimagemgr.pl
# burn manager for bacula CD image files
#
# Copyright (C) 2004-2006 Kern Sibbald
#
# Thu Dec 09 2004 D. Scott Barninger <barninger at fairfieldcomputers.com>
# ASSIGNMENT OF COPYRIGHT
# FOR VALUE RECEIVED, D. Scott Barninger hereby sells, transfers and 
# assigns unto Kern Sibbald, his successors, assigns and personal representatives, 
# all right, title and interest in and to the copyright in this software.
# D. Scott Barninger warrants good title to said copyright, that it is 
# free of all liens, encumbrances or any known claims against said copyright.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License version 2 as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public
# License along with this program; if not, write to the Free
# Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
# MA 02111-1307, USA.
##

my $VERSION = "0.3";

require 5.000; use strict 'vars', 'refs', 'subs';
use DBI;

# get program configuration
use config;

my $prog_config = config->new();

## web server configuration
#
# web server path to program from server root
my $prog_name = $prog_config->{'prog_name'};
#
# web server host
my $http_host = $prog_config->{'http_host'};
#
# path to graphics from document root
my $logo_graphic = $prog_config->{'logo_graphic'};
my $spacer_graphic = $prog_config->{'spacer_graphic'};
my $burn_graphic = $prog_config->{'burn_graphic'};
##

## database configuration
#
# database name
my $database = $prog_config->{'database'} ;
#
# database host
my $host = $prog_config->{'host'};
#
# database user
my $user = $prog_config->{'user'};
#
# database password
my $password = $prog_config->{'password'};
#
# database driver selection
my $db_driver = $prog_config->{'db_driver'};
my $db_name_param = $prog_config->{'db_name_param'};
my $catalog_dump = $prog_config->{'catalog_dump'};
my $sqlitebindir = $prog_config->{'sqlitebindir'};
my $bacula_working_dir = $prog_config->{'bacula_working_dir'};

# path to backup files
my $image_path = $prog_config->{'image_path'};

## path to cdrecord and burner settings
my $cdrecord = $prog_config->{'cdrecord'};
my $mkisofs = $prog_config->{'mkisofs'};
my $cdburner = $prog_config->{'cdburner'};
my $burner_speed = $prog_config->{'burner_speed'};
my $burnfree = $prog_config->{'burnfree'};
##

# temporary files
my $tempfile=$prog_config->{'tempfile'};
my $tempfile_path=$prog_config->{'tempfile_path'};
my $working_dir=$prog_config->{'working_dir'};

# copyright info for page footer
my $copyright = "Copyright &copy; 2004-2006 The Bacula Team";

my %input = &getcgivars;
my $action = $input{'action'};
my $vol = $input{'vol'};

&Main();
exit;

#-------------------------------------------------------------------#
# Function Main
# Description: check requested action and call appropriate subroutine
#

sub Main {
	# set default action & department
	if (!$action) {$action = "display"};

	if ($action eq "display") {
		&Display();
	}
	elsif ($action eq "burn") {
		&Burn();
	}
	elsif ($action eq "reset") {
		&Reset();
	}
	elsif ($action eq "version") {
		&DisplayHeader();
		print "<div align=\"center\">";
		print "<br>Bacula CD Image Manager version $VERSION<br><br>";
		print "Copyright &copy; 2004 D. Scott Barninger<br>";
		print "Licensed under the GNU GPL version 2.0";
		print "</div>";
		print "</body></html>";
	}
	else {
		&HTMLdie("Unknown action $action","So sorry Kimosabe..");
	}
	return;
}

#-------------------------------------------------------------------#
# Function Display
# Description: main page display routine

sub Display {
	my ($MediaId,$VolumeName,$LastWritten,$VolWrites,$VolStatus,$data);

	&DisplayHeader();
	&UpdateImageTable();

	# connect to database
	my ($dbh);
	if ( $db_driver eq "SQLite" ) {
		$dbh = DBI->connect("DBI:$db_driver:$db_name_param=$bacula_working_dir/$database.db","","",
			{'RaiseError' => 1}) || &HTMLdie("Unable to connect to database.");
	}
	else {
		$dbh = DBI->connect("DBI:$db_driver:$db_name_param=$database;host=$host","$user","$password",
			{'RaiseError' => 1}) || &HTMLdie("Unable to connect to database.");
	}
	my $sth = $dbh->prepare("SELECT Media.VolumeName,Media.LastWritten,CDImages.LastBurn,
		Media.VolWrites,Media.VolStatus 
		FROM CDImages,Media 
		WHERE CDImages.MediaId = Media.MediaId");
	
	print "<div align=\"center\">";
	print "<table width=\"80%\" border=\"0\">";
	print "<tr>";
	print "<td colspan=\"6\"><b>";
	print "<p>Backup files which need to be committed to CDR disk since their last write date are shown below with a Burn button. ";
	print "Place a blank CDR disk in the drive and click the Burn button for the volume you wish to burn to disk.</p>";
	print "<p>When CD recording is complete the popup window will display the output of cdrecord. ";
	print "A successful burn is indicated by the last line showing that all bytes were successfully recorded.</p>";
	print "<p>After the popup window indicates that the burn is complete, close the popup and <a href=\"$prog_name\">refresh this window</a>. ";
	print "If the burn is not successful click the Reset link under the last burn date to restore the Burn button and <a href=\"$prog_name\">refresh this window</a>.</p>";
	print "<p>To burn a copy of your catalog click the catalog Burn button at the bottom of the page. ";
	print "Up to date copies of your catalog and all backup volumes ensure that your bacula server can be rebuilt in the event of a catastrophe.</p>";
	print "</b></td>";
	print "</tr>";
	print "<tr>";
	print "<td colspan=\"6\">&nbsp;</td>";
	print "</tr>";
	print "</table>";
	print "<table width=\"80%\" border=\"1\">";
	print "<tr>";
	print "<td align=\"center\" colspan=\"6\"><h3>Current Volume Information</h3>Make sure the backup file path $image_path is mounted.</td>";
	print "</tr>";
	print "<tr>";
	print "<td align=\"center\"><b>Volume Name</b></td>";
	print "<td align=\"center\"><b>Last Written</b></td>";
	print "<td align=\"center\"><b>Last Burn</b></td>";
	print "<td align=\"center\"><b>Writes</b></td>";
	print "<td align=\"center\"><b>Status</b></td>";
	print "<td align=\"center\">&nbsp;</td>";
	print "</tr>";

	$sth->execute();
	while ($data = $sth->fetchrow_arrayref) {
		print "<tr>";
		print "<td align=\"center\">$$data[0]</td>";
		print "<td align=\"center\">$$data[1]</td>";
		print "<td align=\"center\">$$data[2]<br><a href=\"$prog_name?action=reset&vol=$$data[0]\">Reset</a></td>";
		print "<td align=\"center\">$$data[3]</td>";
		print "<td align=\"center\">$$data[4]</td>";
		if ($$data[1] gt $$data[2] && $$data[3] gt "0" && $$data[4] ne "Purged") {
			print "<td align=\"center\"><form><input type=button value=\"Burn\" onClick=\"BurnWindow=window.open(\'$prog_name?action=burn&vol=$$data[0]\', \'BurnWindow\', \'scrollbars=yes,menubar=no,width=550,height=450,screenX=0,screenY=15\')\"></form></td>";
		}
		else {
			print "<td align=\"center\">No Burn</td>";
		}
		print "</tr>";
	}
	$sth->finish();
	$dbh->disconnect();
	
	print "<tr>";
	print "<td align=\"center\" colspan=\"6\"><img src=\"$spacer_graphic\" height=\"18\"><form><input type=button value=\"Burn Catalog\" onClick=\"BurnWindow=window.open(\'$prog_name?action=burn&vol=bacula.sql\', \'BurnWindow\', \'scrollbars=yes,menubar=no,width=550,height=450,screenX=0,screenY=15\')\"><img src=\"$spacer_graphic\" width=\"5\"><input type=button value=\"Blank CDRW\" onClick=\"BurnWindow=window.open(\'$prog_name?action=burn&vol=blank\', \'BurnWindow\', \'scrollbars=yes,menubar=no,width=550,height=450,screenX=0,screenY=15\')\"></form><img src=\"$spacer_graphic\" height=\"1\"></td>";
	print "</tr>";
	print "</table>";
	print "</div>";
	print "<p><p>";
	&DisplayFooter();
	return;
}

#-------------------------------------------------------------------#
# Function Burn
# Description: burn cd images

sub Burn {

	my $Volume = "$image_path/$vol";
	
	# check to see if this is a catalog request
	if ($vol eq "bacula.sql") {
		$Volume = "$working_dir/bacula.sql";
	}
	# check to see if this is a blanking request
	if ($vol eq "blank") {
		$Volume = "Blank CD/RW";
	}
	
	# open the burn results file and write header info
	open(OUTF,">$tempfile_path") || &HTMLdie("Unable to open temporary output file.");
	print OUTF "<html>";
	print OUTF "<head>";
	print OUTF "<title>Burning Volume $Volume</title>";
	print OUTF "</head>";
	print OUTF "<body>";
	print OUTF "<div align=\"center\">";
	print OUTF "<table width=\"100%\">";
	print OUTF "<tbody>";
	print OUTF "<tr>";
	print OUTF "<td align=\"center\">  <img src=\"$logo_graphic\"> </td>";
	print OUTF "</tr>";
	print OUTF "</tbody>";
	print OUTF "</table>";
	print OUTF "</div>";
	close(OUTF);
	
	# now send to the burn status window that we are burning
	print "Content-type: text/html\n\n";
	print "<html>";
	print "<head>";
	print "<meta http-equiv=\"Refresh\" content=\"3; url=http://$http_host/$tempfile\">";
	print "<title>Burning Volume $Volume</title>";
	print "</head>";
	print "<body>";
	print "<div align=\"center\">";
	print "<table width=\"100%\">";
	print "<tbody>";
	print "<tr>";
	print "<td align=\"center\">  <img src=\"$logo_graphic\"> </td>";
	print "</tr>";
	print "<tr>";
	print "<td align=\"center\">  <img src=\"$spacer_graphic\" height=\"10\"> </td>";
	print "</tr>";
	print "<tr>";
	print "<td align=\"center\">  <img src=\"$burn_graphic\"> </td>";
	print "</tr>";
	print "</tbody>";
	print "</table>";
	print "<p>Now burning $Volume ...</p>";
	print "</div>";
	print "</body>";
	print "</html>";
	
	# check to see if this is a catalog request
	if ($vol eq "bacula.sql") {
		system("$catalog_dump > $working_dir/bacula.sql");
	}
	
	# check to see if this is a blanking request
	if ($vol eq "blank") {
		system("$cdrecord -eject speed=2 dev=$cdburner blank=fast >> $tempfile_path");
	}
	else {
	# burn the image and clean up
	system("$mkisofs -o $working_dir/temp.iso -J -r -V $vol $Volume");
	system("$cdrecord -eject $burnfree speed=$burner_speed dev=$cdburner $working_dir/temp.iso >> $tempfile_path");
	system("rm -f $working_dir/temp.iso");
	}
	
	if ($vol eq "bacula.sql") {
		system("rm -f $working_dir/bacula.sql");
	}
		
	# finish up the burn results file
	open(OUTF,">>$tempfile_path") || &HTMLdie("Unable to open temporary output file.");
	print OUTF "<table width=\"100%\">";
	print OUTF "<tbody>";
	print OUTF "<tr>";
	print OUTF "<td><div align=\"center\">If you do not see successful output from cdrecord above the burn has failed.</div></td>";
	print OUTF "</tr>";
	print OUTF "<tr>";
	print OUTF "<td><div align=\"center\">Please close this window and refresh the main window.</div></td>";
	print OUTF "</tr>";
	print OUTF "<tr>";
	print OUTF "<td><div align=\"center\"><font size=\"-3\"> $copyright </font></div></td>";
	print OUTF "</tr>";
	print OUTF "</tbody>";
	print OUTF "</table>";
	print OUTF "</div>";
	print OUTF "</body>";
	print OUTF "</html>";
	close(OUTF);
	
	# now pretty up the burn results file by replacing \n with <br>
	open(INFILE, "$tempfile_path") || &HTMLdie("Unable to open input file $tempfile_path");
	open(OUTFILE, ">$working_dir/bimagemgr-temp") || &HTMLdie("Unable to open output file bimagemgr-temp");
	while(my $line = <INFILE>) {
		$line =~ s/\n/<br>/g;
		print OUTFILE ($line);
	}
	close(INFILE);
	close(OUTFILE);
	system("cp -f $working_dir/bimagemgr-temp $tempfile_path");
	
	if ($vol ne "bacula.sql" && $vol ne "blank") {
		## update the burn date in the CDImages table
		# get current timestamp
		my($sysdate,$systime,$sysyear,$sysmon,$sysmday) = &SysDate;
		my $burndate = "$sysdate $systime";
	
		# connect to database
		my ($dbh);
		if ( $db_driver eq "SQLite" ) {
			$dbh = DBI->connect("DBI:$db_driver:$db_name_param=$bacula_working_dir/$database.db","","",{'RaiseError' => 1}) || &HTMLdie("Unable to connect to database.");
		}
		else {
			$dbh = DBI->connect("DBI:$db_driver:$db_name_param=$database;host=$host","$user","$password",{'RaiseError' => 1}) || &HTMLdie("Unable to connect to database.");
		}
		# get the MediaId for our volume
		my $sth = $dbh->prepare("SELECT MediaId from Media WHERE VolumeName = \"$vol\"");
		$sth->execute();
		my $media_id = $sth->fetchrow_array;
		$sth->finish();
		# set LastBurn date
		$dbh->do("UPDATE CDImages SET LastBurn = \"$burndate\" WHERE MediaId = $media_id");
		$dbh->disconnect();
		##
	}
	
	return;
}

#-------------------------------------------------------------------#
# Function UpdateImageTable
# Description: update the CDImages table from the Media table

sub UpdateImageTable {

	my ($data,@MediaId,$id,$exists,$sth1,$sth2);

	# connect to database
	my ($dbh);
	if ( $db_driver eq "SQLite" ) {
		$dbh = DBI->connect("DBI:$db_driver:$db_name_param=$bacula_working_dir/$database.db","","",
			{'RaiseError' => 1}) || &HTMLdie("Unable to connect to database.");
	}
	else {
		$dbh = DBI->connect("DBI:$db_driver:$db_name_param=$database;host=$host","$user","$password",
			{'RaiseError' => 1}) || &HTMLdie("Unable to connect to database.");
	}

	# get the list of current MediaId
	$sth1 = $dbh->prepare("SELECT MediaId from Media");
	$sth1->execute();
	while ($data = $sth1->fetchrow_arrayref) {
		push(@MediaId,$$data[0]);
	}
	$sth1->finish();

	# now check if we have a matching row in CDImages
	# if not then insert a record
	foreach $id (@MediaId) {
		$sth2 = $dbh->prepare("SELECT MediaId from CDImages 
		WHERE MediaId = $id");
		$sth2->execute();
		$exists = $sth2->fetchrow_array;
		if ($exists ne $id) {
			$dbh->do("INSERT into CDImages VALUES ($id,\"0000-00-00 00:00:00\")");
		}
		$sth2->finish();
	}

	# disconnect
	$dbh->disconnect();
	return;
}

#-------------------------------------------------------------------#
# Function Reset
# Description: reset the Last Burn date to 0

sub Reset {
	my ($id,$sth);

	# connect to database
	my ($dbh);
	if ( $db_driver eq "SQLite" ) {
		$dbh = DBI->connect("DBI:$db_driver:$db_name_param=$bacula_working_dir/$database.db","","",
			{'RaiseError' => 1}) || &HTMLdie("Unable to connect to database.");
	}
	else {
		$dbh = DBI->connect("DBI:$db_driver:$db_name_param=$database;host=$host","$user","$password",
			{'RaiseError' => 1}) || &HTMLdie("Unable to connect to database.");
	}

	# get the MediaId
	$sth = $dbh->prepare("SELECT MediaId FROM Media WHERE VolumeName=\"$vol\"");
	$sth->execute();
	$id = $sth->fetchrow_array;
	$sth->finish();
	
	# reset the date
	$dbh->do("UPDATE CDImages SET LastBurn=\"0000-00-00 00:00:00\" 
			WHERE MediaId=$id") || &HTMLdie("Unable to update Last Burn Date.");

	$dbh->disconnect();
	
	print "Location:http://$http_host$prog_name\n\n";
}

#-------------------------------------------------------------------#
# Function DisplayHeader
# Description: main page display header

sub DisplayHeader {
	my($title)= @_ ;
	$title || ($title= "Bacula CD Image Manager") ;
	print "Content-type: text/html\n\n";
	print "<html>";
	print "<head>";
	print "<title>$title</title>";
	print "</head>";
	print "<body>";
	print "<div align=\"center\">";
	print "<table width=\"100%\">";
	print "<tbody>";
	print "<tr>";
	print "<td align=\"center\"><a href=\"$prog_name?action=version\" alt=\"About bimagemgr\"><img src=\"$logo_graphic\" border=\"0\"></a></td>";
	print "</tr>";
	print "</tbody>";
	print "</table>";
	print "</div>";
	print "<p></p>";
	return;
}

#-------------------------------------------------------------------#
# Function DisplayFooter
# Description: main page display footer

sub DisplayFooter {
	print "<table width=\"100%\">";
	print "<tbody>";
	print "<tr>";
	print "<td><div align=\"center\"><font size=\"-3\"> $copyright </font></div></td>";
	print "</tr>";
	print "</tbody>";
	print "</table>";
	print "</div>";
	print "</body>";
	print "</html>";
	return;
}

#-------------------------------------------------------------------#
# Function SysDate
# Description: get current date/time
#
sub SysDate {
	# usage:  my($sysdate,$systime,$sysyear,$sysmon,$sysmday) = &SysDate;

	my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst)=localtime(time);
	if (length ($min) == 1) {$min = '0'.$min;}
	if (length ($sec) == 1) {$sec = '0'.$sec;}
	# since localtime returns the month as 0-11
	$mon = $mon + 1;
	if (length ($mon) == 1) {$mon = '0'.$mon;}
	if (length ($mday) == 1) {$mday = '0'.$mday;}
	# since localtime returns the year as the number of years since 1900
	# ie year is 100 in the year 2000 (so is y2k OK)
	$year = $year + 1900;
	my $date = "$year-$mon-$mday";
	my $time = "$hour:$min:$sec";
	return($date,$time,$year,$mon,$mday);
}

#-------------------------------------------------------------------#
# Function getcgivars
# Read all CGI vars into an associative array.
# courtesy James Marshall james@jmarshall.com http://www.jmarshall.com/easy/cgi/
# If multiple input fields have the same name, they are concatenated into
#   one array element and delimited with the \0 character (which fails if
#   the input has any \0 characters, very unlikely but conceivably possible).
# Currently only supports Content-Type of application/x-www-form-urlencoded.
sub getcgivars {
	my($in, %in) ;
	my($name, $value) ;


		# First, read entire string of CGI vars into $in
	if ( ($ENV{'REQUEST_METHOD'} eq 'GET') ||
			($ENV{'REQUEST_METHOD'} eq 'HEAD') ) {
		$in= $ENV{'QUERY_STRING'} ;

	} elsif ($ENV{'REQUEST_METHOD'} eq 'POST') {
		if ($ENV{'CONTENT_TYPE'}=~ m#^application/x-www-form-urlencoded$#i) {
			length($ENV{'CONTENT_LENGTH'})
				|| &HTMLdie("No Content-Length sent with the POST request.") ;
			read(STDIN, $in, $ENV{'CONTENT_LENGTH'}) ;

		} else { 
			&HTMLdie("Unsupported Content-Type: $ENV{'CONTENT_TYPE'}") ;
		}

	} else {
		&HTMLdie("Script was called with unsupported REQUEST_METHOD.") ;
	}

		# Resolve and unencode name/value pairs into %in
	foreach (split('&', $in)) {
		s/\+/ /g ;
		($name, $value)= split('=', $_, 2) ;
		$name=~ s/%(..)/chr(hex($1))/ge ;
		$value=~ s/%(..)/chr(hex($1))/ge ;
		$in{$name}.= "\0" if defined($in{$name}) ;  # concatenate multiple vars
		$in{$name}.= $value ;
	}

	return %in ;

}

#-------------------------------------------------------------------#
# Function HTMLdie
# Description: Die, outputting HTML error page
# If no $title, use a default title
sub HTMLdie {
	my ($msg,$title)= @_ ;
	$title || ($title= "CGI Error") ;
	print "Content-type: text/html\n\n";
	print "<html>";
	print "<head>";
	print "<title>$title</title>";
	print "</head>";
	print "<body>";
	print "<h1>$title</h1>";
	print "<h3>$msg</h3>";
	print "<form>";
	print "<input type=button name=\"BackButton\" value=\"<- Back\" id=\"Button1\" onClick=\"history.back()\">";
	print "</form>";
	print "</body>";
	print "</html>";

	exit ;
}

