copy scripts\bacula-dir-tape.conf bin\bacula-dir.conf
copy scripts\bacula-sd-tape.conf bin\bacula-sd.conf
copy scripts\test-bacula-fd.conf bin\bacula-fd.conf
copy scripts\test-console.conf bin\bconsole.conf

REM get proper SD tape definitions
copy scripts\win32_tape_options bin\tape_options
