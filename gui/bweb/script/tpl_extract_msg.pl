#!/usr/bin/perl -w
use strict;

=head1 LICENSE

   Bweb - A Bacula web interface
   BaculaÅ¬ÅÆ - The Network Backup Solution

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.

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

   BaculaÅÆ is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 ZÅ¸rich,
   Switzerland, email:ftf@fsfeurope.org.

=head1 INFORMATIONS

    This script is used to extract strings from tpl files

=cut

package TmplParser;
use base "HTML::Parser";
use HTML::Parser;
use Locale::PO;
my $in_script;

my %dict;
my $l=0;
my $file;

sub init_fp
{
    open(FP, "|sort -n") ;
}

sub print_it
{
    foreach my $w (@_) {
#	print "<$w\n";
	next if ($w eq '&nbsp;');
	next if ($w !~ /\w/);
	next if ($w =~ />/);
	next if ($w eq 'checked');
#	print "$w>\n";
	next if (exists $dict{$w});
	$dict{$w}=1;

	# we add " at the begining and the end of the string
	#$w =~ s/'/\\'/gm; # "
	#print "s\@(?!value='|_)$w\@\$1_${w}_\@g;\n";
	#print "s!(${w})([^a-zA-Z0-9]|\$)!__\$1__\$2!g;\n";
	print "$w\n";
    }
}

sub text {
    my ($self, $text) = @_;
    
    # between <script>..</script> we take only "words"
    if ($in_script) {
	my @words = ($text =~ /"([\w\sÅÈÅËÅ‡ÅÁÅ˚ÅÙÅˆÅ¸ÅÔÅÓÅ˘!?]+)"/gs);
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

sub finish_fp
{
    close(FP);
}
1;

package main;

my $p = new TmplParser;
$p->init_fp();
for (my $f = shift ; $f and -f $f ; $f = shift) {
    #$TmplParser::nb=0;
#    print "#============ $f ==========\n";
    $file = $f;
    $file =~ s!.*?/([\w\d]+).tpl$!$1!;
    $p->parse_file($f); $l=0;
}
$p->finish_fp();
#print "s/value='__([^']+)__'/value='\$1'/g;\n";
