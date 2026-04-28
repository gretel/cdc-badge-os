---
title: "[LOW] No documented evaluation of European alternatives for infrastructure"
severity: LOW
domain: digital-sovereignty
lens: vendor-assessment
labels:
  - "audit:compliance/sovereignty"
---

## Summary

The codebase lacks any documented evaluation of European alternatives for its infrastructure dependencies. There is no architecture decision record (ADR) or documentation addressing:

1. Why Espressif's component registry was chosen over alternatives
2. Why GitHub was selected for hosting (vs. Codeberg, SourceHut)
3. Why unpkg.com was used for the web flasher (vs. EU CDNs)
4. Any consideration of European NTP servers

## Impact

- **Low risk**: Documentation gap rather than technical debt
- Makes it harder to justify infrastructure choices for EU deployments
- Missing context for future maintainers
- No immediate action required but good practice to document decisions

## Evidence

**Files reviewed**:
- `docs/README.md` - No infrastructure decisions documented
- `docs/MODULE_DEVELOPMENT.md` - No vendor selection criteria
- `README.md` - No mention of sovereignty considerations
- No `docs/decisions/` or `docs/adr/` directory exists

## Recommended Fix

Create an architecture decision record documenting infrastructure choices:

**File**: `docs/decisions/001-infrastructure-dependencies.md`

```markdown
# Infrastructure Dependencies

## Context
This firmware requires external services for:
- Component registry (build)
- Release hosting (distribution)
- Web flasher CDN (user experience)
- Time synchronization (runtime)

## Decision
- Components: Espressif registry (Singapore)
- Hosting: GitHub (US)
- CDN: unpkg.com (US)
- NTP: pool.ntp.org + time.google.com

## Alternatives Considered
- Codeberg (Germany) for hosting
- jsDelivr (EU edges) for CDN
- European NTP pools

## Rationale
[Add reasoning based on availability, maturity, and trade-offs]

## Consequences
[Document sovereignty implications]
```

## References

- [European alternatives for software infrastructure](https://european-alternatives.cloud/)
- [ADR template](https://github.com/joelparkerhenderson/architecture-decision-record)
- [Codeberg (Germany)](https://codeberg.org/)
- [jsDelivr (EU presence)](https://www.jsdelivr.com/)

---
