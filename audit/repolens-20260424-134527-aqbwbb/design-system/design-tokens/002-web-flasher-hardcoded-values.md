---
title: "[LOW] Web flasher CSS uses hardcoded values instead of token scale"
severity: LOW
domain: design-system
lens: design-tokens
labels:
  - "audit:design-system/design-tokens"
---

## Summary

The web flasher (`web-flasher/index.html`) defines CSS custom properties (tokens) in `:root` but does not consistently use them throughout the styles. Several hardcoded values bypass the token layer, particularly for spacing, typography, and motion.

**File:** `web-flasher/index.html:11-216`

### Hardcoded Values Found:

**Spacing (lines 24-170):**
- `padding: 2rem 1.5rem;` (line 41) - should use spacing tokens
- `margin-bottom: 2rem;` (line 46) - should use spacing tokens
- `padding: 0.25rem 0.85rem;` (line 65) - should use spacing tokens
- `padding: 1.5rem;` (line 77) - should use spacing tokens
- `padding: 0.75rem 0.75rem 0.75rem 3rem;` (line 97) - should use spacing tokens
- `margin-bottom: 0.5rem;` (line 100) - should use spacing tokens
- `padding: 0.75rem;` (line 129) - should use spacing tokens
- `padding: 2rem 1.5rem;` (line 144) - should use spacing tokens
- `margin-bottom: 1.25rem;` (line 151) - should use spacing tokens
- `padding: 1.5rem;` (line 199) - should use spacing tokens

**Typography (lines 48-200):**
- `font-size: 1.75rem;` (line 50) - should use typography tokens
- `font-size: 0.95rem;` (line 55) - should use typography tokens
- `font-size: 0.85rem;` (line 66) - should use typography tokens
- `font-size: 1.1rem;` (line 79) - should use typography tokens
- `font-size: 0.9rem;` (line 101) - should use typography tokens
- `font-size: 0.85rem;` (line 132) - should use typography tokens
- `font-size: 0.9rem;` (line 152) - should use typography tokens
- `font-size: 0.8rem;` (line 201) - should use typography tokens
- `font-size: 1rem;` (line 166) - should use typography tokens

**Motion (line 169):**
- `transition: background 0.15s;` - hardcoded duration, should use motion token

**Hardcoded inline styles (line 222):**
- `style="width: 80px; height: auto; border-radius: 0.5rem; flex-shrink: 0;"` - inline styles bypass token layer

## Impact

**Maintenance:**
- Changing spacing scale requires finding and replacing values across multiple lines
- No consistent spacing rhythm (mix of `0.25rem`, `0.5rem`, `0.75rem`, `0.85rem`, `1rem`, `1.25rem`, `1.5rem`, `2rem`)
- Hard to enforce design consistency

**Theming:**
- Typography changes require modifying each occurrence individually
- No easy way to create responsive type scale
- Motion preferences (reduced motion) cannot be applied globally

**Code Quality:**
- Inline styles on image element (line 222) break separation of concerns
- Hardcoded `80px` width should be a token for consistency

## Evidence

**Current Token Definitions (lines 12-22):**
```css
:root {
  --bg: #0d1117;
  --surface: #161b22;
  --border: #30363d;
  --text: #e6edf3;
  --text-muted: #8b949e;
  --accent: #58a6ff;
  --accent-hover: #79c0ff;
  --warning: #d29922;
  --step-bg: #1c2128;
}
```

**Hardcoded Spacing Example (line 41):**
```css
.container {
  max-width: 640px;
  width: 100%;
  padding: 2rem 1.5rem;  /* Hardcoded */
}
```

**Hardcoded Typography Example (line 50):**
```css
header h1 {
  font-size: 1.75rem;  /* Hardcoded */
  font-weight: 600;
  margin-bottom: 0.25rem;  /* Hardcoded */
}
```

**Hardcoded Motion Example (line 169):**
```css
esp-web-install-button button:hover {
  transition: background 0.15s;  /* Hardcoded */
}
```

**Inline Style Bypass (line 222):**
```html
<img src="badge.jpg" alt="CDC Badge v1.0" style="width: 80px; height: auto; border-radius: 0.5rem; flex-shrink: 0;">
```

## Recommended Fix

**Step 1: Expand token definitions**

Add spacing, typography, and motion tokens to `:root`:

```css
:root {
  /* Colors */
  --bg: #0d1117;
  --surface: #161b22;
  --border: #30363d;
  --text: #e6edf3;
  --text-muted: #8b949e;
  --accent: #58a6ff;
  --accent-hover: #79c0ff;
  --warning: #d29922;
  --step-bg: #1c2128;

  /* Spacing Scale */
  --space-xs: 0.25rem;
  --space-sm: 0.5rem;
  --space-md: 0.75rem;
  --space-lg: 1rem;
  --space-xl: 1.25rem;
  --space-2xl: 1.5rem;
  --space-3xl: 2rem;

  /* Typography Scale */
  --text-xs: 0.8rem;
  --text-sm: 0.85rem;
  --text-base: 0.95rem;
  --text-lg: 1.1rem;
  --text-xl: 1.4rem;
  --text-2xl: 1.75rem;

  /* Motion */
  --transition-fast: 0.15s;
  --transition-normal: 0.25s;

  /* Layout */
  --container-max-width: 640px;
  --image-size-sm: 80px;
  --border-radius-sm: 0.5rem;
  --border-radius-md: 0.75rem;
  --border-radius-lg: 2rem;
}
```

**Step 2: Update styles to use tokens**

Example refactoring:

```css
/* Before */
.container {
  max-width: 640px;
  width: 100%;
  padding: 2rem 1.5rem;
}

/* After */
.container {
  max-width: var(--container-max-width);
  width: 100%;
  padding: var(--space-3xl) var(--space-2xl);
}

/* Before */
header h1 {
  font-size: 1.75rem;
  font-weight: 600;
  margin-bottom: 0.25rem;
}

/* After */
header h1 {
  font-size: var(--text-2xl);
  font-weight: 600;
  margin-bottom: var(--space-xs);
}

/* Before */
esp-web-install-button button {
  transition: background 0.15s;
}

/* After */
esp-web-install-button button {
  transition: background var(--transition-fast);
}
```

**Step 3: Move inline styles to CSS**

```css
/* Add to styles */
.badge-image {
  width: var(--image-size-sm);
  height: auto;
  border-radius: var(--border-radius-sm);
  flex-shrink: 0;
}
```

```html
<!-- Update HTML -->
<img class="badge-image" src="badge.jpg" alt="CDC Badge v1.0">
```

## References

- **CSS Custom Properties Best Practices**: https://web.dev/articles/design-tokens
- **Design Token Categories**: https://designsystem.digital.gov/design-tokens/types/
- **Spacing Tokens**: https://uxdesign.cc/design-token-cheat-sheet-spacing-417f64b1b6c0
- **Typography Scale**: https://type-scale.com/

## Notes

This is a **low-priority** finding because:
1. The web flasher is a simple, single-page tool with limited scope
2. The current implementation is functional
3. The token foundation exists (`:root` variables) but is incomplete

However, addressing this will:
1. Improve maintainability for future changes
2. Establish better patterns for any future web components
3. Enable easier theming if needed (e.g., light mode, high contrast)

Estimated effort: **30-45 minutes** to refactor all hardcoded values.
