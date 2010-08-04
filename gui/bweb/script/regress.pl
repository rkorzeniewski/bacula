#!/usr/bin/perl -w
use strict;

=head1 LICENSE

   Bweb - A Bacula web interface
   BaculaÂ® - The Network Backup Solution

   Copyright (C) 2000-2006 Free Software Foundation Europe e.V.

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

=head1 USAGE

    Get it working with a regress environment:
     * get regress module from SVN
     * use postgresql or mysql in config
     * make setup
     * add catalog = all, !skipped, !saved into Messages Standard (scripts/bacula-dir.conf)
     * add exit 0 to scripts/cleanup
     * run bacula-backup-test
     * uncomment job schedule in bacula-dir.conf
     * load bweb-(mysql|postgresql).sql
     * ./bin/bacula start
     * configure bweb to point to bconsole and the catalog

=head1 VERSION

    $Id$

=cut

use Test::More qw(no_plan);
use WWW::Mechanize;

use Getopt::Long;

my ($login, $pass, $url, $verbose);
GetOptions ("login=s"  => \$login,
	    "passwd=s" => \$pass,
	    "url|u=s"  => \$url,
	    "verbose"  => \$verbose,
	    );

die "Usage: $0 --url http://.../cgi-bin/bweb/bweb.pl [-l user -p pass]"
    unless ($url);

print "Making tests on $url\n";
my ($req, $res, $c, $cli, $job_url);

my $agent = new WWW::Mechanize(autocheck=>1);
if ($login) {
    $agent->credentials($login, $pass);
}

################################################################
# Check bweb libraries
################################################################

# first, try to load all bweb libraries
require_ok('Bweb');
require_ok('Bconsole');
require_ok('CCircle');
require_ok('GBalloon');
require_ok('GTime');

# test first page and check for the last job 
$agent->get($url); ok($agent->success, "Get main page"); $c = $agent->content;
like($c, qr!</html>!, "Check EOP");
ok($c =~ m!(action=job_zoom;jobid=\d+)!, "Get the first JobId");
die "Can't get first jobid ($c)" unless $1;
$job_url=$1;

# check missing view
ok($agent->follow_link(text_regex=>qr/Missing Jobs/), "Go to Missing Jobs page");
$c=$agent->content;
like($c, qr/BackupCatalog/, "Check for BackupCatalog job");
unlike($c, qr/backup/, "Check for backup job");

# test job_zoom page
# check for
#  - job log
#  - fileset
#  - media view
$agent->get("$url?$job_url");
ok($agent->success,"Get job zoom"); $c=$agent->content;
like($c, qr!</html>!, "Check EOP");
like($c, qr!Using Device!, "Check for job log");

ok($agent->form_name('fileset_view'), "Find form");
$agent->click(); $c=$agent->content;
ok($agent->success, "Get fileset"); 
like($c, qr!</html>!, "Check EOP");
ok($c =~ m!<pre>\s*(/[^>]+?)</pre>!s,"Check fileset");
print $1 if $verbose;
$agent->back(); ok($agent->success,"Return from fileset");

ok($agent->form_name("rerun"), "Find form");
$agent->click(); $c=$agent->content;
ok($agent->success, "ReRun job");
ok($agent->form_name("form1"), "Find form");
ok($c =~ m!<select name='job'>\s*<option value='(.+?)'!,"jobs");
ok($agent->field('job', $1), "set field job=1");
ok($c =~ m!<select name='client'>\s*<option value='(.+?)'!, "clients");
ok($agent->field('client', $1), "set field client=$1");
ok($c =~ m!<select name='storage'>\s*<option value='(.+?)'!, "storages");
ok($agent->field('storage', $1), "set field storage=$1");
ok($c =~ m!<select name='fileset'>\s*<option value='(.+?)'!, "filesets");
ok($agent->field('fileset', $1), "set field fileset=1");
ok($c =~ m!<select name='pool'>\s*<option value=''></option>\s*<option value='(.+?)'!, "pools");
$agent->field('pool', $1);
$agent->click_button(value => 'run_job_now');
ok($agent->success(), "submit");
sleep 2;
ok($agent->follow_link(text_regex=>qr/here/i), "follow link");
ok($agent->success(), "get job page"); $c=$agent->content;
like($c, qr/Using Device/, "Check job log");

# try to delete this job
$agent->get("$url?$job_url");
ok($agent->success,"Get job zoom");
ok($agent->form_name('delete'), "Find form");
$agent->click(); $c=$agent->content;
ok($agent->success, "Delete it"); 
like($c, qr!deleted from the catalog!, "Check deleted message");

$agent->get("$url?$job_url");
ok($agent->success,"Get job zoom");
$c=$agent->content;
like($c, qr!An error has occurred!, "Check deleted job");

# list jobs
ok($agent->follow_link(text_regex=>qr/Defined Jobs/), "Go to Defined Jobs page");
$c=$agent->content;
like($c, qr/BackupCatalog/, "Check for BackupCatalog job");

################################################################
# client tests
################################################################

ok($agent->follow_link(text_regex=>qr/Clients/), "Go to Clients page");
$c=$agent->content;
ok($c =~ m!chkbox.value = '(.+?)';!, "get client name");
$cli = $1;

$agent->get("$url?action=client_status;client=$cli");
ok($agent->success(), "submit"); $c=$agent->content;
like($c, qr/Terminated Jobs/, "check for client status");

$agent->get("$url?action=job;client=$cli");
ok($agent->success(), "submit"); $c=$agent->content;
like($c, qr/'$cli'\).selected = true;/, "list jobs for this client");

################################################################
# Test location basic functions
################################################################

my $loc = "loc$$";
ok($agent->follow_link(text_regex=>qr/Location/), "Go to Location page");
ok($agent->form_number(2), "Find form");
$agent->click_button(value => 'location_add');
ok($agent->success(), "submit");
ok($agent->form_number(2), "Find form");
$agent->field("location", $loc);
ok($agent->field("cost", 20), "set field cost=20");
ok($agent->field("enabled", "yes"), "set field enabled=yes");
ok($agent->field("enabled", "no"), "try set field enabled=no");
ok($agent->field("enabled", "archived"), "try set field enabled=archived");
$agent->click_button(value => 'location_add');
ok($agent->success(), "submit"); $c=$agent->content;
like($c, qr/$loc/, "Check if location is ok");
like($c, qr/inflag2.png/, "Check if enabled is archived");

$agent->get("$url?location=$loc&action=location_edit");
ok($agent->success(), "Try to edit location"); $c=$agent->content;
like($c, qr/$loc/, "Check for location");
ok($agent->form_number(2), "Find form");
$agent->field("cost", 40);
$agent->field("enabled", "no");
$loc = "new$loc";
$agent->field("newlocation", $loc);
$agent->click_button(value => 'location_save');
ok($agent->success(), "submit to edit location"); $c=$agent->content;
like($c, qr/$loc/, "Check if location is ok");
like($c, qr/inflag0.png/, "Check if enabled is 'no'");
like($c, qr/40/, "Check for cost");

################################################################
# Test media
################################################################

ok($agent->follow_link(text_regex=>qr/All Media/), "Go to All Media page");
ok($agent->success(), "submit"); $c=$agent->content;
ok($c =~ m/chkbox.value = '(.+?)'/, "get first media");
my $vol = $1;

$agent->get("$url?media=$vol;action=update_media");
ok($agent->success(), "submit"); $c=$agent->content;
like($c, qr/$vol/, "Check if volume is ok");

################################################################
# Test group basic functions
################################################################

$agent->get("$url?action=client;notingroup=yes");
ok($agent->success(), "submit"); $c=$agent->content;
like($c, qr/$cli/, "check client=$cli is groupless");

# create a group
my $grp = "test$$";
ok($agent->follow_link(text_regex=>qr/Groups/), "Go to Groups page");
ok($agent->success(), "submit"); $c=$agent->content;
unlike($c, qr/error/i, "Check for group installation");
ok($agent->form_number(2), "Find form");
$agent->click_button(value => 'groups_edit');
ok($agent->success(), "submit action=groups_edit");
ok($agent->form_number(2), "Find form to create a group");
$agent->field("newgroup", $grp);
$agent->click_button(value => 'groups_save');
ok($agent->success(), "submit action=groups_save");
$c=$agent->content; 
like($c, qr/$grp/, "Check if group have been created");

# rename group (client is loss because it was set by javascript)
$agent->get("$url?client_group=$grp;action=groups_edit");
ok($agent->success(), "submit action=groups_edit"); $c=$agent->content;
like($c, qr/$grp/, "Check if group is present to rename it");
ok($agent->form_number(2), "Find form");
$grp = "newtest$$";
$agent->field("newgroup", $grp);
$agent->field("client", $cli);
$agent->click_button(value => 'groups_save');
ok($agent->success(), "submit"); $c=$agent->content;
like($c, qr/'$grp'/, "Check if newgroup is present");

# check if group is ok
$agent->get("$url?client_group=$grp;action=client");
ok($agent->success(), "submit"); $c=$agent->content;
like($c, qr/'$cli'/, "Check if client is present in newgrp");

$agent->get("$url?action=client;notingroup=yes");
ok($agent->success(), "check if client=$cli is already 'not in group'");
$c=$agent->content;
unlike($c, qr/$cli/, "check client=$cli");

$agent->get("$url?client_group=other$grp;action=groups_edit");
ok($agent->success(), "create an empty other$grp"); $c=$agent->content;
like($c, qr/'other$grp'/, "check if other$grp was created");

# view job by group
ok($agent->follow_link(text_regex=>qr/Jobs by group/), "Go to Job by group page");
ok($agent->success(), "Get it"); $c=$agent->content;
like($c, qr/"$grp"/, "check if $grp is here");

# view job overview
ok($agent->follow_link(text_regex=>qr/Jobs overview/), "Go to Job overview");
ok($agent->success(), "Get it"); $c=$agent->content;
like($c, qr/"$grp"/, "check if $grp is here");

# get group stats
ok($agent->follow_link(url_regex=>qr/action=group_stats/), "Go to groups stats");
ok($agent->success(), "Get it"); $c=$agent->content;
like($c, qr/"$grp"/, "check if $grp is here");

# view next jobs
ok($agent->follow_link(text_regex=>qr/Next Jobs/), "Go to Next jobs");
ok($agent->success(), "Get it"); $c=$agent->content;
like($c, qr/'BackupCatalog'/, "check if BackupCatalog is here");

# Add media
ok($agent->follow_link(text_regex=>qr/Add Media/), "Go to Add Media");
ok($agent->success(), "Get it");
ok($agent->form_number(2), "Find form");
$agent->field('pool', 'Scratch');
$agent->field('storage', 'File');
$agent->field('nb', '3');
$agent->field('offset', '1');
$agent->field('media', "Vol$$");
$agent->click_button(value => 'add_media');
ok($agent->success(), "Create them"); $c=$agent->content;
like($c, qr/Vol${$}0001/, "Check if media are there");

# view pools
ok($agent->follow_link(text_regex=>qr/Pools/), "Go to Pool");
ok($agent->success(), "Get it"); $c=$agent->content;
like($c, qr/"Default"/, "check if Default pool is here");
like($c, qr/"Scratch"/, "check if Scratch pool is here");

################################################################
# other checks and cleanup
################################################################

# cleanup groups
$agent->get("$url?client_group=$grp;action=groups_del");
ok($agent->success(), "submit"); $c=$agent->content;
unlike($c, qr/'$grp'/, "Check if group was deleted");

$agent->get("$url?client_group=other$grp;action=groups_del");
ok($agent->success(), "submit"); $c=$agent->content;
unlike($c, qr/'other$grp'/, "Check if group was deleted");

# cleanup location
$agent->get("$url?location=$loc;action=location_del");
ok($agent->success(), "submit"); $c=$agent->content;
unlike($c, qr/$loc/, "Check if location was deleted");

exit 0;


__END__

