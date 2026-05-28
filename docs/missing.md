# Plugin Host API - Outstanding Stubs

Snapshot of host API functions that still return `HOST_ERR_NOT_SUPPORTED`
on the badge. All other host_api families are fully implemented and
registered with WAMR. Source of truth is
`components/plugin_manager/src/host_api_stubs.cpp`.

## Low-Level Display (`host_display_*`)

Drawing primitives that would let a plugin paint pixels directly to the
e-paper buffer. Gated behind the `display_lowlevel` capability in the
manifest. The high-level views (Toast, Info, List, T9, ...) are already
covered by the host UI APIs, so this family is only useful for plugins
that want full custom rendering.

- `host_display_width`, `host_display_height`
- `host_display_clear`
- `host_display_draw_pixel`, `host_display_draw_line`
- `host_display_draw_rect`, `host_display_fill_rect`
- `host_display_draw_text`
- `host_display_flush`
- `host_display_is_busy`

What it takes: route the calls through `cdc::hal::getDisplayInstance()`,
respect the existing dirty-region tracking in `EpaperDisplay`, and add a
small capability check (no calls while a plugin view is on the stack).

## BLE GATT server + client (`host_ble_*`)

Read-only state (`is_enabled`, `mac`, `device_name`, `rssi`) is live.
Everything that *changes* BLE state is still stubbed because each call
needs a per-plugin handle registry with a capability-checked UUID
whitelist - otherwise a plugin could shadow vCard / HID / NUS services.

- `host_ble_register_service`, `host_ble_unregister_service`
- `host_ble_send_notification`, `host_ble_send_indication`
- `host_ble_scan_start`, `host_ble_scan_results`
- `host_ble_connect`, `host_ble_disconnect`
- `host_ble_read_char`, `host_ble_write_char`
- `host_ble_subscribe`

What it takes: extend `CapabilityChecker` to enforce
`capabilities.ble_service_uuids` at registration time and reject
duplicate UUIDs across plugins. Then wire each call through
`IBluetoothController`, allocating per-plugin handles.

## Keypad direct poll (`host_key_*`)

Primary input path is `plugin_on_button` from the EventBus. Direct
polling stays stubbed because the keypad has a single callback slot
that the host already owns.

- `host_key_pressed`
- `host_key_consume_next`

What it takes: arbitration with the host's existing
`IKeypad::setCallback`. Only worth doing for plugins that need polling
semantics (rare).

## USB CDC write (`host_usb_*`)

- `host_usb_cdc_write`

Intentionally not implemented. Allowing a plugin to write to the USB
CDC console would let it impersonate firmware log output and confuse
the user. Use `host_log` instead - it routes through cdc_log and goes
to the same stream with a `[PLUGIN]` tag.

## Tracking

- 9 display, 11 BLE-server/-client, 2 keypad, 1 USB = 23 stubs that
  will need real implementations before the host API moves to 1.0.

Update this file (and the API level constant in `host_api.h`) whenever
a family lands.
