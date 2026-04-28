---
title: "[MEDIUM] Redundant inline comments that repeat obvious code"
severity: MEDIUM
domain: readability
lens: clarity
labels:
  - "audit:code-quality/readability"
---

## Summary
Throughout the codebase, numerous inline comments simply restate what the code already clearly expresses. These "obvious" comments add visual noise without providing additional context, making it harder for developers to distinguish between truly important explanatory comments and mere restatements.

**Files affected:**
- `components/cdc_hal/src/TCA9535Keypad.cpp` (lines 180-200)
- `components/cdc_core/src/ModuleRegistry.cpp` (lines 420-460)
- `components/cdc_ui/src/ViewStack.cpp` (lines 50-70)
- `components/mod_password/src/PasswordStore.cpp` (lines 200-240)

## Impact
- **Visual noise**: Developers must scan through redundant comments to find meaningful explanations
- **Maintenance burden**: Comments that duplicate code must be updated whenever code changes
- **Signal-to-noise ratio**: Important comments get lost among trivial restatements

## Evidence

### Example 1: TCA9535Keypad.cpp (lines 180-200)
```cpp
// Set output registers high (for proper pull-up reading)
bus_->writeReg(device_, REG_OUTPUT_0, &allHigh, 1);
bus_->writeReg(device_, REG_OUTPUT_1, &allHigh, 1);

// No polarity inversion
bus_->writeReg(device_, REG_POLARITY_0, &noInvert, 1);
bus_->writeReg(device_, REG_POLARITY_1, &noInvert, 1);

// Configure all pins as inputs
if (bus_->writeReg(device_, REG_CONFIG_0, &allInputs, 1) != ESP_OK ||
    bus_->writeReg(device_, REG_CONFIG_1, &allInputs, 1) != ESP_OK) {
```

The comments "Set output registers high", "No polarity inversion", and "Configure all pins as inputs" simply repeat what the variable names (`allHigh`, `noInvert`, `allInputs`) and register names already make clear.

### Example 2: ModuleRegistry.cpp (lines 420-460)
```cpp
// Build comma-separated list of module names
char moduleList[MAX_MODULE_LIST_SIZE] = {0};
size_t offset = 0;

for (uint8_t i = 0; i < count_; i++) {
    const char* name = modules_[i]->getName();
    size_t nameLen = strlen(name);

    // Check if it fits
    if (offset + nameLen + 2 > sizeof(moduleList)) {
        LOG_W(TAG, "Module list too long, truncating");
        break;
    }

    if (offset > 0) {
        moduleList[offset++] = ',';
    }
    memcpy(moduleList + offset, name, nameLen);
    offset += nameLen;
}
moduleList[offset] = '\0';

// Save to NVS
nvs_handle_t handle;
if (nvs_open(MODULES_NVS_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
    nvs_set_str(handle, MODULES_NVS_KEY, moduleList);
    nvs_commit(handle);
    nvs_close(handle);
```

Comments like "Build comma-separated list of module names", "Check if it fits", and "Save to NVS" add no value beyond what the code structure already shows.

### Example 3: ViewStack.cpp (lines 50-70)
```cpp
// Call onExit on current view (it's being hidden)
if (depth_ > 0 && stack_[depth_ - 1]) {
    // Don't call onExit - view stays in stack
}

// Push new view
stack_[depth_++] = view;
view->onEnter(context);
// ListView-to-ListView transitions don't require full refresh
needsFullRefresh_ = !(isListView(view) && isListView(depth_ > 1 ? stack_[depth_ - 2] : nullptr));
```

The comment "Push new view" before `stack_[depth_++] = view;` is redundant. The more useful comment about ListView optimization is buried among the trivial ones.

### Example 4: PasswordStore.cpp (lines 200-240)
```cpp
PasswordPayload payload = {};
// Copy text fields
copyText(payload.title, sizeof(payload.title), entry.title);
copyText(payload.username, sizeof(payload.username), entry.username);
copyText(payload.password, sizeof(payload.password), entry.password);
copyText(payload.url, sizeof(payload.url), entry.url);
payload.totpSlot = entry.totpSlot;
copyText(payload.notes, sizeof(payload.notes), entry.notes);

// Create header name
char headerName[cdc::hal::ISecureElement::RMEM_NAME_LEN + 1] = {};
if (entry.title[0]) {
    copyText(headerName, sizeof(headerName), entry.title);
} else {
    copyText(headerName, sizeof(headerName), "Password");
}
```

Comments like "Copy text fields" and "Create header name" state the obvious.

## Recommended Fix

### Step 1: Audit and Remove Obvious Comments
Review each inline comment and apply the following criteria:
- **Remove** if the comment simply restates what the code already clearly expresses
- **Keep** if the comment explains *why* something is done, not *what* is done
- **Keep** if the comment provides context that isn't obvious from code (e.g., hardware constraints, algorithm choices)

### Step 2: Replace with Better Code Structure
Where comments are used to explain code blocks, consider:
- Extracting the block into a well-named function
- Using more descriptive variable names
- Grouping related operations with a single header comment

**Example transformation:**
```cpp
// BEFORE:
// Set output registers high (for proper pull-up reading)
bus_->writeReg(device_, REG_OUTPUT_0, &allHigh, 1);
bus_->writeReg(device_, REG_CONFIG_0, &allInputs, 1);

// AFTER:
// Configure TCA9535: outputs high for pull-up reading, all pins as inputs
configureKeypadGpios();  // Extract to function with self-explanatory name
```

### Step 3: Establish Comment Guidelines
Add to project documentation:
- Comments should explain **why**, not **what**
- Avoid comments that simply translate code to English
- Use comments for: hardware constraints, algorithm rationale, non-obvious optimizations

## References
- [Google C++ Style Guide - Comments](https://google.github.io/styleguide/cppguide.html#Comments)
- [Clean Code - Meaningful Comments](https://cleancodestuff.com/)
- [Microsoft Documentation - Comment Best Practices](https://learn.microsoft.com/en-us/dotnet/csharp/programming-guide/statements-expressions-operators/comments)
