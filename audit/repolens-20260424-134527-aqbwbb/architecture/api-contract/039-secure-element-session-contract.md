---
title: "[MEDIUM] ISecureElement Session Management Contract Missing Timeout Handling"
severity: MEDIUM
domain: API Contract Integrity
lens: session-contracts
labels:
  - "audit:architecture/api-contract"
---

## Summary
`ISecureElement` interface defines session management (`sessionStart()`, `sessionEnd()`, `isSessionActive()`) but doesn't specify timeout behavior, session duration limits, or what happens when multiple modules request sessions simultaneously.

**Location**: `components/cdc_hal/include/cdc_hal/ISecureElement.h:51-72` (session management methods)

## Impact
1. **Session starvation**: Long-running operations may hold session indefinitely
2. **Race conditions**: Multiple modules may conflict over session ownership
3. **Resource leaks**: Sessions may never be released if module crashes

## Evidence

In `components/cdc_hal/include/cdc_hal/ISecureElement.h:51-72`:
```cpp
// === Session Management ===

/**
 * Start secure session (required before operations)
 */
virtual bool sessionStart() = 0;

/**
 * End secure session
 */
virtual void sessionEnd() = 0;

/**
 * Check if session is active
 */
virtual bool isSessionActive() const = 0;

/**
 * Put chip to sleep
 */
virtual void sleep() = 0;
```

**No documentation on**:
- How long does a session last?
- What happens if `sessionStart()` is called twice?
- Can multiple modules hold sessions simultaneously?
- What's the timeout for idle sessions?
- Who owns the session (global or per-module)?

In `components/cdc_hal/src/Tropic01Element.cpp:192-215`:
```cpp
/**
 * \brief Opens a secure session with the TROPIC01 chip.
 * \return `true` on success, otherwise `false`.
 */
bool Tropic01Element::sessionStart() {
    lock();

    if (sessionActive_) {
        unlock();
        return true;  // <-- Silent success if already active
    }

    lt_ret_t ret = lt_dev_configure(&handle_);
    if (ret != LT_OK) {
        unlock();
        handleSessionError(ret);
        return false;
    }

    ret = lt_l2_pair(&handle_);
    if (ret != LT_OK) {
        // ...
    }

    sessionActive_ = true;
    unlock();
    return true;
}
```

The implementation allows re-entrant `sessionStart()` but doesn't track reference count. If module A calls `sessionStart()` and module B calls `sessionStart()`, then module A calls `sessionEnd()`, module B's session is also ended.

In `components/cdc_hal/src/Tropic01Element.cpp:220-230`:
```cpp
/**
 * \brief Closes secure session and releases chip resources.
 */
void Tropic01Element::sessionEnd() {
    if (!sessionActive_) {
        unlock();
        return;
    }

    lt_dev_sleep(&handle_);
    sessionActive_ = false;
    unlock();
}
```

No reference counting means:
- Module A: `sessionStart()` → sessionActive_ = true
- Module B: `sessionStart()` → sessionActive_ = true (already true, no change)
- Module A: `sessionEnd()` → sessionActive_ = false (B's session lost!)
- Module B: tries to use chip → fails

## Recommended Fix

1. **Add reference counting** to track session ownership:
   ```cpp
   class ISecureElement {
   public:
       /**
        * \brief Start secure session (reference counted).
        * \return true on success.
        * \note Multiple calls increment reference count.
        */
       virtual bool sessionStart() = 0;

       /**
        * \brief End secure session (reference counted).
        * \note Only releases session when count reaches zero.
        */
       virtual void sessionEnd() = 0;

       /**
        * \brief Get session reference count.
        * \return Number of active session holders.
        */
       virtual uint8_t getSessionRefCount() const = 0;
   };
   ```

2. **Add session timeout** configuration:
   ```cpp
   /**
    * \brief Set session idle timeout.
    * \param timeoutMs Timeout in milliseconds (0 = no timeout).
    */
   virtual void setSessionTimeout(uint32_t timeoutMs) = 0;

   /**
    * \brief Get session idle timeout.
    * \return Timeout in milliseconds.
    */
   virtual uint32_t getSessionTimeout() const = 0;
   ```

3. **Document session contract** in interface:
   ```cpp
   /**
    * \brief Secure Element interface (TROPIC01).
    *
    * Session Management Contract:
    * - Sessions are reference-counted (call sessionStart()/sessionEnd() in pairs)
    * - Default timeout: 30 seconds idle
    * - Maximum concurrent holders: 1 (exclusive access)
    * - Session survives stop/start of individual operations
    * - Call sessionEnd() when done to release chip resources
    *
    * Usage pattern:
    *   se->sessionStart();
    *   // ... operations ...
    *   se->sessionEnd();
    */
   class ISecureElement : public core::IService {
   ```

4. **Add session guard RAII helper**:
   ```cpp
   /**
    * \brief RAII session guard for automatic session management.
    */
   class SessionGuard {
   public:
       SessionGuard(ISecureElement* se) : se_(se) {
           if (se_) se_->sessionStart();
       }
       ~SessionGuard() {
           if (se_) se_->sessionEnd();
       }
       ISecureElement* get() const { return se_; }
   private:
       ISecureElement* se_;
   };

   // Usage:
   void doOperation() {
       SessionGuard guard(se);
       // ... operations with automatic session cleanup ...
   }
   ```

## References
- `components/cdc_hal/include/cdc_hal/ISecureElement.h:51-72` - Session interface
- `components/cdc_hal/src/Tropic01Element.cpp:192-230` - Session implementation
- `components/mod_gpg/src/GpgModule.cpp:575-587` - Module session usage pattern
