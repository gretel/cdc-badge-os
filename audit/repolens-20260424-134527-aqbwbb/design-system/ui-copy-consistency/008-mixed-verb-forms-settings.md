---
title: "[LOW] Mixed verb forms in settings menu labels"
severity: LOW
domain: design-system/ui-copy-consistency
lens: ui-copy-consistency
labels:
  - "microcopy"
  - "settings"
  - "i18n"
---

## Summary
The settings menu uses inconsistent verb forms in labels:

1. **Noun phrases (most common):**
   - "Brightness"
   - "Language"
   - "Timezone"
   - "Sleep Interval"
   - "Badge Text"

2. **Verb phrases (action-oriented):**
   - "Set Date"
   - "Set Time"
   - "Change PIN"
   - "Sync Time"

3. **Mixed patterns within same category:**
   - "Set Date" / "Set Time" (verb phrases)
   - "Daylight Saving" (noun phrase, but implies action)
   - "Auto Sleep" (noun phrase)
   - "Sleep Interval" (noun phrase)

## Impact
- Minor inconsistency in how settings are described
- Some settings describe what they are (noun), others what you do (verb)
- Doesn't significantly impact usability but reduces polish
- New developers won't have a clear pattern to follow

## Evidence
**File: `components/cdc_ui/src/I18n.cpp` lines 257-267**
```cpp
REG(BRIGHTNESS,     "Brightness",       "Helligkeit");
REG(LANGUAGE,       "Language",         "Sprache");
REG(TIMEZONE,       "Timezone",         "Zeitzone");
REG(SUMMER_TIME,    "Daylight Saving",  "Sommerzeit");
REG(BADGE_TEXT,     "Badge Text",       "Badge-Text");
REG(AUTO_SLEEP,     "Sleep Interval",   "Schlafintervall");
REG(SET_DATE,       "Set Date",         "Datum einstellen");
REG(SET_TIME,       "Set Time",         "Uhrzeit einstellen");
REG(CHANGE_PIN,     "Change PIN",       "PIN andern");
REG(NTP_SYNC,       "Sync Time",        "Zeit synchronisieren");
```

## Recommended Fix
1. **Establish a consistent pattern for settings:**
   - **Option A (Noun phrases - recommended for settings list):**
     - "Date" instead of "Set Date"
     - "Time" instead of "Set Time"
     - "PIN" instead of "Change PIN"
   
   - **Option B (Verb phrases - recommended for action buttons):**
     - Keep as is, but ensure all settings follow this pattern

2. **Recommended approach:**
   - Settings menu: Use noun phrases (what you're configuring)
   - Action buttons inside settings: Use verb phrases (what you're doing)
   - Example:
     - Settings list: "Date", "Time", "PIN"
     - Inside date setting: "Set Date", "Cancel"

3. **Update specific strings:**
   - "Set Date" → "Date"
   - "Set Time" → "Time"
   - "Change PIN" → "PIN"
   - "Sync Time" → "Time Sync"

## References
- UI Copy Consistency: Settings menu patterns
- Verb form consistency
