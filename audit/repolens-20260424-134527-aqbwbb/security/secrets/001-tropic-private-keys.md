---
title: "[HIGH] TROPIC01 Secure Element Private Keys Committed to Repository"
severity: HIGH
domain: secrets
lens: secrets-storage
labels:
  - "provisioning-data"
---

## Summary
Private keys used for TROPIC01 secure element provisioning are committed directly to the repository in plaintext PEM format. These keys are located in the `third_party/libtropic/tropic01_model/provisioning_data/` directory and include:

1. **Engineering sample private key**: `sh0_priv_engineering_sample01.pem`
2. **Lab batch package private keys**:
   - `tropic01_ese_private_key.pem` - ESE (Embedded Secure Element) private key
   - `sh0_key_pair/sh_x25519_private_key_2025-03-24T09-15-15Z.pem` - SH0 key pair private key

File paths:
- `third_party/libtropic/tropic01_model/provisioning_data/sh0_priv_engineering_sample01.pem`
- `third_party/libtropic/tropic01_model/provisioning_data/2025-06-27T07-51-29Z__prod_C2S_T200__provisioning__lab_batch_package/tropic01_ese_private_key.pem`
- `third_party/libtropic/tropic01_model/provisioning_data/2025-06-27T07-51-29Z__prod_C2S_T200__provisioning__lab_batch_package/sh0_key_pair/sh_x25519_private_key_2025-03-24T09-15-15Z.pem`

## Impact
**Critical security implications:**
- Anyone with access to the repository can extract these private keys
- These keys are used for provisioning TROPIC01 secure elements - compromise allows:
  - Cloning of secure element configurations
  - Forgery of device certificates
  - Man-in-the-middle attacks using replicated device identities
- The keys appear to be production/lab batch keys (dated 2025-03-24 and 2025-06-27), suggesting they may still be valid
- An attacker could potentially provision counterfeit devices that appear authentic

## Evidence
The private keys are in standard PEM format:

```
-----BEGIN PRIVATE KEY-----
MC4CAQAwBQYDK2VuBCIEIPi5Y1TPtDEZm0DU2kQe2D5dt7PZPri7PKWODEoEaStM
-----END PRIVATE KEY-----
```

Key details from `tropic01_lab_batch_package.yml`:
```yaml
s_h0priv_key:                         sh0_key_pair/sh_x25519_private_key_2025-03-24T09-15-15Z.pem
tropic01_ese_private_key:             tropic01_ese_private_key.pem
```

The certificate shows these are test keys:
```
TROPIC01-X TEST CA v1
TROPIC01 ese TEST
```

## Recommended Fix
1. **Immediately rotate all compromised keys** - Generate new key pairs for:
   - Engineering sample keys
   - Lab batch package keys (tropic01_ese, sh0_key_pair)

2. **Remove keys from repository history**:
   ```bash
   # Using git filter-repo or BFG to wipe from history
   git filter-repo --path 'third_party/libtropic/tropic01_model/provisioning_data/**/*.pem' --force
   ```

3. **Add to .gitignore** to prevent future commits:
   ```
   # TROPIC01 provisioning data
   third_party/libtropic/tropic01_model/provisioning_data/*.pem
   third_party/libtropic/tropic01_model/provisioning_data/**/*_key_pair/*.pem
   ```

4. **Store keys securely**:
   - Use a secrets manager (AWS Secrets Manager, HashiCorp Vault)
   - Keep in a separate, private repository with restricted access
   - Use environment variables or CI/CD secrets for build-time access

5. **Document key management process**:
   - Create a `KEY_MANAGEMENT.md` document explaining where keys are stored
   - Define rotation schedule and procedures

## References
- [TROPIC01 Documentation](https://tropic01.com/)
- [NIST SP 800-57 Key Management](https://csrc.nist.gov/publications/detail/sp/800-57/part-1/rev-5/final)
- [GitHub Secrets Management](https://docs.github.com/en/actions/security-guides/using-secrets-in-github-actions)
- [Pre-commit secret detection](https://pre-commit.com/#secrets)
