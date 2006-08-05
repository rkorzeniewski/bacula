#!/bin/bash

# shell script to build bacula rpm release
# copy this script into a working directory with the src rpm to build and execute
# 16 Jul 2006 D. Scott Barninger

# Copyright (C) 2006 Kern Sibbald
# licensed under GPL-v2

# signing rpms
# Make sure you have a .rpmmacros file in your home directory containing the following:
#
# %_signature gpg
# %_gpg_name Your Name <your-email@site.org>
#
# the %_gpg_name information must match your key


# usage: ./build_rpm.sh

###########################################################################################
# script configuration section

VERSION=1.39.18
RELEASE=1

# set to one of rh7,rh8,rh9,fc1,fc3,fc4,fc5,wb3,rhel3,rhel4,centos3,centos4,su9,su10,mdk,mdv
PLATFORM=su10

# MySQL version
# set to empty (for MySQL 3), 4 or 5
MYSQL=4

# building wxconsole
# set to 1 to build wxconsole package else set 0
WXCONSOLE=0

# enter your name and email address here
PACKAGER="Your Name <your-email@site.org>"

# enter the full path to your RPMS output directory
RPMDIR=/usr/src/packages/RPMS/i586

# enter the full path to your rpm BUILD directory
RPMBUILD=/usr/src/packages/BUILD

# enter your arch string here (i386, i586, i686)
ARCH=i586

############################################################################################

SRPM=bacula-$VERSION-$RELEASE.src.rpm

echo Building MySQL packages for "$PLATFORM"...
sleep 2
if [ "$WXCONSOLE" = "1" ]; then
rpmbuild --rebuild --define "build_${PLATFORM} 1" \
--define "build_mysql${MYSQL} 1" \
--define "contrib_packager ${PACKAGER}" \
--define "build_python 1" \
--define "build_wxconsole 1" \
${SRPM}
else
rpmbuild --rebuild --define "build_${PLATFORM} 1" \
--define "build_mysql${MYSQL} 1" \
--define "build_python 1" \
--define "contrib_packager ${PACKAGER}" ${SRPM}
fi
rm -rf ${RPMBUILD}/*

echo Building PostgreSQL packages for "$PLATFORM"...
sleep 2
rpmbuild --rebuild --define "build_${PLATFORM} 1" \
--define "build_postgresql 1" \
--define "contrib_packager ${PACKAGER}" \
--define "build_python 1" \
--define "nobuild_gconsole 1" ${SRPM}
rm -rf ${RPMBUILD}/*

echo Building SQLite packages for "$PLATFORM"...
sleep 2
rpmbuild --rebuild --define "build_${PLATFORM} 1" \
--define "build_sqlite 1" \
--define "contrib_packager ${PACKAGER}" \
--define "build_python 1" \
--define "nobuild_gconsole 1" ${SRPM}
rm -rf ${RPMBUILD}/*

# delete the updatedb package and any debuginfo packages built
rm -f ${RPMDIR}/bacula*debug*
rm -f ${RPMDIR}/bacula-updatedb*

# copy files to cwd and rename files to final upload names

mv -f ${RPMDIR}/bacula-mysql-${VERSION}-${RELEASE}.${ARCH}.rpm \
./bacula-mysql-${VERSION}-${RELEASE}.${PLATFORM}.${ARCH}.rpm

mv -f ${RPMDIR}/bacula-postgresql-${VERSION}-${RELEASE}.${ARCH}.rpm \
./bacula-postgresql-${VERSION}-${RELEASE}.${PLATFORM}.${ARCH}.rpm

mv -f ${RPMDIR}/bacula-sqlite-${VERSION}-${RELEASE}.${ARCH}.rpm \
./bacula-sqlite-${VERSION}-${RELEASE}.${PLATFORM}.${ARCH}.rpm

mv -f ${RPMDIR}/bacula-mtx-${VERSION}-${RELEASE}.${ARCH}.rpm \
./bacula-mtx-${VERSION}-${RELEASE}.${PLATFORM}.${ARCH}.rpm

mv -f ${RPMDIR}/bacula-client-${VERSION}-${RELEASE}.${ARCH}.rpm \
./bacula-client-${VERSION}-${RELEASE}.${PLATFORM}.${ARCH}.rpm

mv -f ${RPMDIR}/bacula-gconsole-${VERSION}-${RELEASE}.${ARCH}.rpm \
./bacula-gconsole-${VERSION}-${RELEASE}.${PLATFORM}.${ARCH}.rpm

mv -f ${RPMDIR}/bacula-wxconsole-${VERSION}-${RELEASE}.${ARCH}.rpm \
./bacula-wxconsole-${VERSION}-${RELEASE}.${PLATFORM}.${ARCH}.rpm

# now sign the packages
echo Ready to sign packages...
sleep 2
rpm --addsign ./*.rpm

echo
echo Finished.
echo
ls

# changelog
# 16 Jul 2006 initial release
# 05 Aug 2006 add python support



