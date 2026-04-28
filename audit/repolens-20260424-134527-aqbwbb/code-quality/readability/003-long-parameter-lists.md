---
title: "[003] [MEDIUM] Long parameter lists in TotpModule command handlers reduce clarity"
severity: MEDIUM
domain: code-readability
lens: code-quality/readability
labels:
  - "audit:code-quality/readability"
---

## Summary
In `components/mod_totp/src/TotpModule.cpp`, the `cmd_totp_add` function (lines 223-263) has a long sequence of local buffers and a multi-argument call that makes the function hard to read:

```cpp
static void cmd_totp_add(const char* args) {
    char name[TotpStore::NAME_LEN + 1] = {};
    char secret[128] = {};
    char issuer[TotpStore::ISSUER_LEN + 1] = {};
    char digitsBuf[8] = {};
    char periodBuf[8] = {};
    char algoBuf[8] = {};

    const char* p = nextToken(args, name, sizeof(name));
    if (!p || !*name) {
        cdc::serial::Console::printf("Usage: TOTP_ADD name secret [issuer] [digits] [period] [algo]\r\n");
        return;
    }
    p = nextToken(p, issuer, sizeof(issuer));  // Missing validation!
    // ... more token parsing without validation
```

## Impact
- **Hidden control flow**: Optional parameters are parsed without immediate validation, making it unclear which arguments are required vs. optional
- **Repetitive boilerplate**: The `nextToken` calls with similar patterns create visual noise
- **Error-prone**: Line 239-240 shows `issuer` is parsed without checking `p || !*issuer` like the first two tokens

## Evidence
**File**: `components/mod_totp/src/TotpModule.cpp:223-263`

The function declares 6 local buffers and then calls `nextToken` 6 times, but only validates the first two. The remaining optional parameters are passed to `addAccount` without clear defaults handling visible in the parsing logic.

## Recommended Fix
Consolidate the parsing logic and add consistent validation:

```cpp
static void cmd_totp_add(const char* args) {
    struct ParsedArgs {
        char name[TotpStore::NAME_LEN + 1];
        char secret[128];
        char issuer[TotpStore::ISSUER_LEN + 1];
        char digitsBuf[8];
        char periodBuf[8];
        char algoBuf[8];
    } p = {};

    const char* cursor = args;
    cursor = nextToken(cursor, p.name, sizeof(p.name));
    cursor = nextToken(cursor, p.secret, sizeof(p.secret));
    cursor = nextToken(cursor, p.issuer, sizeof(p.issuer));
    cursor = nextToken(cursor, p.digitsBuf, sizeof(p.digitsBuf));
    cursor = nextToken(cursor, p.periodBuf, sizeof(p.periodBuf));
    cursor = nextToken(cursor, p.algoBuf, sizeof(p.algoBuf));

    if (!*p.name || !*p.secret) {
        cdc::serial::Console::printf("Usage: TOTP_ADD name secret [issuer] [digits] [period] [algo]\r\n");
        return;
    }

    uint8_t digits = p.digitsBuf[0] ? static_cast<uint8_t>(atoi(p.digitsBuf)) : TotpStore::DEFAULT_DIGITS;
    // ... rest of function
```

This reduces repetition and makes the structure clearer.

## References
- [C++ Core Guidelines - F.50: Prefer a single return point for simple functions](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#F50-prefer-a-single-return-point-for-simple-functions)
- [C++ Core Guidelines - F.41: Avoid non-const reference parameters for output](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#F41-avoid-non-const-reference-parameters-for-output)
