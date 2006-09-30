@ECHO OFF
SETLOCAL
PATH ..\..\..\..\depkgs-msvc\nsis;..\..\..\..\depkgs-msvc\tools;%PATH%
FOR /F %%i IN ( 'sed -ne "s/.*[ \t]VERSION[ \t][ \t]*\x22\(.*\)\x22/\1/p" ^< ..\..\version.h' ) DO @SET VERSION=%%i 
makensis /V3 /DVERSION=%VERSION% /DOUT_DIR=%1%2 /DDOC_DIR=..\..\..\..\docs /DBUILD_TOOLS=%3 /DVC_REDIST_DIR=%4 /DDEPKGS_BIN=..\..\..\..\depkgs-msvc\bin /DBACULA_BIN=..\%2 /DCATS_DIR=..\cats /DSCRIPT_DIR=..\scripts winbacula.nsi
EXIT /B %ERRORLEVEL%
ENDLOCAL
