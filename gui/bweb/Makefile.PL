use Test::More tests => 7;

require_ok('DBI');
require_ok('GD::Graph');
require_ok('GD');
require_ok('HTML::Template');
require_ok('CGI');
require_ok('Expect');
require_ok('Time::ParseDate');


use ExtUtils::MakeMaker;

WriteMakefile(
 ABSTRACT => "An other Bacula WEB interface",
 AUTHOR   => "Eric Bollengier <eric@eb.homelinux.org>",
 NAME     => 'Bweb',
 VERSION  => "lib/Bweb.pm",

 #INST_BIN => "cgi",
 # setup your cgi installation
 # debian      : /usr/lib/cgi-bin/bweb
 # redhat/suse : /var/www/cgi-bin/bweb
 # 
 #INSTALLSITEBIN => "/usr/lib/cgi-bin/bweb",
);
