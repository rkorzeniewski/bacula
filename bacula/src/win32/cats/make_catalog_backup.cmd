#!/bin/sh
#
# This script dumps your Bacula catalog in ASCII format
# It works for MySQL, SQLite, and PostgreSQL
#
#  $1 is the name of the database to be backed up and the name
#     of the output file (default = bacula
#  $2 is the user name with which to access the database
#     (default = bacula).
#  $3 is the password with which to access the database or "" if no password
#     (default "")
#
#
cd %WORKING_DIR%
del /f bacula.sql

set MYSQLPASSWORD=

if "%3"!="" set MYSQLPASSWORD=" --password=%3"
%SQL_BINDIR%/mysqldump -u %2 %MYSQLPASSWORD% -f --opt %1 >%1.sql

#
#  To read back a MySQL database use: 
#     cd @working_dir@
#     rm -f @SQL_BINDIR@/../var/bacula/*
#     mysql <bacula.sql
#
#  To read back a SQLite database use:
#     cd @working_dir@
#     rm -f bacula.db
#     sqlite bacula.db <bacula.sql
#
#  To read back a PostgreSQL database use:
#     cd @working_dir@
#     dropdb bacula
#     createdb bacula
#     psql bacula <bacula.sql
#
