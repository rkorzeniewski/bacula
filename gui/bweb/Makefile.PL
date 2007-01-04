use ExtUtils::MakeMaker;

WriteMakefile(
 ABSTRACT => "An other Bacula WEB interface",
 AUTHOR   => "Eric Bollengier <eric@eb.homelinux.org>",
 NAME     => 'Bweb',
 INST_BIN => "cgi",
 VERSION_FROM => "lib/Bweb.pm",

 # setup your cgi installation
 # debian      : /usr/lib/cgi-bin/bweb
 # redhat/suse : /var/www/cgi-bin/bweb
 # 
 INSTALLSITEBIN => "/usr/lib/cgi-bin/bweb",
);
