---
title: "[MEDIUM] No PR template with security checklist"
severity: MEDIUM
domain: secure-sdlc
lens: compliance
labels:
  - "code-review"
  - "process"
---

## Summary
The repository lacks a Pull Request template with a security checklist. This means security considerations (testing, documentation, backward compatibility) may be overlooked during code review.

## Impact
- **Inconsistent PR Quality**: Reviewers may miss key security considerations.
- **Incomplete Testing**: Developers may forget to test edge cases or security features.
- **Documentation Gaps**: New features may ship without updated documentation.

## Evidence
File check: No PR template found in:
- `.github/PULL_REQUEST_TEMPLATE.md`
- `.github/PULL_REQUEST_TEMPLATE/`
- Root directory

## Recommended Fix
Create `.github/PULL_REQUEST_TEMPLATE.md`:
```markdown
## Description
Briefly describe the changes in this PR.

## Type of Change
- [ ] Bug fix (non-breaking change)
- [ ] New feature (non-breaking change)
- [ ] Breaking change (fix or feature that changes existing functionality)
- [ ] Documentation update

## Security Checklist
- [ ] No secrets or credentials added
- [ ] PIN/lockout logic tested (if applicable)
- [ ] Memory allocation checked for overflow (if applicable)
- [ ] New module follows security guidelines
- [ ] Feature flags documented (if new)

## Testing
- [ ] Unit tests added/updated
- [ ] Manual testing completed on hardware
- [ ] Serial output verified (no sensitive data leaked)

## Documentation
- [ ] README updated (if new feature)
- [ ] API documentation updated (if changed)
- [ ] Module documentation added (if new module)

## Checklist
- [ ] Code follows project style guidelines
- [ ] Self-review completed
- [ ] Comments added for complex logic
- [ ] No debug code (LOG_D, DEBUG_MODE) left in
```

Add to `build.yml` to validate PRs have templates:
```yaml
- name: Check PR template
  uses: actions/github-script@v6
  with:
    script: |
      const body = context.payload.pull_request.body;
      if (!body.includes('## Security Checklist')) {
        core.warning('PR template with security checklist recommended');
      }
```

## References
- GitHub PR Templates: https://docs.github.com/en/communities/using-templates-to-encourage-useful-issues-and-pull-requests/creating-a-pull-request-template-for-your-repository
- Security checklist best practices: https://cheatsheetseries.owasp.org/
