@ECHO off
IF "%1" == "start" (
   net start bacula-dir
   net start bacula-sd
   net start bacula-fd
) ELSE IF "%1" == "stop" (
   net stop bacula-dir
   net stop bacula-sd
   net stop bacula-fd
) ELSE IF "%1" == "install" (
   bacula-dir /install -c %2\bacula-dir.conf
   bacula-sd /install -c %2\bacula-sd.conf
   bacula-fd /install -c %2\bacula-fd.conf
) ELSE IF "%1" == "uninstall" (
   bacula-dir /remove
   bacula-sd /remove
   bacula-fd /remove
)
