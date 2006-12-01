COPY scripts\bacula-dir.conf bin\bacula-dir.conf
COPY scripts\bacula-sd.conf bin\bacula-sd.conf
COPY scripts\bacula-fd.conf bin\bacula-fd.conf
COPY scripts\bconsole.conf bin\bconsole.conf

REM get proper SD tape definitions
COPY scripts\win32_tape_options bin\tape_options
