---
title: "[LOW] Tests depend on hardware-specific state (TROPIC01 chip slots)"
severity: LOW
domain: testing
lens: test-determinism
labels:
  - "test-determinism"
  - "hardware-dependency"
  - "test-isolation"
---

## Summary
The functional tests for TROPIC01 (e.g., `lt_test_rev_ecc_key_generate.c`, `lt_test_rev_ecdsa_sign.c`, `lt_test_rev_eddsa_sign.c`, `lt_test_rev_r_mem.c`, `lt_test_rev_mac_and_destroy.c`) depend on the physical state of TROPIC01 chip slots (ECC keys, R-Memory, pairing keys, monotonic counters). Tests assume slot states and perform cleanup, but if a previous test run didn't complete cleanup properly or if tests run in different orders, the starting state may vary.

## Impact
- **Non-hermetic tests**: Tests depend on external hardware state that may persist between runs
- **Cleanup failures**: If cleanup fails partway through, subsequent tests may start with unexpected state
- **Different hardware**: Tests may behave differently on different TROPIC01 chips with different initial states
- **Hard to reproduce**: A test that passes on one chip may fail on another due to different slot states

## Evidence

### Tests assume clean slot state
File: `lt_test_rev_ecc_key_generate.c:98-104`
```c
LT_LOG_INFO("Testing ECC_Key_Generate using P256 curve...");
for (uint8_t i = TR01_ECC_SLOT_0; i <= TR01_ECC_SLOT_31; i++) {
    LT_LOG_INFO();
    LT_LOG_INFO("Testing ECC key slot #%" PRIu8 "...", i);

    LT_LOG_INFO("Checking if slot is empty...");
    LT_TEST_ASSERT(LT_L3_INVALID_KEY, lt_ecc_key_read(h, i, read_pub_key, sizeof(read_pub_key), &curve, &origin));
```

This assumes all 32 ECC slots are empty at the start.

### Tests rely on cleanup functions
File: `lt_test_rev_r_mem.c:38-77` - cleanup function erases all slots:
```c
static lt_ret_t lt_test_rev_r_mem_cleanup(void)
{
    // ...
    LT_LOG_INFO("Erasing all slots...");
    for (uint16_t i = 0; i <= TR01_R_MEM_DATA_SLOT_MAX; i++) {
        // ...
        ret = lt_r_mem_data_erase(g_h, i);
        // ...
    }
    // ...
}
```

### Tests with irreversible operations
From `README.md:5`:
```
**Warning: Some functions will result into irreversible changes in TROPIC01**
```

Tests like `lt_test_rev_mac_and_destroy.c` use `MAC_And_Destroy` which is inherently irreversible until slots are restored.

### Multiple slot types affected
- ECC key slots (0-31): `lt_test_rev_ecc_key_generate.c`, `lt_test_rev_ecdsa_sign.c`, `lt_test_rev_eddsa_sign.c`
- R-Memory data slots: `lt_test_rev_r_mem.c`, `lt_test_rev_erase_r_config.c`, `lt_test_rev_write_r_config.c`
- Pairing key slots: `lt_test_ire_pairing_key_slots.c`
- Monotonic counters (0-15): `lt_test_rev_mcounter.c`
- I-Config and R-Config: `lt_test_rev_read_i_config.c`, `lt_test_ire_write_i_config.c`

## Recommended Fix

### 1. Add explicit state verification at test start
Before running tests, verify the expected initial state:

```c
void lt_test_rev_ecc_key_generate(lt_handle_t *h)
{
    LT_LOG_INFO("Verifying initial slot state...");
    for (uint8_t i = TR01_ECC_SLOT_0; i <= TR01_ECC_SLOT_31; i++) {
        LT_TEST_ASSERT(LT_L3_INVALID_KEY, 
            lt_ecc_key_read(h, i, read_pub_key, sizeof(read_pub_key), &curve, &origin));
    }
    
    // ... rest of test
}
```

### 2. Add pre-test state reset option
Create a setup function that resets all relevant slots to a known state:

```c
static lt_ret_t lt_test_reset_all_slots(lt_handle_t *h)
{
    // Reset ECC slots
    for (uint8_t i = TR01_ECC_SLOT_0; i <= TR01_ECC_SLOT_31; i++) {
        lt_ecc_key_erase(h, i);
    }
    // Reset R-Memory slots
    for (uint16_t i = 0; i <= TR01_R_MEM_DATA_SLOT_MAX; i++) {
        lt_r_mem_data_erase(h, i);
    }
    // ... other slots
    return LT_OK;
}
```

### 3. Document hardware prerequisites
Add clear documentation about required initial hardware state for each test.

### 4. Add test setup/teardown hooks
Implement a test framework with explicit setup and teardown phases:

```c
typedef struct {
    void (*setup)(lt_handle_t *h);
    void (*test)(lt_handle_t *h);
    void (*teardown)(lt_handle_t *h);
} test_spec_t;
```

## References
- [Hardware test isolation](https://www.nginyc.com/blog/test-isolation/)
- [Stateful testing patterns](https://www.martinfowler.com/bliki/StateDrivenTesting.html)
