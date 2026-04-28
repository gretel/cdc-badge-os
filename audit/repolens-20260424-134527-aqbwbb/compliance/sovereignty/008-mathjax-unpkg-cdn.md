---
title: "[MEDIUM] MathJax CDN dependency on US-controlled unpkg.com"
severity: MEDIUM
domain: infrastructure-dependency
lens: digital-sovereignty
labels:
  - "audit:compliance/sovereignty"
---

## Summary
The documentation build configuration for the TROPIC01 SDK (`third_party/libtropic/mkdocs.yml`) loads MathJax JavaScript library from `unpkg.com`, a US-controlled Content Delivery Network (CDN).

**File:** `third_party/libtropic/mkdocs.yml` (line 152)
```yaml
extra_javascript:
  - https://unpkg.com/mathjax@3/es5/tex-mml-chtml.js
```

## Impact
- **Geopolitical Risk:** unpkg.com is operated by Fastly (US-headquartered), subjecting the documentation build to US jurisdiction and potential service disruption.
- **Build Dependency:** The documentation build requires fetching the MathJax library from an external US CDN during deployment (GitHub Pages workflow).
- **Availability Risk:** If unpkg.com blocks access or changes pricing/policies, documentation builds will fail or require manual intervention.
- **CLOUD Act Exposure:** Fastly is a US company; request metadata and access patterns could be subject to US government data requests.

## Evidence
**File:** `third_party/libtropic/mkdocs.yml`
```yaml
150: extra_javascript:
151:   - javascripts/mathjax.js
152:   - https://unpkg.com/mathjax@3/es5/tex-mml-chtml.js
```

**File:** `.github/workflows/deploy-pages.yml` (lines 34-58)
The documentation build workflow downloads Doxygen from `doxygen.nl` and SourceForge, then renders the documentation with MathJax loaded from unpkg.com.

## Recommended Fix
Download and host the MathJax library locally within the repository to eliminate the US CDN dependency:

1. **Download MathJax bundle:**
   ```bash
   mkdir -p third_party/libtropic/docs/javascripts/
   curl -L https://unpkg.com/mathjax@3/es5/tex-mml-chtml.js -o third_party/libtropic/docs/javascripts/tex-mml-chtml.js
   ```

2. **Update `mkdocs.yml`** to reference the local file:
   ```yaml
   extra_javascript:
     - javascripts/mathjax.js
     - javascripts/tex-mml-chtml.js
   ```

3. **Add to `.gitignore`** if you want to download it during build:
   - Or commit the file directly for complete offline capability

4. **Update `deploy-pages.yml`** workflow to include the local MathJax file in the build.

**Estimated effort:** ~30 minutes

## References
- **unpkg.com:** US-controlled CDN (Fastly, Inc.) - San Francisco, CA
- **Fastly:** US-headquartered CDN provider, subject to CLOUD Act
- **MathJax:** Open-source JavaScript library for math rendering (can be self-hosted)
- **European CDN alternatives:** Bunny.net (Germany), KeyCDN (Switzerland/Germany)
