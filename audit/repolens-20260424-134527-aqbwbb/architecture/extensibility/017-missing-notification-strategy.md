---
title: "[MEDIUM] Missing Notification Strategy Pattern"
severity: MEDIUM
domain: architecture/extensibility
lens: notification-system
labels:
  - "audit:architecture/extensibility"
---

## Summary
User notifications (success, error, info messages) are implemented as direct function calls to `showToastSuccess()`, `showToastError()`. There is no strategy pattern to allow different notification types (sound, vibration, LED, BLE indication) or to customize notification behavior per context.

**Files affected:**
- `components/cdc_views/src/ToastView.cpp` - hardcoded toast implementation
- `components/cdc_views/include/cdc_views/ToastView.h` - toast view definition
- All modules calling `showToastSuccess()`, `showToastError()`

## Impact
- **Notification flexibility**: Cannot add new notification channels (sound, vibration, LED) without modifying toast code
- **Context-aware notifications**: No way to customize notification style based on context (e.g., silent mode, USB connected)
- **Testing difficulty**: Hard to mock notifications for unit tests
- **User customization**: No way for users to configure notification preferences

## Evidence

### Hardcoded Toast Functions
`components/cdc_views/src/ToastView.cpp`:
```cpp
void showToastSuccess(const char* text) {
    static InfoView* s_toast = nullptr;
    if (!s_toast) {
        s_toast = new InfoView();
    }
    s_toast->init(text, InfoView::Style::SUCCESS);
    ui::ViewStack::instance().push(s_toast);
}

void showToastError(const char* text) {
    static InfoView* s_toast = nullptr;
    if (!s_toast) {
        s_toast = new InfoView();
    }
    s_toast->init(text, InfoView::Style::ERROR);
    ui::ViewStack::instance().push(s_toast);
}
```

### Direct Calls Throughout Modules
`components/mod_totp/src/TotpModule.cpp:908`:
```cpp
if (ok) {
    ui::showToastSuccess(ui::tr(ui::StringId::OK));
    rebuildList();
    // ...
} else {
    ui::showToastError(ui::tr(ui::StringId::FAILED));
}
```

`components/mod_password/src/PasswordModule.cpp:580`:
```cpp
if (ok) {
    ui::showToastSuccess(mstr(STR_SAVED));
    // ...
} else {
    ui::showToastError(mstr(STR_INVALID_INPUT));
}
```

### No Notification Interface
There is no interface like:
```cpp
// Missing - should exist:
class INotification {
public:
    virtual void show(const char* title, const char* message, Style style) = 0;
    virtual void dismiss() = 0;
};
```

## Recommended Fix

### Create Notification Interface
```cpp
// components/cdc_ui/include/cdc_ui/NotificationManager.h
#pragma once
#include <cstdint>

namespace cdc::ui {

enum class NotificationStyle {
    SUCCESS,
    ERROR,
    INFO,
    WARNING
};

enum class NotificationChannel {
    DISPLAY,    // On-screen toast
    SOUND,      // Beep/tones
    VIBRATE,    // Vibration
    LED,        // LED blink
    BLE         // BLE indication
};

class INotificationStrategy {
public:
    virtual ~INotificationStrategy() = default;
    virtual void show(const char* title, const char* message, NotificationStyle style) = 0;
    virtual void dismiss() = 0;
    virtual bool supportsChannel(NotificationChannel channel) const = 0;
};

class NotificationManager {
public:
    static NotificationManager& instance();
    
    // Register a notification strategy
    void registerStrategy(INotificationStrategy* strategy);
    
    // Show notification with all registered strategies
    void show(const char* title, const char* message, NotificationStyle style, 
              uint32_t channels = 0xFFFFFFFF);  // Bitmask of NotificationChannel
    
    // Set global notification preferences
    void setEnabled(NotificationChannel channel, bool enabled);
    bool isEnabled(NotificationChannel channel) const;
    
    // Convenience methods
    void success(const char* message, uint32_t channels = 0xFFFFFFFF);
    void error(const char* message, uint32_t channels = 0xFFFFFFFF);
    void info(const char* message, uint32_t channels = 0xFFFFFFFF);
};

// Convenience functions (delegate to manager)
void showToastSuccess(const char* text);
void showToastError(const char* text);
void showToastInfo(const char* text);

} // namespace cdc::ui
```

### Implement Strategies
```cpp
// components/cdc_views/src/ToastStrategy.cpp
class ToastStrategy : public INotificationStrategy {
public:
    void show(const char* title, const char* message, NotificationStyle style) override {
        static InfoView* s_toast = nullptr;
        if (!s_toast) {
            s_toast = new InfoView();
        }
        s_toast->init(message, static_cast<InfoView::Style>(style));
        ui::ViewStack::instance().push(s_toast);
    }
    
    void dismiss() override {
        ui::ViewStack::instance().pop();
    }
    
    bool supportsChannel(NotificationChannel channel) const override {
        return channel == NotificationChannel::DISPLAY;
    }
};

// components/mod_ble/src/BleNotificationStrategy.cpp
class BleNotificationStrategy : public INotificationStrategy {
public:
    void show(const char* title, const char* message, NotificationStyle style) override {
        // Send BLE notification with status
        uint8_t code = (style == NotificationStyle::SUCCESS) ? 0x01 : 
                       (style == NotificationStyle::ERROR) ? 0x02 : 0x03;
        ble_notify(code);
    }
    
    void dismiss() override {}
    
    bool supportsChannel(NotificationChannel channel) const override {
        return channel == NotificationChannel::BLE;
    }
};

// components/mod_grove_led/src/LedNotificationStrategy.cpp
class LedNotificationStrategy : public INotificationStrategy {
public:
    void show(const char* title, const char* message, NotificationStyle style) override {
        // Flash LED based on style
        uint32_t color = (style == NotificationStyle::SUCCESS) ? 0x00FF00 :
                         (style == NotificationStyle::ERROR) ? 0xFF0000 : 0x0000FF;
        grove_led_blink(color, 3);
    }
    
    void dismiss() override {}
    
    bool supportsChannel(NotificationChannel channel) const override {
        return channel == NotificationChannel::LED;
    }
};
```

### Refactor Modules to Use Manager
```cpp
// components/mod_totp/src/TotpModule.cpp
// Instead of:
ui::showToastSuccess(ui::tr(ui::StringId::OK));

// Use:
ui::NotificationManager::instance().success(
    ui::tr(ui::StringId::OK),
    (1 << static_cast<uint32_t>(ui::NotificationChannel::DISPLAY)) |
    (1 << static_cast<uint32_t>(ui::NotificationChannel::BLE))
);
```

## References
- Strategy Pattern: https://refactoring.guru/design-patterns/strategy
- Observer Pattern for notifications: https://refactoring.guru/design-patterns/observer
- Command Pattern for notification actions: https://refactoring.guru/design-patterns/command

</content>