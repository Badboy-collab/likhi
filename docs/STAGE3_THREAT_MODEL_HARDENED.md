# LIKHI — STAGE 3 THREAT MODEL (HARDENED)
## Version 1.1.0 | Hardening Review Applied

**Document Version:** 1.1.0
**Status:** HARDENED DRAFT — Awaiting Implementation Approval
**Supersedes:** STAGE3_THREAT_MODEL.md v1.0.0
**Key Changes:**
- Added 12 new scenario-based threat assessments (A–L)
- Corrected Ed25519, SHA-256, TLS, cert-pinning scope statements
- Added "what this mechanism does NOT protect" for every security control
- Model rollback failure modes formalized
- Human moderation explicitly scoped as a security control

---

## PART 1 — SECURITY MECHANISM AUDIT

Each security mechanism is evaluated for what it solves and what it does NOT solve.
No mechanism is described as providing more protection than it actually delivers.

### 1.1 Ed25519 Cryptographic Signature on `global_model.bin`

**What it solves:**
- Proves that the model binary was produced by the legitimate Likhi build system.
- Detects bitflip corruption, partial download, and in-transit tampering.
- Prevents a fake `global_model.bin` placed by a malicious local process (without key access)
  from being loaded.

**What it does NOT solve:**
- Does NOT prove the model contains linguistically safe or appropriate content. A model
  signed by the real key can still contain bad phonetic mappings (whether by accident or
  due to bad data reaching the build). Signature = authenticity, NOT linguistic quality.
- Does NOT protect against signing-key compromise. If the private signing key is stolen,
  the attacker can produce arbitrary signed models. (See threat scenario E below.)
- Does NOT prevent the server operator from publishing a bad model intentionally.
- Does NOT protect against a malicious model that passes all structural checks but contains
  subtly manipulated frequency weights.

**Corrected implementation requirement:** Signature verification is a NECESSARY but
INSUFFICIENT gate. Human moderation (Stage B) must also pass before a model is signed and
published.

---

### 1.2 SHA-256 Integrity Verification

**What it solves:**
- Detects download truncation, partial writes, or CDN corruption of `global_model.bin.tmp`.
- Ensures the file on disk matches the file the server intended to distribute.

**What it does NOT solve:**
- Does NOT detect a correctly delivered but malicious model. SHA-256 matches a known-bad
  binary as well as a known-good one.
- Does NOT protect against man-in-the-middle attacks unless combined with TLS + certificate
  pinning (see §1.3).

---

### 1.3 TLS 1.3 + Certificate Pinning

**What it solves:**
- Encrypts the contribution payload in transit so a passive network observer cannot read
  the contents.
- Prevents a DNS-hijack or rogue CA from intercepting the HTTPS connection to
  `api.likhi.org`.
- Certificate pinning specifically prevents interception by corporate HTTPS inspection
  proxies or national-level MITM infrastructure.

**What it does NOT solve:**
- Does NOT protect against the server operator itself (Likhi) accessing the data.
- Does NOT prevent a contributor from sending malicious content within a valid TLS session.
- Certificate pinning failures will block all sync for that user until the pin is updated
  in the next app release. This creates an operational risk if the certificate changes
  unexpectedly. **Risk mitigation:** Pin the CA/intermediate cert, not the leaf cert, with
  a backup pin.
- Does NOT apply to the local file system. An attacker with local admin access can replace
  `global_model.bin` directly. (Mitigation: signature + hash verification on load.)

---

### 1.4 Proof-of-Work (PoW) Challenge

**What it solves:**
- Raises the computational cost for bots submitting fake contributions at high volume.
- Makes mass fake-account contribution attacks economically expensive.

**What it does NOT solve:**
- Does NOT prevent a patient attacker with significant compute from meeting the PoW threshold.
- Does NOT prevent a nation-state actor with GPU farms from submitting millions of valid PoW
  responses.
- Does NOT verify that the contributing client is a genuine Likhi install (only that the
  submitter solved the challenge).
- Is a deterrent and cost-raiser, not an absolute barrier.

**Implementation note:** PoW should be calibrated so that a typical contribution takes
< 200ms on client hardware. If PoW becomes too expensive, it punishes legitimate users
while motivated attackers still pay the cost.

---

### 1.5 Rate Limiting

**What it solves:**
- Prevents a single client from flooding the server with contributions that exceed the
  semantically meaningful rate (max 20 pairs per 24 hours per client session).
- Prevents repeated re-submission of the same tuple pair.

**What it does NOT solve:**
- Does NOT prevent a distributed botnet from meeting rate limits across many IPs.
- Per-client rate limiting without session IDs (which Likhi intentionally avoids for
  privacy) relies on IP subnet limiting + PoW — which can be worked around.
- The K=50 threshold and human moderation are the last meaningful defenses against
  coordinated attacks.

---

### 1.6 K=50 Statistical Aggregation Threshold

**What it solves:** (See also ARCHITECTURE_HARDENED.md §3.1)
- Prevents single-contributor influence on the public model.
- Prevents flash-mob coordinated poisoning by small groups.
- Prevents rare/personal words from reaching the model if they can't accumulate 50
  independent contributions.

**What it does NOT solve:**
- NOT a formal privacy guarantee (see architecture doc §3.2).
- Does NOT prevent large coordinated attacks by 50+ bots.
- Does NOT detect linguistically bad content submitted by 50+ real users in error.
  (Human moderation covers this.)

---

### 1.7 Global Model Rollback

**What it solves:**
- Allows recovery to a previous known-good state if a bad model passes all automated checks.
- Protects against corrupted download or partial atomic swap.

**What it does NOT solve:**
- Does NOT automatically detect that a new model is linguistically worse — only that it
  is structurally corrupt or fails signature/hash. A "bad" model that is validly signed
  and well-formed will NOT trigger automatic rollback. Human moderation prevents this
  scenario upstream.
- Does NOT apply retroactively if a bad model was active for multiple days before being
  detected. Typing from that period is already committed to the user's documents.
- The `.bak` file is only 1 generation deep. If both current and bak are compromised,
  the engine falls back to base lexicon.

---

## PART 2 — SCENARIO-BASED THREAT ASSESSMENTS (A–L)

### Scenario A — Rare Name Submission

**Threat:** A user types a rare Bangladeshi surname (e.g. `khondhaker → খোন্দকার`) and
this pair enters the global model, making it possible to statistically infer user behavior
for a small group of users who share that surname.

| Attribute | Details |
| :--- | :--- |
| **Likelihood** | Low (if Rarity Gate is correctly implemented) |
| **Impact** | Medium — Surname terms identify a user group but not an individual |
| **Mitigation** | Rule R1 (lexical membership gate) rejects words below rank 100,000. Rare surnames are unlikely to be in the common vocabulary lexicon. Human moderation (Stage B) reviews all words flagged as potential personal names. |
| **Residual Risk** | Common surnames that happen to be in the lexicon (e.g. `হোসেন`) may still pass the gate. At K=50, thousands of Bangladeshi users share this surname, so identification of an individual is not possible. Moderators review words near the category boundary. |
| **Status** | Acceptable with Rarity Gate + Moderation |

---

### Scenario B — Bot Frequency Inflation

**Threat:** An attacker operates a botnet that submits fake contributions to boost a word's
crowd count to K=50, causing a non-preferred or incorrect mapping to enter the global model.

| Attribute | Details |
| :--- | :--- |
| **Likelihood** | Medium (technically feasible for well-resourced attacker) |
| **Impact** | Medium — A specific word's ranking is altered globally. Doesn't affect any user's personal dictionary or pinned words. |
| **Mitigation** | Rate limiting (20/day per session) + PoW + IP subnet diversity check (no single /24 > 10% of votes) + 7-day span requirement + human moderation reviews atypical concentration patterns. |
| **Residual Risk** | A patient attacker distributing submissions across 50 distinct /24 subnets over 7+ days can theoretically pass automated checks. Human moderators are the final control. |
| **Status** | Acceptable with moderation; automated flags should highlight atypical distributions |

---

### Scenario C — Candidate Poisoning

**Threat:** A successful bot attack or compromised server publishes a model that promotes
an incorrect or offensive Bengali word as a top candidate for a common phonetic input.

| Attribute | Details |
| :--- | :--- |
| **Likelihood** | Low — Requires passing Stage A + human moderation + signing process |
| **Impact** | High — If an offensive word reaches Candidate #1 for a common input, it causes harm and requires urgent model rollback |
| **Mitigation** | Two-stage moderation (human review mandatory). The moderation checklist explicitly includes toxicity screening. A revocation mechanism in the server version endpoint triggers rollback in all clients. |
| **Recovery** | Model revocation → client rollback to `.bak` or base lexicon → revert within one sync cycle (≤ 24 hours after revocation is published). User's personal dictionary is unaffected. |
| **Residual Risk** | The window between a poisoned model being deployed and revocation being detected. Acceptable only if monitoring for user reports is in place. |
| **Status** | Acceptable with monitoring + fast revocation path |

---

### Scenario D — Server Compromise

**Threat:** The `api.likhi.org` server infrastructure is compromised by an external attacker.
The attacker can read all ingested contribution data and/or replace the model download endpoint.

| Attribute | Details |
| :--- | :--- |
| **Likelihood** | Low (standard server hardening required) |
| **Impact, Data Breach** | Low-Medium. Server stores only aggregated `(roman_key, selected_bengali, count, week_bucket)` tuples. No IP, no device IDs, no session IDs. Individual users cannot be identified from the server's stored data. |
| **Impact, Model Tampering** | High. Attacker could replace `global_model.bin` download with malicious content. Mitigation: the model is signed by a private key stored OFFLINE and not on the server. Server only serves the pre-signed binary. An attacker who compromises the server cannot forge a new signature. |
| **Residual Risk** | An attacker can serve a previously-valid (old/rolled-back) model or delete the model endpoint causing sync failures. Both cases result in clients falling back gracefully. |
| **Status** | Acceptable given offline signing key + client signature verification |

---

### Scenario E — Signing Key Compromise

**Threat:** The Ed25519 private key used to sign `global_model.bin` is stolen.

| Attribute | Details |
| :--- | :--- |
| **Likelihood** | Very Low if key is stored in HSM or air-gapped build environment |
| **Impact** | Critical — Attacker can sign arbitrary malicious models |
| **Mitigation** | Private key MUST be stored in an HSM or offline key vault. Key rotation procedure must be documented and tested. A new key rotation triggers a client update with the new pinned public key. Model version numbers must be monotonically increasing — an attacker cannot re-issue old version numbers. |
| **Detection** | Anomalous model version numbers, unexpected phonetic mapping changes reported by users |
| **Recovery** | Emergency app release with new pinned public key, revocation of old key, rollback of all clients to base lexicon. |
| **Residual Risk** | Until client update is deployed (hours to days), clients with the old pinned key will accept attacker-signed models. This is the highest single-point risk in the architecture. |
| **Status** | Requires HSM/air-gap key custody before production deployment |

---

### Scenario F — Malicious Model Download

**Threat:** A client downloads a model that passes SHA-256 and Ed25519 checks but contains
malicious phonetic mappings (e.g. promoting extremist terminology, politically charged words,
or sexual content as top candidates).

| Attribute | Details |
| :--- | :--- |
| **Likelihood** | Only possible if: (a) signing key compromised (Scenario E), or (b) Likhi operator intentionally builds malicious content |
| **Impact** | High if discovered by users — reputation damage, regulatory risk |
| **Mitigation** | Human moderation is the primary control against (b). Ed25519 signature is the primary control against (a). The engine cannot inject text into documents without user selection — typing is always interactive. |
| **Residual Risk** | An insider at the Likhi operation could publish offensive content that passes human review. No technical control fully prevents insider threats. Open-source model diff publishing allows community scrutiny. |
| **Status** | Acceptable with moderation + audit trail |

---

### Scenario G — User Revokes Consent

**Threat:** User turns off "Contribute Anonymous Suggestions" but some data has already been
queued in `staged_learning.dat` and not yet uploaded.

| Attribute | Details |
| :--- | :--- |
| **Likelihood** | Certain — This is a designed user action |
| **Impact** | Low if handled correctly |
| **Expected Behavior** | Staging stops immediately (in-process flag). `staged_learning.dat` is deleted by `likhi_sync.exe` on next wake (≤ 10 minutes). Upload is aborted if in flight. |
| **Failure Mode** | If `likhi_sync.exe` is not running, the deletion is deferred until the next time it wakes. Data remains on local disk in `staged_learning.dat` until then. |
| **Mitigation** | TSF DLL also checks the consent flag before each staging append. Even if `staged_learning.dat` isn't deleted immediately, no new data is added to it. |
| **Residual Risk** | Data in `staged_learning.dat` at time of revocation may exist on disk for up to 10 minutes. This is data the user chose to stage while consent was active. Acceptable. |
| **Status** | Acceptable with documented maximum residual window |

---

### Scenario H — Offline Operation

**Threat:** Likhi is used on a device with no internet connection. The global model is absent
or outdated. Sync operations fail repeatedly.

| Attribute | Details |
| :--- | :--- |
| **Likelihood** | Certain — Offline users are a primary use case |
| **Expected Behavior** | Engine loads `lexicon.bin` + `user_dict.txt`. `global_model.bin` is optional. Typing works normally. `likhi_sync.exe` checks network availability before attempting connections — uses WinINet `InternetGetConnectedState` or equivalent. Fails silently with local log entry. |
| **Staged Queue Behavior** | `staged_learning.dat` accumulates entries locally. Uploaded on next successful sync. Maximum queue age is 7 days — older entries are discarded silently. |
| **Residual Risk** | Up to 7 days of contribution data is lost if sync never succeeds. Acceptable — this data is a best-effort contribution, not a critical user record. |
| **Status** | Fully acceptable — designed as primary use case |

---

### Scenario I — Corrupted Model

**Threat:** `global_model.bin` is corrupted (by disk error, AV quarantine, bitflip, or
partial crash during atomic swap).

| Attribute | Details |
| :--- | :--- |
| **Likelihood** | Low but non-zero |
| **Detection** | SHA-256 hash verification and magic byte check on every load. |
| **Recovery** | Automatic: engine falls back to `global_model.bin.bak`. If .bak also fails: falls back to `lexicon.bin` + `user_dict.txt` only. Typing never stops. |
| **User Impact** | Temporary loss of crowd-ranking improvements only. Personal and base lexicon candidates unaffected. |
| **Status** | Fully handled by rollback chain |

---

### Scenario J — Corrupted Staging Queue

**Threat:** `staged_learning.dat` is corrupted (by power loss, AV interference, or disk error).

| Attribute | Details |
| :--- | :--- |
| **Likelihood** | Low but non-zero |
| **Impact** | Low — the file contains contribution data, not personal data critical to user's typing. |
| **Detection** | CRC32 per-record validation on read. Invalid records are silently skipped. |
| **Recovery** | If >50% of records are invalid, `likhi_sync.exe` deletes `staged_learning.dat` and starts a fresh queue. No crash, no user notification required. |
| **Typing Impact** | None — the staging file is entirely decoupled from the typing path. |
| **Status** | Fully handled |

---

### Scenario K — Malicious Client

**Threat:** A modified version of `bangla_tsf.dll` or `likhi_sync.exe` is installed by an
attacker (e.g. by replacing the binaries in `%LOCALAPPDATA%\Programs\Likhi\`). The modified
binary exfiltrates `user_dict.txt` or captures keystrokes.

| Attribute | Details |
| :--- | :--- |
| **Likelihood** | Low — Requires either admin access or user being tricked into running a malicious updater |
| **Impact** | High — A modified TSF DLL loaded into Word or Chrome has significant attack surface |
| **Mitigation** | Installer signs all binaries with Authenticode. Windows verifies Authenticode signature before loading. Users are warned by SmartScreen if the signature is absent or invalid. Update mechanism (not yet designed) must validate Authenticode before replacing binaries. |
| **Residual Risk** | A stolen/leaked Authenticode certificate enables this attack. Key custody of code signing certificate is critical. |
| **Status** | Requires Authenticode signing + certificate custody procedures |

---

### Scenario L — Data Correlation Attack

**Threat:** The server operator (or a government with server access) attempts to de-anonymize
contributions by correlating `(week_bucket, client_version, roman_key, bengali_word)` tuples
to identify specific users or organizations.

| Attribute | Details |
| :--- | :--- |
| **Likelihood** | Medium (technically feasible, requires motivated server operator or compelled access) |
| **Impact** | Medium — Most contribution tuples are common words indistinguishable among thousands of users. Rare words (if any pass the gate) could narrow the population. |
| **Mitigation** | (1) Rarity Gate eliminates rare/personal vocabulary before upload. (2) No IP, no device ID, no session token — no direct identifier in the payload. (3) `week_bucket` is coarse (one week = ~5 million typing events for a busy user). (4) `client_version` shared by all users on the same release. (5) IP subnet diversity requirement fragments the apparent submitter pool. |
| **What Correlation Could Still Yield** | With traffic analysis + timing: the server could potentially estimate that a high-volume submitter uses a specific ISP or submits at specific times of day. This inference is possible even without IP storage. |
| **Residual Risk** | Batch upload randomization (random delay 0–4 hours before sync, not immediate) reduces timing-based correlation. This should be implemented in `likhi_sync.exe`. |
| **Status** | Acceptable with batch upload randomization added to implementation |

---

## PART 3 — STRIDE THREAT MATRIX SUMMARY

| STRIDE Category | Primary Controls | Gaps / Residual Risks |
| :--- | :--- | :--- |
| **Spoofing** | TLS + cert pinning, Authenticode binary signing | Stolen signing certificates |
| **Tampering** | SHA-256 + Ed25519 on model, atomic file writes, CRC32 on staging queue | Insider threat to model build |
| **Repudiation** | Human moderation audit log, model version monotonicity | No user-level contribution audit log |
| **Information Disclosure** | Rarity Gate, IP stripping, no session IDs, no timestamps | Residual linkability for rare-word submitters; timing correlation |
| **Denial of Service** | Rate limiting, PoW, offline fallback | Well-resourced DDoS; PoW calibration risk |
| **Elevation of Privilege** | Per-user install (HKCU), zero network in TSF DLL, no admin required for typing | Malicious local process replacing binaries (needs admin or user error) |

---

## PART 4 — SECURITY REVIEW CHECKLIST

The following items must all be confirmed before any Stage 3 production deployment:

| # | Item | Status |
| :-- | :--- | :---: |
| S1 | Ed25519 private key stored in HSM or air-gapped system | TO VERIFY |
| S2 | Model release signed in isolated build pipeline, not production server | TO IMPLEMENT |
| S3 | Authenticode code signing configured for all Likhi binaries | TO IMPLEMENT |
| S4 | Server-side IP stripping verified in nginx/Envoy config | TO IMPLEMENT |
| S5 | Rarity Gate unit tests cover all 4 rules | TO IMPLEMENT |
| S6 | Model rollback chain tested: corrupt → bak → lexicon fallback | TO IMPLEMENT |
| S7 | Revocation mechanism tested: server marks version revoked → client deletes + falls back | TO IMPLEMENT |
| S8 | Staging queue CRC32 corruption test: corrupt records skipped, queue re-initialized | TO IMPLEMENT |
| S9 | Batch upload delay randomized (0–4 hour jitter) | TO IMPLEMENT |
| S10 | Human moderation queue tooling designed before any model is published | TO IMPLEMENT |
| S11 | Model version is monotonically increasing and verified on load | TO IMPLEMENT |
| S12 | `likhi_sync.exe` import table contains no network libs if consent is off | TO VERIFY |
