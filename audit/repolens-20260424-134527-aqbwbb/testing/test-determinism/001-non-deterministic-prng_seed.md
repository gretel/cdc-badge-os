---
title: "[MEDIUM] Test PRNG seed initialized with system entropy instead of fixed value"
severity: MEDIUM
domain: testing
lens: test-determinism
labels:
  - "test-determinism"
  - "randomness"
  - "reproducibility"
---

## Summary
In `/input/20260423-132359-oj8ayc/cdc-badge-os/third_party/libtropic/tropic01_model/main.c:74-79`, the test runner initializes the PRNG seed using `getentropy()`:

```c
// Generate seed for the PRNG.
unsigned int prng_seed;
if (0 != getentropy(&prng_seed, sizeof(prng_seed))) {
    LT_LOG_INFO("main: getentropy() failed (%s)!", strerror(errno));
    return -1;
}

// Seed the PRNG.
srand(prng_seed);
LT_LOG_INFO("PRNG initialized with seed=%u\n", prng_seed);
```

This means each test run uses a different random seed, causing tests that rely on random data (like `lt_test_rev_ping`, `lt_test_rev_random_value_get`, `lt_test_rev_mac_and_destroy`, `lt_test_rev_mcounter`, `lt_test_rev_r_mem`, `lt_test_rev_ecdsa_sign`, `lt_test_rev_eddsa_sign`) to produce different random data on each execution.

## Impact
- **Reduced reproducibility**: When a test fails, developers cannot easily reproduce the exact same conditions to debug the issue
- **Flaky test detection**: Hard to distinguish between actual bugs and tests that just need different random inputs
- **CI/CD debugging**: Failed CI builds are harder to diagnose without knowing the exact seed used

## Evidence
File: `third_party/libtropic/tropic01_model/main.c:74-83`

Tests affected (all use `lt_random_bytes()` which ultimately depends on `rand()`):
- `lt_test_rev_ping.c:43-46` - generates random message lengths and data
- `lt_test_rev_random_value_get.c:40-46` - generates random lengths for random value retrieval
- `lt_test_rev_mac_and_destroy.c:76-88` - generates random PIN attempts, secret values, and PIN length
- `lt_test_rev_mcounter.c:88-90, 117-118` - generates random initial counter values
- `lt_test_rev_r_mem.c:120, 167-169` - generates random data for memory tests
- `lt_test_rev_ecdsa_sign.c:113-118` - generates random message lengths and data
- `lt_test_rev_eddsa_sign.c:113-118` - generates random message lengths and data

The comment at line 80-83 acknowledges this but suggests it's "okay" for the TCP port model:
```c
// Note: We use rand() for random numbers, which is not cryptographically secure, but it is okay here because the
// TCP port is targeted for use with the model only. Thanks to this, we can log the used seed and if needed,
// reproduce the random tests.
```

However, the seed is logged but not easily used for reproduction since it's dynamic per run.

## Recommended Fix
Add a build-time or runtime option to use a fixed seed for deterministic testing:

1. **Add a compile-time seed option**:
```c
#ifndef TEST_PRNG_SEED
#define TEST_PRNG_SEED 12345  // Default fixed seed for reproducibility
#endif

unsigned int prng_seed;
#ifdef USE_FIXED_SEED
    prng_seed = TEST_PRNG_SEED;
#else
    if (0 != getentropy(&prng_seed, sizeof(prng_seed))) {
        LT_LOG_ERROR("main: getentropy() failed (%s)!", strerror(errno));
        return -1;
    }
#endif
srand(prng_seed);
LT_LOG_INFO("PRNG initialized with seed=%u\n", prng_seed);
```

2. **Usage**:
   - For deterministic tests: `cmake -DUSE_FIXED_SEED=1 -DTEST_PRNG_SEED=42 ..`
   - For random tests (default): `cmake ..`

3. **Environment variable alternative**: Support `TEST_PRNG_SEED` environment variable to allow runtime control without recompilation.

## References
- [Testing deterministic vs non-deterministic tests](https://martinfowler.com/articles/nonDeterminism.html)
- [Seeded random number generation for tests](https://www.baeldung.com/cs/seeded-random-number-generation)
