REM
REM Run all root tests
REM
DEL test.out
CALL tests\dev-test-root
CALL tests\etc-test-root
CALL tests\lib-test-root
CALL tests\usr-tape-root
TYPE test.out
CALL scripts\cleanup
