#!/usr/bin/env python3
"""
CDC Badge OS - Plugin Upload Tool

Uploads compiled WebAssembly plugins (.wasm + .meta.json) to a connected
badge over USB-CDC using the PLUGIN UPLOAD serial protocol. Mirrors the
chunk-CRC32 format implemented in components/plugin_manager/src/PluginSerialCommands.cpp.

Usage:
    python tools/upload_plugin.py --wasm hello_world.wasm --meta hello_world.meta.json
    python tools/upload_plugin.py --list
    python tools/upload_plugin.py --info ha
    python tools/upload_plugin.py --delete totp_helper
    python tools/upload_plugin.py --start ha
    python tools/upload_plugin.py --release-url https://github.com/krim404/cdc-badge-plugins/releases/download/v0.1.0 --plugin ha

Required: pyserial (`pip install pyserial`).
"""

import argparse
import binascii
import glob
import json
import os
import sys
import time
from pathlib import Path

DEFAULT_CHUNK = 256              # bytes/write; binary stream, no encoding overhead
SERIAL_BAUD   = 115200
ACK_TIMEOUT_S = 5
END_TIMEOUT_S = 15
MAX_RETRIES   = 5


def detect_port():
    for pattern in ("/dev/cu.usbmodem*", "/dev/ttyUSB*", "/dev/ttyACM*", "COM*"):
        ports = glob.glob(pattern)
        if ports:
            return ports[0]
    return None


def open_port(port):
    try:
        import serial
    except ImportError:
        sys.exit("ERROR: install pyserial -> pip install pyserial")
    p = serial.Serial(port, SERIAL_BAUD, timeout=ACK_TIMEOUT_S)
    # Short settle, then drain the boot banner so it doesn't trip the
    # AUTH response parser.
    time.sleep(0.2)
    p.reset_input_buffer()
    return p


def authenticate(p, pin):
    """Authenticate the serial session via AUTH <pin>. Required when
    FEATURE_SECURE_SERIAL is enabled on the badge."""
    if not pin:
        return

    send_line(p, f"AUTH {pin}")
    # Collect response lines until we see a definitive verdict.
    deadline = time.time() + 5
    while time.time() < deadline:
        resp = readline(p, timeout=1)
        if not resp:
            continue
        u = resp.upper()
        if "AUTHENTICATED" in u or u.startswith("OK"):
            return
        if "WRONG" in u or "LOCKED" in u or u.startswith("ERROR"):
            raise RuntimeError(f"AUTH failed: {resp}")
    raise RuntimeError("AUTH timed out (no OK response)")


def readline(p, timeout=ACK_TIMEOUT_S):
    p.timeout = timeout
    line = p.readline().decode("utf-8", errors="replace").rstrip("\r\n")
    return line


def send_line(p, line):
    # Use bare LF as command terminator. With CRLF the badge would execute
    # on \r, install the byte interceptor for the upload, then the trailing
    # \n would be swallowed as the first payload byte and the whole-stream
    # CRC would never match.
    p.write((line + "\n").encode("utf-8"))
    p.flush()


def crc32_hex(data):
    return binascii.crc32(data) & 0xFFFFFFFF


_KIND_CMD = {
    "wasm": "UPLOAD",
    "aot":  "UPLOAD_AOT",
    "meta": "UPLOAD_META",
    "lang": "UPLOAD_LANG",
}


def upload_file(p, plugin_id, path, kind, chunk_size=DEFAULT_CHUNK, progress=None):
    """Stream `path` to the badge as raw binary.

    Protocol (new):
        > PLUGIN UPLOAD[_META|_LANG] <id> <size> <crc32_hex>
        < READY
        > <size bytes of raw payload>
        < OK <size>     |     ERR <reason>
    """
    data = Path(path).read_bytes()
    total = len(data)
    crc   = crc32_hex(data)

    subcmd = _KIND_CMD.get(kind)
    if not subcmd:
        raise ValueError(f"unknown upload kind: {kind!r}")

    send_line(p, f"PLUGIN {subcmd} {plugin_id} {total} {crc:08x}")
    ready = wait_for(p, ["READY", "ERR"], timeout=5)
    if ready is None:
        raise RuntimeError("no READY response from badge")
    if ready.startswith("ERR"):
        raise RuntimeError(f"badge refused upload: {ready}")

    # Stream the payload in chunks. The badge's TinyUSB CDC RX buffer is
    # ~256 bytes, so we pace writes to that boundary to avoid USB flow-
    # control stalls and keep progress reporting useful.
    sent = 0
    while sent < total:
        end = min(sent + chunk_size, total)
        p.write(data[sent:end])
        p.flush()
        sent = end
        if progress:
            progress(sent / total)

    final = wait_for(p, ["OK", "ERR"], timeout=END_TIMEOUT_S)
    if not final or not final.startswith("OK"):
        raise RuntimeError(f"upload not finalised: {final!r}")
    return final


def wait_for(p, prefixes, timeout=5):
    """Read lines until one starts with any of `prefixes`, ignoring echoes,
    prompts and log lines. The badge's shell prompt `> ` may glue itself to
    the start of a response when output and input race, so strip leading
    prompt characters before matching."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        resp = readline(p, timeout=1)
        if not resp:
            continue
        s = resp.strip()
        # Drop any leading prompt fragment, e.g. "> OK 504" -> "OK 504".
        while s.startswith(">"):
            s = s[1:].lstrip()
        for pref in prefixes:
            if s.startswith(pref):
                return s
    return None


def safe_abort(p):
    """Best-effort ABORT to release a hung upload interceptor on the badge."""
    try:
        send_line(p, "PLUGIN ABORT")
        readline(p, timeout=1)
    except Exception:
        pass


def cmd_upload(args):
    if not args.port:
        args.port = detect_port() or sys.exit("ERROR: no USB serial port detected, use --port")
    p = open_port(args.port)
    authenticate(p, args.pin)

    plugin_id = args.id or json.loads(Path(args.meta).read_text())["id"]
    try:
        print(f"Uploading meta → {args.meta}")
        upload_file(p, plugin_id, args.meta, "meta",
                    progress=lambda f: print(f"  meta {f*100:5.1f} %", end="\r"))
        print()
        binary_kind = "aot" if args.wasm.lower().endswith(".aot") else "wasm"
        print(f"Uploading {binary_kind} → {args.wasm}")
        upload_file(p, plugin_id, args.wasm, binary_kind,
                    progress=lambda f: print(f"  {binary_kind} {f*100:5.1f} %", end="\r"))
        print()
        if args.lang:
            print(f"Uploading lang → {args.lang}")
            upload_file(p, plugin_id, args.lang, "lang",
                        progress=lambda f: print(f"  lang {f*100:5.1f} %", end="\r"))
            print()
        print(f"Installed {plugin_id}.")
    except Exception:
        safe_abort(p)
        raise


def cmd_list(args):
    p = open_port(args.port or detect_port() or sys.exit("ERROR: no port"))
    authenticate(p, args.pin)
    send_line(p, "PLUGIN LIST")
    # The badge streams a JSON array as several lines.
    out = []
    end_time = time.time() + 5
    while time.time() < end_time:
        line = readline(p, timeout=1)
        if not line:
            continue
        out.append(line)
        if line.strip().endswith("]"):
            break
    print("\n".join(out))


def cmd_info(args):
    p = open_port(args.port or detect_port() or sys.exit("ERROR: no port"))
    authenticate(p, args.pin)
    send_line(p, f"PLUGIN INFO {args.info}")
    deadline = time.time() + 5
    while time.time() < deadline:
        line = readline(p, timeout=1)
        if not line:
            continue
        print(line)
        if line.startswith("ERR") or line.startswith("prereqs:"):
            break


def cmd_delete(args):
    p = open_port(args.port or detect_port() or sys.exit("ERROR: no port"))
    authenticate(p, args.pin)
    send_line(p, f"PLUGIN DELETE {args.delete}")
    print(readline(p))


def cmd_start(args):
    p = open_port(args.port or detect_port() or sys.exit("ERROR: no port"))
    authenticate(p, args.pin)
    send_line(p, f"PLUGIN START {args.start}")
    print(readline(p, timeout=10))


def cmd_stop(args):
    p = open_port(args.port or detect_port() or sys.exit("ERROR: no port"))
    authenticate(p, args.pin)
    send_line(p, "PLUGIN STOP")
    print(readline(p))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="Serial port (auto-detected if omitted)")
    ap.add_argument("--id",   help="Plugin id (overrides meta.json#id)")
    grp = ap.add_mutually_exclusive_group(required=True)
    grp.add_argument("--wasm", help="Path to plugin .wasm or .aot (with --meta)")
    grp.add_argument("--list", action="store_true")
    grp.add_argument("--info", metavar="ID")
    grp.add_argument("--delete", metavar="ID")
    grp.add_argument("--start",  metavar="ID")
    grp.add_argument("--stop",   action="store_true")
    ap.add_argument("--meta", help="Path to plugin meta.json (required with --wasm)")
    ap.add_argument("--lang", help="Path to plugin <id>.lang.json (optional; ships translations)")
    ap.add_argument("--pin",  help="Badge PIN for AUTH (required when FEATURE_SECURE_SERIAL=1)")
    args = ap.parse_args()

    if args.wasm:
        if not args.meta:
            ap.error("--meta is required with --wasm")
        cmd_upload(args)
    elif args.list:    cmd_list(args)
    elif args.info:    cmd_info(args)
    elif args.delete:  cmd_delete(args)
    elif args.start:   cmd_start(args)
    elif args.stop:    cmd_stop(args)


if __name__ == "__main__":
    main()
