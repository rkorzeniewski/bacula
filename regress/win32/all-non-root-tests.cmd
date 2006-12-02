REM
REM Run all tests
REM
DEL test1.out
CALL tests\test0
ECHO.
CALL tests\auto-label-test
CALL tests\backup-bacula-test
CALL tests\bextract-test
CALL tests\bscan-test
CALL tests\bsr-opt-test
CALL tests\compressed-test
CALL tests\concurrent-jobs-test
CALL tests\data-encrypt-test
CALL tests\differential-test
CALL tests\four-concurrent-jobs-test
CALL tests\four-jobs-test
CALL tests\incremental-test
CALL tests\query-test
CALL tests\recycle-test
CALL tests\restore2-by-file-test
CALL tests\restore-by-file-test
CALL tests\restore-disk-seek-test
CALL tests\six-vol-test
CALL tests\span-vol-test
CALL tests\sparse-compressed-test
CALL tests\sparse-test
CALL tests\two-jobs-test
CALL tests\two-vol-test
CALL tests\verify-vol-test
REM CALL tests\weird-files2-test
REM CALL tests\weird-files-test
CALL tests\migration-job-test
CALL tests\migration-jobspan-test
CALL tests\migration-volume-test
CALL tests\migration-time-test
REM CALL tests\hardlink-test
REM 
REM The following are Virtual Disk Autochanger tests
CALL tests\two-pool-test
CALL tests\two-volume-test
CALL tests\incremental-2disk
CALL tests\2drive-incremental-2disk
CALL tests\scratch-pool-test
ECHO.
ECHO Test results
TYPE test.out
CALL scripts\cleanup
