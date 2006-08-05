rem 
rem  shell script to drop Bacula database(s)
rem 

bindir=@SQL_BINDIR@

$bindir/dropdb bacula
if ERRORLEVEL 1 GOTO :ERROR
echo "Drop of bacula database succeeded."
EXIT /b 0
GOTO :EOF

:ERROR
echo "Drop of bacula database failed."
EXIT /b 1
