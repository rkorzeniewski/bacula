@echo off
rem 
rem  This script dumps your Bacula catalog in ASCII format
rem  It works for MySQL, SQLite, and PostgreSQL
rem 
rem   %1 is the name of the database to be backed up and the name
rem      of the output file (default = bacula
rem   %2 is the user name with which to access the database
rem      (default = bacula).
rem   %3 is the password with which to access the database or "" if no password
rem      (default "")
rem 
rem 
@echo on

cd @working_dir_cmd@
del /f bacula.sql 2>nul

set MYSQLPASSWORD=

if not "%3"=="" set MYSQLPASSWORD=--password=%3
"@SQL_BINDIR@\mysqldump" -u %2 %MYSQLPASSWORD% -f --opt %1 >%1.sql

@echo off
rem 
rem   To read back a MySQL database use: 
rem      cd @working_dir_cmd@
rem      rd /s /q @SQL_BINDIR@\..\data\bacula
rem      mysql < bacula.sql
rem 
rem   To read back a SQLite database use:
rem      cd @working_dir_cmd@
rem      del /f bacula.db
rem      sqlite bacula.db < bacula.sql
rem 
rem   To read back a PostgreSQL database use:
rem      cd @working_dir_cmd@
rem      dropdb bacula
rem      createdb bacula
rem      psql bacula < bacula.sql
rem 
