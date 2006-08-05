rem 
rem  shell script to delete Bacula tables for PostgreSQL
rem

bindir=@SQL_BINDIR@

$bindir/psql -f drop_postgresql_tables.sql -d bacula $*
if ERRORLEVEL 1 GOTO :ERROR
echo "Deletion of Bacula PostgreSQL tables succeeded."
EXIT /b 0
GOTO :EOF

:ERROR
echo "Deletion of Bacula PostgreSQL tables failed."
EXIT /b 1
