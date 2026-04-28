---
title: "[MEDIUM] Error/status messages lack proper ARIA roles for screen readers"
severity: MEDIUM
domain: frontend
lens: a11y
labels:
  - "audit:frontend/accessibility"
---

## Summary
The browser warning messages inside the `esp-web-install-button` web component slots are important status messages but lack appropriate ARIA roles. Screen reader users may not be immediately notified when these messages appear (e.g., "Your browser does not support Web Serial" or "Serial access was denied").

**Location:** `web-flasher/index.html`, lines 263-273

```html
<esp-web-install-button manifest="manifest.json">
  <button slot="activate">Connect (Flash/Serial)</button>
  <span slot="unsupported">
    <div class="browser-warning" style="display: block;">
      Your browser does not support Web Serial.<br>
      Please use <strong>Google Chrome</strong> or <strong>Microsoft Edge</strong> (desktop).
    </div>
  </span>
  <span slot="not-allowed">
    <div class="browser-warning" style="display: block;">
      Serial access was denied. Please grant permission and try again.
    </div>
  </span>
</esp-web-install-button>
```

## Impact
- **Screen reader users may miss critical error messages** - Dynamic content changes are not announced
- **WCAG 2.1 Success Criterion 4.1.3 (Status Messages)** - Not fully met
- Users with cognitive disabilities may not understand why the button isn't working
- The messages are important for troubleshooting but rely solely on visual presentation

## Evidence
- Line 263-268: "unsupported" slot with browser warning - no `role="alert"`
- Line 269-273: "not-allowed" slot with permission error - no `role="alert"`
- Both warnings are dynamically shown by the web component but lack semantic markup
- The `.browser-warning` class (lines 175-181) only has visual styling, no ARIA attributes

## Recommended Fix
Add appropriate ARIA roles to the warning messages:

### For browser support warning (line 263-268):
```html
<span slot="unsupported">
  <div class="browser-warning" role="alert" style="display: block;">
    Your browser does not support Web Serial.<br>
    Please use <strong>Google Chrome</strong> or <strong>Microsoft Edge</strong> (desktop).
  </div>
</span>
```

### For permission denied warning (line 269-273):
```html
<span slot="not-allowed">
  <div class="browser-warning" role="alert" style="display: block;">
    Serial access was denied. Please grant permission and try again.
  </div>
</span>
```

### Alternative - Use `aria-live="assertive"`:
```html
<span slot="unsupported">
  <div class="browser-warning" aria-live="assertive" style="display: block;">
    Your browser does not support Web Serial.<br>
    Please use <strong>Google Chrome</strong> or <strong>Microsoft Edge</strong> (desktop).
  </div>
</span>
```

**Note:** `role="alert"` and `aria-live="assertive"` both create an assertive live region that announces immediately. Use `role="status"` and `aria-live="polite"` for less urgent messages.

## References
- [WCAG 2.1 Success Criterion 4.1.3 Status Messages](https://www.w3.org/TR/WCAG21/#status-messages)
- [MDN: role=alert](https://developer.mozilla.org/en-US/docs/Web/Accessibility/ARIA/Roles/alert_role)
- [WAI-ARIA Authoring Practices - Alerts](https://www.w3.org/WAI/ARIA/apg/patterns/alert/)

</content>