COPY scripts\new-test-bacula-dir.conf bin\bacula-dir.conf
COPY scripts\test-bacula-sd.conf bin\bacula-sd.conf
COPY scripts\test-bacula-fd.conf bin\bacula-fd.conf
COPY scripts\test-console.conf bin\bconsole.conf

REM get proper SD tape definitions
COPY scripts\win32_tape_options bin\tape_options
