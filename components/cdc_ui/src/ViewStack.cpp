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

static bool isListView(const IView* view) {
    return view && (std::strcmp(view->getName(), "ListView") == 0);
}

ViewStack& ViewStack::instance() {
    static ViewStack instance;
    return instance;
}

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

void ViewStack::popToRoot() {
    while (depth_ > 1) {
        pop();
    }
}

IView* ViewStack::current() const {
    if (depth_ == 0) return nullptr;
    return stack_[depth_ - 1];
}

IView* ViewStack::at(uint8_t idx) const {
    if (idx >= depth_) return nullptr;
    return stack_[idx];
}

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

bool ViewStack::needsRender() const {
    if (modal_ && modal_->needsRender()) return true;
    IView* view = current();
    return view && view->needsRender();
}

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

// === Inactivity timeout ===

void ViewStack::setInactivityTimeout(InactivityCallback callback, uint32_t timeoutMs) {
    inactivityCallback_ = callback;
    inactivityTimeoutMs_ = timeoutMs;
    lastActivityMs_ = 0;  // Will be set on first checkInactivity call
    LOG_D(TAG, "Inactivity timeout set: %lu ms", timeoutMs);
}

void ViewStack::resetInactivityTimer() {
    lastActivityMs_ = 0;  // Will be updated on next checkInactivity
}

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
