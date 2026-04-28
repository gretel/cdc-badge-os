---
title: "[LOW] Engineering Sample Private Key in Repository"
severity: LOW
domain: secrets
lens: provisioning-data
labels:
  - "engineering-data"
  - "tropic01"
---

## Summary
An engineering sample private key for TROPIC01 secure element is committed to the repository. While this appears to be an engineering/test key rather than a production key, it still represents a potential attack surface if the key is used for any validation or authentication purposes.

File path:
- `third_party/libtropic/tropic01_model/provisioning_data/sh0_priv_engineering_sample01.pem`

Key content (truncated):
```
-----BEGIN PRIVATE KEY-----
MC4CAQAwBQYDK2VuBCIEINCZkrHxerxNuTcXaKJ9oFsY+rhWE6eELKZMeRDyLnFr
-----END PRIVATE KEY-----
```

## Impact
**Lower risk than production keys, but still notable:**

1. **Engineering keys may have elevated privileges** - Could be used for factory provisioning or bypassing certain security checks
2. **Key format consistency** - Same format as production keys, may be interchangeable in some contexts
3. **Documentation leakage** - Reveals key structure and format to potential attackers
4. **Accidental use risk** - Developers might use this key in production builds if not careful

## Evidence
File exists at:
```
third_party/libtropic/tropic01_model/provisioning_data/sh0_priv_engineering_sample01.pem
```

Key type: Ed25519 (indicated by `BQYDK2Vu` in base64, which is Ed25519 OID)

## Recommended Fix
1. **Add to .gitignore** specifically for engineering data:
   ```
   third_party/libtropic/tropic01_model/provisioning_data/sh0_priv*.pem
   ```

2. **Consider removing from repository** if not needed for builds:
   - Move to a separate engineering-only repository
   - Store in a secrets manager for CI/CD access

3. **Document usage** - Create a note explaining:
   - What this key is used for
   - Where the "live" version is stored
   - Rotation schedule

## References
- [TROPIC01 Documentation](https://tropic01.com/)
- [Ed25519 Key Format](https://datatracker.ietf.org/doc/html/rfc8410)
