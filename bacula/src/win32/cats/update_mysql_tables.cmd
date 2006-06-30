rem
rem Script to update MySQL tables from version 1.38 to 1.39
rem
echo " "
echo "This script will update a Bacula MySQL database from version 9 to 9"
echo "Depending on the size of your database,"
echo "this script may take several minutes to run."
echo " "

if %SQL_BINDIR%/mysql $* -f < update_mysql_tables.sql
set RESULT=%ERRORLEVEL%
if %RESULT% GTR 0 goto :ERROR
echo "Update of Bacula MySQL tables succeeded."
exit /b 0

:ERROR
echo "Update of Bacula MySQL tables failed."
exit /b %RESULT%
