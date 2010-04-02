#!/usr/bin/perl -w

=head1 NAME

    diff.pl -- Helper to diff files (rights, acl and content)

=head2 USAGE

    diff.pl -s source -d dest [--acl | --attr]

=cut

use strict;
use Cwd 'chdir';
use File::Find;
no warnings 'File::Find';
use Digest::MD5;
use Getopt::Long ;
use Pod::Usage;
use Data::Dumper;
use Cwd;

my ($src, $dst, $help, $acl, $attr);
my %src_attr; 
my %dst_attr;
my $hash;
my $ret=0;

GetOptions("src=s"   => \$src,
           "dst=s"   => \$dst,
           "acl"     => \$acl,
           "attr"    => \$attr,
           "help"    => \$help,
    ) or pod2usage(-verbose => 1, 
                   -exitval => 1);
if (!$src or !$dst) {
   pod2usage(-verbose => 1, 
             -exitval => 1); 
}

if ($help) {
    pod2usage(-verbose => 2, 
              -exitval => 0);
}
my $md5 = Digest::MD5->new;

my $dir = getcwd;

chdir($src);
$hash = \%src_attr;
find(\&wanted_src, '.');

chdir ($dir);

chdir($dst);
$hash = \%dst_attr;
find(\&wanted_src, '.');

#print Data::Dumper::Dumper(\%src_attr);
#print Data::Dumper::Dumper(\%dst_attr);

foreach my $f (keys %src_attr)
{
    if (!defined $dst_attr{$f}) {
        $ret++;
        print "E: Can't find $f in dst\n";

    } else {
        compare($src_attr{$f}, $dst_attr{$f});
    }
    delete $src_attr{$f};
    delete $dst_attr{$f};
}

foreach my $f (keys %dst_attr)
{
    $ret++;
    print "E: Can't find $f in src\n";
}

exit $ret;

sub compare
{
    my ($h1, $h2) = @_;
    my ($f1, $f2) = ($h1->{file}, $h2->{file});
    foreach my $k (keys %$h1) {
        if (!exists $h2->{$k}) {
            $ret++;
            print "E: Can't find $k for dest $f2 ($k=$h1->{$k})\n";
        }
        if ($h2->{$k} ne $h1->{$k}) {
            $ret++;
            print "E: src and dst $f2 differ on $k ($h1->{$k} != $h2->{$k})\n";
        }
        delete $h1->{$k};
        delete $h2->{$k};
    }

    foreach my $k (keys %$h2) {
        $ret++;
        print "E: Found $k on dst file and not on src ($k=$h2->{$k})\n";
    }
}

sub wanted_src
{
    my $f = $_;
    my ($dev,$ino,$mode,$nlink,$uid,$gid,$rdev,$size,
        $atime,$mtime,$ctime,$blksize,$blocks) = stat($f);
 
    if (-l $f) {
        my $target = readlink($f);
        $hash->{$File::Find::name} = {
            nlink => $nlink,
            uid => $uid,
            gid => $gid,
            atime => $atime,
            mtime => $mtime,
            ctime => $ctime,
            target => $target,
            type => 'l',
            file => $File::Find::name,
        };

    } elsif (-f $f)  {
        $hash->{$File::Find::name} = {
            mode => $mode,
            nlink => $nlink,
            uid => $uid,
            gid => $gid,
            size => $size,
            atime => $atime,
            mtime => $mtime,
            ctime => $ctime,
            type => 'f',
            file => $File::Find::name,
        };
        $md5->reset;
        open(FILE, $f) or die "Can't open '$f': $!";
        binmode(FILE);
        $hash->{$File::Find::name}->{md5} = $md5->addfile(*FILE)->hexdigest;
        close(FILE);

        if ($acl) {
            $hash->{$File::Find::name}->{acl} = `getfacl $f 2>/dev/null`;
        }
        if ($attr) {
            $hash->{$File::Find::name}->{attr} = `getfattr $f 2>/dev/null`;
        }

    } elsif (-d $f) {
        $hash->{$File::Find::name} = {
            mode => $mode,
            uid => $uid,
            gid => $gid,
            atime => $atime,
            mtime => $mtime,
            ctime => $ctime,
            type => 'd',
            file =>  $File::Find::name,
        };
        if ($acl) {
            $hash->{$File::Find::name}->{acl} = `getfacl $f 2>/dev/null`;
        }
        if ($attr) {
            $hash->{$File::Find::name}->{attr} = `getfattr $f 2>/dev/null`;
        }

    } else {

    }
}
