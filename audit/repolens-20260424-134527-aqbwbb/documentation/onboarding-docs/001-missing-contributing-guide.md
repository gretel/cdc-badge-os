---
title: "[MEDIUM] Missing CONTRIBUTING.md with PR process and branch conventions"
severity: MEDIUM
domain: developer-onboarding
lens: onboarding-docs
labels:
  - "audit:documentation/onboarding-docs"
---

## Summary
The repository lacks a `CONTRIBUTING.md` file at the root level. There is only a minimal "Contributing" section in `docs/README.md` (3 lines) that covers:
- Code in English
- Follow existing patterns
- Use `cdc_log` for logging
- Reference to Module Development guide

Missing documentation includes:
- Branch naming conventions (e.g., `feature/*`, `fix/*`, `release/*`)
- Commit message format
- Pull request template or checklist
- Code review process
- How to submit a PR
- CI/CD checks that must pass
- When to create issues vs. direct PRs

**Evidence:**
- `docs/README.md` lines 154-159: Only 5 bullet points for "Contributing"
- No `CONTRIBUTING.md` file in root directory
- No `.github/PULL_REQUEST_TEMPLATE.md` or `.github/ISSUE_TEMPLATE/` directory

## Impact
New developers won't know:
- How to structure their branches
- What format to use for commit messages
- What to include in a PR description
- The review workflow (who reviews, what to expect)
- Whether there are specific requirements for different types of changes (bug fixes vs. features)

This leads to:
- Inconsistent PR submissions
- Back-and-forth communication to clarify process
- Potential rejected PRs due to missing information
- Friction during onboarding

## Evidence
1. `docs/README.md:154-159`:
```markdown
## Contributing

- All code and documentation in **English**
- Follow existing patterns in the codebase
- Use `cdc_log` for logging (never `ESP_LOG` directly)
- See [Module Development](MODULE_DEVELOPMENT.md) for architecture guidelines
```

2. No PR template exists:
```bash
$ ls -la .github/
total 3
drwxr-xr-x  3 .github
drwxr-xr-x 14 .
...
```

3. `README.md` mentions "Early Alpha" and "Rewrite" but doesn't explain how to contribute to this state of the codebase.

## Recommended Fix
Create a `CONTRIBUTING.md` file at the repository root with:

1. **Getting Started** section:
   - Prerequisites (PlatformIO, ESP-IDF, Python 3.11)
   - How to fork and clone
   - How to set up development environment

2. **Branch Naming Conventions**:
   - `feature/<description>` for new features
   - `fix/<description>` for bug fixes
   - `docs/<description>` for documentation changes
   - `refactor/<description>` for code refactoring
   - Example: `feature/add-totp-rotation`

3. **Commit Message Format**:
   - Conventional Commits style (e.g., `feat: add TOTP rotation`, `fix: correct PIN lockout`)
   - Keep commits focused and atomic
   - Reference issues in commit messages

4. **Pull Request Process**:
   - Create PR against `main` branch
   - Required checks: build must pass
   - Code review by maintainer
   - Squash merge for cleaner history (optional)

5. **PR Template/Checklist**:
   - [ ] Code follows project conventions
   - [ ] Documentation updated
   - [ ] Tested on hardware (if applicable)
   - [ ] No new warnings in build
   - [ ] Changelog updated (optional)

6. **Code Review Guidelines**:
   - What reviewers look for
   - Typical response time
   - How to address feedback

7. **First-Time Contributors**:
   - Good first issue label
   - How to ask questions
   - Link to `CLAUDE.md` and `docs/MODULE_DEVELOPMENT.md`

**Estimated effort:** ~45-60 minutes to write initial draft based on common patterns.

## References
- [GitHub's guide to contributing](https://docs.github.com/en/get-started/quickstart/contributing-to-projects)
- [Conventional Commits](https://www.conventionalcommits.org/)
- [Keep a Changelog](https://keepachangelog.com/)
