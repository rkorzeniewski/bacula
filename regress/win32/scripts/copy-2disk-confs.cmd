REM
REM Setup for using the Virtual Disk Changer (simulates tape changer)
REM
COPY scripts\bacula-dir-tape.conf bin\bacula-dir.conf
COPY scripts\bacula-sd-2disk.conf bin\bacula-sd.conf
COPY scripts\test-bacula-fd.conf bin\bacula-fd.conf
COPY scripts\test-console.conf bin\bconsole.conf
COPY bin\bacula-dir.conf tmp\1
sed -e "s;# Autochanger = yes;  Autochanger = yes;g" tmp\1 >bin\bacula-dir.conf
