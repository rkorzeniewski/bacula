rem
rem Script to grant privileges to the bacula database
rem

%SQL_BINDIR%\mysql $* -u root -f < grant_mysql_privileges.sql
set RESULT=%ERRORLEVEL%
if %RESULT% GTR 0 goto :ERROR
echo "Privileges for bacula granted."
exit /b 0

:ERROR
echo "Error creating privileges."
exit /b %RESULT%
