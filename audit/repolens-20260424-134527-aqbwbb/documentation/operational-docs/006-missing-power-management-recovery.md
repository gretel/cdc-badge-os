---
title: "[LOW] Missing Power Management and Recovery Procedures"
severity: LOW
domain: operational-docs
lens: documentation/operational-docs
labels:
  - "audit:documentation/operational-docs"
---

## Summary
The power management system has 4 states (Active, Light Sleep, Deep Sleep, Shipping), but there is no documentation on how to recover from power-related issues (device not waking from sleep, battery drain, shipping mode activation).

**Where it should be:** Section in `TROUBLESHOOTING.md` or `README.md`

**Current state:**
- `README.md:231-237` - Power states table (Active, Light Sleep, Deep Sleep, Shipping)
- `docs/UI_FLOWS.md:334-342` - Power states described with triggers
- No recovery procedures for stuck sleep states
- No battery drain troubleshooting
- No procedure to wake from shipping mode

## Impact
**User Impact:**
- Device appears "dead" when in deep sleep or shipping mode
- No clear recovery path for users
- Battery drain issues go undiagnosed
- New users may accidentally enter shipping mode

**Evidence:**
- `README.md:231-237` - Power states documented but no recovery
- `docs/UI_FLOWS.md:334-342` - Wake procedures mentioned but incomplete
- `docs/UI_FLOWS.md:339` - "Deep Sleep: Hold N 5s on lock" - but no recovery if stuck
- No documentation on what to do if device doesn't wake

## Recommended Fix
Create power management recovery section:

1. **Device Not Waking from Sleep**
   - Symptom: Pressing Y key, no response
   - Recovery: Connect USB (device wakes automatically)
   - If still dead: Hold RESET button

2. **Device in Deep Sleep**
   - Symptom: No USB enumeration, display off
   - Recovery: Press Y key (deep sleep wake), or connect USB

3. **Shipping Mode Recovery**
   - Symptom: Device completely unresponsive, no USB
   - Cause: Accidental BOOT hold 3s
   - Recovery: Connect USB (power alone wakes from shipping mode)

4. **Battery Drain Issues**
   - Symptom: Battery depleted in hours/days instead of weeks
   - Diagnosis: Check `STATUS` for wake count, check sleep mode
   - Recovery: Force deep sleep (hold N 5s), check for wake loops

5. **Power States Reference**
   | State | Wake Method | Display |
   |-------|-------------|---------|
   | Light Sleep | Any key (except 3) | Off |
   | Deep Sleep | Y key only | Off |
   | Shipping | USB power | Off |

## References
- `README.md:231-237` - Power states overview
- `docs/UI_FLOWS.md:334-342` - Power state details

---
**Related Issues:**
- Missing troubleshooting guide (#001)
