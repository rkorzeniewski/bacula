#!/usr/bin/perl -w
use strict ;

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

  To speed up database query you have to create theses indexes
    - CREATE INDEX file_pathid on File(PathId);
    - ...

  To follow restore job, you must have a running Bweb installation.

=head1 COPYRIGHT

  Copyright (C) 2006 Marc Cousin and Eric Bollengier

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.
 
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
  
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the
  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
  
  Base 64 functions from Karl Hakimian <hakimian@aha.com>
  Integrally copied from recover.pl from bacula source distribution.

=cut

use File::Spec;			# portable path manipulations
use Gtk2 '-init';		# auto-initialize Gtk2
use Gtk2::GladeXML;
use Gtk2::SimpleList;		# easy wrapper for list views
use Gtk2::Gdk::Keysyms;		# keyboard code constants
use Data::Dumper qw/Dumper/;
use DBI;
my $debug=0;			# can be on brestore.conf

################################################################

package DlgFileVersion;

sub on_versions_close_clicked
{
    my ($self, $widget)=@_;
    $self->{version}->destroy();
}

sub on_selection_button_press_event
{
    print "on_selection_button_press_event()\n";
}

sub fileview_data_get
{
    my ($self, $widget, $context, $data, $info, $time,$string) = @_;

    DlgResto::drag_set_info($widget, 
			    $self->{cwd},
			    $data);
}

sub new
{
    my ($class, $dbh, $client, $path, $file) = @_;
    my $self = bless {
	cwd       => $path,
	version   => undef, # main window
	};

    # we load version widget of $glade_file
    my $glade_box = Gtk2::GladeXML->new($glade_file, "dlg_version");

    # Connect signals magically
    $glade_box->signal_autoconnect_from_package($self);

    $glade_box->get_widget("version_label")
	->set_markup("<b>File revisions : $client:$path/$file</b>");

    my $widget = $glade_box->get_widget('version_fileview');
    my $fileview = Gtk2::SimpleList->new_from_treeview(
		   $widget,
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

    my @v = DlgResto::get_all_file_versions($dbh, 
					    "$path/", 
					    $file,
					    $client,
					    1);
    for my $ver (@v) {
	my (undef,$fn,$jobid,$fileindex,$mtime,$size,$inchanger,$md5,$volname)
	    = @{$ver};
	my $icon = ($inchanger)?$DlgResto::yesicon:$DlgResto::noicon;

	DlgResto::listview_push($fileview,
				$file, $jobid, 'file', 
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
package DlgWarn;

sub new
{
    my ($package, $text) = @_;

    my $self = bless {};

    my $glade = Gtk2::GladeXML->new($glade_file, "dlg_warn");

    # Connect signals magically
    $glade->signal_autoconnect_from_package($self);
    $glade->get_widget('label_warn')->set_text($text);

    print "$text\n";

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

use Bconsole;

# %arg = (bsr_file => '/path/to/bsr',       # on director
#         volumes  => [ '00001', '00004']
#         pref     => ref Pref
#         );

sub new
{
    my ($class, %arg) = @_;

    my $self = bless {
	bsr_file => $arg{bsr_file}, # /path/to/bsr on director
	pref     => $arg{pref},	# Pref ref
	glade => undef,		# GladeXML ref
	bconsole => undef,	# Bconsole ref
    };

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

    my $console = $self->{bconsole} = new Bconsole(pref => $arg{pref});

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

    if ($ret == -1) {
	my $widget = Gtk2::MessageDialog->new(undef, 'modal', 'info', 'close', 
"Your job have been submited to bacula.
To follow it, you must use bconsole (or install/configure bweb)");
	$widget->run;
	$widget->destroy();
    }

    $self->on_cancel_resto_clicked();
}

sub on_cancel_resto_clicked
{
    my ($self) = @_ ;
    $self->{glade}->get_widget('dlg_launch')->destroy();
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

    my $where = $glade->get_widget('entry_launch_where')->get_text();

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
				       where   => $where,
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
# preference reader
package Pref;

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
	glade_file => $glade_file,
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

	if (defined $self->{debug}) {
	    $debug = $self->{debug} ;
	}
    } else {
	# TODO : Force dumb default values and display a message
    }
}

sub write_config
{
    my ($self) = @_;
    
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
	# TODO : Display a message
    }
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

    system("$self->{mozilla} -remote 'Ping()'");
    if ($? != 0) {
	new DlgWarn("Warning, you must have a running $self->{mozilla} to $msg");
	return 0;
    }

    my $cmd = "$self->{mozilla} -remote 'OpenURL($self->{bweb}$url,new-tab)'" ;
    print "$cmd\n";
    system($cmd);
    return ($? == 0);
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

    $pref->write_config();
    if ($pref->connect_db()) {
	$self->{dlgresto}->set_dbh($pref->{dbh});
        $self->{dlgresto}->set_status('Preferences updated');
	$self->{dlgresto}->init_server_backup_combobox();
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
# Main Interface

package DlgResto;

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
    my @unit = qw(b Kb Mb Gb Tb);
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

sub set_dbh
{
    my ($self, $dbh) = @_;
    $self->{dbh} = $dbh;
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
	pref => $pref,
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
	cwd   => '/',
	client_combobox => undef, # client_combobox widget
	restore_backup_combobox => undef, # date combobox widget
	list_client => undef,   # Gtk2::ListStore
        list_backup => undef,   # Gtk2::ListStore
    };

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
					      'h_name'        => 'hidden',
					      'h_jobid'       => 'hidden',
					      'h_type'        => 'hidden',

					      ''              => 'pixbuf',
				              'File Name'     => 'text',
					      'Size'          => 'text',
					      'Date'          => 'text');
    init_drag_drop($fileview);
    $fileview->set_search_column(4); # search on File Name

    # Connect glade-restore_list to Gtk2::SimpleList
    $widget = $glade->get_widget('restorelist');
    my $restore_list = $self->{restore_list} = Gtk2::SimpleList->new_from_treeview(
                                              $widget,
					      'h_name'      => 'hidden',
                                              'h_jobid'     => 'hidden',
					      'h_type'      => 'hidden',
                                              'h_curjobid'  => 'hidden',

                                              ''            => 'pixbuf',
                                              'File Name'   => 'text',
                                              'JobId'       => 'text',
                                              'FileIndex'   => 'text',

					      'Nb Files'    => 'text', #8
                                              'Size'        => 'text', #9
					      'size_b'      => 'hidden', #10
					      );

    my @restore_list_target_table = ({'target' => 'STRING',
				      'flags' => [], 
				      'info' => 40 });	

    $restore_list->enable_model_drag_dest(['copy'],@restore_list_target_table);
    $restore_list->get_selection->set_mode('multiple');
    
    $widget = $glade->get_widget('infoview');
    my $infoview = $self->{fileinfo} = Gtk2::SimpleList->new_from_treeview(
		   $widget,
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
	$self->{dbh} = $pref->{dbh};
	$self->init_server_backup_combobox();
    }
}

# set status bar informations
sub set_status
{
    my ($self, $string) = @_;
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
    print $query,"\n" if $debug;
    my $result = $dbh->selectall_arrayref($query);
    my @return_array;
    foreach my $refrow (@$result)
    {
	push @return_array,($refrow->[0]);
    }
    return @return_array;
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
    my ($dbh, $client, $ok_only)=@_;
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
    print $query,"\n" if $debug;
    my $result = $dbh->selectall_arrayref($query);

    return @$result;
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
    foreach my $entry (@{$self->{restore_list}->{data}})
    {
	unless ($entry->[9]) {
	    my ($size, $nb) = $self->estimate_restore_size($entry);
	    $entry->[10] = $size;
	    $entry->[9] = human($size);
	    $entry->[8] = $nb;
	}

	my $name = unpack('u', $entry->[0]);

	$txt .= "\n<i>$name</i> : " . $entry->[8] . " file(s)/" . $entry->[9] ;
	$widget->set_markup($title . $txt);
	
	$size_total+=$entry->[10];
	$nb_total+=$entry->[8];
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

    new DlgLaunch(pref     => $self->{pref},
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
    my $dbh = $self->{dbh};

    $self->{list_backup}->clear();

    if ($self->current_client eq $client_list_empty) {
	return 0 ;
    }

    my @endtimes=get_all_endtimes_for_job($dbh, 
					  $self->current_client,
					  $self->{pref}->{use_ok_bkp_only});
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

    $self->{CurrentJobIds} = [
			      set_job_ids_for_date($dbh,
						   $self->current_client,
						   $self->current_date,
						   $self->{pref}->{use_ok_bkp_only})
			      ];

    $self->ch_dir('');

#     undef $self->{dirtree};
    $self->refresh_fileview();
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
    fill_server_list($self->{dbh}, 
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
    my $cwd = $self->{cwd};

    @{$fileview->{data}} = ();

    $self->clear_infoview();
    
    my $client_name = $self->current_client;

    if (!$client_name or ($client_name eq $client_list_empty)) {
	$self->set_status("Client list empty");
	return;
    }

    my @dirs     = $self->list_dirs($cwd,$client_name);
    # [ [listfiles.id, listfiles.Name, File.LStat, File.JobId]..]
    my $files    = $self->list_files($cwd); 
    print "CWD : $cwd\n" if ($debug);
    
    my $file_count = 0 ;
    my $total_bytes = 0;
    
    # Add directories to view
    foreach my $dir (@dirs) {
	my $time = localtime($self->dir_attrib("$cwd/$dir",'st_mtime'));
	$total_bytes += 4096;
	$file_count++;

	listview_push($fileview,
		      $dir,
		      $self->dir_attrib("$cwd/$dir",'jobid'),
		      'dir',

		      $diricon, 
		      $dir, 
		      "4 Kb", 
		      $time);
    }
    
    # Add files to view 
    foreach my $file (@$files) 
    {
	my $size = file_attrib($file,'st_size');
	my $time = localtime(file_attrib($file,'st_mtime'));
	$total_bytes += $size;
	$file_count++;
	# $file = [listfiles.id, listfiles.Name, File.LStat, File.JobId]

	listview_push($fileview,
		      $file->[1],
		      $file->[3],
		      'file',
		      
		      $fileicon, 
		      $file->[1], 
		      human($size), $time);
    }
    
    $self->set_status("$file_count files/" . human($total_bytes));

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
        my $file;
        if ($path eq '')
        {
	    $file = pack("u", $file_info[0]);
	}
	else
	{
		$file = pack("u", $path . '/' . $file_info[0]);
	}
	push @ret, join(" ; ", $file, 
			$file_info[1], # $jobid
			$file_info[2], # $type
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

    if  ($info eq 40 || $info eq 0) # patch for display!=:0
    {
	foreach my $elt (split(/ :: /, $data->data()))
	{
	    
	    my ($file, $jobid, $type) = 
		split(/ ; /, $elt);
	    $file = unpack("u", $file);
	    
	    $self->add_selected_file_to_list($file, $jobid, $type);
	}
    }
}

sub on_back_button_clicked {
    my $self = shift;
    $self->up_dir();
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
    $self->{pref}->go_bweb('', "go on bweb");
}

# Change to parent directory
sub up_dir
{
    my $self = shift ;
    if ($self->{cwd} eq '/')
    {
    	$self->ch_dir('');
    }
    my @dirs = File::Spec->splitdir ($self->{cwd});
    pop @dirs;
    $self->ch_dir(File::Spec->catdir(@dirs));
}

# Change the current working directory
#   * Updates fileview, location, and selection
#
sub ch_dir 
{
    my $self = shift;
    $self->{cwd} = shift;
    
    $self->refresh_fileview();
    $self->{location}->set_text($self->{cwd});
    
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
	my ($name, @other) = @{$list->{data}->[$selected[0]]};
	return (unpack('u', $name), @other);
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
	my ($name, @other) = @{$list->{data}->[$i]};
	push @ret, [unpack('u', $name), @other];
    } 
    return @ret;
}


sub listview_push
{
    my ($list, $name, @other) = @_;
    push @{$list->{data}}, [pack('u', $name), @other];
}

#----------------------------------------------------------------------
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
	$self->up_dir();
	return 1; # eat keypress
    }

    return 0; # let gtk have keypress
}

sub on_forward_keypress
{
    return 0;
}

#----------------------------------------------------------------------
# Handle double-click (or enter) on file-view
#   * Translates into a 'cd <dir>' command
#
sub on_fileview_row_activated 
{
    my ($self, $widget) = @_;
    
    my ($name, undef, $type, undef) = listview_get_first($widget);

    if ($type eq 'dir')
    {
    	if ($self->{cwd} eq '')
    	{
    		$self->ch_dir($name);
    	}
    	elsif ($self->{cwd} eq '/')
    	{
    		$self->ch_dir('/' . $name);
    	}
    	else
    	{
		$self->ch_dir($self->{cwd} . '/' . $name);
	}

    } else {
	$self->fill_infoview($self->{cwd}, $name);
    }
    
    return 1; # consume event
}

sub fill_infoview
{
    my ($self, $path, $file) = @_;
    $self->clear_infoview();
    my @v = get_all_file_versions($self->{dbh}, 
				  "$path/", 
				  $file,
				  $self->current_client,
				  $self->{pref}->{see_all_versions});
    for my $ver (@v) {
	my (undef,$fn,$jobid,$fileindex,$mtime,$size,$inchanger,$md5,$volname)
	    = @{$ver};
	my $icon = ($inchanger)?$yesicon:$noicon;

	$mtime = localtime($mtime) ;

	listview_push($self->{fileinfo},
		      $file, $jobid, 'file', 
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
			      set_job_ids_for_date($self->{dbh},
						   $self->current_client,
						   $self->current_date,
						   $self->{pref}->{use_ok_bkp_only})
			      ];

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
	my ($name, undef) = @{$i};

	new DlgFileVersion($self->{dbh}, 
			   $self->current_client, 
			   $self->{cwd}, $name);
    }
}

sub on_right_click_filelist
{
    my ($self,$widget,$event) = @_;
    # I need to know what's selected
    my @sel = listview_get_all($self->{fileview});
    
    my $type = '';

    if (@sel == 1) {
	$type = $sel[0]->[2];	# $type
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
	my ($file, $jobid, $type, undef) = @{$i};
	$file = $self->{cwd} . '/' . $file;
	$self->add_selected_file_to_list($file, $jobid, $type);
    }
}

# Adds a file to the filelist
sub add_selected_file_to_list
{
    my ($self, $name, $jobid, $type)=@_;

    my $dbh = $self->{dbh};
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
	my $dirfileindex = get_fileindex_from_dir_jobid($dbh,$name,$jobid);
	listview_push($restore_list, 
		      $name, $jobid, 'dir', $curjobids,
		      $diricon, $name,$curjobids,$dirfileindex);
    }
    elsif ($type eq 'file')
    {
	my $fileindex = get_fileindex_from_file_jobid($dbh,$name,$jobid);

	listview_push($restore_list,
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
	
    print $query,"\n" if $debug;
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
    print Data::Dumper::Dumper(\@CurrentJobIds) if $debug;

    return @CurrentJobIds;
}

# Lists all directories contained inside a directory.
# Uses the current dir, the client name, and CurrentJobIds for visibility.
# Returns an array of dirs
sub list_dirs
{
    my ($self,$dir,$client)=@_;
    print "list_dirs($dir, $client)\n";

    # Is data allready cached ?
    if (not $self->{dirtree}->{$client})
    {
	$self->cache_dirs($client);
    }

    if ($dir ne '' and substr $dir,-1 ne '/')
    {
	$dir .= '/'; # In the db, there is a / at the end of the dirs ...
    }
    # Here, the tree is cached in ram
    my @dir = split('/',$dir,-1);
    pop @dir; # We don't need the empty trailing element
    
    # We have to get the reference of the hash containing $dir contents
    # Get to the root
    my $refdir=$self->{dirtree}->{$client};

    # Find the leaf
    foreach my $subdir (@dir)
    {
	if ($subdir eq '')
	{
		$subdir = '/';
	}
	$refdir = $refdir->[0]->{$subdir};
    }
    
    # We reached the directory
    my @return_list;
  DIRLOOP:
    foreach my $dir (sort(keys %{$refdir->[0]}))
    {
	# We return the directory's content : only visible directories
	foreach my $jobid (reverse(sort(@{$self->{CurrentJobIds}})))
	{
	    if (defined $refdir->[0]->{$dir}->[1]->{$jobid})
	    {
		my $dirname = $refdir->[0]->{$dir}->[2]; # The real dirname...
		push @return_list,($dirname);
		next DIRLOOP; # No need to waste more CPU cycles...
	    }
	}
    }
    print "LIST DIR : ", Data::Dumper::Dumper(\@return_list),"\n";
    return @return_list;
}


# List all files in a directory. dir as parameter, CurrentJobIds for visibility
# Returns an array of dirs
sub list_files
{
    my ($self, $dir)=@_;
    my $dbh = $self->{dbh};

    my $empty = [];

    print "list_files($dir)\n";

    if ($dir ne '' and substr $dir,-1 ne '/')
    {
	$dir .= '/'; # In the db, there is a / at the end of the dirs ...
    }

    my $query = "SELECT Path.PathId FROM Path WHERE Path.Path = '$dir'";
    print $query,"\n" if $debug;
    my @list_pathid=();
    my $result = $dbh->selectall_arrayref($query);
    foreach my $refrow (@$result)
    {
	push @list_pathid,($refrow->[0]);
    }
	
    if  (@list_pathid == 0)
    {
	print "No pathid found for $dir\n" if $debug;
	return $empty;
    }
	
    my $inlistpath = join (',', @list_pathid);
    my $inclause = join (',', @{$self->{CurrentJobIds}});
    if ($inclause eq '')
    {
	return $empty;
    }
	
    $query = 
"SELECT listfiles.id, listfiles.Name, File.LStat, File.JobId
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
	
    print $query,"\n" if $debug;
    $result = $dbh->selectall_arrayref($query);
	
    return $result;
}

sub refresh_screen
{
    Gtk2->main_iteration while (Gtk2->events_pending);
}

# For the dirs, because of the db schema, it's inefficient to get the
# directories contained inside other directories (regexp match or tossing
# lots of records...). So we load all the tree and cache it.  The data is 
# stored in a structure of this form :
# Each directory is an array. 
# - In this array, the first element is a ref to next dir (hash) 
# - The second element is a hash containing all jobids pointing
# on an array containing their lstat (or 1 if this jobid is there because 
# of dependencies)
# - The third is the filename itself (it could get mangled because of 
# the hashing...) 

# So it looks like this :
# $reftree->[  	{ 'dir1' => $refdir1
#		  'dir2' => $refdir2
#		......
#		},
#		{ 'jobid1' => 'lstat1',
#		  'jobid2' => 'lstat2',
#		  'jobid3' => 1            # This one is here for "visibility"
#		},
#		'dirname'
#	   ]

# Client as a parameter
# Returns an array of dirs
sub cache_dirs
{
    my ($self, $client) = @_;
    print "cache_dirs()\n";

    $self->{dirtree}->{$client} = [];	# reset cache
    my $dbh = $self->{dbh};

    # TODO : If we get here, things could get lenghty ... draw a popup window .
    my $widget = Gtk2::MessageDialog->new($self->{mainwin}, 
					  'destroy-with-parent', 
					  'info', 'none', 
					  'Populating cache');
    $widget->show;
	
    # We have to build the tree, as it's the first time it is asked...
    
    
    # First, we only need the jobids of the selected server.
    # It's not the same as @CurrentJobIds (we need ALL the jobs)
    # We get the JobIds first in order to have the best execution
    # plan possible for the big query, with an in clause.
    my $query;
    my $status = get_wanted_job_status($self->{pref}->{use_ok_bkp_only});
    $query = 
"SELECT JobId 
 FROM Job,Client
 WHERE Job.ClientId = Client.ClientId
   AND Client.Name = '$client'
   AND Job.JobStatus IN ($status)
   AND Job.Type = 'B'";
	
    print $query,"\n" if $debug;
    my $result = $dbh->selectall_arrayref($query);
    refresh_screen();

    my @jobids;
    foreach my $record (@{$result})
    {
	push @jobids,($record->[0]);
    }
    my $inclause = join(',',@jobids);
    if ($inclause eq '')
    {
	$widget->destroy();
        $self->set_status("No previous backup found for $client");
	return ();
    }

# Then, still to help dear mysql, we'll retrieve the PathId from empty Path (directory entries...)
   my @dirids;
    $query =
"SELECT Filename.FilenameId FROM Filename WHERE Filename.Name=''";

    print $query,"\n" if $debug;
    $result = $dbh->selectall_arrayref($query);
    refresh_screen();

    foreach my $record (@{$result})
    {
    	push @dirids,$record->[0];
    }
    my $dirinclause = join(',',@dirids);

   # This query is a bit complicated : 
   # whe need to find all dir entries that should be displayed, even
   # if the directory itself has no entry in File table (it means a file
   # is explicitely chosen in the backup configuration)
   # Here's what I wanted to do :
#     $query = 
# "
# SELECT T1.Path, T2.Lstat, T2.JobId
# FROM (    SELECT DISTINCT Path.PathId, Path.Path FROM File, Path
#     WHERE File.PathId = Path.PathId
# AND File.JobId IN ($inclause)) AS T1
# LEFT JOIN 
#     (    SELECT File.Lstat, File.JobId, File.PathId FROM File
#         WHERE File.FilenameId IN ($dirinclause)
#         AND File.JobId IN ($inclause)) AS T2
# ON (T1.PathId = T2.PathId)
# ";		
    # It works perfectly with postgresql, but mysql doesn't seem to be able
    # to do the hash join correcty, so the performance sucks.
    # So it will be done in 4 steps :
    # o create T1 and T2 as temp tables
    # o create an index on T2.PathId
    # o do the query
    # o remove the temp tables
    $query = "
CREATE TEMPORARY TABLE T1 AS
SELECT DISTINCT Path.PathId, Path.Path FROM File, Path
WHERE File.PathId = Path.PathId
  AND File.JobId IN ($inclause)
";
    print $query,"\n" if $debug;
    $dbh->do($query);
    refresh_screen();

    $query = "
CREATE TEMPORARY TABLE T2 AS
SELECT File.Lstat, File.JobId, File.PathId FROM File
WHERE File.FilenameId IN ($dirinclause)
  AND File.JobId IN ($inclause)
";
    print $query,"\n" if $debug;
    $dbh->do($query);
    refresh_screen();

    $query = "
CREATE INDEX tmp2 ON T2(PathId)
";
    print $query,"\n" if $debug;
    $dbh->do($query);
    refresh_screen();
    
    $query = "
SELECT T1.Path, T2.Lstat, T2.JobId
FROM T1 LEFT JOIN T2
ON (T1.PathId = T2.PathId)
";
    print $query,"\n" if $debug;
    $result = $dbh->selectall_arrayref($query);
    refresh_screen();
    
    my $rcount=0;
    foreach my $record (@{$result})
    {
	if ($rcount > 15000) {
	    refresh_screen();
	    $rcount=0;
	} else {
	    $rcount++;
	}
    	# Dirty hack to force the string encoding on perl... we don't
    	# want implicit conversions
	my $path = pack "U0C*", unpack "C*",$record->[0];
	
	my @path = split('/',$path,-1);
	pop @path; # we don't need the trailing empty element
	my $lstat = $record->[1];
	my $jobid = $record->[2];
	
	# We're going to store all the data on the cache tree.
	# We find the leaf, then store data there
	my $reftree=$self->{dirtree}->{$client};
	foreach my $dir(@path)
	{
	    if ($dir eq '')
	    {
	    	$dir = '/';
	    }
	    if (not defined($reftree->[0]->{$dir}))
	    {
		my @tmparray;
		$reftree->[0]->{$dir}=\@tmparray;
	    }
	    $reftree=$reftree->[0]->{$dir};
	    $reftree->[2]=$dir;
	}
	# We can now add the metadata for this dir ...
	
#         $result = $dbh->selectall_arrayref($query);
	if ($lstat)
	{
            # contains something
            $reftree->[1]->{$jobid}=$lstat;
	}
	else
	{
            # We have a very special case here...
            # lstat is not defined.
            # it means the directory is there because a file has been
            # backuped. so the dir has no entry in File table.
            # That's a rare case, so we can afford to determine it's
            # visibility with a query
            my $select_path=$record->[0];
            $select_path=$dbh->quote($select_path); # gotta be careful
            my $query = "
SELECT File.JobId
FROM File, Path
WHERE File.PathId = Path.PathId
AND Path.Path = $select_path
";
            print $query,"\n" if $debug;
            my $result2 = $dbh->selectall_arrayref($query);
            foreach my $record (@{$result2})
            {
                my $jobid=$record->[0];
                $reftree->[1]->{$jobid}=1;
            }
	}
	
    }
    $query = "
DROP TABLE T1;
";
    print $query,"\n" if $debug;
    $dbh->do($query);
    $query = "
DROP TABLE T2;
";
    print $query,"\n" if $debug;
    $dbh->do($query);


    list_visible($self->{dirtree}->{$client});
    $widget->destroy();

#      print Data::Dumper::Dumper($self->{dirtree});
}

# Recursive function to calculate the visibility of each directory in the cache
# tree Working with references to save time and memory
# For each directory, we want to propagate it's visible jobids onto it's
# parents directory.
# A tree is visible if
# - it's been in a backup pointed by the CurrentJobIds
# - one of it's subdirs is in a backup pointed by the CurrentJobIds
# In the second case, the directory is visible but has no metadata.
# We symbolize this with lstat = 1 for this jobid in the cache.

# Input : reference directory
# Output : visibility of this dir. Has to know visibility of all subdirs
# to know it's visibility, hence the recursing.
sub list_visible
{
    my ($refdir)=@_;
	
    my %visibility;
    # Get the subdirs array references list
    my @list_ref_subdirs;
    while( my (undef,$ref_subdir) = each (%{$refdir->[0]}))
    {
	push @list_ref_subdirs,($ref_subdir);
    }

    # Now lets recurse over these subdirs and retrieve the reference of a hash
    # containing the jobs where they are visible
    foreach my $ref_subdir (@list_ref_subdirs)
    {
	my $ref_list_jobs = list_visible($ref_subdir);
	foreach my $jobid (keys %$ref_list_jobs)
	{
	    $visibility{$jobid}=1;
	}
    }

    # Ok. Now, we've got the list of those jobs.  We are going to update our
    # hash (element 1 of the dir array) containing our jobs Do NOT overwrite
    # the lstat for the known jobids. Put 1 in the new elements...  But first,
    # let's store the current jobids
    my @known_jobids;
    foreach my $jobid (keys %{$refdir->[1]})
    {
	push @known_jobids,($jobid);
    }
    
    # Add the new jobs
    foreach my $jobid (keys %visibility)
    {
	next if ($refdir->[1]->{$jobid});
	$refdir->[1]->{$jobid} = 1;
    }
    # Add the known_jobids to %visibility
    foreach my $jobid (@known_jobids)
    {
	$visibility{$jobid}=1;
    }
    return \%visibility;
}

# Returns the list of media required for a list of jobids.
# Input : dbh, jobid1, jobid2...
# Output : reference to array of (joibd, inchanger)
sub get_required_media_from_jobid
{
    my ($dbh, @jobids)=@_;
    my $inclause = join(',',@jobids);
    my $query = "
SELECT DISTINCT JobMedia.MediaId, Media.InChanger 
FROM JobMedia, Media 
WHERE JobMedia.MediaId=Media.MediaId 
AND JobId In ($inclause)
ORDER BY MediaId";
    my $result = $dbh->selectall_arrayref($query);
    return $result;
}

# Returns the fileindex from dirname and jobid.
# Input : dbh, dirname, jobid
# Output : fileindex
sub get_fileindex_from_dir_jobid
{
    my ($dbh, $dirname, $jobid)=@_;
    my $query;
    $query = "SELECT File.FileIndex
		FROM File, Filename, Path
		WHERE File.FilenameId = Filename.FilenameId
		AND File.PathId = Path.PathId
		AND Filename.Name = ''
		AND Path.Path = '$dirname'
		AND File.JobId = '$jobid'
		";
		
    print $query,"\n" if $debug;
    my $result = $dbh->selectall_arrayref($query);
    return $result->[0]->[0];
}

# Returns the fileindex from filename and jobid.
# Input : dbh, filename, jobid
# Output : fileindex
sub get_fileindex_from_file_jobid
{
    my ($dbh, $filename, $jobid)=@_;
    
    my @dirs = File::Spec->splitdir ($filename);
    $filename=pop(@dirs);
    my $dirname = File::Spec->catdir(@dirs) . '/';
    
    
    my $query;
    $query = 
"SELECT File.FileIndex
 FROM File, Filename, Path
 WHERE File.FilenameId = Filename.FilenameId
   AND File.PathId = Path.PathId
   AND Filename.Name = '$filename'
   AND Path.Path = '$dirname'
   AND File.JobId = '$jobid'";
		
    print $query,"\n" if $debug;
    my $result = $dbh->selectall_arrayref($query);
    return $result->[0]->[0];
}


# Returns list of versions of a file that could be restored
# returns an array of 
# ('FILE:',filename,jobid,fileindex,mtime,size,inchanger,md5,volname)
# It's the same as entries of restore_list (hidden) + mtime and size and inchanger
# and volname and md5
# and of course, there will be only one jobid in the array of jobids...
sub get_all_file_versions
{
    my ($dbh,$path,$file,$client,$see_all)=@_;
    
    defined $see_all or $see_all=0;
    
    my @versions;
    my $query;
    $query = 
"SELECT File.JobId, File.FileIndex, File.Lstat, 
        File.Md5, Media.VolumeName, Media.InChanger
 FROM File, Filename, Path, Job, Client, JobMedia, Media
 WHERE File.FilenameId = Filename.FilenameId
   AND File.PathId=Path.PathId
   AND File.JobId = Job.JobId
   AND Job.ClientId = Client.ClientId
   AND Job.JobId = JobMedia.JobId
   AND File.FileIndex >= JobMedia.FirstIndex
   AND File.FileIndex <= JobMedia.LastIndex
   AND JobMedia.MediaId = Media.MediaId
   AND Path.Path = '$path'
   AND Filename.Name = '$file'
   AND Client.Name = '$client'";
	
    print $query if $debug;
	
    my $result = $dbh->selectall_arrayref($query);
	
    foreach my $refrow (@$result)
    {
	my ($jobid, $fileindex, $lstat, $md5, $volname, $inchanger) = @$refrow;
	my @attribs = parse_lstat($lstat);
	my $mtime = array_attrib('st_mtime',\@attribs);
	my $size = array_attrib('st_size',\@attribs);
		
	my @list = ('FILE:', $path.$file, $jobid, $fileindex, $mtime, $size,
		    $inchanger, $md5, $volname);
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
	    # We take size into account. It may happen that 2 files
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
	
    return @good_versions;
}

# TODO : bsr must use only good backup or not (see use_ok_bkp_only)
# This sub creates a BSR from the information in the restore_list
# Returns the BSR as a string
sub create_filelist
{
    	my $self = shift;
    	my $dbh = $self->{dbh};
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
	

	my $result = $dbh->selectall_arrayref($query);

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
	# ($name,$jobid,'file',$curjobids, undef, undef, undef, $dirfileindex);
	
	# Here, we retrieve every file/dir that could be in the restore
	# We do as simple as possible for the SQL engine (no crazy joins,
	# no pseudo join (>= FirstIndex ...), etc ...
	# We do a SQL union of all the files/dirs specified in the restore_list
	my @select_queries;
	foreach my $entry (@{$self->{restore_list}->{data}})
	{
		if ($entry->[2] eq 'dir')
		{
			my $dir = unpack('u', $entry->[0]);
			my $inclause = $entry->[3]; #curjobids

			my $query = 
"(SELECT Path.Path, Filename.Name, File.FileIndex, File.JobId
  FROM File, Path, Filename
  WHERE Path.PathId = File.PathId
  AND File.FilenameId = Filename.FilenameId
  AND Path.Path LIKE '$dir%'
  AND File.JobId IN ($inclause) )";
			push @select_queries,($query);
		}
		else
		{
			# It's a file. Great, we allready have most 
			# of what is needed. Simple and efficient query
			my $file = unpack('u', $entry->[0]);
			my @file = split '/',$file;
			$file = pop @file;
			my $dir = join('/',@file);
			
			my $jobid = $entry->[1];
			my $fileindex = $entry->[7];
			my $inclause = $entry->[3]; # curjobids
			my $query = 
"(SELECT Path.Path, Filename.Name, File.FileIndex, File.JobId
  FROM File, Path, Filename
  WHERE Path.PathId = File.PathId
  AND File.FilenameId = Filename.FilenameId
  AND Path.Path = '$dir/'
  AND Filename.Name = '$file'
  AND File.JobId = $jobid)";
			push @select_queries,($query);
		}
	}
	$query = join("\nUNION ALL\n",@select_queries) . "\nORDER BY FileIndex\n";

	print $query,"\n" if $debug;
	
	#Now we run the query and parse the result...
	# there may be a lot of records, so we better be efficient
	# We use the bind column method, working with references...

	my $sth = $dbh->prepare($query);
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
	# ref->(jobid,VolsessionId,VolsessionTime,File,FirstIndex,
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
			#Â We print the previous one ... 
			# (before that, save the current range ...)
			if ($first_of_current_range != $prev_fileindex)
			{
			 	#Â we are in a range
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

# This function estimates the size to be restored for an entry of the restore
# list
# In : self,reference to the entry
# Out : size in bytes, number of files
sub estimate_restore_size
{
    # reminder : restore_list looks like this : 
    # ($name,$jobid,'file',$curjobids, undef, undef, undef, $dirfileindex);
    my $self=shift;
    my ($entry)=@_;
    my $query;
    my $dbh = $self->{dbh};
    if ($entry->[2] eq 'dir')
    {
	my $dir = unpack('u', $entry->[0]);
	my $inclause = $entry->[3]; #curjobids
	$query = 
"SELECT Path.Path, File.FilenameId, File.LStat
  FROM File, Path, Job
  WHERE Path.PathId = File.PathId
  AND File.JobId = Job.JobId
  AND Path.Path LIKE '$dir%'
  AND File.JobId IN ($inclause)
  ORDER BY Path.Path, File.FilenameId, Job.StartTime DESC";
    }
    else
    {
	# It's a file. Great, we allready have most 
	# of what is needed. Simple and efficient query
	my $file = unpack('u', $entry->[0]);
	my @file = split '/',$file;
	$file = pop @file;
	my $dir = join('/',@file);
	
	my $jobid = $entry->[1];
	my $fileindex = $entry->[7];
	my $inclause = $entry->[3]; # curjobids
	$query = 
"SELECT Path.Path, File.FilenameId, File.Lstat
  FROM File, Path, Filename
  WHERE Path.PathId = File.PathId
  AND Path.Path = '$dir/'
  AND Filename.Name = '$file'
  AND File.JobId = $jobid
  AND Filename.FilenameId = File.FilenameId";
    }

    print $query,"\n" if $debug;
    my ($path,$nameid,$lstat);
    my $sth = $dbh->prepare($query);
    $sth->execute;
    $sth->bind_columns(\$path,\$nameid,\$lstat);
    my $old_path='';
    my $old_nameid='';
    my $total_size=0;
    my $total_files=0;

    refresh_screen();

    my $rcount=0;
    # We fetch all rows
    while ($sth->fetchrow_arrayref())
    {
        # Only the latest version of a file
        next if ($nameid eq $old_nameid and $path eq $old_path);

	if ($rcount > 15000) {
	    refresh_screen();
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
    {   # $file = [listfiles.id, listfiles.Name, File.LStat, File.JobId]

	my ($file, $attrib)=@_;
	
	if (defined $attrib_name_id{$attrib}) {

	    my @d = split(' ', $file->[2]) ; # TODO : cache this
	    
	    return from_base64($d[$attrib_name_id{$attrib}]);

	} elsif ($attrib eq 'jobid') {

	    return $file->[3];

        } elsif ($attrib eq 'name') {

	    return $file->[1];
	    
	} else	{
	    die "Attribute not known : $attrib.\n";
	}
    }

    # Return the jobid or attribute asked for a dir
    sub dir_attrib
    {
	my ($self,$dir,$attrib)=@_;
	
	my @dir = split('/',$dir,-1);
	my $refdir=$self->{dirtree}->{$self->current_client};
	
	if (not defined $attrib_name_id{$attrib} and $attrib ne 'jobid')
	{
	    die "Attribute not known : $attrib.\n";
	}
	# Find the leaf
	foreach my $subdir (@dir)
	{
	    $refdir = $refdir->[0]->{$subdir};
	}
	
	# $refdir is now the reference to the dir's array
	# Is the a jobid in @CurrentJobIds where the lstat is
	# defined (we'll search in reverse order)
	foreach my $jobid (reverse(sort {$a <=> $b } @{$self->{CurrentJobIds}}))
	{
	    if (defined $refdir->[1]->{$jobid} and $refdir->[1]->{$jobid} ne '1')
	    {
		if ($attrib eq 'jobid')
		{
		    return $jobid;
		}
		else
		{
		    my @attribs = parse_lstat($refdir->[1]->{$jobid});
		    return $attribs[$attrib_name_id{$attrib}+1];
		}
	    }
	}

	return 0; # We cannot get a good attribute.
		  # This directory is here for the sake of visibility
    }
    
    sub lstat_attrib
    {
        my ($lstat,$attrib)=@_;
        if (defined $attrib_name_id{$attrib}) 
        {
	    my @d = split(' ', $lstat) ; # TODO : cache this
	    return from_base64($d[$attrib_name_id{$attrib}]);
	}
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

package main;

my $conf = "$ENV{HOME}/.brestore.conf" ;
my $p = new Pref($conf);

if (! -f $conf) {
    $p->write_config();
}

$glade_file = $p->{glade_file};

foreach my $path ('','.','/usr/share/brestore','/usr/local/share/brestore') {
    if (-f "$path/$glade_file") {
	$glade_file = "$path/$glade_file" ;
	last;
    }
}

if ( -f $glade_file) {
    my $w = new DlgResto($p);

} else {
    my $widget = Gtk2::MessageDialog->new(undef, 'modal', 'error', 'close', 
"Can't find your brestore.glade (glade_file => '$glade_file')
Please, edit your $conf to setup it." );
 
    $widget->signal_connect('destroy', sub { Gtk2->main_quit() ; });
    $widget->run;
    exit 1;
}

Gtk2->main; # Start Gtk2 main loop	

# that's it!

exit 0;


__END__

TODO : 


# Code pour trier les colonnes    
    my $mod = $fileview->get_model();
    $mod->set_default_sort_func(sub {
            my ($model, $item1, $item2) = @_;
            my $a = $model->get($item1, 1);  # récupération de la valeur de la 2ème 
            my $b = $model->get($item2, 1);  # colonne (indice 1)
            return $a cmp $b;
        }
    );
    
    $fileview->set_headers_clickable(1);
    my $col = $fileview->get_column(1);    # la colonne NOM, colonne numéro 2
    $col->signal_connect('clicked', sub {
            my ($colonne, $model) = @_;
            $model->set_sort_column_id (1, 'ascending');
        },
        $mod
    );
