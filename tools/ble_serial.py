#!/usr/bin/env python3
from __future__ import annotations
"""
BLE Serial Console for CDC Badge OS

Connects to the CDC Badge via Bluetooth Low Energy (Nordic UART Service)
and provides an interactive serial console, similar to 'pio device monitor'.

Usage:
    python3 tools/ble_serial.py              # Auto-scan for CDC Badge
    python3 tools/ble_serial.py --address XX:XX:XX:XX:XX:XX  # Connect to specific device
    python3 tools/ble_serial.py --scan       # Scan only, don't connect

Requirements:
    pip install bleak
"""

import argparse
import asyncio
import sys

try:
    from bleak import BleakClient, BleakScanner
except ImportError:
    print("Error: 'bleak' package required. Install with: pip install bleak")
    sys.exit(1)

# Nordic UART Service UUIDs
NUS_SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
NUS_RX_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e"  # Write to device
NUS_TX_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e"  # Notify from device

DEFAULT_DEVICE_NAME = "CDC Badge"
SCAN_TIMEOUT = 8.0


async def scan_devices(name_filter: str | None = None) -> list:
    """Scan for BLE devices, optionally filtering by name."""
    print(f"Scanning for BLE devices ({SCAN_TIMEOUT}s)...")
    devices = await BleakScanner.discover(
        timeout=SCAN_TIMEOUT,
        service_uuids=[NUS_SERVICE_UUID],
    )

    results = []
    for d in devices:
        if name_filter and name_filter.lower() not in (d.name or "").lower():
            continue
        results.append(d)

    return results


async def find_badge(name_filter: str) -> str | None:
    """Scan and return the address of the first matching device."""
    devices = await scan_devices(name_filter)
    if not devices:
        print(f"No device matching '{name_filter}' with NUS service found.")
        return None

    if len(devices) > 1:
        print(f"Found {len(devices)} matching devices:")
        for i, d in enumerate(devices):
            rssi = d.rssi if hasattr(d, "rssi") else "?"
            print(f"  [{i}] {d.name or '?':<20} {d.address}  RSSI: {rssi}")
        try:
            choice = int(input("Select device [0]: ") or "0")
            return devices[choice].address
        except (ValueError, IndexError):
            print("Invalid selection.")
            return None

    dev = devices[0]
    print(f"Found: {dev.name} ({dev.address})")
    return dev.address


async def run_scan():
    """Scan and list all NUS-capable BLE devices."""
    devices = await scan_devices()
    if not devices:
        print("No BLE devices with NUS service found.")
        return

    print(f"\nFound {len(devices)} device(s) with NUS service:\n")
    print(f"  {'Name':<24} {'Address':<20} {'RSSI'}")
    print(f"  {'-'*24} {'-'*20} {'-'*6}")
    for d in devices:
        name = d.name or "(unknown)"
        rssi = d.rssi if hasattr(d, "rssi") else "?"
        print(f"  {name:<24} {d.address:<20} {rssi}")


def on_notify(_sender, data: bytearray):
    """Handle incoming data from the badge."""
    try:
        text = data.decode("utf-8", errors="replace")
        sys.stdout.write(text)
        sys.stdout.flush()
    except Exception:
        pass


async def input_loop(client: BleakClient):
    """Read user input and send to device."""
    loop = asyncio.get_event_loop()
    while client.is_connected:
        try:
            line = await loop.run_in_executor(None, sys.stdin.readline)
            if not line:
                break
            # Send with \r\n as the serial command parser expects
            payload = line.rstrip("\n") + "\r\n"
            data = payload.encode("utf-8")
            # Chunk to MTU-safe size (20 bytes default)
            chunk_size = 20
            for i in range(0, len(data), chunk_size):
                chunk = data[i : i + chunk_size]
                await client.write_gatt_char(NUS_RX_UUID, chunk, response=False)
        except asyncio.CancelledError:
            break
        except Exception as e:
            print(f"\n[TX Error: {e}]")
            break


async def run_console(address: str):
    """Connect to device and run interactive console."""
    print(f"Connecting to {address}...")

    disconnected = asyncio.Event()

    def on_disconnect(_client):
        print("\n[Disconnected]")
        disconnected.set()

    async with BleakClient(address, disconnected_callback=on_disconnect) as client:
        print(f"Connected. MTU: {client.mtu_size}")
        print("Type commands and press Enter. Ctrl+C to exit.\n")

        # Subscribe to TX notifications
        await client.start_notify(NUS_TX_UUID, on_notify)

        # Run input loop until disconnect or Ctrl+C
        input_task = asyncio.create_task(input_loop(client))
        disconnect_task = asyncio.create_task(disconnected.wait())

        try:
            done, pending = await asyncio.wait(
                [input_task, disconnect_task],
                return_when=asyncio.FIRST_COMPLETED,
            )
            for task in pending:
                task.cancel()
        except asyncio.CancelledError:
            pass


async def main():
    parser = argparse.ArgumentParser(
        description="BLE Serial Console for CDC Badge OS",
    )
    parser.add_argument(
        "--address", "-a",
        help="BLE device address (skip scanning)",
    )
    parser.add_argument(
        "--name", "-n",
        default=DEFAULT_DEVICE_NAME,
        help=f"Device name filter for scanning (default: '{DEFAULT_DEVICE_NAME}')",
    )
    parser.add_argument(
        "--scan", "-s",
        action="store_true",
        help="Scan for devices and exit",
    )
    args = parser.parse_args()

    if args.scan:
        await run_scan()
        return

    address = args.address
    if not address:
        address = await find_badge(args.name)
        if not address:
            sys.exit(1)

    await run_console(address)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n[Exit]")
