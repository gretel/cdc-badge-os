/**
 * \file
 * \brief View-stack implementation with recursive mutex.
 *
 * The ViewStack is touched from multiple FreeRTOS contexts (UI task, USB
 * CTAP-HID task, BLE callback task) so the underlying storage needs a
 * synchronization primitive. The public API is reentrant: view callbacks
 * fired from inside dispatchKey/dispatchLongPress/dispatchTick legitimately
 * call back into push/pop/showModal/hideModal on the same task. A
 * FreeRTOS recursive mutex is therefore required - a plain mutex would
 * self-deadlock the UI task as soon as the first key is dispatched.
 */

#include "cdc_ui/ViewStack.h"
#include "cdc_hal/IDisplay.h"
#include "cdc_log.h"
#include <cstring>

static const char* TAG = "ViewStack";

namespace cdc::ui {

/**
 * \brief Checks whether a view is a `ListView` by runtime name.
 */
static bool isListView(const IView* view) {
    return view && (std::strcmp(view->getName(), "ListView") == 0);
}

/**
 * \brief Returns singleton view-stack instance.
 */
ViewStack& ViewStack::instance() {
    static ViewStack instance;
    instance.ensureMutex();
    return instance;
}

/**
 * \brief Lazily creates the FreeRTOS mutex on first access.
 */
void ViewStack::ensureMutex() {
    if (!mutex_) {
        mutex_ = xSemaphoreCreateRecursiveMutex();
    }
}

namespace {

/** \brief Scoped lock guard for the recursive ViewStack mutex. */
class StackLock {
public:
    explicit StackLock(SemaphoreHandle_t m) : m_(m) {
        if (m_) xSemaphoreTakeRecursive(m_, portMAX_DELAY);
    }
    ~StackLock() {
        if (m_) xSemaphoreGiveRecursive(m_);
    }
    StackLock(const StackLock&) = delete;
    StackLock& operator=(const StackLock&) = delete;
private:
    SemaphoreHandle_t m_;
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// Unlocked helpers. Caller MUST hold mutex_.
// ---------------------------------------------------------------------------

void ViewStack::push_unlocked(IView* view, void* context) {
    if (!view) {
        LOG_W(TAG, "Attempted to push null view");
        return;
    }
    if (exclusiveOwner_ && exclusiveOwner_ != view) {
        LOG_W(TAG, "push('%s') blocked: exclusive lock held by %p",
              view->getName(), exclusiveOwner_);
        return;
    }
    if (depth_ >= MAX_DEPTH) {
        LOG_E(TAG, "ViewStack overflow (max %d)", MAX_DEPTH);
        return;
    }

    stack_[depth_++] = view;
    view->onEnter(context);
    needsFullRefresh_ = !(isListView(view) && isListView(depth_ > 1 ? stack_[depth_ - 2] : nullptr));

    LOG_D(TAG, "Pushed view '%s' (depth=%d)", view->getName(), depth_);
}

void ViewStack::pop_unlocked() {
    if (depth_ <= 1) {
        LOG_W(TAG, "Cannot pop root view");
        return;
    }

    IView* top = stack_[depth_ - 1];
    if (exclusiveOwner_ && exclusiveOwner_ != top) {
        LOG_W(TAG, "pop blocked: exclusive lock held by %p (top='%s')",
              exclusiveOwner_, top ? top->getName() : "(null)");
        return;
    }

    depth_--;
    if (top) {
        top->onExit();
        LOG_D(TAG, "Popped view '%s' (depth=%d)", top->getName(), depth_);
    }
    stack_[depth_] = nullptr;

    if (depth_ > 0 && stack_[depth_ - 1]) {
        stack_[depth_ - 1]->onResume();
    }
    needsFullRefresh_ = !(isListView(stack_[depth_ - 1]) && isListView(top));
}

void ViewStack::hideModal_unlocked() {
    if (modal_) {
        LOG_D(TAG, "Hiding modal '%s'", modal_->getName());
        modal_->onExit();
        modal_ = nullptr;

        IView* view = (depth_ == 0) ? nullptr : stack_[depth_ - 1];
        if (view) {
            view->markDirty();
        }
    }
}

// ---------------------------------------------------------------------------
// Public API. Each entry point acquires the mutex once.
// ---------------------------------------------------------------------------

void ViewStack::push(IView* view, void* context) {
    StackLock lock(mutex_);
    push_unlocked(view, context);
}

void ViewStack::pop() {
    StackLock lock(mutex_);
    pop_unlocked();
}

void ViewStack::replace(IView* view, void* context) {
    StackLock lock(mutex_);
    if (!view) {
        LOG_W(TAG, "Attempted to replace with null view");
        return;
    }
    if (depth_ == 0) {
        push_unlocked(view, context);
        return;
    }

    IView* top = stack_[depth_ - 1];
    if (exclusiveOwner_ && exclusiveOwner_ != view && exclusiveOwner_ != top) {
        LOG_W(TAG, "replace('%s') blocked: exclusive lock held by %p",
              view->getName(), exclusiveOwner_);
        return;
    }

    if (top) {
        top->onExit();
        LOG_D(TAG, "Replaced view '%s'", top->getName());
    }

    stack_[depth_ - 1] = view;
    view->onEnter(context);
    needsFullRefresh_ = !(isListView(view) && isListView(top));

    LOG_D(TAG, "Replaced with view '%s'", view->getName());
}

void ViewStack::popToRoot() {
    StackLock lock(mutex_);
    while (depth_ > 1) {
        pop_unlocked();
    }
}

void ViewStack::popToAnchor(IView* anchor) {
    StackLock lock(mutex_);
    while (depth_ > 1) {
        IView* cur = stack_[depth_ - 1];
        if (cur == anchor) break;
        pop_unlocked();
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
    StackLock lock(mutex_);
    resetInactivityTimer();

    if (modal_) {
        InputResult result = modal_->onKey(key);
        if (result == InputResult::REQUEST_POP) {
            hideModal_unlocked();
        }
        return;
    }

    IView* view = (depth_ == 0) ? nullptr : stack_[depth_ - 1];
    if (view) {
        InputResult result = view->onKey(key);
        if (result == InputResult::REQUEST_POP) {
            pop_unlocked();
        }
    }
}

void ViewStack::dispatchLongPress(char key) {
    StackLock lock(mutex_);

    if (key == 'N') {
        if (modal_) {
            hideModal_unlocked();
        } else if (depth_ > 1) {
            pop_unlocked();
        }
        return;
    }

    if (modal_) {
        InputResult result = modal_->onLongPress(key);
        if (result == InputResult::REQUEST_POP) {
            hideModal_unlocked();
        }
        return;
    }

    IView* view = (depth_ == 0) ? nullptr : stack_[depth_ - 1];
    if (view) {
        InputResult result = view->onLongPress(key);
        if (result == InputResult::REQUEST_POP) {
            pop_unlocked();
        }
    }
}

void ViewStack::dispatchTick(uint32_t nowMs) {
    StackLock lock(mutex_);
    if (modal_) {
        modal_->onTick(nowMs);
    }
    IView* view = (depth_ == 0) ? nullptr : stack_[depth_ - 1];
    if (view) {
        view->onTick(nowMs);
    }
}

void ViewStack::render() {
    StackLock lock(mutex_);
    IView* view = (depth_ == 0) ? nullptr : stack_[depth_ - 1];
    if (!view) {
        return;
    }

    bool modalNeedsRender = modal_ && modal_->needsRender();
    bool viewNeedsRender = view->needsRender();

    if (!viewNeedsRender && !modalNeedsRender) {
        return;
    }

    if (viewNeedsRender) {
        view->render(false);
    }
    if (modal_ && modalNeedsRender) {
        modal_->render(true);
    }

    hal::RefreshMode mode = needsFullRefresh_ ? hal::RefreshMode::FULL : hal::RefreshMode::PARTIAL;
    hal::IDisplay* display = hal::getDisplayInstance();
    if (display) {
        display->flush(mode);
    }
    needsFullRefresh_ = false;
}

bool ViewStack::needsRender() const {
    StackLock lock(mutex_);
    if (modal_ && modal_->needsRender()) return true;
    IView* view = (depth_ == 0) ? nullptr : stack_[depth_ - 1];
    return view && view->needsRender();
}

void ViewStack::showModal(IView* modal) {
    StackLock lock(mutex_);
    if (exclusiveOwner_) {
        LOG_W(TAG, "showModal('%s') blocked: exclusive lock held by %p",
              modal ? modal->getName() : "(null)", exclusiveOwner_);
        return;
    }
    if (modal_) {
        modal_->onExit();
    }
    modal_ = modal;
    if (modal_) {
        modal_->onEnter(nullptr);
        LOG_D(TAG, "Showing modal '%s'", modal_->getName());
    }
}

void ViewStack::hideModal() {
    StackLock lock(mutex_);
    hideModal_unlocked();
}

void ViewStack::setInactivityTimeout(InactivityCallback callback, uint32_t timeoutMs) {
    inactivityCallback_ = callback;
    inactivityTimeoutMs_ = timeoutMs;
    lastActivityMs_ = 0;
    LOG_D(TAG, "Inactivity timeout set: %lu ms", timeoutMs);
}

void ViewStack::resetInactivityTimer() {
    lastActivityMs_ = 0;
}

void ViewStack::checkInactivity(uint32_t nowMs) {
    if (inactivityTimeoutMs_ == 0 || !inactivityCallback_) {
        return;
    }
    if (lastActivityMs_ == 0) {
        lastActivityMs_ = nowMs;
        return;
    }
    uint32_t elapsed = nowMs - lastActivityMs_;
    if (elapsed >= inactivityTimeoutMs_) {
        LOG_I(TAG, "Inactivity timeout triggered after %lu ms", elapsed);
        inactivityCallback_();
        lastActivityMs_ = nowMs;
    }
}

bool ViewStack::acquireExclusive(const void* owner) {
    StackLock lock(mutex_);
    if (!owner) {
        LOG_W(TAG, "acquireExclusive called with null owner");
        return false;
    }
    if (exclusiveOwner_ && exclusiveOwner_ != owner) {
        LOG_W(TAG, "Exclusive lock already held by %p, refusing %p", exclusiveOwner_, owner);
        return false;
    }
    exclusiveOwner_ = owner;
    LOG_D(TAG, "Exclusive lock acquired by %p", owner);
    return true;
}

bool ViewStack::releaseExclusive(const void* owner) {
    StackLock lock(mutex_);
    if (!exclusiveOwner_) {
        return false;
    }
    if (exclusiveOwner_ != owner) {
        LOG_W(TAG, "releaseExclusive: owner mismatch (held=%p, caller=%p)",
              exclusiveOwner_, owner);
        return false;
    }
    LOG_D(TAG, "Exclusive lock released by %p", owner);
    exclusiveOwner_ = nullptr;
    return true;
}

} // namespace cdc::ui
