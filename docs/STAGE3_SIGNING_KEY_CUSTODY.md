# LIKHI — STAGE 3 SIGNING KEY CUSTODY ARCHITECTURE
## Ed25519 Global Model Signing Key — Custody Design

**Document Version:** 1.0.0
**Status:** DESIGN SPECIFICATION — Awaiting Approval Before Implementation
**Author:** Architecture Team
**Date:** 2026-09-09

---

> [!CAUTION]
> This document describes the ARCHITECTURE for key custody.
> No private key has been generated, embedded, committed, or stored anywhere.
> Development and CI testing use CLEARLY LABELLED TEST KEYS ONLY.
> Production key generation requires the physical procedures described here.

---

## PART 1 — KEY ROLES AND RESPONSIBILITIES

### 1.1 Why This Key Exists

Every `global_model.bin` distributed to Likhi clients must be verified to originate
from the legitimate Likhi model build system. The Ed25519 private key is the only
mechanism that provides this assurance. If this key is compromised, an attacker can
distribute arbitrary model binaries that all Likhi clients will accept.

**This key is the single highest-impact secret in the entire Likhi system.**

### 1.2 Key Inventory

| Key Name | Algorithm | Purpose | Location |
| :--- | :--- | :--- | :--- |
| `likhi-model-signing-prod` | Ed25519 | Signs production `global_model.bin` releases | Offline HSM (see §2) |
| `likhi-model-signing-dev` | Ed25519 | Signs development builds and test artifacts | Developer workstation keystore (labelled DEV ONLY) |
| `likhi-model-signing-ci` | Ed25519 | Signs CI test artifacts (NOT production) | CI secrets store, isolated from production |

**Only `likhi-model-signing-prod` is accepted by production client builds.**
**`likhi-model-signing-dev` and `likhi-model-signing-ci` are pinned in test/debug builds only.**

### 1.3 Corresponding Public Key Pinning

The production public key is pinned inside `likhi_sync.exe` as a compile-time constant:

```cpp
// engine/src/sync/model_verifier.h
// PRODUCTION PUBLIC KEY — EMBEDDED AT BUILD TIME — NEVER CHANGES AT RUNTIME
// This is the ONLY public key that production clients accept for global_model.bin.
// Key rotation requires a client release with the new public key embedded.
// Source of truth: docs/STAGE3_SIGNING_KEY_CUSTODY.md

static constexpr uint8_t kProductionSigningPublicKey[32] = {
    // TO BE SET AFTER PRODUCTION KEY GENERATION CEREMONY
    // This array MUST be populated with the legitimate public key bytes
    // before any production build is compiled and released.
    // All bytes zeroed = production build MUST NOT PASS CI checks.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// DEV/TEST PUBLIC KEY — USED BY NON-PRODUCTION BUILDS ONLY
// If this key is present in a production binary, the CI build MUST FAIL.
static constexpr uint8_t kDevSigningPublicKey[32] = {
    // Populated with the dev test key bytes after key generation.
    // This constant is used in debug/test build targets only.
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
```

---

## PART 2 — PRODUCTION KEY CUSTODY ARCHITECTURE

### 2.1 Preferred Architecture: Air-Gapped Signing Machine

The production private key is stored on a **dedicated, permanently offline signing machine.**

```
┌──────────────────────────────────────────────────────────────────────┐
│  AIR-GAPPED SIGNING MACHINE                                          │
│  (Never connected to internet, LAN, Wi-Fi, or Bluetooth)            │
│                                                                      │
│  OS: Minimal Linux (e.g. Tails OS on USB, verified SHA-256 boot)    │
│  Storage: Encrypted volume (LUKS2 / AES-256-XTS)                    │
│                                                                      │
│  Key Location Options (choose one):                                  │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │ OPTION A (Preferred): Hardware Security Module (HSM)        │    │
│  │   - FIPS 140-2 Level 2+ device (e.g. YubiHSM2, Nitrokey)  │    │
│  │   - Private key generated ON the HSM (never exportable)     │    │
│  │   - Signing requires physical HSM presence                  │    │
│  │   - HSM PIN protected, lockout after N failed attempts      │    │
│  └─────────────────────────────────────────────────────────────┘    │
│  ┌─────────────────────────────────────────────────────────────┐    │
│  │ OPTION B (Fallback, pre-HSM): Encrypted key file           │    │
│  │   - Ed25519 private key file encrypted with AES-256-GCM    │    │
│  │   - Passphrase held only by authorized persons (see §3)    │    │
│  │   - Key file never written to networked storage             │    │
│  │   - Cold storage: 2 encrypted USB drives in separate       │    │
│  │     physically secured locations                            │    │
│  └─────────────────────────────────────────────────────────────┘    │
│                                                                      │
│  Data IN:  Unsigned global_model.bin  (via verified USB transfer)   │
│  Data OUT: global_model.bin + .sig file (via verified USB transfer) │
└──────────────────────────────────────────────────────────────────────┘
```

**USB Transfer Protocol:**
- USB drives used for file transfer are write-protected before use.
- Each transfer creates an audit log entry (see §4).
- USB drives are formatted and wiped before reuse.
- No autorun, no script execution on the signing machine from transferred files.

### 2.2 Signing Procedure (Step by Step)

Every model release follows this exact sequence. Any deviation must be documented.

```
STEP 1 — PREPARE MODEL BINARY (Online Build System)
  a. Run automated model build in isolated CI environment.
  b. CI produces: global_model.bin (unsigned)
  c. CI computes: SHA-256(global_model.bin) → build_manifest.txt
  d. CI prints: build_manifest.txt (including version, date, size, SHA-256)
  e. Human operator (authorized signatory) inspects moderation log:
       - Reviews human-approved vocabulary entries for this build.
       - Confirms no personal names, offensive content, or suspicious entries.
  f. Operator signs APPROVAL in the audit log (electronic + physical signature).

STEP 2 — TRANSFER TO SIGNING MACHINE (Offline)
  a. Copy global_model.bin + build_manifest.txt to clean write-protected USB.
  b. Boot signing machine from verified OS USB.
  c. Insert transfer USB (mount read-only).
  d. Verify SHA-256 of received global_model.bin matches build_manifest.txt.
     IF mismatch → ABORT. Investigate supply-chain issue.

STEP 3 — SIGN (Offline)
  HSM path:
    a. Unlock HSM with authorized PIN.
    b. Run: likhi-sign --hsm --key likhi-model-signing-prod \
                       --input global_model.bin \
                       --output global_model.bin.sig
    c. HSM performs signing internally. Private key never leaves HSM.
  Encrypted key file path (fallback):
    a. Decrypt key file using passphrase (entered by authorized person).
    b. Run: openssl pkeyutl -sign -inkey likhi-model-signing-prod.pem \
                   -in <SHA-256 of global_model.bin> \
                   -out global_model.bin.sig
    c. Wipe passphrase from memory after signing.

STEP 4 — VERIFY SIGNATURE (Still Offline)
  Run: likhi-verify --pubkey likhi-model-signing-prod.pub \
                    --input global_model.bin \
                    --sig global_model.bin.sig
  IF verification fails → DO NOT EXPORT. Investigate.

STEP 5 — EXPORT (Offline → Online)
  a. Copy global_model.bin + global_model.bin.sig to clean USB.
  b. Transfer to online distribution system.
  c. Online system performs final SHA-256 check.
  d. Publishes to CDN / download endpoint.

STEP 6 — AUDIT LOG ENTRY
  Record: date, time, operator, model version, SHA-256, sig SHA-256, outcome.
```

### 2.3 Authorized Signatories

A minimum of **2 authorized persons** must be present during any signing ceremony.
"Buddy system" is mandatory — no solo signings.

| Role | Responsibility |
| :--- | :--- |
| Primary Signatory | Unlocks HSM / provides passphrase half A |
| Secondary Signatory | Provides passphrase half B (Shamir share or dual-control) |
| Observer (optional) | Records audit log, witnesses procedure |

For initial solo-developer beta: at minimum, two-factor procedure (HSM PIN + encrypted key file)
with documentation before each signing. Dual-person control should be implemented before public release.

---

## PART 3 — KEY STORAGE AND BACKUP

### 3.1 Primary Storage

| Environment | Storage Location | Access |
| :--- | :--- | :--- |
| Production | FIPS 140-2 HSM on air-gapped machine | Physical presence + HSM PIN |
| Backup A | Encrypted key file on encrypted USB (AES-256-GCM) | Physical possession + passphrase |
| Backup B | Second encrypted USB in separate physical location | Physical possession + passphrase |

### 3.2 Backup USB Locations

The two backup USBs must be stored in **physically separate, access-controlled locations**:
- Different buildings or rooms
- Different lock combinations or key holders
- Neither accessible to a single person alone
- Location documented in a sealed physical document held by a trusted party

### 3.3 What Is Never Stored

The following locations must NEVER contain the production private key or passphrase:

| Location | Status |
| :--- | :--- |
| Source code repository (Git) | ❌ NEVER |
| CI/CD environment variables | ❌ NEVER |
| Production server filesystem | ❌ NEVER |
| `likhi_sync.exe` binary | ❌ NEVER |
| `bangla_tsf.dll` binary | ❌ NEVER |
| Installer binary | ❌ NEVER |
| Cloud storage (S3, GCS, Azure Blob) | ❌ NEVER |
| Email, Slack, Teams, chat | ❌ NEVER |
| Password manager shared vault | ❌ NEVER unless encrypted with HSM-backed key |
| Developer laptop keychain | ❌ NEVER (dev key only, never production) |

---

## PART 4 — AUDIT TRAIL

### 4.1 Signing Log Format

Every signing event appends to a physical signing log (paper) and a digital log:

```
====================================================
LIKHI MODEL SIGNING EVENT
====================================================
Date / Time:       2026-XX-XX HH:MM:SS UTC
Model Version:     10001
global_model.bin SHA-256: <64 hex chars>
global_model.bin.sig SHA-256: <64 hex chars>
Signed By:         [Primary Signatory Name]
Witnessed By:      [Secondary Signatory Name]
Outcome:           SUCCESS / FAILURE
Failure Reason:    (if applicable)
Notes:             (any deviations from standard procedure)
====================================================
```

Physical log is stored with the signing machine. Digital log is replicated to a
separate read-only append-only storage location.

### 4.2 Periodic Audit Schedule

| Frequency | Audit Activity |
| :--- | :--- |
| After every signing | Verify log entry is complete and signed |
| Monthly | Verify HSM is accessible, backup USB checksums unchanged |
| Quarterly | Full key custody review by authorized persons |
| Annually | Consider key rotation (see §5) |

---

## PART 5 — KEY ROTATION PROCEDURE

### 5.1 Scheduled Rotation (Annual or as needed)

Key rotation requires a **client release** because the new public key must be pinned in the binary.

**Rotation Sequence:**

```
STEP 1 — Generate new Ed25519 keypair on HSM / signing machine.
STEP 2 — Record new public key bytes.
STEP 3 — Prepare new Likhi client release:
          Update kProductionSigningPublicKey constant with new public key.
          Update kPreviousProductionSigningPublicKey constant with old public key.
          (Dual-key support: client accepts BOTH keys during rotation window.)
STEP 4 — Build, test, sign, and release new client version.
STEP 5 — Begin signing new models with new key.
STEP 6 — After N days (rotation window), remove old public key from kPreviousKey.
STEP 7 — Release client update removing old key support.
STEP 8 — Physically destroy or archive old private key per §5.3.
STEP 9 — Update audit log.
```

### 5.2 Forced Rotation (Key Compromise Suspected)

If the private key is believed to be compromised:

```
STEP 1 — IMMEDIATELY notify all authorized signatories.
STEP 2 — Generate new keypair on a NEWLY VERIFIED signing machine.
STEP 3 — Publish emergency client update with new public key.
          This update MUST replace the compromised key, not add alongside it.
STEP 4 — Publish a signed revocation notice on the /api/v1/model/version endpoint:
          { "revoked_model_version_threshold": <last_good_version> }
          All clients that receive this will stop loading models below the threshold.
STEP 5 — Deploy a new model signed with the new key immediately.
STEP 6 — Investigate origin of compromise.
STEP 7 — Conduct full audit of all models signed with the compromised key.
STEP 8 — Document findings and remediation in post-incident report.
```

### 5.3 Key Archival / Destruction

After rotation:
- Old HSM key slot is zeroized (irreversibly destroyed on the HSM).
- Backup USB with old key is physically destroyed (shredded or degaussed).
- Destruction is witnessed, documented, and recorded in the audit log.

---

## PART 6 — DEVELOPMENT AND CI KEY MANAGEMENT

### 6.1 Development Key (likhi-model-signing-dev)

**Purpose:** Signing model binaries during local development and manual testing.

| Property | Value |
| :--- | :--- |
| Generation | Generated by developer locally: `openssl genpkey -algorithm ed25519` |
| Storage | Developer machine keychain or local encrypted `.env` (NOT committed) |
| Rotation | Per-developer, as needed |
| CI use | NEVER. CI uses `likhi-model-signing-ci` only. |
| Accepted by | Debug/development builds only (`#ifdef LIKHI_DEV_BUILD`) |

**Labelling requirement:** Every test model binary signed with the dev key MUST embed
the string `"DEV_BUILD_ONLY"` in the header comment field of the LGM1 binary format.
The production client MUST reject any model with this label.

### 6.2 CI Test Key (likhi-model-signing-ci)

**Purpose:** Signing test model binaries in the CI pipeline for automated testing of
model load, verification, rollback, and format validation.

| Property | Value |
| :--- | :--- |
| Generation | Generated at CI secret setup time by an authorized person |
| Storage | CI secrets store (e.g. GitHub Actions Encrypted Secrets, isolated to test workflows) |
| Rotation | Annually or on team membership change |
| Accepted by | Test build targets only |
| Used by | `test_model_verification.cpp`, `test_rollback.cpp`, CI integration tests |

**Isolation requirement:** The CI key is scoped to the CI environment only. Its
corresponding public key MUST NOT appear in production build targets. A CI build step
must verify this: grep production source for the CI public key bytes → fail build if found.

### 6.3 Key Segregation Build Check

The following build-time check must pass before any production binary is signed:

```python
# tools/build/check_key_segregation.py
# Run as part of the production build pipeline.

import sys

PRODUCTION_PUBKEY_PLACEHOLDER = bytes(32)  # all zeros = not set
DEV_PUBKEY_BYTES = b'...'    # actual dev key bytes
CI_PUBKEY_BYTES  = b'...'    # actual CI key bytes

def check(binary_path):
    with open(binary_path, 'rb') as f:
        data = f.read()

    if DEV_PUBKEY_BYTES in data:
        print("FAIL: Dev public key found in production binary.")
        sys.exit(1)

    if CI_PUBKEY_BYTES in data:
        print("FAIL: CI public key found in production binary.")
        sys.exit(1)

    if PRODUCTION_PUBKEY_PLACEHOLDER in data:
        print("FAIL: Production public key is all-zeros (not initialized).")
        sys.exit(1)

    print("PASS: Key segregation check OK.")
```

---

## PART 7 — CLIENT-SIDE VERIFICATION IMPLEMENTATION

The following verification sequence is mandatory in `likhi_sync.exe` before any
downloaded model is used:

```cpp
// engine/src/sync/model_verifier.cpp

enum class ModelVerifyResult {
    kOK,
    kFileMissing,
    kReadError,
    kHashMismatch,
    kSignatureInvalid,
    kMagicMismatch,
    kVersionTooOld,
    kVersionTooNew,          // min_client_version > installed version
    kSchemaInvalid,
    kDevKeyInProduction,     // SECURITY: dev-labelled model rejected in prod build
};

ModelVerifyResult VerifyModelFile(const wchar_t* path,
                                  const uint8_t* expected_sha256_32,
                                  const uint8_t* expected_sig_64) {
    // 1. File must exist and be readable.
    // 2. SHA-256(file contents) must match expected_sha256_32.
    // 3. Ed25519 verify(kProductionSigningPublicKey, SHA-256, expected_sig_64) must succeed.
    // 4. First 4 bytes must be 'L','G','M','1'.
    // 5. global_model_version must be > current loaded version.
    // 6. min_client_version in header must be <= installed client version.
    // 7. Header must not contain "DEV_BUILD_ONLY" label.
    // Returns kOK only if ALL 7 checks pass.
    // Any single failure returns immediately with the specific error code.
    // No partial loads. No retries on verification failure.
}
```

**No step may be skipped.** If any step fails, the `.tmp` file is deleted immediately,
the current model is retained, and typing continues without interruption.

---

## PART 8 — EMERGENCY PROCEDURES

### 8.1 HSM Lost or Destroyed

1. Retrieve backup USB from secure location B.
2. Boot fresh signing machine from verified OS.
3. Decrypt backup key using passphrase held by authorized persons.
4. Proceed with signing ceremony using encrypted key file path.
5. Order replacement HSM immediately.
6. Generate new keypair on replacement HSM within 30 days.
7. Initiate key rotation procedure (§5.1).

### 8.2 Signing Machine Lost or Destroyed

1. Signing machine contains no extractable private key (HSM stores it, or encrypted USB stores it separately).
2. Acquire a new air-gapped machine.
3. Verify OS integrity on new machine.
4. Transfer HSM to new machine, OR use backup USB on new machine.
5. Conduct signing with witnesses and record in audit log.

### 8.3 All Backups Lost (Catastrophic Key Loss)

If both HSM and all backup USBs are simultaneously unrecoverable:

1. The production private key is permanently lost.
2. No new production models can be signed with the compromised/lost key.
3. Initiate forced rotation procedure (§5.2, steps 1-8).
4. New production key generated, new client release required.
5. Models already distributed remain valid (they carry the old correct signature,
   and old clients can still verify them until the client update is deployed).
6. Document the incident fully. Conduct retrospective.

---

## PART 9 — CHECKLIST BEFORE PRODUCTION KEY GENERATION

The following items must be confirmed before generating the production key:

| # | Item | Confirmed By | Date |
|:--|:---|:---|:---|
| C1 | Air-gapped signing machine is prepared and verified | | |
| C2 | HSM is procured, tested, and firmware verified | | |
| C3 | Two authorized signatories are identified and available | | |
| C4 | Secure physical location for signing machine confirmed | | |
| C5 | Two backup USB locations are identified and secured | | |
| C6 | Audit log format agreed and physical log book available | | |
| C7 | Key segregation CI check is written and passing for dev/CI keys | | |
| C8 | Model verification code in `likhi_sync.exe` is complete and tested | | |
| C9 | Emergency procedures reviewed by all authorized persons | | |
| C10 | This document has been reviewed and approved | | |

**Production key MUST NOT be generated until all 10 items are confirmed.**

---

## APPENDIX A — WHY NOT GENERATE THE KEY IN CI?

CI environments are connected to the internet, share secrets with multiple workflows,
and are accessible to anyone with repository admin access. A key generated in CI could
be exposed through:
- CI secret leakage (a known attack vector)
- Log file exposure
- Artifact upload to public storage
- Malicious dependency injection via supply-chain attack

**Conclusion:** The production signing key MUST be generated offline, on an air-gapped
machine, by authorized persons, and never transmitted over any network.

## APPENDIX B — WHY NOT STORE THE KEY IN A CLOUD KMS?

Cloud KMS (AWS KMS, GCP Cloud KMS, Azure Key Vault) are viable for many use cases.
For Likhi's initial beta, a cloud KMS introduces:
- Dependency on internet connectivity for every signing operation
- Trust in a third-party cloud provider
- Exposure to account takeover or API credential theft

**Future consideration:** For a larger production deployment, a cloud HSM-backed KMS
may be appropriate, provided that signing is done via the KMS API (key never exported)
and access is restricted to the build pipeline service account with full audit logging.
This decision requires a separate architecture review.
