---
title: "[LOW] TOTP code view lacks visual section grouping for related information"
severity: LOW
domain: information-architecture
lens: content-hierarchy
labels:
  - "audit:information-architecture/content-hierarchy"
---

## Summary
The TOTP code view (`components/mod_totp/src/TotpModule.cpp:390-470`) displays three distinct information types without clear visual section boundaries:
1. **Account metadata** (name, issuer) at top
2. **Primary code** (6-digit TOTP) in center
3. **Timer status** (progress bar + countdown) at bottom

While the layout is functional, there are no visual separators, section headers, or grouped containers to help users quickly identify which information belongs together.

## Impact
On a small 296x128 display, users scanning for specific information (e.g., "which account is this?" or "how many seconds left?") must parse the entire screen linearly. The lack of visual grouping makes it harder to:
- Distinguish account metadata from the actual code
- Identify the timer section quickly
- Scan the view for specific information types

## Evidence
File: `components/mod_totp/src/TotpModule.cpp:390-470`

```cpp
// Account name (no section header)
gfx->setCursor(8, 28);
gfx->print(name_);
if (issuer_[0]) {
    gfx->setCursor(8, 40);
    gfx->print(issuer_);
}

// Large centered code (no visual separation from metadata)
gfx->setTextSize(2);
gfx->setCursor(codeX, 58);
gfx->print(code_);

// Progress bar + counter (no section label)
gfx->drawRect(barX, barY, barW, barH, EPD_BLACK);
gfx->fillRect(barX + 1, barY + 1, fillW, barH - 2, EPD_BLACK);
gfx->setCursor(barX, barY + 14);
display->printf("%us", static_cast<unsigned>(remaining_));
```

All three sections use the same font size (except the code itself) and no visual separators between sections.

## Recommended Fix
1. Add a horizontal divider line between account metadata and TOTP code section
2. Add a second divider between code and timer section
3. Consider adding a small label like "CODE" or "TIME" above the respective sections for clarity

Approximate effort: 30-45 minutes to add visual separators and optional section labels.

## References
- TOTP code view: `components/mod_totp/src/TotpModule.cpp:390-470`
- Similar pattern in GPG status view: `components/mod_gpg/src/GpgModule.cpp:400-406`

</content>