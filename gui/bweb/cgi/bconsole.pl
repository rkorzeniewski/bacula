#!/usr/bin/perl -w
use strict;

=head1 NAME

    bconsole.pl - Interface between brestore and bweb to use bconsole.

    You can also use Bconsole.pm and install bconsole on your workstation.

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
use CGI;
use POSIX qw/strftime/;
use File::Temp qw/tempfile/;

use Bweb;
use Bconsole;

my $conf = new Bweb::Config(config_file => $Bweb::config_file);
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

	my $arg = $bweb->get_form(qw/job client storage fileset regexwhere
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
				   regexwhere => $arg->{regexwhere},
				   replace   => $arg->{replace},
				   priority  => $arg->{priority},
				   bootstrap => $filename);

	print "run=$jobid\n";

    } else {
	print STDERR "unknow argument $a\n";
    }
}

exit 0;
