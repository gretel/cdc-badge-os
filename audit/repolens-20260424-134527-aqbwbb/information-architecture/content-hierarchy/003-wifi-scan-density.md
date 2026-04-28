---
title: "[MEDIUM] WiFi scan list shows 3 data points per row without clear visual hierarchy"
severity: MEDIUM
domain: information-architecture
lens: content-hierarchy
labels:
  - "audit:information-architecture/content-hierarchy"
---

## Summary
The WiFi scan result list (`components/cdc_os_ui/src/WifiMenuUi.cpp:143-197`) displays 3 data points per row: signal strength bars, SSID, and lock icon (for security). However, the layout lacks clear visual hierarchy:
- Signal bars and RSSI value compete for attention (bars shown but RSSI text also printed)
- Lock icon is small and positioned at the far right, easily missed
- No visual grouping of related information (SSID + security status)
- All rows have identical density regardless of content importance

The list supports up to 20 networks (`WIFI_MAX_NETWORKS = 20`) but only shows 4 visible rows at a time, requiring scrolling for larger results.

## Impact
Users scanning for networks may miss important information:
- Security status (lock icon) is subtle and could be overlooked
- RSSI value is shown both as bars AND text, creating redundancy
- No clear way to quickly identify open vs. secured networks at a glance
- Long SSIDs can be truncated without notification

## Evidence
File: `components/cdc_os_ui/src/WifiMenuUi.cpp:143-197`
```cpp
static bool renderWifiRow(Gdey029T94* gfx, const ListItem& item,
                          uint16_t index, int x, int y, int w, int h,
                          bool selected, void* userCtx) {
    // ...
    drawSignalBars(gfx, x + 4, baseline - 6, net ? net->rssi : -100, selected);

    const char* ssid = item.label ? item.label : "";
    // SSID displayed
    gfx->setCursor(x + 22, baseline);
    gfx->print(ssidDisplay);

    // Lock icon at far right
    if (net && net->security != hal::WifiSecurity::OPEN) {
        drawWifiLockIcon(gfx, x + w - 15, baseline - 5, selected);
    }
    // RSSI also printed as text
    snprintf(rssiBuf, sizeof(rssiBuf), "%d", dev->rssi);
    gfx->setCursor(x + w - rw - 4, baseline);
    gfx->print(rssiBuf);
}
```

The row renderer shows signal bars, SSID, lock icon, AND RSSI text - potentially too much information for one row.

## Recommended Fix
1. Remove RSSI text display when signal bars are shown (bars already convey signal strength visually)
2. Make lock icon more prominent or add a color/contrast indicator for security status
3. Add truncation indicator ("...") for SSIDs longer than available width
4. Consider showing security type (WPA2, WPA3) on secondary line for selected row

Approximate effort: 1 hour to refactor the row renderer and simplify the data density.

## References
- WiFi scan renderer: `components/cdc_os_ui/src/WifiMenuUi.cpp:143-197`
- ListView row customization: `components/cdc_views/src/ListView.cpp:220-250`
