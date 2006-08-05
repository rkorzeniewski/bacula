rem 
rem  Shell script to fix PostgreSQL tables in version 8
rem 

echo " "
echo "This script will fix a Bacula PostgreSQL database version 8"
echo "Depending on the size of your database,"
echo "this script may take several minutes to run."
echo " "
#
# Set the following to the path to psql.
bindir=****EDIT-ME to be the path to psql****

$bindir/psql $* -f fix_postgresql_tables.sql
if ERRORLEVEL 1 GOTO :ERROR
echo "Update of Bacula PostgreSQL tables succeeded."
EXIT /b 0
GOTO :EOF

:ERROR
echo "Update of Bacula PostgreSQL tables failed."
EXIT /b 1
