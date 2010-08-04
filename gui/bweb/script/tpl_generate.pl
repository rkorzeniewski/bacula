#!/usr/bin/perl -w
use strict;

=head1 LICENSE

   Bweb - A Bacula web interface
   BaculaÂ® - The Network Backup Solution

   Copyright (C) 2000-2006 Free Software Foundation Europe e.V.

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

=head1 USAGE

    cd bweb
    rm -f lang/fr/tpl/*.tpl
    LANGUAGE=fr ./script/tpl_generate.pl tpl/*.pl

=head1 VERSION

    $Id$

=cut

my $debug=0;

my $lang = $ENV{LANGUAGE};

if (!$lang or $lang !~ /^[a-z]+$/) {
    print "Can't find \$LANGUAGE, please set it before running $0 (to en, fr, it, es...)\n";
    exit 0;
}

my $out = "lang/$lang/tpl";

die "Can't write to $out" unless (-d $out);

print "Using LANGUAGE=$lang and writing tpl to: $out\n";

use Locale::TextDomain ('bweb', './po/');
use File::Basename qw/basename/;

foreach my $f (@ARGV)
{
    my $nb=0;
    if (! -f $f) {
	print STDERR "Skipping $f, file not found\n";
	next;
    }

    my $file = basename($f);

    open(FP, $f) or print STDERR "Can't open $f for reading $!\n";
    open(OUT, ">$out/$file") or print STDERR "Can't open $out/$file for writing $!\n";
    print "Converting $f -> $out/$file ";
    while (my $l = <FP>)
    {
	my (@str) = ($l =~ m/__(.+?)__/g);
        
        while (my $s = shift @str) {
	    $nb++;
	    my $r = __$s;

	    if ($debug) {
		print STDERR "$s -> $r\n";
	    }

	    $s =~ s/[?]/\\?/g;
	    $s =~ s/\$/\\\$/g;
	    $r =~ s/\$/\\\$/g;
	    $s =~ s/([\[\]\(\)])/\\$1/g;

	    $l =~ s/__${s}__/$r/g;
	}
	print OUT $l;
    }
    print "($nb strings)\n";
    close(OUT);
    close(FP);
}
