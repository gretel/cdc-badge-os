#pragma once

#include "cdc_core/IService.h"
#include <cstdint>

namespace cdc::ui {
    class IView;
}

namespace cdc::core {

/**
 * Menu location for module registration
 */
enum class MenuLocation : uint8_t {
    MAIN_MENU,      // Top-level main menu
    TOOLS_MENU,     // Under Tools submenu
    SETTINGS_MENU   // Under Settings submenu
};

/**
 * Menu item registered by a module
 */
struct ModuleMenuItem {
    const char* label;              // Display label (use I18n for translation)
    uint8_t priority;               // Sort order (lower = higher in list)
    ui::IView* (*getView)();        // Factory function to get the view
    bool (*isVisible)();            // Optional visibility check (nullptr = always visible)
    const char* moduleName;         // Owner module name (set automatically)
    MenuLocation location;          // Where to show this item
};

/**
 * Lock screen context menu item registered by a module
 */
struct LockScreenContextItem {
    const char* (*getLabel)();      // Dynamic label getter (for state-dependent text)
    void (*callback)();             // Action when selected
    uint8_t priority;               // Sort order (lower = higher in list)
    const char* moduleName;         // Owner module name (set automatically)
};

/**
 * Module interface - extends IService with module-specific features
 *
 * Modules are self-contained features (TOTP, FIDO2, Password, etc.)
 * that can register menu items, serial commands, and views.
 */
class IModule : public IService {
public:
    struct SlotRequest {
        const char* mapName = nullptr;
        uint8_t minEccSlots = 0;
        uint16_t minRmemSlots = 0;
    };

    struct SlotRange {
        bool hasEcc = false;
        bool hasRmem = false;
        uint8_t eccStart = 0;
        uint8_t eccEnd = 0;
        uint16_t rmemStart = 0;
        uint16_t rmemEnd = 0;
        uint8_t moduleId = 0;
    };

    /**
     * Get module version string
     */
    virtual const char* getVersion() const = 0;

    /**
     * Get module menu items
     * @param items Output array to fill
     * @param maxItems Maximum items to return
     * @return Number of items written
     */
    virtual uint8_t getMenuItems(ModuleMenuItem* items, uint8_t maxItems) {
        (void)items; (void)maxItems;
        return 0;
    }

    /**
     * Get module's entry view (main view when selected from menu)
     * @return View instance or nullptr if no main view
     */
    virtual ui::IView* getEntryView() { return nullptr; }

    /**
     * Get module's lock screen context menu items
     * @param items Output array to fill
     * @param maxItems Maximum items to return
     * @return Number of items written
     */
    virtual uint8_t getLockScreenContextItems(LockScreenContextItem* items, uint8_t maxItems) {
        (void)items; (void)maxItems;
        return 0;
    }

    /**
     * Called when device is unlocked
     */
    virtual void onUnlock() {}

    /**
     * Called when device is locked
     */
    virtual void onLock() {}

    /**
     * Called when USB is connected
     */
    virtual void onUsbConnect() {}

    /**
     * Called when USB is disconnected
     */
    virtual void onUsbDisconnect() {}

    /**
     * Called periodically (optional tick for background work)
     * @param nowMs Current timestamp in milliseconds
     */
    virtual void onTick(uint32_t nowMs) { (void)nowMs; }

    /**
     * Slot range assigned by module registry (from compile-time memory map)
     */
    virtual void setSlotRange(const SlotRange& range) { (void)range; }

    /**
     * Slot requirements for this module (from compile-time memory map)
     */
    virtual SlotRequest getSlotRequest() const { return {}; }
};

} // namespace cdc::core
