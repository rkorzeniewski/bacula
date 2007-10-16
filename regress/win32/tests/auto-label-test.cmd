REM
REM  Test if Bacula can automatically create a Volume label.
REM

SET TestName=auto-label-test
SET JobName=AutoLabel

CALL scripts\functions set_debug 0
CALL scripts\functions copy_test_confs

ECHO %CD:\=/%/build >tmp\file-list

COPY bin\bacula-dir.conf tmp\1
sed -e "s;# Label Format;  Label Format;" tmp\1 >bin\bacula-dir.conf

CALL scripts\functions change_jobname CompressedTest %JobName%
CALL scripts\functions start_test

sed -e "s;@JobName@;%JobName%;g" -e "s;@out@;%out%;g" -e "s;@topdir@;%CD:\=/%;g" tests\auto-label-test.bscr >tmp\bconcmds

CALL scripts\functions run_bacula
CALL scripts\functions check_for_zombie_jobs storage=File
CALL scripts\functions stop_bacula

CALL scripts\functions check_two_logs
CALL scripts\functions check_restore_diff
CALL scripts\functions end_test
