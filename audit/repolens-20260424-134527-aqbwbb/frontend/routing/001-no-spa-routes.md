---
title: "[INFO] No frontend routing to audit - repository is embedded firmware"
severity: INFO
domain: frontend/routing
lens: routing-navigation
labels:
  - audit:frontend/routing
---

## Summary
The **CDC Badge OS** repository is an **embedded firmware project** (C/C++ for ESP32-S3 hardware security key), not a frontend web application with routing.

The codebase consists of:
- **Main firmware**: C/C++ components for ESP32-S3 with TROPIC01 secure element
- **Web flasher**: Single static HTML page (`web-flasher/index.html`) with no routing framework
- **API docs**: Doxygen-generated static HTML documentation

## Impact
The routing & navigation audit scope (URL-based navigation, route guards, 404 pages, deep linking, breadcrumbs, browser back button) does not apply to this codebase.

## Evidence
- No package.json, no frontend build tools (webpack, vite, next, etc.)
- Single static HTML file in `web-flasher/index.html` (309 lines, no client-side routing)
- Main code is C/C++ in `components/` directory (cdc_core, cdc_ui, cdc_views, etc.)

## Recommended Fix
No action needed. If routing audit is required, a different repository containing a frontend web application (React, Vue, Angular, etc.) should be specified.

## References
- Repository structure: `/input/20260423-132359-oj8ayc/cdc-badge-os/`
- Web flasher: `/input/20260423-132359-oj8ayc/cdc-badge-os/web-flasher/index.html`

DONE
