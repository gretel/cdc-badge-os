---
title: "[MEDIUM] Dead Code: Commented-out for loop block in DEPG750BN display driver"
severity: MEDIUM
domain: dead-code
lens: code-quality
labels:
  - "audit:code-quality/dead-code"
---

## Summary
In the DEPG750BN e-paper display driver, there is a commented-out for loop at `components/CalEPD/models/dke/depg750bn.cpp:194` that was left in place rather than being properly removed. The loop header is commented but the block that follows remains active.

**Location:** `components/CalEPD/models/dke/depg750bn.cpp:194-220`

```cpp
//for (uint16_t twice = 0; twice < 2; twice++) // done by N2OCP
{
    // leave both controller buffers equal
    IO.cmd(0x91); // partial in
    _setPartialRamArea(x, y, xe, ye);
    IO.cmd(0x13);
    for (int16_t y1 = y; y1 <= ye; y1++)
    {
        for (int16_t x1 = xs_bx; x1 < xe_bx; x1++)
        {
            uint16_t idx = y1 * (DEPG750BN_WIDTH / 8) + x1;
            // ... rest of block
        }
    }
    IO.cmd(0x92); // partial out
}
```

## Impact
- **Maintenance confusion**: Developers may wonder if the loop should be re-enabled or if the block should be fully commented
- **Code clarity**: The structure is misleading - a block with opening brace but no loop control
- **Potential logic error**: If the original loop ran twice, the current code only runs once, which may be intentional but is unclear

## Evidence
File: `components/CalEPD/models/dke/depg750bn.cpp`
- Line 194: `//for (uint16_t twice = 0; twice < 2; twice++) // done by N2OCP`
- Lines 195-221: Active block that was presumably meant to be inside the loop

The comment indicates the loop was disabled because "done by N2OCP" (Next On Chip Power?), but the block structure suggests incomplete refactoring.

## Recommended Fix
Choose one of the following approaches:

1. **If the block is still needed** (most likely):
   - Remove the commented loop line entirely
   - Keep the block as-is with proper indentation

2. **If the block was meant to be disabled**:
   - Comment out the entire block including braces
   - Or delete it if no longer needed

Example fix:
```cpp
_Init_PartialUpdate();
{
    // leave both controller buffers equal
    IO.cmd(0x91); // partial in
    _setPartialRamArea(x, y, xe, ye);
    // ... rest of block
    IO.cmd(0x92); // partial out
}
```

## References
- Dead code patterns: https://en.wikipedia.org/wiki/Dead_code
- Code cleanup best practices: Remove commented-out code in favor of version control
