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

  X:\> regress-win32.pl [-b basedir] [-i ip_address] [-p c:/bacula]
   or
  X:\> perl regress-win32.pl ...

    -b|--base=path      Where to find regress and bacula directories
    -i|--ip=ip          Restrict access to this tool to this ip address
    -p|--prefix=path    Path to the windows installation
    -h|--help           Print this help

=head2 EXAMPLE

    regress-win32.pl -b z:/git         # will find z:/git/regress z:/git/bacula

    regress-win32.pl -i 192.168.0.1 -b z:

=head2 INSTALL

    This perl script needs a Perl distribution on the Windows Client
    (http://strawberryperl.com)

    You need to have the following subtree on x:
    x:/
      bacula/
      regress/

   This script requires perl to work (http://strawberryperl.com), and by default 
   it assumes that Bacula is installed in the standard location. Once it's 
   started on the windows, you can do remote commands like:
    - start the service
    - stop the service
    - edit the bacula-fd.conf to change the director and password setting
    - install a new binary version (not tested, no plugin support)
    - create weird files and directories
    - create files with windows attributes
    - compare two directories (with md5)
   
   
   To test it, you can follow this procedure
   On the windows box:
    - install perl from http://strawberryperl.com on windows
    - copy or export regress directory somewhere on your windows
    - start the regress/scripts/regress-win32.pl (open it with perl.exe)
    - create c:/tmp (not sure it's mandatory)
    - make sure that the firewall is well configured or just disabled (needs 
   bacula and 8091/tcp)
   
   On Linux box:
    - edit config file to fill the following variables
   
   WIN32_CLIENT="win2008-fd"
   # Client FQDN or IP address
   WIN32_ADDR="192.168.0.6"
   # File or Directory to backup.  This is put in the "File" directive 
   #   in the FileSet
   WIN32_FILE="c:/tmp"
   # Port of Win32 client
   WIN32_PORT=9102
   # Win32 Client password
   WIN32_PASSWORD="xxx"
   # will be the ip address of the linux box
   WIN32_STORE_ADDR="192.168.0.1"
   
    - type make setup
    - run ./tests/backup-bacula-test to be sure that everything is ok
    - start ./tests/win32-fd-test
   
   I'm not very happy with this script, but it works :)

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
use Getopt::Long ;

my $base = 'x:';
my $src_ip = '';
my $help;
my $bacula_prefix="c:/Program Files/Bacula";
my $conf = "C:/Documents and Settings/All Users/Application Data/Bacula";
GetOptions("base=s"   => \$base,
           "help"     => \$help,
           "prefix=s" => \$bacula_prefix,
           "ip=s"     => \$src_ip);

if ($help) {
    pod2usage(-verbose => 2, 
              -exitval => 0);
}

if (! -d $bacula_prefix) {
    print "Could not find Bacula installation dir $bacula_prefix\n";
    print "Won't be able to upgrade the version or modify the configuration\n";
}

if (-f "$bacula_prefix/bacula-fd.conf" and -f "$conf/bacula-fd.conf") {
    print "Unable to determine bacula-fd location $bacula_prefix or $conf ?\n";

} elsif (-f "$bacula_prefix/bacula-fd.conf") {
    $conf = $bacula_prefix;
}

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

# initialize the weird directory for runscript test
sub init_weird_runscript_test
{
    my ($r) = shift;

    if ($r->url !~ m!^/init_weird_runscript_test\?source=(\w:/[\w/]+)$!) {
        return "ERR\nIncorrect url\n";
    }
    my $source = $1;

    if (!chdir($source)) {
        return "ERR\nCan't access to $source $!\n";
    }
    
    if (-d "weird_runscript") {
        system("rmdir /Q /S weird_runscript");
    }

    mkdir("weird_runscript");
    if (!chdir("weird_runscript")) {
        return "ERR\nCan't access to $source $!\n";
    }
   
    open(FP, ">test.bat")                 or return "ERR\n";
    print FP "\@echo off\n";
    print FP "echo hello \%1\n";
    close(FP);
    
    copy("test.bat", "test space.bat")    or return "ERR\n";
    copy("test.bat", "test2 space.bat")   or return "ERR\n";
    copy("test.bat", "testé.bat")         or return "ERR\n";

    mkdir("dir space")                    or return "ERR\n";
    copy("test.bat", "dir space")         or return "ERR\n";
    copy("testé.bat","dir space")         or return "ERR\n"; 
    copy("test2 space.bat", "dir space")  or return "ERR\n";

    mkdir("Évoilà")                       or return "ERR\n";
    copy("test.bat", "Évoilà")            or return "ERR\n";
    copy("testé.bat","Évoilà")            or return "ERR\n"; 
    copy("test2 space.bat", "Évoilà")     or return "ERR\n";

    mkdir("Éwith space")                  or return "ERR\n";
    copy("test.bat", "Éwith space")       or return "ERR\n";
    copy("testé.bat","Éwith space")       or return "ERR\n"; 
    copy("test2 space.bat", "Éwith space") or return "ERR\n";
    return "OK\n";
}

# init the Attrib test by creating some files and settings attributes
sub init_attrib_test
{
    my ($r) = shift;

    if ($r->url !~ m!^/init_attrib_test\?source=(\w:/[\w/]+)$!) {
        return "ERR\nIncorrect url\n";
    }
  
    my $source = $1;
 
    if (!chdir($source)) {
        return "ERR\nCan't access to $source $!\n";
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

# set $src and $dst before using Find call
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

sub set_director_name
{
    my ($r) = shift;

    if ($r->url !~ m!^/set_director_name\?name=([\w\d\.\-]+);pass=([\w\d+]+)$!)
    {
        return "ERR\nIncorrect url\n";
    }

    my ($name, $pass) = ($1, $2);

    open(ORG, "$conf/bacula-fd.conf") or return "ERR\nORG $!\n";
    open(NEW, ">$conf/bacula-fd.conf.new") or return "ERR\nNEW $!\n";
    
    my $in_dir=0;               # don't use monitoring section
    my $nb_dir="";
    while (my $l = <ORG>)
    {
        if ($l =~ /^\s*Director\s+{/i) {
            print NEW $l; 
            $in_dir = 1;
        } elsif ($l =~ /^(\s*)Name\s*=/ and $in_dir) {
            print NEW "${1}Name=$name$nb_dir\n";
        } elsif ($l =~ /^(\s*)Password\s*=/ and $in_dir) {
            print NEW "${1}Password=$pass\n";
        } elsif ($l =~ /\s*}/ and $in_dir) {
            print NEW $l; 
            $in_dir = 0;
            $nb_dir++;
        } else {
            print NEW $l;
        }
    }

    close(ORG);
    close(NEW);
    move("$conf/bacula-fd.conf.new", "$conf/bacula-fd.conf")
        and return "OK\n";

    return "ERR\n";
} 

# convert \ to / and strip the path
sub strip_base
{
    my ($data, $path) = @_;
    $data =~ s!\\!/!sg;
    $data =~ s!\Q$path!!sig;
    return $data;
}

# Compare two directories, make checksums, compare attribs and ACLs
sub compare
{
    my ($r) = shift;

    if ($r->url !~ m!^/compare\?source=(\w:/[\w/]+);dest=(\w:/[\w/]+)$!) {
        return "ERR\nIncorrect url\n";
    }

    my ($source, $dest) = ($1, $2);
    
    if (!Cwd::chdir($source)) {
        return "ERR\nCan't access to $source $!\n";
    }
    
    my $src_attrib = `attrib /D /S`;
    $src_attrib = strip_base($src_attrib, $source);

    if (!Cwd::chdir($dest)) {
        return "ERR\nCan't access to $dest $!\n";
    }
    
    my $dest_attrib = `attrib /D /S`;
    $dest_attrib = strip_base($dest_attrib, $dest);

    if (lc($src_attrib) ne lc($dest_attrib)) {
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

sub cleandir
{
    my ($r) = shift;

    if ($r->url !~ m!^/cleandir\?source=(\w:/[\w/]+)/restore$!) {
        return "ERR\nIncorrect url\n";
    }

    my $source = $1;
 
    if (! -d "$source/restore") {
        return "ERR\nIncorrect path\n";
    }

    if (!chdir($source)) {
        return "ERR\nCan't access to $source $!\n";
    }

    system("rmdir /Q /S restore");

    return "OK\n";
}

my $Registry;
use Win32::TieRegistry qw/KEY_READ KEY_WRITE/;

sub add_registry_key
{
    my ($r) = shift;
    my $ret="ERR";
    if ($r->url !~ m!^/add_registry_key\?key=(\w+);val=(\w+)$!) {
        return "ERR\nIncorrect url\n";
    }
    my ($k, $v) = ($1,$2);
    
    my $key= new Win32::TieRegistry ("LMachine/SOFTWARE/",
                                     { Access=>KEY_READ()|KEY_WRITE(),
                                       Delimiter=>"/" })
        or return "ERR Can't open Registry\n";
    print join(",", keys( %{$key} )), "\n" ;
    my $newKey = $key->{"Bacula"};
    if ($newKey) {
        $newKey->{$k} = $v;
        $ret = "OK\n";
    } else {
        $ret = "ERR can't find Bacula key";
    }

    undef $key;
    undef $newKey;
    return "$ret\n";
}

sub del_registry_key
{
    my ($r) = shift;
    my $ret="ERR";
    if ($r->url !~ m!^/del_registry_key\?key=(\w+)$!) {
        return "ERR\nIncorrect url\n";
    }
    my $k = $1;
    
    my $key= new Win32::TieRegistry ("LMachine/Software/",
                                     { Access=>KEY_READ()|KEY_WRITE(),
                                       Delimiter=>"/" })
        or return "ERR Can't open Registry\n";

    my $newKey = $key->{"Bacula"};
    if ($newKey) {
        delete $newKey->{$k};
        $ret = "OK\n";
    } else {
        $ret = "ERR can't find Bacula key";
    }
    undef $key;
    undef $newKey;
    return "$ret\n";
}

sub get_registry_key
{
    my ($r) = shift;
    my $ret = "ERR";
    if ($r->url !~ m!^/get_registry_key\?key=(\w+);val=(\w+)$!) {
        return "ERR\nIncorrect url\n";
    }
    my ($k, $v) = ($1, $2);
    
    my $key= new Win32::TieRegistry ("LMachine/Software/",
                                     { Access=>KEY_READ()|KEY_WRITE(),
                                       Delimiter=>"/" })
        or return "ERR Can't open Registry\n";

    my $newKey = $key->{"Bacula"};
    if ($newKey) {
        if ($newKey->{$k} eq $v) {
            $ret = "OK\n";
        } else {
            $ret = "ERR key=" . $newKey->{$k}; 
        }
    } else {
        $ret = "ERR can't find Bacula key";
    }
    undef $key;
    undef $newKey;
    return "$ret\n";
}

# When adding an action, fill this hash with the right function
my %action_list = (
    stop    => \&stop_fd,
    start   => \&start_fd,
    install => \&install_fd,
    compare => \&compare,
    init_attrib_test => \&init_attrib_test,
    init_weird_runscript_test => \&init_weird_runscript_test,
    set_director_name => \&set_director_name,
    cleandir => \&cleandir,
    add_registry_key => \&add_registry_key,
    del_registry_key => \&del_registry_key,
    get_registry_key => \&get_registry_key,
    quit => sub {  exit 0; },
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
    || die "Error: Can't bind $!" ;

my $olddir = Cwd::cwd();
while (1) {
    print "Starting daemon...\n";
    my $c = $d->accept ;
    my $ip = $c->peerhost;
    if (!$ip) {
        $c->send_error(RC_FORBIDDEN) ;
    } elsif ($src_ip && $ip ne $src_ip) {
        $c->send_error(RC_FORBIDDEN) ;
    } elsif ($c) {
        handle_client($c, $ip) ;
    } else {
        $c->send_error(RC_FORBIDDEN) ;
    }
    close($c) ;
    chdir($olddir);
}
