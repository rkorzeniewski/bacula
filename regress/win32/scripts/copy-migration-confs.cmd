REM
REM Setup for migration tests
REM
COPY scripts\bacula-dir-migration.conf bin\bacula-dir.conf
COPY scripts\bacula-sd-migration.conf bin\bacula-sd.conf
COPY scripts\test-bacula-fd.conf bin\bacula-fd.conf
COPY scripts\test-console.conf bin\bconsole.conf
