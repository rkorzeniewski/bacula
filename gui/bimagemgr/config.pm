##
# bimagemgr Program Configuration
#
# Copyright (C) 2006 Kern Sibbald
#
# Sun May 07 2006 D. Scott Barninger <barninger at fairfieldcomputers.com>
# ASSIGNMENT OF COPYRIGHT
# FOR VALUE RECEIVED, D. Scott Barninger hereby sells, transfers and 
# assigns unto Kern Sibbald, his successors, assigns and personal representatives, 
# all right, title and interest in and to the copyright in this software.
# D. Scott Barninger warrants good title to said copyright, that it is 
# free of all liens, encumbrances or any known claims against said copyright.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public
# License version 2 as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public
# License along with this program; if not, write to the Free
# Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
# MA 02111-1307, USA.
##

package config;

use strict;

sub new {

	my $self = {};

	## web server configuration
	#
	# web server path to program from server root
	$self->{prog_name} = "/cgi-bin/bimagemgr.pl";
	#
	# web server host
	$self->{http_host}="localhost";
	#
	# path to graphics from document root
	$self->{logo_graphic} = "/bimagemgr.gif";
	$self->{spacer_graphic} = "/clearpixel.gif";
	$self->{burn_graphic} = "/cdrom_spins.gif";
	##

	## database configuration
	#
	# database name
	$self->{database} = "bacula";
	#
	# database host
	$self->{host} = "localhost";
	#
	# database user
	$self->{user} = "bacula";
	#
	# database password
	$self->{password} = "";
	#
	# database driver selection - uncomment one set
	# MySQL
	$self->{db_driver} = "mysql";
	$self->{db_name_param} = "database";
	$self->{catalog_dump} = "mysqldump --host=$self->{'host'} --user=$self->{'user'} --password=$self->{'password'} $self->{'database'}";
	# Postgresql
	# $self->{db_driver} = "Pg";
	# $self->{db_name_param} = "dbname";
	# $self->{catalog_dump} = "pg_dump --host=$self->{'host'} --username=$self->{'user'} --password=$self->{'password'} $self->{'database'}";
	# SQLite
	$self->{sqlitebindir} = "/usr/lib/bacula/sqlite";
	$self->{bacula_working_dir} = "/var/bacula";
	# $self->{db_driver} = "SQLite";
	# $self->{db_name_param} = "dbname";
	# $self->{catalog_dump} = "echo \".dump\" | $self->{'sqlitebindir'}/sqlite $self->{'bacula_working_dir'}/$self->{'database'}.db";
	##

	# path to backup files
	$self->{image_path} = "/backup";

	## path to cdrecord and burner settings
	$self->{cdrecord} = "/usr/bin/cdrecord";
	$self->{mkisofs} = "/usr/bin/mkisofs";
	$self->{cdburner} = "1,0,0";
	$self->{burner_speed} = "40";
	# burnfree option - uncomment one
	#$self->{burnfree} = "driveropts=noburnfree"; # no buffer underrun protection
	$self->{burnfree} = "driveropts=burnfree"; # with buffer underrun
	##

	# temporary files
	$self->{tempfile} = "temp.html";
	$self->{tempfile_path} = "/var/www/html/temp.html";
	$self->{working_dir} = "/var/tmp";

	bless($self);
	return($self);

}

1;
