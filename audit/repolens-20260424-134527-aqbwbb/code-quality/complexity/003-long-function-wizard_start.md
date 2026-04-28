---
title: "[MEDIUM] Complex string formatting logic in `onWizardCurve` with 50+ lines"
severity: MEDIUM
domain: mod_gpg
lens: cyclomatic-complexity
labels:
  - "audit:code-quality/complexity"
---

## Summary

The `onWizardCurve` function in `components/mod_gpg/src/GpgModule.cpp` (lines 448-497) contains complex string formatting logic with nested conditionals and manual buffer manipulation. The function spans ~50 lines with multiple branching paths for constructing user ID strings.

**Location:** `components/mod_gpg/src/GpgModule.cpp:448-497`

## Impact

- **Maintainability:** String formatting logic is tightly coupled with buffer management
- **Bug risk:** Manual string concatenation increases risk of buffer overflows
- **Readability:** Complex conditionals make the intent hard to understand
- **Testing:** Multiple code paths for different string combinations

## Evidence

```cpp
static void onWizardCurve(uint16_t index, void*) {
    s_wizard.curve = (index == 0) ? CDC_CURVE_ED25519 : CDC_CURVE_P256;
    char user_id[GPG_USER_ID_MAX] = {};
    size_t name_len = strnlen(s_wizard.name, sizeof(s_wizard.name) - 1);
    size_t email_len = strnlen(s_wizard.email, sizeof(s_wizard.email) - 1);
    
    if (email_len == 0) {
        snprintf(user_id, sizeof(user_id), "%.*s", static_cast<int>(sizeof(user_id) - 1), s_wizard.name);
    } else {
        size_t max_len = sizeof(user_id) - 1;
        size_t name_fit = name_len > max_len ? max_len : name_len;
        size_t email_fit = 0;
        if (name_fit < max_len) {
            size_t remaining = max_len - name_fit;
            if (remaining > 3) {
                email_fit = remaining - 3;
            }
        }
        if (email_fit == 0) {
            memcpy(user_id, s_wizard.name, name_fit);
            user_id[name_fit] = '\0';
        } else {
            size_t pos = 0;
            memcpy(user_id + pos, s_wizard.name, name_fit);
            pos += name_fit;
            user_id[pos++] = ' ';
            user_id[pos++] = '<';
            memcpy(user_id + pos, s_wizard.email, email_fit);
            pos += email_fit;
            user_id[pos++] = '>';
            user_id[pos] = '\0';
        }
    }
    gpg_set_pending_user_id(user_id);
    // ... rest of function
}
```

**Complexity breakdown:**
- 1 ternary operator
- 1 if-else (email check)
- 3 nested if-statements for length calculation
- 2 if-else branches for formatting
- Multiple string operations (memcpy, snprintf)

## Recommended Fix

1. **Extract string formatting into a helper**:
```cpp
static void format_user_id(char* buffer, size_t buf_size,
                           const char* name, const char* email) {
    size_t name_len = strnlen(name, buf_size - 1);
    
    if (email && email[0]) {
        // Format: "Name <email>"
        size_t header_len = 2;  // " >"
        size_t available = buf_size - 1 - header_len;
        
        if (name_len > available) {
            name_len = available;
            email = "";  // Skip email if no room
        }
        
        size_t email_len = email ? strnlen(email, available - name_len) : 0;
        
        if (email_len > 0) {
            // "Name <email>"
            size_t pos = 0;
            memcpy(buffer + pos, name, name_len);
            pos += name_len;
            buffer[pos++] = ' ';
            buffer[pos++] = '<';
            memcpy(buffer + pos, email, email_len);
            pos += email_len;
            buffer[pos++] = '>';
            buffer[pos] = '\0';
        } else {
            // Just name
            memcpy(buffer, name, name_len);
            buffer[name_len] = '\0';
        }
    } else {
        // Just name
        memcpy(buffer, name, name_len);
        buffer[name_len] = '\0';
    }
}
```

2. **Simplify onWizardCurve**:
```cpp
static void onWizardCurve(uint16_t index, void*) {
    s_wizard.curve = (index == 0) ? CDC_CURVE_ED25519 : CDC_CURVE_P256;
    
    char user_id[GPG_USER_ID_MAX] = {};
    format_user_id(user_id, sizeof(user_id), s_wizard.name, s_wizard.email);
    
    gpg_set_pending_user_id(user_id);
    // ... rest unchanged
}
```

**Estimated effort:** ~1 hour to refactor

## References

- Extract method refactoring: https://refactoring.com/catalog/extractFunction.html
- String formatting best practices: https://www.fluentcpp.com/2017/05/12/better-string-formatting-c17/
