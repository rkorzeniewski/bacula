# Copyright 2004 D. Scott Barninger
# Copyright 1999-2004 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
#
# Modified from bacula-1.34.5.ebuild for 1.36.0 release
# 24 Oct 2004 D. Scott Barninger <barninger at fairfieldcomputers dot com>
#
# added cdrom rescue for 1.36.1
# init script now comes from source package not ${FILES} dir
# 26 Nov 2004 D. Scott Barninger <barninger at fairfieldcomputers dot com>
#
# fix symlink creation in rescue package in post script
# remove mask on x86 keyword
# fix post script so it doesn't talk about server config for client-only build
# bug #181 - unable to reproduce on 2.4 kernel system so add FEATURES="-sandbox"
# 04 Dec 2004 D. Scott Barninger <barninger at fairfieldcomputers dot com>

DESCRIPTION="featureful client/server network backup suite"
HOMEPAGE="http://www.bacula.org/"
SRC_URI="mirror://sourceforge/bacula/${P}.tar.gz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="x86 ~ppc ~sparc"
IUSE="readline tcpd gnome mysql sqlite X static postgres wxwindows"

# 2.6 kernel user reports sandbox error with client build
# can't replicate this on 2.4 soo...
KERNEL=`uname -r | head -c 3`
if [ $KERNEL = "2.6" ]; then
	if [ -n $BUILD_CLIENT_ONLY ]; then
		FEATURES="-sandbox"
	fi
fi

inherit eutils

#theres a local sqlite use flag. use it -OR- mysql, not both.
#mysql is the recommended choice ...
#may need sys-libs/libtermcap-compat but try without first
DEPEND=">=sys-libs/zlib-1.1.4
	sys-apps/mtx
	readline? ( >=sys-libs/readline-4.1 )
	tcpd? ( >=sys-apps/tcp-wrappers-7.6 )
	gnome? ( gnome-base/libgnome )
	gnome? ( app-admin/gnomesu )
	sqlite? ( =dev-db/sqlite-2* )
	mysql? ( >=dev-db/mysql-3.23 )
	postgres? ( >=dev-db/postgresql-7.4.0 )
	X? ( virtual/x11 )
	wxwindows? ( >=x11-libs/wxGTK-2.4.2 )
	virtual/mta
	dev-libs/gmp"
RDEPEND="${DEPEND}
	sys-apps/mtx
	app-arch/mt-st"

src_compile() {

	local myconf=""

	#define this to skip building the other daemons ...
	[ -n "$BUILD_CLIENT_ONLY" ] \
		&& myconf="${myconf} --enable-client-only"

	myconf="
		`use_enable readline`
		`use_enable gnome`
		`use_enable tcpd tcp-wrappers`
		`use_enable X x`"

	[ -n "$BUILD_CLIENT_ONLY" ] \
		 && myconf="${myconf} --enable-client-only"

	# mysql is the reccomended choice ...
	if use mysql
	then
		myconf="${myconf} --with-mysql=/usr"
	elif use postgres
	then
		myconf="${myconf} --with-postgresql=/usr"
	elif use sqlite
	then
		myconf="${myconf} --with-sqlite=/usr"
	elif  use sqlite && use mysql
	then
		myconf="${myconf/--with-sqlite/}"
	fi

	if use wxwindows
	then
	   myconf="${myconf} --enable-wx-console"
	fi

	if use readline
	then
	   myconf="${myconf} --enable-readline"
	fi

	if use gnome
	then
	myconf="${myconf} --enable-tray-monitor"
	fi


	./configure \
		--enable-smartalloc \
		--prefix=/usr \
		--mandir=/usr/share/man \
		--with-pid-dir=/var/run \
		--sysconfdir=/etc/bacula \
		--infodir=/usr/share/info \
		--with-subsys-dir=/var/lock/subsys \
		--with-working-dir=/var/bacula \
		--with-scriptdir=/etc/bacula \
		--with-dir-user=root \
		--with-dir-group=bacula \
		--with-sd-user=root \
		--with-sd-group=bacula \
		--with-fd-user=root \
		--with-fd-group=bacula \
		--host=${CHOST} ${myconf} || die "bad ./configure"

	emake || die "compile problem"

	# for the rescue package regardless of use static
	cd ${S}/src/filed
	make static-bacula-fd
	cd ${S}

	if use static
	then
		cd ${S}/src/console
		make static-console
		cd ${S}/src/dird
		make static-bacula-dir
		if use gnome
		then
		  cd ${S}/src/gnome-console
		  make static-gnome-console
		fi
		if use wxwindows
		then
		  cd ${S}/src/wx-console
		  make static-wx-console
		fi
		cd ${S}/src/stored
		make static-bacula-sd
	fi
}

src_install() {
	make DESTDIR=${D} install || die

	if use static
	then
		cd ${S}/src/filed
		cp static-bacula-fd ${D}/usr/sbin/bacula-fd
		cd ${S}/src/console
		cp static-console ${D}/usr/sbin/console
		cd ${S}/src/dird
		cp static-bacula-dir ${D}/usr/sbin/bacula-dir
		if use gnome
		then
			cd ${S}/src/gnome-console
			cp static-gnome-console ${D}/usr/sbin/gnome-console
		fi
		if use wxwindows
		then
			cd ${S}/src/wx-console
			cp static-wx-console ${D}/usr/sbin/wx-console
		fi
		cd ${S}/src/stored
		cp static-bacula-sd ${D}/usr/sbin/bacula-sd
	fi

	# the menu stuff
	if use gnome
	then
	mkdir -p ${D}/usr/share/pixmaps
	mkdir -p ${D}/usr/share/applications
	cp ${S}/scripts/bacula.png ${D}/usr/share/pixmaps/bacula.png
	cp ${S}/scripts/bacula.desktop.gnome2.xsu ${D}/usr/share/applications/bacula.desktop
	cp ${S}/src/tray-monitor/generic.xpm ${D}/usr/share/pixmaps/bacula-tray-monitor.xpm
	cp ${S}/scripts/bacula-tray-monitor.desktop \
		${D}/usr/share/applications/bacula-tray-monitor.desktop
	chmod 755 ${D}/usr/sbin/bacula-tray-monitor
	chmod 644 ${D}/etc/bacula/tray-monitor.conf
	fi

	# the database update scripts
	mkdir -p ${D}/etc/bacula/updatedb
	cp ${S}/updatedb/* ${D}/etc/bacula/updatedb/
	chmod 754 ${D}/etc/bacula/updatedb/*

	# the cdrom rescue package
	mkdir -p ${D}/etc/bacula/rescue/cdrom
	cp -R ${S}/rescue/linux/cdrom/* ${D}/etc/bacula/rescue/cdrom/
	mkdir ${D}/etc/bacula/rescue/cdrom/bin
	cp ${S}/src/filed/static-bacula-fd ${D}/etc/bacula/rescue/cdrom/bin/bacula-fd
	chmod 754 ${D}/etc/bacula/rescue/cdrom/bin/bacula-fd

	# documentation
	for a in ${S}/{Changelog,README,ReleaseNotes,kernstodo,LICENSE,doc/bacula.pdf}
	do
		dodoc $a
	done

	dohtml -r ${S}/doc/html-manual doc/home-page
	chown -R root:root ${D}/usr/share/doc/${PF}
	chmod -R 644 ${D}/usr/share/doc/${PF}/*
	
	# clean up permissions left broken by install
	chmod o-r ${D}/etc/bacula/query.sql

	# remove the working dir so we can add it postinst with group
	rmdir ${D}/var/bacula

	# this is now in the source package processed by configure
	exeinto /etc/init.d
	newexe ${S}/platforms/gentoo/bacula-init bacula
}

pkg_postinst() {
	# create the daemon group
	HAVE_BACULA=`cat /etc/group | grep bacula 2>/dev/null`
	if [ -z $HAVE_BACULA ]; then
	enewgroup bacula
	einfo
	einfo "The group bacula has been created. Any users you add to"
	einfo "this group have access to files created by the daemons."
	fi

	# the working directory
	install -m0750 -o root -g bacula -d ${ROOT}/var/bacula

	# link installed bacula-fd.conf into rescue directory
	ln -s /etc/bacula/bacula-fd.conf /etc/bacula/rescue/cdrom/bacula-fd.conf

	einfo
	einfo "The CDRom rescue disk package has been installed into the"
	einfo "/etc/bacula/rescue/cdrom/ directory. Please examine the manual"
	einfo "for information on creating a rescue CD."
	einfo

	if [ ! $BUILD_CLIENT_ONLY ]; then
	einfo
	einfo "Please note either/or nature of database USE flags for"
	einfo "Bacula.  If mysql is set, it will be used, else postgres"
	einfo "else finally SQLite.  If you wish to have multiple DBs on"
	einfo "one system, you may wish to unset auxillary DBs for this"
	einfo "build."
	einfo

	if use mysql
	then
	# test for an existing database
	# note: this ASSUMES no password has been set for bacula database
	DB_VER=`mysql 2>/dev/null bacula -e 'select * from Version;'|tail -n 1`
		if [ -z "$DB_VER" ]; then
		einfo "This appears to be a new install and you plan to use mysql"
		einfo "for your catalog database. You should now create it by doing"
		einfo "these commands:"
		einfo " sh /etc/bacula/grant_mysql_privileges"
		einfo " sh /etc/bacula/create_mysql_database"
		einfo " sh /etc/bacula/make_mysql_tables"
		elif [ "$DB_VER" -lt "8" ]; then
		elinfo "This release requires an upgrade to your bacula database"
		einfo "as the database format has changed.  Please read the"
		einfo "manual chapter for how to upgrade your database!!!"
		einfo
		einfo "Backup your database with the command:"
		einfo " mysqldump -f --opt bacula | bzip2 > /var/bacula/bacula_backup.sql.bz"
		einfo
		einfo "Then update your database using the scripts found in"
		einfo "/etc/bacula/updatedb/ from your current version $DB_VER to"
		einfo "version 8. Note that scripts must be run in order from your"
		einfo "version to the current version."
		fi
	fi

	if use postgres
	then
	# test for an existing database
	# note: this ASSUMES no password has been set for bacula database
	DB_VER=`echo 'select * from Version;' | psql bacula 2>/dev/null | tail -3 | head -1`
		if [ -z "$DB_VER" ]; then
		einfo "This appears to be a new install and you plan to use postgresql"
		einfo "for your catalog database. You should now create it by doing"
		einfo "these commands:"
		einfo " sh /etc/bacula/create_postgresql_database"
		einfo " sh /etc/bacula/make_postgresql_tables"
		einfo " sh /etc/bacula/grant_postgresql_privileges"
		elif [ "$DB_VER" -lt "8" ]; then
		elinfo "This release requires an upgrade to your bacula database"
		einfo "as the database format has changed.  Please read the"
		einfo "manual chapter for how to upgrade your database!!!"
		einfo
		einfo "Backup your database with the command:"
		einfo " pg_dump bacula | bzip2 > /var/bacula/bacula_backup.sql.bz2"
		einfo
		einfo "Then update your database using the scripts found in"
		einfo "/etc/bacula/updatedb/ from your current version $DB_VER to"
		einfo "version 8. Note that scripts must be run in order from your"
		einfo "version to the current version."
		fi
	fi

	if use sqlite
	then
	# test for an existing database
	# note: this ASSUMES no password has been set for bacula database
	DB_VER=`echo "select * from Version;" | sqlite 2>/dev/null /var/bacula/bacula.db | tail -n 1`
		if [ -z "$DB_VER" ]; then
		einfo "This appears to be a new install and you plan to use sqlite"
		einfo "for your catalog database. You should now create it by doing"
		einfo "these commands:"
		einfo " sh /etc/bacula/grant_sqlite_privileges"
		einfo " sh /etc/bacula/create_sqlite_database"
		einfo " sh /etc/bacula/make_sqlite_tables"
		elif [ "$DB_VER" -lt "8" ]; then
		elinfo "This release requires an upgrade to your bacula database"
		einfo "as the database format has changed.  Please read the"
		einfo "manual chapter for how to upgrade your database!!!"
		einfo
		einfo "Backup your database with the command:"
		einfo " echo .dump | sqlite /var/bacula/bacula.db | bzip2 > \\"
		einfo "   /var/bacula/bacula_backup.sql.bz2"
		einfo
		einfo "Then update your database using the scripts found in"
		einfo "/etc/bacula/updatedb/ from your current version $DB_VER to"
		einfo "version 8. Note that scripts must be run in order from your"
		einfo "version to the current version."
		fi
	fi
	fi

	einfo
	einfo "Review your configuration files in /etc/bacula and"
	if [ $BUILD_CLIENT_ONLY ]; then
		einfo "since this is a client-only build edit the init"
		einfo "script /etc/init.d/bacula and comment out the sections"
		einfo "for the director and storage daemons and then"
	fi
	einfo "start the daemons:"
	einfo " /etc/init.d/bacula start"
	einfo
	einfo "You may also wish to:"
	einfo " rc-update add bacula default"
	einfo
}
