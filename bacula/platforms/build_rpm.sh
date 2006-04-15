#!/bin/sh

# shell script to build bacula rpm release
# copy this script into your SPECS directory along with the spec file
# 09 Apr 2006 D. Scott Barninger

# usage: ./build_rpm.sh platform mysql_version

if [ "$1" = "--help" ]; then
	echo
	echo usage: ./build_rpm.sh platform mysql_version
	echo
	echo You must specify a platform:
	echo rh7,rh8,rh9,fc1,fc3,fc4,wb3,rhel3,rhel4,centos3,centos4,su9,su10,mdk
	echo
	echo You must specify a MySQL version either 3 or 4.
	echo
	echo Example: ./build_rpm.sh fc4 4
	exit 1
fi

PLATFORM=$1
MYSQL=$2

if [ -z "$PLATFORM" ]; then
	echo
	echo usage: ./build_rpm.sh platform mysql_version
	echo
	echo You must specify a platform:
	echo rh7,rh8,rh9,fc1,fc3,fc4,wb3,rhel3,rhel4,centos3,centos4,su9,su10,mdk
	exit 1
fi
if [ -z "$MYSQL" ]; then
	echo
	echo usage: ./build_rpm.sh platform mysql_version
	echo
	echo You must specify a MySQL version either 3 or 4.
	exit 1
fi

if [ "$MYSQL" = "3" ]; then
	MYSQL_VER=mysql
fi
if [ "$MYSQL" = "4" ]; then
	MYSQL_VER=mysql4
fi

echo Building MySQL packages for "$PLATFORM"...
sleep 5
rpmbuild -ba --define "build_${PLATFORM} 1" \
	--define "build_${MYSQL_VER} 1" \
	bacula.spec

echo Building PostgreSQL packages for "$PLATFORM"...
sleep 5
rpmbuild -bb --define "build_${PLATFORM} 1" \
	--define "build_postgresql 1" \
	bacula.spec

echo Building SQLite packages for "$PLATFORM"...
sleep 5
rpmbuild -bb --define "build_${PLATFORM} 1" \
	--define "build_sqlite 1" \
	bacula.spec
