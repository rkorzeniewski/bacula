rem 
rem  shell script to create Bacula PostgreSQL tables
rem 
bindir=@SQL_BINDIR@

$bindir/psql -f make_postgresql_tables.sql -d bacula $*
if ERRORLEVEL 1 GOTO :ERROR
echo "Creation of Bacula PostgreSQL tables succeeded."
EXIT /b 0
GOTO :EOF

:ERROR
echo "Creation of Bacula PostgreSQL tables failed."
EXIT /b 1
