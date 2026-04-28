---
title: "[MEDIUM] FIDO2 CTAPHID Protocol Handler Lacks Unit Test Coverage"
severity: MEDIUM
domain: Testing
lens: unit-test-gaps
labels:
  - "audit:testing/unit-test-gaps"
---

## Summary
The FIDO2 module's CTAPHID protocol layer (`components/mod_fido2/src/ctaphid.cpp`) handles USB HID communication with no unit tests. Critical untested functions include:

- `ctaphid_init()` - HID initialization
- `ctaphid_send()` - Send response to host
- `ctaphid_recv()` - Receive command from host
- `ctaphid_process()` - Process HID packet
- `ctaphid_keepalive()` - Send keepalive during long operations
- Fragmentation/reassembly handling for large packets

## Impact
**FIDO2 Protocol Risk:** CTAPHID is the transport layer for FIDO2:
1. Packet fragmentation (64-byte HID blocks) is untested
2. Command ID matching for responses is unproven
3. Sequence number handling for multi-packet messages is untested
4. Timeout handling for long operations is unverified
5. Keepalive messages during `CTAPHID_MAKEDC` are untested

## Evidence
File: `components/mod_fido2/src/ctaphid.cpp` (estimated 150-200 lines)

Typical CTAPHID operations:
- 64-byte HID report format
- Header: CID (4) + CMD (1) + BCNT (2) + DATA (32-57)
- Continuation: CID (4) + SEQ (1) + DATA (59)
- Commands: `CTAPHID_PING`, `CTAPHID_MAKEDC`, `CTAPHID_GETINFO`, etc.

Current test coverage:
```bash
$ find test/ -name "*.cpp" -exec grep -l -i "ctaphid\|fido2" {} \;
# Returns nothing - no FIDO2/CTAPHID tests exist
```

## Recommended Fix
Create `test/test_ctaphid/test_ctaphid.cpp` with test cases:

1. **Packet tests:**
   - Test `ctaphid_send()` formats correct HID report
   - Test `ctaphid_recv()` parses HID report header
   - Test sequence number increment

2. **Fragmentation tests:**
   - Test large message (>57 bytes) splits into multiple packets
   - Test reassembly of continuation packets
   - Test sequence number validation

3. **Command tests:**
   - Test `CTAPHID_PING` echoes data
   - Test `CTAPHID_GETINFO` returns device info
   - Test unknown command returns `CTAPHID_ERR_INVALID_CMD`

4. **Error tests:**
   - Test invalid CID returns error
   - Test bad sequence number returns error
   - Test timeout handling

Example test:
```cpp
void test_ctaphid_send_ping() {
    uint8_t data[] = "Hello";
    ctaphid_send(CID_BROADCAST, CTAPHID_PING, data, sizeof(data));
    
    // Verify HID report format
    uint8_t* report = getHidReport();
    TEST_ASSERT_EQUAL(0, report[0]);  // CID
    TEST_ASSERT_EQUAL(0, report[1]);
    TEST_ASSERT_EQUAL(0, report[2]);
    TEST_ASSERT_EQUAL(0, report[3]);
    TEST_ASSERT_EQUAL(0x81, report[4]);  // CMD (PING with init bit)
    TEST_ASSERT_EQUAL(0x05, report[5]);  // BCNT high
    TEST_ASSERT_EQUAL(0x00, report[6]);  // BCNT low
    TEST_ASSERT_EQUAL_MEMORY("Hello", &report[7], 5);
}

void test_ctaphid_recv_fragmented() {
    // Simulate 120-byte message (2 continuation packets)
    uint8_t packet1[] = {0,0,0,0, 0x82, 0x00, 0x78, ...};  // Header
    uint8_t packet2[] = {0,0,0,0, 0x00, ...};  // Continuation
    uint8_t packet3[] = {0,0,0,0, 0x01, ...};  // Continuation
    
    ctaphid_recv(packet1);
    ctaphid_recv(packet2);
    ctaphid_recv(packet3);
    
    uint8_t* data = getReceivedData();
    TEST_ASSERT_EQUAL(120, getReceivedLength());
}

void test_ctaphid_invalid_cid() {
    uint8_t packet[] = {0x12, 0x34, 0x56, 0x78, 0x81, ...};  // Unknown CID
    
    ctaphid_recv(packet);
    
    uint8_t* response = getHidReport();
    TEST_ASSERT_EQUAL(0x80, response[4]);  // ERR response
    TEST_ASSERT_EQUAL(0x0B, response[5]);  // INVALID_CID
}
```

## References
- FIDO 2.1 Spec: CTAPHID Protocol
- File: `components/mod_fido2/include/mod_fido2/ctaphid.h` - Protocol API
- File: `components/mod_fido2/include/mod_fido2/ctap2.h` - CTAP2 commands
