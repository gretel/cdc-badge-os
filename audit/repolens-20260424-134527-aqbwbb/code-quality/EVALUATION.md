# Audit-Ticket-Bewertung

Bewertung der 233 KI-generierten Repolens-Code-Quality-Tickets gegen den realen Code im `cdc-badge-os` Repository.

**Verdikte:**
- **SINNVOLL** – Befund stimmt, Korrektur klar gerechtfertigt, niedriges Risiko
- **TEILWEISE** – Teilweise valider Befund, Korrektur in abgespeckter Form sinnvoll
- **ABLEHNEN** – Befund stimmt, aber Korrektur ist Bikeshedding/Over-Engineering/spec-getrieben/third-party
- **FALSCH** – Behauptung trifft nicht zu (Code prüft bereits / Funktion existiert nicht / Zahlen erfunden)

---

## linting (13)

### 001-inconsistent-logging.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: ESP_LOG in mod_gpg ist eigener Code und sollte zu cdc_log konvertiert werden, jedoch sind CalEPD/Adafruit-GFX gepatchte third-party-Bibliotheken; main.cpp braucht esp_log.h für ESP_ERROR_CHECK.

### 002-missing-lint-config.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Lint-Setup fehlt, aber für ein kleines Embedded-Projekt mit reproduzierbarem PlatformIO-Build geringer Nutzen.

### 003-missing-clang-format-root.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Kein Root-.clang-format vorhanden; Übernahme der libtropic-Style passt nicht automatisch zum Projekt.

### 004-hardcoded-vscode-paths.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Datei ist von PlatformIO auto-generiert und sollte nicht versioniert sein; korrekte Lösung ist Untracking.

### 005-vscode-gitignore-conflict.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: .vscode/ ist in .gitignore aber 3 Dateien sind getrackt - inkonsistent.

### 006-using-namespace-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Geringfügige Stilinkonsistenz ohne praktische Auswirkung.

### 007-no-pre-commit-hooks.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Pre-commit-Framework fuer Solo-Embedded-Projekt overkill.

### 008-unsafe-string-functions.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Audit raeumt selbst ein dass alle Vorkommen safe sind (Literale fester Groesse).

### 009-missing-default-switch-cases.md
- Behauptung stimmt: nein
- Verdikt: FALSCH
- Begründung: Alle Beispiele HABEN ein default-case mit return; bemaengelt nur fehlendes Logging im default.

### 011-malloc-without-check.md
- Behauptung stimmt: teilweise
- Verdikt: TEILWEISE
- Begründung: SerialCmd.cpp HAT bereits Null-Checks; CalEPD/Adafruit sind gepatchte third-party.

### 012-python-requirements-pinning.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Tools-Skripte sind Hilfs-Werkzeuge, harte Pinning erschwert Updates ohne Mehrwert.

### 013-mixed-formatting-styles.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: 2-Space-Code in CalEPD/Adafruit-GFX ist gepatchte third-party.

### 015-missing-function-docs.md
- Behauptung stimmt: teilweise
- Verdikt: TEILWEISE
- Begründung: IModule/IKeypad/IDisplay haben sehr wohl Doxygen-Kommentare; Audit-Beispiele sind teils fabriziert.

---

## comments (3)

### 003-ctaphid-todo-wink-feedback.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: TODO existiert, WINK-Visualisierung laut Spec sinnvoll.

### 004-comment-inconsistency-doxygen.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: CLAUDE.md fordert `\brief`, aber IModule.h und cdc_log.h verwenden `@param`.

### 005-trivial-comments-overdoc.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Manche Kommentare nicht trivial sondern erklaerend; nur einzelne redundante Kommentare relevant.

---

## complexity (18)

### 001-large-switch-cmd_put_data.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: 14 cases mit identischem memcpy/save/return-Muster, Tabellen-Refactor sinnvoll.

### 001-serialcmd-process-function.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: ~92 Zeilen, Extract-Method-Kandidatin ohne Spec-Bindung.

### 002-ctap2-parse-make-credential.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Switch durch CTAP2-Spec vorgegeben; Funktion mit ~64 Zeilen schon kompakt.

### 002-deep-nesting-change_reference_data.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: Echte Code-Duplikation zwischen PW1- und PW3-Pfad.

### 003-appui-settings-switch.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: 33 Zeilen, sehr lesbar; Lambda-Tabelle waere Komplexitaetszuwachs ohne Nutzen.

### 003-long-function-wizard_start.md
- Behauptung stimmt: teilweise
- Verdikt: TEILWEISE
- Begründung: ~41 Zeilen statt 50; format_user_id-Helper sinnvoll, aber niedrige Prioritaet.

### 004-ctap2-get-info-cbor.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: ~104 Zeilen, lange CBOR-Encoder-Funktion risikoarm aufteilbar.

### 004-parameter-overload-getMenuItems.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: Funktion 38 Zeilen, 3 Parameter; behauptete aehnliche Funktion existiert nicht.

### 005-ctap2-make-credential-flow.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: ~73 Zeilen, bereits saubere Step-by-Step-Struktur; Lambda-Pipeline wuerde mehr Komplexitaet erzeugen.

### 006-appui-updatePowerStatusIcons.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: 5 Status-Checks mit identischem Muster, updateStatusIcon-Helper risikoarm.

### 007-tropicstorage-get-slot-info.md
- Behauptung stimmt: nein
- Verdikt: FALSCH
- Begründung: Funktion `getSlotInfo` existiert nicht in TropicStorage.cpp.

### 008-generate-keypair-deep-nesting.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: ~167 Zeilen mit Privatschluessel-Handling; Aufteilung reduziert Sicherheitsrisiko.

### 009-vcard-add-validation-paths.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Funktion bereits idiomatisch mit Guard-Clauses; Pipeline-Refactor Over-Engineering.

### 010-u2f-attest-sign-der-encoding.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: R/S-Encoding identisch dupliziert, encode_der_integer-Helper sinnvoll.

### 011-ble-gap-event-callback-complexity.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: BLE GAP Events durch NimBLE-Stack vorgegeben, Switch idiomatisch.

### 012-start-advertising-long-function.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: ~88 Zeilen, linear ohne tiefe Verschachtelung; Aufteilung sinnvoll, nicht dringend.

### 013-getmenuitems-nested-loops.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Extract-Filter-Helper sinnvoll, std::sort kann Code-Size erhoehen.

### 014-tropicstorage-foreachslot-complexity.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: 37 Zeilen, continue-Kette nachvollziehbar; niedrige Prioritaet.

---

## consistency (37)

### 001-header-guard-inconsistency.md
- Behauptung stimmt: teilweise
- Verdikt: TEILWEISE
- Begründung: Mix existiert, aber "fehlende Guards"-Liste falsch; nur FIDO2 `#ifndef` zu `#pragma once` ist sinnvoll.

### 001-logging-inconsistency Kopie.md
- Behauptung stimmt: ja
- Verdikt: FALSCH
- Begründung: Explizite Duplikat-Datei.

### 001-logging-inconsistency.md
- Behauptung stimmt: teilweise
- Verdikt: TEILWEISE
- Begründung: CalEPD ist third-party; mod_gpg/openpgp und ungenutztes esp_log.h in main.cpp duerfen gefixt werden.

### 002-module-structure-pin-storage.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: pin_storage.h liegt im falschen Pfad, einfacher Fix.

### 002-namespace-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Betrifft third-party CalEPD.

### 002-tag-naming-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Spacing-Fix in 2 Dateien trivial sinnvoll.

### 003-error-type-inconsistency.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: ESP-IDF-Interfaces muessen esp_err_t zurueckgeben; Trennung sinnvoll.

### 003-extern-c-placement.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: SaoModule.h fehlt extern "C" Deklaration, einzeiliger Fix.

### 003-include-style-inconsistency.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: Betrifft third-party CalEPD.

### 004-error-handling-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Mix von esp_err_t/SeResult/bool ist domain-driven sinnvoll.

### 004-logging-tag-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: TAG-Variable einfuehren ist guenstig, eigener Code.

### 004-module-structure.md
- Behauptung stimmt: teilweise
- Verdikt: TEILWEISE
- Begründung: usb_badge-Reorg machbar, CalEPD ablehnen.

### 005-extern-c-block-patterns.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Beide Patterns haben unterschiedlichen Zweck.

### 005-extern-c-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Verteilung folgt natuerlichem Zweck.

### 005-mixed-c-cpp-styles.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: memcpy vs std::memcpy funktional identisch.

### 006-class-naming-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Trennung Klassen=PascalCase, C-Header=snake_case bewaehrt.

### 006-std-qualifier.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Stilfrage ohne funktionalen Mehrwert.

### 006-tag-naming-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Subjektiv, Aufwand vs Nutzen unguenstig.

### 007-brace-placement-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Betrifft third-party CalEPD.

### 007-const-expr.md
- Behauptung stimmt: teilweise
- Verdikt: TEILWEISE
- Begründung: Einige #define zu constexpr migrierbar.

### 008-function-naming-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Inkonsistenz in CalEPD (third-party).

### 008-header-file-naming.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: snake_case fuer C-API-Header, PascalCase fuer C++-Klassen ist bewusste Trennung.

### 009-member-variable-naming-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: CalEPD third-party; eigener Code konsistent.

### 009-nvs-namespace.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Vereinheitlichung waere nett, aber NVS-Namespaces sind 15-Zeichen-limitiert; Migration zu invasiv.

### 010-array-naming-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Low-impact, CalEPD third-party.

### 010-extern-c-style.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Beide Patterns haben unterschiedliche Zwecke.

### 011-doxygen-style-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: cdc_log und andere eigene Header gemaess CLAUDE.md auf \brief umstellen.

### 011-include-path-inconsistency.md
- Behauptung stimmt: teilweise
- Verdikt: TEILWEISE
- Begründung: mod_gpg-interne Pfade konsistenter machen, niedrige Prioritaet.

### 012-ccid-log-macros.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: Triviale Wrapper redundant, einfacher sed-Fix.

### 012-operator-spacing-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Ausschliesslich third-party CalEPD.

### 013-doxygen-style.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Konkreter Hinweis auf cdc_log.h berechtigt, trivialer Fix.

### 013-header-guard-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Betrifft third-party CalEPD.

### 014-enum-style-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Betrifft third-party CalEPD; eigener Code nutzt enum class.

### 015-constant-naming-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: Echte Inkonsistenz im eigenen Code, einfacher Fix.

### 016-usb-types-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: Echte Typduplizierung mit divergierenden Signaturen.

### 017-typedef-enum-vs-enum-class-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: cdc_log/mod_fido2 absichtlich C-kompatibel; nur dort sinnvoll wo C-Kompat egal.

### 018-log-tag-variable-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: Klare Inkonsistenz im eigenen Code, einfacher Fix.

---

## dead-code (4)

### 001-disabled-epaper-file.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Datei dient als Backup; Loeschung vor Ruecksprache pruefen.

### 001-macos-dotfiles.md
- Behauptung stimmt: nein
- Verdikt: FALSCH
- Begründung: Keine `._*`-Dateien im Repo; behauptete 1723 erfunden. Allenfalls .gitignore-Praevention.

### 002-disabled-files.md
- Behauptung stimmt: teilweise
- Verdikt: TEILWEISE
- Begründung: EpaperDisplay.cpp.disabled existiert; macOS-Dotfile nicht. Loeschung explizit klaeren.

### 003-orphaned-secure-element-stub.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Absichtlich nicht im Build (Symbol-Konflikt); Fallback waehrend libtropic v3.x-Migration.

---

## duplication (7)

### 001-duplicated-token-parsing_helpers.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: skipSpaces/nextToken byte-identisch in beiden Modulen.

### 002-duplicated-module-ui-patterns.md
- Behauptung stimmt: teilweise
- Verdikt: TEILWEISE
- Begründung: pushT9WizardStep extrahierbar, andere nur strukturell aehnlich.

### 003-duplicated-wizard-state-pattern.md
- Behauptung stimmt: nein
- Verdikt: ABLEHNEN
- Begründung: Nur strukturelle Aehnlichkeit, Template-Refactor waere Over-Engineering.

### 004-duplicated-wizard-completion-pattern.md
- Behauptung stimmt: teilweise
- Verdikt: TEILWEISE
- Begründung: View-Stack-Pop-Loop echte Duplikation; restliche Wizard-Logik modulspezifisch.

### 005-duplicated-module-lifecycle-boilerplate.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: 9 Module mit nahezu identischen init/start/stop, ModuleBase wuerde ~150 Zeilen einsparen.

### 006-duplicated-storage-layer-patterns.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: Slot-Manager-Logik byte-identisch in TotpStore/PasswordStore.

### 007-duplicated-view-navigation-helpers.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: popToAnchor()-Inline-Funktion klar lohnender DRY-Gewinn.

---

## formatting (6)

### 001-long-lines-lockscreenview.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Auseinanderziehen erzeugt 6x mehr Zeilen fuer trivialen Boilerplate.

### 002-inconsistent-loop-types.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: Eine int-Schleife in KeyFingerprint.cpp:59 weicht vom size_t-Muster ab; minimaler Diff.

### 003-long-lines-multiple-files.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Kein definierter Line-Length-Standard; Aufbrechen bringt keinen Lesbarkeitsgewinn.

### 007-missing-clang-format-root.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: clang-format einzufuehren ist bewusste Projektentscheidung; massive Diff-Noise riskant.

### 008-trailing-whitespace-blank-lines.md
- Behauptung stimmt: nein
- Verdikt: FALSCH
- Begründung: Stichproben zeigen 0 trailing-whitespace-Zeilen; behauptete Zahlen erfunden.

### 009-missing-final-newlines.md
- Behauptung stimmt: nein
- Verdikt: FALSCH
- Begründung: Scan ueber 85 Dateien ergab 0 Dateien ohne finalen Newline.

---

## immutability (5)

### 001-mutable-static-variables.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: Meiste Variablen legitimes Module-State, sig_count ist runtime counter, s_session_pin wird gesetzt/geloescht.

### 002-mutable-static-arrays-descriptors.md
- Behauptung stimmt: teilweise
- Verdikt: TEILWEISE
- Begründung: device_descriptor koennte const werden; s_openpgp_aid wird aktiv mutiert.

### 003-mutable-state-eventbus.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Fehlende Synchronisation real auf Dual-Core; subscribe praktisch nur einmal aufgerufen, Doku-Hinweis pragmatischer.

### 004-mutable-static-structs.md
- Behauptung stimmt: nein
- Verdikt: ABLEHNEN
- Begründung: WizardState/KeyboardReport/Buffer werden aktiv mutiert; const waere falsch.

### 005-array-mutation-tropicstorage.md
- Behauptung stimmt: nein
- Verdikt: FALSCH
- Begründung: Basiert nur auf grep ohne Storage-Code-Analyse; Fixes sind Lehrbuch-Vorschlaege.

---

## type-safety (13)

### 001-unsafe-void-pointer-casts.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: ListView-API ist C-style; std::variant/std::function bringen Heap/RTTI-Overhead.

### 002-service-registry-void-pointer-storage.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: ServiceType-Enum bereits 1:1-Bindung; std::any bringt RTTI-Bloat.

### 003-callback-userdata-void-pointer.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: void* userData ist API-konformes C-Callback-Pattern.

### 004-unsafe-pointer-to-integer-conversions.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Typsicherer Accessor-Helper sinnvoll, std::variant erhoeht Speicherbedarf.

### 005-missing-const-qualifiers-output-pointers.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: T* const Top-Level-Const auf Parameter bringt keinen API-Mehrwert.

### 006-key-fingerprint-const-qualifier.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: NULL-Checks existieren bereits; pure Stilbeschwerde.

### 007-inconsistent-null-check-userdata.md
- Behauptung stimmt: nein
- Verdikt: FALSCH
- Begründung: WifiMenuUi.cpp prueft bereits korrekt; behaupteter Bug existiert nicht.

### 008-service-registry-null-check-pattern.md
- Behauptung stimmt: nein
- Verdikt: FALSCH
- Begründung: ServiceType ist Enum, nicht null-pruefbar; Bounds-Check bereits korrekt.

### 009-tropic-storage-secure-element-const.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: setSecureElement ist Setter; const-Member wuerde ihn unmoeglich machen.

### 010-tropic-slot-overflow-check.md
- Behauptung stimmt: nein
- Verdikt: FALSCH
- Begründung: slot > end Check existiert bereits; end ist uint16_t, kein Overflow moeglich.

### 011-typeString-null-validation.md
- Behauptung stimmt: nein
- Verdikt: FALSCH
- Begründung: BleHidKeyboard.cpp:313 prueft bereits if (!text || !isConnected()).

### 012-gpg-key-type-enum-macro-mismatch.md
- Behauptung stimmt: teilweise
- Verdikt: TEILWEISE
- Begründung: KEY_* sind APDU-Wire, KEY_TYPE_* intern; Konvertierung existiert, Naming koennte klarer sein.

### 013-totp-atoi-overflow-no-bounds-check.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: Echter Robustness-Bug: digits/period/algorithm ohne Validierung.

---

## code-smells (29)

### 001-primitive-obsession-date-values.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Date-Struct nice-to-have, nicht dringend.

### 002-feature-envy-menu-items.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: ModuleRegistry ist die richtige Stelle fuer Aggregation; kein Feature Envy.

### 003-data-clumps-slot-range.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: TotpStore/PasswordStore bekommen 3 Felder einzeln, obwohl SlotRange existiert.

### 004-divergent-change-i18n.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: ~170 Zeilen, aber praktisch nur Tabelle aus REG()-Makros.

### 005-long-method-apply-slot-request.md
- Behauptung stimmt: nein
- Verdikt: ABLEHNEN
- Begründung: Methode nur ~40 Zeilen, delegiert bereits an validateSlotMap/validateEccRange.

### 006-shotgun-surgery-module-creation.md
- Behauptung stimmt: nein
- Verdikt: FALSCH
- Begründung: CMake generiert modules_init.gen.h automatisch; nur MODULES-Eintrag plus _register() noetig.

### 007-message-chains-power-status.md
- Behauptung stimmt: nein
- Verdikt: ABLEHNEN
- Begründung: Keine Message Chains, nur 1-Level-Aufrufe.

### 008-lazy-class-t9-helpers.md
- Behauptung stimmt: nein
- Verdikt: FALSCH
- Begründung: Methoden sind bereits static deklariert.

### 009-speculative-generality-isVisible.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: isVisible wird genutzt; std::function wuerde mehr RAM kosten.

### 010-temporary-fields-lock-screen.md
- Behauptung stimmt: nein
- Verdikt: FALSCH
- Begründung: Beschreibt regulaeres State-Caching fuer partial-render.

### 011-inappropriate-intimacy-totp-payload.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: TotpPayload bereits static in .cpp versteckt; #pragma pack ist korrekter Embedded-Ansatz.

### 012-parallel-inheritance-views.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: Normale Polymorphie ueber ViewBase, nicht parallele Inheritance.

### 013-duplicate-code-findFreeSlot.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: ~95% identisch zwischen TotpStore und PasswordStore.

### 014-long-params-rmemWrite.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: 6 Parameter grenzwertig, aber Builder-Pattern fuer HAL zu schwergewichtig.

### 015-magic-numbers-layout.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: TITLE_Y/HINT_Y in 6+ Files unterschiedlich dupliziert.

### 016-documentation-gaps.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: Privater 3-Zeilen-Helper mit selbsterklaerendem Code.

### 017-inappropriate-intimacy-hal.md
- Behauptung stimmt: teilweise
- Verdikt: TEILWEISE
- Begründung: Direkter hal::getWifiControllerInstance inkonsistent zur power-Abstraktion.

### 018-long-method-setModuleEnabled.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: 56 Zeilen mit komplexer C-String-Parsing-Logik.

### 019-unnecessary-coupling-display.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Views casten auf Gdey029T94* via getNativeHandle; pragmatisch fuer Adafruit-GFX-API.

### 020-long-method-setSlotRange.md
- Behauptung stimmt: nein
- Verdikt: FALSCH
- Begründung: Duplikat zu 005, bereits delegiert.

### 021-duplicate-code-slot-validation.md
- Behauptung stimmt: nein
- Verdikt: ABLEHNEN
- Begründung: Behauptete Duplikation zwischen TropicStorage und ModuleRegistry existiert nicht.

### 022-primitive-obsession-slot-indices.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: Strong-Typed-Wrapper auf Embedded mit C-API-Grenzen Over-Engineering.

### 023-feature-envy-listview.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: ListView rendert ListItem-Daten - exakt die Aufgabe einer View.

### 024-message-chains-display.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: getNativeHandle-Cast-Pattern in 8+ Views; DisplayContext-Helfer sinnvoll.

### 025-shotgun-surgery-layout-constants.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: Konstanten-Duplikation in mind. 6 View-Files erfuellt Shotgun-Surgery-Kriterium.

### 026-long-params-rmemWriteWithHeader.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Duplikat zu 014; HAL-API ohne Mehrwert aufgeblaeht.

### 027-lazy-class-keyboard-provider.md
- Behauptung stimmt: nein
- Verdikt: FALSCH
- Begründung: Audit zitiert falsche Interface-Definition; echtes Interface ist HID-Typing-Service mit ServiceRegistry.

### 028-speculative-generality-isVisible.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: Duplikat zu 009.

### 029-inappropriate-intimacy-storage.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: TropicStorage/TropicSlotMap beide cdc_core-intern; Adapter waere Over-Engineering.

---

## magic-values (36)

### 001-hardcoded-long-press-threshold.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: 800ms Magic-Number, Konstante neben POLL_TIMEOUT_MS waere konsistent.

### 001-hardcoded-timeouts.md
- Behauptung stimmt: teilweise
- Verdikt: TEILWEISE
- Begründung: Viele bereits Konstanten; zentrales Header Over-Engineering.

### 002-hardcoded-buffer-sizes.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: Crypto-Groessen [32]/[64] wiederholt ohne benannte Konstanten.

### 002-hardcoded-pingest-constants.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: minLength_ = 4 sollte BADGE_PIN_MIN nutzen; Toast-Dauer wiederverwendbar.

### 003-hardcoded-ecdsa-hash-size.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: hashLen != 32 / pubKey 64 fuer HW-spezifischen TROPIC01-Code grenzwertig sinnvoll.

### 003-hardcoded-key-codes.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: '2'/'8'/'Y'/'N'/'3' in 15+ Views wiederholt; KEY_UP/KEY_SELECT-Konstanten verbessern Lesbarkeit.

### 004-hardcoded-battery-voltage-thresholds.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: Spannungswerte mehrfach genutzt, klare semantische Bedeutung.

### 004-hardcoded-bit-masks.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Datasheet-Werte mit Inline-Kommentaren akzeptabel; Helper-Macros Over-Engineering.

### 005-hardcoded-i2c-timing.md
- Behauptung stimmt: nein
- Verdikt: ABLEHNEN
- Begründung: Werte sind bereits I2C_FREQ_HZ etc.; Ticket fordert nur bessere Kommentare.

### 006-hardcoded-display-layout.md
- Behauptung stimmt: teilweise
- Verdikt: TEILWEISE
- Begründung: Viele bereits Konstanten; zentrales Header koennte Display-Wechsel erleichtern.

### 006-hardcoded-sleep-constants.md
- Behauptung stimmt: nein
- Verdikt: ABLEHNEN
- Begründung: DEFAULT_LIGHT_SLEEP_INTERVAL_S und MAX_CALLBACKS bereits benannt.

### 007-hardcoded-keypad-bitmasks.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Bitmasken in zwei Funktionen dupliziert; programmatische Generierung besser.

### 007-hardcoded-totp-constants.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: TotpStore.h hat bereits DEFAULT_PERIOD; restliche Werte teils benennbar.

### 008-hardcoded-cbor-keys.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: CTAP2-Spec-IDs in 4+ Switches verstreut, Konstanten verbessern Spec-Konformitaet.

### 008-hardcoded-gpg-slot-allocation.md
- Behauptung stimmt: nein
- Verdikt: ABLEHNEN
- Begründung: RMEM_SLOT_DEC_KEY ist bereits benannte Konstante; SlotAllocator-Klasse Over-Engineering.

### 009-hardcoded-pin-storage-format.md
- Behauptung stimmt: teilweise
- Verdikt: TEILWEISE
- Begründung: MAGIC_V3 bereits benannt; packed struct + static_assert waere echte Verbesserung.

### 009-inconsistent-do-tags.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: cmd_get nutzt DO_NAME, cmd_put direkte Hex-Werte; vereinheitlichen.

### 010-hardcoded-utf8-constants.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: UTF-8-Bitmasken Standard-Algorithmus mit Inline-Kommentaren.

### 011-hardcoded-charge-status.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: chrgStat == 3 ohne Kontext problematisch; enum waere sauberer.

### 012-hardcoded-cose-key-constants.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: COSE-Labels (RFC8152) mehrfach genutzt; benannte Konstanten erhoehen Spec-Lesbarkeit.

### 013-hardcoded-cbor-string-keys.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Strings wie "FIDO_2_0", "rk", "alg" selbsterklaerend.

### 014-hardcoded-fnv1a-constants.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: FNV-Konstanten in 3 Dateien dupliziert; zentrale fnv1a_hash() echte DRY-Verbesserung.

### 015-hardcoded-totp-hash-sizes.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: 20/32/64 mit Kommentaren verstaendlich; benannte Konstanten leicht besser.

### 016-hardcoded-fido2-signature-sizes.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: 64 und 32 mehrfach in fido2_storage.cpp.

### 017-hardcoded-gpg-mpi-sizes.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: 32/34, 65/67 mehrfach in calculate_fingerprint*; benannte OpenPGP-MPI-Konstanten klarer.

### 018-hardcoded-gpg-algo-codes.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: 22/19 zweimal ohne Spec-Kontext; ALGO_EDDSA/ALGO_ECDSA aus RFC 4880 klar besser.

### 019-hardcoded-display-layout-constants.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Werte bereits pro View benannt; zentrales Header koennte Konsistenz erzwingen.

### 020-hardcoded-cbor-encoding-sizes.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: CBOR-Spec-Werte mit Inline-Kommentaren versehen; low-priority.

### 021-hardcoded-battery-voltage-thresholds.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Duplikat von Ticket 004.

### 022-hardcoded-gpg-fingerprint-sizes.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: Wert 20 fuer SHA-1-Fingerprint 6+ Mal; OPENPGP_FINGERPRINT_SIZE klare DRY-Verbesserung.

### 023-hardcoded-p256-public-key-size.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: [65] und 65 erscheint 8+ Mal in openpgp.cpp.

### 024-hardcoded-sec1-point-format.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: 0x04 fuer uncompressed-Prefix nur 3x mit Kommentaren; low-priority.

### 025-hardcoded-sha256-size.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: 32 fuer SHA-256, P-256-Privkey, ECDH-Secret; trennscharfe Konstanten klaeren Bedeutungen.

### 026-hardcoded-ecdsa-sig-size.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: 64 fuer ECDSA-Sig mit Kommentar selbsterklaerend; Konstante leichte Verbesserung.

### 027-hardcoded-openpgp-pw-refs.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: 0x81/0x82/0x83 6+ Mal verwendet; PW1_CODE_1/PW3_CODE klar lesbarer.

### 028-inconsistent-do-tag-usage.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: case 0x005B sollte case DO_NAME werden fuer Konsistenz mit cmd_get.

---

## naming (25)

### 001-snake-case-functions.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: extern "C"-Block nutzt snake_case korrekt; Wrapper-Namespace waere konsistenter.

### 002-boolean-member-naming-listview.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: ListItem ist POD-Struct (oeffentlich); trailing-underscore unueblich.

### 002-camelcase-parameters.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: privKey/pubKey/sigLen sind uebliche Krypto-Konventionen.

### 003-constant-naming-mixed.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: Nur 2 Konstanten brechen SCREAMING_SNAKE_CASE; lokal anpassbar.

### 004-generic-editMode-name.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: editMode in WizardState eindeutig.

### 004-struct-member-naming.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: Vorschlaege sind dogmatisch.

### 005-generic-eventbus-params.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Umbenennung zu intValue/ptrValue marginal hilfreich.

### 005-pin-method-prefix.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: PW1/PW3 sind offizielle OpenPGP-Card-Spec-Bezeichner.

### 006-enum-member-naming.md
- Behauptung stimmt: nein
- Verdikt: ABLEHNEN
- Begründung: Kategorie-Praefixe absichtlich zur Gruppierung.

### 007-abbreviation-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Echte Inkonsistenz beim Struct RMemHeader vs rmem*-Methoden.

### 008-file-naming-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: snake_case fuer ESP-IDF-Komponenten; PascalCase fuer Klassen-Files.

### 009-boolean-method-prefix.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: hasKey() vs isKeyPressed() koennte konsistenter sein.

### 010-c-style-functions.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Duplikat zu 001; extern "C" gewollt.

### 011-generic-loop-variables.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: i/j in kurzen Schleifen Standard.

### 012-mixed-case-style-constants.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: Duplikat zu 003 mit mehr Belegen.

### 013-abbreviation-inconsistency-module-names.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: PW1/PW3 sind OpenPGP-Spec-Begriffe.

### 014-boolean-member-names-without-prefix.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Past-Participle-Formen sind etablierte C++-Konventionen; is-Praefix nicht empfohlen.

### 015-c-style-functions-in-namespace.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: vcard_store.h ist C-Style waehrend PasswordStore/TotpStore C++-Klassen.

### 016-parameter-naming-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: vcard ist C-Style, daher snake_case dort gerechtfertigt.

### 017-struct-member-naming-mixed.md
- Behauptung stimmt: nein
- Verdikt: FALSCH
- Begründung: Ticket bestaetigt selbst dass Konvention konsistent angewandt wird.

### 018-enum-member-naming-pattern.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Key::KEY_1 redundant aber bewusst; Vorschlag _1 ist haesslich.

### 019-interface-naming-convention.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Ticket bestaetigt selbst saubere Konvention.

### 020-file-naming-consistency.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Mischung in mod_vcard (VcardModule.h vs vcard_store.h) echte Inkonsistenz.

### 021-boolean-method-prefix-inconsistency.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Mischung isKeyPressed/hasKey/anyKeyDown in IKeypad.h echt; niedrige Prioritaet.

### 022-generic-loop-variables.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: Duplikat zu 011.

---

## pattern-consistency (7)

### 001-inconsistent-i18n-registration.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: mod_nvsedit hardcoded English; mod_ble_serial nutzt einzig Klassenmethoden.

### 002-inconsistent-singleton-pattern.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Drei Patterns mit klaren Zwecken; Vereinheitlichung gegen ServiceRegistry-Architektur.

### 003-inconsistent-view-storage.md
- Behauptung stimmt: teilweise
- Verdikt: TEILWEISE
- Begründung: Nur Fido2 echter Ausreisser; Behauptung ueber mod_password falsch.

### 004-inconsistent-module-init-patterns.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: mod_fido2 hat Lazy-Registrierung in fido2_ui_get_label zusaetzlich zu fido2_ui_init.

### 005-inconsistent-nvs-namespace-patterns.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: NVS_PREFIX-Doku gilt nur fuer Module, nicht Core/HAL; eher Doku-Fix.

### 006-inconsistent-start-validation.md
- Behauptung stimmt: teilweise
- Verdikt: TEILWEISE
- Begründung: Nur GroveLedModule echter Ausreisser.

### 007-inconsistent-module-registration-timing.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: GroveLedModule registriert sich vor init() im Initializer-Lambda.

---

## readability (30)

### 001-dense-bitwise-operations.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Kommentar-Variante brauchbar; Helper-Funktion Overkill.

### 002-verbose-boolean-parameters.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: Beispiel falsch zitiert; Bikeshedding fuer simple Setter.

### 003-long-parameter-lists.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: Validierung der Pflichtargumente bewusst; Vorschlag aendert kaum Lesbarkeit.

### 004-magic-bitmasks-unexplained.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: Benannte Konstanten oder kompakter Ausdruck (0xFFF ^ (1<<i)) erleichtert Verifikation.

### 005-long-ternary-chains.md
- Behauptung stimmt: nein
- Verdikt: FALSCH
- Begründung: Keine "ternary chains" sondern switches; Lookup-Table waere langsamer.

### 006-redundant-bool-returns.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: Bikeshedding; Ticket gibt selbst zu "already reasonably clear".

### 007-mixed-abstraction-levels.md
- Behauptung stimmt: teilweise
- Verdikt: TEILWEISE
- Begründung: 35 Zeilen, keine echte Abstraktionsmischung.

### 008-unnecessary-cast-patterns.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: readInputs() mutiert nichts; const machen und const_cast eliminieren.

### 009-duplicate-code-patterns.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: register*Callback ~25 Zeilen fast identisch; Helper sinnvoll.

### 010-duplicated-string-tables.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: REG-Macro loest das bereits elegant; Vorschlag aequivalent.

### 011-go-statements-in-cpp.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: goto cleanup fuer mbedTLS-GCM; RAII-Wrapper idiomatisch.

### 012-unclear-boolean-parameters.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: Duplikat zu 002.

### 013-unclear-function-signatures.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: Standard-BLE-API mit Out-Pointern; Doxygen vorhanden.

### 014-unnecessary-const-cast.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: Tripel-const_cast plus saveToStorage aus const-Methode echtes Smell.

### 015-mixed-abstraction-levels.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: 258-Zeilen app_main mit 20+ Init-Stages; Extraktion Best-Practice.

### 016-nested-ternary-chains.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: Keine ternary chains, simple Modulo/Index-Lookups.

### 017-long-function-process-events.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: 56 Zeilen lineare Deserialisierung; Helper-Extraktion koennte Tests erleichtern.

### 018-repetitive-null-checks.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Init-Service-Helper waere nett, ueberlappt mit 015.

### 019-bit-packing-macros.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Diagramm-Doku der Tasten-Pin-Zuordnung sinnvoll.

### 020-inconsistent-naming.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: Inkonsistenzen teils erfunden, teils legitime C-API-Trennung.

### 021-deeply-nested-callbacks.md
- Behauptung stimmt: teilweise
- Verdikt: TEILWEISE
- Begründung: Keine Callbacks; 4-fache Nesting in taskFunc mit Guard-Clauses abflachbar.

### 022-duplicate-validation-logic.md
- Behauptung stimmt: ja
- Verdikt: SINNVOLL
- Begründung: verifyBadgePin/verifyPW1/verifyPW3 Copy-Paste; Sicherheits-Logik konsistent halten.

### 023-callback-function-pointer-api.md
- Behauptung stimmt: ja
- Verdikt: ABLEHNEN
- Begründung: void* userData bewusst gewaehlt fuer C-Kompatibilitaet ohne std::function-Overhead.

### 024-scattered-functions.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: Reihenfolge ueberwiegend logisch; Bikeshedding.

### 025-magic-numbers-rendering.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: Abgeleitete Konstanten sinnvoll; Layout-Diagramm optional.

### 026-repetitive-boolean-expressions.md
- Behauptung stimmt: ja
- Verdikt: TEILWEISE
- Begründung: openpgp PW1/PW3 Schleifen-Duplikation extrahierbar; AppUi-Helper sinnvoll.

### 027-redundant-inline-comments.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: Subjektiv; Empfehlung "extrahiere" Overengineering.

### 028-poor-section-organization.md
- Behauptung stimmt: nein
- Verdikt: FALSCH
- Begründung: ModuleRegistry.h hat 251 Zeilen, IKeypad.h 103, PasswordStore.h 77; alle klein genug.

### 029-excessive-casting-patterns.md
- Behauptung stimmt: teilweise
- Verdikt: TEILWEISE
- Begründung: const_cast-Punkt ueberlappt mit 008/014; uebrige Casts unvermeidbar.

### 030-inconsistent-naming-patterns.md
- Behauptung stimmt: teilweise
- Verdikt: ABLEHNEN
- Begründung: addEntry vs addAccount domaenen-gerecht; Bikeshedding.
