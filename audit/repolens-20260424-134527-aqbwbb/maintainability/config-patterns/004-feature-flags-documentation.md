---
title: "[LOW] Feature flags not documented or validated"
severity: LOW
domain: maintainability/config-patterns
lens: config-patterns
labels:
  - "audit:maintainability/config-patterns"
---

## Summary
Feature flags in `feature_flags.h` are not well-documented or validated. There's no clear list of what flags exist, what they control, or how to set them. The `DEBUG_MODE` flag defaults to `1` without explanation.

## Impact
1. **Developer confusion**: New developers don't know what feature flags exist
2. **Security risk**: `DEBUG_MODE=1` by default may expose debug features in production
3. **Build consistency**: No validation that feature flags are set correctly for the target environment
4. **Maintenance**: Hard to understand what code paths are enabled/disabled

## Evidence
**Current feature flags (`components/cdc_core/include/cdc_core/feature_flags.h`):**
```cpp
// Only 3 flags defined:
#define FEATURE_SECURE_SERIAL 1  // From Kconfig
#define FEATURE_NVS_EDIT 0       // Default 0
#define DEBUG_MODE 1             // Default 1 (no explanation!)
```

**Missing documentation:**
- No description of what `DEBUG_MODE` controls
- No list of all features that can be toggled
- No guidance on recommended settings for production vs development
- No validation that flags are consistent

**Usage in code:**
```cpp
// components/mod_nvsedit/src/NvsEditModule.cpp
static bool isFeatureEnabled() {
    return FEATURE_NVS_EDIT != 0;
}

// components/cdc_core/include/cdc_core/feature_flags.h
// DEBUG_MODE used but never documented what it controls
```

## Recommended Fix
1. **Document all feature flags**:
   ```cpp
   /**
    * Feature Flags - Compile-time feature toggles
    * 
    * ### How to use
    * Set flags in platformio.ini build_flags or sdkconfig.defaults:
    * ```
    * build_flags =
    *     -DDEBUG_MODE=0
    *     -DFEATURE_NVS_EDIT=1
    * ```
    * 
    * ### Available flags
    * | Flag | Default | Description |
    * |------|---------|-------------|
    * | FEATURE_SECURE_SERIAL | 1 | Require PIN for serial commands |
    * | FEATURE_NVS_EDIT | 0 | Enable NVS editor (privileged) |
    * | DEBUG_MODE | 1 | Disable security lockouts for development |
    * 
    * ### Production settings
    * For production, set:
    * - `DEBUG_MODE=0` (enable all lockouts)
    * - `FEATURE_NVS_EDIT=0` (disable NVS editor)
    */
   ```

2. **Add validation**:
   ```cpp
   // components/cdc_core/src/FeatureFlags.cpp
   #include "cdc_core/feature_flags.h"
   #include "cdc_log.h"
   
   void validateFeatureFlags() {
       if (DEBUG_MODE) {
           LOG_W("FeatureFlags", "DEBUG_MODE is enabled - security lockouts disabled!");
           LOG_W("FeatureFlags", "Set DEBUG_MODE=0 for production build");
       }
       
       if (FEATURE_NVS_EDIT) {
           LOG_W("FeatureFlags", "NVS Editor enabled - destructive actions available");
       }
   }
   ```

3. **Create `.env.example` or config documentation**:
   ```bash
   # Build flags for CDC Badge OS
   # Copy to .env and adjust for your environment
   
   # Development
   DEBUG_MODE=1
   FEATURE_NVS_EDIT=1
   
   # Production
   # DEBUG_MODE=0
   # FEATURE_NVS_EDIT=0
   ```

## References
- [Feature Flag Best Practices](https://launchdarkly.com/blog/feature-flag-best-practices/)
- [Configuration Management](https://12factor.net/config)
