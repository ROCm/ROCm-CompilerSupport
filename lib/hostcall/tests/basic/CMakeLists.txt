add_tests(basic)

# Negative tests that check for a non-recoverable error
set_tests_properties(basic-no-handler PROPERTIES
  PASS_REGULAR_EXPRESSION "hostcall fatal error: no handler found for service 23")
set_tests_properties(basic-service-error PROPERTIES
  PASS_REGULAR_EXPRESSION "hostcall fatal error: handler for service 23 returned failure")

# Trivial test that does not depend on third-party headers and always
# returns success. It is meant to perform pedantic compiler checks on
# the exported C header.
target_compile_options(basic-check-header PRIVATE -Wall -Werror -Wpedantic -Wstrict-prototypes)
