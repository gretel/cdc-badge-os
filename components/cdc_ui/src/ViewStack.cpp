/**
 * ViewStack Implementation
 *
 * Manages navigation between views.
 */

#include "cdc_ui/ViewStack.h"
#include "cdc_hal/IDisplay.h"
#include "cdc_log.h"
#include <cstring>

static const char* TAG = "ViewStack";

namespace cdc::ui {

/**
 * \brief Checks whether a view is a `ListView` by runtime name.
 * \param view View pointer to inspect.
 * \return `true` if view name is `ListView`, otherwise `false`.
 */
static bool isListView(const IView* view) {
    return view && (std::strcmp(view->getName(), "ListView") == 0);
}

/**
 * \brief Returns singleton view-stack instance.
 * \return Reference to global `ViewStack` instance.
 */
ViewStack& ViewStack::instance() {
    static ViewStack instance;
    return instance;
}

/**
 * \brief Pushes a view onto the navigation stack.
 * \param view View to push.
 * \param context Optional context passed to `onEnter`.
 * \return void
 */
void ViewStack::push(IView* view, void* context) {
    if (!view) {
        LOG_W(TAG, "Attempted to push null view");
        return;
    }

    if (depth_ >= MAX_DEPTH) {
        LOG_E(TAG, "ViewStack overflow (max %d)", MAX_DEPTH);
        return;
    }

    // Call onExit on current view (it's being hidden)
    if (depth_ > 0 && stack_[depth_ - 1]) {
        // Don't call onExit - view stays in stack
    }

    // Push new view
    stack_[depth_++] = view;
    view->onEnter(context);
    // ListView-to-ListView transitions don't require full refresh
    needsFullRefresh_ = !(isListView(view) && isListView(depth_ > 1 ? stack_[depth_ - 2] : nullptr));

    LOG_D(TAG, "Pushed view '%s' (depth=%d)", view->getName(), depth_);
}

/**
 * \brief Pops the top view from the navigation stack.
 * \return void
 */
void ViewStack::pop() {
    if (depth_ <= 1) {
        LOG_W(TAG, "Cannot pop root view");
        return;
    }

    // Remove top view
    IView* top = stack_[--depth_];
    if (top) {
        top->onExit();
        LOG_D(TAG, "Popped view '%s' (depth=%d)", top->getName(), depth_);
    }
    stack_[depth_] = nullptr;

    // Resume previous view
    if (depth_ > 0 && stack_[depth_ - 1]) {
        stack_[depth_ - 1]->onResume();
    }
    // ListView-to-ListView transitions don't require full refresh
    needsFullRefresh_ = !(isListView(stack_[depth_ - 1]) && isListView(top));
}

/**
 * \brief Replaces the current top view.
 * \param view Replacement view.
 * \param context Optional context passed to `onEnter`.
 * \return void
 */
void ViewStack::replace(IView* view, void* context) {
    if (!view) {
        LOG_W(TAG, "Attempted to replace with null view");
        return;
    }

    if (depth_ == 0) {
        // Stack is empty, just push
        push(view, context);
        return;
    }

    // Exit current view
    IView* top = stack_[depth_ - 1];
    if (top) {
        top->onExit();
        LOG_D(TAG, "Replaced view '%s'", top->getName());
    }

    // Replace with new view
    stack_[depth_ - 1] = view;
    view->onEnter(context);
    // ListView-to-ListView transitions don't require full refresh
    needsFullRefresh_ = !(isListView(view) && isListView(top));

    LOG_D(TAG, "Replaced with view '%s'", view->getName());
}

/**
 * \brief Pops all views until only root remains.
 * \return void
 */
void ViewStack::popToRoot() {
    while (depth_ > 1) {
        pop();
    }
}

/**
 * \brief Returns the current top view.
 * \return Pointer to current view or `nullptr`.
 */
IView* ViewStack::current() const {
    if (depth_ == 0) return nullptr;
    return stack_[depth_ - 1];
}

/**
 * \brief Returns view at stack index.
 * \param idx Stack index.
 * \return Pointer to view at index or `nullptr`.
 */
IView* ViewStack::at(uint8_t idx) const {
    if (idx >= depth_) return nullptr;
    return stack_[idx];
}

/**
 * \brief Dispatches short key presses to modal/current view.
 * \param key Pressed key code.
 * \return void
 */
void ViewStack::dispatchKey(char key) {
    // Reset inactivity timer on any key press
    resetInactivityTimer();

    // Modal gets priority
    if (modal_) {
        InputResult result = modal_->onKey(key);
        if (result == InputResult::REQUEST_POP) {
            hideModal();
        }
        return;
    }

    // Dispatch to current view
    IView* view = current();
    if (view) {
        InputResult result = view->onKey(key);
        if (result == InputResult::REQUEST_POP) {
            pop();
        }
        // REQUEST_PUSH is handled by the view calling push() directly
    }
}

/**
 * \brief Dispatches long-press events with global back behavior on `N`.
 * \param key Long-pressed key code.
 * \return void
 */
void ViewStack::dispatchLongPress(char key) {
    // Long-press N always goes back (universal behavior)
    if (key == 'N') {
        if (modal_) {
            hideModal();
        } else if (depth_ > 1) {
            pop();
        }
        return;
    }

    // Modal gets priority for other keys
    if (modal_) {
        InputResult result = modal_->onLongPress(key);
        if (result == InputResult::REQUEST_POP) {
            hideModal();
        }
        return;
    }

    // Dispatch to current view
    IView* view = current();
    if (view) {
        InputResult result = view->onLongPress(key);
        if (result == InputResult::REQUEST_POP) {
            pop();
        }
    }
}

/**
 * \brief Dispatches periodic tick events to active views.
 * \param nowMs Current monotonic time in milliseconds.
 * \return void
 */
void ViewStack::dispatchTick(uint32_t nowMs) {
    // Tick both modal and current view
    if (modal_) {
        modal_->onTick(nowMs);
    }
    IView* view = current();
    if (view) {
        view->onTick(nowMs);
    }
}

/**
 * \brief Renders current view/modal and flushes display.
 * \return void
 */
void ViewStack::render() {
    IView* view = current();
    if (!view) {
        return;
    }

    // Check if rendering is needed
    bool modalNeedsRender = modal_ && modal_->needsRender();
    bool viewNeedsRender = view->needsRender();

    if (!viewNeedsRender && !modalNeedsRender) {
        return;
    }

    // Render current view (always full, not partial)
    if (viewNeedsRender) {
        view->render(false);
    }

    // Render modal on top
    if (modal_ && modalNeedsRender) {
        modal_->render(true);  // modal is always partial
    }

    // Flush display - FULL refresh after view changes, PARTIAL otherwise
    hal::RefreshMode mode = needsFullRefresh_ ? hal::RefreshMode::FULL : hal::RefreshMode::PARTIAL;
    hal::IDisplay* display = hal::getDisplayInstance();
    if (display) {
        display->flush(mode);
    }
    needsFullRefresh_ = false;  // Reset after flush
}

/**
 * \brief Indicates whether current view or modal requires rendering.
 * \return `true` if rendering is needed.
 */
bool ViewStack::needsRender() const {
    if (modal_ && modal_->needsRender()) return true;
    IView* view = current();
    return view && view->needsRender();
}

/**
 * \brief Shows a modal overlay view.
 * \param modal Modal view pointer.
 * \return void
 */
void ViewStack::showModal(IView* modal) {
    if (modal_) {
        modal_->onExit();
    }
    modal_ = modal;
    if (modal_) {
        modal_->onEnter(nullptr);
        // Modal overlays should not force full refresh
        LOG_D(TAG, "Showing modal '%s'", modal_->getName());
    }
}

/**
 * \brief Hides the current modal overlay.
 * \return void
 */
void ViewStack::hideModal() {
    if (modal_) {
        LOG_D(TAG, "Hiding modal '%s'", modal_->getName());
        modal_->onExit();
        modal_ = nullptr;

        // Mark current view as dirty to redraw
        IView* view = current();
        if (view) {
            view->markDirty();
        }
    }
}

/**
 * \brief Inactivity-timeout handling.
 */

/**
 * \brief Configures inactivity timeout callback.
 * \param callback Callback invoked on timeout.
 * \param timeoutMs Timeout duration in milliseconds.
 * \return void
 */
void ViewStack::setInactivityTimeout(InactivityCallback callback, uint32_t timeoutMs) {
    inactivityCallback_ = callback;
    inactivityTimeoutMs_ = timeoutMs;
    lastActivityMs_ = 0;  // Will be set on first checkInactivity call
    LOG_D(TAG, "Inactivity timeout set: %lu ms", timeoutMs);
}

/**
 * \brief Resets inactivity timer state.
 * \return void
 */
void ViewStack::resetInactivityTimer() {
    lastActivityMs_ = 0;  // Will be updated on next checkInactivity
}

/**
 * \brief Checks and triggers inactivity timeout callback.
 * \param nowMs Current monotonic time in milliseconds.
 * \return void
 */
void ViewStack::checkInactivity(uint32_t nowMs) {
    // Skip if no timeout configured
    if (inactivityTimeoutMs_ == 0 || !inactivityCallback_) {
        return;
    }

    // Initialize on first call
    if (lastActivityMs_ == 0) {
        lastActivityMs_ = nowMs;
        return;
    }

    // Check for timeout
    uint32_t elapsed = nowMs - lastActivityMs_;
    if (elapsed >= inactivityTimeoutMs_) {
        LOG_I(TAG, "Inactivity timeout triggered after %lu ms", elapsed);
        inactivityCallback_();
        lastActivityMs_ = nowMs;  // Reset after callback
    }
}

} // namespace cdc::ui
