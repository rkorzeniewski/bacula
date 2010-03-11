#!/usr/bin/perl -w
use strict;

=head1 LICENSE

   Bweb - A Bacula web interface
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.

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

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zurich,
   Switzerland, email:ftf@fsfeurope.org.

=head1 USAGE

    - You need to be in a X session
    - Install Selenium IDE addon from http://seleniumhq.org/
    - Install through CPAN WWW::Selenium
      $ perl -e 'install WWW::Selenium' -MCPAN 
    - Download Selenium RC (remote control) from  http://seleniumhq.org/
    - unzip the archive, and start the Selenium server (require java >= 1.5)
      $ java -jar selenium-server.jar
    - Load bweb sql file
    - Start the test
      $ ./tests/bweb-test.pl

=cut

use warnings;
use Time::HiRes qw(sleep);
use Test::WWW::Selenium;
use Test::More "no_plan";
use Test::Exception;
use Getopt::Long;
use scripts::functions;

my ($login, $pass, $url, $verbose, %part, @part, $noclean);
my @available = qw/client group location run missingjob media overview/;

GetOptions ("login=s"  => \$login,
	    "passwd=s" => \$pass,
	    "url|u=s"  => \$url,
            "module=s@"   => \@part,
	    "verbose"  => \$verbose,
            "nocleanup" => \$noclean,
	    );

die "Usage: $0 --url http://.../cgi-bin/bweb/bweb.pl [-m module] [-n]"
    unless ($url);

if (scalar(@part)) {
    %part = map { $_ => 1 } @part;
} else {
    %part = map { $_ => 1 } @available;
}

if (!$noclean) {
    cleanup();
    start_bacula();
}

my $sel = Test::WWW::Selenium->new( host => "localhost", 
                                    port => 4444, 
                                    browser => "*firefox", 
                                    browser_url => $url );

if ($part{client}) {
# test client
    $sel->open_ok("/cgi-bin/bweb/bweb.pl?");
    $sel->wait_for_page_to_load_ok("30000");
    $sel->is_text_present_ok("Informations");

    $sel->click_ok("link=Clients");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//input[\@name='client']"); # click the first client
    $sel->click_ok("//button[\@name='action' and \@value='client_status']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Command output");
    $sel->click_ok("link=Clients");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Running Jobs"); # This message is in client status
}

if ($part{media}) {
# add media
    $sel->open_ok("/cgi-bin/bweb/bweb.pl");
    $sel->click_ok("link=Add Media");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->select_ok("pool", "label=Scratch");
    $sel->select_ok("storage", "label=File");
    $sel->type_ok("nb", "10");
    $sel->click_ok("//button[\@name='action']"); # create 10 volumes
    $sel->wait_for_page_to_load_ok("30000");

  WAIT: {
      for (1..60) {
          if (eval { $sel->is_text_present("Select") }) { pass; last WAIT }
        sleep(1);
      }
      fail("timeout");
}
    $sel->click_ok("link=All Media");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Vol0001");
    $sel->is_text_present_ok("Vol0010");
    $sel->select_ok("mediatype", "label=File");
    $sel->select_ok("volstatus", "label=Append");
    $sel->select_ok("pool", "label=Scratch");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Vol0001");
    $sel->click_ok("media");
    $sel->click_ok("//button[\@name='action' and \@value='update_media']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->select_ok("volstatus", "label=Archive");
    $sel->select_ok("enabled", "label=no");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("arrow_0");
    $sel->is_text_present_ok("New Volume status is: Archive");
    $sel->click_ok("link=All Media");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Archive");
    $sel->select_ok("volstatus", "label=Append");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("media");
    $sel->click_ok("//button[\@name='action' and \@value='media_zoom']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Volume Infos");
    $sel->click_ok("//button[\@name='action' and \@value='purge']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Marking it purged");
}

if ($part{missingjob}) {
# view missing jobs
    $sel->open_ok("/cgi-bin/bweb/bweb.pl");
    $sel->click_ok("link=Missing Jobs");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("backup");
    $sel->is_text_present_ok("BackupCatalog");
    $sel->click_ok("job");
    $sel->click_ok("//input[\@name='job' and \@value='BackupCatalog']");
    $sel->click_ok("//button[\@name='action' and \@value='job']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->body_text_isnt("BackupCatalog");
}

if ($part{run}) {
# run a new job
    $sel->open_ok("/cgi-bin/bweb/bweb.pl?");
    $sel->click_ok("link=Defined Jobs");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->select_ok("job", "label=backup");
    $sel->is_text_present_ok("backup");
    $sel->click_ok("//button[\@name='action' and \@value='run_job_mod']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Default");
    $sel->is_text_present_ok("Full Set");
    $sel->is_text_present_ok("Incremental");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

  WAIT: {
      for (1..60) {
          if (eval { $sel->is_text_present("Start Backup JobId") }) { pass; last WAIT }
          sleep(1);
      }
    fail("timeout");
    }
    $sel->is_text_present_ok("Log: backup on");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

  WAIT: {
      for (1..60) {
          if (eval { $sel->is_text_present("Termination: Backup OK") }) { pass; last WAIT }
          sleep(1);
      }
      fail("timeout");
    }
    my $volume = $sel->get_text("//tr[\@id='even_row']/td[12]");
    $sel->click_ok("//button[\@name='action' and \@value='media']");
    $sel->wait_for_page_to_load_ok("30000");

    my $volume_found = $sel->get_text("//tr[\@id='even_row']/td[1]");
    $sel->click_ok("media");
    $sel->text_is("//tr[\@id='even_row']/td[5]", "Append");
    $sel->click_ok("//button[\@name='action' and \@value='media_zoom']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Volume Infos");
    $sel->click_ok("//img[\@title='terminated normally']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//button[\@name='action' and \@value='fileset_view']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("FileSet Full Set");
    $sel->is_text_present_ok("What is included:");
    $sel->is_text_present_ok("/regress/build");
    $sel->go_back_ok();
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//button[\@name='action' and \@value='run_job_mod']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_element_present_ok("level");
    $sel->selected_value_is("name=level", "Full");
    $sel->selected_label_is("name=fileset", "Full Set");
    $sel->selected_label_is("name=job", "backup");
    $sel->click_ok("//button[\@name='action' and \@value='fileset_view']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("FileSet Full Set");
    $sel->is_text_present_ok("/regress/build");
}

if ($part{group}) {
# test group
    $sel->open_ok("/cgi-bin/bweb/bweb.pl");
    $sel->click_ok("link=Groups");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->text_is("//h1", "Groups");
    $sel->click_ok("//button[\@name='action' and \@value='groups_edit']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->type_ok("newgroup", "All");
    $sel->select_ok("name=client", "index=0");
    $sel->click_ok("//button[\@name='action' and \@value='groups_save']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("All");
    $sel->click_ok("//input [\@name='client_group' and \@value='All']");
    $sel->click_ok("//button[3]");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Group: 'All'");
    $sel->selected_index_is("name=client", "0");
    $sel->click_ok("//button[\@name='action' and \@value='groups_save']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//input[\@name='client_group' and \@value='All']");
    $sel->click_ok("//button[4]");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//input[\@name='client']");
    $sel->click_ok("//button[\@name='action' and \@value='client_edit']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("All");
    $sel->click_ok("link=Groups");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//button[\@name='action' and \@value='groups_edit']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->type_ok("newgroup", "Empty");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("client_group");
    $sel->is_text_present_ok("Empty");
    $sel->click_ok("//button[\@name='action' and \@value='client']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//input[\@name='client']");
    $sel->click_ok("//button[\@name='action' and \@value='client_edit']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("link=Groups");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//input[\@name='client_group' and \@value='Empty']");
    $sel->click_ok("//button[\@name='action' and \@value='groups_del']");
    ok($sel->get_confirmation() =~ /^Do you want to delete this group[\s\S]$/);
    $sel->click_ok("link=Groups");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->type_ok("newgroup", "Empty");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Empty");
    $sel->click_ok("//input[\@name='client_group' and \@value='All']");
    $sel->click_ok("//button[\@name='action' and \@value='client']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//input[\@name='client']");
    $sel->click_ok("//button[\@name='action' and \@value='client_edit']");
    $sel->wait_for_page_to_load_ok("30000");

    ok(not $sel->is_checked("//input[\@name='client_group' and \@value='Empty']"));
    $sel->click_ok("link=Group Statistics");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("All");
    $sel->is_text_present_ok("Empty");
    $sel->click_ok("link=Groups");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//input[\@name='client_group' and \@value='Empty']");
    $sel->click_ok("document.forms[1].elements[4]");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->type_ok("newgroup", "EmptyGroup");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("EmptyGroup");
}

if ($part{location}) {
# test location
    $sel->open_ok("/cgi-bin/bweb/bweb.pl");
    $sel->click_ok("link=Locations");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//button[\@name='action' and \@value='location_add']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->type_ok("location", "Bank");
    $sel->click_ok("//button[\@name='action']"); # save
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Bank");
    $sel->click_ok("location");
    $sel->click_ok("//button[\@name='action' and \@value='location_edit']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->type_ok("cost", "100");
    $sel->is_text_present_ok("Location: Bank");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("100");
    $sel->is_element_present_ok("//img[\@src='/bweb/inflag1.png']");
    $sel->click_ok("location");
    $sel->click_ok("//button[\@name='action' and \@value='location_edit']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->select_ok("enabled", "label=no");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_element_present_ok("//img[\@src='/bweb/inflag0.png']");
    $sel->click_ok("location");
    $sel->click_ok("//button[\@name='action' and \@value='location_edit']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->select_ok("enabled", "label=archived");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_element_present_ok("//img[\@src='/bweb/inflag2.png']");
    $sel->click_ok("location");
    $sel->click_ok("//button[\@name='action' and \@value='location_edit']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->type_ok("newlocation", "Office");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Office");
    $sel->click_ok("link=All Media");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("media");
    $sel->click_ok("//button[\@name='action' and \@value='update_media']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->select_ok("location", "label=Office");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->selected_value_is("location", "Office");
    $sel->click_ok("//button[\@name='action' and \@value='media']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->select_ok("location", "label=Office");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->body_text_isnt("Vol0010");
    $sel->click_ok("link=Locations");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("location");
    $sel->click_ok("//button[\@name='action' and \@value='location_del']");
    ok($sel->get_confirmation() =~ /^Do you want to remove this location[\s\S]$/);
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("Sorry, the location must be empty");
    $sel->click_ok("link=Locations");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->type_ok("location", "OtherPlace");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("location");
    $sel->click_ok("//button[\@name='action' and \@value='media']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("media");
    $sel->click_ok("//button[\@name='action' and \@value='update_media']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->select_ok("location", "label=OtherPlace");
    $sel->click_ok("//button[\@name='action']");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("link=Locations");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("location");
    $sel->click_ok("//button[\@name='action' and \@value='location_del']");
    ok($sel->get_confirmation() =~ /^Do you want to remove this location[\s\S]$/);
    $sel->wait_for_page_to_load_ok("30000");

    $sel->body_text_isnt("Office");
}

if ($part{overview}) {
    $sel->open_ok("/cgi-bin/bweb/bweb.pl");
    $sel->click_ok("link=Jobs overview");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("All");
    $sel->click_ok("link=All");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->click_ok("link=zogi-fd");
    $sel->wait_for_page_to_load_ok("30000");

    $sel->is_text_present_ok("backup");
    $sel->is_text_present_ok("Full Set");
}

if ($part{config}) {
    $sel->open_ok("/cgi-bin/bweb/bweb.pl");
    $sel->click_ok("link=Configuration");
    $sel->wait_for_page_to_load_ok("30000");
    $sel->is_text_present_ok("Main Configuration");

    $sel->click_ok("//button[\@name='action' and \@value='edit_main_conf']");
    $sel->wait_for_page_to_load_ok("30000");
    $sel->is_text_present_ok("Main Configuration");
    my $dbi = $sel->get_text("dbi");
    my $user = $sel->get_text("user");
    my $pass = $sel->get_text("password");
    my $histo = $sel->get_text("stat_job_table");

    print "dbi=$dbi histo=$histo\n";
    if ($histo eq 'Job') {
        $sel->type_ok("stat_job_table", "JobHisto");
    } else {
        $sel->type_ok("stat_job_table", "Job");
    }
}
