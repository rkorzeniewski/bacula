rem 
rem  Shell script to update PostgreSQL tables from version 1.38 to 1.39
rem 

echo " "
echo "This script will update a Bacula PostgreSQL database from version 9 to 10"
echo " which is needed to convert from Bacula version 1.38.x to 1.39.x or higher"
echo "Depending on the size of your database,"
echo "this script may take several minutes to run."
echo " "
bindir=@SQL_BINDIR@

$bindir/psql -f update_postgresql_tables.sql -d bacula $*
if ERRORLEVEL 1 GOTO :ERROR
echo "Update of Bacula PostgreSQL tables succeeded."
EXIT /b 0
GOTO :EOF

:ERROR
echo "Update of Bacula PostgreSQL tables failed."
EXIT /b 1
