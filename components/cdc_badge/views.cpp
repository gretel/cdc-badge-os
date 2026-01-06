// Views Module for CDC Badge
// Provides reusable screen components

#include "views.h"
#include "gui.h"
#include "cdc_log.h"
#include "cdc_time.h"
#include <cstring>
#include <cstdio>
#include <cmath>

#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>

// ============================================================================
// Display Constants
// ============================================================================

#define ICON_HEIGHT 14
#define ICON_MARGIN 4
#define ICON_SPACING 3
#define HEADER_HEIGHT 20
#define LINE_HEIGHT 18
#define VISIBLE_TEXT_LINES 4
#define SLIDER_BAR_WIDTH 200
#define SLIDER_BAR_HEIGHT 12

// ============================================================================
// T9 Character Mappings
// ============================================================================

static const char* t9_chars[] = {
    " 0",                           // 0 - space, 0
    ".?!,;:'\"()-_@#$%&*+=/\\1",   // 1 - symbols, 1
    "abcABC2",                      // 2 - abc, 2
    "defDEF3",                      // 3 - def, 3
    "ghiGHI4",                      // 4 - ghi, 4
    "jklJKL5",                      // 5 - jkl, 5
    "mnoMNO6",                      // 6 - mno, 6
    "pqrsPQRS7",                    // 7 - pqrs, 7
    "tuvTUV8",                      // 8 - tuv, 8
    "wxyzWXYZ9"                     // 9 - wxyz, 9
};

char view_t9_get_char(char key, uint8_t index) {
    if (key < '0' || key > '9') return '\0';
    int key_idx = key - '0';
    const char* chars = t9_chars[key_idx];
    uint8_t len = strlen(chars);
    if (len == 0) return '\0';
    return chars[index % len];
}

uint8_t view_t9_get_char_count(char key) {
    if (key < '0' || key > '9') return 0;
    int key_idx = key - '0';
    return strlen(t9_chars[key_idx]);
}

// ============================================================================
// Helper Functions
// ============================================================================

static void draw_header(Gdey029T94 &display, const char *title) {
    display.fillRect(0, 0, display.width(), HEADER_HEIGHT, EPD_BLACK);
    display.setTextColor(EPD_WHITE);
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(4, 14);
    display.print(title);
}

static void draw_hints(Gdey029T94 &display, const char *hint) {
    const int bottom_margin = 2;  // 1px higher
    display.setTextColor(EPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);

    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds("Ag", 0, 0, &x1, &y1, &w, &h);
    if (h == 0) h = 12;

    int16_t text_y = display.height() - bottom_margin;
    int16_t line_y = text_y - h - 1;
    if (line_y < 0) line_y = 0;

    display.drawFastHLine(0, line_y, display.width(), EPD_BLACK);
    display.setCursor(4, text_y);
    if (hint) {
        display.print(hint);
    }
}

static void draw_battery(Gdey029T94 &display, int x, int y, uint8_t percent, bool charging) {
    const int w = 24;
    const int h = ICON_HEIGHT;

    // Battery body outline
    display.drawRect(x, y, w - 3, h, EPD_BLACK);
    // Battery tip
    display.fillRect(x + w - 3, y + h/4, 3, h/2, EPD_BLACK);

    // Fill level
    int inner_w = w - 7;
    int fill_w = (inner_w * percent) / 100;
    if (fill_w > 0) {
        display.fillRect(x + 2, y + 2, fill_w, h - 4, EPD_BLACK);
    }

    // Charging indicator
    if (charging) {
        int cx = x + (w - 3) / 2;
        int cy = y + h / 2;
        display.drawLine(cx + 2, cy - 4, cx - 1, cy, EPD_DARKGREY);
        display.drawLine(cx - 1, cy, cx + 2, cy, EPD_DARKGREY);
        display.drawLine(cx + 2, cy, cx - 1, cy + 4, EPD_DARKGREY);
    }
}

static void draw_lock_icon(Gdey029T94 &display, int x, int y) {
    const int w = 12;
    const int h = 14;
    // Shackle
    display.drawRect(x + 2, y, 8, 6, EPD_BLACK);
    display.drawRect(x + 3, y + 1, 6, 4, EPD_WHITE);
    // Body
    display.fillRect(x, y + 5, w, h - 5, EPD_BLACK);
}

static void draw_sun_icon(Gdey029T94 &display, int x, int y) {
    const int r = 5;  // Sun radius
    // Center circle
    display.fillCircle(x, y, r, EPD_BLACK);
    // Rays
    for (int i = 0; i < 8; i++) {
        float angle = i * 3.14159f / 4.0f;
        int x1 = x + (int)((r + 2) * cos(angle));
        int y1 = y + (int)((r + 2) * sin(angle));
        int x2 = x + (int)((r + 5) * cos(angle));
        int y2 = y + (int)((r + 5) * sin(angle));
        display.drawLine(x1, y1, x2, y2, EPD_BLACK);
    }
}

// Draw comic-style sleep icon (stacked Z's getting smaller)
static void draw_sleep_icon(Gdey029T94 &display, int x, int y) {
    // Draw 3 Z's stacked, getting smaller as they go up-right
    // Large Z (bottom-left)
    int z1_x = x, z1_y = y + 10;
    int z1_w = 10, z1_h = 10;
    display.drawLine(z1_x, z1_y, z1_x + z1_w, z1_y, EPD_BLACK);           // top
    display.drawLine(z1_x + z1_w, z1_y, z1_x, z1_y + z1_h, EPD_BLACK);    // diagonal
    display.drawLine(z1_x, z1_y + z1_h, z1_x + z1_w, z1_y + z1_h, EPD_BLACK); // bottom

    // Medium Z (middle)
    int z2_x = x + 8, z2_y = y + 4;
    int z2_w = 7, z2_h = 7;
    display.drawLine(z2_x, z2_y, z2_x + z2_w, z2_y, EPD_BLACK);
    display.drawLine(z2_x + z2_w, z2_y, z2_x, z2_y + z2_h, EPD_BLACK);
    display.drawLine(z2_x, z2_y + z2_h, z2_x + z2_w, z2_y + z2_h, EPD_BLACK);

    // Small Z (top-right)
    int z3_x = x + 14, z3_y = y;
    int z3_w = 5, z3_h = 5;
    display.drawLine(z3_x, z3_y, z3_x + z3_w, z3_y, EPD_BLACK);
    display.drawLine(z3_x + z3_w, z3_y, z3_x, z3_y + z3_h, EPD_BLACK);
    display.drawLine(z3_x, z3_y + z3_h, z3_x + z3_w, z3_y + z3_h, EPD_BLACK);
}

// ============================================================================
// Lock Screen View
// ============================================================================

void view_lock_screen_render(const view_lock_screen_t *data, bool partial) {
    if (!data) return;

    Gdey029T94 &display = gui_get_display();
    gui_clear();
    display.setTextColor(EPD_BLACK);

    // Battery icon (top-left)
    draw_battery(display, ICON_MARGIN, ICON_MARGIN, data->battery_percent, data->charging);

    // Clock next to battery (hidden if empty)
    if (data->clock[0]) {
        display.setFont(&FreeMonoBold9pt7b);
        display.setCursor(ICON_MARGIN + 30, ICON_MARGIN + 12);
        display.print(data->clock);
    }

    // Sun icon (top-right) when backlight forced on
    if (data->backlight_on) {
        draw_sun_icon(display, display.width() - ICON_MARGIN - 25, ICON_MARGIN + 7);
    }

    // Sleep icon or Lock icon (top-right)
    if (data->show_sleep_icon) {
        draw_sleep_icon(display, display.width() - ICON_MARGIN - 22, ICON_MARGIN);
    } else if (data->show_lock_icon) {
        draw_lock_icon(display, display.width() - ICON_MARGIN - 12, ICON_MARGIN);
    }

    // Name (large, bold)
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(10, 50);
    display.print(data->name);

    // Info line
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(10, 75);
    display.print(data->info);

    // Info2 line
    display.setCursor(10, 95);
    display.print(data->info2);

    // Footer: different text for deep sleep mode
    if (data->show_sleep_icon) {
        draw_hints(display, "Press any key to wake up");
    } else {
        draw_hints(display, "[Y] Unlock");
    }
    gui_flush(!partial);
}

// ============================================================================
// Info Screen View (Scrollable Text)
// ============================================================================

void view_info_screen_init(view_info_screen_t *view, const char *title, const char *text) {
    if (!view) return;

    view->title = title;
    view->text = text;
    view->text_len = text ? strlen(text) : 0;
    view->scroll_offset = 0;
    view->visible_lines = VISIBLE_TEXT_LINES;

    // Calculate total lines properly (counting newlines and wrapping at 25 chars)
    view->total_lines = 0;
    if (view->text && view->text_len > 0) {
        const char *ptr = view->text;
        const int chars_per_line = 25;
        while (*ptr) {
            int line_chars = 0;
            while (*ptr && *ptr != '\n' && line_chars < chars_per_line) {
                ptr++;
                line_chars++;
            }
            if (*ptr == '\n') ptr++;
            view->total_lines++;
        }
    }
}

void view_info_screen_scroll(view_info_screen_t *view, bool down) {
    if (!view) return;

    if (down) {
        if (view->scroll_offset + view->visible_lines < view->total_lines) {
            view->scroll_offset++;
        }
    } else {
        if (view->scroll_offset > 0) {
            view->scroll_offset--;
        }
    }
}

void view_info_screen_render(const view_info_screen_t *view, bool partial) {
    if (!view) return;

    Gdey029T94 &display = gui_get_display();
    gui_clear();

    draw_header(display, view->title ? view->title : "Info");

    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(EPD_BLACK);

    if (view->text && view->text_len > 0) {
        // Simple line-by-line rendering with scroll
        const char *ptr = view->text;
        uint16_t current_line = 0;
        int y = 38;
        const int chars_per_line = 25;

        // Skip to scroll offset
        while (current_line < view->scroll_offset && *ptr) {
            int line_chars = 0;
            while (*ptr && *ptr != '\n' && line_chars < chars_per_line) {
                ptr++;
                line_chars++;
            }
            if (*ptr == '\n') ptr++;
            current_line++;
        }

        // Render visible lines
        for (int i = 0; i < view->visible_lines && *ptr; i++) {
            char line_buf[32];
            int line_chars = 0;

            while (*ptr && *ptr != '\n' && line_chars < chars_per_line) {
                line_buf[line_chars++] = *ptr++;
            }
            line_buf[line_chars] = '\0';
            if (*ptr == '\n') ptr++;

            display.setCursor(10, y);
            display.print(line_buf);
            y += LINE_HEIGHT;
        }

        // Scroll indicators
        if (view->scroll_offset > 0) {
            display.setCursor(display.width() - 15, 38);
            display.print("^");
        }
        if (view->scroll_offset + view->visible_lines < view->total_lines) {
            display.setCursor(display.width() - 15, 38 + LINE_HEIGHT * (view->visible_lines - 1));
            display.print("v");
        }
    } else {
        display.setCursor(10, 50);
        display.print("(empty)");
    }

    // Footer with line position
    char hint[32];
    snprintf(hint, sizeof(hint), "%d/%d  [Y] Back", view->scroll_offset + 1, view->total_lines);
    draw_hints(display, hint);
    gui_flush(!partial);
}

// ============================================================================
// List Screen View
// ============================================================================

void view_list_screen_init(view_list_screen_t *view, const char *title,
                           const view_list_item_t *items, uint8_t count) {
    if (!view) return;

    view->title = title;
    view->items = items;
    view->item_count = count;
    view->selection = 0;
    view->scroll_pos = 0;
}

void view_list_screen_navigate(view_list_screen_t *view, bool down) {
    if (!view || view->item_count == 0) return;

    if (down) {
        if (view->selection < view->item_count - 1) {
            view->selection++;
            // Adjust scroll if needed
            if (view->selection >= view->scroll_pos + VIEW_LIST_VISIBLE_ITEMS) {
                view->scroll_pos = view->selection - VIEW_LIST_VISIBLE_ITEMS + 1;
            }
        }
    } else {
        if (view->selection > 0) {
            view->selection--;
            // Adjust scroll if needed
            if (view->selection < view->scroll_pos) {
                view->scroll_pos = view->selection;
            }
        }
    }
}

bool view_list_screen_select_by_key(view_list_screen_t *view, char key) {
    if (!view || !view->items) return false;

    for (uint8_t i = 0; i < view->item_count; i++) {
        if (view->items[i].shortcut && view->items[i].shortcut[0] == key) {
            view->selection = i;
            // Adjust scroll
            if (view->selection < view->scroll_pos) {
                view->scroll_pos = view->selection;
            } else if (view->selection >= view->scroll_pos + VIEW_LIST_VISIBLE_ITEMS) {
                view->scroll_pos = view->selection - VIEW_LIST_VISIBLE_ITEMS + 1;
            }
            return true;
        }
    }
    return false;
}

uint8_t view_list_screen_get_selection(const view_list_screen_t *view) {
    return view ? view->selection : 0;
}

void view_list_screen_render(const view_list_screen_t *view, bool partial) {
    if (!view) return;

    Gdey029T94 &display = gui_get_display();
    gui_clear();

    draw_header(display, view->title ? view->title : "Menu");

    display.setFont(&FreeMonoBold9pt7b);

    if (view->item_count == 0) {
        display.setTextColor(EPD_BLACK);
        display.setCursor(10, 50);
        display.print("(empty)");
    } else {
        int y = 38;
        for (uint8_t i = 0; i < VIEW_LIST_VISIBLE_ITEMS && (view->scroll_pos + i) < view->item_count; i++) {
            uint8_t idx = view->scroll_pos + i;
            bool is_selected = (idx == view->selection);

            if (is_selected) {
                display.fillRect(0, y - 12, display.width(), 17, EPD_BLACK);
                display.setTextColor(EPD_WHITE);
            } else {
                display.setTextColor(EPD_BLACK);
            }

            display.setCursor(10, y);
            if (view->items[idx].shortcut) {
                display.print("[");
                display.print(view->items[idx].shortcut);
                display.print("] ");
            }
            display.print(view->items[idx].label ? view->items[idx].label : "");
            y += LINE_HEIGHT;
        }

        // Scroll indicators
        display.setTextColor(EPD_BLACK);
        if (view->scroll_pos > 0) {
            display.setCursor(display.width() - 15, 38);
            display.print("^");
        }
        if (view->scroll_pos + VIEW_LIST_VISIBLE_ITEMS < view->item_count) {
            display.setCursor(display.width() - 15, 38 + LINE_HEIGHT * (VIEW_LIST_VISIBLE_ITEMS - 1));
            display.print("v");
        }
    }

    // Footer with position indicator
    char hint[32];
    snprintf(hint, sizeof(hint), "%d/%d  [Y] Select", view->selection + 1, view->item_count);
    draw_hints(display, hint);
    gui_flush(!partial);
}

// ============================================================================
// T9 Input Screen View
// ============================================================================

void view_t9_input_init(view_t9_input_t *view, const char *title, const char *initial_text) {
    if (!view) return;

    view->title = title;
    memset(view->buffer, 0, sizeof(view->buffer));
    if (initial_text) {
        strncpy(view->buffer, initial_text, sizeof(view->buffer) - 1);
        view->len = strlen(view->buffer);
    } else {
        view->len = 0;
    }
    view->last_key = 0;
    view->char_index = 0;
    view->last_press_ms = 0;
    view->cursor_active = false;
}

bool view_t9_input_key(view_t9_input_t *view, char key) {
    if (!view) return false;
    if (key < '0' || key > '9') return false;

    uint32_t now = millis();
    bool same_key = (key == view->last_key);
    bool timeout = (now - view->last_press_ms) > VIEW_T9_TIMEOUT_MS;

    if (same_key && !timeout && view->len > 0) {
        // Cycle through characters for the same key
        view->char_index++;
        uint8_t char_count = view_t9_get_char_count(key);
        if (view->char_index >= char_count) {
            view->char_index = 0;
        }
        // Replace last character
        view->buffer[view->len - 1] = view_t9_get_char(key, view->char_index);
    } else {
        // New key or timeout - add new character
        if (view->len < VIEW_MAX_TEXT_LEN - 1) {
            view->char_index = 0;
            view->buffer[view->len] = view_t9_get_char(key, 0);
            view->len++;
            view->buffer[view->len] = '\0';
        }
    }

    view->last_key = key;
    view->last_press_ms = now;
    view->cursor_active = true;

    return true;
}

void view_t9_input_backspace(view_t9_input_t *view) {
    if (!view || view->len == 0) return;

    view->len--;
    view->buffer[view->len] = '\0';
    view->last_key = 0;
    view->cursor_active = false;
}

void view_t9_input_update(view_t9_input_t *view) {
    if (!view || view->last_key == 0) return;

    uint32_t now = millis();
    if ((now - view->last_press_ms) > VIEW_T9_TIMEOUT_MS) {
        view->last_key = 0;
        view->cursor_active = false;
    }
}

const char* view_t9_input_get_text(const view_t9_input_t *view) {
    return view ? view->buffer : "";
}

void view_t9_input_render(const view_t9_input_t *view, bool partial) {
    if (!view) return;

    Gdey029T94 &display = gui_get_display();
    gui_clear();

    draw_header(display, view->title ? view->title : "Input");

    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(EPD_BLACK);

    // Label
    display.setCursor(10, 42);
    display.print("Text:");

    // Current text with cursor
    display.setCursor(10, 62);
    for (uint8_t i = 0; i < view->len; i++) {
        display.print(view->buffer[i]);
    }
    // Show cursor
    if (view->cursor_active) {
        display.print("_");
    } else {
        display.print("|");
    }

    draw_hints(display, "[N] Del  [2s N] Abort  [Y] OK");
    gui_flush(!partial);
}

// ============================================================================
// Slider Screen View
// ============================================================================

void view_slider_init(view_slider_t *view, const char *title,
                      uint16_t min_val, uint16_t max_val, uint16_t initial,
                      uint16_t step, const char *format, const char *unit,
                      const char *hint) {
    if (!view) return;

    view->title = title;
    view->min_value = min_val;
    view->max_value = max_val;
    view->value = initial;
    view->step = step > 0 ? step : 1;
    view->format = format ? format : "%d";
    view->unit = unit ? unit : "";
    view->hint = hint ? hint : "[2/8] Adjust  [Y] OK";
}

void view_slider_adjust(view_slider_t *view, bool up) {
    if (!view) return;

    if (up) {
        if (view->value + view->step <= view->max_value) {
            view->value += view->step;
        } else {
            view->value = view->max_value;
        }
    } else {
        if (view->value >= view->min_value + view->step) {
            view->value -= view->step;
        } else {
            view->value = view->min_value;
        }
    }
}

uint16_t view_slider_get_value(const view_slider_t *view) {
    return view ? view->value : 0;
}

void view_slider_set_value(view_slider_t *view, uint16_t value) {
    if (!view) return;
    if (value < view->min_value) value = view->min_value;
    if (value > view->max_value) value = view->max_value;
    view->value = value;
}

void view_slider_render(const view_slider_t *view, bool partial) {
    if (!view) return;

    Gdey029T94 &display = gui_get_display();
    gui_clear();

    draw_header(display, view->title ? view->title : "Adjust");

    display.setFont(&FreeMonoBold12pt7b);
    display.setTextColor(EPD_BLACK);

    // Value display
    char value_str[32];
    snprintf(value_str, sizeof(value_str), view->format, view->value);

    // Center the value
    display.setCursor(80, 60);
    display.print(value_str);
    if (view->unit && view->unit[0]) {
        display.print(" ");
        display.print(view->unit);
    }

    // Visual bar
    display.setFont(&FreeMonoBold9pt7b);
    int bar_x = (display.width() - SLIDER_BAR_WIDTH) / 2;
    int bar_y = 80;

    // Calculate fill level
    uint16_t range = view->max_value - view->min_value;
    int filled = 0;
    if (range > 0) {
        filled = (SLIDER_BAR_WIDTH * (view->value - view->min_value)) / range;
    }

    display.drawRect(bar_x, bar_y, SLIDER_BAR_WIDTH, SLIDER_BAR_HEIGHT, EPD_BLACK);
    if (filled > 0) {
        display.fillRect(bar_x, bar_y, filled, SLIDER_BAR_HEIGHT, EPD_BLACK);
    }

    draw_hints(display, view->hint);
    gui_flush(!partial);
}

// ============================================================================
// PIN Entry Screen View
// ============================================================================

void view_pin_entry_init(view_pin_entry_t *view, const char *title,
                         uint8_t max_len, uint8_t max_attempts) {
    if (!view) return;

    view->title = title ? title : "Enter PIN";
    memset(view->buffer, 0, sizeof(view->buffer));
    view->len = 0;
    view->max_len = (max_len > 0 && max_len <= VIEW_PIN_MAX_LEN) ? max_len : VIEW_PIN_MAX_LEN;
    view->attempts = 0;
    view->max_attempts = max_attempts > 0 ? max_attempts : 3;
    view->message = NULL;
    view->locked = false;
    view->error_shown_ms = 0;
}

bool view_pin_entry_digit(view_pin_entry_t *view, char digit) {
    if (!view || view->locked) return false;
    if (digit < '0' || digit > '9') return false;
    if (view->len >= view->max_len) return false;

    view->buffer[view->len] = digit;
    view->len++;
    view->buffer[view->len] = '\0';
    return true;
}

void view_pin_entry_backspace(view_pin_entry_t *view) {
    if (!view || view->len == 0 || view->locked) return;

    view->len--;
    view->buffer[view->len] = '\0';
}

void view_pin_entry_clear(view_pin_entry_t *view) {
    if (!view) return;

    memset(view->buffer, 0, sizeof(view->buffer));
    view->len = 0;
}

const char* view_pin_entry_get_pin(const view_pin_entry_t *view) {
    return view ? view->buffer : "";
}

bool view_pin_entry_is_complete(const view_pin_entry_t *view) {
    return view && view->len == view->max_len;
}

bool view_pin_entry_fail(view_pin_entry_t *view) {
    if (!view) return true;

    view->attempts++;
    view->error_shown_ms = millis();  // Record when error occurred
    view_pin_entry_clear(view);

    if (view->attempts >= view->max_attempts) {
        view->locked = true;
        return true;
    }
    return false;
}

void view_pin_entry_set_message(view_pin_entry_t *view, const char *msg) {
    if (!view) return;
    view->message = msg;
}

void view_pin_entry_render(const view_pin_entry_t *view, bool partial) {
    if (!view) return;

    Gdey029T94 &display = gui_get_display();
    gui_clear();

    draw_header(display, view->title ? view->title : "Enter PIN");

    display.setTextColor(EPD_BLACK);
    display.setFont(&FreeMonoBold12pt7b);

    // PIN dots - centered
    int dot_spacing = 20;
    int dot_radius = 6;
    int total_width = view->max_len * dot_spacing;
    int start_x = (display.width() - total_width) / 2 + dot_spacing / 2;
    int dot_y = 55;

    for (uint8_t i = 0; i < view->max_len; i++) {
        int x = start_x + i * dot_spacing;
        if (i < view->len) {
            // Filled circle for entered digit
            display.fillCircle(x, dot_y, dot_radius, EPD_BLACK);
        } else {
            // Empty circle for remaining digits
            display.drawCircle(x, dot_y, dot_radius, EPD_BLACK);
        }
    }

    // Status/error message
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(10, 85);

    if (view->locked) {
        display.print("Device locked!");
    } else if (view->attempts > 0 && view->error_shown_ms > 0) {
        // Only show error message for a limited time
        uint32_t elapsed = millis() - view->error_shown_ms;
        if (elapsed < VIEW_PIN_ERROR_DURATION_MS) {
            int remaining = view->max_attempts - view->attempts;
            char msg[32];
            snprintf(msg, sizeof(msg), "%d attempt%s left",
                     remaining, remaining == 1 ? "" : "s");
            display.print(msg);
        }
    }

    // Optional message
    if (view->message && view->message[0]) {
        display.setCursor(10, 105);
        display.print(view->message);
    }

    draw_hints(display, "[0-9] Digit  [N] Del  [Y] OK");
    gui_flush(!partial);
}

// ============================================================================
// Toast/Message Overlay
// ============================================================================

#define TOAST_BOX_WIDTH 200
#define TOAST_BOX_HEIGHT 50
#define TOAST_ICON_SIZE 16

static void draw_toast_box(Gdey029T94 &display, const char *message, int icon_type) {
    // Calculate centered position
    int box_x = (display.width() - TOAST_BOX_WIDTH) / 2;
    int box_y = (display.height() - TOAST_BOX_HEIGHT) / 2;

    // Draw white box with black border
    display.fillRect(box_x, box_y, TOAST_BOX_WIDTH, TOAST_BOX_HEIGHT, EPD_WHITE);
    display.drawRect(box_x, box_y, TOAST_BOX_WIDTH, TOAST_BOX_HEIGHT, EPD_BLACK);
    display.drawRect(box_x + 1, box_y + 1, TOAST_BOX_WIDTH - 2, TOAST_BOX_HEIGHT - 2, EPD_BLACK);

    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(EPD_BLACK);

    int text_x = box_x + 10;
    int text_y = box_y + 30;

    // Draw icon if specified
    if (icon_type == 1) {
        // Success: checkmark
        int ix = box_x + 15;
        int iy = box_y + TOAST_BOX_HEIGHT / 2;
        display.drawLine(ix, iy, ix + 4, iy + 4, EPD_BLACK);
        display.drawLine(ix + 4, iy + 4, ix + 12, iy - 4, EPD_BLACK);
        display.drawLine(ix, iy + 1, ix + 4, iy + 5, EPD_BLACK);
        display.drawLine(ix + 4, iy + 5, ix + 12, iy - 3, EPD_BLACK);
        text_x = box_x + 35;
    } else if (icon_type == 2) {
        // Error: X
        int ix = box_x + 15;
        int iy = box_y + TOAST_BOX_HEIGHT / 2;
        display.drawLine(ix - 5, iy - 5, ix + 5, iy + 5, EPD_BLACK);
        display.drawLine(ix - 5, iy + 5, ix + 5, iy - 5, EPD_BLACK);
        display.drawLine(ix - 4, iy - 5, ix + 6, iy + 5, EPD_BLACK);
        display.drawLine(ix - 4, iy + 5, ix + 6, iy - 5, EPD_BLACK);
        text_x = box_x + 35;
    }

    display.setCursor(text_x, text_y);
    display.print(message);
}

void view_toast_show(const char *message, uint16_t duration_ms) {
    if (!message) return;

    Gdey029T94 &display = gui_get_display();
    draw_toast_box(display, message, 0);

    // Synchronous partial update for toast (must block)
    gui_flush_sync(false);

    // Block for duration
    delay(duration_ms);
}

void view_toast_success(const char *message, uint16_t duration_ms) {
    if (!message) return;

    Gdey029T94 &display = gui_get_display();
    draw_toast_box(display, message, 1);

    // Synchronous partial update for toast (must block)
    gui_flush_sync(false);

    // Block for duration
    delay(duration_ms);
}

void view_toast_error(const char *message, uint16_t duration_ms) {
    if (!message) return;

    Gdey029T94 &display = gui_get_display();
    draw_toast_box(display, message, 2);

    // Synchronous partial update for toast (must block)
    gui_flush_sync(false);

    // Block for duration
    delay(duration_ms);
}
