---
title: "[LOW] Doxygen Awesome CSS uses hardcoded `white` for project header elements"
severity: LOW
domain: adaptive-ux/theme-adaptation
lens: theme-adaptation
labels:
  - dark-mode
  - documentation
---

## Summary
The Doxygen Awesome CSS (`third_party/libtropic/docs/doxygen/html/doxygen-awesome.css`) uses hardcoded `white` color values for project header elements instead of CSS variables. This means these elements will always appear white regardless of the theme:

- Line 407: `#projectname { color: white; }`
- Line 417: `#projectnumber { color: white; }`
- Line 425: `#projectbrief { color: white; }`
- Line 445: `.main-menu-btn-icon { background: white; }`

## Impact
In dark mode, white text on a dark background works well. However:
1. If the dark mode background becomes darker or changes, the hardcoded white may not have optimal contrast
2. In light mode (if ever enabled), white text on a light background would be invisible
3. Inconsistent with the rest of the CSS which uses variables like `--header-foreground`

## Evidence
**File: `third_party/libtropic/docs/doxygen/html/doxygen-awesome.css`**

```css
/* Lines 404-408 */
#projectname {
    font-size: var(--title-font-size);
    font-weight: 600;
    color: white;  /* hardcoded */
}

/* Lines 414-420 */
#projectnumber {
    font-family: inherit;
    font-size: 80%;
    color: white;  /* hardcoded */
    margin-left: calc(20% + 1rem);
}

/* Lines 422-427 */
#projectbrief {
    font-family: inherit;
    font-size: 80%;
    color: white;  /* hardcoded */
    line-height: normal;
}

/* Lines 444-446 */
.main-menu-btn-icon, .main-menu-btn-icon:before, .main-menu-btn-icon:after {
    background: white;  /* hardcoded */
}
```

Note: The dark mode variables are defined in `@media (prefers-color-scheme: dark)` and `html.dark-mode` blocks, but these specific selectors don't use the theme variables.

## Recommended Fix
Replace hardcoded `white` with appropriate CSS variables:

```css
#projectname {
    font-size: var(--title-font-size);
    font-weight: 600;
    color: var(--header-foreground);  /* or --page-foreground-color */
}

#projectnumber {
    font-family: inherit;
    font-size: 80%;
    color: var(--header-foreground);
    margin-left: calc(20% + 1rem);
}

#projectbrief {
    font-family: inherit;
    font-size: 80%;
    color: var(--header-foreground);
    line-height: normal;
}

.main-menu-btn-icon, .main-menu-btn-icon:before, .main-menu-btn-icon:after {
    background: var(--header-foreground);
}
```

The `--header-foreground` variable is already defined in the base `html` selector and updated in the dark mode block, so this would provide consistent theming.

## References
- [CSS-Tricks: CSS Custom Properties for Dynamic Theming](https://css-tricks.com/a-complete-guide-to-css-custom-properties/)
