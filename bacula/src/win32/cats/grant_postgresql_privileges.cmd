rem 
rem  shell script to grant privileges to the bacula database
rem 
USER=bacula
bindir=@SQL_BINDIR@

$bindir/psql -f grant_postgresql_privileges.sql -d bacula $*
if ERRORLEVEL 1 GOTO :ERROR
echo "Error creating privileges."
EXIT /b 0
GOTO :EOF

:ERROR
echo "Drop of bacula database failed."
EXIT /b 1
