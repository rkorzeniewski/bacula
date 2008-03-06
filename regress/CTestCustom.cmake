SET(CTEST_CUSTOM_ERROR_EXCEPTION
  ${CTEST_CUSTOM_ERROR_EXCEPTION}
  "ERROR: *database \".*\" already exists"
  "ERROR: *table \".*\" does not exist"
  "NOTICE: .*will create implicit sequence"
  "NOTICE: .*will create implicit index"
  "ERROR: *role \".*\" already exists"
  )

SET(CTEST_CUSTOM_MAXIMUM_PASSED_TEST_OUTPUT_SIZE 10000)
SET(CTEST_CUSTOM_MAXIMUM_FAILED_TEST_OUTPUT_SIZE 1048576)
