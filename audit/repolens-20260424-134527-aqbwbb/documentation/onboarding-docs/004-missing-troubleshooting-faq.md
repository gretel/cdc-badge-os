---
title: "[LOW] Missing troubleshooting/FAQ section"
severity: LOW
domain: developer-onboarding
lens: onboarding-docs
labels:
  - "audit:documentation/onboarding-docs"
---

## Summary
The repository lacks a troubleshooting guide or FAQ section that addresses:
- Common setup issues (PlatformIO, ESP-IDF)
- Device detection problems
- Serial monitor connection issues
- Flashing failures
- Build errors
- Known limitations and workarounds

**Evidence:**
- No `TROUBLESHOOTING.md` or `FAQ.md` file
- No troubleshooting section in `README.md` or `docs/README.md`
- `README.md` mentions "Early Alpha" but doesn't list known issues

## Impact
New developers:
- Struggle with common setup problems without guidance
- May give up if they encounter basic issues
- Duplicate questions that could be answered in FAQ
- Search online for problems that have simple fixes

## Evidence
1. No troubleshooting documentation exists:
```bash
$ find . -name "*trouble*" -o -name "*faq*" -o -name "*known*" 2>/dev/null
./third_party/libtropic/docs/faq.md  # Only in third-party lib
```

2. Common issues that should be documented:
   - Device not detected when flashing (BOOT+RESET combination)
   - PlatformIO path issues (`~/.platformio/penv/bin/pio`)
   - Serial monitor not showing output (baud rate, USB CDC)
   - "DEBUG_MODE" confusion (why PIN lockouts don't work)
   - German umlauts display limitation (ae, oe, ue)

3. `README.md:142-143` mentions BOOT+RESET but only in passing:
   ```
   If the device is not detected, hold BOOT while pressing RESET to enter download mode.
   ```

## Recommended Fix
Create `docs/TROUBLESHOOTING.md` with:

1. **Setup Issues**:
   - PlatformIO not in PATH (use full path)
   - Submodule initialization (`git submodule update --init --recursive`)
   - Python version requirements (3.11)

2. **Device Detection**:
   - Device not found when flashing
   - Serial port not appearing
   - BOOT+RESET sequence

3. **Build Errors**:
   - Missing dependencies
   - CMake configuration issues
   - Memory allocation errors

4. **Serial Issues**:
   - No output in serial monitor
   - Wrong baud rate
   - USB CDC vs. UART

5. **Known Limitations**:
   - German umlauts display (use ae, oe, ue)
   - DEBUG_MODE disables security
   - Feature flags must be set at build time

6. **FAQ**:
   - "How do I change the default PIN?"
   - "Why doesn't FIDO2 work?"
   - "What's the difference between U2F and FIDO2?"

**Estimated effort:** ~30-45 minutes to compile known issues.

## References
- [ESP-IDF Troubleshooting](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/troubleshooting.html)
