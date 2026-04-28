---
title: "[LOW] Update obligation not documented (reasonable period unclear)"
severity: LOW
domain: digital-content-conformity
lens: EU-2019-770
labels:
  - "documentation"
---

## Summary
The firmware is delivered via GitHub releases with automatic release notes generation, but there is no documentation specifying how long users can expect to receive updates (the "reasonable period" required by EU Directive 2019/770 Article 11).

**Files affected:**
- `README.md` - Main documentation
- `.github/workflows/build.yml` - Release workflow
- `docs/README.md` - Documentation index

## Impact
Under EU Directive 2019/770 Article 11(1), the trader must inform the consumer:
- How long updates will be provided
- What conditions apply to updates
- What happens when support ends

Without this documentation, users cannot make informed decisions about the long-term viability of the product.

## Evidence
1. **No update policy in README:**
   - `README.md` mentions "Early Alpha" status and "not production ready"
   - No section on update support duration
   - No mention of security update cadence

2. **Auto-generated release notes only:**
   - `.github/workflows/build.yml:88` - `generate_release_notes: true`
   - Notes are auto-generated from commits, not curated update information

3. **No dedicated update documentation:**
   - `docs/README.md` lists user guides, developer guides, protocol specs
   - No `UPDATES.md` or similar document

## Recommended Fix
Add clear update policy documentation:

1. **Create `UPDATES.md` document:**
   ```markdown
   ## Update Policy
   - **Support period:** 2 years from release date
   - **Security updates:** Critical security patches within 30 days of disclosure
   - **Feature updates:** Minor releases (v0.x) during alpha/beta
   - **End of life:** Repository archived, last release remains available
   ```

2. **Update README.md:**
   - Add "Update Policy" section linking to `UPDATES.md`
   - Include expected support timeline

3. **Define release cadence:**
   - Document what types of updates to expect (security, features, bug fixes)
   - Specify versioning scheme (semantic versioning)

## References
- EU Directive 2019/770 Article 11 (Updates)
- ESP-IDF support policy (reference for embedded firmware)
- Semantic Versioning 2.0.0 (versioning scheme)
