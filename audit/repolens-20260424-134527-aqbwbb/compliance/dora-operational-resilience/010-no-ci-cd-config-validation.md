---
title: "[MEDIUM] No CI/CD Configuration Validation for Production Builds"
severity: MEDIUM
domain: ICT Risk Management
lens: dora-ci-cd-configuration
labels:
  - "audit:compliance/dora-operational-resilience"
---

## Summary
The GitHub Actions workflow in `.github/workflows/build.yml` builds firmware but does not validate production configuration settings. Specifically:
- No check that `DEBUG_MODE=0` for release builds
- No verification of security-critical feature flags
- No validation that required security features are enabled

Key files:
- `.github/workflows/build.yml` - Build workflow (lines 40-44: Build firmware step)
- `platformio.ini` - Build configuration
- `components/cdc_core/include/cdc_core/feature_flags.h` - Feature flags

## Impact
For financial entities:
- Releases may be built with DEBUG_MODE=1 if developer forgets to configure properly
- No automated guardrails to prevent insecure configurations in production
- Harder to maintain consistent security posture across releases
- Manual verification required for every release

From `.github/workflows/build.yml`:
```yaml
- name: Build firmware
  run: pio run
```

No validation step after build to check configuration.

## Evidence
From `.github/workflows/build.yml` (lines 38-44):
```yaml
- name: Build firmware
  run: pio run
```

The build step:
- Does not check `DEBUG_MODE` setting
- Does not validate `FEATURE_SECURE_SERIAL` is enabled
- Does not verify any security-critical flags

From `platformio.ini` (lines 18-29):
```ini
build_flags =
    -std=gnu++17
    -D BUILD_TIME=__TIME__
    -D BUILD_DATE=__DATE__
    -D APP_NAME=\"CDCBos\"
    -D APP_VERSION=\"0.5.0\"
    ; TinyUSB configuration
    -D CONFIG_TINYUSB_ENABLED=1
    -D CONFIG_TINYUSB_CDC_ENABLED=1
    -D CONFIG_TINYUSB_CDC_RX_BUFSIZE=256
    -D CONFIG_TINYUSB_CDC_TX_BUFSIZE=256
    ; libtropic helpers
    -DLT_HELPERS
```

No explicit `DEBUG_MODE` flag set - relies on default from `feature_flags.h` (which is `1`).

From `sdkconfig.defaults` - No `DEBUG_MODE` override (checked all 78 lines).

## Recommended Fix
Add CI/CD validation steps:

1. **Add configuration validation step in `.github/workflows/build.yml`**:
   ```yaml
   - name: Validate production configuration
     run: |
       # Check DEBUG_MODE is disabled
       grep -q "DEBUG_MODE=0" sdkconfig.defaults || \
         (echo "ERROR: DEBUG_MODE not set to 0 in sdkconfig.defaults" && exit 1)
       
       # Check FEATURE_SECURE_SERIAL is enabled
       grep -q "CONFIG_SECURE_SERIAL=y" sdkconfig.defaults || \
         (echo "ERROR: Secure Serial not enabled" && exit 1)
   ```

2. **Add build matrix for validation**:
   ```yaml
   - name: Build with production flags
     run: |
       pio run -D "build_flags=-DDEBUG_MODE=0"
   ```

3. **Add pre-release checklist** (can be a GitHub Action):
   ```yaml
   - name: Security configuration check
     if: startsWith(github.ref, 'refs/tags/v')
     run: |
       echo "Checking production release configuration..."
       # Verify all security flags
       pio run -t clean
       pio run -D "build_flags=-DDEBUG_MODE=0 -DFEATURE_SECURE_SERIAL=1"
   ```

4. **Create a validation script** (`tools/validate_config.sh`):
   ```bash
   #!/bin/bash
   set -e
   
   echo "Validating production configuration..."
   
   # Check feature_flags.h default
   if grep -q "#define DEBUG_MODE 1" components/cdc_core/include/cdc_core/feature_flags.h; then
       echo "WARNING: DEBUG_MODE defaults to 1"
   fi
   
   # Check sdkconfig.defaults
   if ! grep -q "DEBUG_MODE=0" sdkconfig.defaults; then
       echo "ERROR: DEBUG_MODE not explicitly set to 0"
       exit 1
   fi
   
   echo "Configuration validation passed"
   ```

5. **Add to build workflow**:
   ```yaml
   - name: Validate configuration
     run: bash tools/validate_config.sh
   ```

## References
- DORA Regulation (EU) 2022/2554, Article 6(1)(e) - ICT configuration management
- DORA Regulation (EU) 2022/2554, Article 10 - ICT-related incident management (CI/CD as part of detection)
- NIST SP 800-115 - Technical Guide to Information Security Testing and Classification
- OWASP CI/CD Security Cheat Sheet
