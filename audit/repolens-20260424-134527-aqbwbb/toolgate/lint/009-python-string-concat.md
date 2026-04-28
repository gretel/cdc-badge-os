---
title: "[LOW] Python scripts use implicit string concatenation without clear formatting"
severity: LOW
domain: lint
lens: toolgate/lint
labels:
  - "audit:toolgate/lint"
---

## Summary
Python scripts use implicit string concatenation (adjacent string literals) which can be harder to read and maintain.

**Files affected:**
- `tools/flash_firmware.py` (lines 108-109)

## Impact
Implicit string concatenation can be confusing when strings are long or when editing is needed. f-strings or `.format()` provide better readability.

## Evidence
**tools/flash_firmware.py (lines 108-109):**
```python
print("  Available releases: check https://github.com/"
      f"{GITHUB_REPO}/releases")
```

## Recommended Fix
Use f-strings for better readability:

```python
print(f"  Available releases: check https://github.com/{GITHUB_REPO}/releases")
```

Or use string concatenation with `+`:
```python
print("  Available releases: check https://github.com/" + GITHUB_REPO + "/releases")
```

## References
- Python f-strings: https://docs.python.org/3/tutorial/inputoutput.html#formatted-string-literals
- PEP 498 - Literal String Interpolation: https://peps.python.org/pep-0498/
