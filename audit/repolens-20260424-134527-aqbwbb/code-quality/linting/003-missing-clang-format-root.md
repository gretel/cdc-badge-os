---
title: "[LOW] No root-level .clang-format for main codebase"
severity: LOW
domain: linting
lens: code-quality/linting
labels:
  - "audit:code-quality/linting"
---

## Summary
The project has a `.clang-format` file only in `third_party/libtropic/` but no root-level `.clang-format` for the main codebase (components/, main/). This means code formatting is not standardized across the project's own code.

**Evidence:**
```
$ find . -name ".clang-format"
./third_party/libtropic/.clang-format  # Only in third-party directory
```

The existing config in `third_party/libtropic/.clang-format` uses:
- Column limit: 120
- Based on Google style
- Stroustrup braces
- 4-space indent

## Impact
1. **Inconsistent formatting**: Developers may use different formatting styles
2. **No automated formatting**: No easy way to run `clang-format -i` on the whole codebase
3. **Code review overhead**: Reviewers must manually assess style consistency

## Evidence
```bash
# Current state
$ ls -la .clang-format 2>/dev/null || echo "No root .clang-format"
# Output: No root .clang-format

$ cat third_party/libtropic/.clang-format
BasedOnStyle: Google
ColumnLimit: 120
BreakBeforeBraces: Stroustrup
IndentWidth: 4
```

## Recommended Fix
1. Copy the existing config to root:
   ```bash
   cp third_party/libtropic/.clang-format .clang-format
   ```

2. Or create a new config based on ESP-IDF style (documented at https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/contribute/coding-style.html)

3. Add a formatting script or CI step:
   ```bash
   # Format all C++ files
   find components main -name "*.cpp" -o -name "*.h" | xargs clang-format -i
   ```

## References
- ESP-IDF coding style: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/contribute/coding-style.html
- clang-format documentation: https://clang.llvm.org/docs/ClangFormat.html
