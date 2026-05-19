#pragma once

#include "cdc_views/T9InputView.h"

namespace cdc::ui {

/**
 * \brief T9 input variant for secrets: displays asterisks instead of letters
 *        and offers a long-press reveal toggle.
 *
 * Behaviour vs. plain `T9InputView`:
 *  - Characters render as `mask_` (default '*') unless `revealed_` is true.
 *  - While the multi-tap cursor is active (the last char is still cycling),
 *    that last char is shown in plain to give T9 feedback. All earlier chars
 *    stay masked.
 *  - Long-pressing `KEY_YES` toggles the reveal mode for the entire string.
 *  - The save callback always receives the unmasked text.
 *
 * Useful for TOTP secrets, PINs, API tokens, passwords - any secret entered
 * via T9 that must not appear on screen by default.
 */
class PasswordT9View : public T9InputView {
public:
    /**
     * \brief Sets the mask character. Default '*'.
     */
    void setMaskChar(char mask) { maskChar_ = mask; }

    /**
     * \brief Forces the reveal state. Defaults to `false` after each `init()`.
     */
    void setRevealed(bool revealed) { revealed_ = revealed; markDirty(); }

    bool isRevealed() const { return revealed_; }

    // IView overrides
    void render(bool partial) override;
    InputResult onLongPress(char key) override;
    const char* getName() const override { return "PasswordT9View"; }
    const char* getFooterHint() const override;

private:
    char maskChar_ = '*';
    bool revealed_ = false;
};

} // namespace cdc::ui
