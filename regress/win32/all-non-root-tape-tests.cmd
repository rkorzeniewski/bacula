REM
REM Run all tape tests
REM
CALL config_var
IF NOT "%AUTOCHANGER%" == "nul" mtx -f %AUTOCHANGER% load 1 >nul 2>&1
COPY test.out test1.out
CALL tests\test0
CALL tests\backup-bacula-tape
CALL tests\btape-fill-tape
CALL tests\fixed-block-size-tape
CALL tests\four-concurrent-jobs-tape
CALL tests\four-jobs-tape
CALL tests\incremental-tape
CALL tests\relabel-tape
CALL tests\restore-by-file-tape
CALL tests\small-file-size-tape
CALL tests\truncate-bug-tape
CALL tests\two-pool-tape
CALL tests\2drive-incremental-2tape
CALL tests\bscan-tape
CALL tests\verify-vol-tape
ECHO.
ECHO.
ECHO Test results
TYPE test.out
CALL scripts\cleanup
