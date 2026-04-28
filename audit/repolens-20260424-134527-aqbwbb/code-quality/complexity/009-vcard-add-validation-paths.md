---
title: "[LOW] Multiple validation paths in vcard_store_add() function"
severity: LOW
domain: mod_vcard
lens: cyclomatic-complexity
labels:
  - "audit:code-quality/complexity"
---

## Summary

The `vcard_store_add()` function in `components/mod_vcard/src/vcard_store.cpp` (lines 529-588) has multiple sequential validation steps with early returns, creating 7+ independent execution paths. While the function is concise (~60 lines), the branching logic could be simplified.

**Estimated Cyclomatic Complexity: ~8**

## Impact

**Readability:**
- Multiple early returns make it harder to see all failure conditions in one place
- Each validation step has similar error handling pattern

**Maintenance:**
- Adding new validation steps increases complexity linearly
- Error messages are duplicated across branches

## Evidence

**File:** `components/mod_vcard/src/vcard_store.cpp:529-588`

**Code excerpt:**
```cpp
bool vcard_store_add(const char* vcard, size_t len, char* err, size_t err_len) {
    // Validation 1: Validate vCard format
    if (!vcard_validate(vcard, len, err, err_len)) {  // +1
        return false;
    }

    vcard_store_init();

    // Validation 2: Check capacity
    if (g_card_count >= VCARD_MAX_CARDS) {  // +1
        set_err(err, err_len, "vCard list full");
        return false;
    }

    nvs_handle_t nvs;
    // Validation 3: Open NVS
    if (nvs_open(VCARD_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {  // +1
        set_err(err, err_len, "NVS open failed");
        return false;
    }

    uint32_t hash = fnv1a_hash(vcard, len);
    // Validation 4: Check for duplicates
    if (vcard_is_duplicate(nvs, vcard, len, hash)) {  // +1
        nvs_close(nvs);
        set_err(err, err_len, "Duplicate vCard");
        return false;
    }

    // Find free slot
    int free_slot = -1;
    for (uint16_t i = 0; i < VCARD_MAX_CARDS; i++) {  // +1 (loop)
        if (!g_cards[i].used) {  // +1
            free_slot = static_cast<int>(i);
            break;
        }
    }

    // Validation 5: Slot found
    if (free_slot < 0) {  // +1
        nvs_close(nvs);
        set_err(err, err_len, "vCard list full");
        return false;
    }

    // Process vCard
    char key[8];
    vcard_key_for_slot(key, sizeof(key), static_cast<uint16_t>(free_slot));
    char tmp[VCARD_MAX_LEN + 1];
    memcpy(tmp, vcard, len);
    tmp[len] = '\0';
    len = vcard_filter_empty_fields(tmp, len);

    // Validation 6: NVS write
    esp_err_t ret = nvs_set_str(nvs, key, tmp);
    if (ret == ESP_OK) {  // +1
        ret = nvs_commit(nvs);
    }
    nvs_close(nvs);
    if (ret != ESP_OK) {  // +1
        set_err(err, err_len, "NVS write failed");
        return false;
    }

    // Success - parse and store metadata
    vcard_parse_names(tmp, g_cards[free_slot].last_name, ...);
    g_cards[free_slot].used = true;
    g_cards[free_slot].hash = hash;
    g_card_count++;
    return true;
}
```

**Branching count:**
- Line 530: `if (!vcard_validate(...))` (+1)
- Line 534: `if (g_card_count >= VCARD_MAX_CARDS)` (+1)
- Line 539: `if (nvs_open(...) != ESP_OK)` (+1)
- Line 546: `if (vcard_is_duplicate(...))` (+1)
- Line 549-554: `for` loop with `if (!g_cards[i].used)` (+2)
- Line 557: `if (free_slot < 0)` (+1)
- Line 571: `if (ret == ESP_OK)` (+1)
- Line 574: `if (ret != ESP_OK)` (+1)

Total: ~9 independent paths

## Recommended Fix

**Use a validation pipeline pattern:**

```cpp
struct AddContext {
    const char* vcard;
    size_t len;
    char* err;
    size_t err_len;
    uint32_t hash;
    int free_slot;
    nvs_handle_t nvs;
};

static bool validateFormat(const AddContext* ctx) {
    return vcard_validate(ctx->vcard, ctx->len, ctx->err, ctx->err_len);
}

static bool checkCapacity(const AddContext* ctx) {
    if (g_card_count >= VCARD_MAX_CARDS) {
        set_err(ctx->err, ctx->err_len, "vCard list full");
        return false;
    }
    return true;
}

static bool openNVS(AddContext* ctx) {
    if (nvs_open(VCARD_NAMESPACE, NVS_READWRITE, &ctx->nvs) != ESP_OK) {
        set_err(ctx->err, ctx->err_len, "NVS open failed");
        return false;
    }
    return true;
}

static bool checkDuplicate(AddContext* ctx) {
    ctx->hash = fnv1a_hash(ctx->vcard, ctx->len);
    if (vcard_is_duplicate(ctx->nvs, ctx->vcard, ctx->len, ctx->hash)) {
        set_err(ctx->err, ctx->err_len, "Duplicate vCard");
        return false;
    }
    return true;
}

static bool findSlot(AddContext* ctx) {
    for (uint16_t i = 0; i < VCARD_MAX_CARDS; i++) {
        if (!g_cards[i].used) {
            ctx->free_slot = static_cast<int>(i);
            return true;
        }
    }
    set_err(ctx->err, ctx->err_len, "vCard list full");
    return false;
}

static bool saveToNVS(AddContext* ctx) {
    char key[8];
    vcard_key_for_slot(key, sizeof(key), static_cast<uint16_t>(ctx->free_slot));
    char tmp[VCARD_MAX_LEN + 1];
    memcpy(tmp, ctx->vcard, ctx->len);
    tmp[ctx->len] = '\0';
    size_t filtered_len = vcard_filter_empty_fields(tmp, ctx->len);

    esp_err_t ret = nvs_set_str(ctx->nvs, key, tmp);
    if (ret == ESP_OK) {
        ret = nvs_commit(ctx->nvs);
    }
    return ret == ESP_OK;
}

static void updateMetadata(AddContext* ctx) {
    char tmp[VCARD_MAX_LEN + 1];
    memcpy(tmp, ctx->vcard, ctx->len);
    tmp[ctx->len] = '\0';

    vcard_parse_names(tmp, g_cards[ctx->free_slot].last_name, ...);
    g_cards[ctx->free_slot].used = true;
    g_cards[ctx->free_slot].hash = ctx->hash;
    g_card_count++;
}

bool vcard_store_add(const char* vcard, size_t len, char* err, size_t err_len) {
    AddContext ctx = {vcard, len, err, err_len, 0, -1, 0};

    // Validation pipeline
    if (!validateFormat(&ctx)) return false;
    vcard_store_init();
    if (!checkCapacity(&ctx)) return false;
    if (!openNVS(&ctx)) return false;
    if (!checkDuplicate(&ctx)) { nvs_close(ctx.nvs); return false; }
    if (!findSlot(&ctx)) { nvs_close(ctx.nvs); return false; }

    // Save
    if (!saveToNVS(&ctx)) { nvs_close(ctx.nvs); return false; }
    nvs_close(ctx.nvs);

    // Success
    updateMetadata(&ctx);
    return true;
}
```

**Alternative (simpler):** Just consolidate NVS handling:

```cpp
bool vcard_store_add(const char* vcard, size_t len, char* err, size_t err_len) {
    if (!vcard_validate(vcard, len, err, err_len)) return false;
    vcard_store_init();
    if (g_card_count >= VCARD_MAX_CARDS) {
        set_err(err, err_len, "vCard list full");
        return false;
    }

    // Find slot first before opening NVS
    int free_slot = -1;
    for (uint16_t i = 0; i < VCARD_MAX_CARDS; i++) {
        if (!g_cards[i].used) { free_slot = i; break; }
    }
    if (free_slot < 0) {
        set_err(err, err_len, "vCard list full");
        return false;
    }

    nvs_handle_t nvs;
    if (nvs_open(VCARD_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        set_err(err, err_len, "NVS open failed");
        return false;
    }

    uint32_t hash = fnv1a_hash(vcard, len);
    if (vcard_is_duplicate(nvs, vcard, len, hash)) {
        nvs_close(nvs);
        set_err(err, err_len, "Duplicate vCard");
        return false;
    }

    // ... rest of save logic
}
```

**Expected result:**
- Reduced cyclomatic complexity
- Better separation of validation vs. save logic
- Easier to add new validation steps

## References

- [Cyclomatic Complexity - Wikipedia](https://en.wikipedia.org/wiki/Cyclomatic_complexity)
- [Refactoring: Replace Conditional with Strategy](https://refactoring.com/catalog/replaceConditionalsWithPolymorphism.html)
