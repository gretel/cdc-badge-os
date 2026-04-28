---
title: "[MEDIUM] Border colors may have insufficient contrast ratio"
severity: MEDIUM
domain: frontend
lens: a11y
labels:
  - "audit:frontend/accessibility"
---

## Summary
The border color (`--border: #30363d`) used for cards and version badge may not have sufficient contrast against the surface background (`--surface: #161b22`). This affects users with low vision who rely on visual boundaries to distinguish elements.

**Location:** `web-flasher/index.html`, lines 15, 62-63, 76-77

```css
:root {
  --border: #30363d;      /* Border color */
  --surface: #161b22;     /* Card background */
  --step-bg: #1c2128;     /* Step background */
}

/* Lines 62-63 - Version badge */
.version-badge {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 2rem;
}

/* Lines 76-77 - Cards */
.card {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 0.75rem;
}
```

## Impact
- **WCAG 2.1 Success Criterion 1.4.11 (Non-text Contrast)** - May fail Level AA (3:1 ratio required)
- **Users with low vision** may not see element boundaries clearly
- **Visual separation between sections** may be unclear
- The border is the primary visual cue for card boundaries

## Evidence
### Calculated Contrast Ratios:
- Border `#30363d` on Surface `#161b22`: **~2.2:1** (Required: 3:1 for UI components)
- Border `#30363d` on Background `#0d1117`: **~2.7:1** (Required: 3:1 for UI components)

### Problem Areas:
- Line 62: Version badge border
- Line 76: Card borders
- Line 130: Notice border with `rgba(210, 153, 34, 0.3)` (even lower contrast)

## Recommended Fix
Increase border color contrast to meet 3:1 minimum:

### Option 1: Darker border color
```css
:root {
  /* Change from #30363d to a darker shade */
  --border: #404850;  /* Darker for better contrast on --surface */
}
```

### Option 2: Use a more contrasting color
```css
:root {
  /* Use a blue-tinted border that matches the accent */
  --border: #3d4a5a;  /* Better contrast and visual harmony */
}
```

### Option 3: Increase border weight
```css
.card {
  background: var(--surface);
  border: 2px solid var(--border);  /* Thicker border compensates for lower contrast */
  border-radius: 0.75rem;
}
```

### Option 4: Add box-shadow for depth
```css
.card {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: 0.75rem;
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.3);  /* Adds visual depth */
}
```

### Testing
Verify contrast using:
- WebAIM Contrast Checker: https://webaim.org/resources/contrastchecker/
- Chrome DevTools color picker
- axe DevTools extension

## References
- [WCAG 2.1 Success Criterion 1.4.11 Non-text Contrast](https://www.w3.org/TR/WCAG21/#non-text-contrast)
- [WebAIM Contrast Checker](https://webaim.org/resources/contrastchecker/)
- [MDN: border property](https://developer.mozilla.org/en-US/docs/Web/CSS/border)

</content>