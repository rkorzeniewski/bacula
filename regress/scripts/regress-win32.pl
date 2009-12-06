#!/usr/bin/perl -w

=head1 NAME

    regress-win32.pl -- Helper for Windows regression tests

=head2 DESCRIPTION

    This perl script permits to run test Bacula Client Daemon on Windows.
    It allows to:
       - stop/start/upgrade the Bacula Client Daemon
       - compare to subtree with checksums, attribs and ACL
       - create test environments

=head2 USAGE

    X:\> regress-win32.pl [basedir]
     or
    X:\> perl regress-win32.pl [basedir]

=head2 EXAMPLE

    regress-win32.pl z:/git         # will find z:/git/regress z:/git/bacula

=head2 INSTALL

    This perl script needs a Perl distribution on the Windows Client
    (http://strawberryperl.com)

    You need to have the following subtree on x:
    x:/
      bacula/
      regress/

=cut

use strict;
use HTTP::Daemon;
use HTTP::Status;
use HTTP::Response;
use HTTP::Headers;
use File::Copy;
use Pod::Usage;
use Cwd 'chdir';
use File::Find;
use Digest::MD5;

my $base = shift || 'x:';
#if (! -d "$base/bacula" || ! -d "$base/regress") {
#    pod2usage(-verbose => 2, 
#              -exitval => 1,
#              -message => "Can't find bacula or regress dir on $base\n");
#} 

# stop the fd service
sub stop_fd
{
    return `net stop bacula-fd`;
}

# copy binaries for a new fd
sub install_fd
{
    copy("$base/bacula/src/win32/release32/bacula-fd.exe", 
         "c:/Program Files/bacula/bacula-fd.exe"); 

    copy("$base/bacula/src/win32/release32/bacula.dll", 
         "c:/Program Files/bacula/bacula.dll"); 
}

# start the fd service
sub start_fd
{
    return `net start bacula-fd`;
}

# convert \ to / and strip the path
sub strip_base
{
    my ($data, $path) = @_;
    $data =~ s!\\!/!sg;
    $data =~ s!\Q$path!!sig;
    return $data;
}

# initialize the weird directory for runscript test
sub init_weird_runscript_test
{
    my ($r) = shift;

    if ($r->url !~ m!^/init_weird_runscript_test\?source=([\w+:/]+)$!) {
        return "Incorrect url\n";
    }
    my $source = $1;

    if (!chdir($source)) {
        return "Can't access to $source $!\n";
    }
    
    if (-d "weird_runcript") {
        system("rmdir /Q /S weird_runcript");
    }

    mkdir("weird_runcript");
    if (!chdir("weird_runcript")) {
        return "Can't access to $source $!\n";
    }
   
    open(FP, ">test.bat")                 or return "ERROR\n";
    print FP "\@echo off\n";
    print FP "echo hello \%1\n";
    close(FP);
    
    copy("test.bat", "test space.bat")    or return "ERROR\n";
    copy("test.bat", "test2 space.bat")   or return "ERROR\n";
    copy("test.bat", "testé.bat")         or return "ERROR\n";

    mkdir("dir space")                    or return "ERROR\n";
    copy("test.bat", "dir space")         or return "ERROR\n";
    copy("testé.bat","dir space")         or return "ERROR\n"; 
    copy("test2 space.bat", "dir space")  or return "ERROR\n";

    mkdir("Évoilà")                       or return "ERROR\n";
    copy("test.bat", "Évoilà")            or return "ERROR\n";
    copy("testé.bat","Évoilà")            or return "ERROR\n"; 
    copy("test2 space.bat", "Évoilà")     or return "ERROR\n";

    mkdir("Éwith space")                  or return "ERROR\n";
    copy("test.bat", "Éwith space")       or return "ERROR\n";
    copy("testé.bat","Éwith space")       or return "ERROR\n"; 
    copy("test2 space.bat", "Éwith space") or return "ERROR\n";
    return "OK\n";
}

# init the Attrib test by creating some files and settings attributes
sub init_attrib_test
{
    my ($r) = shift;

    if ($r->url !~ m!^/init_attrib_test\?source=([\w+:/]+)$!) {
        return "Incorrect url\n";
    }
  
    my $source = $1;
 
    if (!chdir($source)) {
        return "Can't access to $source $!\n";
    }

    # cleanup the old directory if any
    if (-d "attrib_test") {
        system("rmdir /Q /S attrib_test");
    }

    mkdir("attrib_test");
    chdir("attrib_test");
    
    mkdir("hidden");
    mkdir("hidden/something");
    system("attrib +H hidden");

    mkdir("readonly");
    mkdir("readonly/something");
    system("attrib +R readonly");

    mkdir("normal");
    mkdir("normal/something");
    system("attrib -R -H -S normal");

    mkdir("system");
    mkdir("system/something");
    system("attrib +S system");

    mkdir("readonly_hidden");
    mkdir("readonly_hidden/something");
    system("attrib +R +H readonly_hidden");

    my $ret = `attrib /S /D`;
    $ret = strip_base($ret, $source);

    return "OK\n$ret\n";
}

sub md5sum
{
    my $file = shift;
    open(FILE, $file) or return "Can't open $file $!";
    binmode(FILE);
    return Digest::MD5->new->addfile(*FILE)->hexdigest;
}

my ($src, $dst);
my $error="";
sub wanted
{
    my $f = $File::Find::name;
    $f =~ s!^\Q$src\E/?!!i;
    
    if (-f "$src/$f") {
        if (! -f "$dst/$f") {
            $error .= "$dst/$f is missing\n";
        } else {
            my $a = md5sum("$src/$f");
            my $b = md5sum("$dst/$f");
            if ($a ne $b) {
                $error .= "$src/$f $a\n$dst/$f $b\n";
            }
        }
    }
}

# Compare two directories, make checksums, compare attribs and ACLs
sub compare
{
    my ($r) = shift;

    if ($r->url !~ m!^/compare\?source=([\w+:/]+);dest=([\w+:/]+)$!) {
        return "Incorrect url\n";
    }

    my ($source, $dest) = ($1, $2);
    
    if (!Cwd::chdir($source)) {
        return "Can't access to $source $!\n";
    }
    
    my $src_attrib = `attrib /D /S`;
    $src_attrib = strip_base($src_attrib, $source);

    if (!Cwd::chdir($dest)) {
        return "Can't access to $dest $!\n";
    }
    
    my $dest_attrib = `attrib /D /S`;
    $dest_attrib = strip_base($dest_attrib, $dest);

    if ($src_attrib ne $dest_attrib) {
        return "ERR\n$src_attrib\n=========\n$dest_attrib\n";
    } 

    ($src, $dst, $error) = ($source, $dest, '');
    find(\&wanted, $source);
    if ($error) {
        return "ERR\n$error";
    } else {
        return "OK\n";
    }
}

# When adding an action, fill this hash with the right function
my %action_list = (
    stop    => \&stop_fd,
    start   => \&start_fd,
    install => \&install_fd,
    compare => \&compare,
    init_attrib_test => \&init_attrib_test,
    init_weird_runscript_test => \&init_weird_runscript_test,
    );

# handle client request
sub handle_client
{
    my ($c, $ip) = @_ ;
    my $action;
    my $r = $c->get_request ;

    if (!$r) {
        $c->send_error(RC_FORBIDDEN) ;
        return;
    }
    if ($r->url->path !~ m!^/(\w+)!) {
        $c->send_error(RC_NOT_FOUND) ;
        return;
    }
    $action = $1;

    if (($r->method eq 'GET') 
        and $action_list{$action})       
    {
        my $ret = $action_list{$action}($r);
        my $h = HTTP::Headers->new('Content-Type' => 'text/plain') ;
        my $r = HTTP::Response->new(HTTP::Status::RC_OK,
                                    'OK', $h, $ret) ;

        $c->send_response($r) ;
    } else {
        $c->send_error(RC_NOT_FOUND) ;
    }

    $c->close;
}

my $d = HTTP::Daemon->new ( LocalPort =>  8091,
                            ReuseAddr => 1) 
    || die "E : Can't bind $!" ;

my $olddir = Cwd::cwd();
while (1) {
    chdir($olddir);
    my ($c, $ip) = $d->accept ;
    if ($c and $ip) {
        handle_client($c, $ip) ;
    }
    close($c) ;
}
