---
title: "[LOW] No CONTRIBUTING.md with security review guidelines"
severity: LOW
domain: secure-sdlc
lens: compliance
labels:
  - "documentation"
  - "code-review"
---

## Summary
The repository lacks a `CONTRIBUTING.md` file with guidelines for contributors, including security-focused review expectations. This means contributors may not know what security considerations to include in their PRs.

## Impact
- **Inconsistent Contributions**: New contributors may not follow security best practices.
- **Review Burden**: Maintainers must remind contributors about security basics.
- **Knowledge Transfer**: Security patterns not documented for future reference.

## Evidence
File check: No `CONTRIBUTING.md` found in:
- Root directory
- `docs/`

README.md mentions the project is a "proof-of-concept / demonstrator" but provides no contribution guidelines.

## Recommended Fix
Create `CONTRIBUTING.md`:
```markdown
# Contributing to CDC Badge OS

## Security Considerations

### Before Submitting a PR
1. Review the [Threat Model](docs/THREAT_MODEL.md)
2. Check if your change affects security-critical code:
   - PIN handling
   - Crypto operations
   - Memory allocation
   - USB HID/CCID protocols

### In Your PR
Include:
- [ ] Security impact analysis (what could be attacked?)
- [ ] Test cases for edge cases
- [ ] Updated documentation if applicable

## Code Review Process

### Security Review Checklist
Reviewers should verify:
- [ ] No hardcoded secrets or credentials
- [ ] PIN lockout logic preserved
- [ ] Memory bounds checked
- [ ] No debug code (LOG_D, DEBUG_MODE) left in
- [ ] Error handling covers all cases

### Two-Reviewer Rule
Security-critical changes require 2 reviewers:
- `components/mod_fido2/`
- `components/mod_gpg/`
- `components/mod_password/`
- `components/cdc_core/` (PIN handling)

## Getting Help
- Discord: [link]
- Issues: Tag with `question` or `security`
- Email: [maintainer email]
```

Link from README.md:
```markdown
## Contributing
See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines.
```

## References
- Contributing guidelines: https://opensource.guide/how-to-contribute/
- Security review checklist: https://cheatsheetseries.owasp.org/
