---
title: "[LOW] mod_gpg: Empty ccid/ directory in public include path"
severity: LOW
domain: architecture
lens: module-boundaries
labels:
  - "audit:architecture/module-boundaries"
---

## Summary

The `mod_gpg` module has an empty `ccid/` directory in its public include path:

- **`components/mod_gpg/include/mod_gpg/ccid/`** - Empty directory (no files)
- This is a leftover artifact that adds no value and may confuse developers
- CCID headers are actually in `openpgp/ccid.h` (inside the `openpgp/` subdirectory)

## Impact

- **Confusion**: Developers may wonder what belongs in this directory
- **Cleanup needed**: Empty directories clutter the module structure
- **Inconsistent**: Suggests incomplete refactoring or migration

## Evidence

**Directory structure:**
```
components/mod_gpg/include/mod_gpg/
├── GpgModule.h
├── GpgStorage.h
├── gpg.h
├── openpgp/
│   ├── openpgp.h
│   ├── apdu.h
│   └── ccid.h          # CCID header is here (in openpgp/)
└── ccid/               # Empty directory!
    (no files)
```

**CCID header location:**
```cpp
// The actual CCID header is in openpgp/ccid.h:
components/mod_gpg/include/mod_gpg/openpgp/ccid.h
```

## Recommended Fix

1. **Remove empty directory**:
   ```bash
   rmdir components/mod_gpg/include/mod_gpg/ccid/
   ```

2. **Verify no references**:
   ```bash
   grep -r "mod_gpg/ccid/" components/mod_gpg/
   # Should return no results
   ```

3. **Document CCID location** (optional):
   - Add a comment in `openpgp/ccid.h` explaining it's part of the openpgp/ subdirectory
   - Update any README or documentation if it mentions the ccid/ directory

## References

- Module Architecture documentation: `CLAUDE.md` - "Modules must be: Removable, Isolated, Self-registering"
- Clean directory structure is part of good module design

(End of file - total 72 lines)
