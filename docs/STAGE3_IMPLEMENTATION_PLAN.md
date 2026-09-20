# LIKHI — STAGE 3 IMPLEMENTATION PLAN
## Global Learning Architecture — Phased Implementation

**Document Version:** 1.0.0
**Status:** READY FOR REVIEW — Awaiting Signing-Key Approval Before Execution
**Date:** 2026-09-09
**Depends On:** STAGE3_SIGNING_KEY_CUSTODY.md (must be approved first)

---

## OVERVIEW

Stage 3 adds Global Learning capability to Likhi in 16 strictly ordered phases.
Each phase ends with BUILD + TEST + REGRESSION + SECURITY REVIEW before the next begins.
No phase skips allowed. No phase merging without explicit approval.

**The TSF typing path is never modified.** All additions are in new components
(`likhi_sync.exe`) or in read-only integration points within `bangla_engine.cpp`.

---

## PHASE 3.1 — Global Model Reader
**Goal:** `BanglaEngine` can optionally read from `global_model.bin` if it is present.
**Scope:** `bangla_tsf.dll` only. No network. No new process.

### Files to Create / Modify

#### [NEW] `engine/src/global_model/global_model_reader.h`
```
Class: GlobalModelReader
Methods:
  Open(path) → bool
  GetCandidateScore(roman_key, bengali_word) → float  [0.0–1.0]
  IsLoaded() → bool
  Close()
Internal:
  Memory-mapped read-only file handle (CreateFileMappingW + MapViewOfFile)
  LGM1 magic validation on Open
  Returns 0.0f for any key not in model
  Zero blocking: all lookups O(1) hash-bucket probe
```

#### [NEW] `engine/src/global_model/global_model_reader.cpp`
```
Implements GlobalModelReader.
Open() sequence:
  1. Validate first 4 bytes == 'L','G','M','1'
  2. Validate global_model_version field
  3. Validate min_client_version <= LIKHI_CLIENT_VERSION
  4. If any validation fails: set loaded=false, return false, log to debug log
  5. If all pass: set loaded=true, store mapped view pointer
GetCandidateScore():
  If not loaded: return 0.0f immediately (no crash, no exception)
  Hash-bucket probe on roman_key → find bengali_word match → return score
  If not found: return 0.0f
```

#### [MODIFY] `engine/src/bangla_engine.cpp`
```
Add GlobalModelReader as optional member.
In GetCandidates():
  If global_model_reader_.IsLoaded():
    score += kGlobalModelWeight * global_model_reader_.GetCandidateScore(roman_key, candidate)
This must NOT change behavior when global_model.bin is absent.
All 881 existing tests must pass unchanged.
```

### Exit Criteria for Phase 3.1
- [ ] `GlobalModelReader::Open()` returns false gracefully on missing/corrupt file
- [ ] `GlobalModelReader::Open()` returns false gracefully on wrong magic
- [ ] `GlobalModelReader::GetCandidateScore()` returns 0.0f when not loaded
- [ ] Memory-mapped file is opened PAGE_READONLY (verified by test)
- [ ] All 881 regression tests pass with and without `global_model.bin` present
- [ ] `bangla_tsf.dll` import table unchanged (no new DLL imports)
- [ ] No performance regression on candidate generation (< 2ms per call)

---

## PHASE 3.2 — Local Staging Queue
**Goal:** `BanglaEngine` can optionally append de-identified tuples to `staged_learning.dat`.
**Scope:** New class `StagingQueue` in `bangla_tsf.dll`. No network.

### Files to Create / Modify

#### [NEW] `engine/src/staging/staging_queue.h`
```
Class: StagingQueue
Methods:
  Initialize(path, consent_provider) → bool
  TryAppend(roman_key, bengali_word, vote_weight, week_bucket) → bool
  IsEnabled() → bool
  Disable()
Internal:
  CRC32 per record
  File opened with FILE_APPEND_DATA | FILE_SHARE_READ | FILE_SHARE_WRITE
  If file open fails: silently returns false (never crashes or throws)
  Checks consent_provider before every append
  Applies Rarity Gate (all 4 rules) before every append
```

#### [NEW] `engine/src/staging/rarity_gate.h` / `.cpp`
```
Class: RarityGate
Methods:
  PassesAllRules(roman_key, bengali_word, personal_dict, lexicon) → bool
Rules (all must pass):
  R1: lexicon.Contains(bengali_word) && lexicon.GetRank(bengali_word) <= 100000
  R2: phonetic_index.HasMapping(roman_key)
  R3: personal_dict.GetFrequency(roman_key, bengali_word) >= 5
  R4: !personal_dict.HasFlag(roman_key, bengali_word, kFlagManualCustom | kFlagContributionExcluded)
```

#### [NEW] `engine/src/staging/staged_record.h`
```
Struct: StagedRecord
  uint32_t magic = 0x4C4B5354  // 'LKST'
  uint8_t  roman_key_len
  char     roman_key[24]
  uint8_t  bengali_len        // bytes in UTF-8
  char     bengali_word[120]  // max 40 codepoints * 3 bytes
  uint8_t  vote_weight = 1
  char     week_bucket[8]     // "YYYY-WNN"
  uint32_t crc32              // CRC32 of all preceding fields
```

#### [MODIFY] `engine/src/bangla_engine.cpp`
```
In BanglaEngine_CommitWordWithOrigin():
  After personal_dict.AddWord() and SaveToFile():
    staging_queue_.TryAppend(roman_key, bengali_word, 1, GetCurrentWeekBucket())
  TryAppend is fire-and-forget: return value ignored.
  No blocking wait. No exception propagation.
```

### Exit Criteria for Phase 3.2
- [ ] `StagingQueue::TryAppend()` silently returns false when consent is off
- [ ] `StagingQueue::TryAppend()` silently returns false when file open fails
- [ ] Rarity Gate rejects words not in lexicon (R1)
- [ ] Rarity Gate rejects roman keys with no lexicon mapping (R2)
- [ ] Rarity Gate rejects pairs with local frequency < 5 (R3)
- [ ] Rarity Gate rejects `FLAG_MANUAL_CUSTOM` words (R4)
- [ ] All 881 regression tests pass (staging file absent = normal operation)
- [ ] **PAYLOAD FIELD TEST PASSES** (see `STAGE3_SECURITY_GATE.md §2`)
- [ ] `bangla_tsf.dll` import table unchanged (no new DLL imports)

---

## PHASE 3.3 — Consent Controls
**Goal:** Settings system for Personal Learning, Global Contribution, and Download Global Improvements.
**Scope:** Settings stored in `HKCU\Software\Likhi\`. Read by TSF at load. Modified by UI.

### Files to Create / Modify

#### [NEW] `engine/src/settings/consent_provider.h` / `.cpp`
```
Class: ConsentProvider
Methods:
  IsPersonalLearningEnabled() → bool        // default: true
  IsGlobalContributionEnabled() → bool      // default: false (HARD DEFAULT)
  IsModelDownloadEnabled() → bool           // default: false
  SetPersonalLearning(bool)
  SetGlobalContribution(bool)
  SetModelDownload(bool)
  RevokeGlobalContribution()               // calls SetGlobalContribution(false)
                                           // sets kFlagContributionRevoked
                                           // next: signals sync worker to delete queue
Storage: HKCU\Software\Likhi\Settings
Keys:
  PersonalLearning     REG_DWORD  default=1
  GlobalContribution   REG_DWORD  default=0  ← MUST be 0 in all code paths
  ModelDownload        REG_DWORD  default=0
  ContributionRevoked  REG_DWORD  default=0
```

#### [MODIFY] `engine/src/staging/staging_queue.cpp`
```
TryAppend():
  first check: consent_provider_.IsGlobalContributionEnabled()
  If false: return false immediately (no logging, no side effects)
```

#### [MODIFY] `bangla_engine.cpp`
```
In CommitWordWithOrigin():
  If consent_provider_.IsPersonalLearningEnabled():
    personal_dict.AddWord() + SaveToFile()
  If consent_provider_.IsGlobalContributionEnabled():
    staging_queue_.TryAppend(...)
```

### Exit Criteria for Phase 3.3
- [ ] `GlobalContribution` registry key defaults to `0` in clean install
- [ ] `ModelDownload` registry key defaults to `0` in clean install
- [ ] `PersonalLearning` registry key defaults to `1` in clean install
- [ ] Setting `GlobalContribution=0` stops all staging immediately (in same process invocation)
- [ ] `RevokeGlobalContribution()` sets `ContributionRevoked=1` AND `GlobalContribution=0`
- [ ] Settings are independent: disabling one does not affect the others
- [ ] All 881 regression tests pass

---

## PHASE 3.4 — Sync Worker (likhi_sync.exe) — Skeleton
**Goal:** Create `likhi_sync.exe` as a separate process. No network yet. Handles:
consent revocation queue cleanup, staging queue age management.

### Files to Create

#### [NEW] `sync_worker/main.cpp`
```
Entry point for likhi_sync.exe.
Arguments: --check-consent, --upload, --download, --cleanup
For Phase 3.4 scope: --cleanup only.
```

#### [NEW] `sync_worker/consent_cleanup.h` / `.cpp`
```
Class: ConsentCleanup
Methods:
  Run(staged_dat_path, consent_provider) → void
Logic:
  Read ConsentProvider from registry.
  If GlobalContribution == 0 AND ContributionRevoked == 1:
    Delete staged_learning.dat
    Set ContributionRevoked = 0 (cleanup complete)
  Prune records older than 7 days from staged_learning.dat.
  Log result to debug log.
```

#### [NEW] `sync_worker/CMakeLists.txt`
```
Target: likhi_sync.exe
Links: kernel32, user32, advapi32 only (for Phase 3.4)
       ws2_32 added ONLY in Phase 3.5 when network is introduced
```

### Exit Criteria for Phase 3.4
- [ ] `likhi_sync.exe --cleanup` runs without crashing
- [ ] `--cleanup` deletes `staged_learning.dat` when `ContributionRevoked=1`
- [ ] `--cleanup` prunes records older than 7 days
- [ ] `--cleanup` does nothing when `ContributionRevoked=0` and contribution is enabled
- [ ] `likhi_sync.exe` import table contains NO network DLLs at this phase
- [ ] All 881 regression tests still pass (`bangla_tsf.dll` unchanged)

---

## PHASE 3.5 — Secure Model Download
**Goal:** `likhi_sync.exe` can check the version endpoint and download a new model binary.
**Scope:** Network code enters `likhi_sync.exe` for the first time. `bangla_tsf.dll` is UNCHANGED.

### Files to Create

#### [NEW] `sync_worker/model_downloader.h` / `.cpp`
```
Class: ModelDownloader
Methods:
  CheckForUpdate(current_version) → {has_update, version, sha256, sig_url, download_url}
  Download(download_url, out_path) → bool
Internal:
  WinINet or libcurl (statically linked or system DLL)
  TLS 1.3 only (reject TLS 1.2 and below)
  Certificate pinning: verify server cert chain against pinned intermediate CA
  Timeout: connect=10s, download=120s
  User-Agent: "Likhi/<version> (Windows; Model Sync)"
  No cookies. No session tokens. No auth headers.
  Follows HTTP 3xx redirects only to same domain
  Download to .tmp file only (never directly to final path)
```

#### [NEW] `sync_worker/http_client.h` / `.cpp`
```
Thin wrapper around WinINet / libcurl.
Certificate pinning implementation.
Retry logic: exponential backoff, max 3 retries, jitter.
```

### Exit Criteria for Phase 3.5
- [ ] `ModelDownloader` downloads to `.tmp` only (never to final path)
- [ ] TLS 1.2 connection is rejected (tested with a TLS 1.2-only test server)
- [ ] Certificate mismatch causes download abort (tested with self-signed cert)
- [ ] Download timeout correctly aborts after configured limit
- [ ] `bangla_tsf.dll` import table is UNCHANGED (no network DLLs)
- [ ] `likhi_sync.exe` correctly checks `IsModelDownloadEnabled()` before any HTTP call
- [ ] All 881 regression tests pass

---

## PHASE 3.6 — Signature and Integrity Verification
**Goal:** All downloaded models are verified (SHA-256 + Ed25519 + magic + schema) before use.
**Scope:** `sync_worker/model_verifier.cpp`. NO production key exists yet — CI test key used.

### Files to Create

#### [NEW] `sync_worker/model_verifier.h` / `.cpp`
```
Class: ModelVerifier
Methods:
  Verify(tmp_path, expected_sha256, expected_sig) → ModelVerifyResult
Internal:
  Step 1: Open file, compute SHA-256, compare to expected_sha256
  Step 2: Ed25519 verify(public_key, sha256_bytes, expected_sig)
  Step 3: Read first 4 bytes, check == 'L','G','M','1'
  Step 4: Read global_model_version, check > current_version
  Step 5: Read min_client_version, check <= installed version
  Step 6: Check header does NOT contain "DEV_BUILD_ONLY" in production builds
  Returns kOK only if all 6 steps pass.
  On any failure: delete .tmp, return error code, log to debug log.
```

#### [NEW] `engine/src/sync/model_verifier.h` (shared header)
```
Contains kProductionSigningPublicKey (all-zeros until key generation ceremony).
Contains kDevSigningPublicKey (populated with dev test key).
Build system selects which key to embed based on LIKHI_BUILD_TYPE flag.
```

#### [NEW] `tools/build/check_key_segregation.py`
```
CI step: verify production binary does not contain dev/CI public key bytes.
Fail build if detected.
```

### Exit Criteria for Phase 3.6
- [ ] Correct SHA-256 + correct sig → `kOK`
- [ ] Correct SHA-256 + wrong sig → `kSignatureInvalid`, `.tmp` deleted
- [ ] Wrong SHA-256 + anything → `kHashMismatch`, `.tmp` deleted
- [ ] Missing magic → `kMagicMismatch`, `.tmp` deleted
- [ ] Old version → `kVersionTooOld`, `.tmp` deleted
- [ ] Dev-labelled model → `kDevKeyInProduction` in production build, `.tmp` deleted
- [ ] Key segregation CI check passes
- [ ] All 881 regression tests pass

---

## PHASE 3.7 — Model Installation and Rollback
**Goal:** Atomic swap of verified model into active position. `.bak` chain maintained.

### Files to Create

#### [NEW] `sync_worker/model_installer.h` / `.cpp`
```
Class: ModelInstaller
Methods:
  Install(verified_tmp_path, active_path, bak_path) → bool
  Rollback(active_path, bak_path) → bool
  CleanOldBackup(bak_path, max_age_days=30) → void
Install() sequence:
  1. Rename active_path → bak_path (MoveFileExW REPLACE_EXISTING)
  2. MoveFileExW(tmp → active, REPLACE_EXISTING | WRITE_THROUGH)
  3. If step 2 fails: attempt to restore bak → active, return false
Rollback() sequence:
  1. Delete active_path
  2. Rename bak_path → active_path
  3. Return true if new active exists, false otherwise
```

#### [MODIFY] `engine/src/global_model/global_model_reader.cpp`
```
On next engine initialization after model update:
  Close old memory-mapped view.
  Open new global_model.bin.
  If new file fails validation: use bak or return not-loaded.
  Typing never waits for this — engine continues with previous state
  until the next initialization cycle.
```

### Exit Criteria for Phase 3.7
- [ ] Install: `.tmp` → `.bin`, old `.bin` → `.bak` (both verified by file hash)
- [ ] Install failure: `.bak` restored as active, `.tmp` deleted
- [ ] Rollback: `.bak` → `.bin`, new candidates reflect previous model
- [ ] Corrupted `.bak`: fallback to lexicon-only (IsLoaded=false), typing continues
- [ ] Old `.bak` (> 30 days): cleanup routine deletes it
- [ ] All 881 regression tests pass
- [ ] Specific rollback tests from security gate pass (see `STAGE3_SECURITY_GATE.md §4`)

---

## PHASE 3.8 — Server Ingestion API
**Goal:** Design and implement the server-side contribution ingestion endpoint.
**Scope:** Server code (separate from client). Client is NOT modified in this phase.

### Server Components

#### `POST /api/v1/contribute`
```
Request validation:
  - PoW response valid
  - JWT / HMAC request integrity (no auth, just request integrity)
  - Content-Type: application/json
  - Max payload size: 8 KB
  - Max 20 contribution tuples per request
Payload field validation per tuple:
  - roman_key: ^[a-z]{1,24}$
  - selected_bengali: valid Unicode, Bangla script only (block U+0980–U+09FF),
                      1–40 codepoints
  - vote_weight: must be 1 (fixed, rejected if not exactly 1)
  - week_bucket: YYYY-WNN format, max 7 days in past (future dates rejected)
IP handling:
  - Source IP: used for rate-limit counter (ephemeral Redis, 60s TTL) ONLY
  - Source IP: NEVER written to application database or logs
  - Nginx configured: access_log off; or custom log format without $remote_addr
Response:
  200 OK: {"accepted": N, "rejected": M, "challenge": "<next_pow_nonce>"}
  429 Too Many Requests: {"error": "rate_limited", "retry_after": N}
  400 Bad Request: {"error": "invalid_payload", "detail": "<reason>"}
```

#### `GET /api/v1/challenge`
```
Returns PoW challenge: {"nonce": "<32 hex chars>", "difficulty": 20}
Difficulty = number of leading zero bits required in SHA-256(nonce || answer)
```

#### `GET /api/v1/model/version`
```
Returns:
{
  "current_version": 10001,
  "min_client_version": "1.0.0",
  "sha256": "<64 hex chars>",
  "signature": "<128 hex chars (Ed25519 sig)>",
  "signature_url": "https://cdn.likhi.app/models/10001/global_model.bin.sig",
  "download_url": "https://cdn.likhi.app/models/10001/global_model.bin",
  "revoked_before": 0   // optional: all versions below this are revoked
}
```

### Exit Criteria for Phase 3.8
- [ ] Server rejects payloads with non-Bangla Bengali words
- [ ] Server rejects vote_weight != 1
- [ ] Server rejects future week_bucket values
- [ ] Server rejects payloads larger than 8 KB
- [ ] Server NEVER logs source IP in application database
- [ ] Server-side log format confirmed to exclude source IP (verified by log inspection)
- [ ] Rate limiting works (429 returned at correct threshold)
- [ ] PoW validation works (invalid PoW rejected)

---

## PHASE 3.9 — Aggregation Pipeline
**Goal:** Server aggregates received tuples into per-(roman_key, bengali_word) frequency counts.
**Scope:** Server only. No client changes.

### Aggregation Logic
```
On receiving valid tuple (roman_key, bengali_word):
  1. Increment crowd_count for (roman_key, bengali_word) in aggregation store.
  2. Record distinct_day (date only, not time) for this (roman_key, bengali_word, source_subnet_24).
     (This is used only for the distinct-days threshold check, not stored per-user.)
  3. Track distinct_subnet_24 count for this (roman_key, bengali_word).
  4. Delete individual raw record after aggregation (within 48 hours).
  5. Pairs that have not reached K=50 within 90 days are purged.
```

### Exit Criteria for Phase 3.9
- [ ] Duplicate tuples from the same source increment count only once per subnet per day
- [ ] 90-day purge of sub-threshold pairs works correctly
- [ ] Individual raw records are deleted after aggregation (verified by database inspection)
- [ ] Aggregation store contains no IP addresses, device IDs, or user identifiers

---

## PHASE 3.10 — Automated Validation (Stage A)
**Goal:** Server-side automated checks before any candidate enters the moderation queue.
**Scope:** Server only.

### Automated Checks (all must pass to enter moderation queue)
```
1. crowd_count >= 50
2. distinct_days >= 7
3. No single /24 subnet provides > 10% of crowd_count
4. selected_bengali is valid Unicode Bangla sequence (full grapheme check)
5. Server-side phonetic alignment score >= 0.45
6. roman_key length: 1–24 chars
7. selected_bengali codepoints: 1–40
8. Toxicity classifier confidence < 0.20
9. selected_bengali exists in current lexicon.bin
```

### Exit Criteria for Phase 3.10
- [ ] Pairs below K=50 do not enter moderation queue
- [ ] Pairs with < 7 distinct days do not enter moderation queue
- [ ] Single-subnet-concentrated pairs are flagged for security review
- [ ] Non-Bangla Unicode rejected at validation
- [ ] Words not in lexicon rejected
- [ ] All 9 checks are logged per candidate for moderation audit trail

---

## PHASE 3.11 — Human Moderation Workflow
**Goal:** A simple, functional moderation interface for reviewing approved candidates.
**Scope:** Internal admin tool only. Not user-facing.

### Moderation Interface Requirements
```
For each candidate in the queue, show:
  - roman_key
  - selected_bengali
  - crowd_count
  - distinct_days
  - subnet_diversity percentage
  - automated check results (all 9, pass/fail)
  - existing lexicon entry for this roman_key (for comparison)

Moderator actions:
  [✅ Approve] [❌ Reject: <reason>] [⏸ Defer: elevate to K=200]

Moderation audit log:
  Timestamp, moderator ID, roman_key, bengali_word, decision, reason
```

### Exit Criteria for Phase 3.11
- [ ] Moderation queue only shows candidates that passed all Stage A checks
- [ ] Moderator cannot publish a model directly — approval only queues the entry
- [ ] Audit log is append-only and includes moderator identity for every decision
- [ ] Rejected entries are permanently discarded and cannot be re-submitted by clients
- [ ] Deferred entries have elevated K threshold enforced by system

---

## PHASE 3.12 — Signed Model Generation
**Goal:** Produce a signed `global_model.bin` from approved moderation queue entries.
**Scope:** Offline build system + signing procedure. Uses CI test key during development.

### Build Process
```
1. Export approved entries from moderation queue.
2. Merge with current model vocabulary.
3. Run build_model tool:
   - Generates LGM1 binary
   - Embeds version, min_client_version, hash-bucket layout
   - Writes unsigned global_model.bin
4. Compute SHA-256 of global_model.bin.
5. Transfer to signing machine (air-gapped, per key custody procedure).
6. Sign using Ed25519 private key.
7. Verify signature on signing machine.
8. Transfer signed binary + signature back.
9. Publish to CDN + update /api/v1/model/version endpoint.
```

### Exit Criteria for Phase 3.12
- [ ] `build_model` tool produces valid LGM1 binary (tested with CI test key)
- [ ] Signing ceremony procedure tested with CI test key (not production key)
- [ ] `GlobalModelReader::Open()` accepts the CI-signed test model
- [ ] `ModelVerifier::Verify()` passes for CI-signed model
- [ ] `ModelVerifier::Verify()` rejects model signed with dev key in CI build target

---

## PHASE 3.13 — End-to-End Integration
**Goal:** Full pipeline tested end-to-end with CI test keys and a test server.
**Scope:** Integration test environment only. NO production data.

### End-to-End Test Scenario
```
1. User types "ami" and selects "আমি" 5 times locally.
2. Rarity Gate passes (common word, in lexicon, freq >= 5).
3. Tuple appended to staged_learning.dat.
4. likhi_sync.exe --upload runs, sends batch to test server.
5. Test server aggregates to K=50 (seeded with mock votes).
6. Stage A automated validation passes.
7. Moderator approves in moderation UI.
8. build_model generates new global_model.bin (CI-signed).
9. likhi_sync.exe --download discovers new version, downloads to .tmp.
10. ModelVerifier passes all 6 checks.
11. ModelInstaller performs atomic swap.
12. BanglaEngine reloads global_model.bin on next initialization.
13. "ami" → "আমি" has elevated global score in candidate ranking.
14. All 881 regression tests still pass.
```

### Exit Criteria for Phase 3.13
- [ ] Full scenario above completes without error
- [ ] Consent revocation mid-scenario: staging stops, queue deleted, upload aborted
- [ ] Network failure mid-download: .tmp deleted, current model retained, typing continues
- [ ] Corrupted .tmp: verification fails, .tmp deleted, current model retained
- [ ] Bad signature: verification fails, .tmp deleted, current model retained
- [ ] All 881 regression tests pass

---

## PHASE 3.14 — Security Testing
**Goal:** Dedicated adversarial testing of all security controls.
See `STAGE3_SECURITY_GATE.md` for complete test list.

---

## PHASE 3.15 — Privacy Testing
**Goal:** Automated verification that outgoing payloads contain only approved fields.
See `STAGE3_SECURITY_GATE.md §2` for payload field allowlist/denylist tests.

---

## PHASE 3.16 — Offline Regression Testing
**Goal:** Verify that the complete offline typing experience is unaffected by Stage 3.

### Offline Test Scenarios
```
1. global_model.bin absent → typing works, base lexicon used
2. global_model.bin corrupt → typing works, model ignored
3. global_model.bin wrong signature → typing works, model ignored
4. global_model.bin present but model version too old → typing works
5. network unavailable → likhi_sync.exe exits cleanly, no error to user
6. server returns 500 → likhi_sync.exe retries with backoff, then exits
7. server returns revocation notice → client falls back to .bak or lexicon
8. staged_learning.dat corrupt → sync worker prunes, typing unaffected
9. All 881 regression tests pass (run without any network access)
```

---

## IMPLEMENTATION DECISION TABLE

| Decision | Value | Frozen? |
| :--- | :--- | :---: |
| Global Contribution default | OFF | ✅ |
| Personal Learning default | ON | ✅ |
| Model Download default | OFF | ✅ |
| Rarity Gate R3 threshold | freq >= 5 | ✅ |
| K-threshold | 50 | ✅ |
| Distinct-day threshold | 7 | ✅ |
| Max tuples per batch | 20 | ✅ |
| Max queue age | 7 days | ✅ |
| .bak retention | 30 days | ✅ |
| Upload jitter | 0–4 hours random | ✅ |
| Human moderation for beta | Mandatory, all entries | ✅ |
| Auto-publication at K=50 | NEVER (removed) | ✅ |
| TSF DLL network imports | Zero | ✅ |
| user_dict.txt modification by global update | NEVER | ✅ |
| Offline typing support | Full, unconditional | ✅ |

---

## WHAT WILL NOT BE IMPLEMENTED IN STAGE 3

The following are explicitly out of scope and must not be implemented without a new approval:

| Item | Reason |
| :--- | :--- |
| Cloud Sync / user account backup | Stage 4 feature — requires separate design |
| Differential privacy noise injection | Not in v1.0 scope — future enhancement |
| Real-time model updates (push) | Architecture uses polling only |
| Mobile / cross-platform sync | Windows-only in Stage 3 |
| Analytics dashboard (internal) | Requires separate privacy review |
| Automated fast-track publication | Manual moderation mandatory for beta |
| K=200 fast-track override | Not approved — all go through K=50 + moderation |
| Server-side personalization | Architecture is local-first only |
| Any change to TSF typing path | Frozen |
| Any change to user_dict.txt format | Frozen (v1 schema, Stage 2 verified) |
