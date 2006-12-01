COPY scripts\win32-bacula-dir-tape.conf bin\bacula-dir.conf
COPY scripts\win32-bacula-sd-tape.conf bin\bacula-sd.conf
COPY scripts\win32-bacula-fd.conf bin\bacula-fd.conf
COPY scripts\test-console.conf bin\bconsole.conf

REM get proper SD tape definitions
COPY scripts\win32_tape_options bin\tape_options
