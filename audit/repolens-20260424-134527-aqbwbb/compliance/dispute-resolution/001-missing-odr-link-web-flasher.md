---
title: "[LOW] Missing EU ODR Platform link in web flasher footer"
severity: LOW
domain: compliance
lens: audit:compliance/dispute-resolution
labels:
  - "audit:compliance/dispute-resolution"
  - "web-flasher"
  - "eu-regulation"
---

## Summary
The web flasher page (`web-flasher/index.html`) is a consumer-facing web interface used for flashing firmware to the CDC Badge hardware. The footer (line 286-288) contains only a link to the GitHub repository but lacks the legally required EU Online Dispute Resolution (ODR) platform link.

**Location:** `web-flasher/index.html:286-288`

```html
<footer>
  <a href="https://github.com/krim404/cdc-badge-os">CDC Badge OS on GitHub</a>
</footer>
```

## Impact
Under EU Regulation 524/2013 (Online Dispute Resolution), e-commerce websites and online services selling to EU consumers must provide:
1. A link to the EU ODR platform: https://ec.europa.eu/consumers/odr/
2. Information about consumer arbitration (Streitschlichtung)

If the CDC Badge hardware is sold to consumers (B2C) and the web flasher is considered part of the after-sales service, the ODR link should be present. The impact is **LOW** because:
- The web flasher is a utility tool, not a shopping/checkout flow
- The firmware itself is free/open-source
- This applies primarily if hardware is sold via an online shop

## Evidence
**File:** `web-flasher/index.html`
**Lines:** 286-288

Current footer content:
```html
<footer>
  <a href="https://github.com/krim404/cdc-badge-os">CDC Badge OS on GitHub</a>
</footer>
```

**Missing:**
- Link to EU ODR platform: `https://ec.europa.eu/consumers/odr/`
- ODR platform name/description

## Recommended Fix
Add the EU ODR platform link to the footer of `web-flasher/index.html`:

```html
<footer>
  <a href="https://github.com/krim404/cdc-badge-os">CDC Badge OS on GitHub</a>
  | 
  <a href="https://ec.europa.eu/consumers/odr/" target="_blank">EU ODR Platform</a>
</footer>
```

Optionally, add a brief explanation if this is the only legal page:
```html
<footer>
  <a href="https://github.com/krim404/cdc-badge-os">CDC Badge OS on GitHub</a>
  <br>
  <small>EU consumers: <a href="https://ec.europa.eu/consumers/odr/" target="_blank">Online Dispute Resolution Platform</a></small>
</footer>
```

## References
- [EU Regulation 524/2013](https://eur-lex.europa.eu/legal-content/EN/TXT/?uri=CELEX:32013R0524) - Online dispute resolution for consumer disputes
- [EU ODR Platform](https://ec.europa.eu/consumers/odr/) - Official ODR platform
- [German VSBG](https://www.gesetze-im-internet.de/vsbg/) - Act on out-of-court consumer dispute resolution

---
**Note:** This finding is **LOW severity** because the web flasher is a utility tool rather than an e-commerce checkout flow. The primary ODR requirements apply to the online shop where the hardware is sold (if applicable), which is outside this repository.
