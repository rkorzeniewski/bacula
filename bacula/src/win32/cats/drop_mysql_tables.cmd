rem
rem Script to delete Bacula tables for MySQL
rem

if %SQL_BINDIR%/mysql $* < drop_mysql_tables.sql
set RESULT=%ERRORLEVEL%
if %RESULT% GTR 0 goto :ERROR
echo "Deletion of Bacula MySQL tables succeeded."
exit /b 0

:ERROR
echo "Deletion of Bacula MySQL tables failed."
exit /b %RESULT%
