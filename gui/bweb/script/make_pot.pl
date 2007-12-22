#!/usr/bin/perl -w
use strict;
use Getopt::Long;

# Usage: $0 *.po *.tpl
#my $src = shift or die "Usage $0 pofile";
#
#my $tab;
#{
#    no strict;
#    $tab = eval `cat $src`;
#}

open(OUT, "|msguniq > bweb.pot");

foreach my $f (@ARGV)
{
    next if (! -f $f) ;

    open(FP, $f) or print STDERR "Can't open $f for reading\n";
    while (my $l = <FP>)
    {
	if ($l =~ m/__(.+?)__/) {
	    my $s = $1;
	    my $r = ''; #$tab->{$s} || $s;
	    $s =~ s/\n/\\n/g;
	    $s =~ s/"/\\"/g;
#	    $r =~ s/\n/\\n/g;
#	    $r =~ s/"/\\"/g;
	    print OUT "#: $f:$.\n";
	    print OUT "msgid \"$s\"\n";
	    print OUT "msgstr \"$r\"\n\n";
	}
    }
    close(FP);
}

close(OUT);
# msginit  pot -> lang.po
# msgmerge fr.po bweb.pot
