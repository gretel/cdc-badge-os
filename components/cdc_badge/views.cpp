// Views Module for CDC Badge
// Provides reusable screen components

#include "views.h"
#include "gui.h"
#include "cdc_log.h"
#include "cdc_time.h"
#include "i18n.h"
#include "pin_expander.h"
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

// Draw small tilted 'z' for light sleep
static void draw_light_sleep_icon(Gdey029T94 &display, int x, int y) {
    // Single small z, slightly tilted (italicized)
    int w = 6, h = 6;
    int tilt = 1;  // Pixel offset for italic effect
    display.drawLine(x + tilt, y, x + w + tilt, y, EPD_BLACK);        // top
    display.drawLine(x + w + tilt, y, x, y + h, EPD_BLACK);           // diagonal
    display.drawLine(x, y + h, x + w, y + h, EPD_BLACK);              // bottom
}

// Draw USB icon (simplified USB connector shape)
static void draw_usb_icon(Gdey029T94 &display, int x, int y) {
    // USB connector shape - simplified trident
    int w = 10, h = 12;
    // Main stem
    display.drawLine(x + w/2, y + 4, x + w/2, y + h, EPD_BLACK);
    // Top horizontal
    display.drawLine(x + 2, y + 4, x + w - 2, y + 4, EPD_BLACK);
    // Left branch
    display.drawLine(x + 2, y + 4, x + 2, y, EPD_BLACK);
    display.fillCircle(x + 2, y, 1, EPD_BLACK);
    // Right branch
    display.drawLine(x + w - 2, y + 4, x + w - 2, y + 2, EPD_BLACK);
    display.fillRect(x + w - 4, y, 4, 3, EPD_BLACK);
    // Bottom arrow
    display.drawLine(x + w/2, y + h, x + w/2 - 2, y + h - 2, EPD_BLACK);
    display.drawLine(x + w/2, y + h, x + w/2 + 2, y + h - 2, EPD_BLACK);
}

// Draw BLE icon (simplified Bluetooth rune)
static void draw_ble_icon_color(Gdey029T94 &display, int x, int y, uint16_t color) {
    int h = 12;
    // Central vertical
    display.drawLine(x + 4, y, x + 4, y + h, color);
    // Top right diagonal
    display.drawLine(x + 4, y, x + 8, y + 3, color);
    display.drawLine(x + 8, y + 3, x + 1, y + h/2, color);
    // Bottom right diagonal
    display.drawLine(x + 1, y + h/2, x + 8, y + h - 3, color);
    display.drawLine(x + 8, y + h - 3, x + 4, y + h, color);
}

static void draw_ble_icon(Gdey029T94 &display, int x, int y) {
    draw_ble_icon_color(display, x, y, EPD_BLACK);
}

// Draw comic-style sleep icon (stacked Z's getting smaller) for deep sleep
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

// Draw WiFi icon (3 signal arcs + dot at base)
static void draw_wifi_icon_color(Gdey029T94 &display, int x, int y, uint16_t color) {
    int cx = x + 6;  // center x
    int by = y + 12; // base y

    // Base dot
    display.fillCircle(cx, by, 1, color);

    // Signal arcs (from small to large)
    // Arc 1 (smallest)
    for (int i = -20; i <= 20; i++) {
        int px = cx + (i * 3 / 20);
        int py = by - 4 + (i * i / 100);
        display.drawPixel(px, py, color);
    }
    // Arc 2 (medium)
    for (int i = -30; i <= 30; i++) {
        int px = cx + (i * 5 / 30);
        int py = by - 7 + (i * i / 150);
        display.drawPixel(px, py, color);
    }
    // Arc 3 (largest)
    for (int i = -40; i <= 40; i++) {
        int px = cx + (i * 6 / 40);
        int py = by - 10 + (i * i / 180);
        display.drawPixel(px, py, color);
    }
}

static void draw_wifi_icon(Gdey029T94 &display, int x, int y) {
    draw_wifi_icon_color(display, x, y, EPD_BLACK);
}

static void draw_list_icon(Gdey029T94 &display, view_list_icon_t icon, bool disabled,
                           int x, int y, bool inverted) {
    if (icon == VIEW_LIST_ICON_NONE) return;

    uint16_t color = inverted ? EPD_WHITE : EPD_BLACK;
    switch (icon) {
        case VIEW_LIST_ICON_WIFI:
            draw_wifi_icon_color(display, x, y, color);
            break;
        case VIEW_LIST_ICON_BLE:
            draw_ble_icon_color(display, x, y, color);
            break;
        default:
            break;
    }

    if (disabled) {
        display.drawLine(x, y, x + 11, y + 11, color);
        display.drawLine(x, y + 11, x + 11, y, color);
    }
}

// ============================================================================
// Status Icon Rendering (right-to-left from x,y position)
// ============================================================================

int view_render_status_icons(uint16_t icons, int x, int y) {
    if (icons == ICON_NONE) return 0;

    Gdey029T94 &display = gui_get_display();
    int total_width = 0;
    int cur_x = x;

    // Icons are rendered right-to-left in priority order
    // Each icon function expects top-left corner

    if (icons & ICON_WIFI) {
        cur_x -= 12;
        draw_wifi_icon(display, cur_x, y);
        cur_x -= ICON_SPACING;
        total_width += 12 + ICON_SPACING;
    }

    if (icons & ICON_BLE) {
        cur_x -= 12;
        draw_ble_icon(display, cur_x, y);
        cur_x -= ICON_SPACING;
        total_width += 12 + ICON_SPACING;
    }

    if (icons & ICON_USB) {
        cur_x -= 12;
        draw_usb_icon(display, cur_x, y);
        cur_x -= ICON_SPACING;
        total_width += 12 + ICON_SPACING;
    }

    if (icons & ICON_BACKLIGHT) {
        cur_x -= 14;
        draw_sun_icon(display, cur_x + 7, y + 7);  // sun_icon uses center point
        cur_x -= ICON_SPACING;
        total_width += 14 + ICON_SPACING;
    }

    // Deep sleep and light sleep are mutually exclusive (deep sleep has priority)
    if (icons & ICON_DEEP_SLEEP) {
        cur_x -= 20;
        draw_sleep_icon(display, cur_x, y);
        cur_x -= ICON_SPACING;
        total_width += 20 + ICON_SPACING;
    } else if (icons & ICON_LIGHT_SLEEP) {
        cur_x -= 8;
        draw_light_sleep_icon(display, cur_x, y + 4);
        cur_x -= ICON_SPACING;
        total_width += 8 + ICON_SPACING;
    }

    if (icons & ICON_LOCK) {
        cur_x -= 12;
        draw_lock_icon(display, cur_x, y);
        cur_x -= ICON_SPACING;
        total_width += 12 + ICON_SPACING;
    }

    return total_width;
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

    // Clock and date next to battery (on same line)
    if (data->clock[0]) {
        display.setFont(&FreeMonoBold9pt7b);
        display.setCursor(ICON_MARGIN + 30, ICON_MARGIN + 12);
        display.print(data->clock);
        // Date after clock (if set)
        if (data->date[0]) {
            display.print(" ");
            display.print(data->date);
        }
    }

    // Build icon bitmask from both new field and legacy fields
    uint16_t icons = data->status_icons;
    if (data->backlight_on) icons |= ICON_BACKLIGHT;
    if (data->show_sleep_icon) icons |= ICON_DEEP_SLEEP;
    if (data->show_light_sleep_icon) icons |= ICON_LIGHT_SLEEP;
    if (data->show_lock_icon) icons |= ICON_LOCK;

    // Render all status icons (right-to-left from top-right corner)
    view_render_status_icons(icons, display.width() - ICON_MARGIN, ICON_MARGIN);

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
        draw_hints(display, i18n_str(STR_PRESS_ANY_KEY));
    } else {
        draw_hints(display, i18n_str(STR_UNLOCK));
    }
    gui_flush(false);  // Always partial refresh
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
    if (!view || view->total_lines == 0) return;

    if (down) {
        if (view->scroll_offset + view->visible_lines < view->total_lines) {
            view->scroll_offset++;
        } else {
            // Wrap to top
            view->scroll_offset = 0;
        }
    } else {
        if (view->scroll_offset > 0) {
            view->scroll_offset--;
        } else {
            // Wrap to bottom
            if (view->total_lines > view->visible_lines) {
                view->scroll_offset = view->total_lines - view->visible_lines;
            }
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

    // Footer with line range (e.g., "1-4/8")
    char hint[32];
    uint16_t first_line = view->scroll_offset + 1;
    uint16_t last_line = view->scroll_offset + view->visible_lines;
    if (last_line > view->total_lines) last_line = view->total_lines;
    snprintf(hint, sizeof(hint), "%d-%d/%d  %s", first_line, last_line, view->total_lines, i18n_str(STR_HINT_BACK));
    draw_hints(display, hint);
    gui_flush(false);  // Always partial refresh
}

// ============================================================================
// List Screen View
// ============================================================================

void view_list_screen_init(view_list_screen_t *view, const char *title,
                           const view_list_item_t *items, uint16_t count) {
    if (!view) return;

    view->title = title;
    view->items = items;
    view->item_count = count;
    view->selection = 0;
    view->scroll_pos = 0;
    view->hint = NULL;  // Use default hint
}

void view_list_screen_navigate(view_list_screen_t *view, bool down) {
    if (!view || view->item_count == 0) return;

    if (down) {
        if (view->selection < view->item_count - 1) {
            view->selection++;
        } else {
            // Wrap to top
            view->selection = 0;
            view->scroll_pos = 0;
        }
        // Adjust scroll if needed
        if (view->selection >= view->scroll_pos + VIEW_LIST_VISIBLE_ITEMS) {
            view->scroll_pos = view->selection - VIEW_LIST_VISIBLE_ITEMS + 1;
        }
    } else {
        if (view->selection > 0) {
            view->selection--;
        } else {
            // Wrap to bottom
            view->selection = view->item_count - 1;
            if (view->item_count > VIEW_LIST_VISIBLE_ITEMS) {
                view->scroll_pos = view->item_count - VIEW_LIST_VISIBLE_ITEMS;
            }
        }
        // Adjust scroll if needed
        if (view->selection < view->scroll_pos) {
            view->scroll_pos = view->selection;
        }
    }
}

uint16_t view_list_screen_get_selection(const view_list_screen_t *view) {
    return view ? view->selection : 0;
}

void view_list_screen_render(const view_list_screen_t *view, bool partial) {
    if (!view) return;

    Gdey029T94 &display = gui_get_display();
    gui_clear();

    draw_header(display, view->title ? view->title : "Menu");

    display.setFont(&FreeMonoBold9pt7b);

    bool has_icons = false;
    for (uint16_t i = 0; i < view->item_count; i++) {
        if (view->items[i].icon != VIEW_LIST_ICON_NONE) {
            has_icons = true;
            break;
        }
    }

    if (view->item_count == 0) {
        display.setTextColor(EPD_BLACK);
        display.setCursor(10, 50);
        display.print("(empty)");
    } else {
        int y = 38;
        int icon_x = 10;
        int label_x = has_icons ? 28 : 10;
        for (uint16_t i = 0; i < VIEW_LIST_VISIBLE_ITEMS && (view->scroll_pos + i) < view->item_count; i++) {
            uint16_t idx = view->scroll_pos + i;
            bool is_selected = (idx == view->selection);

            if (is_selected) {
                display.fillRect(0, y - 12, display.width(), 17, EPD_BLACK);
                display.setTextColor(EPD_WHITE);
            } else {
                display.setTextColor(EPD_BLACK);
            }

            if (has_icons) {
                draw_list_icon(display, (view_list_icon_t)view->items[idx].icon,
                               view->items[idx].icon_disabled,
                               icon_x, y - 12, is_selected);
            }

            display.setCursor(label_x, y);
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
    char hint[64];
    if (view->hint) {
        // Custom hint with position prefix
        snprintf(hint, sizeof(hint), "%d/%d  %s", view->selection + 1, view->item_count, view->hint);
    } else {
        // Default hint
        snprintf(hint, sizeof(hint), "%d/%d  %s", view->selection + 1, view->item_count, i18n_str(STR_HINT_SELECT));
    }
    draw_hints(display, hint);
    gui_flush(false);  // Always partial refresh
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

void view_t9_input_force_digit(view_t9_input_t *view, char key) {
    if (!view) return;
    if (key < '0' || key > '9') return;

    if (view->len > 0 && view->last_key == key && view->cursor_active) {
        view->buffer[view->len - 1] = key;
    } else if (view->len < VIEW_MAX_TEXT_LEN - 1) {
        view->buffer[view->len] = key;
        view->len++;
        view->buffer[view->len] = '\0';
    }

    view->last_key = 0;
    view->char_index = 0;
    view->last_press_ms = 0;
    view->cursor_active = false;
}

bool view_t9_input_update(view_t9_input_t *view) {
    if (!view || view->last_key == 0) return false;

    uint32_t now = millis();
    if ((now - view->last_press_ms) > VIEW_T9_TIMEOUT_MS) {
        view->last_key = 0;
        view->cursor_active = false;
        return true;
    }
    return false;
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

    // Current text
    display.setCursor(10, 62);
    bool cycling = (view->cursor_active && view->last_key != 0 && view->len > 0);

    if (cycling) {
        // Still iterating through characters - show all but last char, then last char with underline
        for (uint8_t i = 0; i < view->len - 1; i++) {
            display.print(view->buffer[i]);
        }
        // Show last character (the one being cycled)
        display.print(view->buffer[view->len - 1]);
        // Underline under current character - backspace and draw underscore
        int16_t cursor_x = display.getCursorX();
        display.setCursor(cursor_x - 11, 62);  // Move back under last char
        display.print("_");
    } else {
        // Not cycling - show all text with cursor at end
        for (uint8_t i = 0; i < view->len; i++) {
            display.print(view->buffer[i]);
        }
        // Show cursor at insert position
        display.print("|");
    }

    draw_hints(display, i18n_str(STR_HINT_T9_INPUT));
    gui_flush(false);  // Always partial refresh
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
    view->display_offset = 0;
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

    // Value display (with optional offset for signed display)
    char value_str[32];
    int16_t display_value = (int16_t)view->value + view->display_offset;
    snprintf(value_str, sizeof(value_str), view->format, display_value);

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
    gui_flush(false);  // Always partial refresh
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

    draw_hints(display, i18n_str(STR_HINT_PIN_INPUT));
    gui_flush(false);  // Always partial refresh
}

// ============================================================================
// Date Input Screen View
// ============================================================================

void view_date_input_init(view_date_input_t *view, uint8_t day, uint8_t month, uint16_t year) {
    if (!view) return;
    view->day = day > 0 ? day : 1;
    view->month = month > 0 ? month : 1;
    view->year = year > 0 ? year : 2025;
    view->field = 0;
    view->digit = 0;
}

bool view_date_input_key(view_date_input_t *view, char key) {
    if (!view || key < '0' || key > '9') return false;

    uint8_t d = key - '0';

    if (view->field == 0) {  // Day (01-31)
        if (view->digit == 0) {
            view->day = d * 10 + (view->day % 10);
            view->digit = 1;
        } else {
            view->day = (view->day / 10) * 10 + d;
            if (view->day > 31) view->day = 31;
            if (view->day == 0) view->day = 1;
            view->digit = 0;
            view->field = 1;  // Auto-advance to month
        }
    } else if (view->field == 1) {  // Month (01-12)
        if (view->digit == 0) {
            view->month = d * 10 + (view->month % 10);
            view->digit = 1;
        } else {
            view->month = (view->month / 10) * 10 + d;
            if (view->month > 12) view->month = 12;
            if (view->month == 0) view->month = 1;
            view->digit = 0;
            view->field = 2;  // Auto-advance to year
        }
    } else {  // Year (4 digits)
        uint16_t y = view->year;
        if (view->digit == 0) {
            y = d * 1000 + (y % 1000);
        } else if (view->digit == 1) {
            y = (y / 1000) * 1000 + d * 100 + (y % 100);
        } else if (view->digit == 2) {
            y = (y / 100) * 100 + d * 10 + (y % 10);
        } else {
            y = (y / 10) * 10 + d;
        }
        view->year = y;
        view->digit = (view->digit + 1) % 4;
        if (view->digit == 0) {
            // Year complete, stay on year field
        }
    }
    return true;
}

void view_date_input_next_field(view_date_input_t *view) {
    if (!view) return;
    view->field = (view->field + 1) % 3;
    view->digit = 0;
}

void view_date_input_prev_field(view_date_input_t *view) {
    if (!view) return;
    view->field = view->field > 0 ? view->field - 1 : 2;
    view->digit = 0;
}

bool view_date_input_clear_field(view_date_input_t *view) {
    if (!view) return false;

    // If digit > 0, just reset digit position (backspace within field)
    if (view->digit > 0) {
        view->digit = 0;
        // Reset current field value to default
        if (view->field == 0) view->day = 1;
        else if (view->field == 1) view->month = 1;
        else view->year = 2025;
        return true;
    }

    // digit == 0: go to previous field if not at start
    if (view->field > 0) {
        view->field--;
        view->digit = 0;
        // Reset the field we're now on
        if (view->field == 0) view->day = 1;
        else if (view->field == 1) view->month = 1;
        return true;
    }

    // At field 0, digit 0: nothing to clear
    return false;
}

void view_date_input_render(const view_date_input_t *view, bool partial) {
    if (!view) return;

    Gdey029T94 &display = gui_get_display();
    gui_clear();

    draw_header(display, "Set Date");

    display.setTextColor(EPD_BLACK);
    display.setFont(&FreeMonoBold12pt7b);

    // Date display: DD / MM / YYYY
    char date_str[20];
    snprintf(date_str, sizeof(date_str), "%02d / %02d / %04d",
             view->day, view->month, view->year);

    display.setCursor(30, 55);
    display.print(date_str);

    // Underline current field
    int underline_x = 30;
    int underline_w = 26;
    if (view->field == 0) {
        underline_x = 30;  // Day
    } else if (view->field == 1) {
        underline_x = 30 + 52;  // Month (after "DD / ")
    } else {
        underline_x = 30 + 104;  // Year (after "DD / MM / ")
        underline_w = 52;  // 4 digits
    }
    display.fillRect(underline_x, 60, underline_w, 3, EPD_BLACK);

    draw_hints(display, i18n_str(STR_HINT_DATE_TIME));
    gui_flush(false);
}

// ============================================================================
// Time Input Screen View
// ============================================================================

void view_time_input_init(view_time_input_t *view, uint8_t hour, uint8_t minute) {
    if (!view) return;
    view->hour = hour < 24 ? hour : 0;
    view->minute = minute < 60 ? minute : 0;
    view->field = 0;
    view->digit = 0;
}

bool view_time_input_key(view_time_input_t *view, char key) {
    if (!view || key < '0' || key > '9') return false;

    uint8_t d = key - '0';

    if (view->field == 0) {  // Hour (00-23)
        if (view->digit == 0) {
            view->hour = d * 10 + (view->hour % 10);
            view->digit = 1;
        } else {
            view->hour = (view->hour / 10) * 10 + d;
            if (view->hour > 23) view->hour = 23;
            view->digit = 0;
            view->field = 1;  // Auto-advance to minute
        }
    } else {  // Minute (00-59)
        if (view->digit == 0) {
            view->minute = d * 10 + (view->minute % 10);
            view->digit = 1;
        } else {
            view->minute = (view->minute / 10) * 10 + d;
            if (view->minute > 59) view->minute = 59;
            view->digit = 0;
            // Stay on minute field after complete
        }
    }
    return true;
}

void view_time_input_next_field(view_time_input_t *view) {
    if (!view) return;
    view->field = (view->field + 1) % 2;
    view->digit = 0;
}

void view_time_input_prev_field(view_time_input_t *view) {
    if (!view) return;
    view->field = view->field > 0 ? view->field - 1 : 1;
    view->digit = 0;
}

bool view_time_input_clear_field(view_time_input_t *view) {
    if (!view) return false;

    // If digit > 0, just reset digit position (backspace within field)
    if (view->digit > 0) {
        view->digit = 0;
        // Reset current field value to default
        if (view->field == 0) view->hour = 0;
        else view->minute = 0;
        return true;
    }

    // digit == 0: go to previous field if not at start
    if (view->field > 0) {
        view->field--;
        view->digit = 0;
        // Reset the field we're now on
        view->hour = 0;
        return true;
    }

    // At field 0, digit 0: nothing to clear
    return false;
}

void view_time_input_render(const view_time_input_t *view, bool partial) {
    if (!view) return;

    Gdey029T94 &display = gui_get_display();
    gui_clear();

    draw_header(display, "Set Time");

    display.setTextColor(EPD_BLACK);
    display.setFont(&FreeMonoBold12pt7b);

    // Time display: HH : MM
    char time_str[12];
    snprintf(time_str, sizeof(time_str), "%02d : %02d",
             view->hour, view->minute);

    display.setCursor(70, 55);
    display.print(time_str);

    // Underline current field
    int underline_x = 70;
    if (view->field == 1) {
        underline_x = 70 + 52;  // Minutes (after "HH : ")
    }
    display.fillRect(underline_x, 60, 26, 3, EPD_BLACK);

    draw_hints(display, i18n_str(STR_HINT_DATE_TIME));
    gui_flush(false);
}

// ============================================================================
// IP Input Screen View
// ============================================================================

static bool parse_ip_string(const char *ip_str, uint8_t octet[4]) {
    if (!ip_str || !octet) return false;

    int a = 0;
    int b = 0;
    int c = 0;
    int d = 0;
    if (sscanf(ip_str, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) {
        return false;
    }

    if (a < 0 || a > 255 || b < 0 || b > 255 || c < 0 || c > 255 || d < 0 || d > 255) {
        return false;
    }

    octet[0] = (uint8_t)a;
    octet[1] = (uint8_t)b;
    octet[2] = (uint8_t)c;
    octet[3] = (uint8_t)d;
    return true;
}

void view_ip_input_init(view_ip_input_t *view, const char *title, const char *initial_ip) {
    if (!view) return;

    view->title = title;
    view->field = 0;
    view->digit = 0;
    view->octet[0] = 0;
    view->octet[1] = 0;
    view->octet[2] = 0;
    view->octet[3] = 0;

    if (initial_ip && initial_ip[0]) {
        parse_ip_string(initial_ip, view->octet);
    }
}

bool view_ip_input_key(view_ip_input_t *view, char key) {
    if (!view || key < '0' || key > '9') return false;

    if (view->field > 3) return false;

    uint8_t d = (uint8_t)(key - '0');
    uint8_t field = view->field;
    uint8_t digit = view->digit;

    if (digit == 0) {
        view->octet[field] = d;
        view->digit = 1;
    } else if (digit < 3) {
        uint16_t value = (uint16_t)view->octet[field] * 10 + d;
        if (value > 255) value = 255;
        view->octet[field] = (uint8_t)value;
        view->digit++;
    } else {
        return false;
    }

    if (view->digit >= 3) {
        if (view->field < 3) {
            view->field++;
            view->digit = 0;
        } else {
            view->digit = 3;
        }
    }

    return true;
}

void view_ip_input_next_field(view_ip_input_t *view) {
    if (!view) return;
    if (view->field < 3) {
        view->field++;
        view->digit = 0;
    }
}

void view_ip_input_prev_field(view_ip_input_t *view) {
    if (!view) return;
    if (view->field > 0) {
        view->field--;
        view->digit = 0;
    }
}

bool view_ip_input_clear_field(view_ip_input_t *view) {
    if (!view) return false;

    if (view->digit > 0) {
        view->octet[view->field] = 0;
        view->digit = 0;
        return true;
    }

    if (view->field > 0) {
        view->field--;
        view->digit = 0;
        return true;
    }

    return false;
}

void view_ip_input_render(const view_ip_input_t *view, bool partial) {
    if (!view) return;

    Gdey029T94 &display = gui_get_display();
    gui_clear();

    draw_header(display, view->title ? view->title : "IP");

    display.setTextColor(EPD_BLACK);
    display.setFont(&FreeMonoBold12pt7b);

    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    display.getTextBounds("000.000.000.000", 0, 0, &x1, &y1, &w, &h);
    (void)x1;
    (void)y1;
    (void)h;
    int start_x = (display.width() - (int)w) / 2;
    int y = 55;

    int field_start[4] = {0, 0, 0, 0};
    int field_end[4] = {0, 0, 0, 0};

    display.setCursor(start_x, y);
    for (int i = 0; i < 4; i++) {
        field_start[i] = display.getCursorX();
        char buf[4];
        snprintf(buf, sizeof(buf), "%03u", view->octet[i]);
        display.print(buf);
        field_end[i] = display.getCursorX();
        if (i < 3) {
            display.print(".");
        }
    }

    int underline_x = field_start[view->field];
    int underline_w = field_end[view->field] - field_start[view->field];
    if (underline_w < 1) underline_w = 1;
    display.fillRect(underline_x, y + 5, underline_w, 3, EPD_BLACK);

    draw_hints(display, i18n_str(STR_HINT_IP_INPUT));
    gui_flush(false);
}

// ============================================================================
// TOTP Code View (with progress bar)
// ============================================================================

#define TOTP_PROGRESS_BAR_WIDTH 200
#define TOTP_PROGRESS_BAR_HEIGHT 10
#define TOTP_PROGRESS_BAR_X 40
#define TOTP_PROGRESS_BAR_Y 95

void view_totp_code_init(view_totp_code_t *view, const char *issuer, const char *name,
                         const char *code, uint8_t digits, uint32_t period,
                         int8_t remaining, const char *hint) {
    if (!view) return;

    memset(view, 0, sizeof(*view));

    if (issuer) {
        strncpy(view->issuer, issuer, VIEW_MAX_TEXT_LEN - 1);
    }
    if (name) {
        strncpy(view->name, name, VIEW_MAX_TEXT_LEN - 1);
    }
    if (code) {
        strncpy(view->code, code, sizeof(view->code) - 1);
    }
    view->digits = digits > 0 ? digits : 6;
    view->period = period > 0 ? period : 30;
    view->remaining = remaining;
    view->hint = hint;
}

void view_totp_code_update(view_totp_code_t *view, const char *code, int8_t remaining) {
    if (!view) return;

    if (code) {
        strncpy(view->code, code, sizeof(view->code) - 1);
    }
    view->remaining = remaining;
}

void view_totp_code_render(const view_totp_code_t *view, bool partial) {
    if (!view) return;

    Gdey029T94 &display = gui_get_display();
    gui_clear();

    // Header with issuer (or name if no issuer)
    if (view->issuer[0]) {
        draw_header(display, view->issuer);
    } else if (view->name[0]) {
        draw_header(display, view->name);
    } else {
        draw_header(display, "TOTP");
    }

    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(EPD_BLACK);

    // Show account name below header if issuer was used
    if (view->issuer[0] && view->name[0]) {
        display.setCursor(10, 42);
        display.print(view->name);
    }

    // Format code with space in middle: "123 456" or "1234 5678"
    char formatted[12];
    if (view->code[0]) {
        if (view->digits == 8) {
            snprintf(formatted, sizeof(formatted), "%.4s %.4s", view->code, view->code + 4);
        } else {
            snprintf(formatted, sizeof(formatted), "%.3s %.3s", view->code, view->code + 3);
        }
    } else {
        strcpy(formatted, "--- ---");
    }

    // Large code display (centered)
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(50, 75);
    display.print(formatted);

    // Time remaining progress bar
    display.setFont(&FreeMonoBold9pt7b);

    if (view->remaining >= 0) {
        // Calculate filled portion
        int filled = (view->remaining * TOTP_PROGRESS_BAR_WIDTH) / view->period;
        if (filled > TOTP_PROGRESS_BAR_WIDTH) filled = TOTP_PROGRESS_BAR_WIDTH;

        // Draw progress bar outline
        display.drawRect(TOTP_PROGRESS_BAR_X, TOTP_PROGRESS_BAR_Y,
                         TOTP_PROGRESS_BAR_WIDTH, TOTP_PROGRESS_BAR_HEIGHT, EPD_BLACK);

        // Draw filled portion
        if (filled > 0) {
            display.fillRect(TOTP_PROGRESS_BAR_X, TOTP_PROGRESS_BAR_Y,
                             filled, TOTP_PROGRESS_BAR_HEIGHT, EPD_BLACK);
        }

        // Show seconds remaining
        char sec_str[8];
        snprintf(sec_str, sizeof(sec_str), "%ds", view->remaining);
        display.setCursor(TOTP_PROGRESS_BAR_X + TOTP_PROGRESS_BAR_WIDTH + 5, 103);
        display.print(sec_str);
    } else {
        // Time not synced warning
        display.setCursor(30, 100);
        display.print(i18n_str(STR_TIME_NOT_SYNCED));
    }

    draw_hints(display, view->hint ? view->hint : i18n_str(STR_HINT_BACK));
    gui_flush(false);
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

// Wait for duration, but allow early exit on Y/N keypress
static void toast_wait(uint16_t duration_ms) {
    uint32_t start = millis();
    while (millis() - start < duration_ms) {
        char key = pin_expander_get_key();
        if (key == 'Y' || key == 'N') {
            break;  // Early exit on confirmation key
        }
        delay(10);  // Small delay to avoid busy-waiting
    }
}

void view_toast_show(const char *message, uint16_t duration_ms) {
    if (!message) return;

    Gdey029T94 &display = gui_get_display();
    draw_toast_box(display, message, 0);

    // Synchronous partial update for toast (must block)
    gui_flush_sync(false);

    // Wait for duration (interruptable by Y/N)
    toast_wait(duration_ms);
}

void view_toast_success(const char *message, uint16_t duration_ms) {
    if (!message) return;

    Gdey029T94 &display = gui_get_display();
    draw_toast_box(display, message, 1);

    // Synchronous partial update for toast (must block)
    gui_flush_sync(false);

    // Wait for duration (interruptable by Y/N)
    toast_wait(duration_ms);
}

void view_toast_error(const char *message, uint16_t duration_ms) {
    if (!message) return;

    Gdey029T94 &display = gui_get_display();
    draw_toast_box(display, message, 2);

    // Synchronous partial update for toast (must block)
    gui_flush_sync(false);

    // Wait for duration (interruptable by Y/N)
    toast_wait(duration_ms);
}

void view_toast_render_success(const char *message) {
    if (!message) return;

    Gdey029T94 &display = gui_get_display();
    draw_toast_box(display, message, 1);

    // Synchronous partial update for toast (no wait)
    gui_flush_sync(false);
}

// ============================================================================
// WiFi List View
// ============================================================================

// Draw signal strength bars (4 bars, each 3px wide, 2px gap)
// Heights: 4, 7, 10, 13 pixels from bottom
static void draw_signal_bars(Gdey029T94 &display, int x, int y, int8_t rssi, bool inverted) {
    // Determine number of bars based on RSSI
    // Excellent: > -50 dBm (4 bars)
    // Good: -50 to -60 dBm (3 bars)
    // Fair: -60 to -70 dBm (2 bars)
    // Weak: < -70 dBm (1 bar)
    int bars;
    if (rssi > -50) bars = 4;
    else if (rssi > -60) bars = 3;
    else if (rssi > -70) bars = 2;
    else bars = 1;

    uint16_t fg = inverted ? EPD_WHITE : EPD_BLACK;

    int bar_width = 3;
    int gap = 1;
    int base_y = y + 13;  // Bottom of tallest bar

    for (int i = 0; i < 4; i++) {
        int bar_height = 4 + i * 3;  // 4, 7, 10, 13
        int bx = x + i * (bar_width + gap);
        int by = base_y - bar_height;

        if (i < bars) {
            // Filled bar
            display.fillRect(bx, by, bar_width, bar_height, fg);
        } else {
            // Empty bar (outline only)
            display.drawRect(bx, by, bar_width, bar_height, fg);
        }
    }
}

// Draw lock icon (simple padlock)
static void draw_lock_icon(Gdey029T94 &display, int x, int y, bool inverted) {
    uint16_t fg = inverted ? EPD_WHITE : EPD_BLACK;

    // Lock body (filled rectangle)
    display.fillRect(x, y + 5, 9, 7, fg);

    // Lock shackle (arc at top)
    display.drawRect(x + 2, y, 5, 6, fg);
    display.drawRect(x + 3, y + 1, 3, 4, inverted ? EPD_BLACK : EPD_WHITE);
}

void view_wifi_list_init(view_wifi_list_t *view, const char *title) {
    if (!view) return;
    view->title = title;
    view->item_count = 0;
    view->selection = 0;
    view->scroll_pos = 0;
}

int view_wifi_list_add(view_wifi_list_t *view, const char *ssid, int8_t rssi, uint8_t auth_mode) {
    if (!view || view->item_count >= VIEW_WIFI_MAX_ITEMS) return -1;

    int idx = view->item_count;
    strncpy(view->items[idx].ssid, ssid, sizeof(view->items[idx].ssid) - 1);
    view->items[idx].ssid[sizeof(view->items[idx].ssid) - 1] = '\0';
    view->items[idx].rssi = rssi;
    view->items[idx].auth_mode = auth_mode;
    view->item_count++;

    return idx;
}

void view_wifi_list_clear(view_wifi_list_t *view) {
    if (!view) return;
    view->item_count = 0;
    view->selection = 0;
    view->scroll_pos = 0;
}

void view_wifi_list_sort(view_wifi_list_t *view) {
    if (!view || view->item_count < 2) return;

    // Simple bubble sort by RSSI (descending - strongest first)
    for (uint16_t i = 0; i < view->item_count - 1; i++) {
        for (uint16_t j = 0; j < view->item_count - i - 1; j++) {
            if (view->items[j].rssi < view->items[j + 1].rssi) {
                // Swap
                view_wifi_item_t tmp = view->items[j];
                view->items[j] = view->items[j + 1];
                view->items[j + 1] = tmp;
            }
        }
    }
}

void view_wifi_list_navigate(view_wifi_list_t *view, bool down) {
    if (!view || view->item_count == 0) return;

    if (down) {
        if (view->selection < view->item_count - 1) {
            view->selection++;
        } else {
            view->selection = 0;
            view->scroll_pos = 0;
        }
        if (view->selection >= view->scroll_pos + VIEW_LIST_VISIBLE_ITEMS) {
            view->scroll_pos = view->selection - VIEW_LIST_VISIBLE_ITEMS + 1;
        }
    } else {
        if (view->selection > 0) {
            view->selection--;
        } else {
            view->selection = view->item_count - 1;
            if (view->item_count > VIEW_LIST_VISIBLE_ITEMS) {
                view->scroll_pos = view->item_count - VIEW_LIST_VISIBLE_ITEMS;
            }
        }
        if (view->selection < view->scroll_pos) {
            view->scroll_pos = view->selection;
        }
    }
}

uint16_t view_wifi_list_get_selection(const view_wifi_list_t *view) {
    return view ? view->selection : 0;
}

void view_wifi_list_render(const view_wifi_list_t *view, bool partial) {
    if (!view) return;

    Gdey029T94 &display = gui_get_display();
    gui_clear();

    draw_header(display, view->title ? view->title : "WiFi");

    display.setFont(&FreeMonoBold9pt7b);

    if (view->item_count == 0) {
        display.setTextColor(EPD_BLACK);
        display.setCursor(10, 50);
        display.print("(empty)");
    } else {
        int y = 38;
        for (uint16_t i = 0; i < VIEW_LIST_VISIBLE_ITEMS && (view->scroll_pos + i) < view->item_count; i++) {
            uint16_t idx = view->scroll_pos + i;
            const view_wifi_item_t *item = &view->items[idx];
            bool is_selected = (idx == view->selection);

            if (is_selected) {
                display.fillRect(0, y - 12, display.width(), 17, EPD_BLACK);
                display.setTextColor(EPD_WHITE);
            } else {
                display.setTextColor(EPD_BLACK);
            }

            // Draw signal bars (x=4, centered vertically in row)
            draw_signal_bars(display, 4, y - 11, item->rssi, is_selected);

            // Draw SSID (truncated to ~14 chars to leave room for auth+lock)
            char ssid_display[15];
            strncpy(ssid_display, item->ssid, 14);
            ssid_display[14] = '\0';

            display.setCursor(22, y);
            display.print(ssid_display);

            // Draw lock icon for encrypted networks
            if (item->auth_mode != 0) {
                draw_lock_icon(display, display.width() - 15, y - 10, is_selected);
            }

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

    // Footer
    char hint[64];
    snprintf(hint, sizeof(hint), "%d/%d  %s", view->selection + 1, view->item_count, i18n_str(STR_HINT_SELECT));
    draw_hints(display, hint);
    gui_flush(false);
}

// ============================================================================
// Context Menu Overlay
// ============================================================================

#define CTX_BOX_WIDTH 140
#define CTX_TITLE_HEIGHT 18
#define CTX_ITEM_HEIGHT 16
#define CTX_PADDING 4

void view_context_menu_init(view_context_menu_t *menu, const char *title,
                            const view_context_item_t *items, uint8_t count) {
    if (!menu) return;
    // Copy title to avoid dangling pointer from stack variables
    if (title) {
        strncpy(menu->title, title, VIEW_CONTEXT_TITLE_LEN - 1);
        menu->title[VIEW_CONTEXT_TITLE_LEN - 1] = '\0';
    } else {
        menu->title[0] = '\0';
    }
    menu->items = items;
    menu->item_count = (count > VIEW_CONTEXT_MAX_ITEMS) ? VIEW_CONTEXT_MAX_ITEMS : count;
    menu->selection = 0;
    menu->visible = false;
}

void view_context_menu_show(view_context_menu_t *menu) {
    if (!menu) return;
    menu->visible = true;
    menu->selection = 0;
    view_context_menu_render(menu);
}

void view_context_menu_hide(view_context_menu_t *menu) {
    if (!menu) return;
    menu->visible = false;
}

void view_context_menu_navigate(view_context_menu_t *menu, bool down) {
    if (!menu || menu->item_count == 0) return;

    if (down) {
        menu->selection = (menu->selection + 1) % menu->item_count;
    } else {
        menu->selection = (menu->selection == 0) ? menu->item_count - 1 : menu->selection - 1;
    }
}

void view_context_menu_render(const view_context_menu_t *menu) {
    if (!menu || !menu->visible || !menu->items) return;

    Gdey029T94 &display = gui_get_display();

    int box_height = CTX_TITLE_HEIGHT + (menu->item_count * CTX_ITEM_HEIGHT) + CTX_PADDING * 2;
    int box_x = (display.width() - CTX_BOX_WIDTH) / 2;
    int box_y = (display.height() - box_height) / 2;

    // White box with border
    display.fillRect(box_x, box_y, CTX_BOX_WIDTH, box_height, EPD_WHITE);
    display.drawRect(box_x, box_y, CTX_BOX_WIDTH, box_height, EPD_BLACK);
    display.drawRect(box_x + 1, box_y + 1, CTX_BOX_WIDTH - 2, box_height - 2, EPD_BLACK);

    // Title bar (inverted)
    display.fillRect(box_x + 2, box_y + 2, CTX_BOX_WIDTH - 4, CTX_TITLE_HEIGHT - 2, EPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(EPD_WHITE);
    display.setCursor(box_x + CTX_PADDING + 2, box_y + 13);
    if (menu->title[0]) {
        display.print(menu->title);
    }

    // Menu items
    int y = box_y + CTX_TITLE_HEIGHT + CTX_PADDING;
    for (uint8_t i = 0; i < menu->item_count; i++) {
        bool is_selected = (i == menu->selection);

        if (is_selected) {
            display.fillRect(box_x + 2, y - 2, CTX_BOX_WIDTH - 4, CTX_ITEM_HEIGHT, EPD_BLACK);
            display.setTextColor(EPD_WHITE);
        } else {
            display.setTextColor(EPD_BLACK);
        }

        display.setCursor(box_x + CTX_PADDING + 4, y + 10);
        if (is_selected) {
            display.print("> ");
        } else {
            display.print("  ");
        }
        if (menu->items[i].label) {
            display.print(menu->items[i].label);
        }

        y += CTX_ITEM_HEIGHT;
    }

    gui_flush_sync(false);
}

uint8_t view_context_menu_get_action(const view_context_menu_t *menu) {
    if (!menu || !menu->items || menu->selection >= menu->item_count) return 0;
    return menu->items[menu->selection].action_id;
}

bool view_context_menu_is_visible(const view_context_menu_t *menu) {
    return menu && menu->visible;
}

// ============================================================================
// QR Code View
// ============================================================================

#include "qrcode.h"

// QR code display callback context
static struct {
    int offset_x;
    int offset_y;
    int scale;
    int actual_size;    // Filled during sizing pass
    bool sizing_pass;   // True = just measure, don't draw
} qr_render_ctx;

// QR code display callback - draws directly to e-paper (or just measures)
static void qr_display_callback(esp_qrcode_handle_t qrcode) {
    int size = esp_qrcode_get_size(qrcode);
    qr_render_ctx.actual_size = size;

    // If sizing pass, just record size and return
    if (qr_render_ctx.sizing_pass) {
        return;
    }

    Gdey029T94 &display = gui_get_display();
    int scale = qr_render_ctx.scale;
    int x0 = qr_render_ctx.offset_x;
    int y0 = qr_render_ctx.offset_y;

    LOG_D("VIEW", "QR size=%d, scale=%d, pos=(%d,%d)", size, scale, x0, y0);

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            bool black = esp_qrcode_get_module(qrcode, x, y);
            uint16_t color = black ? EPD_BLACK : EPD_WHITE;

            // Draw scaled pixel
            for (int dy = 0; dy < scale; dy++) {
                for (int dx = 0; dx < scale; dx++) {
                    display.drawPixel(x0 + x * scale + dx, y0 + y * scale + dy, color);
                }
            }
        }
    }
}

void view_qr_code_init(view_qr_code_t *view, const char *title, const char *subtitle, const char *data) {
    if (!view) return;
    view->title = title;
    view->subtitle = subtitle;
    view->data = data;
    view->scale = 0;  // Auto-calculate
}

void view_qr_code_render(const view_qr_code_t *view, bool partial) {
    if (!view || !view->data) return;

    // Enable backlight for better QR scanning
    gui_backlight_on();

    Gdey029T94 &display = gui_get_display();
    display.fillScreen(EPD_WHITE);
    display.setTextColor(EPD_BLACK);

    // Display: 296x128 pixels
    // Layout: QR code fills display height, text on right if space
    const int display_height = 128;
    const int display_width = 296;
    const int qr_margin = 0;  // No margin - maximize QR size

    // QR code uses full display height
    int max_qr_height = display_height;

    // Generate QR code with callback
    // Version 20 = 97 modules, can hold ~858 alphanumeric chars (ECC_LOW)
    esp_qrcode_config_t cfg = {
        .display_func = qr_display_callback,
        .max_qrcode_version = 20,
        .qrcode_ecc_level = ESP_QRCODE_ECC_LOW,
        .user_data = NULL
    };

    // First pass: determine actual QR size (without drawing)
    qr_render_ctx.sizing_pass = true;
    qr_render_ctx.actual_size = 0;
    esp_err_t err = esp_qrcode_generate(&cfg, view->data);
    if (err != ESP_OK) {
        LOG_E("VIEW", "QR generate failed: %s", esp_err_to_name(err));
        display.setFont(NULL);
        display.setCursor(10, 64);
        display.print("QR Error");
        gui_flush_sync(false);
        return;
    }

    int actual_modules = qr_render_ctx.actual_size;
    if (actual_modules <= 0) actual_modules = 97;  // Fallback

    // QR code includes 4-module quiet zone on each side (added by library)
    // So effective size = actual_modules (quiet zone is inside the reported size)

    // Calculate optimal scale to fill display height exactly
    // Use the largest scale that fits
    int scale = max_qr_height / actual_modules;
    if (scale < 1) scale = 1;

    // Calculate actual QR pixel size
    int qr_pixel_size = actual_modules * scale;

    // If there's significant unused space, try to center better or use remaining space
    int unused_height = max_qr_height - qr_pixel_size;

    // Position QR code on left, vertically centered in available space
    qr_render_ctx.offset_x = qr_margin;
    qr_render_ctx.offset_y = unused_height / 2;
    qr_render_ctx.scale = scale;

    // Second pass: actually render
    qr_render_ctx.sizing_pass = false;
    err = esp_qrcode_generate(&cfg, view->data);
    if (err != ESP_OK) {
        LOG_E("VIEW", "QR render failed: %s", esp_err_to_name(err));
        gui_flush_sync(false);
        return;
    }

    LOG_I("VIEW", "QR: %d modules, scale=%d, %dx%d px", actual_modules, scale, qr_pixel_size, qr_pixel_size);

    // Right side text area starts after QR code
    int text_area_x = qr_margin + qr_pixel_size + 8;
    int text_area_width = display_width - text_area_x - 4;

    // Draw title on right side (top)
    int y = 14;
    if (view->title && view->title[0]) {
        display.setFont(&FreeMonoBold9pt7b);

        // Word wrap title into text area
        const char *p = view->title;
        char line[32];
        int max_chars = text_area_width / 7;  // Approx char width at 9pt

        while (*p && y < 80) {
            // Copy up to max_chars or until end
            int len = 0;
            while (p[len] && len < max_chars && len < 31) {
                line[len] = p[len];
                len++;
            }
            line[len] = '\0';

            display.setCursor(text_area_x, y);
            display.print(line);

            p += len;
            y += 16;
        }
    }

    if (view->subtitle && view->subtitle[0] && y < 98) {
        display.setFont(NULL);
        int max_chars = text_area_width / 6;  // Approx char width for default font
        if (max_chars > 31) max_chars = 31;
        char line[32];
        int len = 0;
        while (view->subtitle[len] && len < max_chars) {
            line[len] = view->subtitle[len];
            len++;
        }
        line[len] = '\0';
        display.setCursor(text_area_x, y + 2);
        display.print(line);
        y += 12;
    }

    // Draw hint at bottom right
    display.setFont(NULL);
    const char *hint = "NO=Back";
    display.setCursor(text_area_x, 116);
    display.print(hint);

    gui_flush_sync(false);
}
