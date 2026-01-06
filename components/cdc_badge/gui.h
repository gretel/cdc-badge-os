#pragma once

// GUI Module for CDC Badge
// Display abstraction with full and partial refresh support

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
#include <goodisplay/gdey029T94.h>

extern "C" {
#endif

// Default backlight level (0-1023)
#define GUI_BACKLIGHT_DEFAULT 512
#define GUI_BACKLIGHT_MIN 0
#define GUI_BACKLIGHT_MAX 1023
#define GUI_BACKLIGHT_STEP 100

// Refresh modes
typedef enum {
    GUI_REFRESH_FULL,      // Full refresh with screen blink (cleanest)
    GUI_REFRESH_PARTIAL    // Partial refresh without blink (faster)
} gui_refresh_mode_t;

// Initialize display hardware (loads backlight from NVS)
void gui_init(void);

// Show splash screen (full refresh - call once at boot)
void gui_show_splash(void);

// Clear display buffer to white
void gui_clear(void);

// Flush buffer to screen (async - returns immediately)
// full_refresh = true: Full update with blink (cleaner, use for first draw)
// full_refresh = false: Partial update (faster, no blink, use for updates)
void gui_flush(bool full_refresh);

// Convenience functions (async)
void gui_flush_full(void);     // Full refresh
void gui_flush_partial(void);  // Partial refresh

// Synchronous flush - blocks until display update complete
void gui_flush_sync(bool full_refresh);

// Backlight control (0-1023)
void gui_set_backlight(uint16_t level);    // Set and apply backlight
uint16_t gui_get_backlight(void);          // Get current backlight level
void gui_save_backlight(void);             // Save current level to NVS
void gui_load_backlight(void);             // Load from NVS
void gui_backlight_on(void);               // Turn on backlight (use saved level)
void gui_backlight_off(void);              // Turn off backlight (0)
void gui_backlight_toggle(void);           // Toggle on/off
bool gui_is_backlight_on(void);            // Check if backlight is on

#ifdef __cplusplus
// Get display reference for direct drawing
Gdey029T94& gui_get_display(void);
}
#endif
