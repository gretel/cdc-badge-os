---
title: "[MEDIUM] O(n²) nested loop in WiFi scan result deduplication"
severity: MEDIUM
domain: cdc_os_ui
lens: algorithm-efficiency
labels:
  - "audit:performance/algorithm"
---

## Summary
In `components/cdc_os_ui/src/WifiMenuUi.cpp` (lines 383-405), the WiFi scan result deduplication uses a nested loop approach, resulting in O(n²) time complexity. For each newly discovered network, the code performs a linear search through the existing `s_wifiScanResults` array to check for duplicates.

**Evidence:**
```cpp
for (uint8_t i = 0; i < rawCount && s_wifiScanCount < WIFI_MAX_NETWORKS; i++) {
    bool found = false;
    for (uint8_t j = 0; j < s_wifiScanCount; j++) {  // O(n) inner loop
        if (strcmp(s_wifiScanResults[j].ssid, rawResults[i].ssid) == 0) {
            // ...
        }
    }
}
```

## Impact
- **Performance**: With `WIFI_MAX_NETWORKS` potentially up to 30-50 networks in dense environments, the algorithm performs up to 1,225-2,500 comparisons per scan.
- **Scalability**: As the number of available networks grows, the time spent deduplicating increases quadratically.
- **User Experience**: WiFi scanning already takes time; unnecessary computation adds to the delay.

## Evidence
**File**: `components/cdc_os_ui/src/WifiMenuUi.cpp`
**Lines**: 383-405
**Context**: The outer loop iterates `rawCount` times, and for each iteration, the inner loop scans up to `s_wifiScanCount` entries (growing from 0 to n).

## Recommended Fix
Use a hash-based lookup for O(1) average-case duplicate detection. Since SSIDs are short strings (max 32 chars), a simple trie or fixed-size hash table would be efficient:

```cpp
// Option 1: Simple hash table for SSID lookup
static uint8_t ssidHash[256];  // First byte of SSID as index
// Before inner loop: compute hash of rawResults[i].ssid[0]
// Check only entries with matching hash first

// Option 2: Reorder logic to avoid nested loop entirely
// Sort by SSID first, then deduplicate in a single pass (O(n log n))
```

Or, if the number of networks is typically small (<10), document this as a known limitation and add a comment explaining the tradeoff.

## References
- [Big O notation - Wikipedia](https://en.wikipedia.org/wiki/Big_O_notation)
- [Hash tables for efficient lookups](https://en.wikipedia.org/wiki/Hash_table)
