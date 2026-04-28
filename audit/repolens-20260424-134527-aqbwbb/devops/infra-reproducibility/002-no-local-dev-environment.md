---
title: "[MEDIUM] No one-command local development environment setup"
severity: MEDIUM
domain: infra-reproducibility
lens: devops
labels:
  - "audit:devops/infra-reproducibility"
  - "developer-experience"
---

## Summary
The project lacks a one-command local development environment setup mechanism. Developers must manually:
1. Install PlatformIO (via `~/.platformio/penv/bin/pio`)
2. Initialize git submodules manually (`git submodule update --init --recursive`)
3. Know the exact build commands and paths

**Files affected:**
- `README.md:143-160` - Manual build instructions
- No `Makefile`, `docker-compose.yml`, `devcontainer.json`, or `justfile` present

## Impact
- **Onboarding friction**: New developers need to follow multi-step manual setup
- **Environment drift**: Different developers may have different PlatformIO versions or configurations
- **Missing documentation**: Edge cases like "what if PlatformIO is already installed?" are not handled
- **Build consistency**: No guarantee that local builds match CI builds

## Evidence
`README.md:143-160`:
```bash
### Build from Source

Requires [PlatformIO](https://platformio.org/) with ESP-IDF framework.

```bash
# Initialize submodules (first time only)
git submodule update --init --recursive

# Build firmware
~/.platformio/penv/bin/pio run

# Build and flash
~/.platformio/penv/bin/pio run -t upload

# Monitor serial output
~/.platformio/penv/bin/pio device monitor
```
```

No `Makefile`, `docker-compose.yml`, or `devcontainer.json` exists in the repository root.

## Recommended Fix
Create a `Makefile` with common development commands:

```makefile
.PHONY: setup build flash monitor clean

setup:
	git submodule update --init --recursive
	~/.platformio/penv/bin/pio upgrade
	~/.platformio/penv/bin/pio pkg install

build:
	~/.platformio/penv/bin/pio run

flash:
	~/.platformio/penv/bin/pio run -t upload

monitor:
	~/.platformio/penv/bin/pio device monitor

clean:
	rm -rf .pio/build/

dev: setup build
```

Alternatively, add a `devcontainer.json` for VS Code Dev Containers with all prerequisites pre-installed.

## References
- [Makefile best practices](https://www.gnu.org/software/make/manual/make.html)
- [VS Code Dev Containers](https://code.visualstudio.com/docs/devcontainers/containers)
- [Just command runner](https://github.com/casey/just) (modern alternative to Makefile)
