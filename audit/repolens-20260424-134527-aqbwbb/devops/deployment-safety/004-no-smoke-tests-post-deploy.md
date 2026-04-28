---
title: "[HIGH] No Post-Deployment Smoke Tests"
severity: HIGH
domain: deployment-safety
lens: deployment-safety
labels:
  - "audit:devops/deployment-safety"
---

## Summary
The deployment pipeline marks success based solely on the build completing and artifacts being uploaded. There are no automated smoke tests to verify the firmware actually boots and functions correctly after deployment.

**Files:**
- `.github/workflows/build.yml` (lines 1-90)
- `.github/workflows/deploy-pages.yml` (lines 1-139)

## Impact
- **Silent failures**: Broken firmware can be released without detection
- **User impact**: Users flash broken firmware and discover issues manually
- **No validation**: Build success doesn't guarantee runtime correctness
- **Slower feedback**: Issues are discovered by users rather than CI

## Evidence
The build workflow has no verification step after firmware compilation:
```yaml
# build.yml:34-65
- name: Build firmware
  run: pio run

- name: Get version info
  id: version
  run: |
    # Version extraction only, no functional test

- name: Upload firmware artifacts
  uses: actions/upload-artifact@v4
  with:
    name: cdc-badge-firmware-${{ steps.version.outputs.version }}
    path: artifacts/
```

The deploy-pages workflow similarly has no validation:
```yaml
# deploy-pages.yml:67-139
- name: Build Doxygen documentation
  run: doxygen Doxygenfile

- name: Assemble site
  # Downloads firmware but doesn't verify it

- name: Deploy to GitHub Pages
  uses: actions/deploy-pages@v4
  # Marks success without firmware validation
```

## Recommended Fix
Add smoke tests to verify:
1. **Binary integrity**: Check firmware.bin size is within expected range (not truncated/corrupted)
2. **Partition table validation**: Verify partitions.bin has correct structure
3. **Basic ELF check**: If using ELF, verify symbols load correctly

Example implementation:
```yaml
- name: Validate firmware binaries
  run: |
    # Check firmware size (should be > 100KB, < 1MB for ESP32-S3)
    SIZE=$(stat -c%s .pio/build/cdc_badge_usb/firmware.bin)
    if [ $SIZE -lt 102400 ] || [ $SIZE -gt 1048576 ]; then
      echo "ERROR: Firmware size out of range: $SIZE bytes"
      exit 1
    fi
    
    # Verify bootloader is valid ESP32 image
    python -c "import esptool; esptool.main(['info', '.pio/build/cdc_badge_usb/bootloader.bin'])"
```

For hardware-in-the-loop testing, consider:
- ESP32 test fixture with automated flashing and serial output capture
- Check for boot messages in serial output to verify firmware boots

## References
- ESP32 flash validation: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/flash-update.html
- esptool verification: https://github.com/espressif/esptool
