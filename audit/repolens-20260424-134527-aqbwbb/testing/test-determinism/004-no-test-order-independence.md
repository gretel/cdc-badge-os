---
title: "[LOW] No test order independence verification"
severity: LOW
domain: testing
lens: test-determinism
labels:
  - "test-determinism"
  - "test-order"
  - "CI/CD"
---

## Summary
The test runner in `third_party/libtropic/tropic01_model/main.c` includes tests via `lt_test_registry.c.inc` but there's no mechanism to:
1. Run tests in random order to verify order independence
2. Track and report test execution order
3. Ensure tests can run in any order without failures

Tests are currently executed in a fixed order determined by the registry, which may hide order-dependent bugs.

## Impact
- **Hidden order dependencies**: Tests may pass in the current order but fail when run in a different order
- **CI/CD flakiness**: If tests are run in different orders across environments, intermittent failures may occur
- **Regression detection**: Order-dependent bugs may go undetected until a new test is added

## Evidence

### Test registry inclusion
File: `third_party/libtropic/tropic01_model/main.c:89`
```c
#ifdef LT_BUILD_TESTS
#include "lt_test_registry.c.inc"
#endif
```

The registry includes tests but doesn't specify order independence guarantees.

### No order randomization
The test runner doesn't have any mechanism for:
- Randomizing test order
- Running tests in reverse order
- Running subsets of tests

### Shared global state (see finding #2)
Multiple tests share `lt_test_cleanup_function` and `g_h` globals, creating potential for order-dependent behavior.

### Cleanup function pattern
From `lt_test_common.c:21-36`:
```c
void lt_assert_fail_handler(void)
{
    if (NULL != lt_test_cleanup_function) {
        // ...
        lt_ret_t ret = lt_test_cleanup_function();
        // ...
    }
    LT_FINISH_TEST();
}
```

If a test fails and cleanup runs, the global state changes. The next test may start with unexpected state.

## Recommended Fix

### 1. Add test order randomization option
Add a build flag to randomize test order:

```c
#ifdef LT_RANDOMIZE_TEST_ORDER
// Shuffle test array before running
static void shuffle_tests(test_entry_t *tests, int count) {
    for (int i = count - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        test_entry_t temp = tests[i];
        tests[i] = tests[j];
        tests[j] = temp;
    }
}
#endif
```

### 2. Log test execution order
Add logging of test execution order for debugging:

```c
LT_LOG_INFO("=== Running test: %s ===", tests[i].name);
tests[i].func(&__lt_handle__);
LT_LOG_INFO("=== Test %s completed ===", tests[i].name);
```

### 3. Add verification that globals are reset
Ensure each test resets shared globals at the start:

```c
void lt_test_rev_mcounter(lt_handle_t *h)
{
    // Reset globals at start of each test
    lt_test_cleanup_function = NULL;
    g_h = NULL;
    
    // ... rest of test
}
```

### 4. Create CI script to run tests multiple times with random order
Add a CI script that runs the test suite multiple times with different random seeds:

```bash
#!/bin/bash
for i in {1..10}; do
    echo "=== Run $i ==="
    ./test_runner --random-seed $RANDOM
done
```

## References
- [Test order independence](https://www.testim.io/blog/test-ordering/)
- [Randomized test execution](https://www.baeldung.com/java-random-test-order)
