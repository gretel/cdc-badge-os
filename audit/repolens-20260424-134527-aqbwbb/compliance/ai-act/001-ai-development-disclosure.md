---
title: "[LOW] AI-assisted development acknowledged but no machine-readable metadata for AI-generated code"
severity: LOW
domain: transparency
lens: ai-act-art50
labels:
  - "ai-generated-content"
---

## Summary
The repository acknowledges AI-assisted development in `README.md:250` and multiple plan documents (`docs/plans/*.md`), but does not provide machine-readable metadata or structured documentation of which code sections were AI-generated vs human-written.

**Evidence:**
- `README.md:250`: "*Co-developed with [Claude Code](https://claude.ai/code) by Anthropic.*"
- `docs/plans/2026-01-28-tropic-storage-implementation-plan.md:3`: "> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task."
- 8 additional plan files in `docs/plans/` with similar Claude-specific instructions

## Impact
While low-risk for a firmware project, the EU AI Act (Art. 50) requires transparency about AI-generated content. For projects with more extensive AI involvement, the lack of structured metadata could make it difficult to:
- Trace which parts of the codebase were AI-generated
- Audit AI-generated code for security vulnerabilities specific to AI synthesis
- Provide provenance information to downstream users

## Evidence
**File:** `README.md`, line 250
```markdown
*Co-developed with [Claude Code](https://claude.ai/code) by Anthropic.*
```

**Files:** `docs/plans/*.md` (8 files)
```markdown
> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.
```

The project uses AI for:
- Implementation planning
- Code generation (implied by "rewrite" mentioned in README)
- Development assistance

But no structured tracking exists.

## Recommended Fix
Add a simple AI disclosure section to the project documentation:

1. **Create `AI_DISCLOSURE.md`** with:
   - Which AI tools were used (Claude Code)
   - What types of tasks AI assisted with (planning, code generation, refactoring)
   - Percentage estimate of AI-generated vs human-written code
   - Human review process for AI-generated code

2. **Optional (more thorough):** Add a `CODE_ORIGIN.md` tracking major components:
   - Human-written: Core HAL, TROPIC01 integration
   - AI-assisted: Module implementations (FIDO2, TOTP, Password vault)
   - AI-generated: Boilerplate, test scaffolding

Example structure:
```markdown
# AI Development Disclosure

## Tools Used
- Claude Code (Anthropic)

## Scope of AI Assistance
- 30% AI-generated (boilerplate, tests, documentation)
- 70% human-written (core security logic, TROPIC01 integration)

## Review Process
- All AI-generated code reviewed by human developer
- Security-critical code (PIN logic, key storage) fully human-written
```

## References
- **EU AI Act Article 50**: Transparency obligations for AI-generated content
- **ISO/IEC 22987**: AI-generated content identification
- **C2PA (Content Credentials)**: Standard for AI content provenance (more relevant for media, but principle applies)

---

**Note:** This is a LOW severity finding because:
1. The project already discloses AI use in plain text
2. It's a firmware project, not a consumer-facing AI system
3. No AI-generated content is being distributed to end users as "AI output"
4. The disclosure is visible and accessible

DONE
