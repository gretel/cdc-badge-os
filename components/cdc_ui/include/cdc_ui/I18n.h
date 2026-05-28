/**
 * \file I18n.h
 * \brief Internationalization with English fallbacks in code and overlay
 *        translations loaded at runtime from a VFAT JSON file.
 *
 * Architecture:
 *  - English fallbacks live in code as `I18nEntry` tables registered by each
 *    module at startup. They sit in rodata and are always available.
 *  - All other languages live in `/plugins/i18n/lang.json` on the plugins
 *    FAT partition. The file is parsed at boot into PSRAM-backed key-value
 *    tables, one per language code.
 *  - Plugin manifest strings keep their own `i18n_strings` map and are
 *    queried via `host_i18n_tr_key` - unchanged by this rewrite.
 *
 * Key conventions:
 *  - `core.*`         for firmware-wide strings.
 *  - `mod_<name>.*`   for module-specific strings.
 *  - Keys are 7-bit ASCII, snake_case, no spaces.
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace cdc::ui {

/**
 * \brief Single English translation entry.
 *
 * Both fields point to rodata literals. Modules declare static constexpr
 * arrays of these and register them with `I18n::registerEnglishTable()`.
 */
struct I18nEntry {
    const char* key;   ///< Stable string key, e.g. "core.save" or "mod_totp.codes".
    const char* en;    ///< English translation - rodata literal.
};

/**
 * \brief Internationalization singleton.
 *
 * Lookup order for `tr(key)`:
 *   1. If current language is "en" or no overlay loaded: return English fallback.
 *   2. If overlay has a translation for the current language and key: return it.
 *   3. Else: fall back to English. Returns "?<key>" if even English is missing.
 *
 * Thread safety: registration is expected to happen single-threaded during
 * module init. `tr()` is read-only after all modules have registered.
 */
class I18n {
public:
    /// Default path to the overlay file on the plugins FAT.
    static constexpr const char* DEFAULT_OVERLAY_PATH = "/plugins/i18n/lang.json";

    /// Singleton accessor.
    static I18n& instance();

    /**
     * \brief Initialize and load persisted language code from NVS.
     * \return true on success.
     */
    bool init();

    /**
     * \brief Load overlay translations from a JSON file at \p path.
     *
     * The file format is:
     * \code
     * { "version": 1,
     *   "translations": {
     *     "de": {"core.save": "Speichern", ...},
     *     "fr": {"core.save": "Enregistrer", ...}
     *   }
     * }
     * \endcode
     *
     * \return true on success, false on missing/invalid file (caller falls back to English).
     */
    bool loadOverlay(const char* path = DEFAULT_OVERLAY_PATH);

    /**
     * \brief Append English entries to the lookup table.
     *
     * Typically called once per module from its `init()`. Entries are stored
     * by-pointer (no copy); the caller must keep the array alive for the
     * lifetime of the firmware (rodata is fine).
     *
     * \param entries Pointer to the first entry.
     * \param count   Number of entries.
     */
    void registerEnglishTable(const I18nEntry* entries, std::size_t count);

    /**
     * \brief Look up a translation by key.
     * \param key Stable string key. Must not be null.
     * \return Pointer to translation text. Never null - returns "?<key>" if no match.
     */
    const char* tr(const char* key) const;

    /**
     * \brief Overlay-only lookup with no English fallback.
     *
     * Returns the translation from the currently active overlay language, or
     * `nullptr` if the key is missing OR the active language is "en". Useful
     * for plugin code that wants to try a namespaced key in the central
     * overlay before falling back to its own English manifest table.
     *
     * \param key Stable string key.
     * \return Pointer into PSRAM overlay storage (stable until the active
     *         language changes), or `nullptr`.
     */
    const char* overlayTr(const char* key) const;

    /// Current language code (lower-case ISO-639-1, e.g. "en", "de").
    const std::string& getLanguageCode() const { return currentLang_; }

    /**
     * \brief Set the active language by code.
     *
     * If the overlay does not contain the requested language, the call still
     * succeeds and persists the choice, but `tr()` will fall back to English
     * until an overlay covering the language is loaded.
     *
     * \param code Language code (e.g. "en", "de"). Empty defaults to "en".
     * \return true on success.
     */
    bool setLanguageCode(const char* code);

    /// List of language codes present in the loaded overlay (does not include "en").
    const std::vector<std::string>& availableOverlayLanguages() const { return overlayLangs_; }

    /**
     * \brief Callback invoked whenever the active translation table changes.
     *
     * Triggered on:
     *  - successful `loadOverlay()` (initial load or reload)
     *  - `setLanguageCode()` switching to a different language
     *
     * UI code uses this to refresh any cached label pointers - menu items hold
     * `const char*` into either rodata (English fallback) or the overlay's
     * PSRAM-backed string storage, both of which are invalidated by a language
     * change.
     */
    using LanguageChangedCallback = std::function<void()>;
    void setOnLanguageChanged(LanguageChangedCallback cb) { onChanged_ = std::move(cb); }

private:
    I18n();

    void registerCoreEnglishTable();
    bool sortIfNeeded() const;
    const char* enLookup(const char* key) const;
    const char* overlayLookup(const char* key) const;
    void loadLanguageFromNvs();
    void saveLanguageToNvs();

    mutable std::vector<I18nEntry> en_;
    mutable bool                   enSorted_ = false;

    struct OverlayEntry {
        std::string key;
        std::string value;
    };
    std::vector<OverlayEntry> activeOverlay_;
    std::vector<std::string>  overlayLangs_;
    std::string               overlayJsonPath_;

    std::string currentLang_ = "en";

    LanguageChangedCallback onChanged_;
};

/// Look up a translation by string key.
inline const char* tr(const char* key)
{
    return I18n::instance().tr(key);
}

}  // namespace cdc::ui
