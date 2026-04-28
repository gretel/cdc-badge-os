---
title: "[LOW] IKeyboardProvider::getStatusText return value not documented"
severity: LOW
domain: documentation/code-docs
lens: code-docs
labels:
  - "audit:documentation/code-docs"
---

## Summary

The `IKeyboardProvider::getStatusText()` method in `components/cdc_core/include/cdc_core/IKeyboardProvider.h:58-61` has documentation for the return value but the inline implementation shows it returns different strings than documented.

**File:** `components/cdc_core/include/cdc_core/IKeyboardProvider.h:58-61`

## Impact

- Developer may expect more detailed status (e.g., "Connected to MacBook Pro")
- Implementation is simpler than documented

## Evidence

```cpp
// Line 58-61: Documentation doesn't match implementation
/**
 * \brief Get human-readable connection status for UI
 * \return Status string (e.g., "Connected to MacBook Pro")
 */
virtual const char* getStatusText() const { return isConnected() ? "Connected" : "Disconnected"; }
```

The documentation says it returns "Connected to MacBook Pro" style strings, but the actual implementation just returns "Connected" or "Disconnected".

## Recommended Fix

Update documentation to match implementation:

```cpp
/**
 * \brief Get human-readable connection status for UI.
 * \return Status string ("Connected" or "Disconnected").
 */
virtual const char* getStatusText() const { return isConnected() ? "Connected" : "Disconnected"; }
```

Or, if more detailed status is desired, enhance the implementation:

```cpp
/**
 * \brief Get human-readable connection status for UI.
 * \return Status string (e.g., "Connected", "Disconnected").
 *
 * Subclasses may override to provide more detailed status.
 */
virtual const char* getStatusText() const { return isConnected() ? "Connected" : "Disconnected"; }
```

## References

- Related: `getStatusText()` is used in UI for keyboard status display
