---
title: "[MEDIUM] Orphaned source file: SecureElementStub.cpp not in build"
severity: MEDIUM
domain: dead-code
lens: dead-code
labels:
  - "audit:code-quality/dead-code"
---

## Summary
The source file `SecureElementStub.cpp` exists in the cdc_hal component's src directory but is **not included** in the CMakeLists.txt build configuration. This means the file is compiled but the resulting object file contains code that is never linked into the final binary.

**Location:** `/input/20260423-132359-oj8ayc/cdc-badge-os/components/cdc_hal/src/SecureElementStub.cpp`

The file contains a stub implementation of the `ISecureElement` interface for the TROPIC01 secure element.

## Impact
- **Build time**: File is still parsed by the preprocessor but not linked
- **Repository clutter**: Dead source file adds noise
- **Developer confusion**: Developers may wonder if this is an alternative implementation or work-in-progress

## Evidence
**File exists in src directory:**
```
$ ls components/cdc_hal/src/*.cpp
BQ25895Power.cpp
BleAdvParser.cpp
BluetoothController.cpp
EpaperDisplay.cpp
...
SecureElementStub.cpp    <-- Present
...
```

**But NOT in CMakeLists.txt:**
```
$ cat components/cdc_hal/CMakeLists.txt | grep -c "SecureElementStub"
0  <-- Not found
```

CMakeLists.txt SRCS section (line 5-18):
```cmake
SRCS
    "src/EpaperDisplay.cpp"
    "src/SpiBus.cpp"
    "src/I2cBus.cpp"
    "src/TCA9535Keypad.cpp"
    "src/EspHardware.cpp"
    "src/Rtc.cpp"
    "src/SleepController.cpp"
    "src/Tropic01Element.cpp"
    "src/BQ25895Power.cpp"
    "src/libtropic_port_esp32.cpp"
    "src/WifiController.cpp"
    "src/BluetoothController.cpp"
    "src/BleAdvParser.cpp"
```

Note: `SecureElementStub.cpp` is missing from this list.

## Recommended Fix
Choose one of the following:

**Option 1: If the stub is needed**
Add to CMakeLists.txt:
```cmake
SRCS
    ...
    "src/SecureElementStub.cpp"
    ...
```

**Option 2: If the stub is obsolete**
Delete the file:
```bash
rm components/cdc_hal/src/SecureElementStub.cpp
rm components/cdc_hal/src/._SecureElementStub.cpp  # Mac resource file
```

**Option 3: If keeping as reference**
Move to a `docs/` or `references/` folder with documentation on why it was disabled.

## References
- CMake documentation: https://cmake.org/cmake/help/latest/command/idf_component_register.html
- Dead code detection: https://docs.codeclimate.com/docs/tripling-your-codereview-speed#dead-code
