#pragma once

// Views Module for CDC Badge
// Provides reusable screen components:
// 1. Lock Screen - status display with battery, clock, user info
// 2. Info Screen - scrollable long text display
// 3. List Screen - selectable menu with max 4 visible items
// 4. T9 Input Screen - text input with T9 multi-tap
// 5. Slider Screen - value adjustment with visual bar
// 6. PIN Entry Screen - secure PIN code input

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Common Constants
// ============================================================================

#define VIEW_MAX_TEXT_LEN 64

// ============================================================================
// Status Bar Icons (rendered right-to-left in order of priority)
// ============================================================================

typedef enum {
    ICON_NONE        = 0,
    ICON_LOCK        = (1 << 0),  // Padlock
    ICON_DEEP_SLEEP  = (1 << 1),  // zzZ
    ICON_LIGHT_SLEEP = (1 << 2),  // z
    ICON_BACKLIGHT   = (1 << 3),  // Sun
    ICON_USB         = (1 << 4),  // USB connected
    ICON_BLE         = (1 << 5),  // Bluetooth
    ICON_WIFI        = (1 << 6),  // WiFi connected
} status_icon_t;

// Render status bar icons at specified position
// Returns total width used
int view_render_status_icons(uint16_t icons, int x, int y);
#define VIEW_LIST_MAX_ITEMS 32
#define VIEW_LIST_VISIBLE_ITEMS 4
#define VIEW_T9_TIMEOUT_MS 2000
#define VIEW_PIN_MAX_LEN 6

// ============================================================================
// Lock Screen View
// ============================================================================

typedef struct {
    char name[VIEW_MAX_TEXT_LEN];
    char info[VIEW_MAX_TEXT_LEN];
    char info2[VIEW_MAX_TEXT_LEN];
    uint8_t battery_percent;
    bool charging;
    char clock[8];  // "HH:MM" or "--:--"
    char date[12];  // "DD.MM.YYYY" or empty
    uint16_t status_icons;      // Bitmask of status_icon_t values
    // Legacy fields (deprecated, use status_icons instead)
    bool show_lock_icon;
    bool show_sleep_icon;       // Show zzZ for deep sleep mode
    bool show_light_sleep_icon; // Show small z for light sleep mode
    bool backlight_on;  // Show sun icon when backlight is forced on
} view_lock_screen_t;

// Render (partial=true for updates, false for first draw)
void view_lock_screen_render(const view_lock_screen_t *data, bool partial);

// ============================================================================
// Info Screen View (Scrollable Text)
// ============================================================================

typedef struct {
    const char *title;
    const char *text;           // Full text to display
    uint16_t text_len;          // Length of text
    uint16_t scroll_offset;     // Current scroll position (in lines)
    uint16_t total_lines;       // Total lines (calculated)
    uint8_t visible_lines;      // Lines visible on screen
} view_info_screen_t;

// Initialize info screen (calculates total_lines)
void view_info_screen_init(view_info_screen_t *view, const char *title, const char *text);

// Scroll up/down (key 2 = up, key 8 = down)
void view_info_screen_scroll(view_info_screen_t *view, bool down);

// Render (partial=true for scroll updates)
void view_info_screen_render(const view_info_screen_t *view, bool partial);

// ============================================================================
// List Screen View (Selection Menu)
// ============================================================================

typedef struct {
    const char *label;
} view_list_item_t;

typedef struct {
    const char *title;
    const view_list_item_t *items;
    uint8_t item_count;
    uint8_t selection;          // Currently selected index
    uint8_t scroll_pos;         // First visible item index
    const char *hint;           // Optional custom hint (NULL = default)
} view_list_screen_t;

// Initialize list screen
void view_list_screen_init(view_list_screen_t *view, const char *title,
                           const view_list_item_t *items, uint8_t count);

// Navigate up/down (key 2 = up, key 8 = down)
void view_list_screen_navigate(view_list_screen_t *view, bool down);


// Get currently selected index
uint8_t view_list_screen_get_selection(const view_list_screen_t *view);

// Render (partial=true for navigation updates)
void view_list_screen_render(const view_list_screen_t *view, bool partial);

// ============================================================================
// T9 Input Screen View
// ============================================================================

typedef struct {
    const char *title;
    char buffer[VIEW_MAX_TEXT_LEN];
    uint8_t len;
    char last_key;              // Last pressed key for T9 cycling
    uint8_t char_index;         // Current character index in T9 cycle
    uint32_t last_press_ms;     // Timestamp of last keypress
    bool cursor_active;         // Show cursor blinking
} view_t9_input_t;

// Initialize T9 input screen
void view_t9_input_init(view_t9_input_t *view, const char *title, const char *initial_text);

// Process T9 key input (0-9)
// Returns true if character was added/changed
bool view_t9_input_key(view_t9_input_t *view, char key);

// Delete last character (backspace)
void view_t9_input_backspace(view_t9_input_t *view);

// Update T9 state (call periodically to finalize character on timeout)
void view_t9_input_update(view_t9_input_t *view);

// Get current text
const char* view_t9_input_get_text(const view_t9_input_t *view);

// Render (partial=true for input updates)
void view_t9_input_render(const view_t9_input_t *view, bool partial);

// ============================================================================
// Slider Screen View
// ============================================================================

typedef struct {
    const char *title;
    const char *hint;           // Hint text for bottom bar
    uint16_t value;             // Current value
    uint16_t min_value;         // Minimum value
    uint16_t max_value;         // Maximum value
    uint16_t step;              // Step size for adjustments
    const char *format;         // printf format for value (e.g., "%d")
    const char *unit;           // Unit string (e.g., "%", "min")
    int16_t display_offset;     // Offset added to value for display (default 0)
} view_slider_t;

// Initialize slider screen
void view_slider_init(view_slider_t *view, const char *title,
                      uint16_t min_val, uint16_t max_val, uint16_t initial,
                      uint16_t step, const char *format, const char *unit,
                      const char *hint);

// Adjust value up/down (key 2 = up/increase, key 8 = down/decrease)
void view_slider_adjust(view_slider_t *view, bool up);

// Get current value
uint16_t view_slider_get_value(const view_slider_t *view);

// Set current value
void view_slider_set_value(view_slider_t *view, uint16_t value);

// Render (partial=true for value updates)
void view_slider_render(const view_slider_t *view, bool partial);

// ============================================================================
// PIN Entry Screen View
// ============================================================================

typedef struct {
    const char *title;
    char buffer[VIEW_PIN_MAX_LEN + 1];
    uint8_t len;
    uint8_t max_len;            // Maximum PIN length (default 6)
    uint8_t attempts;           // Failed attempts count
    uint8_t max_attempts;       // Maximum attempts (default 3)
    const char *message;        // Optional message below PIN
    bool locked;                // Account locked after max attempts
    uint32_t error_shown_ms;    // Timestamp when error was shown (0 = no error)
} view_pin_entry_t;

#define VIEW_PIN_ERROR_DURATION_MS 2000

// Initialize PIN entry screen
void view_pin_entry_init(view_pin_entry_t *view, const char *title,
                         uint8_t max_len, uint8_t max_attempts);

// Add digit to PIN (0-9)
// Returns true if digit was added
bool view_pin_entry_digit(view_pin_entry_t *view, char digit);

// Delete last digit (backspace)
void view_pin_entry_backspace(view_pin_entry_t *view);

// Clear PIN buffer
void view_pin_entry_clear(view_pin_entry_t *view);

// Get current PIN
const char* view_pin_entry_get_pin(const view_pin_entry_t *view);

// Check if PIN is complete
bool view_pin_entry_is_complete(const view_pin_entry_t *view);

// Set failed attempt (increments counter, returns true if locked)
bool view_pin_entry_fail(view_pin_entry_t *view);

// Set message
void view_pin_entry_set_message(view_pin_entry_t *view, const char *msg);

// Render (partial=true for input updates)
void view_pin_entry_render(const view_pin_entry_t *view, bool partial);

// ============================================================================
// Date Input Screen View
// ============================================================================

typedef struct {
    uint8_t day;        // 1-31
    uint8_t month;      // 1-12
    uint16_t year;      // e.g. 2025
    uint8_t field;      // 0=day, 1=month, 2=year
    uint8_t digit;      // Current digit position within field
} view_date_input_t;

// Initialize date input screen
void view_date_input_init(view_date_input_t *view, uint8_t day, uint8_t month, uint16_t year);

// Process digit key (0-9), returns true if value changed
bool view_date_input_key(view_date_input_t *view, char key);

// Move to next/prev field (key 6 = next, key 4 = prev)
void view_date_input_next_field(view_date_input_t *view);
void view_date_input_prev_field(view_date_input_t *view);

// Clear current field (N key) - returns true if cleared, false if already at start
bool view_date_input_clear_field(view_date_input_t *view);

// Render
void view_date_input_render(const view_date_input_t *view, bool partial);

// ============================================================================
// Time Input Screen View
// ============================================================================

typedef struct {
    uint8_t hour;       // 0-23
    uint8_t minute;     // 0-59
    uint8_t field;      // 0=hour, 1=minute
    uint8_t digit;      // Current digit position within field
} view_time_input_t;

// Initialize time input screen
void view_time_input_init(view_time_input_t *view, uint8_t hour, uint8_t minute);

// Process digit key (0-9), returns true if value changed
bool view_time_input_key(view_time_input_t *view, char key);

// Move to next/prev field (key 6 = next, key 4 = prev)
void view_time_input_next_field(view_time_input_t *view);
void view_time_input_prev_field(view_time_input_t *view);

// Clear current field (N key) - returns true if cleared, false if already at start
bool view_time_input_clear_field(view_time_input_t *view);

// Render
void view_time_input_render(const view_time_input_t *view, bool partial);

// ============================================================================
// Toast/Message Overlay
// ============================================================================

// Show a toast message overlay (centered box with message)
// Uses partial refresh, blocks for duration_ms, then returns
// Caller should render the next screen after this returns
void view_toast_show(const char *message, uint16_t duration_ms);

// Show success toast (green checkmark icon + message)
void view_toast_success(const char *message, uint16_t duration_ms);

// Show error toast (X icon + message)
void view_toast_error(const char *message, uint16_t duration_ms);

// ============================================================================
// T9 Character Mapping (exposed for external use)
// ============================================================================

// Get character for key and index (T9 multi-tap)
char view_t9_get_char(char key, uint8_t index);

// Get number of characters for key
uint8_t view_t9_get_char_count(char key);

// ============================================================================
// TOTP Code View (with progress bar)
// ============================================================================

typedef struct {
    char issuer[VIEW_MAX_TEXT_LEN];     // Account issuer (shown in header)
    char name[VIEW_MAX_TEXT_LEN];       // Account name (shown below header if issuer exists)
    char code[12];                      // Formatted code "123 456" or "1234 5678"
    uint8_t digits;                     // 6 or 8
    uint32_t period;                    // Time period (usually 30)
    int8_t remaining;                   // Seconds remaining (-1 = time not synced)
    const char *hint;                   // Bottom hint text
} view_totp_code_t;

// Initialize TOTP code view
void view_totp_code_init(view_totp_code_t *view, const char *issuer, const char *name,
                         const char *code, uint8_t digits, uint32_t period,
                         int8_t remaining, const char *hint);

// Update code and remaining time (for refresh)
void view_totp_code_update(view_totp_code_t *view, const char *code, int8_t remaining);

// Render TOTP code screen
void view_totp_code_render(const view_totp_code_t *view, bool partial);

// ============================================================================
// Context Menu Overlay
// ============================================================================

#define VIEW_CONTEXT_MAX_ITEMS 6
#define VIEW_CONTEXT_TITLE_LEN 32

typedef struct {
    const char *label;
    uint8_t action_id;      // Returned when selected (0 = cancel)
} view_context_item_t;

typedef struct {
    char title[VIEW_CONTEXT_TITLE_LEN];  // Copied title to avoid dangling pointer
    const view_context_item_t *items;
    uint8_t item_count;
    uint8_t selection;
    bool visible;
} view_context_menu_t;

// Initialize context menu
void view_context_menu_init(view_context_menu_t *menu, const char *title,
                            const view_context_item_t *items, uint8_t count);

// Show context menu overlay (renders on top of current screen)
void view_context_menu_show(view_context_menu_t *menu);

// Hide context menu (caller must re-render underlying screen)
void view_context_menu_hide(view_context_menu_t *menu);

// Navigate selection (up/down)
void view_context_menu_navigate(view_context_menu_t *menu, bool down);

// Render the overlay
void view_context_menu_render(const view_context_menu_t *menu);

// Get selected action_id
uint8_t view_context_menu_get_action(const view_context_menu_t *menu);

// Check if visible
bool view_context_menu_is_visible(const view_context_menu_t *menu);

// ============================================================================
// QR Code View
// ============================================================================

typedef struct {
    const char *title;          // Title shown at top (can be NULL)
    const char *data;           // Data to encode in QR code
    uint8_t scale;              // Pixels per QR module (1-3, auto-calculated if 0)
} view_qr_code_t;

// Initialize QR code view
void view_qr_code_init(view_qr_code_t *view, const char *title, const char *data);

// Render QR code (always full refresh due to complexity)
void view_qr_code_render(const view_qr_code_t *view, bool partial);

#ifdef __cplusplus
}
#endif
