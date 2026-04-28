---
title: "[LOW] ExpertMenuUi and hardware info integration lacks tests"
severity: LOW
domain: ui
lens: integration-test-gaps
labels:
  - "audit:testing/integration-test-gaps"
  - "component:cdc_os_ui"
  - "area:expert-menu"
---

## Summary
The `ExpertMenuUi` and `HardwareInfo` components (`components/cdc_os_ui/src/ExpertMenuUi.cpp`, `HardwareInfo.cpp`) handle expert features and hardware information display, but **no integration tests** verify that hardware info is read correctly.

## Impact
- **Hardware info**: Chip info may not be read correctly
- **Expert features**: Expert menu items may not work
- **Display**: Hardware info may not display correctly

## Evidence

**ExpertMenuUi API** (`components/cdc_os_ui/src/ExpertMenuUi.cpp`):
```cpp
void showExpertMenu();
void onExpertFeature1();
void onExpertFeature2();
```

**HardwareInfo API** (`components/cdc_os_ui/src/HardwareInfo.cpp`):
```cpp
std::string getChipName();
std::string getChipRevision();
std::string getFlashSize();
std::string getPsramSize();
std::string getUuid();
```

**Hardware reading** (`components/cdc_os_ui/src/HardwareInfo.cpp:20-80`):
```cpp
std::string getChipName() {
    return "ESP32-S3";
}

std::string getChipRevision() {
    esp_chip_info_t info;
    esp_chip_info(&info);
    return std::to_string(info.revision);
}

std::string getUuid() {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    // Format as UUID
}
```

**Current test coverage**: None

## Recommended Fix

Create integration test `test_hardware_info/` that verifies:

1. **Chip info**: Chip name and revision read correctly
2. **Memory**: Flash and PSRAM sizes correct
3. **UUID**: UUID generated correctly from MAC

**Test structure** (example):
```cpp
// test/test_hardware_info/test_hardware.cpp
#include "cdc_os_ui/HardwareInfo.h"

void test_chip_info() {
    std::string name = getChipName();
    std::string revision = getChipRevision();
    
    ASSERT_EQ(name, "ESP32-S3");
    ASSERT_FALSE(revision.empty());
}

void test_memory_info() {
    std::string flash = getFlashSize();
    std::string psram = getPsramSize();
    
    ASSERT_FALSE(flash.empty());
    ASSERT_FALSE(psram.empty());
}

void test_uuid() {
    std::string uuid = getUuid();
    
    // UUID should be 36 chars (8-4-4-4-12 + hyphens)
    ASSERT_EQ(uuid.length(), 36);
}
```

## References
- [ExpertMenuUi implementation](components/cdc_os_ui/src/ExpertMenuUi.cpp)
- [HardwareInfo](components/cdc_os_ui/src/HardwareInfo.cpp)

</content>