#!/usr/bin/perl -w
use strict ;

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

use Data::Dumper;
use Bweb;
use CGI;

my $client_re = qr/^([\w\d\.-]+)$/;

my $action = CGI::param('action') || 'begin';

if ($action eq 'restore') {
    print CGI::header('text/brestore');	# specialy to run brestore.pl

} else {
    print CGI::header('text/html');
}

# loading config file
my $conf = new Bweb::Config(config_file => $Bweb::config_file);
$conf->load();

my $bweb = new Bweb(info => $conf);

# just send data with text/brestore content
if ($action eq 'restore') {
    $bweb->restore();
    exit 0;
}

my $arg = $bweb->get_form('jobid', 'limit', 'offset', 'age');

$bweb->display_begin();

# if no configuration, we send edit_conf
if ($action ne 'apply_conf' and !$bweb->{info}->{dbi}) {
    $action = 'edit_conf';
}

if ($action eq 'begin') {		# main display
    print "<div style='left=0;'><table border='0'><tr><td valign='top' width='100%'>\n";
    $bweb->display_general(age => $arg->{age});
    $bweb->display_running_jobs(0);
    print "</td><td valign='top'>";
    $bweb->display({}, "stats.tpl");
    print "</td></tr></table></div>";
    $bweb->display_job(limit => 10); 

} elsif ($action eq 'view_conf') {
    $bweb->can_do('r_configure');
    $conf->view()

} elsif ($action eq 'edit_conf') {
    $bweb->can_do('r_configure');
    $conf->edit();

} elsif ($action eq 'apply_conf') {
    $bweb->can_do('r_configure');
    $conf->modify();

} elsif ($action eq 'user_del') {
    $bweb->users_del();

} elsif ($action eq 'user_add') {
    $bweb->users_add();

} elsif ($action eq 'user_edit') {
    $bweb->display_user();

} elsif ($action eq 'user_save') {
    $bweb->users_add();

} elsif ($action eq 'users') {
    $bweb->display_users();

} elsif ($action eq 'client') {	
    $bweb->display_clients();

} elsif ($action eq 'pool') {
    $bweb->display_pool();

} elsif ($action eq 'location_edit') {
    $bweb->location_edit();

} elsif ($action eq 'location_save') {
    $bweb->location_save();

} elsif ($action eq 'location_add') {
    $bweb->location_add();

} elsif ($action eq 'location_del') {
    $bweb->location_del();

} elsif ($action eq 'media') {
    print "<div><table border='0'><tr><td valign='top'>\n";
    my $fields = $bweb->get_form(qw/db_locations db_pools expired
				    qlocations qpools volstatus qre_media
				    limit  qmediatypes db_mediatypes/);
    $bweb->display($fields, "display_form_media.tpl");

    print "</td><td valign='top'>";
    $bweb->display_media(offset => $arg->{offset},
			 limit => $arg->{limit});
    print "</td></tr></table></div>";

} elsif ($action eq 'allmedia') {
    $bweb->display_allmedia();

} elsif ($action eq 'eject') {
    $bweb->can_do('r_autochanger_mgnt');

    my $arg = $bweb->get_form("ach");
    my $a = $bweb->ach_get($arg->{ach});
    
    if ($a) {
	$a->status();
	foreach my $slot (CGI::param('slot')) {
	    print $a->{error} unless $a->send_to_io($slot);
	}

	foreach my $media (CGI::param('media')) {
	    my $slot = $a->get_media_slot($media);
	    print $a->{error} unless $a->send_to_io($slot);
	}

	$a->display_content();
    }

} elsif ($action eq 'eject_media') {
    $bweb->eject_media();

} elsif ($action eq 'clear_io') {
    $bweb->can_do('r_autochanger_mgnt');

    my $arg = $bweb->get_form('ach');

    my $a = $bweb->ach_get($arg->{ach});
    if (defined $a) {
	$a->status();
	$a->clear_io();
	$a->display_content();
    }

} elsif ($action eq 'ach_edit') {
    $bweb->ach_edit();

} elsif ($action eq 'ach_del') {
    $bweb->ach_del();

} elsif ($action eq 'ach_view') {
    $bweb->can_do('r_autochanger_mgnt');

    # TODO : get autochanger name and create it
    $bweb->connect_db();
    my $arg = $bweb->get_form('ach');

    my $a = $bweb->ach_get($arg->{ach});
    if ($a) {
	$a->status();
	$a->display_content();
    }

} elsif ($action eq 'ach_add') {
    $bweb->ach_add();

} elsif ($action eq 'ach_load') {
    $bweb->can_do('r_autochanger_mgnt');

    my $arg = $bweb->get_form('ach', 'drive', 'slot');
    
    my $a = $bweb->ach_get($arg->{ach});

    if (defined $a and defined $arg->{drive} and defined $arg->{slot})
    {
	my $b = new Bconsole(pref => $conf, timeout => 300, log_stdout => 1) ;
	# TODO : use template here
	print "<pre>\n";
	$b->send_cmd("mount slot=$arg->{slot} drive=$arg->{drive} storage=\"" . $a->get_drive_name($arg->{drive}) . '"');
	print "</pre>\n";
    } else {
	$bweb->error("Can't get drive, slot or ach");
    }
    
} elsif ($action eq 'ach_unload') {
    $bweb->can_do('r_autochanger_mgnt');

    my $arg = $bweb->get_form('drive', 'slot', 'ach');

    my $a = $bweb->ach_get($arg->{ach});

    if (defined $a and defined $arg->{drive} and defined $arg->{slot})
    {
	my $b = new Bconsole(pref => $conf, timeout => 300, log_stdout => 1) ;
	# TODO : use template here
	print "<pre>\n";
	$b->send_cmd("umount drive=$arg->{drive} storage=\"" . $a->get_drive_name($arg->{drive}) . '"');
	print "</pre>\n";

    } else {
	$bweb->error("Can't get drive, slot or ach");
    }   
} elsif ($action eq 'intern_media') {
    $bweb->help_intern();

} elsif ($action eq 'compute_intern_media') {
    $bweb->help_intern_compute();

} elsif ($action eq 'extern_media') {
    $bweb->help_extern();

} elsif ($action eq 'compute_extern_media') {
    $bweb->help_extern_compute();

} elsif ($action eq 'extern') {
    $bweb->can_do('r_media_mgnt');
    $bweb->can_do('r_autochanger_mgnt');

    print "<div style='float: left;'>";
    my @achs = $bweb->eject_media();
    for my $ach (@achs) {
        CGI::param('ach', $ach);
	$bweb->update_slots();
    }
    print "</div><div style='float: left;margin-left: 20px;'>";
    $bweb->move_media('no');	# enabled = no
    print "</div>";

} elsif ($action eq 'move_email') {
    $bweb->move_email();

} elsif ($action eq 'change_location') {
    $bweb->location_change();

} elsif ($action eq 'location') {
    $bweb->location_display();

} elsif ($action eq 'about') {
    $bweb->display($bweb, 'about.tpl');

} elsif ($action eq 'intern') {
    $bweb->move_media('yes'); # TODO : remove that

} elsif ($action eq 'move_media') {
    my $a = $bweb->get_form('enabled');
    $bweb->move_media($a->{enabled}); 

} elsif ($action eq 'save_location') {
    $bweb->save_location();

} elsif ($action eq 'update_location') {
    $bweb->update_location();

} elsif ($action eq 'update_media') {
    $bweb->update_media();

} elsif ($action eq 'do_update_media') {
    $bweb->do_update_media();

} elsif ($action eq 'update_slots') {
    $bweb->update_slots();

} elsif ($action eq 'graph') {
    $bweb->display_graph();

} elsif ($action eq 'next_job') {
    $bweb->director_show_sched();

} elsif ($action eq 'enable_job') {
    $bweb->enable_disable_job(1);

} elsif ($action eq 'disable_job') {
    $bweb->enable_disable_job(0);

} elsif ($action eq 'groups') {
    $bweb->display_groups();

} elsif ($action eq 'groups_edit') {
    $bweb->groups_edit();

} elsif ($action eq 'groups_save') {
    $bweb->groups_save();

} elsif ($action eq 'groups_add') {
    $bweb->groups_add();

} elsif ($action eq 'groups_del') {
    $bweb->groups_del();

} elsif ($action eq 'job') {
    $bweb->can_do('r_view_job');
    print "<div><table border='0'><tr><td valign='top'>\n";
    my $fields = $bweb->get_form(qw/status level filter db_clients
				    db_filesets 
				    limit age offset qclients qfilesets
				    jobtype qpools db_pools
				    db_client_groups qclient_groups/); # drop this to hide 

    $bweb->display($fields, "display_form_job.tpl");

    print "</td><td valign='top'>";
    $bweb->display_job(age => $arg->{age},  # last 7 days
		       offset => $arg->{offset},
		       limit => $arg->{limit});
    print "</td></tr></table></div>";
} elsif ($action eq 'job_group') {
    $bweb->can_do('r_view_job');
    print "<div><table border='0'><tr><td valign='top'>\n";
    my $fields = $bweb->get_form(qw/limit level age filter 
                                    db_client_groups qclient_groups/); # drop this to hide 

    $fields->{hide_status} = 1;
    $fields->{hide_type} = 1;
    $fields->{action} = 'job_group';

    $bweb->display($fields, "display_form_job.tpl");

    print "</td><td valign='top'>";
    $bweb->display_job_group(age => $arg->{age},  # last 7 days
			     limit => $arg->{limit});
    print "</td></tr></table></div>";
} elsif ($action eq 'client_stats') {

    foreach my $client (CGI::param('client')) {
	if ($client =~ m/$client_re/) {
	    $bweb->display_client_stats(clientname => $1,
					age => $arg->{age});
	}
    }

} elsif ($action eq 'group_stats') {
    $bweb->display_group_stats(age => $arg->{age});

} elsif ($action eq 'running') {
    $bweb->display_running_jobs(1);

} elsif ($action eq 'dsp_cur_job') {
    $bweb->display_running_job();

} elsif ($action eq 'update_from_pool') {
    $bweb->can_do('r_media_mgnt');
    my $elt = $bweb->get_form(qw/media pool/);
    unless ($elt->{media} || $elt->{pool}) {
	$bweb->error("Can't get media or pool param");
    } else {
	my $b = new Bconsole(pref => $conf) ;

	$bweb->display({
 content => $b->send_cmd("update volume=$elt->{media} fromPool=$elt->{pool}"),
 title => "Update pool",
 name => "update volume=$elt->{media} fromPool=$elt->{pool}",
	}, "command.tpl");	
    }
    
    $bweb->update_media();

} elsif ($action eq 'client_status') {
    $bweb->can_do('r_client_status');
    my $b;
    foreach my $client (CGI::param('client')) {
	if ($client =~ m/$client_re/) {
	    $client = $1;
	    $b = new Bconsole(pref => $conf) 
		unless ($b) ;

	    $bweb->display({
		content => $b->send_cmd("st client=$client"),
		title => "Client status",
		name => $client,
	    }, "command.tpl");
	    
	} else {
	    $bweb->error("Can't get client selection");
	}
    }

} elsif ($action eq 'cancel_job') {
    $bweb->cancel_job();

} elsif  ($action eq 'media_zoom') {
    $bweb->display_media_zoom();

} elsif  ($action eq 'job_zoom') {
    if ($arg->{jobid}) {
	$bweb->display_job_zoom($arg->{jobid});
	$bweb->get_job_log();
    } 
} elsif ($action eq 'job_log') {
    $bweb->get_job_log();

} elsif ($action eq 'prune') {
    $bweb->prune();

} elsif ($action eq 'purge') {
    $bweb->purge();

} elsif ($action eq 'run_job') {
    $bweb->run_job();

} elsif ($action eq 'run_job_mod') {
    $bweb->run_job_mod();

} elsif ($action eq 'run_job_now') {
    $bweb->run_job_now();

} elsif ($action eq 'label_barcodes') {
    $bweb->label_barcodes();

} elsif ($action eq 'delete') {
    $bweb->delete();

} elsif ($action eq 'fileset_view') {
    $bweb->fileset_view();

} else {
    $bweb->error("Sorry, this action doesn't exist");
}

$bweb->display_end();

$bweb->dbh_disconnect();

__END__

TODO :

 o Affichage des job en cours, termines
 o Affichage du detail d'un job (status client)
 o Acces aux log d'une sauvegarde
 o Cancel d'un job
 o Lancement d'un job

 o Affichage des medias (pool, cf bacweb)
 o Affichage de la liste des cartouches
 o Affichage d'un autochangeur
 o Mise a jour des slots
 o Label barcodes
 o Affichage des medias qui ont besoin d'etre change

 o Affichage des stats sur les dernieres sauvegardes (cf bacula-web)
 o Affichage des stats sur un type de job
 o Affichage des infos de query.sql

 - Affichage des du TapeAlert sur le site
 - Recuperation des erreurs SCSI de /var/log/kern.log

 o update d'un volume
 o update d'un pool

 o Configuration des autochanger a la main dans un hash dumper
