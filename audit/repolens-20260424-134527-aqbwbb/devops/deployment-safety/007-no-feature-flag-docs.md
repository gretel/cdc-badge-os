---
title: "[LOW] No Feature Flag Documentation for Runtime Configuration"
severity: LOW
domain: deployment-safety
lens: deployment-safety
labels:
  - "audit:devops/deployment-safety"
---

## Summary

Feature flags exist in `components/cdc_core/include/cdc_core/feature_flags.h` but there is no documentation for their runtime behavior or deployment implications. Specifically:

1. `DEBUG_MODE` - Disables security lockouts but no clear migration path
2. `FEATURE_SECURE_SERIAL` - No documentation on security implications
3. `FEATURE_NVS_EDIT` - No documentation on data migration

**Evidence:**
- Feature flags defined in `components/cdc_core/include/cdc_core/feature_flags.h`
- README mentions flags but no deployment guidance
- No feature flag lifecycle management documented

## Impact

**Security:**
- Developers may forget to disable `DEBUG_MODE` in production
- No clear checklist for feature flag audit before release

**Operations:**
- Hard to track which features are enabled in production
- No rollback mechanism for feature flags (requires rebuild)

## Evidence

**README.md** (lines 172-180):
```
### Compile-Time Flags

Feature flags in `components/cdc_core/include/cdc_core/feature_flags.h`:

| Flag | Default | Description |
|------|---------|-------------|
| `DEBUG_MODE` | 1 | Disables PIN lockouts and increases log verbosity. **Set to 0 for production!** |
```

**No documentation on:**
- How to audit feature flags before release
- What flags are safe for production
- Migration path for feature flag changes

## Recommended Fix

Add feature flag documentation to `docs/DEPLOYMENT.md` or create `docs/FEATURE_FLAGS.md`:

1. **Feature flag inventory:**
   ```markdown
   ## Feature Flags
   
   ### DEBUG_MODE
   - **Production Safe:** No
   - **Default:** 1 (development)
   - **Production Default:** 0
   - **Impact:** Disables PIN lockouts
   
   ### FEATURE_SECURE_SERIAL
   - **Production Safe:** Yes
   - **Default:** 0
   - **Impact:** Requires PIN for serial commands
   ```

2. **Pre-release checklist:**
   ```markdown
   ## Feature Flag Audit
   
   Before release, verify:
   - [ ] DEBUG_MODE = 0
   - [ ] FEATURE_SECURE_SERIAL = 1 (optional)
   - [ ] All experimental flags documented
   ```

**Estimated effort:** 30 minutes

## References

- [Feature Flags Best Practices](https://featureflags.io/)
- Existing flags: `components/cdc_core/include/cdc_core/feature_flags.h`

</content>