---
title: "[LOW] Missing prefers-reduced-motion support for animations"
severity: LOW
domain: frontend
lens: a11y
labels:
  - "audit:frontend/accessibility"
---

## Summary
The web flasher uses CSS transitions (`transition: background 0.15s` on line 167) but does not respect the user's `prefers-reduced-motion` preference. Users with vestibular disorders or those who prefer minimal motion may find even subtle transitions distracting.

**Location:** `web-flasher/index.html`, line 167

```css
esp-web-install-button button {
  background: var(--accent);
  color: var(--bg);
  border: none;
  border-radius: 0.5rem;
  padding: 0.85rem 2rem;
  font-size: 1rem;
  font-weight: 600;
  cursor: pointer;
  transition: background 0.15s;  /* No reduced-motion fallback */
}
```

## Impact
- **Users with vestibular disorders** may experience dizziness or nausea from even subtle animations
- **WCAG 2.1 Success Criterion 2.3.3 (Animation from Interactions)** - Not fully compliant
- Users who set their OS to "reduce motion" expect websites to respect this preference
- The transition is subtle but adding support is good practice for inclusivity

## Evidence
- Line 167: `transition: background 0.15s;` on the install button
- No `@media (prefers-reduced-motion: reduce)` query in the stylesheet
- The transition affects the button background on hover/focus

## Recommended Fix
Add a `prefers-reduced-motion` media query to reduce or remove animations:

```css
/* Existing transition */
esp-web-install-button button {
  background: var(--accent);
  color: var(--bg);
  border: none;
  border-radius: 0.5rem;
  padding: 0.85rem 2rem;
  font-size: 1rem;
  font-weight: 600;
  cursor: pointer;
  transition: background 0.15s;
}

/* Reduced motion support */
@media (prefers-reduced-motion: reduce) {
  esp-web-install-button button {
    transition: none;
  }
  
  /* Add other reduced-motion styles if needed */
}
```

Alternatively, reduce the duration instead of removing it completely:
```css
@media (prefers-reduced-motion: reduce) {
  esp-web-install-button button {
    transition: background 0.05s;  /* Faster, less noticeable */
  }
}
```

## References
- [WCAG 2.1 Success Criterion 2.3.3 Animation from Interactions](https://www.w3.org/TR/WCAG21/#animation-from-interactions)
- [MDN: prefers-reduced-motion](https://developer.mozilla.org/en-US/docs/Web/CSS/@media/prefers-reduced-motion)
- [Inclusive Components: Animations](https://inclusive-components.design/animation/)

</content>