#pragma once

#include "IView.h"
#include <cstdint>

namespace cdc::ui {

/**
 * ViewStack - Navigation stack for views
 *
 * Manages a stack of views for hierarchical navigation.
 * Supports push/pop/replace operations and modal overlays.
 *
 * Reference: ~/GIT/cdc-badge-os-legacy/main/app_input.cpp (state machine)
 */
class ViewStack {
public:
    static constexpr uint8_t MAX_DEPTH = 8;

    /**
     * Get singleton instance
     */
    static ViewStack& instance();

    /**
     * Push a view onto the stack
     * @param view View to push
     * @param context Optional context to pass to onEnter
     */
    void push(IView* view, void* context = nullptr);

    /**
     * Pop the top view from stack
     * Does nothing if only one view remains
     */
    void pop();

    /**
     * Replace top view with another
     * @param view New view
     * @param context Optional context
     */
    void replace(IView* view, void* context = nullptr);

    /**
     * Pop all views except root
     */
    void popToRoot();

    /**
     * Get current (top) view
     */
    IView* current() const;

    /**
     * Get view at specific depth (0 = root)
     */
    IView* at(uint8_t depth) const;

    /**
     * Get current stack depth
     */
    uint8_t depth() const { return depth_; }

    /**
     * Check if stack is empty
     */
    bool isEmpty() const { return depth_ == 0; }

    // === Input dispatch ===

    /**
     * Dispatch key press to current view
     * @param key Key character
     */
    void dispatchKey(char key);

    /**
     * Dispatch long press to current view
     * @param key Key character
     */
    void dispatchLongPress(char key);

    /**
     * Dispatch tick to current view and modal
     * @param nowMs Current time
     */
    void dispatchTick(uint32_t nowMs);

    // === Rendering ===

    /**
     * Render current view (and modal if present)
     * Flushes to display automatically
     */
    void render();

    /**
     * Check if any view needs rendering
     */
    bool needsRender() const;

    // === Modal support ===

    /**
     * Show a modal overlay (e.g., toast, context menu)
     * @param modal Modal view
     */
    void showModal(IView* modal);

    /**
     * Hide current modal
     */
    void hideModal();

    /**
     * Check if modal is active
     */
    bool hasModal() const { return modal_ != nullptr; }

    /**
     * Get modal view
     */
    IView* getModal() const { return modal_; }

    /**
     * Force next render to use FULL refresh
     * (called automatically after view changes)
     */
    void forceFullRefresh() { needsFullRefresh_ = true; }

    // === Inactivity timeout ===

    /**
     * Callback type for inactivity timeout
     */
    using InactivityCallback = void(*)();

    /**
     * Set inactivity timeout callback
     * @param callback Function to call when timeout expires
     * @param timeoutMs Timeout in milliseconds (0 to disable)
     */
    void setInactivityTimeout(InactivityCallback callback, uint32_t timeoutMs);

    /**
     * Reset inactivity timer (called automatically on key press)
     */
    void resetInactivityTimer();

    /**
     * Check and handle inactivity (call in tick/loop)
     * @param nowMs Current time in milliseconds
     */
    void checkInactivity(uint32_t nowMs);

private:
    ViewStack() = default;

    IView* stack_[MAX_DEPTH] = {};
    uint8_t depth_ = 0;
    IView* modal_ = nullptr;
    IView* pendingPush_ = nullptr;
    void* pendingContext_ = nullptr;
    bool needsFullRefresh_ = true;  // True after view changes

    // Inactivity timeout
    InactivityCallback inactivityCallback_ = nullptr;
    uint32_t inactivityTimeoutMs_ = 0;
    uint32_t lastActivityMs_ = 0;
};

} // namespace cdc::ui
