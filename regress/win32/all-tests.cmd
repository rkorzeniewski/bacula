REM
REM Run all tests
REM
CALL all-non-root-tests
CALL all-root-tests
TYPE test.out
CALL scripts\cleanup
