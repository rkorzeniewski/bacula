@echo off
REM
REM
REM Bacula interface to mtx autoloader
REM
REM  $Id$
REM
REM  If you set in your Device resource
REM
REM  Changer Command = "path-to-this-script/mtx-changer %c %o %S %a %d"
REM    you will have the following input to this script:
REM
REM  So Bacula will always call with all the following arguments, even though
REM    in come cases, not all are used.
REM
REM  mtx-changer "changer-device" "command" "slot" "archive-device" "drive-index"
REM		   $1		   $2	    $3	      $4	       $5
REM
REM  for example:
REM
REM  mtx-changer /dev/sg0 load 1 /dev/nst0 0 (on a Linux system)
REM 
REM  will request to load the first cartidge into drive 0, where
REM   the SCSI control channel is /dev/sg0, and the read/write device
REM   is /dev/nst0.
REM
REM  If you need to an offline, refer to the drive as $4
REM    e.g.   mt -f $4 offline
REM
REM  Many changers need an offline after the unload. Also many
REM   changers need a sleep 60 after the mtx load.
REM
REM  N.B. If you change the script, take care to return either 
REM   the mtx exit code or a 0. If the script exits with a non-zero
REM   exit code, Bacula will assume the request failed.
REM

SET MTX=@MTX@
SET MT=@MT@
SET working_dir=@working_dir@

SET dbgfile=%working_dir%\mtx.log

REM to turn on logging, uncomment the following line
REM findstr xxx <nul >>%working_dir%\mtx.log

REM
REM check parameter count on commandline
REM
REM Check for special cases where only 2 arguments are needed, 
REM  all others are a minimum of 5
REM
IF "%1" EQU "" goto :param_count_invalid
IF "%2" EQU "" goto :param_count_invalid
IF "%2" EQU "list" goto :param_count_valid
IF "%2" EQU "slots" goto :param_count_valid
IF "%3" EQU "" goto :param_count_invalid
IF "%4" EQU "" goto :param_count_invalid
IF "%5" EQU "" goto :param_count_invalid
GOTO :param_count_valid

:param_count_invalid
   echo Insufficient number of arguments given.
   IF "%2" EQU "" (
      echo   At least two arguments must be specified.
   ) else echo   Command expected 5 arguments.
:usage
   ECHO.
   ECHO usage: mtx-changer ctl-device command [slot archive-device drive-index]
   ECHO        Valid commands are: unload, load, list, loaded, and slots.
   EXIT /B 1

:param_count_valid

REM Setup arguments
SET ctl=%1
SET cmd=%2
SET slot=%3
SET device=%4
SET drive=%5

CALL :debug "Parms: %ctl% %cmd% %slot% %device% %drive%"

IF "%cmd%" NEQ "unload" goto :cmdLoad
   CALL :debug "Doing mtx -f %ctl% unload %slot% %drive%"
   %MT% -f %device% offline
   %MTX% -f %ctl% unload %slot% %drive%
   EXIT /B %ERRORLEVEL%

:cmdLoad
IF "%cmd%" NEQ "load" goto :cmdList
   CALL :debug "Doing mtx -f %ctl% load %slot% %drive%"
   %MTX% -f %ctl% load %slot% %drive%
   SET rtn=%ERRORLEVEL%
   %MT% -f %device% load
   CALL :wait_for_drive %device%
   EXIT /B %rtn%

:cmdList
IF "%cmd%" NEQ "list" goto :cmdLoaded
   CALL :debug "Doing mtx -f %ctl% -- to list volumes"
   CALL :make_temp_file
   IF ERRORLEVEL 1 GOTO :EOF
REM Enable the following if you are using barcodes and need an inventory
REM   %MTX% -f %ctl% inventory
   %MTX% -f %ctl% status >%TMPFILE%
   SET rtn=%ERRORLEVEL%
   FOR /F "usebackq tokens=3,6 delims==: " %%i in ( `findstr /R /C:" *Storage Element [0-9]*:.*Full" %TMPFILE%` ) do echo %%i:%%j
   FOR /F "usebackq tokens=7,10" %%i in ( `findstr /R /C:"^Data Transfer Element [0-9]*:Full (Storage Element [0-9]" %TMPFILE%` ) do echo %%i:%%j
   DEL /F "%TMPFILE%" >nul 2>&1
REM
REM If you have a VXA PacketLoader and the above does not work, try
REM  turning it off and enabling the following line.
REM   %MTX% -f %ctl% status | grep " *Storage Element [0-9]*:.*Full" | sed "s/*Storage Element //" | sed "s/Full :VolumeTag=//"
   EXIT /B %rtn%

:cmdLoaded
IF "%cmd%" NEQ "loaded" goto :cmdSlots
   CALL :debug "Doing mtx -f %ctl% %drive% -- to find what is loaded"
   CALL :make_temp_file
   %MTX% -f %ctl% status >%TMPFILE%
   SET rtn=%ERRORLEVEL%
   FOR /F "usebackq tokens=7" %%i in ( `findstr /R /C:"^Data Transfer Element %drive%:Full" %TMPFILE%` ) do echo %%i
   findstr /R /C:"^Data Transfer Element %drive%:Empty" %TMPFILE% >nul && echo 0
   DEL /F "%TMPFILE%" >nul 2>&1
   EXIT /B %rtn%

:cmdSlots
IF "%cmd%" NEQ "slots" goto :cmdUnknown
   CALL :debug "Doing mtx -f %ctl% -- to get count of slots"
   CALL :make_temp_file
   %MTX% -f %ctl% status >%TMPFILE%
   SET rtn=%ERRORLEVEL%
   FOR /F "usebackq tokens=5" %%i in ( `findstr /R /C:" *Storage Changer" %TMPFILE%` ) do echo %%i
   DEL /F "%TMPFILE%" >nul 2>&1
   EXIT /B %rtn%

:cmdUnknown
   ECHO '%cmd%' is an invalid command.
   GOTO :usage

REM
REM log whats done
REM
:debug
   IF NOT EXIST "%dbgfile%" GOTO :EOF
   FOR /F "usebackq tokens=2-4,5-7 delims=/:. " %%i in ( '%DATE% %TIME%' ) do SET TIMESTAMP=%%k%%i%%j-%%l:%%m:%%n
   ECHO %TIMESTAMP% %*>> %dbgfile%
   GOTO :EOF

REM
REM Create a temporary file
REM
:make_temp_file
   REM SET TMPFILE=%working_dir%\mtx.tmp
   SET TMPFILE=c:\bacula.test\working\mtx.tmp
   IF EXIST "%TMPFILE%" (
      ECHO Temp file security problem on: %TMPFILE%
      EXIT /B 1
   )
   GOTO :EOF

REM
REM The purpose of this function to wait a maximum 
REM   time for the drive. It will
REM   return as soon as the drive is ready, or after
REM   waiting a maximum of 300 seconds.
REM Note, this is very system dependent, so if you are
REM   not running on Linux, you will probably need to
REM   re-write it, or at least change the grep target.
REM
:wait_for_drive
   FOR /L %%i IN ( 1, 1, 300 ) DO (
      %MT% -f %1 status | findstr ONLINE >NUL 2>&1
      IF %ERRORLEVEL%==0 GOTO :EOF
      CALL :debug "Device %1 - not ready, retrying..."
      CALL :sleep 1
   )
   CALL :debug "Device %1 - not ready, timed out..."
   GOTO :EOF

:sleep
   CALL :get_secs
   SET start_time=%ERRORLEVEL%
   SET /A end_time=100*%1+start_time
:sleep_wait
   CALL :get_secs
   IF %ERRORLEVEL% LSS %start_time% GOTO :sleep
   IF %ERRORLEVEL% LSS %end_time% GOTO :sleep_wait
   GOTO :EOF

:get_secs
   FOR /F "tokens=3,4 delims=:. " %%i IN ( "%TIME%" ) do SET /A "secs= ( 1%%i %% 100 ) * 100 + ( 1%%j %% 100 )"
   EXIT /B %secs%
