#!/usr/bin/perl -w

use strict;

# path to your brestore.glade
my $glade_file = 'brestore.glade' ;

=head1 NAME

    brestore.pl - A Perl/Gtk console for Bacula

=head1 VERSION

    $Id$

=head1 INSTALL
  
  Setup ~/.brestore.conf to find your brestore.glade

  On debian like system, you need :
    - libgtk2-gladexml-perl
    - libdbd-mysql-perl or libdbd-pg-perl
    - libexpect-perl

  You have to add brestore_xxx tables to your catalog.

  To speed up database query you have to create theses indexes
    - CREATE INDEX file_pathid on File(PathId);
    - ...

  To follow restore job, you must have a running Bweb installation.

=head1 COPYRIGHT

   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2006 Free Software Foundation Europe e.V.

   Brestore authors are Marc Cousin and Eric Bollengier.
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

   Base 64 functions from Karl Hakimian <hakimian@aha.com>
   Integrally copied from recover.pl from bacula source distribution.

=cut

use Gtk2;		# auto-initialize Gtk2
use Gtk2::GladeXML;
use Gtk2::SimpleList;		# easy wrapper for list views
use Gtk2::Gdk::Keysyms;		# keyboard code constants
use Data::Dumper qw/Dumper/;
my $debug=0;			# can be on brestore.conf
our ($VERSION) = ('$Revision$' =~ /(\d+)/);

package Pref;
use DBI;
sub new
{
    my ($class, $config_file) = @_;
    
    my $self = bless {
	config_file => $config_file,
	password => '',		# db passwd
	username => '',		# db username
	connection_string => '',# db connection string
	bconsole => 'bconsole',	# path and arg to bconsole
	bsr_dest => '',		# destination url for bsr files
	debug    => 0,		# debug level 0|1
	use_ok_bkp_only => 1,	# dont use bad backup
	bweb     => 'http://localhost/cgi-bin/bweb/bweb.pl', # bweb url
	see_all_versions => 0,  # display all file versions in FileInfo
	mozilla  => 'mozilla',  # mozilla bin
	default_restore_job => 'restore', # regular expression to select default
				   # restore job

	# keywords that are used to fill DlgPref
	chk_keyword =>  [ qw/use_ok_bkp_only debug see_all_versions/ ],
        entry_keyword => [ qw/username password bweb mozilla
			  connection_string default_restore_job
			  bconsole bsr_dest glade_file/],
    };

    $self->read_config();

    return $self;
}

sub read_config
{
    my ($self) = @_;

    # We read the parameters. They come from the configuration files
    my $cfgfile ; my $tmpbuffer;
    if (open FICCFG, $self->{config_file})
    {
	while(read FICCFG,$tmpbuffer,4096)
	{
	    $cfgfile .= $tmpbuffer;
	}
	close FICCFG;
	my $refparams;
	no strict; # I have no idea of the contents of the file
	eval '$refparams' . " = $cfgfile";
	use strict;
	
	for my $p (keys %{$refparams}) {
	    $self->{$p} = $refparams->{$p};
	}

    } else {
	# TODO : Force dumb default values and display a message
    }
}

sub write_config
{
    my ($self) = @_;
    
    $self->{error} = '';
    my %parameters;

    for my $k (@{ $self->{entry_keyword} }) { 
	$parameters{$k} = $self->{$k};
    }

    for my $k (@{ $self->{chk_keyword} }) { 
	$parameters{$k} = $self->{$k};
    }

    if (open FICCFG,">$self->{config_file}")
    {
	print FICCFG Data::Dumper->Dump([\%parameters], [qw($parameters)]);
	close FICCFG;
    }
    else
    {
	$self->{error} = "Can't write configuration $!";
    }
    return $self->{error};
}

sub connect_db
{
    my $self = shift ;

    if ($self->{dbh}) {
	$self->{dbh}->disconnect() ;
    }

    delete $self->{dbh};
    delete $self->{error};

    if (not $self->{connection_string})
    {
	# The parameters have not been set. Maybe the conf
	# file is empty for now
	$self->{error} = "No configuration found for database connection. " .
	                 "Please set this up.";
	return 0;
    }
    
    if (not eval {
	$self->{dbh} = DBI->connect($self->{connection_string}, 
				    $self->{username},
				    $self->{password})
	})
    {
	$self->{error} = "Can't open bacula database. " . 
	                 "Database connect string '" . 
                         $self->{connection_string} ."' $!";
	return 0;
    }
    $self->{is_mysql} = ($self->{connection_string} =~ m/dbi:mysql/i);
    $self->{dbh}->{RowCacheSize}=100;
    return 1;
}

sub go_bweb
{    
    my ($self, $url, $msg) = @_;

    unless ($self->{mozilla} and $self->{bweb}) {
	new DlgWarn("You must install Bweb and set your mozilla bin to $msg");
	return -1;
    }

    if ($^O eq 'MSWin32') {
	system("start /B $self->{mozilla} \"$self->{bweb}$url\"");

    } elsif (!fork()) {
	system("$self->{mozilla} -remote 'Ping()'");
	my $cmd = "$self->{mozilla} '$self->{bweb}$url'";
	if ($? == 0) {
	    $cmd = "$self->{mozilla} -remote 'OpenURL($self->{bweb}$url,new-tab)'" ;
	}
	exec($cmd);
	exit 1;
    } 
    return ($? == 0);
}

1;

################################################################

package Bbase;

sub new
{
    my ($class, %arg) = @_;

    my $self = bless {
	conf => $arg{conf},
    }, $class;

    return $self;
}

use Data::Dumper;

sub debug
{
    my ($self, $what, %arg) = @_;
    
    if ($self->{conf}->{debug} and defined $what) {
	my $level=0;
	if ($arg{up}) {
	    $level++;
	}
	my $line = (caller($level))[2];
	my $func = (caller($level+1))[3] || 'main';
	print "$func:$line\t";  
	if (ref $what) {
	    print Data::Dumper::Dumper($what);
	} elsif ($arg{md5}) {
	    print "MD5=", md5_base64($what), " str=", $what,"\n";
	} else {
	    print $what, "\n";
	}
    }
}

sub dbh_strcat
{
    my ($self, @what) = @_;
    if ($self->{conf}->{connection_string} =~ /dbi:pg/i) {
	return join(' || ', @what);
    } else {
	return 'CONCAT(' . join(',', @what) . ')' ;
    }
}

sub dbh_prepare
{
    my ($self, $query) = @_;
    $self->debug($query, up => 1);
    return $self->{conf}->{dbh}->prepare($query);    
}

sub dbh_do
{
    my ($self, $query) = @_;
    $self->debug($query, up => 1);
    return $self->{conf}->{dbh}->do($query);
}

sub dbh_selectall_arrayref
{
    my ($self, $query) = @_;
    $self->debug($query, up => 1);
    return $self->{conf}->{dbh}->selectall_arrayref($query);
}

sub dbh_selectrow_arrayref
{
    my ($self, $query) = @_;
    $self->debug($query, up => 1);
    return $self->{conf}->{dbh}->selectrow_arrayref($query);
}

sub dbh
{
    my ($self) = @_;
    return $self->{conf}->{dbh};
}

1;

################################################################
# Manage preference
package DlgPref;

# my $pref = new Pref(config_file => 'brestore.conf');
# my $dlg = new DlgPref($pref);
# my $dlg_resto = new DlgResto($pref);
# $dlg->display($dlg_resto);
sub new
{
    my ($class, $pref) = @_;

    my $self = bless {
	pref => $pref,		# Pref ref
	dlgresto => undef,	# DlgResto ref
	};

    return $self;
}

sub display
{
    my ($self, $dlgresto) = @_ ;

    unless ($self->{glade}) {
	$self->{glade} = Gtk2::GladeXML->new($glade_file, "dlg_pref") ;
	$self->{glade}->signal_autoconnect_from_package($self);
    }

    $self->{dlgresto} = $dlgresto;

    my $g = $self->{glade};
    my $p = $self->{pref};

    for my $k (@{ $p->{entry_keyword} }) {
	$g->get_widget("entry_$k")->set_text($p->{$k}) ;
    }

    for my $k (@{ $p->{chk_keyword} }) {
	$g->get_widget("chkbp_$k")->set_active($p->{$k}) ;
    }

    $g->get_widget("dlg_pref")->show_all() ;
}

sub on_applybutton_clicked
{
    my ($self) = @_;
    my $glade = $self->{glade};
    my $pref  = $self->{pref};

    for my $k (@{ $pref->{entry_keyword} }) {
	my $w = $glade->get_widget("entry_$k") ;
	$pref->{$k} = $w->get_text();
    }

    for my $k (@{ $pref->{chk_keyword} }) {
	my $w = $glade->get_widget("chkbp_$k") ;
	$pref->{$k} = $w->get_active();
    }

    if (!$pref->write_config() && $pref->connect_db()) {
        $self->{dlgresto}->set_status('Preferences updated');
	$self->{dlgresto}->init_server_backup_combobox();
	$self->{dlgresto}->set_status($pref->{error});

    } else {
	$self->{dlgresto}->set_status($pref->{error});
    }
}

# Handle prefs ok click (apply/dismiss dialog)
sub on_okbutton_clicked 
{
    my ($self) = @_;
    $self->on_applybutton_clicked();

    unless ($self->{pref}->{error}) {
	$self->on_cancelbutton_clicked();
    }
}
sub on_dialog_delete_event
{
    my ($self) = @_;
    $self->on_cancelbutton_clicked();
    1;
}

sub on_cancelbutton_clicked
{
    my ($self) = @_;
    $self->{glade}->get_widget('dlg_pref')->hide();
    delete $self->{dlgresto};
}
1;

################################################################
# Display all file revision in a separated window
package DlgFileVersion;

sub on_versions_close_clicked
{
    my ($self, $widget)=@_;
    $self->{version}->destroy();
}

sub on_selection_button_press_event
{
    print STDERR "on_selection_button_press_event()\n";
}

sub fileview_data_get
{
    my ($self, $widget, $context, $data, $info, $time,$string) = @_;

    DlgResto::drag_set_info($widget, 
			    $self->{pwd},
			    $data);
}

#
# new DlgFileVersion(Bvfs, "client", pathid, fileid, "/path/to/", "filename");
#
sub new
{
    my ($class, $bvfs, $client, $pathid, $fileid, $path, $fn) = @_;
    my $self = bless {
	pwd       => $path,
	version   => undef, # main window
	};

    # we load version widget of $glade_file
    my $glade_box = Gtk2::GladeXML->new($glade_file, "dlg_version");

    # Connect signals magically
    $glade_box->signal_autoconnect_from_package($self);

    $glade_box->get_widget("version_label")
	->set_markup("<b>File revisions : $client:$path$fn</b>");

    my $widget = $glade_box->get_widget('version_fileview');
    my $fileview = Gtk2::SimpleList->new_from_treeview(
		   $widget,
		   'h_pathid'      => 'hidden',
		   'h_filenameid'  => 'hidden',
		   'h_name'        => 'hidden',
                   'h_jobid'       => 'hidden',
                   'h_type'        => 'hidden',

		   'InChanger'     => 'pixbuf',
		   'Volume'        => 'text',
		   'JobId'         => 'text',
		   'Size'          => 'text',
		   'Date'          => 'text',
		   'MD5'           => 'text',
						       );
    DlgResto::init_drag_drop($fileview);

    my @v = $bvfs->get_all_file_versions($pathid, 
					 $fileid,
					 $client,
					 1);
    for my $ver (@v) {
	my (undef,$pid,$fid,$jobid,$fileindex,$mtime,$size,
	    $inchanger,$md5,$volname) = @{$ver};
	my $icon = ($inchanger)?$DlgResto::yesicon:$DlgResto::noicon;

	DlgResto::listview_push($fileview,$pid,$fid,
				$fn, $jobid, 'file', 
				$icon, $volname, $jobid,DlgResto::human($size),
				scalar(localtime($mtime)), $md5);
    }

    $self->{version} = $glade_box->get_widget('dlg_version');
    $self->{version}->show();
    
    return $self;
}

sub on_forward_keypress
{
    return 0;
}

1;
################################################################
# Display a warning message
package DlgWarn;

sub new
{
    my ($package, $text) = @_;

    my $self = bless {};

    my $glade = Gtk2::GladeXML->new($glade_file, "dlg_warn");

    # Connect signals magically
    $glade->signal_autoconnect_from_package($self);
    $glade->get_widget('label_warn')->set_text($text);

    print STDERR "$text\n";

    $self->{window} = $glade->get_widget('dlg_warn');
    $self->{window}->show_all();
    return $self;
}

sub on_close_clicked
{
    my ($self) = @_;
    $self->{window}->destroy();
}
1;

################################################################

package DlgLaunch;

#use Bconsole;

# %arg = (bsr_file => '/path/to/bsr',       # on director
#         volumes  => [ '00001', '00004']
#         pref     => ref Pref
#         );

sub get_bconsole
{
    my ($pref) = (@_);

    if ($pref->{bconsole} =~ /^http/) {
	return new BwebConsole(pref => $pref);
    } else {
	if (eval { require Bconsole; }) {
	    return new Bconsole(pref => $pref);
	} else {
	    new DlgWarn("Can't use bconsole, verify your setup");
	    return undef;
	}
    }
}

sub new
{
    my ($class, %arg) = @_;

    my $self = bless {
	bsr_file => $arg{bsr_file}, # /path/to/bsr on director
	pref     => $arg{pref},	    # Pref ref
	glade    => undef,	    # GladeXML ref
	bconsole => undef,	    # Bconsole ref
    };

    my $console = $self->{bconsole} = get_bconsole($arg{pref});
    unless ($console) {
	return undef;
    }

    # we load launch widget of $glade_file
    my $glade = $self->{glade} = Gtk2::GladeXML->new($glade_file, 
						     "dlg_launch");

    # Connect signals magically
    $glade->signal_autoconnect_from_package($self);

    my $widget = $glade->get_widget('volumeview');
    my $volview = Gtk2::SimpleList->new_from_treeview(
		   $widget,
		   'InChanger'     => 'pixbuf',
                   'Volume'        => 'text', 
		   );       

    my $infos = get_volume_inchanger($arg{pref}->{dbh}, $arg{volumes}) ;
    
    # we replace 0 and 1 by $noicon and $yesicon
    for my $i (@{$infos}) {
	if ($i->[0] == 0) {
	    $i->[0] = $DlgResto::noicon;
	} else {
	    $i->[0] = $DlgResto::yesicon;
	}
    }

    # fill volume view
    push @{ $volview->{data} }, @{$infos} ;
    
    $console->prepare(qw/list_client list_job list_fileset list_storage/);

    # fill client combobox (with director defined clients
    my @clients = $console->list_client() ; # get from bconsole
    if ($console->{error}) {
	new DlgWarn("Can't use bconsole:\n$arg{pref}->{bconsole}: $console->{error}") ;
    }
    my $w = $self->{combo_client} = $glade->get_widget('combo_launch_client') ;
    $self->{list_client} = DlgResto::init_combo($w, 'text');
    DlgResto::fill_combo($self->{list_client}, 
			 $DlgResto::client_list_empty,
			 @clients);
    $w->set_active(0);

    # fill fileset combobox
    my @fileset = $console->list_fileset() ;
    $w = $self->{combo_fileset} = $glade->get_widget('combo_launch_fileset') ;
    $self->{list_fileset} = DlgResto::init_combo($w, 'text');
    DlgResto::fill_combo($self->{list_fileset}, '', @fileset); 

    # fill job combobox
    my @job = $console->list_job() ;
    $w = $self->{combo_job} = $glade->get_widget('combo_launch_job') ;
    $self->{list_job} = DlgResto::init_combo($w, 'text');
    DlgResto::fill_combo($self->{list_job}, '', @job);
    
    # find default_restore_job in jobs list
    my $default_restore_job = $arg{pref}->{default_restore_job} ;
    my $index=0;
    my $i=1;			# 0 is ''
    for my $j (@job) {
	if ($j =~ /$default_restore_job/io) {
	    $index=$i;
	    last;
	}
	$i++;
    }
    $w->set_active($index);

    # fill storage combobox
    my @storage = $console->list_storage() ;
    $w = $self->{combo_storage} = $glade->get_widget('combo_launch_storage') ;
    $self->{list_storage} = DlgResto::init_combo($w, 'text');
    DlgResto::fill_combo($self->{list_storage}, '', @storage);

    $glade->get_widget('dlg_launch')->show_all();

    return $self;
}

sub show_job
{
    my ($self, $client, $jobid) = @_;

    my $ret = $self->{pref}->go_bweb("?action=dsp_cur_job;jobid=$jobid;client=$client", "view job status");

    $self->on_cancel_resto_clicked();

    if ($ret == -1) {
	my $widget = Gtk2::MessageDialog->new(undef, 'modal', 'info', 'close', 
"Your job have been submited to bacula.
To follow it, you must use bconsole (or install/configure bweb)");
	$widget->run;
	$widget->destroy();
    }
}

sub on_use_regexp_toggled
{
    my ($self,$widget) = @_;
    my $act = $widget->get_active();

    foreach my $w ('entry_launch_where') {
	$self->{glade}->get_widget($w)->set_sensitive(!$act);
    }

    foreach my $w ('entry_add_prefix', 'entry_strip_prefix', 
		   'entry_add_suffix','entry_rwhere','chk_use_regexp')
    {
	$self->{glade}->get_widget($w)->set_sensitive($act);
    }

    if ($act) { # if we activate file relocation, we reset use_regexp
	$self->{glade}->get_widget('entry_rwhere')->set_sensitive(0);
	$self->{glade}->get_widget('chk_use_regexp')->set_active(0);
    }
}


sub on_use_rwhere_toggled
{
    my ($self,$widget) = @_;
    my $act = $widget->get_active();

    foreach my $w ('entry_rwhere') {
	$self->{glade}->get_widget($w)->set_sensitive($act);
    }

    foreach my $w ('entry_add_prefix', 'entry_strip_prefix', 
		   'entry_add_suffix')
    {
	$self->{glade}->get_widget($w)->set_sensitive(!$act);
    }
}

sub on_cancel_resto_clicked
{
    my ($self) = @_ ;
    $self->{glade}->get_widget('dlg_launch')->destroy();
}

sub get_where
{
    my ($self) = @_ ;

    if ($self->{glade}->get_widget('chk_file_relocation')->get_active()) {
	# using regexp
	if ($self->{glade}->get_widget('chk_use_regexp')->get_active()) {

	    return ('regexwhere', 
		    $self->{glade}->get_widget('entry_rwhere')->get_active());
	}
	    
	# using regexp utils
	my @ret;
	my ($strip_prefix, $add_prefix, $add_suffix) = 
	    ($self->{glade}->get_widget('entry_strip_prefix')->get_text(),
	     $self->{glade}->get_widget('entry_add_prefix')->get_text(),
	     $self->{glade}->get_widget('entry_add_suffix')->get_text());
	    
	if ($strip_prefix) {
	    push @ret,"!$strip_prefix!!i";
	}
	
	if ($add_prefix) {
	    push @ret,"!^!$add_prefix!";
	}

	if ($add_suffix) {
	    push @ret,"!([^/])\$!\$1$add_suffix!";
	}

	return ('regexwhere', join(',', @ret));

    } else { # using where
	return ('where', 
		$self->{glade}->get_widget('entry_launch_where')->get_text());
    }
}

sub on_submit_resto_clicked
{
    my ($self) = @_ ;
    my $glade = $self->{glade};

    my $r = $self->copy_bsr($self->{bsr_file}, $self->{pref}->{bsr_dest}) ;
    
    unless ($r) {
	new DlgWarn("Can't copy bsr file to director ($self->{error})");
	return;
    }

    my $fileset = $glade->get_widget('combo_launch_fileset')
	                       ->get_active_text();

    my $storage = $glade->get_widget('combo_launch_storage')
	                       ->get_active_text();

    my ($where_cmd, $where) = $self->get_where();
    print "$where_cmd => $where\n";

    my $job = $glade->get_widget('combo_launch_job')
	                       ->get_active_text();

    if (! $job) {
	new DlgWarn("Can't use this job");
	return;
    }

    my $client = $glade->get_widget('combo_launch_client')
	                       ->get_active_text();

    if (! $client or $client eq $DlgResto::client_list_empty) {
	new DlgWarn("Can't use this client ($client)");
	return;
    }

    my $prio = $glade->get_widget('spin_launch_priority')->get_value();

    my $replace = $glade->get_widget('chkbp_launch_replace')->get_active();
    $replace=($replace)?'always':'never';    

    my $jobid = $self->{bconsole}->run(job => $job,
				       client  => $client,
				       storage => $storage,
				       fileset => $fileset,
				       $where_cmd => $where,
				       replace => $replace,
				       priority=> $prio,
				       bootstrap => $r);

    $self->show_job($client, $jobid);
}

sub on_combo_storage_button_press_event
{
    my ($self) = @_;
    print "on_combo_storage_button_press_event()\n";
}

sub on_combo_fileset_button_press_event
{
    my ($self) = @_;
    print "on_combo_fileset_button_press_event()\n";

}

sub on_combo_job_button_press_event
{
    my ($self) = @_;
    print "on_combo_job_button_press_event()\n";
}

sub get_volume_inchanger
{
    my ($dbh, $vols) = @_;

    my $lst = join(',', map { $dbh->quote($_) } @{ $vols } ) ;

    my $rq = "SELECT InChanger, VolumeName 
               FROM  Media  
               WHERE VolumeName IN ($lst)
             ";

    my $res = $dbh->selectall_arrayref($rq);
    return $res;		# [ [ 1, VolName].. ]
}


use File::Copy qw/copy/;
use File::Basename qw/basename/; 

# We must kown the path+filename destination
# $self->{error} contains error message
# it return 0/1 if fail/success
sub copy_bsr
{
    my ($self, $src, $dst) = @_ ;
    print "$src => $dst\n"
	if ($debug);

    if (!$dst) {
        return $src;
    }

    my $ret=0 ;
    my $err ; 
    my $dstfile;

    if ($dst =~ m!file:/(/.+)!) {
	$ret = copy($src, $1);
	$err = $!;
	$dstfile = "$1/" . basename($src) ;

    } elsif ($dst =~ m!scp://([^:]+:(.+))!) {
	$err = `scp $src $1 2>&1` ;
	$ret = ($? == 0) ;
	$dstfile = "$2/" . basename($src) ;

    } else {
	$ret = 0;
	$err = "$dst not implemented yet";
	File::Copy::copy($src, \*STDOUT);
    }

    $self->{error} = $err;

    if ($ret == 0) {
	$self->{error} = $err;
	return '';

    } else {
	return $dstfile;
    }
}
1;

################################################################

package DlgAbout;

my $about_widget;

sub display
{
    unless ($about_widget) {
	my $glade_box = Gtk2::GladeXML->new($glade_file, "dlg_about") ;
	$about_widget = $glade_box->get_widget("dlg_about") ;
	$glade_box->signal_autoconnect_from_package('DlgAbout');
    }
    $about_widget->show() ;
}

sub on_about_okbutton_clicked
{
    $about_widget->hide() ;
}

1;

################################################################

package DlgResto;
use base qw/Bbase/;

our $diricon;
our $fileicon;
our $yesicon;
our $noicon;

# Kept as is from the perl-gtk example. Draws the pretty icons
sub render_icons 
{
    my $self = shift;
    unless ($diricon) {
	my $size = 'button';
	$diricon  = $self->{mainwin}->render_icon('gtk-open', $size); 
	$fileicon = $self->{mainwin}->render_icon('gtk-new',  $size);
	$yesicon  = $self->{mainwin}->render_icon('gtk-yes',  $size); 
	$noicon   = $self->{mainwin}->render_icon('gtk-no',   $size);
    }
}
# init combo (and create ListStore object)
sub init_combo
{
    my ($widget, @type) = @_ ;
    my %type_info = ('text' => 'Glib::String',
		     'markup' => 'Glib::String',
		     ) ;
    
    my $lst = new Gtk2::ListStore ( map { $type_info{$_} } @type );

    $widget->set_model($lst);
    my $i=0;
    for my $t (@type) {
	my $cell;
	if ($t eq 'text' or $t eq 'markup') {
	    $cell = new Gtk2::CellRendererText();
	}
	$widget->pack_start($cell, 1);
	$widget->add_attribute($cell, $t, $i++);
    }
    return $lst;
}

# fill simple combo (one element per row)
sub fill_combo
{
    my ($list, @what) = @_;

    $list->clear();
    
    foreach my $w (@what)
    {
	chomp($w);
	my $i = $list->append();
	$list->set($i, 0, $w);
    }
}

# display Mb/Gb/Kb
sub human
{
    my @unit = qw(B KB MB GB TB);
    my $val = shift;
    my $i=0;
    my $format = '%i %s';
    while ($val / 1024 > 1) {
	$i++;
	$val /= 1024;
    }
    $format = ($i>0)?'%0.1f %s':'%i %s';
    return sprintf($format, $val, $unit[$i]);
}

sub get_wanted_job_status
{
    my ($ok_only) = @_;

    if ($ok_only) {
	return "'T'";
    } else {
	return "'T', 'A', 'E'";
    }
}

# This sub gives a full list of the EndTimes for a ClientId
# ( [ 'Date', 'FileSet', 'Type', 'Status', 'JobId'], 
#   ['Date', 'FileSet', 'Type', 'Status', 'JobId']..)
sub get_all_endtimes_for_job
{
    my ($self, $client, $ok_only)=@_;
    my $status = get_wanted_job_status($ok_only);
    my $query = "
 SELECT Job.EndTime, FileSet.FileSet, Job.Level, Job.JobStatus, Job.JobId
  FROM Job,Client,FileSet
  WHERE Job.ClientId=Client.ClientId
  AND Client.Name = '$client'
  AND Job.Type = 'B'
  AND JobStatus IN ($status)
  AND Job.FileSetId = FileSet.FileSetId
  ORDER BY EndTime desc";
    my $result = $self->dbh_selectall_arrayref($query);

    return @$result;
}

sub init_drag_drop
{
    my ($fileview) = shift;
    my $fileview_target_entry = {target => 'STRING',
				 flags => ['GTK_TARGET_SAME_APP'],
				 info => 40 };

    $fileview->enable_model_drag_source(['button1_mask', 'button3_mask'],
					['copy'],$fileview_target_entry);
    $fileview->get_selection->set_mode('multiple');

    # set some useful SimpleList properties    
    $fileview->set_headers_clickable(0);
    foreach ($fileview->get_columns()) 
    {
	$_->set_resizable(1);
	$_->set_sizing('grow-only');
    }
}

sub new
{
    my ($class, $pref) = @_;
    my $self = bless { 
	conf => $pref,
	dirtree => undef,
	CurrentJobIds => [],
	location => undef,	# location entry widget
	mainwin  => undef,	# mainwin widget
	filelist_file_menu => undef, # file menu widget
	filelist_dir_menu => undef,  # dir menu widget
	glade => undef,         # glade object
	status => undef,        # status bar widget
	dlg_pref => undef,      # DlgPref object
	fileattrib => {},	# cache file
	fileview   => undef,    # fileview widget SimpleList
	fileinfo   => undef,    # fileinfo widget SimpleList
	cwd => '/',
	client_combobox => undef, # client_combobox widget
	restore_backup_combobox => undef, # date combobox widget
	list_client => undef,   # Gtk2::ListStore
        list_backup => undef,   # Gtk2::ListStore
	cache_ppathid => {},    #
	bvfs => undef,		# Bfvs object
    };

    $self->{bvfs} = new Bvfs(conf => $pref);

    # load menu (to use handler with self reference)
    my $glade = Gtk2::GladeXML->new($glade_file, "filelist_file_menu");
    $glade->signal_autoconnect_from_package($self);
    $self->{filelist_file_menu} = $glade->get_widget("filelist_file_menu");

    $glade = Gtk2::GladeXML->new($glade_file, "filelist_dir_menu");
    $glade->signal_autoconnect_from_package($self);
    $self->{filelist_dir_menu} = $glade->get_widget("filelist_dir_menu");

    $glade = $self->{glade} = Gtk2::GladeXML->new($glade_file, "dlg_resto");
    $glade->signal_autoconnect_from_package($self);

    $self->{status}  = $glade->get_widget('statusbar');
    $self->{mainwin} = $glade->get_widget('dlg_resto');
    $self->{location} = $glade->get_widget('entry_location');
    $self->render_icons();

    $self->{dlg_pref} = new DlgPref($pref);

    my $c = $self->{client_combobox} = $glade->get_widget('combo_client');    
    $self->{list_client} = init_combo($c, 'text');

    $c = $self->{restore_backup_combobox} = $glade->get_widget('combo_list_backups');
    $self->{list_backup} = init_combo($c, 'text', 'markup');
 
    # Connect glade-fileview to Gtk2::SimpleList
    # and set up drag n drop between $fileview and $restore_list

    # WARNING : we have big dirty thinks with gtk/perl and utf8/iso strings
    # we use an hidden field uuencoded to bypass theses bugs (h_name)

    my $widget = $glade->get_widget('fileview');
    my $fileview = $self->{fileview} = Gtk2::SimpleList->new_from_treeview(
					      $widget,
		                              'h_pathid'      => 'hidden',
		                              'h_filenameid'  => 'hidden',
					      'h_name'        => 'hidden',
					      'h_jobid'       => 'hidden',
					      'h_type'        => 'hidden',

					      ''              => 'pixbuf',
				              'File Name'     => 'text',
					      'Size'          => 'text',
					      'Date'          => 'text');
    init_drag_drop($fileview);
    $fileview->set_search_column(6); # search on File Name

    # Connect glade-restore_list to Gtk2::SimpleList
    $widget = $glade->get_widget('restorelist');
    my $restore_list = $self->{restore_list} = Gtk2::SimpleList->new_from_treeview(
                                              $widget,
		                              'h_pathid'      => 'hidden', #0
		                              'h_filenameid'  => 'hidden',
					      'h_name'      => 'hidden',
                                              'h_jobid'     => 'hidden',
					      'h_type'      => 'hidden',
                                              'h_curjobid'  => 'hidden', #5

                                              ''            => 'pixbuf',
                                              'File Name'   => 'text',
                                              'JobId'       => 'text',
                                              'FileIndex'   => 'text',

					      'Nb Files'    => 'text', #10
                                              'Size'        => 'text', #11
					      'size_b'      => 'hidden', #12
					      );

    my @restore_list_target_table = ({'target' => 'STRING',
				      'flags' => [], 
				      'info' => 40 });	

    $restore_list->enable_model_drag_dest(['copy'],@restore_list_target_table);
    $restore_list->get_selection->set_mode('multiple');
    
    $widget = $glade->get_widget('infoview');
    my $infoview = $self->{fileinfo} = Gtk2::SimpleList->new_from_treeview(
		   $widget,
                   'h_pathid'      => 'hidden',
		   'h_filenameid'  => 'hidden',
                   'h_name'        => 'hidden',
                   'h_jobid'       => 'hidden',
		   'h_type'        => 'hidden',

		   'InChanger'     => 'pixbuf',
		   'Volume'        => 'text',
		   'JobId'         => 'text',
		   'Size'          => 'text',
		   'Date'          => 'text',
		   'MD5'           => 'text');

    init_drag_drop($infoview);

    $pref->connect_db() ||  $self->{dlg_pref}->display($self);

    if ($pref->{dbh}) {
	$self->init_server_backup_combobox();
	if ($self->{bvfs}->create_brestore_tables()) {
	    new DlgWarn("brestore can't find brestore_xxx tables on your database. I will create them.");
	}
    }

    $self->set_status($pref->{error});
}

# set status bar informations
sub set_status
{
    my ($self, $string) = @_;
    return unless ($string);

    my $context = $self->{status}->get_context_id('Main');
    $self->{status}->push($context, $string);
}

sub on_time_select_changed
{
    my ($self) = @_;
}

sub get_active_time
{
    my ($self) = @_;
    my $c = $self->{glade}->get_widget('combo_time');
    return $c->get_active_text;
}

# This sub returns all clients declared in DB
sub get_all_clients
{
    my $dbh = shift;
    my $query = "SELECT Name FROM Client ORDER BY Name";
    print STDERR $query,"\n" if $debug;

    my $result = $dbh->selectall_arrayref($query);

    return map { $_->[0] } @$result;
}

# init infoview widget
sub clear_infoview
{
    my $self = shift;
    @{$self->{fileinfo}->{data}} = ();
}

# init restore_list
sub on_clear_clicked
{
    my $self = shift;
    @{$self->{restore_list}->{data}} = ();
}

sub on_estimate_clicked
{
    my ($self) = @_;

    my $size_total=0;
    my $nb_total=0;

    # TODO : If we get here, things could get lenghty ... draw a popup window .
    my $widget = Gtk2::MessageDialog->new($self->{mainwin}, 
					  'destroy-with-parent', 
					  'info', 'close', 
					  'Computing size...');
    $widget->show;
    refresh_screen();

    my $title = "Computing size...\n";
    my $txt="";

    # ($pid,$fid,$name,$jobid,'file',$curjobids,
    #   undef, undef, undef, $dirfileindex);
    foreach my $entry (@{$self->{restore_list}->{data}})
    {
	unless ($entry->[11]) {
	    my ($size, $nb) = $self->{bvfs}->estimate_restore_size($entry,\&refresh_screen);
	    $entry->[12] = $size;
	    $entry->[11] = human($size);
	    $entry->[10] = $nb;
	}

	my $name = unpack('u', $entry->[2]);

	$txt .= "\n<i>$name</i> : ". $entry->[10] ." file(s)/". $entry->[11] ;
	$self->debug($title . $txt);
	$widget->set_markup($title . $txt);
	
	$size_total+=$entry->[12];
	$nb_total+=$entry->[10];
	refresh_screen();
    }
    
    $txt .= "\n\n<b>Total</b> : $nb_total file(s)/" . human($size_total);
    $widget->set_markup("Size estimation :\n" . $txt);
    $widget->signal_connect ("response", sub { my $w=shift; $w->destroy();});

    return 0;
}



sub on_gen_bsr_clicked
{
    my ($self) = @_;
    
    my @options = ("Choose a bsr file", $self->{mainwin}, 'save', 
		   'gtk-save','ok', 'gtk-cancel', 'cancel');

    
    my $w = new Gtk2::FileChooserDialog ( @options );
    my $ok = 0;
    my $save;
    while (!$ok) {
	my $a = $w->run();
	if ($a eq 'cancel') {
	    $ok = 1;
	}

	if ($a eq 'ok') {
	    my $f = $w->get_filename();
	    if (-f $f) {
		my $dlg = Gtk2::MessageDialog->new($self->{mainwin}, 
						   'destroy-with-parent', 
						   'warning', 'ok-cancel', 'This file already exists, do you want to overwrite it ?');
		if ($dlg->run() eq 'ok') {
		    $save = $f;
		}
		$dlg->destroy();
	    } else {
		$save = $f;
	    }
	    $ok = 1;
	}
    }

    $w->destroy();
    
    if ($save) {
	if (open(FP, ">$save")) {
	    my $bsr = $self->create_filelist();
	    print FP $bsr;
	    close(FP);
	    $self->set_status("Dumping BSR to $save ok");
	} else {
	    $self->set_status("Can't dump BSR to $save: $!");
	}
    }
}


use File::Temp qw/tempfile/;

sub on_go_button_clicked 
{
    my $self = shift;
    unless (scalar(@{$self->{restore_list}->{data}})) {
	new DlgWarn("No file to restore");
	return 0;
    }
    my $bsr = $self->create_filelist();
    my ($fh, $filename) = tempfile();
    $fh->print($bsr);
    close($fh);
    chmod(0644, $filename);

    print "Dumping BSR info to $filename\n"
	if ($debug);

    # we get Volume list
    my %a = map { $_ => 1 } ($bsr =~ /Volume="(.+)"/g);
    my $vol = [ keys %a ] ;	# need only one occurrence of each volume

    new DlgLaunch(pref     => $self->{conf},
		  volumes  => $vol,
		  bsr_file => $filename,
		  );

}


our $client_list_empty = 'Clients list'; 
our %type_markup = ('F' => '<b>$label F</b>',
		    'D' => '$label D',
		    'I' => '$label I',
		    'B' => '<b>$label B</b>',

		    'A' => '<span foreground=\"red\">$label</span>',
		    'T' => '$label',
		    'E' => '<span foreground=\"red\">$label</span>',
		    );

sub on_list_client_changed 
{
    my ($self, $widget) = @_;
    return 0 unless defined $self->{fileview};

    $self->{list_backup}->clear();

    if ($self->current_client eq $client_list_empty) {
	return 0 ;
    }

    $self->{CurrentJobIds} = [
			      set_job_ids_for_date($self->dbh(),
						   $self->current_client,
						   $self->current_date,
						   $self->{conf}->{use_ok_bkp_only})
			      ];

    my $fs = $self->{bvfs};
    $fs->set_curjobids(@{$self->{CurrentJobIds}});
    $fs->ch_dir($fs->get_root());
    # refresh_fileview will be done by list_backup_changed


    my @endtimes=$self->get_all_endtimes_for_job($self->current_client,
						 $self->{conf}->{use_ok_bkp_only});

    foreach my $endtime (@endtimes)
    {
	my $i = $self->{list_backup}->append();

	my $label = $endtime->[1] . " (" . $endtime->[4] . ")";
	eval "\$label = \"$type_markup{$endtime->[2]}\""; # job type
	eval "\$label = \"$type_markup{$endtime->[3]}\""; # job status

	$self->{list_backup}->set($i, 
				  0, $endtime->[0],
				  1, $label,
				  );
    }
    $self->{restore_backup_combobox}->set_active(0);
    0;
}


sub fill_server_list
{
    my ($dbh, $combo, $list) = @_;

    my @clients=get_all_clients($dbh);

    $list->clear();
    
    my $i = $list->append();
    $list->set($i, 0, $client_list_empty);
    
    foreach my $client (@clients)
    {
	$i = $list->append();
	$list->set($i, 0, $client);
    }
    $combo->set_active(0);
}

sub init_server_backup_combobox
{
    my $self = shift ;
    fill_server_list($self->{conf}->{dbh}, 
		     $self->{client_combobox},
		     $self->{list_client}) ;
}

#----------------------------------------------------------------------
#Refreshes the file-view Redraws everything. The dir data is cached, the file
#data isn't.  There is additionnal complexity for dirs (visibility problems),
#so the @CurrentJobIds is not sufficient.
sub refresh_fileview 
{
    my ($self) = @_;
    my $fileview = $self->{fileview};
    my $client_combobox = $self->{client_combobox};
    my $bvfs = $self->{bvfs};

    @{$fileview->{data}} = ();

    $self->clear_infoview();
    
    my $client_name = $self->current_client;

    if (!$client_name or ($client_name eq $client_list_empty)) {
	$self->set_status("Client list empty");
	return;
    }

    # [ [dirid, dir_basename, File.LStat, jobid]..]
    my $list_dirs = $bvfs->ls_dirs();
    # [ [filenameid, listfiles.id, listfiles.Name, File.LStat, File.JobId]..]
    my $files     = $bvfs->ls_files(); 
    
    my $file_count = 0 ;
    my $total_bytes = 0;
    
    # Add directories to view
    foreach my $dir_entry (@$list_dirs) {
	my $time = localtime(Bvfs::lstat_attrib($dir_entry->[2],'st_mtime'));
	$total_bytes += 4096;
	$file_count++;

	listview_push($fileview,
		      $dir_entry->[0],
		      0,
		      $dir_entry->[1],
		      # TODO: voir ce que l'on met la
		      $dir_entry->[3],
		      'dir',

		      $diricon, 
		      $dir_entry->[1], 
		      "4 Kb", 
		      $time);
    }
    
    # Add files to view 
    foreach my $file (@$files) 
    {
	my $size = Bvfs::file_attrib($file,'st_size');
	my $time = localtime(Bvfs::file_attrib($file,'st_mtime'));
	$total_bytes += $size;
	$file_count++;
	# $file = [filenameid,listfiles.id,listfiles.Name,File.LStat,File.JobId]
	listview_push($fileview,
		      $bvfs->{cwdid},
		      $file->[0],
		      $file->[2],
		      $file->[4],
		      'file',
		      
		      $fileicon, 
		      $file->[2], 
		      human($size), $time);
    }
    
    $self->set_status("$file_count files/" . human($total_bytes));
    $self->{cwd} = $self->{bvfs}->pwd();
    $self->{location}->set_text($self->{cwd});
    # set a decent default selection (makes keyboard nav easy)
    $fileview->select(0);
}


sub on_about_activate
{
    DlgAbout::display();
}

sub drag_set_info
{
    my ($tree, $path, $data) = @_;

    my @items = listview_get_all($tree) ;
    my @ret;
    foreach my $i (@items)
    {
	my @file_info = @{$i};

	# doc ligne 93
        # Ok, we have a corner case :
        # path can be empty
	my $file = pack("u", $path . $file_info[2]);

	push @ret, join(" ; ", $file,
			$file_info[0], # $pathid
			$file_info[1], # $filenameid
			$file_info[3], # $jobid
			$file_info[4], # $type
			);
    }

    my $data_get = join(" :: ", @ret);
    
    $data->set_text($data_get,-1);
}



sub fileview_data_get
{
    my ($self, $widget, $context, $data, $info, $time,$string) = @_;
    drag_set_info($widget, $self->{cwd}, $data);
}

sub fileinfo_data_get
{
    my ($self, $widget, $context, $data, $info, $time,$string) = @_;
    drag_set_info($widget, $self->{cwd}, $data);
}

sub restore_list_data_received
{
    my ($self, $widget, $context, $x, $y, $data, $info, $time) = @_;
    my @ret;
    $self->debug("start\n");
    if  ($info eq 40 || $info eq 0) # patch for display!=:0
    {
	foreach my $elt (split(/ :: /, $data->data()))
	{
	    my ($file, $pathid, $filenameid, $jobid, $type) = 
		split(/ ; /, $elt);
	    $file = unpack("u", $file);
	    
	    $self->add_selected_file_to_list($pathid,$filenameid,
					     $file, $jobid, $type);
	}
    }
    $self->debug("end\n");
}

sub on_back_button_clicked {
    my $self = shift;
    $self->{bvfs}->up_dir();
    $self->refresh_fileview();
}
sub on_location_go_button_clicked 
{
    my $self = shift; 
    $self->ch_dir($self->{location}->get_text());
}
sub on_quit_activate {Gtk2->main_quit;}
sub on_preferences_activate
{
    my $self = shift; 
    $self->{dlg_pref}->display($self) ;
}
sub on_main_delete_event {Gtk2->main_quit;}
sub on_bweb_activate
{
    my $self = shift; 
    $self->set_status("Open bweb on your browser");
    $self->{conf}->go_bweb('', "go on bweb");
}

# Change the current working directory
#   * Updates fileview, location, and selection
#
sub ch_dir 
{
    my ($self,$l) = @_;

    my $p = $self->{bvfs}->get_pathid($l);
    if ($p) {
	$self->{bvfs}->ch_dir($p);
	$self->refresh_fileview();
    } else {
	$self->set_status("Can't find $l");
    }
    1;
}

# Handle dialog 'close' (window-decoration induced close)
#   * Just hide the dialog, and tell Gtk not to do anything else
#
sub on_delete_event 
{
    my ($self, $w) = @_;
    $w->hide; 
    Gtk2::main_quit();
    1; # consume this event!
}

# Handle key presses in location text edit control
#   * Translate a Return/Enter key into a 'Go' command
#   * All other key presses left for GTK
#
sub on_location_entry_key_release_event 
{
    my $self = shift;
    my $widget = shift;
    my $event = shift;
    
    my $keypress = $event->keyval;
    if ($keypress == $Gtk2::Gdk::Keysyms{KP_Enter} ||
	$keypress == $Gtk2::Gdk::Keysyms{Return}) 
    {
	$self->ch_dir($widget->get_text());
	
	return 1; # consume keypress
    }

    return 0; # let gtk have the keypress
}

sub on_fileview_key_press_event
{
    my ($self, $widget, $event) = @_;
    return 0;
}

sub listview_get_first
{
    my ($list) = shift; 
    my @selected = $list->get_selected_indices();
    if (@selected > 0) {
	my ($pid,$fid,$name, @other) = @{$list->{data}->[$selected[0]]};
	return ($pid,$fid,unpack('u', $name), @other);
    } else {
	return undef;
    }
}

sub listview_get_all
{
    my ($list) = shift; 

    my @selected = $list->get_selected_indices();
    my @ret;
    for my $i (@selected) {
	my ($pid,$fid,$name, @other) = @{$list->{data}->[$i]};
	push @ret, [$pid,$fid,unpack('u', $name), @other];
    } 
    return @ret;
}

sub listview_push
{
    my ($list, $pid, $fid, $name, @other) = @_;
    push @{$list->{data}}, [$pid,$fid,pack('u', $name), @other];
}

#-----------------------------------------------------------------
# Handle keypress in file-view
#   * Translates backspace into a 'cd ..' command 
#   * All other key presses left for GTK
#
sub on_fileview_key_release_event 
{
    my ($self, $widget, $event) = @_;
    if (not $event->keyval)
    {
	return 0;
    }
    if ($event->keyval == $Gtk2::Gdk::Keysyms{BackSpace}) {
	$self->on_back_button_clicked();
	return 1; # eat keypress
    }

    return 0; # let gtk have keypress
}

sub on_forward_keypress
{
    return 0;
}

#-------------------------------------------------------------------
# Handle double-click (or enter) on file-view
#   * Translates into a 'cd <dir>' command
#
sub on_fileview_row_activated 
{
    my ($self, $widget) = @_;
    
    my ($pid,$fid,$name, undef, $type, undef) = listview_get_first($widget);

    if ($type eq 'dir')
    {
	$self->{bvfs}->ch_dir($pid);
	$self->refresh_fileview();
    } else {
	$self->fill_infoview($pid,$fid,$name);
    }
    
    return 1; # consume event
}

sub fill_infoview
{
    my ($self, $path, $file, $fn) = @_;
    $self->clear_infoview();
    my @v = $self->{bvfs}->get_all_file_versions($path, 
						 $file,
						 $self->current_client,
						 $self->{conf}->{see_all_versions});
    for my $ver (@v) {
	my (undef,$pid,$fid,$jobid,$fileindex,$mtime,
	    $size,$inchanger,$md5,$volname) = @{$ver};
	my $icon = ($inchanger)?$yesicon:$noicon;

	$mtime = localtime($mtime) ;

	listview_push($self->{fileinfo},$pid,$fid,
		      $fn, $jobid, 'file', 
		      $icon, $volname, $jobid, human($size), $mtime, $md5);
    }
}

sub current_date
{
    my $self = shift ;
    return $self->{restore_backup_combobox}->get_active_text;
}

sub current_client
{
    my $self = shift ;
    return $self->{client_combobox}->get_active_text;
}

sub on_list_backups_changed 
{
    my ($self, $widget) = @_;
    return 0 unless defined $self->{fileview};

    $self->{CurrentJobIds} = [
			      set_job_ids_for_date($self->dbh(),
						   $self->current_client,
						   $self->current_date,
						   $self->{conf}->{use_ok_bkp_only})
			      ];
    $self->{bvfs}->set_curjobids(@{$self->{CurrentJobIds}});
    $self->refresh_fileview();
    0;
}

sub on_restore_list_keypress
{
    my ($self, $widget, $event) = @_;
    if ($event->keyval == $Gtk2::Gdk::Keysyms{Delete})
    {
	my @sel = $widget->get_selected_indices;
	foreach my $elt (reverse(sort {$a <=> $b} @sel))
	{
	    splice @{$self->{restore_list}->{data}},$elt,1;
	}
    }
}

sub on_fileview_button_press_event
{
    my ($self,$widget,$event) = @_;
    if ($event->button == 3)
    {
	$self->on_right_click_filelist($widget,$event);
	return 1;
    }
    
    if ($event->button == 2)
    {
	$self->on_see_all_version();
	return 1;
    }

    return 0;
}

sub on_see_all_version
{
    my ($self) = @_;
    
    my @lst = listview_get_all($self->{fileview});

    for my $i (@lst) {
	my ($pid,$fid,$name, undef) = @{$i};

	new DlgFileVersion($self->{bvfs}, 
			   $self->current_client, 
			   $pid,$fid,$self->{cwd},$name);
    }
}

sub on_right_click_filelist
{
    my ($self,$widget,$event) = @_;
    # I need to know what's selected
    my @sel = listview_get_all($self->{fileview});
    
    my $type = '';

    if (@sel == 1) {
	$type = $sel[0]->[4];	# $type
    }

    my $w;

    if (@sel >=2 or $type eq 'dir')
    {
	# We have selected more than one or it is a directories
	$w = $self->{filelist_dir_menu};
    }
    else
    {
	$w = $self->{filelist_file_menu};
    }
    $w->popup(undef,
	      undef,
	      undef,
	      undef,
	      $event->button, $event->time);
}

sub context_add_to_filelist
{
    my ($self) = @_;

    my @sel = listview_get_all($self->{fileview});

    foreach my $i (@sel)
    {
	my ($pid, $fid, $file, $jobid, $type, undef) = @{$i};
	$file = $self->{cwd} . $file;
	$self->add_selected_file_to_list($pid, $fid, $file, $jobid, $type);
    }
}

# Adds a file to the filelist
sub add_selected_file_to_list
{
    my ($self, $pid, $fid, $name, $jobid, $type)=@_;

    my $restore_list = $self->{restore_list};

    my $curjobids=join(',', @{$self->{CurrentJobIds}});

    if ($type eq 'dir')
    {
	# dirty hack
	$name =~ s!^//+!/!;

	if ($name and substr $name,-1 ne '/')
	{
		$name .= '/'; # For bacula
	}
	my $dirfileindex = $self->{bvfs}->get_fileindex_from_dir_jobid($pid,$jobid);
	listview_push($restore_list,$pid,0, 
		      $name, $jobid, 'dir', $curjobids,
		      $diricon, $name,$curjobids,$dirfileindex);
    }
    elsif ($type eq 'file')
    {
	my $fileindex = $self->{bvfs}->get_fileindex_from_file_jobid($pid,$fid,$jobid);

	listview_push($restore_list,$pid,$fid,
		      $name, $jobid, 'file', $curjobids,
		      $fileicon, $name, $jobid, $fileindex );
    }
}

# TODO : we want be able to restore files from a bad ended backup
# we have JobStatus IN ('T', 'A', 'E') and we must 

# Data acces subs from here. Interaction with SGBD and caching

# This sub retrieves the list of jobs corresponding to the jobs selected in the
# GUI and stores them in @CurrentJobIds
sub set_job_ids_for_date
{
    my ($dbh, $client, $date, $only_ok)=@_;

    if (!$client or !$date) {
	return ();
    }
    
    my $status = get_wanted_job_status($only_ok);
	
    # The algorithm : for a client, we get all the backups for each
    # fileset, in reverse order Then, for each fileset, we store the 'good'
    # incrementals and differentials until we have found a full so it goes
    # like this : store all incrementals until we have found a differential
    # or a full, then find the full #

    my $query = "SELECT JobId, FileSet, Level, JobStatus
		FROM Job, Client, FileSet
		WHERE Job.ClientId = Client.ClientId
		AND FileSet.FileSetId = Job.FileSetId
		AND EndTime <= '$date'
		AND Client.Name = '$client'
		AND Type IN ('B')
		AND JobStatus IN ($status)
		ORDER BY FileSet, JobTDate DESC";
	
    my @CurrentJobIds;
    my $result = $dbh->selectall_arrayref($query);
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

sub refresh_screen
{
    Gtk2->main_iteration while (Gtk2->events_pending);
}

# TODO : bsr must use only good backup or not (see use_ok_bkp_only)
# This sub creates a BSR from the information in the restore_list
# Returns the BSR as a string
sub create_filelist
{
    	my $self = shift;
	my %mediainfos;
	# This query gets all jobid/jobmedia/media combination.
	my $query = "
SELECT Job.JobId, Job.VolsessionId, Job.VolsessionTime, JobMedia.StartFile, 
       JobMedia.EndFile, JobMedia.FirstIndex, JobMedia.LastIndex,
       JobMedia.StartBlock, JobMedia.EndBlock, JobMedia.VolIndex, 
       Media.Volumename, Media.MediaType
FROM Job, JobMedia, Media
WHERE Job.JobId = JobMedia.JobId
  AND JobMedia.MediaId = Media.MediaId
  ORDER BY JobMedia.FirstIndex, JobMedia.LastIndex";
	

	my $result = $self->dbh_selectall_arrayref($query);

	# We will store everything hashed by jobid.

	foreach my $refrow (@$result)
	{
		my ($jobid, $volsessionid, $volsessiontime, $startfile, $endfile,
		$firstindex, $lastindex, $startblock, $endblock,
		$volindex, $volumename, $mediatype) = @{$refrow};

                # We just have to deal with the case where starfile != endfile
                # In this case, we concatenate both, for the bsr
                if ($startfile != $endfile) { 
		      $startfile = $startfile . '-' . $endfile;
		}

		my @tmparray = 
		($jobid, $volsessionid, $volsessiontime, $startfile, 
		$firstindex, $lastindex, $startblock .'-'. $endblock,
		$volindex, $volumename, $mediatype);
		
		push @{$mediainfos{$refrow->[0]}},(\@tmparray);
	}

	
	# reminder : restore_list looks like this : 
	# ($pid,$fid,$name,$jobid,'file',$curjobids,
	#   undef, undef, undef, $dirfileindex);
	
	# Here, we retrieve every file/dir that could be in the restore
	# We do as simple as possible for the SQL engine (no crazy joins,
	# no pseudo join (>= FirstIndex ...), etc ...
	# We do a SQL union of all the files/dirs specified in the restore_list
	my @select_queries;
	foreach my $entry (@{$self->{restore_list}->{data}})
	{
		if ($entry->[4] eq 'dir')
		{
			my $dirid = $entry->[0];
			my $inclause = $entry->[5]; #curjobids
	
			my $query = 
"(SELECT Path.Path, Filename.Name, File.FileIndex, File.JobId
  FROM File, Path, Filename
  WHERE Path.PathId = File.PathId
  AND File.FilenameId = Filename.FilenameId
  AND Path.Path LIKE 
        (SELECT ". $self->dbh_strcat('Path',"'\%'") ." FROM Path 
          WHERE PathId IN ($dirid)
        )
  AND File.JobId IN ($inclause) )";
			push @select_queries,($query);
		}
		else
		{
			# It's a file. Great, we allready have most 
			# of what is needed. Simple and efficient query
			my $dir = $entry->[0];
			my $file = $entry->[1];
			
			my $jobid = $entry->[3];
			my $fileindex = $entry->[9];
			my $inclause = $entry->[5]; # curjobids
			my $query = 
"(SELECT Path.Path, Filename.Name, File.FileIndex, File.JobId
  FROM File,Path,Filename
  WHERE File.PathId = $dir
  AND File.PathId = Path.PathId
  AND File.FilenameId = $file
  AND File.FilenameId = Filename.FilenameId
  AND File.JobId = $jobid
 )
";
			push @select_queries,($query);
		}
	}
	$query = join("\nUNION ALL\n",@select_queries) . "\nORDER BY FileIndex\n";

	#Now we run the query and parse the result...
	# there may be a lot of records, so we better be efficient
	# We use the bind column method, working with references...

	my $sth = $self->dbh_prepare($query);
	$sth->execute;

	my ($path,$name,$fileindex,$jobid);
	$sth->bind_columns(\$path,\$name,\$fileindex,\$jobid);
	
	# The temp place we're going to save all file
	# list to before the real list
	my @temp_list;

	RECORD_LOOP:
	while ($sth->fetchrow_arrayref())
	{
		# This may look dumb, but we're going to do a join by ourselves,
		# to save memory and avoid sending a complex query to mysql
		my $complete_path = $path . $name;
		my $is_dir = 0;
		
		if ( $name eq '')
		{
			$is_dir = 1;
		}
		
		# Remove trailing slash (normalize file and dir name)
		$complete_path =~ s/\/$//;
		
		# Let's find the ref(s) for the %mediainfo element(s) 
		# containing the data for this file
		# There can be several matches. It is the pseudo join.
		my $med_idx=0;
		my $max_elt=@{$mediainfos{$jobid}}-1;
		MEDIA_LOOP:
		while($med_idx <= $max_elt)
		{
			my $ref = $mediainfos{$jobid}->[$med_idx];
			# First, can we get rid of the first elements of the
			# array ? (if they don't contain valuable records
			# anymore
			if ($fileindex > $ref->[5])
			{
				# It seems we don't need anymore
				# this entry in %mediainfo (the input data
				# is sorted...)
				# We get rid of it.
				shift @{$mediainfos{$jobid}};
				$max_elt--;
				next MEDIA_LOOP;
			}
			# We will do work on this elt. We can ++
			# $med_idx for next loop
			$med_idx++;

			# %mediainfo row looks like : 
			# (jobid,VolsessionId,VolsessionTime,File,FirstIndex,
			# LastIndex,StartBlock-EndBlock,VolIndex,Volumename,
			# MediaType)
			
			# We are in range. We store and continue looping
			# in the medias
			if ($fileindex >= $ref->[4])
			{
				my @data = ($complete_path,$is_dir,
					    $fileindex,$ref);
				push @temp_list,(\@data);
				next MEDIA_LOOP;
			}
			
			# We are not in range. No point in continuing looping
			# We go to next record.
			next RECORD_LOOP;
		}
	}
	# Now we have the array.
	# We're going to sort it, by 
	# path, volsessiontime DESC (get the most recent file...)
	# The array rows look like this :
	# complete_path,is_dir,fileindex,
	# ref->(jobid,VolsessionId,VolsessionTime,File,FirstIndex,
	#       LastIndex,StartBlock-EndBlock,VolIndex,Volumename,MediaType)
	@temp_list = sort {$a->[0] cmp $b->[0]
                        || $b->[3]->[2] <=> $a->[3]->[2]
                          } @temp_list;

	my @restore_list;
	my $prev_complete_path='////'; # Sure not to match
	my $prev_is_file=1;
	my $prev_jobid;

	while (my $refrow = shift @temp_list)
	{
		# For the sake of readability, we load $refrow 
		# contents in real scalars
		my ($complete_path, $is_dir, $fileindex, $refother)=@{$refrow};
		my $jobid= $refother->[0]; # We don't need the rest...

		# We skip this entry.
		# We allready have a newer one and this 
		# isn't a continuation of the same file
		next if ($complete_path eq $prev_complete_path 
		         and $jobid != $prev_jobid);
		
		
		if ($prev_is_file 
		    and $complete_path =~ m|^\Q$prev_complete_path\E/|)
		{
			# We would be recursing inside a file.
			# Just what we don't want (dir replaced by file
			# between two backups
			next;
		}
		elsif ($is_dir)
		{
			# It is a directory
			push @restore_list,($refrow);
			
			$prev_complete_path = $complete_path;
			$prev_jobid = $jobid;
			$prev_is_file = 0;
		}
		else
		{
			# It is a file
			push @restore_list,($refrow);
			
			$prev_complete_path = $complete_path;
			$prev_jobid = $jobid;
			$prev_is_file = 1;
		}
	}
	# We get rid of @temp_list... save memory
	@temp_list=();

	# Ok everything is in the list. Let's sort it again in another way.
	# This time it will be in the bsr file order

	# we sort the results by 
	# volsessiontime, volsessionid, volindex, fileindex 
	# to get all files in right order...
	# Reminder : The array rows look like this :
	# complete_path,is_dir,fileindex,
	# ref->(jobid,VolsessionId,VolsessionTime,File,FirstIndex,LastIndex,
	#       StartBlock-EndBlock,VolIndex,Volumename,MediaType)

	@restore_list= sort { $a->[3]->[2] <=> $b->[3]->[2] 
	                   || $a->[3]->[1] <=> $b->[3]->[1] 
	                   || $a->[3]->[7] <=> $b->[3]->[7] 
	                   || $a->[2] <=> $b->[2] } 
	                   	@restore_list;

	# Now that everything is ready, we create the bsr
	my $prev_fileindex=-1;
	my $prev_volsessionid=-1;
	my $prev_volsessiontime=-1;
	my $prev_volumename=-1;
	my $prev_volfile=-1;
	my $prev_mediatype;
	my $prev_volblocks;
	my $count=0;
	my $first_of_current_range=0;
	my @fileindex_ranges;
	my $bsr='';

	foreach my $refrow (@restore_list)
	{
		my (undef,undef,$fileindex,$refother)=@{$refrow};
		my (undef,$volsessionid,$volsessiontime,$volfile,undef,undef,
		    $volblocks,undef,$volumename,$mediatype)=@{$refother};
		
		# We can specifiy the number of files in each section of the
		# bsr to speedup restore (bacula can then jump over the
		# end of tape files.
		$count++;
		
		
		if ($prev_volumename eq '-1')
		{
			# We only have to start the new range...
			$first_of_current_range=$fileindex;
		}
		elsif ($prev_volsessionid != $volsessionid 
		       or $prev_volsessiontime != $volsessiontime 
		       or $prev_volumename ne $volumename 
		       or $prev_volfile ne $volfile)
		{
			# We have to create a new section in the bsr...
			#Â We print the previous one ... 
			# (before that, save the current range ...)
			if ($first_of_current_range != $prev_fileindex)
			{
			 	#Â we are in a range
				push @fileindex_ranges,
				    ("$first_of_current_range-$prev_fileindex");
			}
			else
			{
				 # We are out of a range,
				 # but there is only one element in the range
				push @fileindex_ranges,
				    ("$first_of_current_range");
			}
			
			$bsr.=print_bsr_section(\@fileindex_ranges,
			                        $prev_volsessionid,
			                        $prev_volsessiontime,
			                        $prev_volumename,
			                        $prev_volfile,
			                        $prev_mediatype,
			                        $prev_volblocks,
			                        $count-1);
			$count=1;
			# Reset for next loop
			@fileindex_ranges=();
			$first_of_current_range=$fileindex;
		}
		elsif ($fileindex-1 != $prev_fileindex)
		{
			# End of a range of fileindexes
			if ($first_of_current_range != $prev_fileindex)
			{
				#we are in a range
				push @fileindex_ranges,
				    ("$first_of_current_range-$prev_fileindex");
			}
			else
			{
				 # We are out of a range,
				 # but there is only one element in the range
				push @fileindex_ranges,
				    ("$first_of_current_range");
			}
			$first_of_current_range=$fileindex;
		}
		$prev_fileindex=$fileindex;
		$prev_volsessionid = $volsessionid;
		$prev_volsessiontime = $volsessiontime;
		$prev_volumename = $volumename;
		$prev_volfile=$volfile;
		$prev_mediatype=$mediatype;
		$prev_volblocks=$volblocks;

	}

	# Ok, we're out of the loop. Alas, there's still the last record ...
	if ($first_of_current_range != $prev_fileindex)
	{
		# we are in a range
		push @fileindex_ranges,("$first_of_current_range-$prev_fileindex");
		
	}
	else
	{
		# We are out of a range,
		# but there is only one element in the range
		push @fileindex_ranges,("$first_of_current_range");
		
	}
	$bsr.=print_bsr_section(\@fileindex_ranges,
				$prev_volsessionid,
				$prev_volsessiontime,
				$prev_volumename,
				$prev_volfile,
				$prev_mediatype,
				$prev_volblocks,
				$count);
	
	return $bsr;
}

sub print_bsr_section
{
    my ($ref_fileindex_ranges,$volsessionid,
	$volsessiontime,$volumename,$volfile,
	$mediatype,$volblocks,$count)=@_;
    
    my $bsr='';
    $bsr .= "Volume=\"$volumename\"\n";
    $bsr .= "MediaType=\"$mediatype\"\n";
    $bsr .= "VolSessionId=$volsessionid\n";
    $bsr .= "VolSessionTime=$volsessiontime\n";
    $bsr .= "VolFile=$volfile\n";
    $bsr .= "VolBlock=$volblocks\n";
    
    foreach my $range (@{$ref_fileindex_ranges})
    {
	$bsr .= "FileIndex=$range\n";
    }
    
    $bsr .= "Count=$count\n";
    return $bsr;
}

1;

################################################################

package Bvfs;
use base qw/Bbase/;

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


sub update_cache
{
    my ($self) = @_;

    $self->{conf}->{dbh}->begin_work();

    my $query = "
  SELECT JobId from Job 
   WHERE JobId NOT IN (SELECT JobId FROM brestore_knownjobid) AND JobStatus IN ('T', 'f', 'A') ORDER BY JobId";
    my $jobs = $self->dbh_selectall_arrayref($query);

    $self->update_brestore_table(map { $_->[0] } @$jobs);

    $self->{conf}->{dbh}->commit();
    $self->{conf}->{dbh}->begin_work();

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

    $self->{conf}->{dbh}->commit();
}

sub get_root
{
    my ($self, $dir) = @_;
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
    $self->update_brestore_table(@jobids);
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

# Returns the list of media required for a list of jobids.
# Input : self, jobid1, jobid2...
# Output : reference to array of (joibd, inchanger)
sub get_required_media_from_jobid
{
    my ($self, @jobids)=@_;
    my $inclause = join(',',@jobids);
    my $query = "
SELECT DISTINCT JobMedia.MediaId, Media.InChanger 
FROM JobMedia, Media 
WHERE JobMedia.MediaId=Media.MediaId 
AND JobId In ($inclause)
ORDER BY MediaId";
    my $result = $self->dbh_selectall_arrayref($query);
    return $result;
}

# Returns the fileindex from dirname and jobid.
# Input : self, dirid, jobid
# Output : fileindex
sub get_fileindex_from_dir_jobid
{
    my ($self, $dirid, $jobid)=@_;
    my $query;
    $query = "SELECT File.FileIndex
		FROM File, Filename
		WHERE File.FilenameId = Filename.FilenameId
		AND File.PathId = $dirid
		AND Filename.Name = ''
		AND File.JobId = '$jobid'
		";
		
    $self->debug($query);
    my $result = $self->dbh_selectall_arrayref($query);
    return $result->[0]->[0];
}

# Returns the fileindex from filename and jobid.
# Input : self, dirid, filenameid, jobid
# Output : fileindex
sub get_fileindex_from_file_jobid
{
    my ($self, $dirid, $filenameid, $jobid)=@_;
    
    my $query;
    $query = 
"SELECT File.FileIndex
 FROM File
 WHERE File.PathId = $dirid
   AND File.FilenameId = $filenameid
   AND File.JobId = $jobid";
		
    $self->debug($query);
    my $result = $self->dbh_selectall_arrayref($query);
    return $result->[0]->[0];
}

# This function estimates the size to be restored for an entry of the restore
# list
# In : self,reference to the entry
# Out : size in bytes, number of files
sub estimate_restore_size
{
    # reminder : restore_list looks like this : 
    # ($pid,$fid,$name,$jobid,'file',$curjobids, 
    #  undef, undef, undef, $dirfileindex);
    my ($self, $entry, $refresh) = @_;
    my $query;
    if ($entry->[4] eq 'dir')
    {
	my $dir = $entry->[0];

	my $inclause = $entry->[5]; #curjobids
	$query = 
"SELECT Path.Path, File.FilenameId, File.LStat
  FROM File, Path, Job
  WHERE Path.PathId = File.PathId
  AND File.JobId = Job.JobId
  AND Path.Path LIKE 
        (SELECT " . $self->dbh_strcat('Path',"'\%'") . " FROM Path WHERE PathId IN ($dir)
        )
  AND File.JobId IN ($inclause)
  ORDER BY Path.Path, File.FilenameId, Job.StartTime DESC";
    }
    else
    {
	# It's a file. Great, we allready have most 
	# of what is needed. Simple and efficient query
	my $dir = $entry->[0];
	my $fileid = $entry->[1];
	
	my $jobid = $entry->[3];
	my $fileindex = $entry->[9];
	my $inclause = $entry->[5]; # curjobids
	$query = 
"SELECT Path.Path, File.FilenameId, File.Lstat
  FROM File, Path
  WHERE Path.PathId = File.PathId
  AND Path.PathId = $dir
  AND File.FilenameId = $fileid
  AND File.JobId = $jobid";
    }

    my ($path,$nameid,$lstat);
    my $sth = $self->dbh_prepare($query);
    $sth->execute;
    $sth->bind_columns(\$path,\$nameid,\$lstat);
    my $old_path='';
    my $old_nameid='';
    my $total_size=0;
    my $total_files=0;

    &$refresh();

    my $rcount=0;
    # We fetch all rows
    while ($sth->fetchrow_arrayref())
    {
        # Only the latest version of a file
        next if ($nameid eq $old_nameid and $path eq $old_path);

	if ($rcount > 15000) {
	    &$refresh();
	    $rcount=0;
	} else {
	    $rcount++;
	}

        # We get the size of this file
        my $size=lstat_attrib($lstat,'st_size');
        $total_size += $size;
        $total_files++;
        $old_path=$path;
        $old_nameid=$nameid;
    }

    return ($total_size,$total_files);
}

# Returns list of versions of a file that could be restored
# returns an array of 
# ('FILE:',jobid,fileindex,mtime,size,inchanger,md5,volname)
# there will be only one jobid in the array of jobids...
sub get_all_file_versions
{
    my ($self,$pathid,$fileid,$client,$see_all)=@_;
    
    defined $see_all or $see_all=0;
    
    my @versions;
    my $query;
    $query = 
"SELECT File.JobId, File.FileIndex, File.Lstat, 
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
	my ($jobid, $fileindex, $lstat, $md5, $volname, $inchanger) = @$refrow;
	my @attribs = parse_lstat($lstat);
	my $mtime = array_attrib('st_mtime',\@attribs);
	my $size = array_attrib('st_size',\@attribs);
		
	my @list = ('FILE:',$pathid,$fileid,$jobid,
		    $fileindex, $mtime, $size, $inchanger,
		    $md5, $volname);
	push @versions, (\@list);
    }
	
    # We have the list of all versions of this file.
    # We'll sort it by mtime desc, size, md5, inchanger desc
    # the rest of the algorithm will be simpler
    # ('FILE:',filename,jobid,fileindex,mtime,size,inchanger,md5,volname)
    @versions = sort { $b->[5] <=> $a->[5] 
		    || $a->[6] <=> $b->[6] 
		    || $a->[8] cmp $a->[8] 
		    || $b->[7] <=> $a->[7]} @versions;

	
    my @good_versions;
    my %allready_seen_by_mtime;
    my %allready_seen_by_md5;
    # Now we should create a new array with only the interesting records
    foreach my $ref (@versions)
    {	
	if ($ref->[8])
	{
	    # The file has a md5. We compare his md5 to other known md5...
	    # We take size into account. It may happen that 2 files
	    # have the same md5sum and are different. size is a supplementary
	    # criterion
            
            # If we allready have a (better) version
	    next if ( (not $see_all) 
	              and $allready_seen_by_md5{$ref->[8] .'-'. $ref->[6]}); 

	    # we never met this one before...
	    $allready_seen_by_md5{$ref->[8] .'-'. $ref->[6]}=1;
	}
	# Even if it has a md5, we should also work with mtimes
        # We allready have a (better) version
	next if ( (not $see_all)
	          and $allready_seen_by_mtime{$ref->[5] .'-'. $ref->[6]}); 
	$allready_seen_by_mtime{$ref->[5] .'-'. $ref->[6] . '-' . $ref->[8]}=1;
	
	# We reached there. The file hasn't been seen.
	push @good_versions,($ref);
    }
	
    # To be nice with the user, we re-sort good_versions by
    # inchanger desc, mtime desc
    @good_versions = sort { $b->[5] <=> $a->[5] 
                         || $b->[3] <=> $a->[3]} @good_versions;
	
    return @good_versions;
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
	    my $sth2 = $self->{conf}->{dbh}->prepare_cached($query);
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
		$sth2 = $self->{conf}->{dbh}->prepare_cached($query);
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
    my $sth = $self->{conf}->{dbh}->prepare_cached($query);
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
	$sth = $self->{conf}->{dbh}->prepare_cached($query);
	$sth->execute($path);
	$sth->finish();
        
	$query = "SELECT PathId FROM Path WHERE Path = ?";
	#print STDERR $query,"\n" if $debug;
	$sth = $self->{conf}->{dbh}->prepare_cached($query);
	$sth->execute($path);
	$result = $sth->fetchrow_arrayref();
	$sth->finish();
	return $result->[0];
    }
}


sub create_brestore_tables
{
    my ($self) = @_;
    my $ret = 0;
    my $verif = "SELECT 1 FROM brestore_knownjobid LIMIT 1";

    unless ($self->dbh_do($verif)) {
	$ret=1;

	my $req = "
    CREATE TABLE brestore_knownjobid
    (
     JobId int4 NOT NULL,
     CONSTRAINT brestore_knownjobid_pkey PRIMARY KEY (JobId)
    )";
	$self->dbh_do($req);
    }
    
    $verif = "SELECT 1 FROM brestore_pathhierarchy LIMIT 1";
    unless ($self->dbh_do($verif)) {
	$ret=1;
	my $req = "
   CREATE TABLE brestore_pathhierarchy
   (
     PathId int4 NOT NULL,
     PPathId int4 NOT NULL,
     CONSTRAINT brestore_pathhierarchy_pkey PRIMARY KEY (PathId)
   )";
	$self->dbh_do($req);


	$req = "CREATE INDEX brestore_pathhierarchy_ppathid 
                          ON brestore_pathhierarchy (PPathId)";
	$self->dbh_do($req);
    }
    
    $verif = "SELECT 1 FROM brestore_pathvisibility LIMIT 1";
    unless ($self->dbh_do($verif)) {
	$ret=1;
	my $req = "
    CREATE TABLE brestore_pathvisibility
    (
      PathId int4 NOT NULL,
      JobId int4 NOT NULL,
      Size int8 DEFAULT 0,
      Files int4 DEFAULT 0,
      CONSTRAINT brestore_pathvisibility_pkey PRIMARY KEY (JobId, PathId)
    )";
	$self->dbh_do($req);

	$req = "CREATE INDEX brestore_pathvisibility_jobid
                          ON brestore_pathvisibility (JobId)";
	$self->dbh_do($req);
    }
    return $ret;
}

# Get metadata
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

1;

################################################################
package BwebConsole;
use LWP::UserAgent;
use HTTP::Request::Common;

sub new
{
    my ($class, %arg) = @_;

    my $self = bless {
	pref => $arg{pref},	# Pref object
	timeout => $arg{timeout} || 20,
	debug   => $arg{debug} || 0,
	'list_job'     => '',
	'list_client'  => '',
	'list_fileset' => '',
	'list_storage' => '',
	'run'          => '',
    };

    return $self;
}

sub prepare
{
    my ($self, @what) = @_;
    my $ua = LWP::UserAgent->new();
    $ua->agent("Brestore/$VERSION");
    my $req = POST($self->{pref}->{bconsole},
		   Content_Type => 'form-data',
		   Content => [ map { (action => $_) } @what ]);
    #$req->authorization_basic('eric', 'test');

    my $res = $ua->request($req);

    if ($res->is_success) {
	foreach my $l (split(/\n/, $res->content)) {
	    my ($k, $c) = split(/=/,$l,2);
	    $self->{$k} = $c;
	}
    } else {
	$self->{error} = "Can't connect to bweb : " . $res->status_line;
	new DlgWarn($self->{error});
    }
}

sub run
{
    my ($self, %arg) = @_;

    my $ua = LWP::UserAgent->new();
    $ua->agent("Brestore/$VERSION");
    my $req = POST($self->{pref}->{bconsole},
		   Content_Type => 'form-data',
		   Content => [ job     => $arg{job},
				client  => $arg{client},
				storage => $arg{storage} || '',
				fileset => $arg{fileset} || '',
				where   => $arg{where}   || '',
				regexwhere  => $arg{regexwhere}  || '',
				priority=> $arg{prio}    || '',
				replace => $arg{replace},
				action  => 'run',
				timeout => 10,
				bootstrap => [$arg{bootstrap}],
				]);
    #$req->authorization_basic('eric', 'test');

    my $res = $ua->request($req);

    if ($res->is_success) {
	foreach my $l (split(/\n/, $res->content)) {
	    my ($k, $c) = split(/=/,$l,2);
	    $self->{$k} = $c;
	}
    } 

    if (!$self->{run}) {
        new DlgWarn("Can't connect to bweb : " . $res->status_line);
    } 

    unlink($arg{bootstrap});

    return $self->{run};
}

sub list_job
{
    my ($self) = @_;
    return sort split(/;/, $self->{'list_job'});
}

sub list_fileset
{
    my ($self) = @_;
    return sort split(/;/, $self->{'list_fileset'});
}

sub list_storage
{
    my ($self) = @_;
    return sort split(/;/, $self->{'list_storage'});
}
sub list_client
{
    my ($self) = @_;
    return sort split(/;/, $self->{'list_client'});
}

1;
################################################################

package main;

use Getopt::Long ;

sub HELP_MESSAGE
{
    print STDERR "Usage: $0 [--conf=brestore.conf] [--batch] [--debug]\n";
    exit 1;
}

my $file_conf = (exists $ENV{HOME})? "$ENV{HOME}/.brestore.conf" : undef ;
my $batch_mod;

GetOptions("conf=s"   => \$file_conf,
	   "batch"    => \$batch_mod,
	   "debug"    => \$debug,
	   "help"     => \&HELP_MESSAGE) ;

if (! defined $file_conf) {
    print STDERR "Could not detect default config and no config file specified\n";
    HELP_MESSAGE();
}

my $p = new Pref($file_conf);

if (! -f $file_conf) {
    $p->write_config();
}

if ($batch_mod) {
    my $vfs = new Bvfs(conf => $p);
    if ($p->connect_db()) {
	if ($vfs->create_brestore_tables()) {
	    print "Creating brestore tables\n";
	}
	$vfs->update_cache();
    }
    exit (0);
}

$glade_file = $p->{glade_file} || $glade_file;

foreach my $path ('','.','/usr/share/brestore','/usr/local/share/brestore') {
    if (-f "$path/$glade_file") {
	$glade_file = "$path/$glade_file" ;
	last;
    }
}

# gtk have lots of warning on stderr
if ($^O eq 'MSWin32')
{
    close(STDERR);
    open(STDERR, ">stderr.log");
}

Gtk2->init();

if ( -f $glade_file) {
    my $w = new DlgResto($p);

} else {
    my $widget = Gtk2::MessageDialog->new(undef, 'modal', 'error', 'close', 
"Can't find your brestore.glade (glade_file => '$glade_file')
Please, edit your $file_conf to setup it." );
 
    $widget->signal_connect('destroy', sub { Gtk2->main_quit() ; });
    $widget->run;
    exit 1;
}

Gtk2->main; # Start Gtk2 main loop	

# that's it!

exit 0;

__END__
package main;

my $p = new Pref("$ENV{HOME}/.brestore.conf");
$p->{debug} = 1;
$p->connect_db() || print $p->{error};

my $bvfs = new Bvfs(conf => $p);

$bvfs->debug($bvfs->get_root());
$bvfs->ch_dir($bvfs->get_root());
$bvfs->up_dir();
$bvfs->set_curjobids(268,178,282,281,279);
$bvfs->ls_files();
my $dirs = $bvfs->ls_dirs();
$bvfs->ch_dir(123496);
$dirs = $bvfs->ls_dirs();
$bvfs->ls_files();
map { $bvfs->debug($_) } $bvfs->get_all_file_versions($bvfs->{cwdid},312433, "exw3srv3", 1);
