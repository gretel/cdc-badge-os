#!/usr/bin/env python3
"""
CDC Badge OS - Core Dump Analyzer

Reads and analyzes ESP32 core dumps from flash memory.

Usage:
    python tools/coredump.py [port]

If port is not specified, it will try to find a USB device automatically.

Requirements:
    pip install esp-coredump esptool

The script will:
1. Read the core dump from flash (partition at 0xFF0000)
2. Analyze it using espcoredump with GDB
"""

import subprocess
import sys
import os
import glob

# Configuration
COREDUMP_OFFSET = 0xFF0000
COREDUMP_SIZE = 0x10000
COREDUMP_FILE = "coredump.bin"
FIRMWARE_ELF = ".pio/build/cdc_badge_usb/firmware.elf"

# PlatformIO paths
PLATFORMIO_DIR = os.path.expanduser("~/.platformio")
ESPTOOL = os.path.join(PLATFORMIO_DIR, "packages/tool-esptoolpy/esptool.py")
ESPCOREDUMP = os.path.join(PLATFORMIO_DIR, "packages/framework-espidf/components/espcoredump/espcoredump.py")
GDB = os.path.join(PLATFORMIO_DIR, "packages/tool-xtensa-esp-elf-gdb/bin/xtensa-esp32s3-elf-gdb")
PIO_PYTHON = os.path.join(PLATFORMIO_DIR, "penv/bin/python")


def find_port():
    """Find USB serial port."""
    patterns = ["/dev/cu.usbmodem*", "/dev/ttyUSB*", "/dev/ttyACM*"]
    for pattern in patterns:
        ports = glob.glob(pattern)
        if ports:
            return ports[0]
    return None


def run_command(cmd, description):
    """Run a command and handle errors."""
    print(f"\n>>> {description}")
    print(f"    Command: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=False)
    if result.returncode != 0:
        print(f"    FAILED with exit code {result.returncode}")
        return False
    return True


def main():
    # Get port
    if len(sys.argv) > 1:
        port = sys.argv[1]
    else:
        port = find_port()
        if not port:
            print("ERROR: No USB port found. Please specify port as argument.")
            print("Usage: python tools/coredump.py /dev/cu.usbmodem1101")
            sys.exit(1)

    print(f"Using port: {port}")

    # Check if firmware.elf exists
    if not os.path.exists(FIRMWARE_ELF):
        print(f"ERROR: {FIRMWARE_ELF} not found. Please build the firmware first.")
        sys.exit(1)

    # Step 1: Read core dump from flash
    print("\n" + "="*60)
    print("STEP 1: Reading core dump from flash")
    print("="*60)

    cmd = [
        PIO_PYTHON, "-m", "esptool",
        "--port", port,
        "read_flash",
        hex(COREDUMP_OFFSET),
        hex(COREDUMP_SIZE),
        COREDUMP_FILE
    ]

    # Try using pio pkg exec instead
    cmd = [
        os.path.expanduser("~/.platformio/penv/bin/pio"),
        "pkg", "exec", "--",
        "esptool.py",
        "--port", port,
        "read_flash",
        hex(COREDUMP_OFFSET),
        hex(COREDUMP_SIZE),
        COREDUMP_FILE
    ]

    if not run_command(cmd, "Reading core dump from flash"):
        print("\nTip: Put device in download mode (hold BOOT while pressing RESET)")
        sys.exit(1)

    # Step 2: Analyze core dump
    print("\n" + "="*60)
    print("STEP 2: Analyzing core dump")
    print("="*60)

    cmd = [
        PIO_PYTHON,
        ESPCOREDUMP,
        "info_corefile",
        "-t", "raw",
        "-c", COREDUMP_FILE,
        FIRMWARE_ELF,
        "--gdb", GDB
    ]

    if not run_command(cmd, "Analyzing core dump"):
        print("\nTip: Make sure esp-coredump is installed:")
        print("     ~/.platformio/penv/bin/pip install esp-coredump")
        sys.exit(1)

    print("\n" + "="*60)
    print("DONE")
    print("="*60)


if __name__ == "__main__":
    main()
