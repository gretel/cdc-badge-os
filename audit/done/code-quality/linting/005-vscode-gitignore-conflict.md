---
title: "[LOW] .vscode/ directory in .gitignore but contains tracked files"
severity: LOW
domain: linting
lens: code-quality/linting
labels:
  - "audit:code-quality/linting"
---

## Summary
The `.gitignore` file excludes `.vscode/` directory (line 6), but the repository still contains tracked files in `.vscode/` (`.vscode/c_cpp_properties.json`, `extensions.json`, `launch.json`). This creates confusion about whether IDE settings should be committed.

**Location:** `.gitignore` (line 6) and `.vscode/` directory

## Impact
- **Git confusion**: Developers may wonder if they should commit VS Code settings
- **Inconsistent state**: Some VS Code files are tracked, others ignored
- **Merge conflicts**: If `.vscode/` is partially ignored, different developers may have conflicting local settings

## Evidence
`.gitignore` (line 6):
```
.vscode/
```

But `.vscode/` contains tracked files:
- `.vscode/c_cpp_properties.json` (auto-generated, 500+ lines with hardcoded paths)
- `.vscode/extensions.json` (recommended extensions)
- `.vscode/launch.json` (debug configuration)

## Recommended Fix
Choose one of these approaches:

**Option A: Commit minimal VS Code settings**
1. Remove `.vscode/` from `.gitignore`
2. Keep only `extensions.json` and `launch.json` (portable settings)
3. Add `.vscode/c_cpp_properties.json` to `.gitignore` (auto-generated, machine-specific)

**Option B: Ignore all VS Code settings**
1. Keep `.vscode/` in `.gitignore`
2. Untrack existing files: `git rm --cached .vscode/`
3. Create a `.vscode/README.md` with manual setup instructions

Recommended: **Option A** - `extensions.json` helps new developers install required extensions.

## References
- [VS Code settings best practices](https://code.visualstudio.com/docs/getstarted/settings)
