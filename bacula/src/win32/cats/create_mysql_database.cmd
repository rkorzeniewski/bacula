rem
rem Script to create Bacula database(s)
rem

%SQL_BINDIR%\mysql $* -e "CREATE DATABASE bacula;"
set RESULT=%ERRORLEVEL%
if %RESULT% GTR 0 goto :ERROR
echo "Creation of bacula database succeeded."
exit /b 0

:ERROR
echo "Creation of bacula database failed."
exit /b %RESULT%
