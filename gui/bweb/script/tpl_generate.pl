#!/usr/bin/perl -w
use strict;

my $f = shift or die "Usage $0 pofile";

my $tab;
{ 
    no strict;
    $tab = eval `cat $f`;
}

die "E: Can't read $f" unless ($tab);
my $r;
foreach my $e (keys %$tab)
{
    $e =~ s/[?]/\\?/g;
    $e =~ s/@/\\@/g;
    $r = $tab->{$e} || $e;
    $e =~ s/([()])/\\$1/g;
    print "s!__${e}__!${r}!g;\n";
}

print "print;";


__END__

copy tpl files
generate perl script
tpl_generate.pl fr.pl > a
perl -i.bak -n a *.tpl
