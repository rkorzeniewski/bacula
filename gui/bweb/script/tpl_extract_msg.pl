#!/usr/bin/perl -w
use strict;

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

=head1 INFORMATIONS

    This script is used to extract strings from tpl files

=head1 VERSION

    $Id$

=cut

package TmplParser;
use base "HTML::Parser";
use HTML::Parser;
use Locale::PO;
my $in_script;

my %dict;
sub print_it
{
    foreach my $w (@_) {
	next if ($w eq '&nbsp;');
	next if ($w !~ /\w/);
	next if ($w =~ />/);

	next if (exists $dict{$w});
	$dict{$w}=1;

	# we add " at the begining and the end of the string
	$w =~ s/(^|$)/"/gm; # "
	print "msgid ", $w, "\n";
        print 'msgstr ""', "\n\n";
    }
}

sub text {
    my ($self, $text) = @_;
    
    # between <script>..</script> we take only "words"
    if ($in_script) {
	my @words = ($text =~ /"([\w\s]+)"/gs);
	print_it(@words);
	return;
    }

    # just print out the original text
#    $text =~ s/<\/?TMPL[^>]+>//gs;

    # strip extra spaces
    $text =~ s/(^\s+)|(\s+$)//gs;

    # skip some special cases
    return if ($text eq 'selected');
    print_it($text);
}

sub start {
    my ($self, $tag, $attr, $attrseq, $origtext) = @_;

    # On note qu'on se trouve dans un script
    # pour prendre que les chaines entre ""
    if ($tag eq 'script') {
	$in_script = 1;
    }

    #return unless ($tag =~ /^(a|input|img)$/);

    # liste des attributs a traduire
    if (defined $attr->{'title'}) {
	print_it($attr->{'title'});
    }

    if (defined $attr->{'alt'}) {
	print_it($attr->{'alt'});
    }
}

sub end {
    if ($in_script) {
	$in_script=0;
    }
}

package main;

my $p = new TmplParser;
for (my $f = shift ; $f and -f $f ; $f = shift) {
    #$TmplParser::nb=0;
    print "#============ $f ==========\n";
    $p->parse_file($f);
}

