#include "mod_password/PasswordModule.h"
#include "mod_password/PasswordStore.h"
#include "cdc_core/ModuleRegistry.h"
#include "cdc_core/TropicStorage.h"
#include "cdc_ui/I18n.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_views/ListView.h"
#include "cdc_views/ContextMenuView.h"
#include "cdc_views/T9InputView.h"
#include "cdc_views/InfoView.h"
#include "cdc_views/ConfirmView.h"
#include "cdc_views/ToastView.h"
#include "serial_cmd/ICommandRegistry.h"
#include "serial_cmd/Console.h"
#include "cdc_log.h"
#include <cctype>
#include <cstring>
#include <new>
#include <memory>
#include <cstdio>

static const char* TAG = "PASSWORD";

namespace cdc::mod_password {

// Module-specific string IDs
static uint16_t s_strIdBase = 0;
static constexpr uint16_t STR_PASSWORDS = 0;
static constexpr uint16_t STR_NEW_ENTRY = 1;
static constexpr uint16_t STR_TITLE = 2;
static constexpr uint16_t STR_USERNAME = 3;
static constexpr uint16_t STR_PASSWORD = 4;
static constexpr uint16_t STR_URL = 5;
static constexpr uint16_t STR_TOTP_SLOT = 6;
static constexpr uint16_t STR_NOTES = 7;
static constexpr uint16_t STR_VIEW = 8;
static constexpr uint16_t STR_EDIT = 9;
static constexpr uint16_t STR_DELETE = 10;
static constexpr uint16_t STR_ACTIONS = 11;
static constexpr uint16_t STR_SAVED = 12;
static constexpr uint16_t STR_DELETED = 13;
static constexpr uint16_t STR_INVALID_INPUT = 14;
static constexpr uint16_t STR_SLOT_ERROR = 15;
static constexpr uint16_t STR_DETAILS = 16;
static constexpr uint16_t STR_HINT_LIST = 17;
static constexpr uint16_t STR_CONFIRM_DELETE = 18;
static constexpr uint16_t STR_COUNT = 19;

static const char* mstr(uint16_t offset) {
    return ui::tr(s_strIdBase + offset);
}

static void registerStrings() {
    auto& i18n = ui::I18n::instance();
    s_strIdBase = i18n.registerModule("mod_password", STR_COUNT);
    if (s_strIdBase == 0) {
        LOG_E(TAG, "Failed to register i18n strings");
        return;
    }

    i18n.registerTranslation(s_strIdBase + STR_PASSWORDS, ui::Language::EN, "Passwords");
    i18n.registerTranslation(s_strIdBase + STR_NEW_ENTRY, ui::Language::EN, "New Entry");
    i18n.registerTranslation(s_strIdBase + STR_TITLE, ui::Language::EN, "Title");
    i18n.registerTranslation(s_strIdBase + STR_USERNAME, ui::Language::EN, "Username");
    i18n.registerTranslation(s_strIdBase + STR_PASSWORD, ui::Language::EN, "Password");
    i18n.registerTranslation(s_strIdBase + STR_URL, ui::Language::EN, "URL");
    i18n.registerTranslation(s_strIdBase + STR_TOTP_SLOT, ui::Language::EN, "TOTP Slot (optional)");
    i18n.registerTranslation(s_strIdBase + STR_NOTES, ui::Language::EN, "Notes");
    i18n.registerTranslation(s_strIdBase + STR_VIEW, ui::Language::EN, "View");
    i18n.registerTranslation(s_strIdBase + STR_EDIT, ui::Language::EN, "Edit");
    i18n.registerTranslation(s_strIdBase + STR_DELETE, ui::Language::EN, "Delete");
    i18n.registerTranslation(s_strIdBase + STR_ACTIONS, ui::Language::EN, "Actions");
    i18n.registerTranslation(s_strIdBase + STR_SAVED, ui::Language::EN, "Saved");
    i18n.registerTranslation(s_strIdBase + STR_DELETED, ui::Language::EN, "Deleted");
    i18n.registerTranslation(s_strIdBase + STR_INVALID_INPUT, ui::Language::EN, "Invalid input");
    i18n.registerTranslation(s_strIdBase + STR_SLOT_ERROR, ui::Language::EN, "Slot map error");
    i18n.registerTranslation(s_strIdBase + STR_DETAILS, ui::Language::EN, "Details");
    i18n.registerTranslation(s_strIdBase + STR_HINT_LIST, ui::Language::EN, "[Y] View  [3] Menu  [N] Back");
    i18n.registerTranslation(s_strIdBase + STR_CONFIRM_DELETE, ui::Language::EN, "Delete entry?");

    i18n.registerTranslation(s_strIdBase + STR_PASSWORDS, ui::Language::DE, "Passwoerter");
    i18n.registerTranslation(s_strIdBase + STR_NEW_ENTRY, ui::Language::DE, "Neuer Eintrag");
    i18n.registerTranslation(s_strIdBase + STR_TITLE, ui::Language::DE, "Titel");
    i18n.registerTranslation(s_strIdBase + STR_USERNAME, ui::Language::DE, "Benutzername");
    i18n.registerTranslation(s_strIdBase + STR_PASSWORD, ui::Language::DE, "Passwort");
    i18n.registerTranslation(s_strIdBase + STR_URL, ui::Language::DE, "URL");
    i18n.registerTranslation(s_strIdBase + STR_TOTP_SLOT, ui::Language::DE, "TOTP Slot (optional)");
    i18n.registerTranslation(s_strIdBase + STR_NOTES, ui::Language::DE, "Notizen");
    i18n.registerTranslation(s_strIdBase + STR_VIEW, ui::Language::DE, "Ansehen");
    i18n.registerTranslation(s_strIdBase + STR_EDIT, ui::Language::DE, "Bearbeiten");
    i18n.registerTranslation(s_strIdBase + STR_DELETE, ui::Language::DE, "Loeschen");
    i18n.registerTranslation(s_strIdBase + STR_ACTIONS, ui::Language::DE, "Aktionen");
    i18n.registerTranslation(s_strIdBase + STR_SAVED, ui::Language::DE, "Gespeichert");
    i18n.registerTranslation(s_strIdBase + STR_DELETED, ui::Language::DE, "Geloescht");
    i18n.registerTranslation(s_strIdBase + STR_INVALID_INPUT, ui::Language::DE, "Ungueltige Eingabe");
    i18n.registerTranslation(s_strIdBase + STR_SLOT_ERROR, ui::Language::DE, "Slot-Map Fehler");
    i18n.registerTranslation(s_strIdBase + STR_DETAILS, ui::Language::DE, "Details");
    i18n.registerTranslation(s_strIdBase + STR_HINT_LIST, ui::Language::DE, "[Y] Ansehen  [3] Menu  [N] Zurueck");
    i18n.registerTranslation(s_strIdBase + STR_CONFIRM_DELETE, ui::Language::DE, "Eintrag loeschen?");

    LOG_I(TAG, "Registered i18n strings (base=%d)", s_strIdBase);
}

// === Serial Commands ===

static constexpr const char* CMD_MODULE = "password";
static bool s_commandsRegistered = false;

static const char* skipSpaces(const char* s) {
    while (s && *s && std::isspace(static_cast<unsigned char>(*s))) {
        s++;
    }
    return s;
}

static const char* nextToken(const char* s, char* out, size_t outSize) {
    if (!out || outSize == 0) return nullptr;
    s = skipSpaces(s);
    if (!s || !*s) return nullptr;
    size_t i = 0;
    while (*s && !std::isspace(static_cast<unsigned char>(*s)) && i + 1 < outSize) {
        out[i++] = *s++;
    }
    out[i] = '\0';
    return s;
}

static bool findSlotByIndex(uint16_t index, uint16_t* slotOut) {
    if (!slotOut) return false;
    auto& store = PasswordStore::instance();
    if (!store.hasSlotRange()) return false;

    uint16_t cap = store.capacity();
    if (cap == 0) return false;
    auto list = std::unique_ptr<PasswordStore::EntryIndex[]>(new (std::nothrow) PasswordStore::EntryIndex[cap]);
    if (!list) return false;

    uint16_t count = 0;
    if (!store.listEntriesSorted(list.get(), cap, &count)) return false;
    if (index >= count) return false;

    *slotOut = list[index].slot;
    return true;
}

static void cmd_password_list(const char* args) {
    (void)args;
    auto& store = PasswordStore::instance();
    if (!store.hasSlotRange()) {
        cdc::serial::Console::printf("ERROR: slot map not configured\r\n");
        return;
    }
    uint16_t cap = store.capacity();
    if (cap == 0) {
        cdc::serial::Console::printf("(no entries)\r\n");
        return;
    }
    auto list = std::unique_ptr<PasswordStore::EntryIndex[]>(new (std::nothrow) PasswordStore::EntryIndex[cap]);
    if (!list) {
        cdc::serial::Console::printf("ERROR: out of memory\r\n");
        return;
    }
    uint16_t count = 0;
    if (!store.listEntriesSorted(list.get(), cap, &count)) {
        cdc::serial::Console::printf("ERROR: list failed\r\n");
        return;
    }
    if (count == 0) {
        cdc::serial::Console::printf("(no entries)\r\n");
        return;
    }
    for (uint16_t i = 0; i < count; i++) {
        cdc::serial::Console::printf("%u: %s (slot %u)\r\n", i, list[i].title, list[i].slot);
    }
}

static void cmd_password_get(const char* args) {
    char indexBuf[8] = {};
    const char* p = nextToken(args, indexBuf, sizeof(indexBuf));
    if (!p || !indexBuf[0]) {
        cdc::serial::Console::printf("Usage: PASSWORD_GET <index>\r\n");
        return;
    }
    uint16_t index = static_cast<uint16_t>(atoi(indexBuf));
    uint16_t slot = 0;
    if (!findSlotByIndex(index, &slot)) {
        cdc::serial::Console::printf("ERROR: invalid index\r\n");
        return;
    }
    PasswordEntry entry = {};
    if (!PasswordStore::instance().readEntry(slot, &entry)) {
        cdc::serial::Console::printf("ERROR: read failed\r\n");
        return;
    }
    cdc::serial::Console::printf("Title: %s\r\n", entry.title);
    cdc::serial::Console::printf("Username: %s\r\n", entry.username);
    cdc::serial::Console::printf("Password: %s\r\n", entry.password);
    cdc::serial::Console::printf("URL: %s\r\n", entry.url);
    if (entry.totpSlot == PasswordStore::TOTP_SLOT_NONE) {
        cdc::serial::Console::printf("TOTP Slot: none\r\n");
    } else {
        cdc::serial::Console::printf("TOTP Slot: %u\r\n", entry.totpSlot);
    }
    cdc::serial::Console::printf("Notes: %s\r\n", entry.notes);
}

static void cmd_password_add(const char* args) {
    PasswordEntry entry = {};
    entry.totpSlot = PasswordStore::TOTP_SLOT_NONE;

    char title[PasswordStore::TITLE_LEN + 1] = {};
    char username[PasswordStore::USERNAME_LEN + 1] = {};
    char password[PasswordStore::PASSWORD_LEN + 1] = {};
    char url[PasswordStore::URL_LEN + 1] = {};
    char totpBuf[8] = {};

    const char* p = nextToken(args, title, sizeof(title));
    if (!p || !title[0]) {
        cdc::serial::Console::printf("Usage: PASSWORD_ADD <title> <username|- > <password> <url|- > [totpSlot] [notes]\r\n");
        return;
    }
    p = nextToken(p, username, sizeof(username));
    p = nextToken(p, password, sizeof(password));
    p = nextToken(p, url, sizeof(url));
    if (!password[0]) {
        cdc::serial::Console::printf("Usage: PASSWORD_ADD <title> <username|- > <password> <url|- > [totpSlot] [notes]\r\n");
        return;
    }
    p = nextToken(p, totpBuf, sizeof(totpBuf));

    const char* notes = skipSpaces(p);

    strncpy(entry.title, title, sizeof(entry.title) - 1);
    if (strcmp(username, "-") != 0) {
        strncpy(entry.username, username, sizeof(entry.username) - 1);
    }
    strncpy(entry.password, password, sizeof(entry.password) - 1);
    if (strcmp(url, "-") != 0) {
        strncpy(entry.url, url, sizeof(entry.url) - 1);
    }

    if (totpBuf[0]) {
        int totp = atoi(totpBuf);
        if (totp >= 0 && totp <= 255) {
            entry.totpSlot = static_cast<uint8_t>(totp);
        }
    }

    if (notes && notes[0]) {
        strncpy(entry.notes, notes, sizeof(entry.notes) - 1);
    }

    bool ok = PasswordStore::instance().addEntry(entry);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}

static void cmd_password_del(const char* args) {
    char indexBuf[8] = {};
    const char* p = nextToken(args, indexBuf, sizeof(indexBuf));
    if (!p || !indexBuf[0]) {
        cdc::serial::Console::printf("Usage: PASSWORD_DEL <index>\r\n");
        return;
    }
    uint16_t index = static_cast<uint16_t>(atoi(indexBuf));
    uint16_t slot = 0;
    if (!findSlotByIndex(index, &slot)) {
        cdc::serial::Console::printf("ERROR: invalid index\r\n");
        return;
    }
    bool ok = PasswordStore::instance().deleteEntry(slot);
    cdc::serial::Console::printf(ok ? "OK\r\n" : "ERROR\r\n");
}

static void registerCommands() {
    if (s_commandsRegistered) return;
    s_commandsRegistered = true;

    auto& reg = cdc::serial::getCommandRegistry();
    reg.registerCommand({"PASSWORD_LIST", "List password entries", cmd_password_list, CMD_MODULE, true});
    reg.registerCommand({"PASSWORD_GET", "Get password entry", cmd_password_get, CMD_MODULE, true});
    reg.registerCommand({"PASSWORD_ADD", "Add password entry", cmd_password_add, CMD_MODULE, true});
    reg.registerCommand({"PASSWORD_DEL", "Delete password entry", cmd_password_del, CMD_MODULE, true});
}

// === UI State ===

static ui::ListView s_listView;
static ui::T9InputView s_t9Input;
static ui::InfoView s_infoView;
static bool s_viewsInitialized = false;

static ui::ListItem* s_listItems = nullptr;
static PasswordStore::EntryIndex* s_entries = nullptr;
static uint16_t s_entryCount = 0;
static uint16_t s_capacity = 0;

static uint16_t s_activeSlot = 0;

struct WizardState {
    PasswordEntry entry;
    bool editMode;
    uint16_t editSlot;
};

static WizardState s_wizard = {};

static constexpr uint16_t NOTES_INPUT_MAX =
    (PasswordStore::NOTES_LEN < ui::T9InputView::MAX_TEXT_LEN)
        ? static_cast<uint16_t>(PasswordStore::NOTES_LEN)
        : static_cast<uint16_t>(ui::T9InputView::MAX_TEXT_LEN);

static void freeListBuffers() {
    delete[] s_listItems;
    delete[] s_entries;
    s_listItems = nullptr;
    s_entries = nullptr;
    s_capacity = 0;
    s_entryCount = 0;
}

static bool ensureListBuffers() {
    uint16_t cap = PasswordStore::instance().capacity();
    if (cap == 0) return false;
    if (cap == s_capacity && s_listItems && s_entries) return true;

    delete[] s_listItems;
    delete[] s_entries;
    s_listItems = nullptr;
    s_entries = nullptr;
    s_capacity = 0;

    s_listItems = new (std::nothrow) ui::ListItem[cap + 1];
    s_entries = new (std::nothrow) PasswordStore::EntryIndex[cap];
    if (!s_listItems || !s_entries) {
        delete[] s_listItems;
        delete[] s_entries;
        s_listItems = nullptr;
        s_entries = nullptr;
        s_capacity = 0;
        return false;
    }
    s_capacity = cap;
    return true;
}

static void rebuildList() {
    if (!PasswordStore::instance().hasSlotRange()) {
        ui::showToastError(mstr(STR_SLOT_ERROR));
        return;
    }
    if (!ensureListBuffers()) {
        cdc::core::ModuleRegistry::instance().reportModuleError(PasswordModule::instance().getName(),
                                                               "Password list allocation failed");
        return;
    }
    s_entryCount = 0;
    s_listItems[0] = {mstr(STR_NEW_ENTRY), 0, false, nullptr};

    uint16_t count = 0;
    PasswordStore::instance().listEntriesSorted(s_entries, s_capacity, &count);
    s_entryCount = count;

    for (uint16_t i = 0; i < s_entryCount; i++) {
        uint16_t idx = static_cast<uint16_t>(i + 1);
        s_listItems[idx].label = s_entries[i].title;
        s_listItems[idx].icon = 0;
        s_listItems[idx].iconDisabled = false;
        s_listItems[idx].userData = reinterpret_cast<void*>(static_cast<uintptr_t>(s_entries[i].slot));
    }

    s_listView.init(mstr(STR_PASSWORDS), s_listItems, static_cast<uint16_t>(s_entryCount + 1));
    s_listView.setHint(mstr(STR_HINT_LIST));
}

static void showDetails(uint16_t slot) {
    PasswordEntry entry = {};
    if (!PasswordStore::instance().readEntry(slot, &entry)) {
        ui::showToastError(ui::tr(ui::StringId::FAILED));
        return;
    }

    static char detailText[ui::InfoView::MAX_TEXT_LEN];
    char totpBuf[16] = {};
    const char* emptyText = ui::tr(ui::StringId::EMPTY);
    char emptyWrapped[16] = {};
    snprintf(emptyWrapped, sizeof(emptyWrapped), "(%s)", emptyText);
    const char* totpText = emptyWrapped;
    if (entry.totpSlot != PasswordStore::TOTP_SLOT_NONE) {
        snprintf(totpBuf, sizeof(totpBuf), "%u", entry.totpSlot);
        totpText = totpBuf;
    }
    const char* usernameText = entry.username[0] ? entry.username : emptyWrapped;
    const char* passwordText = entry.password[0] ? entry.password : emptyWrapped;
    const char* urlText = entry.url[0] ? entry.url : emptyWrapped;
    const char* notesText = entry.notes[0] ? entry.notes : emptyWrapped;

    snprintf(detailText, sizeof(detailText),
             "Title: %s\n"
             "Username: %s\n"
             "Password: %s\n"
             "URL: %s\n"
             "TOTP Slot: %s\n"
             "Notes:\n%s",
             entry.title,
             usernameText,
             passwordText,
             urlText,
             totpText,
             notesText);

    s_infoView.init(mstr(STR_DETAILS), detailText);
    ui::ViewStack::instance().push(&s_infoView);
}

static void wizardFinish() {
    bool ok = false;
    if (s_wizard.editMode) {
        ok = PasswordStore::instance().updateEntry(s_wizard.editSlot, s_wizard.entry);
    } else {
        ok = PasswordStore::instance().addEntry(s_wizard.entry);
    }

    if (ok) {
        ui::showToastSuccess(mstr(STR_SAVED));
        s_listView.preservePosition();
        rebuildList();
        while (ui::ViewStack::instance().current() != &s_listView &&
               ui::ViewStack::instance().depth() > 1) {
            ui::ViewStack::instance().pop();
        }
    } else {
        ui::showToastError(ui::tr(ui::StringId::FAILED));
    }
}

static void pushT9WizardStep(const char* title, const char* initialText,
                             uint16_t maxLen, ui::T9InputView::SaveCallback onSave) {
    s_t9Input.init(title, initialText, maxLen);
    s_t9Input.setOnSave(onSave);
    ui::ViewStack::instance().push(&s_t9Input);
}

static void onWizardTitle(const char* text);
static void onWizardUsername(const char* text);
static void onWizardPassword(const char* text);
static void onWizardUrl(const char* text);
static void onWizardTotp(const char* text);
static void onWizardNotes(const char* text);

static void wizardStart() {
    memset(&s_wizard, 0, sizeof(s_wizard));
    s_wizard.entry.totpSlot = PasswordStore::TOTP_SLOT_NONE;
    s_wizard.editMode = false;
    s_wizard.editSlot = 0;

    pushT9WizardStep(mstr(STR_TITLE), nullptr, PasswordStore::TITLE_LEN, onWizardTitle);
}

static void wizardEdit(uint16_t slot) {
    PasswordEntry entry = {};
    if (!PasswordStore::instance().readEntry(slot, &entry)) {
        ui::showToastError(ui::tr(ui::StringId::FAILED));
        return;
    }

    memset(&s_wizard, 0, sizeof(s_wizard));
    s_wizard.entry = entry;
    s_wizard.editMode = true;
    s_wizard.editSlot = slot;

    pushT9WizardStep(mstr(STR_TITLE), s_wizard.entry.title, PasswordStore::TITLE_LEN, onWizardTitle);
}

static void onWizardTitle(const char* text) {
    strncpy(s_wizard.entry.title, text ? text : "", sizeof(s_wizard.entry.title) - 1);
    pushT9WizardStep(mstr(STR_USERNAME), s_wizard.entry.username, PasswordStore::USERNAME_LEN, onWizardUsername);
}

static void onWizardUsername(const char* text) {
    strncpy(s_wizard.entry.username, text ? text : "", sizeof(s_wizard.entry.username) - 1);
    pushT9WizardStep(mstr(STR_PASSWORD), s_wizard.entry.password, PasswordStore::PASSWORD_LEN, onWizardPassword);
}

static void onWizardPassword(const char* text) {
    strncpy(s_wizard.entry.password, text ? text : "", sizeof(s_wizard.entry.password) - 1);
    pushT9WizardStep(mstr(STR_URL), s_wizard.entry.url, PasswordStore::URL_LEN, onWizardUrl);
}

static void onWizardUrl(const char* text) {
    strncpy(s_wizard.entry.url, text ? text : "", sizeof(s_wizard.entry.url) - 1);

    char totpBuf[8] = {};
    if (s_wizard.entry.totpSlot != PasswordStore::TOTP_SLOT_NONE) {
        snprintf(totpBuf, sizeof(totpBuf), "%u", s_wizard.entry.totpSlot);
    }
    pushT9WizardStep(mstr(STR_TOTP_SLOT), totpBuf, 3, onWizardTotp);
}

static void onWizardTotp(const char* text) {
    if (!text || !text[0]) {
        s_wizard.entry.totpSlot = PasswordStore::TOTP_SLOT_NONE;
    } else {
        int value = atoi(text);
        if (value < 0 || value > 255) {
            ui::showToastError(mstr(STR_INVALID_INPUT));
            pushT9WizardStep(mstr(STR_TOTP_SLOT), text, 3, onWizardTotp);
            return;
        }
        s_wizard.entry.totpSlot = static_cast<uint8_t>(value);
    }

    pushT9WizardStep(mstr(STR_NOTES), s_wizard.entry.notes, NOTES_INPUT_MAX, onWizardNotes);
}

static void onWizardNotes(const char* text) {
    strncpy(s_wizard.entry.notes, text ? text : "", sizeof(s_wizard.entry.notes) - 1);
    wizardFinish();
}

static void onMenuView() {
    showDetails(s_activeSlot);
}

static void onMenuEdit() {
    wizardEdit(s_activeSlot);
}

static void onMenuDeleteConfirm(void* userData) {
    uint16_t slot = *static_cast<uint16_t*>(userData);
    bool ok = PasswordStore::instance().deleteEntry(slot);
    if (ok) {
        ui::showToastSuccess(mstr(STR_DELETED));
        s_listView.preservePosition();
        rebuildList();
        while (ui::ViewStack::instance().current() != &s_listView &&
               ui::ViewStack::instance().depth() > 1) {
            ui::ViewStack::instance().pop();
        }
    } else {
        ui::showToastError(ui::tr(ui::StringId::FAILED));
    }
}

static void onMenuDelete() {
    static uint16_t slot = 0;
    slot = s_activeSlot;
    ui::showConfirm(mstr(STR_CONFIRM_DELETE), onMenuDeleteConfirm, nullptr,
                    ui::ConfirmView::Icon::WARNING, &slot);
}

static void onListMenu(uint16_t index, void* userData) {
    (void)userData;
    if (index == 0) {
        static ui::ContextMenuItem items[] = {
            {mstr(STR_NEW_ENTRY), []() { wizardStart(); }}
        };
        ui::showContextMenu(mstr(STR_ACTIONS), items, 1);
        return;
    }
    if (index - 1 >= s_entryCount) return;
    s_activeSlot = s_entries[index - 1].slot;

    static ui::ContextMenuItem items[] = {
        {mstr(STR_VIEW), onMenuView},
        {mstr(STR_EDIT), onMenuEdit},
        {mstr(STR_DELETE), onMenuDelete}
    };
    ui::showContextMenu(mstr(STR_ACTIONS), items, 3);
}

static void onListSelect(uint16_t index, void* userData) {
    (void)userData;
    if (index == 0) {
        wizardStart();
        return;
    }
    if (index - 1 >= s_entryCount) return;
    s_activeSlot = s_entries[index - 1].slot;
    showDetails(s_activeSlot);
}

PasswordModule& PasswordModule::instance() {
    static PasswordModule inst;
    return inst;
}

bool PasswordModule::init() {
    LOG_I(TAG, "Initializing Password module");
    registerStrings();
    registerCommands();

    core::ModuleRegistry::instance().registerModule(this);
    if (slotRange_.hasRmem) {
        PasswordStore::instance().setSlotRange(slotRange_.rmemStart, slotRange_.rmemEnd, slotRange_.moduleId);
        core::ModuleRegistry::instance().clearModuleErrorByName(getName());
    } else {
        core::ModuleRegistry::instance().reportModuleError(getName(), "Password slot range missing");
        state_ = core::ServiceState::ERROR;
        return false;
    }
    state_ = core::ServiceState::INITIALIZED;
    return true;
}

bool PasswordModule::start() {
    if (state_ != core::ServiceState::INITIALIZED &&
        state_ != core::ServiceState::STOPPED) {
        return false;
    }
    state_ = core::ServiceState::STARTED;
    return true;
}

void PasswordModule::stop() {
    freeListBuffers();
    state_ = core::ServiceState::STOPPED;
}

void PasswordModule::setSlotRange(const core::IModule::SlotRange& range) {
    slotRange_ = range;
}

core::IModule::SlotRequest PasswordModule::getSlotRequest() const {
    core::IModule::SlotRequest req = {};
    req.mapName = getName();
    req.minRmemSlots = 1;
    return req;
}

uint8_t PasswordModule::getMenuItems(core::ModuleMenuItem* items, uint8_t maxItems) {
    if (!items || maxItems == 0) return 0;

    items[0] = {mstr(STR_PASSWORDS), 55, []() -> ui::IView* {
        if (!s_viewsInitialized) {
            s_listView.setOnSelect(onListSelect);
            s_listView.setOnMenu(onListMenu);
            s_viewsInitialized = true;
        }
        if (!PasswordStore::instance().hasSlotRange()) {
            ui::showToastError(mstr(STR_SLOT_ERROR));
            return nullptr;
        }
        rebuildList();
        return &s_listView;
    }, nullptr, getName(), core::MenuLocation::MAIN_MENU};

    return 1;
}

} // namespace cdc::mod_password

extern "C" void mod_password_register() {
    cdc::core::ModuleRegistry::instance().registerInitializer([]() {
        auto& module = cdc::mod_password::PasswordModule::instance();
        if (module.init()) {
            module.start();
        }
    });
}
