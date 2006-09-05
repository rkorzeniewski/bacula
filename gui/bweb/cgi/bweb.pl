#!/usr/bin/perl -w
use strict ;

=head1 LICENSE

    Copyright (C) 2006 Eric Bollengier
        All rights reserved.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

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
my $conf = new Bweb::Config(config_file => '/etc/bweb/config');
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
    print "
<div class='titlediv'>
  <h1 class='newstitle'> Statistics (last 48 hours)</h1>
</div>
<div class='bodydiv'>
<a href='?action=job;age=172800;jobtype=B'>
<img src='bgraph.pl?age=172800;width=600;height=250;graph=job_size;limit=100;action=graph;legend=off' alt='Nothing to display'>
</a>
</div>";
    print "</td></tr></table></div>";
    $bweb->display_job(limit => 10); 

} elsif ($action eq 'view_conf') {
    $conf->view()

} elsif ($action eq 'edit_conf') {
    $conf->edit();

} elsif ($action eq 'apply_conf') {
    $conf->modify();

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
    $bweb->del_location();

} elsif ($action eq 'media') {
    $bweb->display_media();

} elsif ($action eq 'medias') {
    $bweb->display_medias();

} elsif ($action eq 'eject') {
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
    my $arg = $bweb->get_form('ach', 'drive', 'slot');
    
    my $a = $bweb->ach_get($arg->{ach});

    if (defined $a and defined $arg->{drive} and defined $arg->{slot})
    {
	my $b = new Bconsole(pref => $conf, timeout => 300, log_stdout => 1) ;
	# TODO : use template here
	print "<pre>\n";
	$b->send_cmd_with_drive("mount slot=$arg->{slot} storage=\"" . $a->get_drive_name($arg->{drive}) . '"',
				$arg->{drive});
	print "</pre>\n";
    } else {
	$bweb->error("Can't get drive, slot or ach");
    }
    
} elsif ($action eq 'ach_unload') {
    my $arg = $bweb->get_form('drive', 'slot', 'ach');

    my $a = $bweb->ach_get($arg->{ach});

    if (defined $a and defined $arg->{drive} and defined $arg->{slot})
    {
	my $b = new Bconsole(pref => $conf, timeout => 300, log_stdout => 1) ;
	# TODO : use template here
	print "<pre>\n";
	$b->send_cmd_with_drive("umount storage=\"" . $a->get_drive_name($arg->{drive}) . '"',
				$arg->{drive});

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
    print "TODO : Eject ", join(",", CGI::param('media'));
    $bweb->move_media();

} elsif ($action eq 'change_location') {
    $bweb->change_location();

} elsif ($action eq 'location') {
    $bweb->display_location();

} elsif ($action eq 'about') {
    $bweb->display($bweb, 'about.tpl');

} elsif ($action eq 'intern') {
    $bweb->move_media(); # TODO : remove that

} elsif ($action eq 'move_media') {
    $bweb->move_media(); 

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

} elsif ($action eq 'job') {

    print "<div><table border='0'><tr><td valign='top'>\n";
    my $fields = $bweb->get_form(qw/status level db_clients db_filesets
				    limit age offset qclients qfilesets
				    jobtype/);
    $bweb->display($fields, "display_form_job.tpl");

    print "</td><td valign='top'>";
    $bweb->display_job(age => $arg->{age},  # last 7 days
		       offset => $arg->{offset},
		       limit => $arg->{limit});
    print "</td></tr></table></div>";
} elsif ($action eq 'client_stats') {

    foreach my $client (CGI::param('client')) {
	if ($client =~ m/$client_re/) {
	    $bweb->display_client_stats(clientname => $1,
					age => $arg->{age});
	}
    }

} elsif ($action eq 'running') {
    $bweb->display_running_jobs(1);

} elsif ($action eq 'dsp_cur_job') {
    $bweb->display_running_job();

} elsif ($action eq 'update_from_pool') {
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
    $bweb->error("Sorry, this action don't exist");
}

$bweb->display_end();


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
