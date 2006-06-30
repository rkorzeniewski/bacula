rem
rem shell script to drop Bacula database(s)
rem

%SQL_BINDIR%/mysql $* -f -e "DROP DATABASE bacula;"
set RESULT=%ERRORLEVEL%
if %RESULT% GTR 0 goto :ERROR
echo "Drop of bacula database succeeded."
exit /b 0

:ERROR
echo "Drop of bacula database failed."
exit /b %RESULT%
