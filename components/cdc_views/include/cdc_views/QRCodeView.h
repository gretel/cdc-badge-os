#pragma once

#include "cdc_ui/IView.h"
#include <cstdint>

namespace cdc::ui {

/**
 * QRCodeView - Display QR codes with optional title/subtitle
 *
 * Renders QR code on the left side of the display, maximizing size.
 * Title and subtitle displayed on the right side.
 *
 * Uses two-pass rendering:
 * 1. Sizing pass: Determine QR module count
 * 2. Render pass: Draw QR at optimal scale
 *
 * Keys:
 *   Any = Back (close view)
 */
class QRCodeView : public ViewBase {
public:
    /**
     * Initialize QR code view
     * @param data Data to encode in QR code
     * @param title Optional title (displayed right of QR)
     * @param subtitle Optional subtitle (displayed below title)
     */
    void init(const char* data, const char* title = nullptr, const char* subtitle = nullptr);

    /**
     * Set hint text for footer
     */
    void setHint(const char* hint) { customHint_ = hint; }

    // IView implementation
    void render(bool partial) override;
    InputResult onKey(char key) override;
    const char* getName() const override { return "QRCodeView"; }
    const char* getFooterHint() const override;

private:
    const char* data_ = nullptr;
    const char* title_ = nullptr;
    const char* subtitle_ = nullptr;
    const char* customHint_ = nullptr;

    // QR code sizing
    int qrModuleCount_ = 0;
    int qrScale_ = 1;
    int qrOffsetX_ = 0;
    int qrOffsetY_ = 0;

    void calculateLayout();
    void renderQrCode();
    void renderText();
};

// ============================================================================
// Convenience Functions
// ============================================================================

/**
 * Show a QR code view.
 *
 * @param data Data to encode
 * @param title Optional title
 * @param subtitle Optional subtitle
 * @param hint Optional footer hint (default: "Any key = Back")
 * @return Pointer to the QRCodeView
 *
 * Example:
 *   showQRCode("https://example.com", "Website", "Scan me!");
 */
QRCodeView* showQRCode(const char* data, const char* title = nullptr,
                       const char* subtitle = nullptr, const char* hint = nullptr);

} // namespace cdc::ui
