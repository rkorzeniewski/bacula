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

=head1 USAGE

    Get it working with a regress environment:
     * get regress module from SVN
     * use postgresql or mysql in config
     * make setup
     * add catalog = all, !skipped, !saved into Messages Standard (scripts/bacula-dir.conf)
     * add exit 0 to scripts/cleanup
     * run bacula-backup-test
     * charger bweb-(mysql|postgresql).sql
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

die "Usage: $0 --url http://.../cgi-bin/bweb/bweb.pl [-u user -p pass]"
    unless ($url);

print "Making tests on $url\n";
my ($req, $res, $c, $cli);

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

# test first page and check for the last job 
$agent->get($url); ok($agent->success, "Get main page"); $c = $agent->content;
like($c, qr!</html>!, "Check EOP");
ok($c =~ m!(action=job_zoom;jobid=\d+)!, "Get the first JobId");
die "Can't get first jobid ($c)" unless $1;

# test job_zoom page
# check for
#  - job log
#  - fileset
#  - media view
$agent->get("$url?$1"); ok($agent->success,"Get job zoom"); $c=$agent->content;
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
ok($agent->field('pool', $1), "set field pool=$1");
$agent->click_button(value => 'run_job_now');
ok($agent->success(), "submit");
ok($agent->follow_link(text_regex=>qr/here/i), "follow link");
ok($agent->success(), "get job page");

################################################################
# client tests
################################################################

ok($agent->follow_link(text_regex=>qr/Clients/), "Go to Clients page");
$c=$agent->content;
ok($c =~ m!chkbox.value = '(.+?)';!, "get client name");
$cli = $1;

$agent->get("$url?action=client_status;client=$cli");
ok($agent->success(), "submit");
$c=$agent->content;
like($c, qr/Terminated Jobs/, "client status");

$agent->get("$url?action=job;client=$cli");
ok($agent->success(), "submit");
$c=$agent->content;
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
ok($agent->field("location", $loc), "set field location=$loc");
ok($agent->field("cost", 20), "set field cost=20");
ok($agent->field("enabled", "archived"), "try set field enabled=archived");
ok($agent->field("enabled", "no"), "try set field enabled=no");
ok($agent->field("enabled", "yes"), "set field enabled=yes");
$agent->click_button(value => 'location_add');
ok($agent->success(), "submit"); $c=$agent->content;
like($c, qr/$loc/, "Check if location is ok");

################################################################
# Test group basic functions
################################################################

$agent->get("$url?action=client;notingroup=yes");
ok($agent->success(), "submit"); $c=$agent->content;
like($c, qr/$cli/, "check client=$cli");

my $grp = "test$$";
ok($agent->follow_link(text_regex=>qr/Groups/), "Go to Groups page");
$c=$agent->content;
unlike($c, qr/error/i, "Check for group installation");
ok($agent->form_number(2), "Find form");
$agent->click_button(value => 'groups_add');
ok($agent->success(), "submit");
ok($agent->form_number(2), "Find form to create a group");
ok($agent->field("client_group", $grp), "set field client_group=$grp");
$agent->click_button(value => 'groups_add');
ok($agent->success(), "submit");
$c=$agent->content; 
like($c, qr/$grp/, "Check if group is present");

# we can select javascript radio
$agent->get("$url?client_group=$grp;action=groups_edit");
ok($agent->success(), "submit"); $c=$agent->content;
like($c, qr/$grp/, "Check if group is present");
ok($agent->form_number(2), "Find form");
$grp = "newtest$$";
ok($agent->field("newgroup", $grp), "set field newgroup=$grp");
like($c, qr/$cli/, "check client=$cli");
ok($agent->field("client", $cli), "set field client=$cli");
$agent->click_button(value => 'groups_save');
ok($agent->success(), "submit"); $c=$agent->content;
like($c, qr/'$grp'/, "Check if newgroup is present");

$agent->get("$url?client_group=$grp;action=client");
ok($agent->success(), "submit"); $c=$agent->content;
like($c, qr/'$cli'/, "Check if client is present in newgrp");

$agent->get("$url?action=client;notingroup=yes");
ok($agent->success(), "check if client=$cli is already 'not in group'");
$c=$agent->content;
unlike($c, qr/$cli/, "check client=$cli");

$agent->get("$url?client_group=other$grp;action=groups_add");
ok($agent->success(), "create an empty other$grp"); $c=$agent->content;
like($c, qr/'other$grp'/, "check if other$grp was created");

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


# view media
my @vol = ($cont =~ m!<input type='hidden' name='media' value='([^>]+)'>!sg);
@vol = map { ('media', $_) } @vol;
ok(scalar(@vol), "Check for job media using retry fields");
$cont2 = get_content("View media " .  join('=', @vol),
		     action => 'media', @vol);
ok($cont2 =~ m!</html>!, "Check end of page");


ok($cont =~ m!<input type='hidden' name='client' value='[^>]+'>!,
   "Check for job client using retry fields");

# test delete job page
#$cont = get_content("Job delete", action => 'delete', jobid => $1);
#ok($cont =~ m!</html>!, "Check end of page");
