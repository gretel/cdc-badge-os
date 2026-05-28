#!/usr/bin/env python3
"""CDC Badge OS - i18n overlay (lang.json) upload tool.

Uploads a translation overlay file to a connected badge over USB-CDC using
the `LANG UPLOAD` serial protocol. The badge writes the file to
`/plugins/i18n/lang.json` on the plugins FAT partition and reloads the
overlay automatically afterwards.

Usage:
    python tools/upload_lang.py assets/i18n/lang.json
    python tools/upload_lang.py assets/i18n/lang.json --port /dev/cu.usbmodem1101

Required: pyserial (`pip install pyserial`).
"""

from __future__ import annotations

import argparse
import binascii
import glob
import sys
import time
from pathlib import Path

CHUNK = 256
SERIAL_BAUD = 115200
ACK_TIMEOUT_S = 5
END_TIMEOUT_S = 15
MAX_RETRIES = 5


def detect_port() -> str | None:
    for pattern in ("/dev/cu.usbmodem*", "/dev/ttyUSB*", "/dev/ttyACM*", "COM*"):
        ports = glob.glob(pattern)
        if ports:
            return ports[0]
    return None


def open_port(port: str):
    try:
        import serial  # type: ignore
    except ImportError:
        sys.exit("ERROR: install pyserial -> pip install pyserial")
    p = serial.Serial(port, SERIAL_BAUD, timeout=ACK_TIMEOUT_S)
    p.reset_input_buffer()
    return p


def readline(p, timeout=ACK_TIMEOUT_S) -> str:
    p.timeout = timeout
    return p.readline().decode("utf-8", errors="replace").rstrip("\r\n")


def send_line(p, line: str) -> None:
    p.write((line + "\n").encode("utf-8"))
    p.flush()


def upload(p, data: bytes) -> str:
    total = len(data)
    send_line(p, f"LANG UPLOAD {total}")
    while True:
        resp = readline(p)
        if resp == "READY":
            break
        if resp.startswith("ERR"):
            raise RuntimeError(f"badge refused upload: {resp}")
        if not resp:
            raise RuntimeError("no response from badge")

    sent = 0
    idx = 0
    while sent < total:
        chunk = data[sent : sent + CHUNK]
        crc = binascii.crc32(chunk) & 0xFFFFFFFF
        hex_ = chunk.hex().upper()

        for attempt in range(MAX_RETRIES):
            send_line(p, f"{idx} {crc} {hex_}")
            resp = readline(p)
            if resp == f"ACK {idx}":
                break
            if resp.startswith("NACK") and attempt + 1 < MAX_RETRIES:
                time.sleep(0.05)
                continue
            raise RuntimeError(f"chunk {idx} rejected: {resp!r}")

        sent += len(chunk)
        idx += 1
        print(f"  {sent / total * 100:5.1f} %", end="\r")

    print()
    send_line(p, "END")
    final = readline(p, timeout=END_TIMEOUT_S)
    if not final.startswith("OK"):
        raise RuntimeError(f"upload not finalised: {final!r}")
    return final


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("lang_json", type=Path, help="Path to lang.json")
    ap.add_argument("--port", help="Serial port (auto-detected if omitted)")
    args = ap.parse_args()

    if not args.lang_json.is_file():
        print(f"error: file not found: {args.lang_json}", file=sys.stderr)
        return 1

    port = args.port or detect_port()
    if not port:
        print("error: no serial port found; pass --port", file=sys.stderr)
        return 1

    print(f"Uploading {args.lang_json} via {port}")
    data = args.lang_json.read_bytes()

    try:
        result = upload(open_port(port), data)
        print(f"Done: {result}")
    except RuntimeError as e:
        print(f"error: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
