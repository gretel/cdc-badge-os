---
title: "[LOW] CI/CD pipeline uses GitHub-hosted runners (US/Microsoft infrastructure)"
severity: LOW
domain: digital-sovereignty
lens: ci-cd-dependency
labels:
  - "audit:compliance/sovereignty"
---

## Summary

The GitHub Actions workflows (`.github/workflows/build.yml` and `.github/workflows/deploy-pages.yml`) use GitHub-hosted runners with no EU region guarantee:

**Files:**
- `.github/workflows/build.yml:18`
- `.github/workflows/deploy-pages.yml:26`

```yaml
jobs:
  build:
    runs-on: ubuntu-latest

  deploy:
    runs-on: ubuntu-24.04
```

GitHub Actions runners are powered by Microsoft Azure infrastructure. While Azure has EU data centers, the default `ubuntu-latest` runner does not guarantee EU location.

## Impact

- **Build Environment Jurisdiction:** Firmware is compiled on US-controlled infrastructure
- **No Region Control:** No explicit EU region selection for runners
- **Dependency Fetching:** All dependencies (ESP-IDF, PlatformIO, components) are fetched during build, potentially from US endpoints
- **CLOUD Act Exposure:** Build artifacts and dependency downloads could theoretically be subject to US government access requests
- **Supply Chain Verification:** No documented procedure for verifying build reproducibility across different runner locations

## Evidence

**File:** `.github/workflows/build.yml`  
**Line:** 18

```yaml
runs-on: ubuntu-latest
```

**File:** `.github/workflows/deploy-pages.yml`  
**Line:** 26

```yaml
runs-on: ubuntu-24.04
```

**Provider:** GitHub Actions (Microsoft, USA)

## Recommended Fix

**Option A: Use self-hosted EU runners**

1. Set up self-hosted runners on European infrastructure:
   - Hetzner Cloud (Germany)
   - OVHcloud (France)
   - Scaleway (France)

2. Register runners in GitHub repository settings:
   ```bash
   ./config.sh --url https://github.com/krim404/cdc-badge-os --token <TOKEN>
   ```

3. Update workflows to use runner labels:
   ```yaml
   runs-on: eu-runner
   ```

**Option B: Use GitHub's EU-hosted runners (if available)**

GitHub offers EU-based runners for certain plans. Check availability and configure:
```yaml
runs-on: [ubuntu-latest, eu]
```

**Option C: Alternative CI providers**

Consider European CI/CD providers:
- [Codeberg Actions](https://codeberg.org/) (Germany, Forgejo)
- [GitLab CI](https://gitlab.com) with EU hosting
- [Drone CI](https://www.drone.io) self-hosted

**Option D: Document the trade-off**

If continuing with GitHub Actions, add a note acknowledging the jurisdiction risk and mitigation steps (e.g., reproducible builds, artifact verification).

## References

- [GitHub Actions Runners](https://docs.github.com/en/actions/hosting-your-own-runners)
- [Microsoft Azure EU Data Boundary](https://azure.microsoft.com/en-us/data-boundary/)
- [Hetzner Cloud](https://www.hetzner.com/cloud) - EU hosting
- [OVHcloud](https://www.ovhcloud.com) - EU hosting
- [Codeberg](https://codeberg.org/) - German Forgejo instance
- [Reproducible Builds](https://reproducible-builds.org/)

---
