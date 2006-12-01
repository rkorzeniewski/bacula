REM
REM Run all tape tests
REM
CALL tests\test0
CALL tests\two-volume-tape
CALL tests\incremental-2tape
ECHO.
ECHO.
echo 2 Tape Test results
TYPE test.out
CALL scripts\cleanup
