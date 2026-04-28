---
title: "[MEDIUM] Missing accessibility statement and conformance information"
severity: MEDIUM
domain: web-flasher
lens: accessibility-eaa
labels:
  - "wcag-2.1"
  - "eaa"
  - "bfsg"
---

## Summary
The CDC Badge OS Web Flasher (located at `/input/20260423-132359-oj8ayc/cdc-badge-os/web-flasher/index.html`) lacks an accessibility statement and does not document target conformance levels. There is no mechanism for users to report accessibility issues.

## Impact
Per EAA (European Accessibility Act) and German BFSG (effective June 2025), public-facing digital products must provide:
- An accessibility statement documenting conformance status
- A mechanism for users to report accessibility problems
- Clear communication of the target WCAG level (e.g., WCAG 2.1 Level AA)

Without this information, users cannot understand what accessibility features to expect or how to report issues.

## Evidence
- File: `web-flasher/index.html` (lines 1-309)
- The footer (line 288) only contains a link to the GitHub repository
- No accessibility statement link or section exists
- No contact mechanism for accessibility feedback
- No mention of WCAG conformance anywhere in the page

## Recommended Fix
Add an accessibility statement section or link to the footer:

1. Create an `accessibility.md` file in the `web-flasher/` directory with:
   - Target conformance level (e.g., "Target: WCAG 2.1 Level AA")
   - Current conformance status
   - Known accessibility limitations
   - Contact information for reporting issues

2. Update the footer in `web-flasher/index.html` (line 288):
```html
<footer>
  <a href="https://github.com/krim404/cdc-badge-os">CDC Badge OS on GitHub</a>
  <span style="margin: 0 0.5rem;">•</span>
  <a href="accessibility.md">Accessibility Statement</a>
</footer>
```

## References
- [EAA (European Accessibility Act)](https://digital-strategy.ec.europa.eu/en/policies/european-accessibility-act)
- [German BFSG (Barrierefreiheitsstärkungsgesetz)](https://www.bmwi.de/Redaktion/EN/Dossier/barrierefreiheitsstaerkungsgesetz.html)
- [WCAG 2.1 Conformance Requirements](https://www.w3.org/WAI/WCAG21/Understanding/conformance.html)
- [Accessibility Statement Guide (W3C)](https://www.w3.org/WAI/planning/statements/)
