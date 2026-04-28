---
title: "[MEDIUM] Potential color contrast issues for text elements"
severity: MEDIUM
domain: frontend
lens: a11y
labels:
  - "audit:frontend/accessibility"
---

## Summary
Several text color combinations in the web flasher may not meet WCAG AA contrast ratio requirements. The dark theme uses low-light colors that could fail contrast checks for normal text (4.5:1) and large text (3:1).

**Location:** `web-flasher/index.html`, lines 12-22 (CSS variables)

```css
:root {
  --bg: #0d1117;           /* Background */
  --surface: #161b22;      /* Card background */
  --border: #30363d;       /* Border color */
  --text: #e6edf3;         /* Main text */
  --text-muted: #8b949e;   /* Muted text - POTENTIAL ISSUE */
  --accent: #58a6ff;       /* Accent color */
  --accent-hover: #79c0ff; /* Hover accent */
  --warning: #d29922;      /* Warning color */
  --step-bg: #1c2128;      /* Step background */
}
```

## Impact
- **WCAG 2.1 Success Criterion 1.4.3 (Contrast Minimum)** - May fail Level AA
- **Users with visual impairments** may struggle to read muted text
- **Low vision users** relying on high contrast may find text difficult to distinguish
- Muted text (`--text-muted`) on surface background is the highest risk

## Evidence
### Calculated Contrast Ratios (approximate):

| Element | Foreground | Background | Ratio | Required | Status |
|---------|------------|------------|-------|----------|--------|
| Main text | `#e6edf3` | `#0d1117` | ~16:1 | 4.5:1 | ✅ Pass |
| **Muted text** | `#8b949e` | `#161b22` | ~6.5:1 | 4.5:1 | ⚠️ Borderline |
| **Muted text on bg** | `#8b949e` | `#0d1117` | ~8:1 | 4.5:1 | ✅ Pass |
| **Warning text** | `#d29922` | `rgba(210, 153, 34, 0.1)` | ~5:1 | 4.5:1 | ⚠️ Borderline |
| **Accent link** | `#58a6ff` | `#161b22` | ~5.5:1 | 4.5:1 | ⚠️ Borderline |
| **Footer link** | `#58a6ff` | `#0d1117` | ~7:1 | 4.5:1 | ✅ Pass |

### Problem Areas (lines):
- Line 28: `color: var(--text-muted)` on `.subtitle` - Used on dark `--bg` (OK)
- Line 79: Text inside `.card` with `--surface` background - Muted may be borderline
- Line 128-133: Warning notice with `--warning` color on semi-transparent background
- Line 205: Footer links with `--accent` color

## Recommended Fix
Adjust colors to ensure guaranteed contrast compliance:

### Option 1: Increase contrast of muted text
```css
:root {
  /* Change from #8b949e to a darker shade */
  --text-muted: #76838b;  /* Darker for better contrast on --surface */
}
```

### Option 2: Increase contrast of warning color
```css
:root {
  /* Change to a darker, more contrasting color */
  --warning: #b8860b;     /* Darker goldenrod */
}
```

### Option 3: Add explicit contrast-safe overrides
```css
/* Ensure muted text on cards has sufficient contrast */
.card p {
  color: var(--text);  /* Use main text color */
}

.card .subtitle {
  color: #a0aec0;  /* Explicit value with known contrast ratio */
}

/* Ensure links have sufficient contrast */
footer a,
.card a {
  color: #79b8ff;  /* Slightly darker blue for better contrast */
}
```

### Testing
Use these tools to verify contrast:
- WebAIM Contrast Checker: https://webaim.org/resources/contrastchecker/
- Chrome DevTools: Color picker shows contrast ratio
- axe DevTools extension

## References
- [WCAG 2.1 Success Criterion 1.4.3 Contrast (Minimum)](https://www.w3.org/TR/WCAG21/#contrast-minimum)
- [WebAIM Contrast Checker](https://webaim.org/resources/contrastchecker/)
- [MDN: color property](https://developer.mozilla.org/en-US/docs/Web/CSS/color)
