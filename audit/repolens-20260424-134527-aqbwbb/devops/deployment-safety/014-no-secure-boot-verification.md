---
title: "[MEDIUM] No Secure Boot Verification in CI/CD Pipeline"
severity: MEDIUM
domain: deployment-safety
lens: deployment-safety
labels:
  - "audit:devops/deployment-safety"
---

## Summary
The CI/CD pipeline builds firmware but does not verify that secure boot is enabled or that the firmware can be signed. ESP32-S3 supports secure boot v2, but the build workflow doesn't enforce or document this security feature.

**Location:** `.github/workflows/build.yml`

## Impact
- **Unverified security**: Firmware may be flashed without secure boot enabled
- **Tampering risk**: Device can boot any firmware, not just signed releases
- **Production risk**: Security-conscious users may not know how to enable secure boot
- **No signing workflow**: No automated process to sign firmware for secure boot

## Evidence
Build workflow only checks compilation succeeds:

```yaml
- name: Build firmware
  run: pio run

- name: Build firmware
  run: pio run -t upload
```

No verification of security configuration:
- No check for `CONFIG_SECURE_BOOT=y`
- No signing key management
- No secure boot v2 verification

SDK config has secure boot options but not enforced:
```bash
CONFIG_SECURE_BOOT=y  # May or may not be set
CONFIG_SECURE_SIGNED_APPS=y  # May or may not be set
```

## Recommended Fix
1. **Add secure boot build variant**:
```ini
[env:cdc_badge_usb_secure]
extends: env:cdc_badge_usb
build_flags =
    ${env:cdc_badge_usb.build_flags}
    -D CONFIG_SECURE_BOOT=1
    -D CONFIG_SECURE_SIGNED_APPS=1
```

2. **Add signing step to CI**:
```yaml
- name: Sign firmware
  run: |
    esptool.py --chip esp32s3 sign_bin \
      --keyfile ${{ secrets.SECURE_BOOT_KEY }} \
      --address 0x10000 \
      .pio/build/cdc_badge_usb/firmware.bin
```

3. **Add verification step**:
```yaml
- name: Verify secure boot config
  run: |
    grep "CONFIG_SECURE_BOOT=y" .pio/build/cdc_badge_usb/sdkconfig
    grep "CONFIG_SECURE_SIGNED_APPS=y" .pio/build/cdc_badge_usb/sdkconfig
```

4. **Document secure boot setup** in README:
```markdown
## Secure Boot Setup
1. Generate secure boot key: `esptool.py --chip esp32s3 securebootv2 generate_key ...`
2. Enable in SDK config: `menuconfig → Security features`
3. Sign firmware before flashing
```

## References
- [ESP32-S3 Secure Boot](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/secure-boot.html)
- [ESP-IDF Security Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/security.html)
- [esptool.py sign command](https://github.com/espressif/esptool/blob/master/esptool.py#L3000)
