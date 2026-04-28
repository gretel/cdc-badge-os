---
title: "[LOW] Release version detection depends on GitHub API (US-based)"
severity: LOW
domain: digital-sovereignty
lens: api-dependency
labels:
  - "audit:compliance/sovereignty"
---

## Summary

The web flasher and Python flash tool use GitHub's REST API to fetch release information:

**Web Flasher** (`web-flasher/index.html:296-301`):
```javascript
const resp = await fetch(
  "https://api.github.com/repos/krim404/cdc-badge-os/releases/latest"
);
```

**Python Flash Tool** (`tools/flash_firmware.py:42,98-100`):
```python
GITHUB_REPO = "krim404/cdc-badge-os"
GITHUB_API = f"https://api.github.com/repos/{GITHUB_REPO}"
```

GitHub is owned by Microsoft (US-based).

## Impact

- **Low risk**: Only used for version display and release downloading
- Core firmware functionality doesn't depend on GitHub
- Users can flash from local binaries without API access
- No user data sent to GitHub (public repo only)
- GitHub Pages hosting is optional (can self-host static files)

## Evidence

**File**: `web-flasher/index.html`  
**Lines**: 296-301

```javascript
async function fetchLatestVersion() {
  const badge = document.getElementById("version-badge");
  try {
    const resp = await fetch(
      "https://api.github.com/repos/krim404/cdc-badge-os/releases/latest"
    );
```

**File**: `tools/flash_firmware.py`  
**Lines**: 42, 98-100

```python
GITHUB_REPO = "krim404/cdc-badge-os"
GITHUB_API = f"https://api.github.com/repos/{GITHUB_REPO}"
```

## Recommended Fix

**Option A: Embed version in build**

Generate a version file during CI that the flasher reads locally:

```html
<script>
  const VERSION = "{{VERSION}}";  // Replaced by CI
</script>
```

**Option B: Use alternative release hosting**

Host releases on:
- [Codeberg](https://codeberg.org/) (Germany, Forgejo)
- [SourceHut](https://sourcehut.org/) (Netherlands)
- Self-hosted static server

**Option C: Local manifest file**

Download a simple `manifest.json` instead of full GitHub API:

```json
{
  "latest": "v0.5.0",
  "download_url": "https://example.com/releases/"
}
```

## References

- [GitHub (Microsoft)](https://github.com/)
- [Codeberg (Germany)](https://codeberg.org/)
- [SourceHut (Netherlands)](https://sourcehut.org/)

---
