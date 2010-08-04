################################################################
use strict;

=head1 LICENSE

   Bweb - A Bacula web interface
   BaculaÂ® - The Network Backup Solution

   Copyright (C) 2000-2009 Free Software Foundation Europe e.V.

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

=head1 VERSION

    $Id$

=cut

package Bweb::Gui;

=head1 PACKAGE

    Bweb::Gui - Base package for all Bweb object

=head2 DESCRIPTION

    This package define base fonction like new, display, etc..

=cut

use HTML::Template;
our $template_dir='/usr/share/bweb/tpl';

=head1 FUNCTION

    new - creation a of new Bweb object

=head2 DESCRIPTION

    This function take an hash of argument and place them
    on bless ref

    IE : $obj = new Obj(name => 'test', age => '10');

         $obj->{name} eq 'test' and $obj->{age} eq 10

=cut

sub new
{
    my ($class, %arg) = @_;
    my $self = bless {
        name => undef,
    }, $class;

    map { $self->{lc($_)} = $arg{$_} } keys %arg ;

    return $self;
}

sub debug
{
    my ($self, $what) = @_;

    if ($self->{debug}) {
        if (ref $what) {
            print "<pre>" . Data::Dumper::Dumper($what) . "</pre>";
        } else {
            print "<pre>$what</pre>";
        }
    }
}

sub fdebug
{
    my ($self, $what) = @_;

    my $old = $self->{debug};
    $self->{debug}=1;
    $self->debug($what);
    $self->{debug} = $old;
}

=head1 FUNCTION

    error - display an error to the user

=head2 DESCRIPTION

    this function set $self->{error} with arg, display a message with
    error.tpl and return 0

=head2 EXAMPLE

    unless (...) {
        return $self->error("Can't use this file");
    }

=cut

sub error
{
    my ($self, $what) = @_;
    $self->{error} = $what;
    $self->display($self, 'error.tpl');
    return 0;
}

=head1 FUNCTION

    display - display an html page with HTML::Template

=head2 DESCRIPTION

    this function is use to render all html codes. it takes an
    ref hash as arg in which all param are usable in template.

    it will use user template_dir then global template_dir 
    to search the template file.

    hash keys are not sensitive. See HTML::Template for more
    explanations about the hash ref. (it's can be quiet hard to understand) 

=head2 EXAMPLE

    $ref = { name => 'me', age => 26 };
    $self->display($ref, "people.tpl");

=cut

sub display
{
    my ($self, $hash, $tpl) = @_ ;
    my $dir = $self->{template_dir} || $template_dir;
    my $lang = $self->{lang} || 'en';
    my $template = HTML::Template->new(filename => $tpl,
                                       path =>["$dir/$lang",
                                               "$dir/$lang/tpl",
                                               $dir],
                                       die_on_bad_params => 0,
                                       case_sensitive => 0);

    foreach my $var (qw/limit offset/) {

        unless ($hash->{$var}) {
            my $value = CGI::param($var) || '';

            if ($value =~ /^(\d+)$/) {
                $template->param($var, $1) ;
            }
        }
    }

    $template->param('thisurl', CGI::url(-relative => 1, -query=>1));
    $template->param('loginname', CGI::remote_user());

    $template->param($hash);
    print $template->output();
}
1;

################################################################

package Bweb::Config;

use base q/Bweb::Gui/;

=head1 PACKAGE
    
    Bweb::Config - read, write, display, modify configuration

=head2 DESCRIPTION

    this package is used for manage configuration

=head2 USAGE

    $conf = new Bweb::Config(config_file => '/path/to/conf');
    $conf->load();

    $conf->edit();

    $conf->save();

=cut

use CGI;

=head1 PACKAGE VARIABLE

    %k_re - hash of all acceptable option.

=head2 DESCRIPTION

    this variable permit to check all option with a regexp.

=cut

our %k_re = ( dbi      => qr/^(dbi:(Pg|mysql):(?:\w+=[\w\d\.-]+;?)+)$/i,
              user     => qr/^([\w\d\.-]+)$/i,
              password => qr/^(.*)$/,
              fv_write_path => qr!^([/\w\d\.-]*)$!,
              template_dir => qr!^([/\w\d\.-]+)$!,
              debug    => qr/^(on)?$/,
              lang     => qr/^(\w\w)?$/,
              email_media => qr/^([\w\d\.-]+@[\d\w\.-]+)$/,
              graph_font  => qr!^([/\w\d\.-]+.ttf)?$!,
              bconsole    => qr!^(.+)?$!,
              syslog_file => qr!^(.+)?$!,
              log_dir     => qr!^(.+)?$!,
              wiki_url    => qr!(.*)$!,
              stat_job_table => qr!^(\w*)$!,
              display_log_time => qr!^(on)?$!,
              enable_security => qr/^(on)?$/,
              enable_security_acl => qr/^(on)?$/,
              default_age => qr/^((?:\d+(?:[ywdhms]\s*?)?)+)\s*$/,
              );

=head1 FUNCTION

    load - load config_file

=head2 DESCRIPTION

    this function load the specified config_file.

=cut

sub load
{
    my ($self) = @_ ;

    unless (open(FP, $self->{config_file}))
    {
        return $self->error("can't load config_file $self->{config_file} : $!");
    }
    my $f=''; my $tmpbuffer;
    while(read FP,$tmpbuffer,4096)
    {
        $f .= $tmpbuffer;
    }
    close(FP);

    my $VAR1;

    no strict; # I have no idea of the contents of the file
    eval "$f" ;
    use strict;

    if ($f and $@) {
        $self->load_old();
        $self->save();
        return $self->error("If you update from an old bweb install, your must reload this page and if it's fail again, you have to configure bweb again...") ;
    }

    # set default values
    $self->{default_age} = '7d';

    foreach my $k (keys %$VAR1) {
        $self->{$k} = $VAR1->{$k};
    }

    return 1;
}

=head1 FUNCTION

    load_old - load old configuration format

=cut

sub load_old
{
    my ($self) = @_ ;

    unless (open(FP, $self->{config_file}))
    {
        return $self->error("$self->{config_file} : $!");
    }

    while (my $line = <FP>)
    {
        chomp($line);
        my ($k, $v) = split(/\s*=\s*/, $line, 2);
        if ($k_re{$k}) {
            $self->{$k} = $v;
        }
    }

    close(FP);
    return 1;
}

=head1 FUNCTION

    save - save the current configuration to config_file

=cut

sub save
{
    my ($self) = @_ ;

    if ($self->{ach_list}) {
        # shortcut for display_begin
        $self->{achs} = [ map {{ name => $_ }} 
                          keys %{$self->{ach_list}}
                        ];
    }

    unless (open(FP, ">$self->{config_file}"))
    {
        return $self->error("$self->{config_file} : $!\n" .
                            "You must add this to your config file\n" 
                            . Data::Dumper::Dumper($self));
    }

    print FP Data::Dumper::Dumper($self);
    
    close(FP);       
    return 1;
}

=head1 FUNCTIONS
    
    edit, view, modify - html form ouput

=cut

sub edit
{
    my ($self) = @_ ;

    $self->display($self, "config_edit.tpl");
}

sub view
{
    my ($self) = @_ ;
    $self->display($self, "config_view.tpl");
}

sub modify
{
    my ($self) = @_;
    
    $self->{error} = '';
    # we need to reset checkbox first
    $self->{debug} = 0;
    $self->{display_log_time} = 0;
    $self->{enable_security} = 0;
    $self->{enable_security_acl} = 0;

    foreach my $k (CGI::param())
    {
        next unless (exists $k_re{$k}) ;
        my $val = CGI::param($k);
        if ($val =~ $k_re{$k}) {
            $self->{$k} = $1;
        } else {
            $self->{error} .= "bad parameter : $k = [$val]";
        }
    }

    $self->view();

    if ($self->{error}) {       # an error as occured
        $self->display($self, 'error.tpl');
    } else {
        $self->save();
    }
}

1;

################################################################

package Bweb::Client;

use base q/Bweb::Gui/;

=head1 PACKAGE
    
    Bweb::Client - Bacula FD

=head2 DESCRIPTION

    this package is use to do all Client operations like, parse status etc...

=head2 USAGE

    $client = new Bweb::Client(name => 'zog-fd');
    $client->status();            # do a 'status client=zog-fd'

=cut

=head1 FUNCTION

    display_running_job - Html display of a running job

=head2 DESCRIPTION

    this function is used to display information about a current job

=cut

sub display_running_job
{
    my ($self, $bweb, $jobid, $infos) = @_ ;
    my $status = $self->status($bweb->{info});

    if ($jobid) {
        if ($status->{$jobid}) {
            $status = $status->{$jobid};
            $status->{last_jobbytes} = $infos->{jobbytes};
            $status->{last_jobfiles} = $infos->{jobfiles};
            $status->{corr_jobbytes} = $infos->{corr_jobbytes};
            $status->{corr_jobfiles} = $infos->{corr_jobfiles};
            $status->{jobbytes}=$status->{Bytes}; 
            $status->{jobbytes} =~ s![^\d]!!g;
            $status->{jobfiles}=$status->{'Files Examined'}; 
            $status->{jobfiles} =~ s/,//g;
            $bweb->display($status, "client_job_status.tpl");
        }
    } else {
        for my $id (keys %$status) {
            $bweb->display($status->{$id}, "client_job_status.tpl");
        }
    }
}

=head1 FUNCTION

    $client = new Bweb::Client(name => 'plume-fd');
                               
    $client->status($bweb);

=head2 DESCRIPTION

    dirty hack to parse "status client=xxx-fd"

=head2 INPUT

   JobId 105 Job Full_plume.2006-06-06_17.22.23 is running.
       Backup Job started: 06-jun-06 17:22
       Files=8,971 Bytes=194,484,132 Bytes/sec=7,480,158
       Files Examined=10,697
       Processing file: /home/eric/.openoffice.org2/user/config/standard.sod
       SDReadSeqNo=5 fd=5
   
=head2 OUTPUT

    $VAR1 = { 105 => {
                JobName => Full_plume.2006-06-06_17.22.23,
                JobId => 105,
                Files => 8,971,
                Bytes => 194,484,132,
                ...
              },
              ...
    };

=cut

sub status
{
    my ($self, $conf) = @_ ;

    if (defined $self->{cur_jobs}) {
        return $self->{cur_jobs} ;
    }

    my $arg = {};
    my $b = new Bconsole(pref => $conf);
    my $ret = $b->send_cmd("st client=$self->{name}");
    my @param;
    my $jobid;

    for my $r (split(/\n/, $ret)) {
        chomp($r);
        $r =~ s/(^\s+|\s+$)//g;
        if ($r =~ /JobId (\d+) Job (\S+)/) {
            if ($jobid) {
                $arg->{$jobid} = { @param, JobId => $jobid } ;
            }

            $jobid = $1;
            @param = ( JobName => $2 );

        } elsif ($r =~ /=.+=/) {
            push @param, split(/\s+|\s*=\s*/, $r) ;

        } elsif ($r =~ /=/) {   # one per line
            push @param, split(/\s*=\s*/, $r) ;

        } elsif ($r =~ /:/) {   # one per line
            push @param, split(/\s*:\s*/, $r, 2) ;
        }
    }

    if ($jobid and @param) {
        $arg->{$jobid} = { @param,
                           JobId => $jobid, 
                           Client => $self->{name},
                       } ;
    }

    $self->{cur_jobs} = $arg ;

    return $arg;
}
1;

################################################################

package Bweb::Autochanger;

use base q/Bweb::Gui/;

=head1 PACKAGE
    
    Bweb::Autochanger - Object to manage Autochanger

=head2 DESCRIPTION

    this package will parse the mtx output and manage drives.

=head2 USAGE

    $auto = new Bweb::Autochanger(precmd => 'sudo');
    or
    $auto = new Bweb::Autochanger(precmd => 'ssh root@robot');
                                  
    $auto->status();

    $auto->slot_is_full(10);
    $auto->transfer(10, 11);

=cut

sub new
{
    my ($class, %arg) = @_;

    my $self = bless {
        name  => '',    # autochanger name
        label => {},    # where are volume { label1 => 40, label2 => drive0 }
        drive => [],    # drive use [ 'media1', 'empty', ..]
        slot  => [],    # slot use [ undef, 'empty', 'empty', ..] no slot 0
        io    => [],    # io slot number list [ 41, 42, 43...]
        info  => {slot => 0,    # informations (slot, drive, io)
                  io   => 0,
                  drive=> 0,
                 },
        mtxcmd => '/usr/sbin/mtx',
        debug => 0,
        device => '/dev/changer',
        precmd => '',   # ssh command
        bweb => undef,  # link to bacula web object (use for display) 
    } ;

    map { $self->{lc($_)} = $arg{$_} } keys %arg ;

    return $self;
}

=head1 FUNCTION

    status - parse the output of mtx status

=head2 DESCRIPTION

    this function will launch mtx status and parse the output. it will
    give a perlish view of the autochanger content.

    it uses ssh if the autochanger is on a other host.

=cut

sub status
{
    my ($self) = @_;
    my @out = `$self->{precmd} $self->{mtxcmd} -f $self->{device} status` ;

    # TODO : reset all infos
    $self->{info}->{drive} = 0;
    $self->{info}->{slot}  = 0;
    $self->{info}->{io}    = 0;

    #my @out = `cat /home/eric/travail/brestore/plume/mtx` ;

#
#  Storage Changer /dev/changer:2 Drives, 45 Slots ( 5 Import/Export )
#Data Transfer Element 0:Full (Storage Element 1 Loaded):VolumeTag = 000000
#Data Transfer Element 1:Empty
#      Storage Element 1:Empty
#      Storage Element 2:Full :VolumeTag=000002
#      Storage Element 3:Empty
#      Storage Element 4:Full :VolumeTag=000004
#      Storage Element 5:Full :VolumeTag=000001
#      Storage Element 6:Full :VolumeTag=000003
#      Storage Element 7:Empty
#      Storage Element 41 IMPORT/EXPORT:Empty
#      Storage Element 41 IMPORT/EXPORT:Full :VolumeTag=000002
#

    for my $l (@out) {

        #          Storage Element 7:Empty
        #          Storage Element 2:Full :VolumeTag=000002
        if ($l =~ /Storage Element (\d+):(Empty|Full)(\s+:VolumeTag=([\w\d.-]+))?/){

            if ($2 eq 'Empty') {
                $self->set_empty_slot($1);
            } else {
                $self->set_slot($1, $4);
            }

        } elsif ($l =~ /Data Transfer.+(\d+):(Full|Empty)(\s+.Storage Element (\d+) Loaded.(:VolumeTag = ([\w\d.-]+))?)?/) {

            if ($2 eq 'Empty') {
                $self->set_empty_drive($1);
            } else {
                $self->set_drive($1, $4, $6);
            }

        } elsif ($l =~ /Storage Element (\d+).+IMPORT\/EXPORT:(Empty|Full)( :VolumeTag=([\d\w.-]+))?/) 
        {
            if ($2 eq 'Empty') {
                $self->set_empty_io($1);
            } else {
                $self->set_io($1, $4);
            }

#       Storage Changer /dev/changer:2 Drives, 30 Slots ( 1 Import/Export )

        } elsif ($l =~ /Storage Changer .+:(\d+) Drives, (\d+) Slots/) {
            $self->{info}->{drive} = $1;
            $self->{info}->{slot} = $2;
            if ($l =~ /(\d+)\s+Import/) {
                $self->{info}->{io} = $1 ;
            } else {
                $self->{info}->{io} = 0;
            }
        } 
    }

    $self->debug($self) ;
}

sub is_slot_loaded
{
    my ($self, $slot) = @_;

    # no barcodes
    if ($self->{slot}->[$slot] eq 'loaded') {
        return 1;
    } 

    my $label = $self->{slot}->[$slot] ;

    return $self->is_media_loaded($label);
}

sub unload
{
    my ($self, $drive, $slot) = @_;

    return 0 if (not defined $drive or $self->{drive}->[$drive] eq 'empty') ;
    return 0 if     ($self->slot_is_full($slot)) ;

    my $out = `$self->{precmd} $self->{mtxcmd} -f $self->{device} unload $slot $drive 2>&1`;
    
    if ($? == 0) {
        my $content = $self->get_slot($slot);
        print "content = $content<br/> $drive => $slot<br/>";
        $self->set_empty_drive($drive);
        $self->set_slot($slot, $content);
        return 1;
    } else {
        $self->{error} = $out;
        return 0;
    }
}

# TODO: load/unload have to use mtx script from bacula
sub load
{
    my ($self, $drive, $slot) = @_;

    return 0 if (not defined $drive or $self->{drive}->[$drive] ne 'empty') ;
    return 0 unless ($self->slot_is_full($slot)) ;

    print "Loading drive $drive with slot $slot<br/>\n";
    my $out = `$self->{precmd} $self->{mtxcmd} -f $self->{device} load $slot $drive 2>&1`;
    
    if ($? == 0) {
        my $content = $self->get_slot($slot);
        print "content = $content<br/> $slot => $drive<br/>";
        $self->set_drive($drive, $slot, $content);
        return 1;
    } else {
        $self->{error} = $out;
        print $out;
        return 0;
    }
}

sub is_media_loaded
{
    my ($self, $media) = @_;

    unless ($self->{label}->{$media}) {
        return 0;
    }

    if ($self->{label}->{$media} =~ /drive\d+/) {
        return 1;
    }

    return 0;
}

sub have_io
{
    my ($self) = @_;
    return (defined $self->{info}->{io} and $self->{info}->{io} > 0);
}

sub set_io
{
    my ($self, $slot, $tag) = @_;
    $self->{slot}->[$slot] = $tag || 'full';
    push @{ $self->{io} }, $slot;

    if ($tag) {
        $self->{label}->{$tag} = $slot;
    } 
}

sub set_empty_io
{
    my ($self, $slot) = @_;

    push @{ $self->{io} }, $slot;

    unless ($self->{slot}->[$slot]) {       # can be loaded (parse before) 
        $self->{slot}->[$slot] = 'empty';
    }
}

sub get_slot
{
    my ($self, $slot) = @_;
    return $self->{slot}->[$slot];
}

sub set_slot
{
    my ($self, $slot, $tag) = @_;
    $self->{slot}->[$slot] = $tag || 'full';

    if ($tag) {
        $self->{label}->{$tag} = $slot;
    }
}

sub set_empty_slot
{
    my ($self, $slot) = @_;

    unless ($self->{slot}->[$slot]) {       # can be loaded (parse before) 
        $self->{slot}->[$slot] = 'empty';
    }
}

sub set_empty_drive
{
    my ($self, $drive) = @_;
    $self->{drive}->[$drive] = 'empty';
}

sub set_drive
{
    my ($self, $drive, $slot, $tag) = @_;
    $self->{drive}->[$drive] = $tag || $slot;
    $self->{drive_slot}->[$drive] = $slot;

    $self->{slot}->[$slot] = $tag || 'loaded';

    if ($tag) {
        $self->{label}->{$tag} = "drive$drive";
    }
}

sub slot_is_full
{
    my ($self, $slot) = @_;
    
    # slot don't exists => full
    if (not defined $self->{slot}->[$slot]) {
        return 0 ;
    }

    if ($self->{slot}->[$slot] eq 'empty') {
        return 0;
    }
    return 1;                   # vol, full, loaded
}

sub slot_get_first_free
{
    my ($self) = @_;
    for (my $slot=1; $slot < $self->{info}->{slot}; $slot++) {
        return $slot unless ($self->slot_is_full($slot));
    }
}

sub io_get_first_free
{
    my ($self) = @_;
    
    foreach my $slot (@{ $self->{io} }) {
        return $slot unless ($self->slot_is_full($slot));       
    }
    return 0;
}

sub get_media_slot
{
    my ($self, $media) = @_;

    return $self->{label}->{$media} ;    
}

sub have_media
{
    my ($self, $media) = @_;

    return defined $self->{label}->{$media} ;    
}

sub send_to_io
{
    my ($self, $slot) = @_;

    unless ($self->slot_is_full($slot)) {
        print "Autochanger $self->{name} slot $slot is empty<br>\n";
        return 1;               # ok
    }

    # first, eject it
    if ($self->is_slot_loaded($slot)) {
        # bconsole->umount
        # self->eject
        print "Autochanger $self->{name} $slot is currently in use<br>\n";
        return 0;
    }

    # autochanger must have I/O
    unless ($self->have_io()) {
        print "Autochanger $self->{name} don't have I/O, you can take media yourself<br>\n";
        return 0;
    }

    my $dst = $self->io_get_first_free();

    unless ($dst) {
        print "Autochanger $self->{name} mailbox is full, you must empty I/O first<br>\n";
        return 0;
    }

    $self->transfer($slot, $dst);
}

sub transfer
{
    my ($self, $src, $dst) = @_ ;
    if ($self->{debug}) {
        print "<pre>$self->{precmd} $self->{mtxcmd} -f $self->{device} transfer $src $dst</pre>\n";
    }
    my $out = `$self->{precmd} $self->{mtxcmd} -f $self->{device} transfer $src $dst 2>&1`;
    
    if ($? == 0) {
        my $content = $self->get_slot($src);
        $self->{slot}->[$src] = 'empty';
        $self->set_slot($dst, $content);
        return 1;
    } else {
        $self->{error} = $out;
        return 0;
    }
}

sub get_drive_name
{
    my ($self, $index) = @_;
    return $self->{drive_name}->[$index];
}

# TODO : do a tapeinfo request to get informations
sub tapeinfo
{
    my ($self) = @_;
}

sub clear_io
{
    my ($self) = @_;

    print "<table><tr>\n";
    for my $slot (@{$self->{io}})
    {
        if ($self->is_slot_loaded($slot)) {
            print "<td></td><td>Slot $slot is currently loaded</td></tr>\n";
            next;
        }

        if ($self->slot_is_full($slot))
        {
            my $free = $self->slot_get_first_free() ;
            print "</tr><tr><td>move slot $slot to $free :</td>";

            if ($free) {
                if ($self->transfer($slot, $free)) {
                    print "<td><img src='/bweb/T.png' alt='ok'></td>\n";
                } else {
                    print "<td><img src='/bweb/E.png' alt='ok' title='$self->{error}'></td>\n";
                }
                
            } else {
                $self->{error} = "<td><img src='/bweb/E.png' alt='ok' title='E : Can t find free slot'></td>\n";
            }
        }
    }
    print "</tr></table>\n";
}

# TODO : this is with mtx status output,
# we can do an other function from bacula view (with StorageId)
sub display_content
{
    my ($self) = @_;
    my $bweb = $self->{bweb};

    # $self->{label} => ('vol1', 'vol2', 'vol3', ..);
    my $media_list = $bweb->dbh_join( keys %{ $self->{label} });

    my $query="
SELECT Media.VolumeName  AS volumename,
       Media.VolStatus   AS volstatus,
       Media.LastWritten AS lastwritten,
       Media.VolBytes    AS volbytes,
       Media.MediaType   AS mediatype,
       Media.Slot        AS slot,
       Media.InChanger   AS inchanger,
       Pool.Name         AS name,
       $bweb->{sql}->{FROM_UNIXTIME}(
          $bweb->{sql}->{UNIX_TIMESTAMP}(Media.LastWritten) 
        + $bweb->{sql}->{TO_SEC}(Media.VolRetention)
       ) AS expire
FROM Media 
 INNER JOIN Pool USING (PoolId) 

WHERE Media.VolumeName IN ($media_list)
";

    my $all = $bweb->dbh_selectall_hashref($query, 'volumename') ;

    # TODO : verify slot and bacula slot
    my $param = [];
    my @to_update;

    for (my $slot=1; $slot <= $self->{info}->{slot} ; $slot++) {

        if ($self->slot_is_full($slot)) {

            my $vol = $self->{slot}->[$slot];
            if (defined $all->{$vol}) {    # TODO : autochanger without barcodes 

                my $bslot = $all->{$vol}->{slot} ;
                my $inchanger = $all->{$vol}->{inchanger};

                # if bacula slot or inchanger flag is bad, we display a message
                if ($bslot != $slot or !$inchanger) {
                    push @to_update, $slot;
                }
                
                $all->{$vol}->{realslot} = $slot;

                push @{ $param }, $all->{$vol};

            } else {            # empty or no label
                push @{ $param }, {realslot => $slot,
                                   volstatus => 'Unknown',
                                   volumename => $self->{slot}->[$slot]} ;
            }
        } else {                # empty
            push @{ $param }, {realslot => $slot, volumename => 'empty'} ;
        }
    }

    my $i=0; my $drives = [] ;
    foreach my $d (@{ $self->{drive} }) {
        $drives->[$i] = { index => $i,
                          load  => $self->{drive}->[$i],
                          name  => $self->{drive_name}->[$i],
                      };
        $i++;
    }

    $bweb->display({ Name   => $self->{name},
                     nb_drive => $self->{info}->{drive},
                     nb_io => $self->{info}->{io},
                     Drives => $drives,
                     Slots  => $param,
                     Update => scalar(@to_update) },
                   'ach_content.tpl');

}

1;

################################################################

package Bweb::Sched;
use base q/Bweb::Gui/;

=head1 PACKAGE

    Bweb::Sched() - Bweb package that parse show schedule ouput

    new Bweb::Sched(format => '%Y-%m-%d', year => 2008);

=head2 USAGE

 my $b = $bweb->get_bconsole();
 my $s = $b->send_cmd("show schedule");
 my $sched = new Bweb::Sched(begin => '2007-01-01', end => '2007-01-02 12:00');
 $sched->parse_scheds(split(/\r?\n/, $s));


$VAR1 = {
          'event' => [
                       '2007-01-04 04:05',
                       '2007-12-05 04:05',
                       '2007-12-12 04:05',
                       '2007-12-19 04:05',
                       '2007-12-26 04:05'
                     ],
          'level' => 'Differential',
          'pool' => 'Semaine'
        };
=cut

sub new
{
    my ($class, @arg) = @_;
    my $self = $class->SUPER::new(@arg);

    # we compare the current schedule date with begin and end
    # in a float form ex: 20071212.1243 > 20070101
    if ($self->{begin} and $self->{end}) {
        ($self->{fbegin}, $self->{fend}) = ($self->{begin}, $self->{end});
        $self->{fbegin} =~ s/(-|:)//g;  $self->{fbegin} =~ s/ /./;
        $self->{fend}   =~ s/(-|:)//g;  $self->{fend}   =~ s/ /./;
    } 

    bless($self,$class);

    if ($self->{bconsole}) {
        my $sel = $self->{name}?"=\"$self->{name}\"":'';
        my $b = $self->{bconsole};
        my $out = $b->send_cmd("show schedule$sel");
        $self->parse_scheds(split(/\r?\n/, $out));
        undef $self->{bconsole}; # useless now
    }

    return $self;
}

# cleanup and add a schedule
sub add_sched
{
    my ($self, $name, $info) = @_;
    # bacula uses dates that start from 0, we start from 1
    foreach (@{$info->{mday}},@{$info->{month}}) { $_++ };

    # get events
    $info->{event} = [ $self->get_events($info, $self->{format}) ];

    foreach my $i (qw/hour mday month wday wom woy mins/) {
        delete $info->{$i};
    }

    push @{$self->{schedules}->{$name}}, $info;
}

# return the name of all schedules
sub list_scheds
{
    my ($self, $name) = @_;
    my %ret;
    return keys %{ $self->{schedules} };
}

# return an array of all schedule
sub get_scheds
{
    my ($self, $sched) = @_;
    return $self->{schedules}->{$sched};
}

# return an ref array of all events
# [ '2007-12-19 04:05', '2007-12-19 04:05' ]
sub get_event
{
    my ($self, $sched) = @_;
    return $sched->{event};
}

# return the pool name
sub get_pool
{
    my ($self, $sched) = @_;
    return $sched->{pool} || '';
}

# return the level name (Incremental, Differential, Full)
sub get_level
{
    my ($self, $sched) = @_;
    return $sched->{level};
}

# parse bacula sched bitmap
sub parse_scheds
{
    my ($self, @output) = @_;

    my $run;
    my $name;
    my $schedule;
    my $elt;

    foreach my $ligne (@output) {
        if ($ligne =~ /Schedule: name=(.+)/) {
            if ($name and $elt) {
                $elt->{level} = $run;
                $self->add_sched($name, $elt);
                $elt=undef;
            }
            $name = $1;
        }
        elsif ($ligne =~ /Run Level=(.+)/) {
            if ($name and $elt) {
                $elt->{level} = $run;
                $self->add_sched($name, $elt);
                $elt=undef;
            }
            $run = $1;
        }
        elsif ($ligne =~ /(hour|mday|month|mins)=(.+)/) {
            # All theses lines have the same format
            
            my ($k,$v) = ($1,$2);
            # we get all values (0 1 4 9)
            $elt->{$k}=[split (/\s/,$v)];
        }
        # we make a bitmap for this kind of data (0 0 1 0 0 0 1) for a quick access
        elsif ($ligne =~ /(wday|wom|woy)=(.+)/) {
            my ($k,$v) = ($1,$2);
            foreach my $e (split (/\s/,$v)) {
                $elt->{$k}->[$e]=1;
            }
        }
        elsif ($ligne =~ /Pool: name=(.+?) PoolType/) {
            $elt->{pool} = $1;
        }
    }

    if ($name and $elt) {
        $elt->{level} = $run;
        $self->add_sched($name, $elt);
    }
}

use Date::Calc qw(:all);

# read bacula schedule bitmap and get $format date string
sub get_events
{
    use integer;
    my ($self, $s,$format) = @_;
    my $year = $self->{year} || ((localtime)[5] + 1900);
    $format = $format || '%u-%02u-%02u %02u:%02u';
    my @ret;
    foreach my $m (@{$s->{month}})              # mois de l'annee
    {
        foreach my $md (@{$s->{mday}})  # jour du mois
        {
#           print "  m=$m md=$md\n";
            # we check if this day exists (31 fev)
            next if (!check_date($year,$m,$md));
#           print "    check_date ok\n";

            my $w = ($md-1)/7; # we use the same thing than bacula
            next if (! $s->{wom}->[$w]);
#           print "      wom ok\n";

            # on recupere le jour de la semaine 
            my $wd = Day_of_Week($year,$m,$md);

            my ($w1,undef) = Week_of_Year($year,$m,$wd);
            next if (! $s->{woy}->[$w1-1]); # bacula 0-51
#           print "        woy ok\n";

            $wd = 0 if ($wd == 7) ; # sunday is 0 on bacula
            next if (! $s->{wday}->[$wd]);
#           print "          wday ok\n";

            foreach my $h (@{$s->{hour}}) # hour of the day
            {
                foreach my $min (@{$s->{mins}}) # minute
                {
                    if ($self->{fbegin}) {
                        no integer;
                        my $d = sprintf('%d%0.2d%0.2d.%0.2d%0.2d',
                                        $year,$m,$md,$h,$min);
                        next if ($d < $self->{fbegin} or $d > $self->{fend});
                    }
                    push @ret, sprintf($format, $year,$m,$md,$h,$min);
                }
            }
        }
    }
    return @ret;
}
1;

################################################################

package Bweb;

use base q/Bweb::Gui/;

=head1 PACKAGE

    Bweb - main Bweb package

=head2

    this package is use to compute and display informations

=cut

use DBI;
use POSIX qw/strftime/;

our $config_file= '/etc/bacula/bweb.conf';

if ($ENV{BWEBCONF} && -f $ENV{BWEBCONF}) {
    $config_file = $ENV{BWEBCONF};
}

our $cur_id=0;

=head1 VARIABLE

    %sql_func - hash to make query mysql/postgresql compliant

=cut

our %sql_func = ( 
          Pg => { 
              UNIX_TIMESTAMP => '',
              FROM_UNIXTIME => '',
              TO_SEC => " interval '1 second' * ",
              SEC_TO_INT => "SEC_TO_INT",
              SEC_TO_TIME => '',
              MATCH => " ~* ",
              STARTTIME_SEC  => " date_trunc('sec', Job.StartTime) ",
              STARTTIME_DAY  => " date_trunc('day', Job.StartTime) ",
              STARTTIME_HOUR => " date_trunc('hour', Job.StartTime) ",
              STARTTIME_MONTH  => " date_trunc('month', Job.StartTime) ",
              STARTTIME_WEEK => " date_trunc('week', Job.StartTime) ",
              STARTTIME_PHOUR=> " date_part('hour', Job.StartTime) ",
              STARTTIME_PDAY => " date_part('day', Job.StartTime) ",
              STARTTIME_PMONTH => " date_part('month', Job.StartTime) ",
              STARTTIME_PWEEK => " date_part('week', Job.StartTime) ",
              DB_SIZE => " SELECT pg_database_size(current_database()) ",
              CAT_POOL_TYPE => " MediaType || '_' || Pool.Name ",
              CONCAT_SEP => "",
          },
          mysql => {
              UNIX_TIMESTAMP => 'UNIX_TIMESTAMP',
              FROM_UNIXTIME => 'FROM_UNIXTIME',
              SEC_TO_INT => '',
              TO_SEC => '',
              SEC_TO_TIME => 'SEC_TO_TIME',
              MATCH => " REGEXP ",
              STARTTIME_SEC => " DATE_FORMAT(Job.StartTime, '%Y-%m-%d %T') ",
              STARTTIME_DAY  => " DATE_FORMAT(Job.StartTime, '%Y-%m-%d') ",
              STARTTIME_HOUR => " DATE_FORMAT(Job.StartTime, '%Y-%m-%d %H') ",
              STARTTIME_MONTH => " DATE_FORMAT(Job.StartTime, '%Y-%m') ",
              STARTTIME_WEEK => " DATE_FORMAT(Job.StartTime, '%Y-%v') ",
              STARTTIME_PHOUR=> " DATE_FORMAT(Job.StartTime, '%H') ",
              STARTTIME_PDAY => " DATE_FORMAT(Job.StartTime, '%d') ",
              STARTTIME_PMONTH => " DATE_FORMAT(Job.StartTime, '%m') ",
              STARTTIME_PWEEK => " DATE_FORMAT(Job.StartTime, '%v') ",
              # with mysql < 5, you have to play with the ugly SHOW command
              #DB_SIZE => " SELECT 0 ",
              # works only with mysql 5
              DB_SIZE => " SELECT sum(DATA_LENGTH) FROM INFORMATION_SCHEMA.TABLES ",
              CAT_POOL_TYPE => " CONCAT(MediaType,'_',Pool.Name) ",
              CONCAT_SEP => " SEPARATOR '' ",
          },
         );

sub dbh_is_mysql
{
    my ($self) = @_;
    return $self->{info}->{dbi} =~ /dbi:mysql/i;
}

sub dbh_disconnect
{
    my ($self) = @_;
    if ($self->{dbh}) {
       $self->{dbh}->disconnect();
       undef $self->{dbh};
    }
}

sub dbh_selectall_arrayref
{
    my ($self, $query) = @_;
    $self->connect_db();
    $self->debug($query);
    return $self->{dbh}->selectall_arrayref($query);
}

sub dbh_join
{
    my ($self, @what) = @_;
    return join(',', $self->dbh_quote(@what)) ;
}

sub dbh_quote
{
    my ($self, @what) = @_;

    $self->connect_db();
    if (wantarray) {
        return map { $self->{dbh}->quote($_) } @what;
    } else {
        return $self->{dbh}->quote($what[0]) ;
    }
}

sub dbh_do
{
    my ($self, $query) = @_ ; 
    $self->connect_db();
    $self->debug($query);
    return $self->{dbh}->do($query);
}

sub dbh_selectall_hashref
{
    my ($self, $query, $join) = @_;
    
    $self->connect_db();
    $self->debug($query);
    return $self->{dbh}->selectall_hashref($query, $join) ;
}

sub dbh_selectrow_hashref
{
    my ($self, $query) = @_;
    
    $self->connect_db();
    $self->debug($query);
    return $self->{dbh}->selectrow_hashref($query) ;
}

sub dbh_strcat
{
    my ($self, @what) = @_;
    if ($self->dbh_is_mysql()) {
        return 'CONCAT(' . join(',', @what) . ')' ;
    } else {
        return join(' || ', @what);
    }
}

sub dbh_prepare
{
    my ($self, $query) = @_;
    $self->debug($query, up => 1);
    return $self->{dbh}->prepare($query);    
}

# display Mb/Gb/Kb
sub human_size
{
    my @unit = qw(B KB MB GB TB);
    my $val = shift || 0;
    my $i=0;
    my $format = '%i %s';
    while ($val / 1024 > 1) {
        $i++;
        $val /= 1024;
    }
    $format = ($i>0)?'%0.1f %s':'%i %s';
    return sprintf($format, $val, $unit[$i]);
}

sub human_sec_unit
{
    my $val = shift;
    my $orig = $val;
    if ($val =~ /^(?:\d+(?:[ywdhms]\s*)?)+\s*$/) {
        for ($val) {
            s/y/*52w/g;
            s/w/*7d/g;
            s/d/*24h/g;
            s/h/*60m/g;
            s/m/*60s/g;
            s/s(\d)/+$1/g;
            s/s//;
        }
        $val = eval $val;
    }
    return int($val);
}

# display Day, Hour, Year
sub human_sec
{
    use integer;

    my $val = shift;
    $val /= 60;                 # sec -> min

    if ($val / 60 <= 1) {
        return "$val mins";
    } 

    $val /= 60;                 # min -> hour
    if ($val / 24 <= 1) {
        return "$val hours";
    } 

    $val /= 24;                 # hour -> day
    if ($val / 365 < 2) {
        return "$val days";
    } 

    $val /= 365 ;               # day -> year

    return "$val years";   
}

# display Enabled
sub human_enabled
{
    my $val = shift || 0;

    if ($val eq '1' or $val eq "yes") {
        return "yes";
    } elsif ($val eq '2' or $val eq "archived") {
        return "archived";
    } else {
        return  "no";
    }
}

# display Enabled
sub from_human_enabled
{
    my $val = shift || 0;

    if ($val eq '1' or $val eq "yes") {
        return 1;
    } elsif ($val eq '2' or $val eq "archived") {
        return 2;
    } else {
        return  0;
    }
}

# get Day, Hour, Year
sub from_human_sec
{
    use integer;

    my $val = shift;
    unless ($val =~ /^\s*(\d+)\s*(\w)\w*\s*$/) {
        return 0;
    }

    my %times = ( m   => 60,
                  h   => 60*60,
                  d   => 60*60*24,
                  m   => 60*60*24*31,
                  y   => 60*60*24*365,
                  );
    my $mult = $times{$2} || 0;

    return $1 * $mult;   
}

# get long term statistic table
sub get_stat_table
{
    my ($self) = @_;
    my $ret = $self->{info}->{stat_job_table} || 'JobHisto';
    if ($ret !~ m/^job$/i) {
        $ret = "(SELECT * FROM Job UNION SELECT * FROM $ret)";
    }
    return $ret;
}

sub connect_db
{
    my ($self) = @_;

    unless ($self->{dbh}) {

        $self->{dbh} = DBI->connect($self->{info}->{dbi}, 
                                    $self->{info}->{user},
                                    $self->{info}->{password});

        $self->error("Can't connect to your database:\n$DBI::errstr\n")
            unless ($self->{dbh});

        $self->{dbh}->{FetchHashKeyName} = 'NAME_lc';

        if ($self->dbh_is_mysql()) {
            $self->{dbh}->do("SET group_concat_max_len=1000000");
        } else {
            $self->{dbh}->do("SET datestyle TO 'ISO, YMD'");
        }
    }
}

sub new
{
    my ($class, %arg) = @_;
    my $self = bless ({ 
        dbh => undef,           # connect_db();
        info => {
            dbi   => '', # DBI:Pg:database=bacula;host=127.0.0.1
            user  => 'bacula',
            password => 'test', 
        },
    },$class) ;

    map { $self->{lc($_)} = $arg{$_} } keys %arg ;

    if ($self->{info}->{dbi} =~ /DBI:(\w+):/i) {
        $self->{sql} = $sql_func{$1};
    }

    $self->{loginname} = CGI::remote_user();
    $self->{debug} = $self->{info}->{debug};
    $self->{lang} = $self->{info}->{lang};
    $self->{template_dir} = $self->{info}->{template_dir};

    return $self;
}

sub display_begin
{
    my ($self) = @_;
    if ($self->{info}->{enable_security}) {
        $self->get_roles();     # get lang
    }
    $self->display($self->{info}, "begin.tpl");
}

sub display_end
{
    my ($self) = @_;
    $self->display($self->{info}, "end.tpl");
}

sub client_edit
{
    my ($self) = @_;
    my $arg = $self->get_form("qclient");
    my $f1 = $self->get_client_group_filter();
    my $f2 = $self->get_client_filter();
 
# client_group_name | here 
#-------------------+-----
#      GROUP1       | 1
#      GRP3         | 0

    my $query = "
SELECT client_group_name, max(here) AS here FROM (
  SELECT client_group_name, 1 AS here
    FROM client_group
    JOIN client_group_member USING (client_group_id)
    JOIN Client USING (ClientId) $f2
   WHERE Name = $arg->{qclient}
  UNION ALL
  SELECT client_group_name, 0 
    FROM client_group $f1
) AS temp
GROUP by client_group_name";

    my $all = $self->dbh_selectall_hashref($query, 'client_group_name');
    
    $self->display({ client_group => [ values %$all ], %$arg }, "client_edit.tpl");
}

sub display_clients
{
    my ($self) = @_;
    my $where='';       # by default

    my $arg = $self->get_form("client", "qre_client", 
                              "jclient_groups", "qnotingroup");

    if ($arg->{qre_client}) {
        $where = "WHERE Name $self->{sql}->{MATCH} $arg->{qre_client} ";
    } elsif ($arg->{client}) {
        $where = "WHERE Name = '$arg->{client}' ";
    } elsif ($arg->{jclient_groups}) {
        # $filter could already contains client_group_member 
        $where = "
 JOIN client_group_member USING (ClientId) 
 JOIN client_group USING (client_group_id)
 WHERE client_group_name IN ($arg->{jclient_groups}) ";
    } elsif ($arg->{qnotingroup}) {
        $where =   "
  WHERE NOT EXISTS
   (SELECT 1 FROM client_group_member
     WHERE Client.ClientId = client_group_member.ClientId
   )
";
    }

    my $query = "
SELECT Name   AS name,
       Uname  AS uname,
       AutoPrune AS autoprune,
       FileRetention AS fileretention,
       JobRetention  AS jobretention
FROM Client " . $self->get_client_filter() .
$where ;

    my $all = $self->dbh_selectall_hashref($query, 'name') ;

    my $dsp = { ID => $cur_id++,
                clients => [ values %$all] };

    $self->display($dsp, "client_list.tpl") ;
}

sub get_limit
{
    my ($self, %arg) = @_;

    my $limit = '';
    my $label = '';

    if ($arg{since} and $arg{age}) {
        my $now = "$self->{sql}->{UNIX_TIMESTAMP}(TIMESTAMP '$arg{since}')";
        $limit .= "
 AND $self->{sql}->{UNIX_TIMESTAMP}(StartTime) > $now 
 AND $self->{sql}->{UNIX_TIMESTAMP}(EndTime) < ($now + $self->{sql}->{TO_SEC}($arg{age}))";
        $label .= "since $arg{since} and during " . human_sec($arg{age});

    } elsif ($arg{age}) {
        $limit .=
  "AND $self->{sql}->{UNIX_TIMESTAMP}(EndTime)
         >
       ( $self->{sql}->{UNIX_TIMESTAMP}(NOW())
         -
         $self->{sql}->{TO_SEC}($arg{age})
       )" ;

        $label = "last " . human_sec($arg{age});
    }

    if ($arg{groupby}) {
        $limit .= " GROUP BY $arg{groupby} ";
    }

    if ($arg{order}) {
        $limit .= " ORDER BY $arg{order} ";
    }

    if ($arg{limit}) {
        $limit .= " LIMIT $arg{limit} ";
        $label .= " limited to $arg{limit}";
    }

    if ($arg{offset}) {
        $limit .= " OFFSET $arg{offset} ";
        $label .= " with $arg{offset} offset ";
    }

    unless ($label) {
        $label = 'no filter';
    }

    return ($limit, $label);
}

=head1 FUNCTION

    $bweb->get_form(...) - Get useful stuff

=head2 DESCRIPTION

    This function get and check parameters against regexp.
    
    If word begin with 'q', the return will be quoted or join quoted
    if it's end with 's'.
    

=head2 EXAMPLE

    $bweb->get_form('jobid', 'qclient', 'qpools') ;

    { jobid    => 12,
      qclient  => 'plume-fd',
      qpools   => "'plume-fd', 'test-fd', '...'",
    }

=cut

sub get_form
{
    my ($self, @what) = @_;
    my %what = map { $_ => 1 } @what;
    my %ret;

    my %opt_i = (
                 limit  => 100,
                 cost   =>  10,
                 offset =>   0,
                 start  =>   0,
                 width  => 640,
                 height => 480,
                 jobid  =>   0,
                 slot   =>   0,
                 drive  =>   0,
                 priority => 10,
                 age    => $self->{info}->{default_age},
                 days   => 1,
                 maxvoljobs  => 0,
                 maxvolbytes => 0,
                 maxvolfiles => 0,
                 filenameid => 0,
                 pathid => 0,
                 nb => 0,
                 );

    my %opt_ss =(               # string with space
                 job     => 1,
                 storage => 1,
                 );
    my %opt_s = (               # default to ''
                 ach    => 1,
                 status => 1,
                 volstatus => 1,
                 inchanger => 1,
                 client => 1,
                 level  => 1,
                 pool   => 1,
                 media  => 1,
                 ach    => 1,
                 jobtype=> 1,
                 graph  => 1,
                 gtype  => 1,
                 type   => 1,
                 poolrecycle => 1,
                 replace => 1,
                 expired => 1,
                 enabled => 1,
                 username => 1,
                 rolename => 1,
                 storage_cmd => 1,
                 );
    my %opt_p = (               # option with path
                 fileset=> 1,
                 mtxcmd => 1,
                 precmd => 1,
                 device => 1,
                 where  => 1,
                 );
    my %opt_r = (regexwhere => 1);
    my %opt_d = (               # option with date
                 voluseduration=> 1,
                 volretention => 1,
                );
    my %opt_t = (when => 2,     # option with time 
                 begin => 1,    # 1 hh:min are optionnal
                 end => 1,      # 2 hh:min are required
                 );

    foreach my $i (@what) {
        if (exists $opt_i{$i}) {# integer param
            my $value = CGI::param($i) || $opt_i{$i} ;
            if ($value =~ /^(\d+)$/) {
                $ret{$i} = $1;
            } elsif ($i eq 'age' &&  # can have unit
                     $value =~ /^(?:\d+(?:[ywdhms]\s*)?)+\s*$/) # 2y1h2m34s
            {
                $ret{$i} = human_sec_unit($value);
            }
        } elsif ($opt_s{$i}) {  # simple string param
            my $value = CGI::param($i) || '';
            if ($value =~ /^([\w\d\.-]+)$/) {
                $ret{$i} = $1;
            }
        } elsif ($opt_ss{$i}) { # simple string param (with space)
            my $value = CGI::param($i) || '';
            if ($value =~ /^([\w\d\.\-\s]+)$/) {
                $ret{$i} = $1;
            }
        } elsif ($i =~ /^j(\w+)s$/) { # quote join args "'arg1', 'arg2'"
            my @value = grep { ! /^\s*$/ } CGI::param($1) ;
            if (@value) {
                $ret{$i} = $self->dbh_join(@value) ;
            }

        } elsif ($i =~ /^q(\w+[^s])$/) { # 'arg1'
            my $value = CGI::param($1) ;
            if ($value) {
                $ret{$i} = $self->dbh_quote($value);
            }

        } elsif ($i =~ /^q(\w+)s$/) { #[ 'arg1', 'arg2']
            $ret{$i} = [ map { { name => $self->dbh_quote($_) } } 
                                           grep { ! /^\s*$/ } CGI::param($1) ];
        } elsif (exists $opt_p{$i}) {
            my $value = CGI::param($i) || '';
            if ($value =~ /^([\w\d\.\/\s:\@\-]+)$/) {
                $ret{$i} = $1;
            }
        } elsif (exists $opt_r{$i}) {
            my $value = CGI::param($i) || '';
            if ($value =~ /^([^'"']+)$/) {
                $ret{$i} = $1;
            }
        } elsif (exists $opt_d{$i}) {
            my $value = CGI::param($i) || '';
            if ($value =~ /^\s*(\d+\s+\w+)$/) {
                $ret{$i} = $1;
            }
        } elsif (exists $opt_t{$i}) { # 1: hh:min optionnal, 2: hh:min required
            my $when = CGI::param($i) || '';
            if ($when =~ /(\d{4}-\d{2}-\d{2}( \d{2}:\d{2}(:\d{2})?)?)/) {
                if ($opt_t{$i} == 1 or defined $2) {
                    $ret{$i} = $1;
                }
            }
        }
    }

    if ($what{storage_cmd}) {
        if (!grep {/^\Q$ret{storage_cmd}\E$/} ('mount', 'umount', 'release','status')) {
            delete $ret{storage_cmd};
        }
    }

    if ($what{slots}) {
        foreach my $s (CGI::param('slot')) {
            if ($s =~ /^(\d+)$/) {
                push @{$ret{slots}}, $s;
            }
        }
    }

    if ($what{since}) {
        my $age = $ret{age} || human_sec_unit($opt_i{age});
        my $since = CGI::param('since') || strftime('%F %T', localtime(time - $age));
        if ($since =~ /^(\d{4}-\d{2}-\d{2}( \d{2}:\d{2}:\d{2})?)$/) {
            $ret{since} = $1;
        }
    }

    if ($what{lang}) {
        my $lang = CGI::param('lang') || 'en';
        if ($lang =~ /^(\w\w)$/) {
            $ret{lang} = $1;
        }
    }

    if ($what{db_clients}) {
        my $filter='';
        if ($what{filter}) {
            # get security filter only if asked
            $filter = $self->get_client_filter();
        }

        my $query = "
SELECT Client.Name as clientname
  FROM Client $filter
";

        my $clients = $self->dbh_selectall_hashref($query, 'clientname');
        $ret{db_clients} = [sort {$a->{clientname} cmp $b->{clientname} } 
                              values %$clients] ;
    }

    if ($what{db_client_groups}) {
        my $filter='';
        if ($what{filter}) {
            # get security filter only if asked
            $filter = $self->get_client_group_filter();
        }

        my $query = "
SELECT client_group_name AS name, comment AS comment
  FROM client_group $filter
";
        my $grps = $self->dbh_selectall_hashref($query, 'name');
        $ret{db_client_groups} = [sort {$a->{name} cmp $b->{name} } 
                                  values %$grps] ;
    }

    if ($what{db_usernames}) {
        my $query = "
SELECT username, comment
  FROM bweb_user
";
        my $users = $self->dbh_selectall_hashref($query, 'username');
        $ret{db_usernames} = [sort {$a->{username} cmp $b->{username} } 
                                  values %$users] ;
    }

    if ($what{db_roles}) {
        my $query = "
SELECT rolename, comment
  FROM bweb_role
";
        my $r = $self->dbh_selectall_hashref($query, 'rolename');
        $ret{db_roles} = [sort {$a->{rolename} cmp $b->{rolename} } 
                                  values %$r] ;
    }

    if ($what{db_mediatypes}) {
        my $query = "
SELECT MediaType as mediatype
  FROM MediaType
";
        my $media = $self->dbh_selectall_hashref($query, 'mediatype');
        $ret{db_mediatypes} = [sort {$a->{mediatype} cmp $b->{mediatype} } 
                                  values %$media] ;
    }

    if ($what{db_locations}) {
        my $query = "
SELECT Location as location, Cost as cost 
  FROM Location
";
        my $loc = $self->dbh_selectall_hashref($query, 'location');
        $ret{db_locations} = [ sort { $a->{location} 
                                      cmp 
                                      $b->{location} 
                                  } values %$loc ];
    }

    if ($what{db_pools}) {
        my $query = "SELECT Name as name FROM Pool";

        my $all = $self->dbh_selectall_hashref($query, 'name') ;
        $ret{db_pools} = [ sort { $a->{name} cmp $b->{name} } values %$all ];
    }

    if ($what{db_filesets}) {
        my $query = "
SELECT FileSet.FileSet AS fileset 
  FROM FileSet
";
        my $filesets = $self->dbh_selectall_hashref($query, 'fileset');

        $ret{db_filesets} = [sort {lc($a->{fileset}) cmp lc($b->{fileset}) } 
                               values %$filesets] ;
    }

    if ($what{db_jobnames}) {
        my $filter='';
        if ($what{filter}) {
            $filter = " JOIN Client USING (ClientId) " . $self->get_client_filter();
        }
        my $query = "
SELECT DISTINCT Job.Name AS jobname 
  FROM Job $filter
";
        my $jobnames = $self->dbh_selectall_hashref($query, 'jobname');

        $ret{db_jobnames} = [sort {lc($a->{jobname}) cmp lc($b->{jobname}) } 
                               values %$jobnames] ;
    }

    if ($what{db_devices}) {
        my $query = "
SELECT Device.Name AS name
  FROM Device
";
        my $devices = $self->dbh_selectall_hashref($query, 'name');

        $ret{db_devices} = [sort {lc($a->{name}) cmp lc($b->{name}) } 
                               values %$devices] ;
    }

    return \%ret;
}

sub display_graph
{
    my ($self) = @_;
    $self->can_do('r_view_stat');
    my $fields = $self->get_form(qw/age level status clients filesets 
                                    graph gtype type filter db_clients
                                    limit db_filesets width height
                                    qclients qfilesets qjobnames db_jobnames/);
    
    my $url = CGI::url(-full => 0,
                      -base => 0,
                      -query => 1);
    $url =~ s/^.+?\?//; # http://path/to/bweb.pl?arg => arg

# this organisation is to keep user choice between 2 click
# TODO : fileset and client selection doesn't work

    $self->display({
        url => $url,
        %$fields,
    }, "graph.tpl");

    if ($fields->{gtype} and $fields->{gtype} eq 'balloon') {
        system("./bgraph.pl");
    }
}

sub get_selected_media_location
{
    my ($self) = @_ ;

    my $media = $self->get_form('jmedias');

    unless ($media->{jmedias}) {
        return undef;
    }

    my $query = "
SELECT Media.VolumeName AS volumename, Location.Location AS location
FROM Media LEFT JOIN Location ON (Media.LocationId = Location.LocationId)
WHERE Media.VolumeName IN ($media->{jmedias})
";

    my $all = $self->dbh_selectall_hashref($query, 'volumename') ;
  
    # { 'vol1' => { [volumename => 'vol1', location => 'ici'],
    #               ..
    #             }
    # }
    return $all;
}

sub move_media
{
    my ($self, $in) = @_ ;
    $self->can_do('r_media_mgnt');
    my $media = $self->get_selected_media_location();

    unless ($media) {
        return ;
    }

    my $elt = $self->get_form('db_locations');

    $self->display({ ID => $cur_id++,
                     enabled => human_enabled($in),
                     %$elt,     # db_locations
                     media => [ 
            sort { $a->{volumename} cmp $b->{volumename} } values %$media
                               ],
                     },
                   "move_media.tpl");
}

sub help_extern
{
    my ($self) = @_ ;
    $self->can_do('r_media_mgnt');

    my $elt = $self->get_form(qw/db_pools db_mediatypes db_locations/) ;
    $self->debug($elt);
    $self->display($elt, "help_extern.tpl");
}

sub help_extern_compute
{
    my ($self) = @_;
    $self->can_do('r_media_mgnt');

    my $number = CGI::param('limit') || '' ;
    unless ($number =~ /^(\d+)$/) {
        return $self->error("Bad arg number : $number ");
    }

    my ($sql, undef) = $self->get_param('pools', 
                                        'locations', 'mediatypes');

    my $query = "
SELECT Media.VolumeName  AS volumename,
       Media.VolStatus   AS volstatus,
       Media.LastWritten AS lastwritten,
       Media.MediaType   AS mediatype,
       Media.VolMounts   AS volmounts,
       Pool.Name         AS name,
       Media.Recycle     AS recycle,
       $self->{sql}->{FROM_UNIXTIME}(
          $self->{sql}->{UNIX_TIMESTAMP}(Media.LastWritten) 
        + $self->{sql}->{TO_SEC}(Media.VolRetention)
       ) AS expire
FROM Media 
 INNER JOIN Pool     ON (Pool.PoolId = Media.PoolId)
 LEFT  JOIN Location ON (Media.LocationId = Location.LocationId)

WHERE Media.InChanger = 1
  AND Media.VolStatus IN ('Disabled', 'Error', 'Full', 'Used')
  $sql
ORDER BY expire DESC, recycle, Media.VolMounts DESC
LIMIT $number
" ;
    
    my $all = $self->dbh_selectall_hashref($query, 'volumename') ;

    $self->display({ Media => [ values %$all ] },
                   "help_extern_compute.tpl");
}

sub help_intern
{
    my ($self) = @_ ;
    $self->can_do('r_media_mgnt');

    my $param = $self->get_form(qw/db_locations db_pools db_mediatypes/) ;
    $self->display($param, "help_intern.tpl");
}

sub help_intern_compute
{
    my ($self) = @_;
    $self->can_do('r_media_mgnt');

    my $number = CGI::param('limit') || '' ;
    unless ($number =~ /^(\d+)$/) {
        return $self->error("Bad arg number : $number ");
    }

    my ($sql, undef) = $self->get_param('pools', 'locations', 'mediatypes');

    if (CGI::param('expired')) {
        # we take only expired volumes or purged/recycle ones
        $sql = "
AND (
 (  ($self->{sql}->{UNIX_TIMESTAMP}(Media.LastWritten) 
      + $self->{sql}->{TO_SEC}(Media.VolRetention)
    ) < NOW()
 ) OR ( 
  Media.VolStatus IN ('Purged', 'Recycle')
 )
)
 " . $sql ;
    }

    my $query = "
SELECT Media.VolumeName  AS volumename,
       Media.VolStatus   AS volstatus,
       Media.LastWritten AS lastwritten,
       Media.MediaType   AS mediatype,
       Media.VolMounts   AS volmounts,
       Pool.Name         AS name,
       $self->{sql}->{FROM_UNIXTIME}(
          $self->{sql}->{UNIX_TIMESTAMP}(Media.LastWritten) 
        + $self->{sql}->{TO_SEC}(Media.VolRetention)
       ) AS expire
FROM Media 
 INNER JOIN Pool ON (Pool.PoolId = Media.PoolId) 
 LEFT  JOIN Location ON (Location.LocationId = Media.LocationId)

WHERE Media.InChanger <> 1
  AND Media.VolStatus IN ('Purged', 'Full', 'Append', 'Recycle')
  AND Media.Recycle = 1
  $sql
ORDER BY Media.VolUseDuration DESC, Media.VolMounts ASC, expire ASC 
LIMIT $number
" ;
    
    my $all = $self->dbh_selectall_hashref($query, 'volumename') ;

    $self->display({ Media => [ values %$all ] },
                   "help_intern_compute.tpl");

}

sub display_general
{
    my ($self, %arg) = @_ ;

    my ($limit, $label) = $self->get_limit(%arg);

    my $query = "
SELECT
    (SELECT count(Pool.PoolId)   FROM Pool)   AS nb_pool,
    (SELECT count(Media.MediaId) FROM Media)  AS nb_media,
    (SELECT count(Job.JobId)     FROM Job)    AS nb_job,
    (SELECT sum(VolBytes)        FROM Media)  AS nb_bytes,
    ($self->{sql}->{DB_SIZE})                 AS db_size,
    (SELECT count(Job.JobId)
      FROM Job
      WHERE Job.JobStatus IN ('E','e','f','A')
      $limit
    )                                         AS nb_err,
    (SELECT count(Client.ClientId) FROM Client) AS nb_client
";

    my $row = $self->dbh_selectrow_hashref($query) ;

    $row->{nb_bytes} = human_size($row->{nb_bytes});

    $row->{db_size} = human_size($row->{db_size});
    $row->{label} = $label;
    $row->{age} = $arg{age};

    $self->display($row, "general.tpl");
}

sub get_param
{
    my ($self, @what) = @_ ;
    my %elt = map { $_ => 1 } @what;
    my %ret;

    my $limit = '';

    if ($elt{clients}) {
        my @clients = grep { ! /^\s*$/ } CGI::param('client');
        if (@clients) {
            $ret{clients} = \@clients;
            my $str = $self->dbh_join(@clients);
            $limit .= "AND Client.Name IN ($str) ";
        }
    }

    if ($elt{client_groups}) {
        my @clients = grep { ! /^\s*$/ } CGI::param('client_group');
        if (@clients) {
            $ret{client_groups} = \@clients;
            my $str = $self->dbh_join(@clients);
            $limit .= "AND client_group_name IN ($str) ";
        }
    }

    if ($elt{filesets}) {
        my @filesets = grep { ! /^\s*$/ } CGI::param('fileset');
        if (@filesets) {
            $ret{filesets} = \@filesets;
            my $str = $self->dbh_join(@filesets);
            $limit .= "AND FileSet.FileSet IN ($str) ";
        }
    }

    if ($elt{mediatypes}) {
        my @media = grep { ! /^\s*$/ } CGI::param('mediatype');
        if (@media) {
            $ret{mediatypes} = \@media;
            my $str = $self->dbh_join(@media);
            $limit .= "AND Media.MediaType IN ($str) ";
        }
    }

    if ($elt{client}) {
        my $client = CGI::param('client');
        if ($client) {
            $ret{client} = $client;
            $client = $self->dbh_quote($client);
            $limit .= "AND Client.Name = $client ";
        }
    }

    if ($elt{level}) {
        my $level = CGI::param('level') || '';
        if ($level =~ /^(\w)$/) {
            $ret{level} = $1;
            $limit .= "AND Job.Level = '$1' ";
        }
    }

    if ($elt{jobid}) {
        my $jobid = CGI::param('jobid') || '';

        if ($jobid =~ /^(\d+)$/) {
            $ret{jobid} = $1;
            $limit .= "AND Job.JobId = '$1' ";
        }
    }

    if ($elt{status}) {
        my $status = CGI::param('status') || '';
        if ($status =~ /^(\w)$/) {
            $ret{status} = $1;
            if ($1 eq 'f') {
                $limit .= "AND Job.JobStatus IN ('E','e','f','A') ";            
            } elsif ($1 eq 'W') {
                $limit .= "AND Job.JobStatus IN ('T', 'W') OR Job.JobErrors > 0 ";              
            } else {
                $limit .= "AND Job.JobStatus = '$1' ";          
            }
        }
    }

    if ($elt{volstatus}) {
        my $status = CGI::param('volstatus') || '';
        if ($status =~ /^(\w+)$/) {
            $ret{status} = $1;
            $limit .= "AND Media.VolStatus = '$1' ";            
        }
    }

    if ($elt{locations}) {
        my @location = grep { ! /^\s*$/ } CGI::param('location') ;
        if (@location) {
            $ret{locations} = \@location;           
            my $str = $self->dbh_join(@location);
            $limit .= "AND Location.Location IN ($str) ";
        }
    }

    if ($elt{pools}) {
        my @pool = grep { ! /^\s*$/ } CGI::param('pool') ;
        if (@pool) {
            $ret{pools} = \@pool; 
            my $str = $self->dbh_join(@pool);
            $limit .= "AND Pool.Name IN ($str) ";
        }
    }

    if ($elt{location}) {
        my $location = CGI::param('location') || '';
        if ($location) {
            $ret{location} = $location;
            $location = $self->dbh_quote($location);
            $limit .= "AND Location.Location = $location ";
        }
    }

    if ($elt{pool}) {
        my $pool = CGI::param('pool') || '';
        if ($pool) {
            $ret{pool} = $pool;
            $pool = $self->dbh_quote($pool);
            $limit .= "AND Pool.Name = $pool ";
        }
    }

    if ($elt{jobtype}) {
        my $jobtype = CGI::param('jobtype') || '';
        if ($jobtype =~ /^(\w)$/) {
            $ret{jobtype} = $1;
            $limit .= "AND Job.Type = '$1' ";
        }
    }

    return ($limit, %ret);
}

=head1

    get last backup

=cut 

sub display_job
{
    my ($self, %arg) = @_ ;
    return if $self->cant_do('r_view_job');

    $arg{order} = ' Job.JobId DESC ';

    my ($limit, $label) = $self->get_limit(%arg);
    my ($where, undef) = $self->get_param('clients',
                                          'client_groups',
                                          'level',
                                          'filesets',
                                          'jobtype',
                                          'pools',
                                          'jobid',
                                          'status');
    my $cgq='';
    if (CGI::param('client_group')) {
        $cgq .= "
JOIN client_group_member USING (ClientId)
JOIN client_group USING (client_group_id)
";
    }
    my $filter = $self->get_client_filter();

    my $query="
SELECT  Job.JobId       AS jobid,
        Client.Name     AS client,
        FileSet.FileSet AS fileset,
        Job.Name        AS jobname,
        Level           AS level,
        StartTime       AS starttime,
        EndTime         AS endtime,
        Pool.Name       AS poolname,
        JobFiles        AS jobfiles, 
        JobBytes        AS jobbytes,
        JobStatus       AS jobstatus,
        Type            AS jobtype,
     $self->{sql}->{SEC_TO_TIME}(  $self->{sql}->{UNIX_TIMESTAMP}(EndTime)  
                                 - $self->{sql}->{UNIX_TIMESTAMP}(StartTime)) 
                        AS duration,

        JobErrors       AS joberrors

 FROM Client $filter $cgq, 
      Job LEFT JOIN Pool     ON (Job.PoolId    = Pool.PoolId)
          LEFT JOIN FileSet  ON (Job.FileSetId = FileSet.FileSetId)
 WHERE Client.ClientId=Job.ClientId
   AND Job.JobStatus NOT IN ('R', 'C')
 $where
 $limit
";

    my $all = $self->dbh_selectall_hashref($query, 'jobid') ;

    $self->display({ Filter => $label,
                     ID => $cur_id++,
                     Jobs => 
                           [ 
                             sort { $a->{jobid} <=>  $b->{jobid} } 
                                        values %$all 
                             ],
                   },
                   "display_job.tpl");
}

# display job informations
sub display_job_zoom
{
    my ($self, $jobid) = @_ ;
    $self->can_do('r_view_job');

    $jobid = $self->dbh_quote($jobid);

    # get security filter
    my $filter = $self->get_client_filter();

    my $query="
SELECT DISTINCT Job.JobId       AS jobid,
                Client.Name     AS client,
                Job.Name        AS jobname,
                FileSet.FileSet AS fileset,
                Level           AS level,
                Pool.Name       AS poolname,
                StartTime       AS starttime,
                JobFiles        AS jobfiles, 
                JobBytes        AS jobbytes,
                JobStatus       AS jobstatus,
                JobErrors       AS joberrors,
                Type            AS jobtype,
                $self->{sql}->{SEC_TO_TIME}(  $self->{sql}->{UNIX_TIMESTAMP}(EndTime)  
                                            - $self->{sql}->{UNIX_TIMESTAMP}(StartTime)) AS duration

 FROM Client $filter,
      Job LEFT JOIN FileSet ON (Job.FileSetId = FileSet.FileSetId)
          LEFT JOIN Pool    ON (Job.PoolId    = Pool.PoolId)
 WHERE Client.ClientId=Job.ClientId
 AND Job.JobId = $jobid
";

    my $row = $self->dbh_selectrow_hashref($query) ;

    # display all volumes associate with this job
    $query="
SELECT Media.VolumeName as volumename
FROM Job,Media,JobMedia
WHERE Job.JobId = $jobid
 AND JobMedia.JobId=Job.JobId 
 AND JobMedia.MediaId=Media.MediaId
";

    my $all = $self->dbh_selectall_hashref($query, 'volumename');

    $row->{volumes} = [ values %$all ] ;
    $row->{wiki_url} = $self->{info}->{wiki_url};

    $self->display($row, "display_job_zoom.tpl");
}

sub display_job_group
{
    my ($self, %arg) = @_;
    $self->can_do('r_view_job');

    my ($limit, $label) = $self->get_limit(groupby => 'client_group_name',  %arg);

    my ($where, undef) = $self->get_param('client_groups',
                                          'level',
                                          'pools');
    my $filter = $self->get_client_group_filter();
    my $query = 
"
SELECT client_group_name AS client_group_name,
       COALESCE(jobok.jobfiles,0)  + COALESCE(joberr.jobfiles,0)  AS jobfiles,
       COALESCE(jobok.jobbytes,0)  + COALESCE(joberr.jobbytes,0)  AS jobbytes,
       COALESCE(jobok.joberrors,0) + COALESCE(joberr.joberrors,0) AS joberrors,
       COALESCE(jobok.nbjobs,0)  AS nbjobok,
       COALESCE(joberr.nbjobs,0) AS nbjoberr,
       COALESCE(jobok.duration, '0:0:0') AS duration

FROM client_group $filter LEFT JOIN (
    SELECT client_group_name AS client_group_name, COUNT(1) AS nbjobs, 
           SUM(JobFiles) AS jobfiles, SUM(JobBytes) AS jobbytes, 
           SUM(JobErrors) AS joberrors,
           SUM($self->{sql}->{SEC_TO_TIME}(  $self->{sql}->{UNIX_TIMESTAMP}(EndTime)  
                              - $self->{sql}->{UNIX_TIMESTAMP}(StartTime)))
                        AS duration

    FROM Job JOIN client_group_member ON (Job.ClientId = client_group_member.ClientId)
             JOIN client_group USING (client_group_id)
    
    WHERE Type IN ('B', 'R') AND JobStatus IN ('T', 'W')
    $where
    $limit
) AS jobok USING (client_group_name) LEFT JOIN

(
    SELECT client_group_name AS client_group_name, COUNT(1) AS nbjobs, 
           SUM(JobFiles) AS jobfiles, SUM(JobBytes) AS jobbytes, 
           SUM(JobErrors) AS joberrors
    FROM Job JOIN client_group_member ON (Job.ClientId = client_group_member.ClientId)
             JOIN client_group USING (client_group_id)
    
    WHERE Type IN ('B', 'R') AND JobStatus IN ('f','E', 'A')
    $where
    $limit
) AS joberr USING (client_group_name)

    ";

    my $all = $self->dbh_selectall_hashref($query, 'client_group_name');

    my $rep = { groups => [ values %$all ], age => $arg{age}, filter => $label };
                
    $self->debug($rep);
    $self->display($rep, "display_job_group.tpl");
}

sub display_media
{
    my ($self, %arg) = @_ ;
    $self->can_do('r_view_media');

    my ($limit, $label) = $self->get_limit(%arg);    
    my ($where, %elt) = $self->get_param('pools',
                                         'mediatypes',
                                         'volstatus',
                                         'locations');

    my $arg = $self->get_form('jmedias', 'qre_media', 'expired');

    if ($arg->{jmedias}) {
        $where = "AND Media.VolumeName IN ($arg->{jmedias}) $where"; 
    }
    if ($arg->{qre_media}) {
        $where = "AND Media.VolumeName $self->{sql}->{MATCH} $arg->{qre_media} $where"; 
    }
    if ($arg->{expired}) {
        $where = " 
        AND VolStatus = ('Full', 'Used')
        AND (    $self->{sql}->{UNIX_TIMESTAMP}(Media.LastWritten) 
               + $self->{sql}->{TO_SEC}(Media.VolRetention)
            ) < NOW()  " . $where ;
    }

    my $query="
SELECT Media.VolumeName  AS volumename, 
       Media.VolBytes    AS volbytes,
       Media.VolStatus   AS volstatus,
       Media.MediaType   AS mediatype,
       Media.InChanger   AS online,
       Media.LastWritten AS lastwritten,
       Location.Location AS location,
       (volbytes*100/COALESCE(media_avg_size.size,-1))  AS volusage,
       Pool.Name         AS poolname,
       $self->{sql}->{FROM_UNIXTIME}(
          $self->{sql}->{UNIX_TIMESTAMP}(Media.LastWritten) 
        + $self->{sql}->{TO_SEC}(Media.VolRetention)
       ) AS expire
FROM      Pool, Media 
LEFT JOIN Location ON (Media.LocationId = Location.LocationId)
LEFT JOIN (SELECT avg(Media.VolBytes) AS size,
                  Media.MediaType     AS MediaType
           FROM Media 
          WHERE Media.VolStatus = 'Full' 
          GROUP BY Media.MediaType
           ) AS media_avg_size ON (Media.MediaType = media_avg_size.MediaType)

WHERE Media.PoolId=Pool.PoolId
$where
$limit
";

    my $all = $self->dbh_selectall_hashref($query, 'volumename') ;

    $self->display({ ID => $cur_id++,
                     Pool => $elt{pool},
                     Location => $elt{location},
                     Media => [ values %$all ],
                   },
                   "display_media.tpl");
}

sub display_allmedia
{
    my ($self) = @_ ;

    my $pool = $self->get_form('db_pools');
    
    foreach my $name (@{ $pool->{db_pools} }) {
        CGI::param('pool', $name->{name});
        $self->display_media();
    }
}

sub display_media_zoom
{
    my ($self) = @_ ;

    my $media = $self->get_form('jmedias');
    
    unless ($media->{jmedias}) {
        return $self->error("Can't get media selection");
    }
    
    my $query="
SELECT InChanger     AS online,
       Media.Enabled AS enabled,
       VolBytes      AS nb_bytes,
       VolumeName    AS volumename,
       VolStatus     AS volstatus,
       VolMounts     AS nb_mounts,
       Media.VolUseDuration   AS voluseduration,
       Media.MaxVolJobs AS maxvoljobs,
       Media.MaxVolFiles AS maxvolfiles,
       Media.MaxVolBytes AS maxvolbytes,
       VolErrors     AS nb_errors,
       Pool.Name     AS poolname,
       Location.Location AS location,
       Media.Recycle AS recycle,
       Media.VolRetention AS volretention,
       Media.LastWritten  AS lastwritten,
       Media.VolReadTime/1000000  AS volreadtime,
       Media.VolWriteTime/1000000 AS volwritetime,
       Media.RecycleCount AS recyclecount,
       Media.Comment      AS comment,
       $self->{sql}->{FROM_UNIXTIME}(
          $self->{sql}->{UNIX_TIMESTAMP}(Media.LastWritten) 
        + $self->{sql}->{TO_SEC}(Media.VolRetention)
       ) AS expire
 FROM Pool,
      Media LEFT JOIN Location ON (Media.LocationId = Location.LocationId)
 WHERE Pool.PoolId = Media.PoolId
 AND VolumeName IN ($media->{jmedias})
";

    my $all = $self->dbh_selectall_hashref($query, 'volumename') ;

    foreach my $media (values %$all) {
        my $mq = $self->dbh_quote($media->{volumename});

        $query = "
SELECT DISTINCT Job.JobId AS jobid,
                Job.Name  AS name,
                Job.StartTime AS starttime,
                Job.Type  AS type,
                Job.Level AS level,
                Job.JobFiles AS files,
                Job.JobBytes AS bytes,
                Job.jobstatus AS status
 FROM Media,JobMedia,Job
 WHERE Media.VolumeName=$mq
 AND Media.MediaId=JobMedia.MediaId              
 AND JobMedia.JobId=Job.JobId
";

        my $jobs = $self->dbh_selectall_hashref($query, 'jobid') ;

        $query = "
SELECT LocationLog.Date    AS date,
       Location.Location   AS location,
       LocationLog.Comment AS comment
 FROM Media,LocationLog INNER JOIN Location ON (LocationLog.LocationId = Location.LocationId)
 WHERE Media.MediaId = LocationLog.MediaId
   AND Media.VolumeName = $mq
";

        my $logtxt = '';
        my $log = $self->dbh_selectall_arrayref($query) ;
        if ($log) {
            $logtxt = join("\n", map { ($_->[0] . ' ' . $_->[1] . ' ' . $_->[2])} @$log ) ;
        }

        $self->display({ jobs => [ values %$jobs ],
                         LocationLog => $logtxt,
                         %$media },
                       "display_media_zoom.tpl");
    }
}

sub location_edit
{
    my ($self) = @_ ;
    $self->can_do('r_location_mgnt');

    my $loc = $self->get_form('qlocation');
    unless ($loc->{qlocation}) {
        return $self->error("Can't get location");
    }

    my $query = "
SELECT Location.Location AS location, 
       Location.Cost   AS cost,
       Location.Enabled AS enabled
FROM Location
WHERE Location.Location = $loc->{qlocation}
";

    my $row = $self->dbh_selectrow_hashref($query);
    $row->{enabled} = human_enabled($row->{enabled});
    $self->display({ ID => $cur_id++,
                     %$row }, "location_edit.tpl") ;
}

sub location_save
{
    my ($self) = @_ ;
    $self->can_do('r_location_mgnt');

    my $arg = $self->get_form(qw/qlocation qnewlocation cost enabled/) ;
    unless ($arg->{qlocation}) {
        return $self->error("Can't get location");
    }    
    unless ($arg->{qnewlocation}) {
        return $self->error("Can't get new location name");
    }
    unless ($arg->{cost}) {
        return $self->error("Can't get new cost");
    }

    my $enabled = from_human_enabled($arg->{enabled});

    my $query = "
UPDATE Location SET Cost     = $arg->{cost}, 
                    Location = $arg->{qnewlocation},
                    Enabled   = $enabled
WHERE Location.Location = $arg->{qlocation}
";

    $self->dbh_do($query);

    $self->location_display();
}

sub location_del
{
    my ($self) = @_ ;
    $self->can_do('r_location_mgnt');

    my $arg = $self->get_form(qw/qlocation/) ;

    unless ($arg->{qlocation}) {
        return $self->error("Can't get location");
    }

    my $query = "
SELECT count(Media.MediaId) AS nb 
  FROM Media INNER JOIN Location USING (LocationID)
WHERE Location = $arg->{qlocation}
";

    my $res = $self->dbh_selectrow_hashref($query);

    if ($res->{nb}) {
        return $self->error("Sorry, the location must be empty");
    }

    $query = "
DELETE FROM Location WHERE Location = $arg->{qlocation}
";

    $self->dbh_do($query);

    $self->location_display();
}

sub location_add
{
    my ($self) = @_ ;
    $self->can_do('r_location_mgnt');

    my $arg = $self->get_form(qw/qlocation cost/) ;

    unless ($arg->{qlocation}) {
        $self->display({}, "location_add.tpl");
        return 1;
    }
    unless ($arg->{cost}) {
        return $self->error("Can't get new cost");
    }

    my $enabled = CGI::param('enabled') || '';
    $enabled = from_human_enabled($enabled);

    my $query = "
INSERT INTO Location (Location, Cost, Enabled) 
       VALUES ($arg->{qlocation}, $arg->{cost}, $enabled)
";

    $self->dbh_do($query);

    $self->location_display();
}

sub location_display
{
    my ($self) = @_ ;

    my $query = "
SELECT Location.Location AS location, 
       Location.Cost     AS cost,
       Location.Enabled  AS enabled,
       (SELECT count(Media.MediaId) 
         FROM Media 
        WHERE Media.LocationId = Location.LocationId
       ) AS volnum
FROM Location
";

    my $location = $self->dbh_selectall_hashref($query, 'location');

    $self->display({ ID => $cur_id++,
                     Locations => [ values %$location ] },
                   "display_location.tpl");
}

sub update_location
{
    my ($self) = @_ ;

    my $media = $self->get_selected_media_location();
    unless ($media) {
        return ;
    }

    my $arg = $self->get_form('db_locations', 'qnewlocation');

    $self->display({ email  => $self->{info}->{email_media},
                     %$arg,
                     media => [ values %$media ],
                   },
                   "update_location.tpl");
}

###########################################################

sub client_save
{
    my ($self) = @_;
    my $arg = $self->get_form(qw/jclient_groups qclient/);

    unless ($arg->{qclient}) {
        return $self->error("Can't get client name");
    }

    $self->can_do('r_group_mgnt');

    my $f1 = $self->get_client_filter();
    my $f2 = $self->get_client_group_filter();

    $self->{dbh}->begin_work();

    my $query = "
DELETE FROM client_group_member 
      WHERE ClientId IN 
           (SELECT ClientId 
              FROM Client $f1
             WHERE Client.Name = $arg->{qclient})
";
    $self->dbh_do($query);

    if ($arg->{jclient_groups}) {
        $query = "
 INSERT INTO client_group_member (client_group_id, ClientId) 
    (SELECT client_group_id, (SELECT ClientId
                FROM Client $f1
               WHERE Name = $arg->{qclient})
       FROM client_group $f2 WHERE client_group_name IN ($arg->{jclient_groups})
    )
";
        $self->dbh_do($query);
    }

    $self->{dbh}->commit() or $self->error($self->{dbh}->errstr);

    $self->display_clients();
}

sub groups_edit
{
    my ($self) = @_;
    my $grp = $self->get_form(qw/qclient_group db_clients/);

    unless ($grp->{qclient_group}) {
        $self->can_do('r_group_mgnt');
        $self->display({ ID => $cur_id++,
                         client_group => "''",
                         %$grp,
                     }, "groups_edit.tpl");
        return;
    }

    unless ($self->cant_do('r_group_mgnt')) {
        $self->can_do('r_view_group');
    }

    my $query = "
SELECT Name AS name 
  FROM Client JOIN client_group_member using (ClientId)
              JOIN client_group using (client_group_id)
WHERE client_group_name = $grp->{qclient_group}
";

    my $row = $self->dbh_selectall_hashref($query, "name");
    $self->debug($row);
    $self->display({ ID => $cur_id++,
                     client_group => $grp->{qclient_group},
                     %$grp,
                     client_group_member => [ values %$row]}, 
                   "groups_edit.tpl");
}

sub groups_save
{
    my ($self) = @_;
    $self->can_do('r_group_mgnt');

    my $arg = $self->get_form(qw/qclient_group jclients qnewgroup qcomment/);
    if (!$arg->{qcomment}) {
        $arg->{qcomment} = "''";
    }

    if (!$arg->{qclient_group} and $arg->{qnewgroup}) {
        my $query = "
INSERT INTO client_group (client_group_name, comment) 
VALUES ($arg->{qnewgroup}, $arg->{qcomment})
";
        $self->dbh_do($query);
        $arg->{qclient_group} = $arg->{qnewgroup};
    }

    unless ($arg->{qclient_group}) {
        return $self->error("Can't get groups");
    }

    $self->{dbh}->begin_work();

    my $query = "
DELETE FROM client_group_member 
      WHERE client_group_id IN 
           (SELECT client_group_id 
              FROM client_group 
             WHERE client_group_name = $arg->{qclient_group})
";
    $self->dbh_do($query);

    if ($arg->{jclients}) {
        $query = "
    INSERT INTO client_group_member (ClientId, client_group_id) 
       (SELECT  ClientId, 
                (SELECT client_group_id 
                   FROM client_group 
                  WHERE client_group_name = $arg->{qclient_group})
          FROM Client WHERE Name IN ($arg->{jclients})
       )
";
        $self->dbh_do($query);
    }
    if ($arg->{qclient_group} ne $arg->{qnewgroup}) {
        $query = "
UPDATE client_group 
   SET client_group_name = $arg->{qnewgroup}, comment = $arg->{qcomment}
 WHERE client_group_name = $arg->{qclient_group}
";

        $self->dbh_do($query);
    }

    $self->{dbh}->commit() or $self->error($self->{dbh}->errstr);

    $self->display_groups();
}

sub groups_del
{
    my ($self) = @_;
    $self->can_do('r_group_mgnt');

    my $arg = $self->get_form(qw/qclient_group/);

    unless ($arg->{qclient_group}) {
        return $self->error("Can't get groups");
    }

    $self->{dbh}->begin_work();

    $self->dbh_do("
DELETE FROM client_group_member 
      WHERE client_group_id IN 
           (SELECT client_group_id 
              FROM client_group 
             WHERE client_group_name = $arg->{qclient_group})");

    $self->dbh_do("
DELETE FROM bweb_client_group_acl
      WHERE client_group_id IN
           (SELECT client_group_id 
              FROM client_group 
             WHERE client_group_name = $arg->{qclient_group})");

    $self->dbh_do("
DELETE FROM client_group
      WHERE client_group_name = $arg->{qclient_group}");

    $self->{dbh}->commit();
    $self->display_groups();
}

sub display_groups
{
    my ($self) = @_;
    my $arg;

    if ($self->cant_do('r_group_mgnt')) {
        $arg = $self->get_form(qw/db_client_groups filter/) ;
    } else {
        $arg = $self->get_form(qw/db_client_groups/) ;
    }

    if ($self->{dbh}->errstr) {
        return $self->error("Can't use groups with bweb, read INSTALL to enable them");
    }

    $self->debug($arg);

    $self->display({ ID => $cur_id++,
                     %$arg},
                   "display_groups.tpl");
}

###########################################################

sub get_roles
{
    my ($self) = @_;
    if (not $self->{info}->{enable_security}) {
        return 1;
    }
    if (!$self->{loginname}) {
        $self->error("Can't get your login name");
        $self->display_end();
        exit 0;
    }
    # admin is a special user that can do everything
    if ($self->{loginname} eq 'admin') {
        return 1;
    }
    # already fill
    if (defined $self->{security}) {
        return 1;
    }
    $self->{security} = {};
    my $u = $self->dbh_quote($self->{loginname});
           
    my $query = "
 SELECT use_acl, rolename, tpl
  FROM bweb_user 
       JOIN bweb_role_member USING (userid)
       JOIN bweb_role USING (roleid)
 WHERE username = $u
";
    my $rows = $self->dbh_selectall_arrayref($query);
    # do cache with this role   
    if (!$rows or !scalar(@$rows)) {
        $self->error("Can't get $self->{loginname}'s roles");
        $self->display_end();
        exit 0;
    }
    foreach my $r (@$rows) {
        $self->{security}->{$r->[1]}=1;
    }
    $self->{security}->{use_acl} = $rows->[0]->[0];
    if ($rows->[0]->[2] =~ /^(\w\w)$/) {
        $self->{lang} = $1;
    }
    return 1;
}

sub can_view_client
{
    my ($self, $client) = @_;

    my $filter = $self->get_client_filter();
    if (!$filter) {
        return 1;
    }
    my $cont = $self->dbh_selectrow_hashref("
 SELECT 1
   FROM Client $filter
  WHERE Name = '$client'
");
    return defined $cont;
}

sub cant_do
{
    my ($self, $action) = @_;
    # is security enabled in configuration ?
    if (not $self->{info}->{enable_security}) {
        return 0
    }
    # admin is a special user that can do everything
    if ($self->{loginname} eq 'admin') {
        return 0;
    }
    # must be logged
    if (!$self->{loginname}) {
        $self->{error} = "Can't do $action, your are not logged. " .
            "Check security with your administrator";
        return 1;
    }
    if (!$self->get_roles()) {
        return 0;
    }
    if (!$self->{security}->{$action}) {
        $self->{error} =
            "$self->{loginname} sorry, but this action ($action) " .
            "is not permited. " .
            "Check security with your administrator";
        return 1;
    }
    return 0;
}

# make like an assert (program die)
sub can_do
{
    my ($self, $action) = @_;
    if ($self->cant_do($action)) {
        $self->error($self->{error});
        $self->display_end();
        exit 0;
    }
    return 1;
}

sub use_filter
{
    my ($self) = @_;

    if (!$self->{info}->{enable_security} or 
        !$self->{info}->{enable_security_acl})
    {
        return 0 ;
    }
    
    if ($self->get_roles()) {
        return $self->{security}->{use_acl};
    } else {
        return 1;
    }
}

# JOIN Client USING (ClientId) " . $b->get_client_filter() . "
sub get_client_filter
{
    my ($self, $login) = @_;
    my $u;
    if ($login) {
        $u = $self->dbh_quote($login);
    } elsif ($self->use_filter()) {
        $u = $self->dbh_quote($self->{loginname});
    } else {
        return '';
    }
    return "
 JOIN (SELECT ClientId FROM client_group_member
   JOIN client_group USING (client_group_id) 
   JOIN bweb_client_group_acl USING (client_group_id) 
   JOIN bweb_user USING (userid)
   WHERE bweb_user.username = $u 
 ) AS filter USING (ClientId)";
}

#JOIN client_group USING (client_group_id)" . $b->get_client_group_filter()
sub get_client_group_filter
{
    my ($self, $login) = @_;
    my $u;
    if ($login) {
        $u = $self->dbh_quote($login);
    } elsif ($self->use_filter()) {
        $u = $self->dbh_quote($self->{loginname});
    } else {
        return '';
    }
    return "
 JOIN (SELECT client_group_id 
         FROM bweb_client_group_acl
         JOIN bweb_user USING (userid)
   WHERE bweb_user.username = $u 
 ) AS filter USING (client_group_id)";
}

# role and username have to be quoted before
# role and username can be a quoted list
sub revoke
{
    my ($self, $role, $username) = @_;
    $self->can_do("r_user_mgnt");
    
    my $nb = $self->dbh_do("
 DELETE FROM bweb_role_member 
       WHERE roleid = (SELECT roleid FROM bweb_role
                        WHERE rolename IN ($role))
         AND userid = (SELECT userid FROM bweb_user
                        WHERE username IN ($username))");
    return $nb;
}

# role and username have to be quoted before
# role and username can be a quoted list
sub grant
{
    my ($self, $role, $username) = @_;
    $self->can_do("r_user_mgnt");

    my $nb = $self->dbh_do("
   INSERT INTO bweb_role_member (roleid, userid)
     SELECT roleid, userid FROM bweb_role, bweb_user 
      WHERE rolename IN ($role)
        AND username IN ($username)
     ");
    return $nb;
}

# role and username have to be quoted before
# role and username can be a quoted list
sub grant_like
{
    my ($self, $copy, $user) = @_;
    $self->can_do("r_user_mgnt");

    my $nb = $self->dbh_do("
  INSERT INTO bweb_role_member (roleid, userid) 
   SELECT roleid, a.userid 
     FROM bweb_user AS a, bweb_role_member 
     JOIN bweb_user USING (userid)
    WHERE bweb_user.username = $copy
      AND a.username = $user");
    return $nb;
}

# username can be a join quoted list of usernames
sub revoke_all
{
    my ($self, $username) = @_;
    $self->can_do("r_user_mgnt");

    $self->dbh_do("
   DELETE FROM bweb_role_member
         WHERE userid IN (
           SELECT userid 
             FROM bweb_user 
            WHERE username in ($username))");
    $self->dbh_do("
DELETE FROM bweb_client_group_acl 
 WHERE userid IN (
  SELECT userid 
    FROM bweb_user 
   WHERE username IN ($username))");
    
}

sub users_del
{
    my ($self) = @_;
    $self->can_do("r_user_mgnt");

    my $arg = $self->get_form(qw/jusernames/);

    unless ($arg->{jusernames}) {
        return $self->error("Can't get user");
    }

    $self->{dbh}->begin_work();
    {
        $self->revoke_all($arg->{jusernames});
        $self->dbh_do("
DELETE FROM bweb_user WHERE username IN ($arg->{jusernames})");
    }
    $self->{dbh}->commit();
    
    $self->display_users();
}

sub users_add
{
    my ($self) = @_;
    $self->can_do("r_user_mgnt");

    # we don't quote username directly to check that it is conform
    my $arg = $self->get_form(qw/username qpasswd qcomment jrolenames qcreate 
                                 lang qcopy_username jclient_groups/) ;

    if (not $arg->{qcreate}) {
        $arg = $self->get_form(qw/db_roles db_usernames db_client_groups/);
        $self->display($arg, "display_user.tpl");
        return 1;
    }

    my $u = $self->dbh_quote($arg->{username});
    
    $arg->{use_acl}=(CGI::param('use_acl')?'true':'false');

    if (!$arg->{qpasswd}) {
        $arg->{qpasswd} = "''";
    }
    if (!$arg->{qcomment}) {
        $arg->{qcomment} = "''";
    }

    # will fail if user already exists
    # UPDATE with mysql dbi does not return if update is ok
    ($self->dbh_do("
  UPDATE bweb_user 
     SET passwd=$arg->{qpasswd}, comment=$arg->{qcomment}, 
         use_acl=$arg->{use_acl}, tpl='$arg->{lang}'
   WHERE username = $u") 
#     and (! $self->dbh_is_mysql() )
     ) and
    $self->dbh_do("
  INSERT INTO bweb_user (username, passwd, use_acl, comment, tpl) 
        VALUES ($u, $arg->{qpasswd}, $arg->{use_acl}, 
                $arg->{qcomment}, '$arg->{lang}')");

    $self->{dbh}->begin_work();
    {
        $self->revoke_all($u);

        if ($arg->{qcopy_username}) {
            $self->grant_like($arg->{qcopy_username}, $u);
        } else {
            $self->grant($arg->{jrolenames}, $u);
        }

        if ($arg->{jclient_groups}) {
            $self->dbh_do("
INSERT INTO bweb_client_group_acl (client_group_id, userid)
 SELECT client_group_id, userid 
   FROM client_group, bweb_user
  WHERE client_group_name IN ($arg->{jclient_groups})
    AND username = $u
");
        }
    }
    $self->{dbh}->commit();

    $self->display_users();
}

# TODO: we miss a matrix with all user/roles
sub display_users
{
    my ($self) = @_;
    $self->can_do("r_user_mgnt");

    my $arg = $self->get_form(qw/db_usernames/) ;

    if ($self->{dbh}->errstr) {
        return $self->error("Can't use users with bweb, read INSTALL to enable them");
    }

    $self->display({ ID => $cur_id++,
                     %$arg},
                   "display_users.tpl");
}

sub display_user
{
    my ($self) = @_;
    $self->can_do("r_user_mgnt");

    my $arg = $self->get_form('username');
    my $user = $self->dbh_quote($arg->{username});

    my $userp = $self->dbh_selectrow_hashref("
   SELECT username, passwd, comment, use_acl, tpl
     FROM bweb_user
    WHERE username = $user
");
    if (!$userp) {
        return $self->error("Can't find $user in catalog");
    }
    my $filter = $self->get_client_group_filter($arg->{username});
    my $scg = $self->dbh_selectall_hashref("
 SELECT client_group_name AS name 
   FROM client_group $filter
", 'name');

#  rolename  | userid
#------------+--------
# cancel_job |      0
# restore    |      0
# run_job    |      1

    my $role = $self->dbh_selectall_hashref("
SELECT rolename, max(here) AS userid FROM (
        SELECT rolename, 1 AS here
          FROM bweb_user 
          JOIN bweb_role_member USING (userid)
          JOIN bweb_role USING (roleid)
          WHERE username = $user
       UNION ALL
       SELECT rolename, 0 
         FROM bweb_role
) AS temp
GROUP by rolename", 'rolename');

    $arg = $self->get_form(qw/db_usernames db_client_groups/);    

    $self->display({
        db_usernames => $arg->{db_usernames},
        username => $userp->{username},
        comment => $userp->{comment},
        passwd => $userp->{passwd},
        lang => $userp->{tpl},
        use_acl => $userp->{use_acl},
        db_client_groups => $arg->{db_client_groups},
        client_group => [ values %$scg ],
        db_roles => [ values %$role], 
    }, "display_user.tpl");
}


###########################################################

sub get_media_max_size
{
    my ($self, $type) = @_;
    my $query = 
"SELECT avg(VolBytes) AS size
  FROM Media 
 WHERE Media.VolStatus = 'Full' 
   AND Media.MediaType = '$type'
";
    
    my $res = $self->selectrow_hashref($query);

    if ($res) {
        return $res->{size};
    } else {
        return 0;
    }
}

sub update_media
{
    my ($self) = @_ ;

    my $media = $self->get_form('qmedia');

    unless ($media->{qmedia}) {
        return $self->error("Can't get media");
    }

    my $query = "
SELECT Media.Slot         AS slot,
       PoolMedia.Name     AS poolname,
       Media.VolStatus    AS volstatus,
       Media.InChanger    AS inchanger,
       Location.Location  AS location,
       Media.VolumeName   AS volumename,
       Media.MaxVolBytes  AS maxvolbytes,
       Media.MaxVolJobs   AS maxvoljobs,
       Media.MaxVolFiles  AS maxvolfiles,
       Media.VolUseDuration AS voluseduration,
       Media.VolRetention AS volretention,
       Media.Comment      AS comment,
       PoolRecycle.Name   AS poolrecycle,
       Media.Enabled      AS enabled

FROM Media INNER JOIN Pool AS PoolMedia ON (Media.PoolId = PoolMedia.PoolId)
           LEFT  JOIN Pool AS PoolRecycle ON (Media.RecyclePoolId = PoolRecycle.PoolId)
           LEFT  JOIN Location ON (Media.LocationId = Location.LocationId)

WHERE Media.VolumeName = $media->{qmedia}
";

    my $row = $self->dbh_selectrow_hashref($query);
    $row->{volretention} = human_sec($row->{volretention});
    $row->{voluseduration} = human_sec($row->{voluseduration});
    $row->{enabled} = human_enabled($row->{enabled});

    my $elt = $self->get_form(qw/db_pools db_locations/);

    $self->display({
        %$elt,
        %$row,
    }, "update_media.tpl");
}

sub save_location
{
    my ($self) = @_ ;
    $self->can_do('r_media_mgnt');

    my $arg = $self->get_form('jmedias', 'qnewlocation') ;

    unless ($arg->{jmedias}) {
        return $self->error("Can't get selected media");
    }
    
    unless ($arg->{qnewlocation}) {
        return $self->error("Can't get new location");
    }

    my $query = "
 UPDATE Media 
     SET LocationId = (SELECT LocationId 
                       FROM Location 
                       WHERE Location = $arg->{qnewlocation}) 
     WHERE Media.VolumeName IN ($arg->{jmedias})
";

    my $nb = $self->dbh_do($query);

    print "$nb media updated, you may have to update your autochanger.";

    $self->display_media();
}

sub location_change
{
    my ($self) = @_ ;
    $self->can_do('r_media_mgnt');

    my $media = $self->get_selected_media_location();
    unless ($media) {
        return $self->error("Can't get media selection");
    }
    my $newloc = CGI::param('newlocation');

    my $user = CGI::param('user') || 'unknown';
    my $comm = CGI::param('comment') || '';
    $comm = $self->dbh_quote("$user: $comm");

    my $arg = $self->get_form('enabled');
    my $en = from_human_enabled($arg->{enabled});
    my $b = $self->get_bconsole();

    my $query;
    foreach my $vol (keys %$media) {
        $query = "
INSERT INTO LocationLog (Date,Comment,MediaId,LocationId,NewEnabled,NewVolStatus)
 SELECT NOW(), $comm, Media.MediaId, Location.LocationId, $en, VolStatus 
   FROM Media, Location
  WHERE Media.VolumeName = '$vol'
    AND Location.Location = '$media->{$vol}->{location}'
";
        $self->dbh_do($query);
        $self->debug($query);
        $b->send_cmd("update volume=\"$vol\" enabled=$en");
    }
    $b->close();

    my $q = new CGI;
    $q->param('action', 'update_location');
    my $url = $q->url(-full => 1, -query=>1);

    $self->display({ email  => $self->{info}->{email_media},
                     url => $url,
                     newlocation => $newloc,
                     # [ { volumename => 'vol1' }, { volumename => 'vol2'},..]
                     media => [ values %$media ],
                   },
                   "change_location.tpl");

}

sub display_client_stats
{
    my ($self, %arg) = @_ ;
    $self->can_do('r_view_stat');

    my $client = $self->dbh_quote($arg{clientname});
    # get security filter
    my $filter = $self->get_client_filter();

    my ($limit, $label) = $self->get_limit(%arg);
    my $query = "
SELECT 
    count(Job.JobId)     AS nb_jobs,
    sum(Job.JobBytes)    AS nb_bytes,
    sum(Job.JobErrors)   AS nb_err,
    sum(Job.JobFiles)    AS nb_files,
    Client.Name          AS clientname
FROM Job JOIN Client USING (ClientId) $filter
WHERE 
    Client.Name = $client
    $limit 
GROUP BY Client.Name
";

    my $row = $self->dbh_selectrow_hashref($query);

    $row->{ID} = $cur_id++;
    $row->{label} = $label;
    $row->{grapharg} = "client";
    $row->{age} = $arg{age};

    $self->display($row, "display_client_stats.tpl");
}


sub _display_group_stats
{
    my ($self, %arg) = @_ ;

    my $carg = $self->get_form(qw/qclient_group/);

    unless ($carg->{qclient_group}) {
        return $self->error("Can't get group");
    }
    my $jobt = $self->get_stat_table();
    my ($limit, $label) = $self->get_limit(%arg);

    my $query = "
SELECT 
    count(Job.JobId)     AS nb_jobs,
    sum(Job.JobBytes)    AS nb_bytes,
    sum(Job.JobErrors)   AS nb_err,
    sum(Job.JobFiles)    AS nb_files,
    client_group.client_group_name  AS clientname
FROM $jobt AS Job
         JOIN Client USING (ClientId) 
         JOIN client_group_member ON (Client.ClientId = client_group_member.ClientId) 
         JOIN client_group USING (client_group_id)
WHERE 
    client_group.client_group_name = $carg->{qclient_group}
    $limit 
GROUP BY client_group.client_group_name
";

    my $row = $self->dbh_selectrow_hashref($query);

    $row->{ID} = $cur_id++;
    $row->{label} = $label;
    $row->{grapharg} = "client_group";

    $self->display($row, "display_client_stats.tpl");
}

# [ name, num, value, joberrors, nb_job ] =>
# {  items => 
#      [ { name => 'ALL',
#          events => [ { num => 1, label => '2007-01', 
#                        value => 'T', title => 10 },
#                      { num => 2, label => '2007-02', 
#                        value => 'R', title => 11 },
#                     ]
#         },
#         { name => 'Other',
#            ...
#       ]
# };
sub make_overview_tab
{
    my ($self, $q) = @_;
    my $ret = $self->dbh_selectall_arrayref($q);
    my @items;
    my $events=[];
    my $cur_name='';
    for my $elt (@$ret) {
        if ($cur_name and $cur_name ne $elt->[0]) { # order by name, num
            push @items, { name => $cur_name, events => $events};
            $events = [];
        }
        $cur_name = $elt->[0];
        push @$events, 
          { num => $elt->[1], status => $elt->[2], 
            joberrors => $elt->[3], title => "$elt->[4] jobs"};
    }
    push @items, { name => $cur_name, events => $events};
    return \@items;
}

sub get_time_overview
{
    my ($self, $arg) = @_; # want since et age from get_form();
    my $type = $arg->{type} || 'day';
    if ($type =~ /^(day|week|hour|month)$/) {
        $type = uc($1);
    } else {
        $arg->{type}='day';
        $type = 'DAY';
    }
    my $jobt = $self->get_stat_table();
    my $stime1 = $self->{sql}->{"STARTTIME_P" . $type}; # get 1,2,3
    $stime1 =~ s/Job.StartTime/date/;
    my $stime2 = $self->{sql}->{"STARTTIME_" . $type}; # get 2007-01-03, 2007-01-23

    my ($limit, $label) = $self->get_limit('since' => $arg->{since},
                                           'age' => $arg->{age});
    return ($stime1, $stime2, $limit, $label, $jobt);
}

#              lu ma me je ve sa di
#  groupe1     v  v  x  w  v  v  v    overview
#   |-- s1     v  v  v  v  v  v  v    overview_zoom
#   |-- s2     v  v  x  v  v  v  v
#   `-- s3     v  v  v  w  v  v  v
sub display_overview_zoom
{
    my ($self) = @_;
    $self->can_do('r_view_stat');

    my $arg = $self->get_form(qw/jclient_groups age since type level/);

    if (!$arg->{jclient_groups}) {
        return $self->error("Can't get client_group selection");
    }
    my ($filter2, undef) = $self->get_param(qw/level jobtype/);
    my ($stime1, $stime2, $limit, $label, $jobt) = $self->get_time_overview($arg);

    my $filter = $self->get_client_filter();
    my $q = "
SELECT name, $stime1 AS num,
       JobStatus AS value, joberrors, nb_job
FROM (
  SELECT $stime2        AS date,
         Client.Name    AS name,
         MAX(severity)  AS severity,
         COUNT(1)       AS nb_job,
         SUM(JobErrors) AS joberrors
    FROM $jobt AS Job
    JOIN client_group_member USING (ClientId)
    JOIN client_group        USING (client_group_id)
    JOIN Client              USING (ClientId)  $filter
    JOIN Status              USING (JobStatus)
   WHERE client_group_name IN ($arg->{jclient_groups})
     AND JobStatus IN ('T', 'W', 'f', 'A', 'e', 'E')
         $limit $filter2
   GROUP BY Client.Name, date
) AS sub JOIN Status USING (severity)
 ORDER BY name, date
";
    my $items = $self->make_overview_tab($q);
    $self->display({label => $label,
                    action => "job;since=$arg->{since};level=$arg->{level};type=$arg->{type};age=$arg->{age};client=", 
                    items => $items}, "overview.tpl");
}

sub display_overview
{
    my ($self) = @_ ;
    $self->can_do('r_view_stat');

    my $arg = $self->get_form(qw/jclient_groups age since type level/);
    my ($filter2, undef) = $self->get_param(qw/client_groups level jobtype/);
    my $filter3 = $self->get_client_group_filter();
    my ($stime1, $stime2, $filter1, $label, $jobt) = $self->get_time_overview($arg);

    my $q = "
SELECT name, $stime1 AS num, 
       JobStatus AS value, joberrors, nb_job
FROM (
  SELECT $stime2        AS date, 
         client_group_name AS name,
         MAX(severity)  AS severity,
         COUNT(1)       AS nb_job,
         SUM(JobErrors) AS joberrors
    FROM $jobt AS Job
    JOIN client_group_member USING (ClientId)
    JOIN client_group        USING (client_group_id) $filter3
    JOIN Status              USING (JobStatus)
   WHERE JobStatus IN ('T', 'W', 'f', 'A', 'e', 'E')
       $filter1 $filter2
   GROUP BY client_group_name, date
) AS sub JOIN Status USING (severity)
 ORDER BY name, date
";
    my $items = $self->make_overview_tab($q);
    $self->display({label=>$label,
                    action => "overview_zoom;since=$arg->{since};level=$arg->{level};type=$arg->{type};age=$arg->{age};client_group=", 
                    items => $items}, "overview.tpl");

}

# poolname can be undef
sub display_pool
{
    my ($self, $poolname) = @_ ;
    $self->can_do('r_view_media');

    my $whereA = '';
    my $whereW = '';

    my $arg = $self->get_form('jmediatypes', 'qmediatypes');
    if ($arg->{jmediatypes}) { 
        $whereW = "WHERE MediaType IN ($arg->{jmediatypes}) ";
        $whereA = "AND   MediaType IN ($arg->{jmediatypes}) ";
    }
    
# TODO : afficher les tailles et les dates

    my $query = "
SELECT subq.volmax        AS volmax,
       subq.volnum        AS volnum,
       subq.voltotal      AS voltotal,
       Pool.Name          AS name,
       Pool.Recycle       AS recycle,
       Pool.VolRetention  AS volretention,
       Pool.VolUseDuration AS voluseduration,
       Pool.MaxVolJobs    AS maxvoljobs,
       Pool.MaxVolFiles   AS maxvolfiles,
       Pool.MaxVolBytes   AS maxvolbytes,
       subq.PoolId        AS PoolId,
       subq.MediaType     AS mediatype,
       $self->{sql}->{CAT_POOL_TYPE}  AS uniq
FROM
  (
    SELECT COALESCE(media_avg_size.volavg,0) * count(Media.MediaId) AS volmax,
           count(Media.MediaId)  AS volnum,
           sum(Media.VolBytes)   AS voltotal,
           Media.PoolId          AS PoolId,
           Media.MediaType       AS MediaType
    FROM Media
    LEFT JOIN (SELECT avg(Media.VolBytes) AS volavg,
                      Media.MediaType     AS MediaType
               FROM Media 
              WHERE Media.VolStatus = 'Full' 
              GROUP BY Media.MediaType
               ) AS media_avg_size ON (Media.MediaType = media_avg_size.MediaType)
    GROUP BY Media.MediaType, Media.PoolId, media_avg_size.volavg
  ) AS subq
LEFT JOIN Pool ON (Pool.PoolId = subq.PoolId)
$whereW
";

    my $all = $self->dbh_selectall_hashref($query, 'uniq') ;

    $query = "
SELECT Pool.Name AS name,
       sum(VolBytes) AS size
FROM   Media JOIN Pool ON (Media.PoolId = Pool.PoolId)
WHERE  Media.VolStatus IN ('Recycled', 'Purged')
       $whereA
GROUP BY Pool.Name;
";
    my $empty = $self->dbh_selectall_hashref($query, 'name');

    foreach my $p (values %$all) {
        if ($p->{volmax} > 0) { # mysql returns 0.0000
            # we remove Recycled/Purged media from pool usage
            if (defined $empty->{$p->{name}}) {
                $p->{voltotal} -= $empty->{$p->{name}}->{size};
            }
            $p->{poolusage} = sprintf('%.2f', $p->{voltotal} * 100/ $p->{volmax}) ;
        } else {
            $p->{poolusage} = 0;
        }

        $query = "
  SELECT VolStatus AS volstatus, count(MediaId) AS nb
    FROM Media 
   WHERE PoolId=$p->{poolid}
     AND Media.MediaType = '$p->{mediatype}'
         $whereA
GROUP BY VolStatus
";
        my $content = $self->dbh_selectall_hashref($query, 'volstatus');
        foreach my $t (values %$content) {
            $p->{"nb_" . $t->{volstatus}} = $t->{nb} ;
        }
    }

    $self->debug($all);
    $self->display({ ID => $cur_id++,
                     MediaType => $arg->{qmediatypes}, # [ { name => type1 } , { name => type2 } ]
                     Pools => [ values %$all ]},
                   "display_pool.tpl");
}

# With this function, we get an estimation of next jobfiles/jobbytes count
sub get_estimate_query
{
    my ($self, $mode, $job, $level) = @_;
    # get security filter
    my $filter = $self->get_client_filter();

    my $query;
   
    if ($self->dbh_is_mysql()) { # mysql doesn't have statistics functions
        $query = "
SELECT jobname AS jobname, 
       0.1 AS corr_jobbytes, AVG(jobbytes) AS jobbytes,
       COUNT(1) AS nb_jobbytes ";
    } else {
        # postgresql have functions that permit to handle lineal regression
        # in y=ax + b
        # REGR_SLOPE(Y,X) = get x
        # REGR_INTERCEPT(Y,X) = get b
        # and we need y when x=now()
        # CORR gives the correlation
        # (TODO: display progress bar only if CORR > 0.8)
        my $now = scalar(time);
        $query = "
SELECT temp.jobname AS jobname, 
       CORR(jobbytes,jobtdate) AS corr_jobbytes,
       ($now*REGR_SLOPE(jobbytes,jobtdate) 
         + REGR_INTERCEPT(jobbytes,jobtdate)) AS jobbytes,
       COUNT(1) AS nb_jobbytes ";
    }
    # if it's a differential, we need to compare since the last full
    # 
    #   F D D D F D D D      F I I I I D I I I
    # | #     # #     #    | #         #
    # | #   # # #   # #    | #         #
    # | # # # # # # # #    | # # # # # # # # #
    # +-----------------   +-------------------
    my $filter2='';
    if ($level eq 'D') {
        $filter2 = "
AND Job.StartTime > (
 SELECT StartTime 
   FROM Job 
  WHERE Job.Name = '$job' 
    AND Job.Level = 'F' 
    AND Job.JobStatus IN ('T', 'W') 
ORDER BY Job.StartTime DESC LIMIT 1
) ";
    }
    
    $query .= 
"
FROM (
   SELECT Job.Name AS jobname, 
          JobBytes AS jobbytes,
          JobTDate AS jobtdate
   FROM Job INNER JOIN Client USING (ClientId) $filter
   WHERE Job.Name = '$job'
     AND Job.Level = '$level'
     AND Job.JobStatus IN ('T', 'W')
     $filter2
   ORDER BY StartTime DESC
   LIMIT 4
) AS temp GROUP BY temp.jobname
";
 
    if ($mode eq 'jobfiles') {
        $query =~ s/jobbytes/jobfiles/g;
        $query =~ s/JobBytes/JobFiles/g;
    }
    return $query;
}

sub display_running_job
{
    my ($self) = @_;
    return if $self->cant_do('r_view_running_job');

    my $arg = $self->get_form('jobid');

    return $self->error("Can't get jobid") unless ($arg->{jobid});

    # get security filter
    my $filter = $self->get_client_filter();

    my $query = "
SELECT Client.Name AS name, Job.Name AS jobname, 
       Job.Level AS level, Type AS type, JobStatus AS jobstatus
FROM Job INNER JOIN Client USING (ClientId) $filter
WHERE Job.JobId = $arg->{jobid}
";

    my $row = $self->dbh_selectrow_hashref($query);

    if ($row) {
        $arg->{client} = $row->{name};
    } else {
        return $self->error("Can't get client");
    }

    my $status = $row->{jobstatus};

    if ($status =~ /[TfAaEWD]/) {
        $self->display_job_zoom($arg->{jobid});
        $self->get_job_log();
        return;
    }

    if ($row->{type} eq 'B') {
        # for jobfiles, we use only last Full backup. status client= returns
        # all files that have been checked
        my $query1 = $self->get_estimate_query('jobfiles', $row->{jobname}, 'F');
        my $query2 = $self->get_estimate_query('jobbytes', 
                                               $row->{jobname}, $row->{level});

        # LEFT JOIN because we always have a previous Full
        $query = "
SELECT  corr_jobbytes, jobbytes, corr_jobfiles, jobfiles
  FROM ($query1) AS A LEFT JOIN ($query2) AS B USING (jobname)
";
        $row = $self->dbh_selectrow_hashref($query);

        if (!$row) {
            $row->{jobbytes} = $row->{jobfiles} = 0;
        }
    }
    if ($status =~ /[RBSmMsjlL]/) {
        my $cli = new Bweb::Client(name => $arg->{client});
        $cli->display_running_job($self, $arg->{jobid}, $row);
    }
    if ($arg->{jobid}) {
        $self->get_job_log();
    }
}

sub display_running_jobs
{
    my ($self, $display_action) = @_;
    return if $self->cant_do('r_view_running_job');

    # get security filter
    my $filter = $self->get_client_filter();

    my $query = "
SELECT Job.JobId AS jobid, 
       Job.Name  AS jobname,
       Job.Level     AS level,
       Job.StartTime AS starttime,
       Job.JobFiles  AS jobfiles,
       Job.JobBytes  AS jobbytes,
       Job.JobStatus AS jobstatus,
$self->{sql}->{SEC_TO_TIME}(  $self->{sql}->{UNIX_TIMESTAMP}(NOW())  
                            - $self->{sql}->{UNIX_TIMESTAMP}(StartTime)) 
         AS duration,
       Client.Name AS clientname
FROM Job INNER JOIN Client USING (ClientId) $filter
WHERE 
  JobStatus IN ('C','R','B','e','D','F','S','m','M','s','j','c','d','t','p')
";      
    my $all = $self->dbh_selectall_hashref($query, 'jobid') ;
    
    $self->display({ ID => $cur_id++,
                     display_action => $display_action,
                     Jobs => [ values %$all ]},
                   "running_job.tpl") ;
}

sub display_group_stats
{
    my ($self) = @_;
    my $arg = $self->get_form('age', 'since');
    return if $self->cant_do('r_view_stat');

    my $filter = $self->get_client_group_filter();

    my $jobt = $self->get_stat_table();

    my ($limit, $label) = $self->get_limit(%$arg);
    my ($where, undef) = $self->get_param('client_groups', 'level');

    my $query = "
SELECT client_group_name AS name, nb_byte, nb_file, nb_job, nb_err, nb_resto
  FROM (

    SELECT sum(JobBytes) AS nb_byte,
           sum(JobFiles) AS nb_file,
           count(1) AS nb_job, client_group_name 
      FROM $jobt AS Job JOIN client_group_member USING (ClientId) 
      JOIN client_group USING (client_group_id) $filter
     WHERE JobStatus IN ('T', 'W') AND Type IN ('M', 'B', 'g') 
           $where $limit
    GROUP BY client_group_name ORDER BY client_group_name

  ) AS T1 LEFT JOIN (

    SELECT count(1) AS nb_err, client_group_name 
      FROM $jobt AS Job JOIN client_group_member USING (ClientId) 
      JOIN client_group USING (client_group_id)
     WHERE JobStatus IN ('E','e','f','A') AND Type = 'B'
           $where $limit
    GROUP BY client_group_name ORDER BY client_group_name

  ) AS T3 USING (client_group_name) LEFT JOIN (

    SELECT count(1) AS nb_resto, client_group_name 
      FROM $jobt AS Job JOIN client_group_member USING (ClientId) 
      JOIN client_group USING (client_group_id)
     WHERE JobStatus IN ('T','W') AND Type = 'R'
           $where $limit
    GROUP BY client_group_name ORDER BY client_group_name

  ) AS T2 USING (client_group_name)
";
    $self->debug($query);
    my $all = $self->dbh_selectall_hashref($query, 'name') ;
    $self->debug($all);

    $self->display({ ID => $cur_id++,
                     label => $label,
                     Stats => [ values %$all ]},
                   "display_stats.tpl") ;
}

# return the autochanger list to update
sub eject_media
{
    my ($self) = @_;
    $self->can_do('r_media_mgnt');

    my %ret; 
    my $arg = $self->get_form('jmedias');

    unless ($arg->{jmedias}) {
        return $self->error("Can't get media selection");
    }

    my $query = "
SELECT Media.VolumeName  AS volumename,
       Storage.Name      AS storage,
       Location.Location AS location,
       Media.Slot        AS slot
FROM Media INNER JOIN Storage  ON (Media.StorageId  = Storage.StorageId)
           LEFT  JOIN Location ON (Media.LocationId = Location.LocationId)
WHERE Media.VolumeName IN ($arg->{jmedias})
  AND Media.InChanger = 1
";

    my $all = $self->dbh_selectall_hashref($query, 'volumename');

    foreach my $vol (values %$all) {
        my $a = $self->ach_get($vol->{location});
        next unless ($a) ;
        $ret{$vol->{location}} = 1;

        unless ($a->{have_status}) {
            $a->status();
            $a->{have_status} = 1;
        }
        # TODO: set enabled
        print "eject $vol->{volumename} from $vol->{storage} : ";
        if ($a->send_to_io($vol->{slot})) {
            print "<img src='/bweb/T.png' alt='ok'><br/>";
        } else {
            print "<img src='/bweb/E.png' alt='err'><br/>";
        }
    }
    return keys %ret;
}

sub move_email
{
    my ($self) = @_;

    my ($to, $subject, $content) = (CGI::param('email'),
                                    CGI::param('subject'),
                                    CGI::param('content'));
    $to =~ s/[^\w\d\.\@<>,]//;
    $subject =~ s/[^\w\d\.\[\]]/ /;    

    open(MAIL, "|mail -s '$subject' '$to'") ;
    print MAIL $content;
    close(MAIL);

    print "Mail sent";
}

sub restore
{
    my ($self) = @_;
    
    my $arg = $self->get_form('jobid', 'client');

    print CGI::header('text/brestore');
    print "jobid=$arg->{jobid}\n" if ($arg->{jobid});
    print "client=$arg->{client}\n" if ($arg->{client});
    print "\n\nYou have to assign this mime type with /usr/bin/brestore.pl\n";
    print "\n";
}

# TODO : move this to Bweb::Autochanger ?
# TODO : make this internal to not eject tape ?
use Bconsole;


sub ach_get
{
    my ($self, $name) = @_;
    
    unless ($name) {
        return $self->error("Can't get your autochanger name ach");
    }

    unless ($self->{info}->{ach_list}) {
        return $self->error("Could not find any autochanger");
    }
    
    my $a = $self->{info}->{ach_list}->{$name};

    unless ($a) {
        $self->error("Can't get your autochanger $name from your ach_list");
        return undef;
    }

    $a->{bweb}  = $self;
    $a->{debug} = $self->{debug};

    return $a;
}

sub ach_register
{
    my ($self, $ach) = @_;
    $self->can_do('r_configure');

    $self->{info}->{ach_list}->{$ach->{name}} = $ach;

    $self->{info}->save();
    
    return 1;
}

sub ach_edit
{
    my ($self) = @_;
    $self->can_do('r_configure');

    my $arg = $self->get_form('ach');
    if (!$arg->{ach} 
        or !$self->{info}->{ach_list} 
        or !$self->{info}->{ach_list}->{$arg->{ach}}) 
    {
        return $self->error("Can't get autochanger name");
    }

    my $ach = $self->{info}->{ach_list}->{$arg->{ach}};

    my $i=0;
    $ach->{drives} = 
        [ map { { name => $_, index => $i++ } } @{$ach->{drive_name}} ] ;

    my $b = $self->get_bconsole();

    my @storages = $b->list_storage() ;

    $ach->{devices} = [ map { { name => $_ } } @storages ];
    
    $self->display($ach, "ach_add.tpl");
    delete $ach->{drives};
    delete $ach->{devices};
    return 1;
}

sub ach_del
{
    my ($self) = @_;
    $self->can_do('r_configure');

    my $arg = $self->get_form('ach');

    if (!$arg->{ach} 
        or !$self->{info}->{ach_list} 
        or !$self->{info}->{ach_list}->{$arg->{ach}}) 
    {
        return $self->error("Can't get autochanger name");
    }
   
    delete $self->{info}->{ach_list}->{$arg->{ach}} ;
   
    $self->{info}->save();
    $self->{info}->view();
}

sub ach_add
{
    my ($self) = @_;
    $self->can_do('r_configure');

    my $arg = $self->get_form('ach', 'mtxcmd', 'device', 'precmd');

    my $b = $self->get_bconsole();
    my @storages = $b->list_storage() ;

    unless ($arg->{ach}) {
        $arg->{devices} = [ map { { name => $_ } } @storages ];
        return $self->display($arg, "ach_add.tpl");
    }

    my @drives ;
    foreach my $drive (CGI::param('drives'))
    {
        unless (grep(/^$drive$/,@storages)) {
            return $self->error("Can't find $drive in storage list");
        }

        my $index = CGI::param("index_$drive");
        unless (defined $index and $index =~ /^(\d+)$/) {
            return $self->error("Can't get $drive index");
        }

        $drives[$index] = $drive;
    }

    unless (@drives) {
        return $self->error("Can't get drives from Autochanger");
    }

    my $a = new Bweb::Autochanger(name   => $arg->{ach},
                                  precmd => $arg->{precmd},
                                  drive_name => \@drives,
                                  device => $arg->{device},
                                  mtxcmd => $arg->{mtxcmd});

    $self->ach_register($a) ;
    
    $self->{info}->view();
}

sub delete
{
    my ($self) = @_;
    $self->can_do('r_delete_job');

    my $arg = $self->get_form('jobid');

    if ($arg->{jobid}) {
        my $b = $self->get_bconsole();
        my $ret = $b->send_cmd("delete jobid=\"$arg->{jobid}\"");

        $self->display({
            content => $ret,
            title => "Delete a job ",
            name => "delete jobid=$arg->{jobid}",
            id => $cur_id++,
        }, "command.tpl");      
    }
}

sub do_update_media
{
    my ($self) = @_ ;
    $self->can_do('r_media_mgnt');

    my $arg = $self->get_form(qw/media volstatus inchanger pool
                                 slot volretention voluseduration 
                                 maxvoljobs maxvolfiles maxvolbytes
                                 qcomment poolrecycle enabled
                              /);

    unless ($arg->{media}) {
        return $self->error("Can't find media selection");
    }

    my $update = "update volume=$arg->{media} ";

    if ($arg->{volstatus}) {
        $update .= " volstatus=$arg->{volstatus} ";
    }
    
    if ($arg->{inchanger}) {
        $update .= " inchanger=yes " ;
        if ($arg->{slot}) {
            $update .= " slot=$arg->{slot} ";
        }
    } else {
        $update .= " slot=0 inchanger=no ";
    }

    if ($arg->{enabled}) {
        $update .= " enabled=$arg->{enabled} ";
    }

    if ($arg->{pool}) {
        $update .= " pool=$arg->{pool} " ;
    }

    if (defined $arg->{volretention}) {
        $update .= " volretention=\"$arg->{volretention}\" " ;
    }

    if (defined $arg->{voluseduration}) {
        $update .= " voluse=\"$arg->{voluseduration}\" " ;
    }

    if (defined $arg->{maxvoljobs}) {
        $update .= " maxvoljobs=$arg->{maxvoljobs} " ;
    }
    
    if (defined $arg->{maxvolfiles}) {
        $update .= " maxvolfiles=$arg->{maxvolfiles} " ;
    }    

    if (defined $arg->{maxvolbytes}) {
        $update .= " maxvolbytes=$arg->{maxvolbytes} " ;
    }    

    if (defined $arg->{poolrecycle}) {
        $update .= " recyclepool=\"$arg->{poolrecycle}\" " ;
    }        
    
    my $b = $self->get_bconsole();

    $self->display({
        content => $b->send_cmd($update),
        title => "Update a volume ",
        name => $update,
        id => $cur_id++,
        hide_output => 1,
    }, "command.tpl");  


    my @q;
    my $media = $self->dbh_quote($arg->{media});

    my $loc = CGI::param('location') || '';
    if ($loc) {
        $loc = $self->dbh_quote($loc); # is checked by db
        push @q, "LocationId=(SELECT LocationId FROM Location WHERE Location=$loc)";
    }
    if (!$arg->{qcomment}) {
        $arg->{qcomment} = "''";
    }
    push @q, "Comment=$arg->{qcomment}";
    

    my $query = "
UPDATE Media 
   SET " . join (',', @q) . "
 WHERE Media.VolumeName = $media
";
    $self->dbh_do($query);

    $self->update_media();
}

sub update_slots
{
    my ($self) = @_;
    $self->can_do('r_autochanger_mgnt');

    my $ach = CGI::param('ach') ;
    $ach = $self->ach_get($ach);
    unless ($ach) {
        return $self->error("Bad autochanger name");
    }

    $self->display({
        title => "Scanning autochanger content ",
        name => "update slots",
        notail => 1,
        id => $cur_id++,
    }, "command.tpl");  
    my $b = new Bconsole(pref => $self->{info},timeout => 60,log_stdout => 1);
    $b->update_slots($ach->{name});

    $self->display({
        nohead => 1,
    }, "command.tpl");  
}

sub get_job_log
{
    my ($self) = @_;
    $self->can_do('r_view_log');

    my $arg = $self->get_form('jobid', 'limit', 'offset');
    unless ($arg->{jobid}) {
        return $self->error("Can't get jobid");
    }

    if ($arg->{limit} == 100) {
        $arg->{limit} = 1000;
    }
    # get security filter
    my $filter = $self->get_client_filter();

    my $query = "
SELECT Job.Name as name, Client.Name as clientname
 FROM  Job INNER JOIN Client USING (ClientId) $filter
 WHERE JobId = $arg->{jobid}
";

    my $row = $self->dbh_selectrow_hashref($query);

    unless ($row) {
        return $self->error("Can't find $arg->{jobid} in catalog");
    }

    # display only Error and Warning messages
    $filter = '';
    if (CGI::param('error')) {
        $filter = " AND LogText $self->{sql}->{MATCH} 'Error|Warning|ERR=' ";
    }

    my $logtext;
    if (CGI::param('time') || $self->{info}->{display_log_time}) {
        $logtext = $self->dbh_strcat('Time', " ' ' ", 'LogText');
    } else {
        $logtext = 'LogText';
    }

    $query = "
SELECT count(1) AS nbline, 
       GROUP_CONCAT($logtext $self->{sql}->{CONCAT_SEP}) AS logtxt, id
  FROM  (
    SELECT 1 AS id, Time, LogText
    FROM Log 
   WHERE ( Log.JobId = $arg->{jobid} 
      OR (Log.JobId = 0 
          AND Time >= (SELECT StartTime FROM Job WHERE JobId=$arg->{jobid}) 
          AND Time <= (SELECT COALESCE(EndTime,NOW()) FROM Job WHERE JobId=$arg->{jobid})
       ) ) $filter
 ORDER BY LogId
 LIMIT $arg->{limit}
 OFFSET $arg->{offset}
 ) AS temp
 GROUP BY id

";

    my $log = $self->dbh_selectrow_hashref($query);
    unless ($log) {
        return $self->error("Can't get log for jobid $arg->{jobid}");
    }
    $log->{logtxt} =~ s/\0//g;
    $self->display({ lines=> $log->{logtxt},
                     nbline => $log->{nbline},
                     jobid => $arg->{jobid},
                     name  => $row->{name},
                     client => $row->{clientname},
                     offset => $arg->{offset},
                     limit  => $arg->{limit},
                 }, 'display_log.tpl');
}

sub cancel_future_job
{
    my ($self) = @_;
    $self->can_do('r_cancel_job');

    my $arg = $self->get_form(qw/job pool level client when/);

    if ( !$arg->{job} or !$arg->{pool} or !$arg->{level} 
         or !$arg->{client} or !$arg->{when})
    {
        return $self->error("Can't get enough information to mark this job as canceled");
    }

    $arg->{level} =~ s/^(.).+/$1/; # we keep the first letter
    my $jobtable = $self->{info}->{stat_job_table} || 'JobHisto';

    if ($jobtable =~ /^Job$/i) {
        return $self->error("Can add records only in history table");
    }
    my $jname = "$arg->{job}.$arg->{when}";
    $jname =~ s/\s/_/g;

    my $found = $self->dbh_selectrow_hashref("
SELECT 1
  FROM $jobtable
 WHERE JobId = 0
   AND Job = '$jname'
   AND Name = '$arg->{job}'
");
    if ($found) {
        return $self->error("$jname is already in history table");
    }

        $self->dbh_do("
INSERT INTO $jobtable 
  (JobId, Name, Job, Type, Level, JobStatus, SchedTime, StartTime, EndTime, 
   RealEndTime, ClientId, PoolId) 
 VALUES 
  (0, '$arg->{job}', '$jname', 'B', '$arg->{level}', 'A',
   '$arg->{when}', '$arg->{when}', '$arg->{when}', '$arg->{when}',
   (SELECT ClientId FROM Client WHERE Name = '$arg->{client}'),
   (SELECT PoolId FROM Pool WHERE Name = '$arg->{pool}')
  )
");
    $self->display({ Filter => "Dummy record for $jname",
                     ID => 1,
                     Jobs => 
                         [{ jobid => 0,
                            client => $arg->{client},
                            jobname => $arg->{job},
                            pool => $arg->{pool},
                            level => $arg->{level},
                            starttime => $arg->{when},
                            duration => '00:00:00',
                            jobfiles => 0,
                            jobbytes => 0,
                            joberrors => 0,
                            jobstatus => 'A',
                     }]
                   },
                   "display_job.tpl");
}

sub add_media
{
    my ($self) = @_ ;
    $self->can_do('r_media_mgnt');
    my $arg = $self->get_form('storage', 'pool', 'nb', 'media', 'offset');
    my $b = $self->get_bconsole();

    if (!$arg->{storage} || !$arg->{pool} || not defined $arg->{nb} || !$arg->{media} || !$arg->{offset}) {
        CGI::param(offset => 0);
        $arg = $self->get_form('db_pools');
        $arg->{storage} = [ map { { name => $_ } }$b->list_storage()];
        $self->display($arg, 'add_media.tpl');
        return 1;
    }
    $b->connect();
    $b->send("add pool=\"$arg->{pool}\" storage=\"$arg->{storage}\"\n");
    if ($arg->{nb} > 0) {
        $arg->{offset} = $arg->{offset}?$arg->{offset}:1; 
        $b->send("$arg->{nb}\n");
        $b->send("$arg->{media}\n");
        $b->send("$arg->{offset}\n");

    } else {
        $b->send("0\n");
        $b->send("$arg->{media}\n");
    }

    $b->expect_it('-re','^[*]');

    CGI::param('media', '');
    CGI::param('re_media', $arg->{media});
    $self->display_media();
}

sub label_barcodes
{
    my ($self) = @_ ;
    $self->can_do('r_autochanger_mgnt');

    my $arg = $self->get_form('ach', 'slots', 'drive', 'pool');

    unless ($arg->{ach}) {
        return $self->error("Can't find autochanger name");
    }

    my $a = $self->ach_get($arg->{ach});
    unless ($a) {
        return $self->error("Can't find autochanger name in configuration");
    } 

    my $storage = $a->get_drive_name($arg->{drive});
    unless ($storage) {
        return $self->error("Can't get your drive name");
    }

    my $slots = '';
    my $slots_sql = '';
    my $t = 300 ;
    if ($arg->{slots}) {
        $slots = join(",", @{ $arg->{slots} });
        $slots_sql = " AND Slot IN ($slots) ";
        $t += 60*scalar( @{ $arg->{slots} }) ;
    }
    my $pool = $arg->{pool} || 'Scratch';
    my $b = new Bconsole(pref => $self->{info}, timeout => $t,log_stdout => 1);
    print "<h1>This command can take long time, be patient...</h1>";
    print "<pre>" ;
    $b->label_barcodes(storage => $storage,
                       drive => $arg->{drive},
                       pool  => $pool,
                       slots => $slots) ;
    $b->close();
    print "</pre>";

    $self->dbh_do("
  UPDATE Media 
       SET LocationId =   (SELECT LocationId 
                             FROM Location 
                            WHERE Location = '$arg->{ach}')

     WHERE (LocationId = 0 OR LocationId IS NULL)
       $slots_sql
");

}

sub purge
{
    my ($self) = @_;
    $self->can_do('r_purge');

    my @volume = CGI::param('media');

    unless (@volume) {
        return $self->error("Can't get media selection");
    }

    my $b = new Bconsole(pref => $self->{info}, timeout => 60);

    foreach my $v (@volume) {
        $self->display({
            content => $b->purge_volume($v),
            title => "Purge media",
            name => "purge volume=$v",
            id => $cur_id++,
        }, "command.tpl");
    }   
    $b->close();
}

sub prune
{
    my ($self) = @_;
    $self->can_do('r_prune');

    my @volume = CGI::param('media');
    unless (@volume) {
        return $self->error("Can't get media selection");
    }

    my $b = new Bconsole(pref => $self->{info}, timeout => 60);

    foreach my $v (@volume) {
        $self->display({
            content => $b->prune_volume($v),
            title => "Prune volume",
            name => "prune volume=$v",
            id => $cur_id++,
        }, "command.tpl");
    }
    $b->close();
}

sub cancel_job
{
    my ($self) = @_;
    $self->can_do('r_cancel_job');

    my $arg = $self->get_form('jobid');
    unless ($arg->{jobid}) {
        return $self->error("Can't get jobid");
    }

    my $b = $self->get_bconsole();
    $self->display({
        content => $b->cancel($arg->{jobid}),
        title => "Cancel job",
        name => "cancel jobid=$arg->{jobid}",
        id => $cur_id++,
    }, "command.tpl");  
}

sub fileset_view
{
    # Warning, we display current fileset
    my ($self) = @_;

    my $arg = $self->get_form('fileset');

    if ($arg->{fileset}) {
        my $b = $self->get_bconsole();
        my $ret = $b->get_fileset($arg->{fileset});
        $self->display({ fileset => $arg->{fileset},
                         %$ret,
                     }, "fileset_view.tpl");
    } else {
        $self->error("Can't get fileset name");
    }
}

sub director_show_sched
{
    my ($self) = @_ ;
    $self->can_do('r_view_job');
    my $arg = $self->get_form('days');

    my $b = $self->get_bconsole();
    my $ret = $b->director_get_sched( $arg->{days} );

    $self->display({
        id => $cur_id++,
        list => $ret,
    }, "scheduled_job.tpl");
}

sub enable_disable_job
{
    my ($self, $what) = @_ ;
    $self->can_do('r_run_job');

    my $arg = $self->get_form('job');
    if (!$arg->{job}) {
        return $self->error("Can't find job name");
    }

    my $b = $self->get_bconsole();

    my $cmd;
    if ($what) {
        $cmd = "enable";
    } else {
        $cmd = "disable";
    }

    $self->display({
        content => $b->send_cmd("$cmd job=\"$arg->{job}\""),
        title => "$cmd $arg->{job}",
        name => "$cmd job=\"$arg->{job}\"",
        id => $cur_id++,
    }, "command.tpl");  
}

sub get_bconsole
{
    my ($self) = @_;
    return new Bconsole(pref => $self->{info});
}

sub cmd_storage
{
    my ($self) = @_;
    $self->can_do('r_storage_mgnt');
    my $arg = $self->get_form(qw/storage storage_cmd drive slot/);
    my $b = $self->get_bconsole();

    if ($arg->{storage} and $arg->{storage_cmd}) {
        my $cmd = "$arg->{storage_cmd} storage=\"$arg->{storage}\" drive=$arg->{drive} slot=$arg->{slot}";
        my $ret = $b->send_cmd($cmd);

        $self->display({
            content => $ret,
            title => "Storage ",
            name => $cmd,
            id => $cur_id++,
        }, "command.tpl");              
    } else {
        my $storages= [ map { { name => $_ } } $b->list_storage()];
        $self->display({ storage => $storages}, "cmd_storage.tpl");
    }
}

sub run_job_select
{
    my ($self) = @_;
    $self->can_do('r_run_job');

    my $b = $self->get_bconsole();

    my $joblist = [ map { { name => $_ } } $b->list_backup() ];

    $self->display({ Jobs => $joblist }, "run_job.tpl");
}

sub run_parse_job
{
    my ($self, $ouput) = @_;

    my %arg;
    $self->debug($ouput);
    foreach my $l (split(/\r?\n/, $ouput)) {
        $self->debug($l);
        if ($l =~ /(\w+): name=([\w\d\.\s-]+?)(\s+\w+=.+)?$/) {
            $arg{$1} = $2;
            $l = $3 
                if ($3) ;
        } 

        if (my @l = $l =~ /(\w+)=([\w\d*]+)/g) {
            %arg = (%arg, @l);
        }
    }

    my %lowcase ;
    foreach my $k (keys %arg) {
        $lowcase{lc($k)} = $arg{$k} ;
    }
    $self->debug(\%lowcase);
    return \%lowcase;
}

sub run_job_mod
{
    my ($self) = @_;
    $self->can_do('r_run_job');

    my $b = $self->get_bconsole();
    my $arg = $self->get_form(qw/pool level client fileset storage media job/);

    if (!$arg->{job}) {
        return $self->error("Can't get job name");
    }

    # we take informations from director, and we overwrite with user wish
    my $info = $b->send_cmd("show job=\"$arg->{job}\"");
    my $attr = $self->run_parse_job($info);
    
    if (!$arg->{pool} and $arg->{media}) {
        my $r = $self->dbh_selectrow_hashref("
SELECT Pool.Name AS name
  FROM Media JOIN Pool USING (PoolId)
 WHERE Media.VolumeName = '$arg->{media}'
   AND Pool.Name != 'Scratch'
");
        if ($r) {
            $arg->{pool} = $r->{name};
        }
    }

    my %job_opt = (%$attr, %$arg);
    
    my $jobs   = [ map {{ name => $_ }} $b->list_job() ];

    my $pools  = [ map { { name => $_ } } $b->list_pool() ];
    my $clients = [ map { { name => $_ } }$b->list_client()];
    my $filesets= [ map { { name => $_ } }$b->list_fileset() ];
    my $storages= [ map { { name => $_ } }$b->list_storage()];

    $self->display({
        jobs     => $jobs,
        pools    => $pools,
        clients  => $clients,
        filesets => $filesets,
        storages => $storages,
        %job_opt,
    }, "run_job_mod.tpl");
}

sub run_job
{
    my ($self) = @_;
    $self->can_do('r_run_job');

    my $b = $self->get_bconsole();
    
    my $jobs   = [ map {{ name => $_ }} $b->list_job() ];

    $self->display({
        jobs     => $jobs,
    }, "run_job.tpl");
}

sub run_job_now
{
    my ($self) = @_;
    $self->can_do('r_run_job');

    my $b = $self->get_bconsole();
    
    # TODO: check input (don't use pool, level)

    my $arg = $self->get_form(qw/pool level client priority when 
                                 fileset job storage/);
    if (!$arg->{job}) {
        return $self->error("Can't get your job name");
    }

    my $jobid = $b->run(job => $arg->{job},
                        client => $arg->{client},
                        priority => $arg->{priority},
                        level => $arg->{level},
                        storage => $arg->{storage},
                        pool => $arg->{pool},
                        fileset => $arg->{fileset},
                        when => $arg->{when},
                        );

    print $b->{error};    

    print "<br>You can follow job (jobid=$jobid) execution <a href='?action=dsp_cur_job;client=$arg->{client};jobid=$jobid'> here </a><script type='text/javascript' language='JavaScript'>setTimeout(function() { window.location='?action=dsp_cur_job;client=$arg->{client};jobid=$jobid'},2000);</script>";
}

sub display_next_job
{
    my ($self) = @_;

    my $arg = $self->get_form(qw/job begin end/);
    if (!$arg->{job}) {
        return $self->error("Can't get job name");
    }

    my $b = $self->get_bconsole();

    my $job = $b->send_cmd("show job=\"$arg->{job}\"");
    my $attr = $self->run_parse_job($job);
    
    if (!$attr->{schedule}) {
        return $self->error("Can't get $arg->{job} schedule");
    }
    my $jpool=$attr->{pool} || '';

    my $sched = new Bweb::Sched(bconsole => $b, name => $attr->{schedule},
                                begin => $arg->{begin}, end => $arg->{end});

    my $ss = $sched->get_scheds($attr->{schedule}); 
    my @ret;

    foreach my $s (@$ss) {
        my $level = $sched->get_level($s);
        my $pool  = $sched->get_pool($s) || $jpool;
        my $evt = $sched->get_event($s);
        push @ret, map { "$_ : $pool ($level)\n" } @$evt;
    }
    
    print "<b>$arg->{job}:</b><pre>", sort @ret, "</pre><br>";
}

# permit to verify for higher level backup
# we attempt a Increment, we made a Full, that ok
# TODO: Pool may have change
sub get_higher_level
{
    my ($self, $level) = @_;
    if ($level eq 'F') {
        return "'F'";
    } elsif ($level eq 'D') {
        return "'F', 'D'";
    } elsif ($level eq 'I') {
        return "'F', 'D', 'I'";
    }
    return "''";
}

# check jobs against their schedule
sub check_job
{
    my ($self, $sched, $schedname, $job, $job_pool, $client, $type) = @_;
    return undef if (!$self->can_view_client($client));

    my $sch = $sched->get_scheds($schedname);    
    return undef if (!$sch);

    my @ret;
    foreach my $s (@$sch) {
        my $pool;
        if ($type eq 'B') {     # we take the pool only for backup job
            $pool = $sched->get_pool($s) || $job_pool;
        }
        my $level = $sched->get_level($s);
        my ($l) = ($level =~ m/^(.)/); # we keep the first letter
        $l = $self->get_higher_level($l);
        my $evts = $sched->get_event($s);
        my $end = $sched->{end}; # this backup must have start before the next one
        foreach my $evt (reverse @$evts) {
            my $all = $self->dbh_selectrow_hashref("
 SELECT 1
   FROM Job
   JOIN Client USING (ClientId) LEFT JOIN Pool USING (PoolId)
  WHERE Job.StartTime >= '$evt' 
    AND Job.StartTime <  '$end'
    AND Job.Name = '$job'
    AND Job.Type = '$type'
    AND Job.JobStatus IN ('T', 'W')
    AND Job.Level IN ($l)
" . ($pool?" AND Pool.Name = '$pool' ":'') . "
    AND Client.Name = '$client'
 LIMIT 1
");             
            if ($all) {
#               print "ok $job ";
            } else {
                push @{$self->{tmp}}, {date => $evt, level => $level,
                                       type => 'Backup', name => $job,
                                       pool => $pool, volume => $pool,
                                       client => $client};
            }
            $end = $evt;
        }
    }
}

sub display_missing_job
{
    my ($self) = @_;
    my $arg = $self->get_form(qw/begin end age/);

    if (!$arg->{begin}) { # TODO: change this
        $arg->{begin} = strftime('%F %T', localtime(time - $arg->{age}));
    }
    if (!$arg->{end}) {
        $arg->{end} = strftime('%F %T', localtime(time));
    }
    $self->{tmp} = [];          # check_job use this for result

    my $bconsole = $self->get_bconsole();

    my $sched = new Bweb::Sched(bconsole => $bconsole,
                                begin => $arg->{begin},
                                end => $arg->{end});

    my $job = $bconsole->send_cmd("show job");
    my ($jname, $jsched, $jclient, $jpool, $jtype);
    foreach my $j (split(/\r?\n/, $job)) {
        if ($j =~ /Job: name=([\w\d\-]+?) JobType=(\d+)/i) {
            if ($jname and $jsched) {
                $self->check_job($sched, $jsched, $jname, 
                                 $jpool, $jclient, $jtype);
            }
            $jname = $1;
            $jtype = chr($2);
            $jclient = $jpool = $jsched = undef;
        } elsif ($j =~ /Client: name=(.+?) address=/i) {
            $jclient = $1;
        } elsif ($j =~ /Pool: name=([\w\d\-]+) PoolType=/i) {
            $jpool = $1;
        } elsif ($j =~ /Schedule: name=([\w\d\-]+)/i) {
            $jsched = $1;
        }
    }
    $self->display({
        id => $cur_id++,
        title => "Missing Jobs (from $arg->{begin} to $arg->{end})",
        list => $self->{tmp},
        wiki_url => $self->{info}->{wiki_url},
        missing_mode => 1,
    }, "scheduled_job.tpl");

    delete $self->{tmp};
}

1;
