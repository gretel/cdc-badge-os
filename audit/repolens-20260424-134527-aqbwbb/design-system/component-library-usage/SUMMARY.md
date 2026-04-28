# Component Library Adherence Audit Summary

## Repository: CDC Badge OS (Embedded C++ Firmware)

**Audit Focus:** UI component library consistency, I18n (internationalization) usage, and view component patterns.

---

## Findings Overview

| #  | Severity | Title | Component |
|----|----------|-------|-----------|
| 001 | MEDIUM | Inconsistent UI component library namespace usage across modules | multiple |
| 002 | HIGH | ConfirmView footer hint hardcoded in German, bypassing I18n system | cdc_views |
| 003 | MEDIUM | RgbInputView has hardcoded English "Preview:" label | grove_led |
| 004 | LOW | ConfirmView has dead code: getFooterHint() override never used | cdc_views |
| 005 | MEDIUM | ToastView and MessageBox provide overlapping functionality - potential consolidation | cdc_views |
| 006 | MEDIUM | RgbInputView footer hint hardcoded in English, bypassing I18n system | grove_led |
| 007 | MEDIUM | HidStatusView has hardcoded English strings, bypassing I18n system | mod_hid |
| 008 | LOW | Audit findings have duplicate numbering (two 001s) - needs reorganization | audit-process |

---

## Key Observations

### Component Library Structure

The project has a well-organized component library structure:

1. **cdc_ui** - Core UI framework (IView interface, ViewStack, I18n)
2. **cdc_views** - Reusable standard views (ListView, PinEntryView, SliderView, etc.)
3. **cdc_os_ui** - OS-level views (LockScreenView, PinChangeView)
4. **Module-specific views** - grove_led/RgbInputView, mod_hid/HidStatusView

### I18n System

The I18n system is well-implemented:
- Core strings defined in `I18n.h` with `StringId` enum
- Translations for English and German in `I18n.cpp`
- German umlauts correctly use `ae`, `oe`, `ue` format
- Modules can register their own strings dynamically

### Common Patterns

Most views follow consistent patterns:
- Use `getFooterHint()` method for footer text
- Call `tr(StringId::...)` for internationalized strings
- Use `render::drawFooterBar()` helper for consistent footer rendering

### Common Issues

The most common issues found:
1. **Hardcoded strings in module-specific views** - Views in modules (grove_led, mod_hid) often forget to use I18n
2. **Component redundancy** - ToastView and MessageBox provide similar functionality
3. **Dead code** - Some view methods declared but never used

---

## Issues Detail

### 001 - ConfirmView I18n Inconsistency (HIGH)

**Problem:** The `ConfirmView` component has hardcoded German text that bypasses the I18n system.

**Evidence:**
- Header declares: `getFooterHint()` returns `"Y=OK  N=Abbruch"`
- Source renders: `"Y=Ja  N=Nein"` (different German text)
- Other views use `tr(StringId::HINT_APPROVE_DENY)` correctly

**Impact:** German-only UI, inconsistent with other views, two different German strings exist.

**Fix:** Use `tr(StringId::HINT_APPROVE_DENY)` in both header and source.

---

### 002 - RgbInputView Preview Label (MEDIUM)

**Problem:** The `RgbInputView` has a hardcoded English "Preview:" label.

**Evidence:**
- Line 249: `gfx->print("Preview:");`
- Same file line 260 correctly uses: `ui::tr(ui::StringId::HINT_FIELD_NAV)`

**Impact:** Mixed English/German UI for German users.

**Fix:** Add `PREVIEW_LABEL` string to I18n and use it.

---

### 003 - ConfirmView Dead Code (LOW)

**Problem:** The `getFooterHint()` override in ConfirmView is never called.

**Evidence:**
- Header has: `getFooterHint() const override { return "Y=OK  N=Abbruch"; }`
- Source render function directly prints: `gfx->print("Y=Ja  N=Nein");`
- No call to `getFooterHint()` in the render function

**Impact:** Confusing for developers, maintenance burden, inconsistent with other views.

**Fix:** Either use `getFooterHint()` pattern or remove the override.

---

### 004 - ToastView and MessageBox Redundancy (MEDIUM)

**Problem:** Two similar modal overlay components (`ToastView` and `MessageBox`) provide essentially the same functionality.

**Evidence:**
- Both implement centered modal dialogs with icons
- Both support auto-dismiss timeout
- Both have similar icon types (SUCCESS, ERROR, INFO)
- `ToastView::Icon` has 6 types, `MessageBox::MessageIcon` has 5 types

**Impact:** Developer confusion, code duplication, maintenance overhead.

**Fix:** Consolidate into single component (keep MessageBox, deprecate ToastView).

---

### 005 - RgbInputView Footer Hint (MEDIUM)

**Problem:** The `RgbInputView` footer hint is hardcoded in English while the same file correctly uses I18n elsewhere.

**Evidence:**
- Line 178: `return "0-9:Input 4/6:Field Y:OK";`
- Line 260: `gfx->print(ui::tr(ui::StringId::HINT_FIELD_NAV));` (correct I18n)

**Impact:** English-only footer for German users, inconsistent with same file's other I18n usage.

**Fix:** Register module-specific I18n string for footer hint.

---

### 006 - RgbInputView Footer Hint (MEDIUM)

**Problem:** The `RgbInputView` footer hint is hardcoded in English while the same file correctly uses I18n elsewhere.

**Evidence:**
- Line 178: `return "0-9:Input 4/6:Field Y:OK";`
- Line 260: `gfx->print(ui::tr(ui::StringId::HINT_FIELD_NAV));` (correct I18n)

**Impact:** English-only footer for German users, inconsistent with same file's other I18n usage.

**Fix:** Register module-specific I18n string for footer hint.

---

### 007 - HidStatusView I18n (MEDIUM)

**Problem:** The `HidStatusView` class has hardcoded English strings while the module correctly uses I18n elsewhere.

**Evidence:**
- Lines 142, 145, 148: `gfx->print("Windows")`, `gfx->print("Linux")`, `gfx->print("macOS")`
- Line 170: `return "[N] Back";`
- Lines 211-218: Module correctly uses `mstr()` for menu items

**Impact:** Mixed English/German UI, German translations exist but are ignored.

**Fix:** Add OS name strings to module I18n registration and use `mstr()` helper.

---

### 008 - Duplicate Numbering (LOW)

**Problem:** Audit findings have duplicate numbering (two 001s) which creates confusion.

**Evidence:**
- `001-confirmview-i18n-inconsistency.md` and `001-inconsistent-namespace-usage.md`

**Impact:** Confusion when referencing findings, automation issues, unprofessional presentation.

**Fix:** Re-number files sequentially and update SUMMARY.md.

---

## Recommendations

1. **Standardize ConfirmView:** Fix the I18n inconsistency and dead code issues together (Issues 002, 004)
2. **Add PREVIEW_LABEL:** Add a common UI label to the I18n system for reuse (Issue 003)
3. **Review Module Views:** Ensure module-specific views follow the same I18n patterns as core views (Issues 006, 007)
4. **Consolidate ToastView/MessageBox:** Reduce component redundancy (Issue 005)
5. **Fix Numbering:** Re-number audit findings sequentially (Issue 008)
6. **Create I18n checklist:** Add a review step for new views to ensure I18n compliance

---

## What Was Checked

- [x] All view components in cdc_views
- [x] OS-level views in cdc_os_ui
- [x] Module-specific views (grove_led, mod_hid)
- [x] I18n string definitions and translations
- [x] Footer hint consistency across views
- [x] German umlaut handling (ae/oe/ue format)
- [x] Module string registration patterns
- [x] Render helper usage
- [x] Component redundancy analysis
- [x] Namespace usage consistency
- [x] Audit file organization

---

**Audit Date:** April 26, 2026
**Total Findings:** 8 (1 HIGH, 4 MEDIUM, 2 LOW, 1 INFO)

</content>