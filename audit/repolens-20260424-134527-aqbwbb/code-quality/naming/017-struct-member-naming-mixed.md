---
title: "[MEDIUM] Struct member naming: inconsistent use of trailing underscores"
severity: MEDIUM
domain: naming-conventions
lens: code-quality
labels:
  - "audit:code-quality/naming"
---

## Summary
Struct and class member variables use inconsistent naming. Some use trailing underscores (`items_`, `selection_`), while others in similar structures don't follow the same pattern.

**Evidence:**

1. **components/cdc_views/include/cdc_views/ListView.h** (lines 135-144):
   ```cpp
   private:
       const char* title_ = nullptr;
       const char* customHint_ = nullptr;
       const ListItem* items_ = nullptr;
       uint16_t itemCount_ = 0;
       uint16_t selection_ = 0;
       uint16_t scrollPos_ = 0;
       SelectCallback onSelect_ = nullptr;
       MenuCallback onMenu_ = nullptr;
       ItemRenderCallback itemRenderer_ = nullptr;
       void* itemRendererCtx_ = nullptr;
       bool preservePosition_ = false;
       uint8_t itemHeight_ = DEFAULT_ITEM_HEIGHT;
       uint8_t visibleItems_ = 4;
   ```
   Consistently uses trailing underscores for all members.

2. **components/cdc_core/include/cdc_core/EventBus.h** (lines 136-141):
   ```cpp
   private:
       struct Subscription {
           EventHandler handler;
           uint32_t mask;
           bool active;
       };

       Subscription handlers_[MAX_HANDLERS] = {};
       void* queue_ = nullptr;
       bool initialized_ = false;
   ```
   Uses trailing underscores for class members.

3. **components/cdc_hal/include/cdc_hal/ISecureElement.h** (line 163):
   ```cpp
   struct __attribute__((packed)) RMemHeader {
       uint8_t magic;
       uint8_t checksum;
       uint8_t moduleId;
       uint8_t flags;
       char name[RMEM_NAME_LEN];
       uint16_t payloadLen;
   };
   ```
   Struct members use NO trailing underscores (correct for pure data structs).

4. **components/mod_password/include/mod_password/PasswordStore.h** (lines 63-67):
   ```cpp
   private:
       bool hasSlotRange_ = false;
       uint16_t rmemStart_ = 0;
       uint16_t rmemEnd_ = 0;
       uint8_t moduleId_ = 0;
   ```
   Uses trailing underscores.

5. **components/cdc_core/include/cdc_core/PinManager.h** (lines 112-130):
   ```cpp
   private:
       uint8_t badgeHash_[BADGE_HASH_SIZE] = {};
       uint8_t badgeRetries_ = MAX_RETRIES;
       uint32_t iterations_ = DEFAULT_ITERATIONS;
       uint8_t pw1Salt_[SALT_SIZE] = {};
       uint8_t pw3Salt_[SALT_SIZE] = {};
       uint8_t pw1Hash_[KDF_HASH_SIZE] = {};
       uint8_t pw3Hash_[KDF_HASH_SIZE] = {};
       uint8_t pw1Retries_ = MAX_RETRIES;
       uint8_t pw3Retries_ = MAX_RETRIES;
       bool pinLoaded_ = false;
       bool badgePinIsSet_ = false;
       uint32_t lockoutStartMs_ = 0;
       bool lockoutActive_ = false;
   ```
   Uses trailing underscores.

## Impact
- **Consistency**: The codebase is actually fairly consistent with trailing underscores for class members
- **Struct vs Class**: Pure data structs (like `RMemHeader`) correctly don't use underscores
- **Minor issue**: This is actually well-handled in most places

## Evidence
The pattern is mostly consistent:
- **Class members**: Trailing underscore (`title_`, `items_`, `selection_`)
- **Struct data members**: No trailing underscore (`magic`, `checksum`, `moduleId`)
- **Local variables**: No trailing underscore

## Recommended Fix
The codebase already follows a good pattern. The only issue is documenting this convention:

**Document the convention:**
1. Class/struct member variables: Use trailing underscore (`name_`)
2. Pure data structs (POD): No trailing underscore needed
3. Local variables: No trailing underscore
4. Parameters: No trailing underscore (to distinguish from members)

**Steps:**
1. Add a comment to the style guide documenting this convention
2. Ensure new code follows the established pattern
3. No major refactoring needed - the convention is already well-applied

## References
- [Google C++ Style Guide - Nonmember, Member, and Local Variables](https://google.github.io/styleguide/cppguide.html#Nonmember_Member_and_Local_Variables)
