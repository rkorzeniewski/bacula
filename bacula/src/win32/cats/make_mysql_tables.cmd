rem
rem Script to create Bacula MySQL tables
rem

%SQL_BINDIR%\mysql -f < make_mysql_tables.sql
set RESULT=%ERRORLEVEL%
if %RESULT% gt 0 goto :ERROR
echo "Creation of Bacula MySQL tables succeeded."
exit /b 0

:ERROR
echo "Creation of Bacula MySQL tables failed."
exit /b %RESULT%
