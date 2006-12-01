#!/usr/bin/perl -w
use strict;

=head1 NAME

    bconsole.pl - Interface between brestore and bweb to use bconsole.

    You can also use Bconsole.pm and install bconsole on your workstation.

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
use CGI;
use POSIX qw/strftime/;
use File::Temp qw/tempfile/;

use Bweb;
use Bconsole;

my $conf = new Bweb::Config(config_file => '/etc/bweb/config');
$conf->load();

my $bweb = new Bweb(info => $conf);
my $bconsole = new Bconsole(pref => $conf);
my $debug = $bweb->{debug};

my $opts = $bweb->get_form('timeout');
$bconsole->{timeout} = $opts->{timeout} || 40;

# on doit utiliser du POST
print CGI::header('text/plain');

my @action = CGI::param('action') ;
my %cmd = ( 'list_job'      => '.job', 
	    'list_client'   => '.client' ,
	    'list_storage'  => '.storage',
	    'list_fileset'  => '.fileset',
	   );


my $have_run=0;
for my $a (@action)
{
    if (defined $cmd{$a})
    {
	my $out = $bconsole->send_cmd($cmd{$a});
	$out =~ s/\r\n/;/g ;
	print "$a=$out\n";

    } elsif ($a eq 'run' and $have_run==0) {
	$have_run=1;

	my $arg = $bweb->get_form(qw/job client storage fileset 
				     where replace priority/);

	my $bootstrap = CGI::param('bootstrap');
	
	if (!$bootstrap or !$arg->{job} or !$arg->{client}) {
	    print "ERROR: missing bootstrap or job or client\n";
	    exit 1;
	}

	my ($fh, $filename) = tempfile();
	while (my $l = <$bootstrap>) {
	    $fh->print($l);
	}
	close($fh);
	chmod(0644, $filename);
	
	my $jobid = $bconsole->run(job       => $arg->{job},
				   client    => $arg->{client},
				   storage   => $arg->{storage},
				   fileset   => $arg->{fileset},
				   where     => $arg->{where},
				   replace   => $arg->{replace},
				   priority  => $arg->{priority},
				   bootstrap => $filename);

	print "run=$jobid\n";

    } else {
	print STDERR "unknow argument $a\n";
    }
}

exit 0;
