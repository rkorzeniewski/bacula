rem 
rem  shell script to create Bacula database(s)
rem 

bindir=@SQL_BINDIR@

rem use SQL_ASCII to be able to put any filename into
rem  the database even those created with unusual character sets
ENCODING="ENCODING 'SQL_ASCII'"
rem use UTF8 if you are using standard Unix/Linux LANG specifications
rem  that use UTF8 -- this is normally the default and *should* be
rem  your standard.  Bacula consoles work correctly *only* with UTF8.
rem ENCODING="ENCODING 'UTF8'"
     
$bindir/psql -f create_postgresql_database.sql -d template1 $*
if ERRORLEVEL 1 GOTO :ERROR
echo "Creation of bacula database succeeded."
EXIT /b 0
GOTO :EOF

:ERROR
echo "Creation of bacula database failed."
EXIT /b 1
