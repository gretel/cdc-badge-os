---
title: "[LOW] Multiple font families in Doxygen CSS without clear hierarchy"
severity: LOW
domain: visual-design
lens: typography-scale
labels:
  - audit:visual-design/typography-scale
---

## Summary
The Doxygen documentation CSS (`doxygen_output/html/doxygen.css`) defines 7 different font-family variables without a clear visual hierarchy or role definition:

- `--font-family-normal`: system-ui, Roboto, Ubuntu, sans-serif
- `--font-family-monospace`: JetBrains Mono, Consolas, Monaco, monospace
- `--font-family-nav`: Lucida Grande, Geneva, Helvetica, Arial
- `--font-family-title`: system-ui, Roboto, Ubuntu, sans-serif (same as normal)
- `--font-family-toc`: Verdana, DejaVu Sans, Geneva, sans-serif
- `--font-family-search`: Arial, Verdana, sans-serif
- `--font-family-icon`: Arial, Helvetica
- `--font-family-tooltip`: Roboto, sans-serif

**Evidence location:** `doxygen_output/html/doxygen.css:65-75`

## Impact
- **Visual inconsistency**: 7 font families create a fragmented visual identity
- **Maintenance burden**: Hard to maintain consistent typography across documentation
- **Performance**: Multiple font stacks may cause subtle rendering differences
- **Cohesion**: Navigation uses Lucida Grande while body uses system-ui - no clear reason

## Evidence
```css
/** font-family */
--font-family-normal: system-ui,-apple-system,Segoe UI,Roboto,Ubuntu,Cantarell,Noto Sans,sans-serif,BlinkMacSystemFont,"Segoe UI",Helvetica,Arial,sans-serif,"Apple Color Emoji","Segoe UI Emoji","Segoe UI Symbol";
--font-family-monospace: 'JetBrains Mono',Consolas,Monaco,'Andale Mono','Ubuntu Mono',monospace,fixed;
--font-family-nav: 'Lucida Grande',Geneva,Helvetica,Arial,sans-serif;
--font-family-title: system-ui,-apple-system,Segoe UI,Roboto,Ubuntu,Cantarell,Noto Sans,sans-serif,BlinkMacSystemFont,"Segoe UI",Helvetica,Arial,sans-serif,"Apple Color Emoji","Segoe UI Emoji","Segoe UI Symbol";
--font-family-toc: Verdana,'DejaVu Sans',Geneva,sans-serif;
--font-family-search: Arial,Verdana,sans-serif;
--font-family-icon: Arial,Helvetica;
--font-family-tooltip: Roboto,sans-serif;
```

Note: `--font-family-normal` and `--font-family-title` are identical, suggesting redundancy.

## Recommended Fix
Consolidate to 2-3 font families with clear roles:

1. **Define simplified font families**:
```css
:root {
  /* Primary sans-serif (body, headings, nav, title) */
  --font-family-primary: system-ui,-apple-system,Segoe UI,Roboto,Ubuntu,Cantarell,Noto Sans,sans-serif;
  
  /* Monospace for code */
  --font-family-mono: 'JetBrains Mono',Consolas,Monaco,monospace;
}
```

2. **Apply consistently**:
```css
body, h1, h2, h3, .nav, .title {
  font-family: var(--font-family-primary);
}

code, pre {
  font-family: var(--font-family-mono);
}
```

3. **Remove unused families**: Delete `--font-family-nav`, `--font-family-toc`, `--font-family-search`, `--font-family-icon`, `--font-family-tooltip` if they don't add visual distinction.

## References
- [Google Fonts - Font Pairing](https://fonts.google.com/knowledge/using_type/font_pairing_basics)
- [MDN - font-family](https://developer.mozilla.org/en-US/docs/Web/CSS/font-family)
- [CSS Tricks - System Font Stack](https://css-tricks.com/snippets/css/system-font-stack/)
