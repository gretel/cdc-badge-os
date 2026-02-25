/**
 * \file
 * \brief Expert UI menus including module control and TROPIC maintenance actions.
 */

#include "AppUiInternal.h"
#include "cdc_os_ui/AppUi.h"
#include "cdc_os_ui/HardwareInfo.h"
#include "cdc_core/TropicStorage.h"
#include "cdc_core/UsbManager.h"
#include "cdc_core/EventBus.h"

#include <cstdio>
#include <cstring>

namespace cdc::ui {

/** \brief Expert menu sizing constants. */

static constexpr uint8_t EXPERT_FIXED_COUNT = 3;
static constexpr uint8_t EXPERT_MAX_ITEMS = 12;
static constexpr uint8_t MODULES_VIEW_MAX = 16;

/** \brief Static view pointers and menu item storage for expert/module views. */

static ListView* s_expertMenu = nullptr;
static ListItem s_expertItems[EXPERT_MAX_ITEMS];
static core::ModuleMenuItem s_expertModuleItems[EXPERT_MAX_ITEMS - EXPERT_FIXED_COUNT];
static uint8_t s_expertModuleCount = 0;

static ListView* s_modulesView = nullptr;
static ListItem s_modulesItems[MODULES_VIEW_MAX];
static char s_moduleLabels[MODULES_VIEW_MAX][48];

/** \brief Rebuilds module status list view content. */
static void rebuildModulesView();

/**
 * \brief Retries failed module initialization after user confirmation.
 * \param userData Encoded module index.
 */
static void onModuleRetryConfirm(void* userData) {
    uint8_t index = static_cast<uint8_t>(reinterpret_cast<uintptr_t>(userData));
    auto& moduleReg = core::ModuleRegistry::instance();

    if (moduleReg.retryModule(index)) {
        showToastSuccess("OK", TOAST_DURATION_SHORT_MS);
    } else {
        const char* error = moduleReg.getModuleSlotError(index);
        showToastError(error ? error : tr(StringId::FAILED), TOAST_DURATION_MEDIUM_MS);
    }

    ui_rebuild_menus();
    rebuildModulesView();
}

/**
 * \brief Handles module list selection for retry/toggle behavior.
 * \param index Selected module index.
 * \param userData Optional callback user data.
 */
static void onModuleSelect(uint16_t index, void* userData) {
    (void)userData;

    auto& moduleReg = core::ModuleRegistry::instance();
    if (index >= moduleReg.getModuleCount()) return;

    core::IModule* module = moduleReg.getModuleAt(index);
    if (!module) return;

    uint8_t idx = static_cast<uint8_t>(index);

    // If module has error, show retry dialog
    if (moduleReg.hasModuleSlotError(idx)) {
        const char* error = moduleReg.getModuleSlotError(idx);

        static char confirmMsg[128];
        snprintf(confirmMsg, sizeof(confirmMsg), "%s\n\nNochmal laden?",
                 error ? error : "Modul-Fehler");

        showConfirm(confirmMsg, onModuleRetryConfirm, nullptr,
                    ConfirmView::Icon::ERROR, reinterpret_cast<void*>(static_cast<uintptr_t>(idx)));
        return;
    }

    // Remember USB state before toggle
    bool needsReplugBefore = core::UsbManager::instance().needsReplug();

    // Normal toggle: enable/disable module
    bool nowEnabled = moduleReg.toggleModuleEnabled(idx);

    if (nowEnabled) {
        if (!moduleReg.startModule(idx)) {
            const char* error = moduleReg.getModuleSlotError(idx);
            if (error) {
                showToastError(error, TOAST_DURATION_MEDIUM_MS);
            } else {
                showToastError(tr(StringId::FAILED), TOAST_DURATION_MEDIUM_MS);
            }
        }
    } else {
        if (module->getState() == core::ServiceState::STARTED) {
            module->stop();
        }
    }

    // If USB config changed by THIS module toggle, show sticky alert
    bool needsReplugAfter = core::UsbManager::instance().needsReplug();
    if (!needsReplugBefore && needsReplugAfter) {
        showToastAlertSticky(tr(StringId::USB_REPLUG_REQUIRED));
    }

    ui_rebuild_menus();
    rebuildModulesView();
}

/**
 * \brief Rebuilds module list rows with current enabled/state/error markers.
 */
static void rebuildModulesView() {
    auto& moduleReg = core::ModuleRegistry::instance();
    uint8_t count = moduleReg.getModuleCount();

    if (count == 0) {
        s_modulesItems[0] = {"(none)", 0, false, nullptr};
        count = 1;
    } else {
        for (uint8_t i = 0; i < count && i < MODULES_VIEW_MAX; i++) {
            core::IModule* module = moduleReg.getModuleAt(i);
            if (module) {
                const char* status;
                if (moduleReg.hasModuleSlotError(i)) {
                    status = "[FAIL]";
                } else {
                    bool enabled = moduleReg.isModuleEnabled(i);
                    status = enabled
                        ? (module->getState() == core::ServiceState::STARTED ? "[ON]" : "[--]")
                        : "[OFF]";
                }
                snprintf(s_moduleLabels[i], sizeof(s_moduleLabels[i]),
                         "%s %s", module->getName(), status);
                s_modulesItems[i] = {s_moduleLabels[i], 0, false, nullptr};
            }
        }
        if (count > MODULES_VIEW_MAX) count = MODULES_VIEW_MAX;
    }

    if (s_modulesView) {
        s_modulesView->init(tr(StringId::MODULES), s_modulesItems, count);
    }
}

/**
 * \brief Shows module management list view.
 */
void showModulesView() {
    if (!s_modulesView) {
        s_modulesView = new ListView();
    }

    rebuildModulesView();
    s_modulesView->setOnSelect(onModuleSelect);
    ViewStack::instance().push(s_modulesView);
}

/**
 * \brief Opens hardware information/system-test screen.
 */
static void runSystemTest() {
    showHardwareInfo();
}

/**
 * \brief Rebuilds cached TROPIC metadata and reports operation result.
 */
static void runTropicCacheRebuild() {
    showToastTask(tr(StringId::TASK_WORKING), 0);
    bool ok = core::TropicStorage::instance().rebuild();
    ViewStack::instance().hideModal();
    if (ok) {
        showToastSuccess(tr(StringId::OK));
    } else {
        showToastError(tr(StringId::FAILED));
    }
}

/**
 * \brief Cleans cached TROPIC metadata and reports operation result.
 */
static void runTropicCacheCleanup() {
    showToastTask(tr(StringId::TASK_WORKING), 0);
    bool ok = core::TropicStorage::instance().cleanup();
    ViewStack::instance().hideModal();
    if (ok) {
        showToastSuccess(tr(StringId::OK));
    } else {
        showToastError(tr(StringId::FAILED));
    }
}

/** \brief Rebuilds expert menu item list including module-provided entries. */
static void rebuildExpertMenu();
/** \brief Handles expert menu selection actions. */
static void onExpertMenuSelect(uint16_t index, void* userData);

/**
 * \brief Shows expert menu and initial warning toast.
 */
void showExpertMenu() {
    showToastInfo(tr(StringId::EXPERT_WARNING), TOAST_DURATION_MEDIUM_MS);
    if (!s_expertMenu) {
        s_expertMenu = new ListView();
        s_expertMenu->setOnSelect(onExpertMenuSelect);
    }

    rebuildExpertMenu();
    ViewStack::instance().push(s_expertMenu);
}

/**
 * \brief Rebuilds expert menu entries including dynamically provided items.
 */
static void rebuildExpertMenu() {
    auto& moduleReg = core::ModuleRegistry::instance();

    s_expertItems[0] = {tr(StringId::HARDWARE_INFO), 0, false, nullptr};
    s_expertItems[1] = {tr(StringId::TR01_CACHE_REBUILD), 0, false, nullptr};
    s_expertItems[2] = {tr(StringId::TR01_CACHE_CLEANUP), 0, false, nullptr};

    s_expertModuleCount = moduleReg.getMenuItems(
        core::MenuLocation::EXPERT_MENU,
        s_expertModuleItems,
        EXPERT_MAX_ITEMS - EXPERT_FIXED_COUNT
    );

    for (uint8_t i = 0; i < s_expertModuleCount; i++) {
        const auto& item = s_expertModuleItems[i];
        if (item.isVisible && !item.isVisible()) continue;

        s_expertItems[EXPERT_FIXED_COUNT + i] = {
            item.label,
            0,
            false,
            nullptr
        };
    }

    uint8_t totalCount = EXPERT_FIXED_COUNT + s_expertModuleCount;
    s_expertMenu->init(tr(StringId::EXPERT), s_expertItems, totalCount);
}

/**
 * \brief Handles selected expert-menu action.
 * \param index Selected menu item index.
 * \param userData Optional callback user data.
 */
static void onExpertMenuSelect(uint16_t index, void* userData) {
    (void)userData;

    if (index < EXPERT_FIXED_COUNT) {
        switch (index) {
            case 0: runSystemTest(); break;
            case 1: runTropicCacheRebuild(); break;
            case 2: runTropicCacheCleanup(); break;
        }
        return;
    }

    uint8_t moduleIdx = index - EXPERT_FIXED_COUNT;
    if (moduleIdx < s_expertModuleCount) {
        const auto& item = s_expertModuleItems[moduleIdx];
        if (item.getView) {
            IView* view = item.getView();
            if (view) {
                ViewStack::instance().push(view);
            }
        }
    }
}

/**
 * \brief Displays toast notification for module error events.
 * \param evt Event payload from module registry.
 */
void onModuleErrorEvent(const core::Event& evt) {
    if (evt.type != core::EventType::MODULE_ERROR) return;

    auto& moduleReg = core::ModuleRegistry::instance();
    uint8_t index = static_cast<uint8_t>(evt.data.value);

    if (index >= moduleReg.getModuleCount()) return;

    const char* error = moduleReg.getModuleSlotError(index);
    core::IModule* module = moduleReg.getModuleAt(index);
    const char* name = module ? module->getName() : "?";

    static char errMsg[96];
    snprintf(errMsg, sizeof(errMsg), "%s: %s", name, error ? error : "Fehler");

    showToastError(errMsg, TOAST_DURATION_LONG_MS);
}

} // namespace cdc::ui
