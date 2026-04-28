---
title: "[LOW] No Firmware Binary Integrity Verification on Flash"
severity: LOW
domain: deployment-safety
lens: deployment-safety
labels:
  - "audit:devops/deployment-safety"
---

## Summary

The flash tools (`tools/flash_firmware.py` and web flasher) do not verify binary integrity before or after flashing. No checksums, hashes, or signatures are validated.

**Evidence:**
- `flash_firmware.py` downloads binaries but no hash verification
- Web flasher uses `esp-web-tools` which doesn't verify by default
- No manifest with checksums for release binaries

## Impact

**Security:**
- Corrupted downloads can be flashed without detection
- No protection against man-in-the-middle attacks
- No verification of binary authenticity

**Reliability:**
- Partial downloads may cause boot failures
- No post-flash verification

## Evidence

**flash_firmware.py** (lines 118-132):
```python
for asset in assets:
    name = asset["name"]
    if not name.endswith(".bin"):
        continue
    dl_url = asset["browser_download_url"]
    dest = os.path.join(tmpdir, name)
    print(f"  Downloading {name} ({asset['size'] // 1024} KB)...")
    r = requests.get(dl_url, timeout=60)
    r.raise_for_status()
    with open(dest, "wb") as f:
        f.write(r.content)  # No hash verification
```

**Web flasher** (`web-flasher/index.html`):
- Uses `esp-web-tools` manifest
- No checksum verification in manifest.json

## Recommended Fix

Add basic integrity verification:

1. **Add checksums to release process:**
   ```python
   # tools/flash_firmware.py - add after download
   import hashlib
   
   def verify_hash(filepath, expected_hash):
       with open(filepath, 'rb') as f:
           actual_hash = hashlib.sha256(f.read()).hexdigest()
       return actual_hash == expected_hash
   ```

2. **Create checksum file for releases:**
   ```bash
   # Generate during release
   sha256sum *.bin > checksums.txt
   ```

3. **Verify in flash tool:**
   ```python
   # Download checksums and verify
   checksums = download_checksums()
   for addr, path in binaries.items():
       if not verify_hash(path, checksums[os.path.basename(path)]):
           print(f"ERROR: {path} checksum mismatch")
           sys.exit(1)
   ```

**Estimated effort:** 1 hour

## References

- [ESP-IDF Secure Boot](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/esp-idf/en/latest/esp32s3/api-reference/system/security.html)
- SHA-256 for binary verification

</content>