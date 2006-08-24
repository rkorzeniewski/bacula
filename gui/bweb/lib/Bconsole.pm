use strict;

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


################################################################
# Manage with Expect the bconsole tool
package Bconsole;
use Expect;
use POSIX qw/_exit/;

# my $pref = new Pref(config_file => 'brestore.conf');
# my $bconsole = new Bconsole(pref => $pref);
sub new
{
    my ($class, %arg) = @_;

    my $self = bless {
	pref => $arg{pref},	# Pref object
	bconsole => undef,	# Expect object
	log_stdout => $arg{log_stdout} || 0,
	timeout => $arg{debug} || 10,
	debug   => $arg{debug} || 0,
    };

    return $self;
}

sub run
{
    my ($self, %arg) = @_;

    my $cmd = 'run ';
    for my $key (keys %arg) {
	if ($arg{$key}) {
	    $arg{$key} =~ tr/""/  /;
	    $cmd .= "$key=\"$arg{$key}\" ";
	}
    }

    unless ($self->connect()) {
	return 0;
    }

    print "=> $cmd yes\n";

    $self->{bconsole}->clear_accum();
    $self->send("$cmd yes\n");
    $self->expect_it('-re',qr/^[*]/);
    my $ret = $self->before();
    if ($ret =~ /jobid=(\d+)/is) {
	return $1;
    } else {
	return 0;
    }
}

sub send
{
    my ($self, $what) = @_;
    $self->{bconsole}->send($what);
}

sub expect_it
{
    my ($self, @what) = @_;
    unless ($self->{bconsole}->expect($self->{timeout}, @what)) {
	$self->{error} = $!;
	return 0;
    }
    return 1;
}

sub connect
{
    my ($self) = @_;

    if ($self->{error}) {
	return 0 ;
    }

    unless ($self->{bconsole}) {
	my @cmd = split(/\s+/, $self->{pref}->{bconsole}) ;
	unless (@cmd) {
	    $self->{error} = "bconsole string not found";
	    return 0;
	}
	$self->{bconsole} = new Expect;
	$self->{bconsole}->raw_pty(0);
	$self->{bconsole}->debug($self->{debug});
	$self->{bconsole}->log_stdout($self->{debug} || $self->{log_stdout});

	# WARNING : die is trapped by gtk_main_loop()
	# and exit() closes DBI connection 
	my $ret;
	{ 
	    my $sav = $SIG{__DIE__};
	    $SIG{__DIE__} = sub {  _exit 1 ;};
	    $ret = $self->{bconsole}->spawn(@cmd) ;
	    $SIG{__DIE__} = $sav;
	}

	unless ($ret) {
	    $self->{error} = $!;
	    return 0;
	}
	
	# TODO : we must verify that expect return the good value

	$self->expect_it('*');
	$self->send_cmd('gui on');
    }
    return 1 ;
}

sub cancel
{
    my ($self, $jobid) = @_;
    return $self->send_cmd("cancel jobid=$jobid");
}

# get text between to expect
sub before
{
    my ($self) = @_;
    return $self->{bconsole}->before();
}

sub send_cmd
{
    my ($self, $cmd) = @_;
    unless ($self->connect()) {
	return '';
    }
    $self->send("$cmd\n");
    $self->expect_it($cmd);
    $self->{bconsole}->clear_accum();
    $self->expect_it('-re',qr/^[*]/);
    return $self->before();
}

sub send_cmd_yes
{
    my ($self, $cmd) = @_;
    unless ($self->connect()) {
	return '';
    }
    $self->send("$cmd\n");
    $self->expect_it('-re', '[?].+:');

    $self->send("yes\n");
    $self->expect_it("yes");
    $self->{bconsole}->clear_accum();
    $self->expect_it('-re',qr/^[*]/);
    return $self->before();
}

sub send_cmd_with_drive
{
    my ($self, $cmd, $drive) = @_;
    $drive = $drive || '0';

    unless ($self->connect()) {
	return '';
    }
    $self->send("$cmd\n");
    $self->expect_it('-re', '\[0\]\s*:');

    $self->send("$drive\n");
    $self->expect_it('-re', '[0-9]');
    $self->{bconsole}->clear_accum();
    $self->expect_it('-re',qr/^[*]/);
    return $self->before();
}

sub label_barcodes
{
    my ($self, %arg) = @_;

    unless ($arg{storage}) {
	return '';
    }

    unless ($self->connect()) {
	return '';
    }

    $arg{drive} = $arg{drive} || '0' ;
    $arg{pool} = $arg{pool} || 'Scratch';

    my $cmd = "label barcodes pool=\"$arg{pool}\" storage=\"$arg{storage}\"";

    if ($arg{slots}) {
	$cmd .= " slots=$arg{slots}";
    }

    $self->send("$cmd\n");
    $self->expect_it('-re', '\[0\]\s*:');
    $self->send("$arg{drive}\n");
    $self->expect_it('-re', '[?].+\)\s*:');
    my $res = $self->before();
    $self->send("yes\n");
    $self->expect_it("yes");
    $res .= $self->before();
    $self->expect_it('-re',qr/^[*]/);
    $res .= $self->before();
    return $res;
}

#
# return [ { name => 'test1', vol => '00001', ... },
#          { name => 'test2', vol => '00002', ... }... ] 
#
sub director_get_sched
{
    my ($self, $days) = @_ ;

    $days = $days || 1;

    unless ($self->connect()) {
	return '';
    }
   
    my $status = $self->send_cmd("st director days=$days") ;

    my @ret;
    foreach my $l (split(/\r?\n/, $status)) {
	#Level          Type     Pri  Scheduled        Name       Volume
	#Incremental    Backup    11  03-ao-06 23:05  TEST_DATA  000001
	if ($l =~ /^(I|F|Di)\w+\s+\w+\s+\d+/i) {
	    my ($level, $type, $pri, $d, $h, @name_vol) = split(/\s+/, $l);

	    my $vol = pop @name_vol; # last element
	    my $name = join(" ", @name_vol); # can contains space

	    push @ret, {
		level => $level,
		type  => $type,
		priority => $pri,
		date  => "$d $h",
		name  => $name,
		volume => $vol,
	    };
	}

    }
    return \@ret;
}

sub update_slots
{
    my ($self, $storage, $drive) = @_;
    
    return $self->send_cmd_with_drive("update slots storage=$storage", $drive);
}

sub list_job
{
    my ($self) = @_;
    return split(/\r\n/, $self->send_cmd(".jobs"));
}

sub list_fileset
{
    my ($self) = @_;
    return split(/\r\n/, $self->send_cmd(".filesets"));
}

sub list_storage
{
    my ($self) = @_;
    return split(/\r\n/, $self->send_cmd(".storage"));
}

sub list_client
{
    my ($self) = @_;
    return split(/\r\n/, $self->send_cmd(".clients"));
}

sub list_pool
{
    my ($self) = @_;
    return split(/\r\n/, $self->send_cmd(".pools"));
}

use Time::ParseDate qw/parsedate/;
use POSIX qw/strftime/;
use Data::Dumper;

sub _get_volume
{
    my ($self, @volume) = @_;
    return '' unless (@volume);

    my $sel='';
    foreach my $vol (@volume) {
	if ($vol =~ /^([\w\d\.-]+)$/) {
	    $sel .= " volume=$1";

	} else {
	    $self->{error} = "Sorry media is bad";
	    return '';
	}
    }

    return $sel;
}

sub purge_volume
{
    my ($self, @volume) = @_;

    my $sel = $self->_get_volume(@volume);
    my $ret;
    if ($sel) {
	$ret = $self->send_cmd("purge $sel");
    } else {
	$ret = $self->{error};
    }
    return $ret;
}

sub prune_volume
{
    my ($self, @volume) = @_;

    my $sel = $self->_get_volume(@volume);
    my $ret;
    if ($sel) {
	$ret = $self->send_cmd_yes("prune $sel");
    } else {
	$ret = $self->{error};
    }
    return $ret;
}

sub purge_job
{
    my ($self, @jobid) = @_;

    return 0 unless (@jobid);

    my $sel='';
    foreach my $job (@jobid) {
	if ($job =~ /^(\d+)$/) {
	    $sel .= " jobid=$1";

	} else {
	    $self->{error} = "Sorry jobid is bad";
	    return 0;
	}
    }

    $self->send_cmd("purge $sel");
}

sub close
{
    my ($self) = @_;
    $self->send("quit\n");
    $self->{bconsole}->soft_close();
    $self->{bconsole} = undef;
}

1;

__END__

# to use this
# grep -v __END__ Bconsole.pm | perl

package main;

print "test sans conio\n";

my $c = new Bconsole(pref => {
    bconsole => '/tmp/bacula/sbin/bconsole -n -c /tmp/bacula/etc/bconsole.conf',
},
		     debug => 0);

print "fileset : ", join(',', $c->list_fileset()), "\n";
print "job : ",     join(',', $c->list_job()), "\n";
print "storage : ", join(',', $c->list_storage()), "\n";
#print "prune : " . $c->prune_volume('000001'), "\n";
#print "update : " . $c->send_cmd_with_drive('update slots storage=SDLT-1-2'), "\n";
#print "label : ", join(',', $c->label_barcodes(storage => 'SDLT-1-2',
#					       slots => 6,
#					       drive => 0)), "\n";


