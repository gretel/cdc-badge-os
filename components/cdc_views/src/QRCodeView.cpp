/**
 * QRCodeView Implementation
 *
 * QR code display with two-pass rendering for optimal sizing.
 * Layout matches legacy implementation: QR on left, text on right.
 */

#include "cdc_views/QRCodeView.h"
#include "cdc_ui/ViewStack.h"
#include "cdc_ui/I18n.h"
#include "cdc_hal/IDisplay.h"
#include "cdc_log.h"
#include "qrcode.h"
#include <goodisplay/gdey029T94.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <cstring>

static const char* TAG = "QRCodeView";

/**
 * \brief Display dimensions for Gdey029T94 panel.
 */
static constexpr int DISPLAY_WIDTH = 296;
static constexpr int DISPLAY_HEIGHT = 128;
static constexpr int QR_MARGIN = 0;  // No margin - maximize QR size

namespace cdc::ui {

/**
 * \brief QR rendering context used by callback-driven rendering.
 */
static struct {
    int offsetX;
    int offsetY;
    int scale;
    int actualSize;    // Filled during sizing pass
    bool sizingPass;   // True = just measure, don't draw
    Gdey029T94* display;
} s_qrCtx;

/**
 * \brief Renders or measures the QR code through the ESP QR callback.
 * \param qrcode QR handle provided by the ESP QR generator.
 * \return void
 */
static void qrDisplayCallback(esp_qrcode_handle_t qrcode) {
    int size = esp_qrcode_get_size(qrcode);
    s_qrCtx.actualSize = size;

    // If sizing pass, just record size and return
    if (s_qrCtx.sizingPass) {
        return;
    }

    if (!s_qrCtx.display) return;

    int scale = s_qrCtx.scale;
    int x0 = s_qrCtx.offsetX;
    int y0 = s_qrCtx.offsetY;

    LOG_D(TAG, "QR size=%d, scale=%d, pos=(%d,%d)", size, scale, x0, y0);

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            bool black = esp_qrcode_get_module(qrcode, x, y);
            uint16_t color = black ? EPD_BLACK : EPD_WHITE;

            // Draw scaled pixel
            for (int dy = 0; dy < scale; dy++) {
                for (int dx = 0; dx < scale; dx++) {
                    s_qrCtx.display->drawPixel(x0 + x * scale + dx, y0 + y * scale + dy, color);
                }
            }
        }
    }
}

/**
 * \brief Initializes QR code content and layout state.
 * \param data QR payload text.
 * \param title Optional title text.
 * \param subtitle Optional subtitle text.
 * \return void
 */
void QRCodeView::init(const char* data, const char* title, const char* subtitle) {
    data_ = data;
    title_ = title;
    subtitle_ = subtitle;
    customHint_ = nullptr;
    qrModuleCount_ = 0;
    qrScale_ = 1;
    qrOffsetX_ = 0;
    qrOffsetY_ = 0;
    dirty_ = true;

    LOG_D(TAG, "init: data='%s', title='%s'",
             data ? data : "(null)", title ? title : "(null)");
}

/**
 * \brief Handles key input by closing the QR view.
 * \param key Pressed key code.
 * \return Always requests pop from view stack.
 */
InputResult QRCodeView::onKey(char key) {
    (void)key;
    // Any key closes the QR view
    return InputResult::REQUEST_POP;
}

/**
 * \brief Returns footer hint text.
 * \return Footer hint string.
 */
const char* QRCodeView::getFooterHint() const {
    if (customHint_) {
        return customHint_;
    }
    return tr(StringId::HINT_BACK);
}

/**
 * \brief Computes QR module count, scale, and offsets for rendering.
 * \return void
 */
void QRCodeView::calculateLayout() {
    if (!data_) return;

    // First pass: determine actual QR size (without drawing)
    esp_qrcode_config_t cfg = {
        .display_func = qrDisplayCallback,
        .max_qrcode_version = 20,  // Max 97 modules
        .qrcode_ecc_level = ESP_QRCODE_ECC_LOW,
        .user_data = nullptr
    };

    s_qrCtx.sizingPass = true;
    s_qrCtx.actualSize = 0;
    s_qrCtx.display = nullptr;

    esp_err_t err = esp_qrcode_generate(&cfg, data_);
    if (err != ESP_OK) {
        LOG_E(TAG, "QR sizing failed: %s", esp_err_to_name(err));
        qrModuleCount_ = 97;  // Fallback
    } else {
        qrModuleCount_ = s_qrCtx.actualSize;
        if (qrModuleCount_ <= 0) qrModuleCount_ = 97;
    }

    // QR code uses full display height
    int maxQrHeight = DISPLAY_HEIGHT;

    // Calculate optimal scale to fill display height
    qrScale_ = maxQrHeight / qrModuleCount_;
    if (qrScale_ < 1) qrScale_ = 1;

    // Calculate actual QR pixel size
    int qrPixelSize = qrModuleCount_ * qrScale_;

    // Center vertically in available space
    int unusedHeight = maxQrHeight - qrPixelSize;
    qrOffsetX_ = QR_MARGIN;
    qrOffsetY_ = unusedHeight / 2;

    LOG_I(TAG, "Layout: %d modules, scale=%d, %dx%d px",
             qrModuleCount_, qrScale_, qrPixelSize, qrPixelSize);
}

/**
 * \brief Renders the QR matrix onto the display.
 * \return void
 */
void QRCodeView::renderQrCode() {
    if (!data_) return;

    hal::IDisplay* display = hal::getDisplayInstance();
    if (!display) return;

    auto* gfx = static_cast<Gdey029T94*>(display->getNativeHandle());
    if (!gfx) return;

    // Set up context for callback
    s_qrCtx.sizingPass = false;
    s_qrCtx.offsetX = qrOffsetX_;
    s_qrCtx.offsetY = qrOffsetY_;
    s_qrCtx.scale = qrScale_;
    s_qrCtx.display = gfx;

    esp_qrcode_config_t cfg = {
        .display_func = qrDisplayCallback,
        .max_qrcode_version = 20,
        .qrcode_ecc_level = ESP_QRCODE_ECC_LOW,
        .user_data = nullptr
    };

    esp_err_t err = esp_qrcode_generate(&cfg, data_);
    if (err != ESP_OK) {
        LOG_E(TAG, "QR render failed: %s", esp_err_to_name(err));
        gfx->setFont(nullptr);
        gfx->setCursor(10, 64);
        gfx->print(tr(StringId::QR_ERROR));
    }
}

/**
 * \brief Renders title/subtitle/footer text beside the QR area.
 * \return void
 */
void QRCodeView::renderText() {
    hal::IDisplay* display = hal::getDisplayInstance();
    if (!display) return;

    auto* gfx = static_cast<Gdey029T94*>(display->getNativeHandle());
    if (!gfx) return;

    // Calculate QR pixel size
    int qrPixelSize = qrModuleCount_ * qrScale_;

    // Right side text area starts after QR code
    int textAreaX = QR_MARGIN + qrPixelSize + 8;
    int textAreaWidth = DISPLAY_WIDTH - textAreaX - 4;

    // Draw title on right side (top)
    int y = 14;
    if (title_ && title_[0]) {
        gfx->setFont(&FreeMonoBold9pt7b);

        // Word wrap title into text area
        const char* p = title_;
        char line[32];
        int maxChars = textAreaWidth / 7;  // Approx char width at 9pt

        while (*p && y < 80) {
            // Copy up to maxChars or until end
            int len = 0;
            while (p[len] && len < maxChars && len < 31) {
                line[len] = p[len];
                len++;
            }
            line[len] = '\0';

            gfx->setCursor(textAreaX, y);
            gfx->print(line);

            p += len;
            y += 16;
        }
    }

    // Subtitle below title
    if (subtitle_ && subtitle_[0] && y < 98) {
        gfx->setFont(nullptr);
        int maxChars = textAreaWidth / 6;  // Approx char width for default font
        if (maxChars > 31) maxChars = 31;

        char line[32];
        int len = 0;
        while (subtitle_[len] && len < maxChars) {
            line[len] = subtitle_[len];
            len++;
        }
        line[len] = '\0';

        gfx->setCursor(textAreaX, y + 2);
        gfx->print(line);
        y += 12;
    }

    // Draw hint at bottom right
    gfx->setFont(nullptr);
    const char* hint = getFooterHint();
    if (hint) {
        gfx->setCursor(textAreaX, 116);
        gfx->print(hint);
    }
}

/**
 * \brief Renders the complete QR code view.
 * \param partial Indicates partial/full redraw mode.
 * \return void
 */
void QRCodeView::render(bool partial) {
    hal::IDisplay* display = hal::getDisplayInstance();
    if (!display) return;

    auto* gfx = static_cast<Gdey029T94*>(display->getNativeHandle());
    if (!gfx) return;

    if (!partial) {
        gfx->fillScreen(EPD_WHITE);
    }
    gfx->setTextColor(EPD_BLACK);

    if (!data_) {
        gfx->setFont(nullptr);
        gfx->setCursor(10, 64);
        gfx->print(tr(StringId::NO_DATA));
        dirty_ = false;
        return;
    }

    // Calculate layout on first render
    if (qrModuleCount_ == 0) {
        calculateLayout();
    }

    // Render QR code
    renderQrCode();

    // Render text
    renderText();

    dirty_ = false;
}

/**
 * \brief Convenience factory/helper function.
 */

static QRCodeView s_sharedQRCodeView;

/**
 * \brief Shows a shared QR code view instance.
 * \param data QR payload text.
 * \param title Optional title text.
 * \param subtitle Optional subtitle text.
 * \param hint Optional custom footer hint.
 * \return Pointer to the shared `QRCodeView` instance.
 */
QRCodeView* showQRCode(const char* data, const char* title,
                       const char* subtitle, const char* hint) {
    s_sharedQRCodeView.init(data, title, subtitle);
    if (hint) {
        s_sharedQRCodeView.setHint(hint);
    }
    ViewStack::instance().push(&s_sharedQRCodeView);
    return &s_sharedQRCodeView;
}

} // namespace cdc::ui
