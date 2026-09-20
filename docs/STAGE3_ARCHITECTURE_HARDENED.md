# LIKHI — STAGE 3 ARCHITECTURE SPECIFICATION (HARDENED)
## Version 1.1.0 | Hardening Review Applied

**Document Version:** 1.1.0
**Status:** HARDENED DRAFT — Awaiting Implementation Approval
**Supersedes:** STAGE3_ARCHITECTURE.md v1.0.0
**Change Summary:** Privacy audit applied; "anonymous" language removed or qualified throughout;
two-stage moderation pipeline added; IPC boundary formalized; rollback chain defined;
consent revocation behavior made precise.

---

## PART 1 — PRIVACY CLAIM AUDIT

### 1.1 Use of the Word "Anonymous"

The word **"anonymous"** appeared 11 times in v1.0.0 documents. Every use has been audited.

**Finding:** "Anonymous" was used to describe the contribution system without formally
justifying the claim. The absence of a persistent user ID alone does not make data
anonymous. The correct term for what Likhi achieves is **"pseudonymous at best"** and
**"non-identifying by design"** when describing public-model contributions.

**New Language Policy:** The documents below use **"non-identifying contribution"** or
**"de-identified batch"** instead of "anonymous." No document claims full anonymity without
a formal justification.

---

### 1.2 Field-by-Field Privacy Classification

Every field that could leave the device is classified below.

| Field | Classification | Reason | Contribution Pipeline Inclusion? |
| :--- | :---: | :--- | :---: |
| `roman_key` (e.g. `"porishkar"`) | **Potentially Identifying** | Common words: benign. Rare/personal words: could identify a person, place, or institution. Regex `^[a-z]{1,24}$` filters obvious PII but does not eliminate all identifying potential. | Only if word passes rarity filter. See §2.3. |
| `selected_bengali_word` (e.g. `পরিষ্কার`) | **Potentially Identifying** | Standard lexicon words: benign. Rare custom words, personal names in Bangla script, or unique abbreviations: potentially identifying. | Only if word passes lexical membership + rarity filter. |
| `vote_weight` (`1`, clamped) | **Non-Sensitive** | Fixed integer. Conveys no user identity. | Yes — safe. |
| `week_bucket` (e.g. `"2026-W37"`) | **Operational Metadata** | Prevents replay attacks and temporal aggregation fraud. Not tied to user identity at week granularity. | Yes — safe at weekly granularity. |
| IP Address / Network Metadata | **Potentially Sensitive** | Can identify ISP, city, organization, and sometimes individual households. Must not be persisted or joined with learning data. | Never in payload. Rate-limit only. See §1.3. |
| `client_version` (e.g. `"1.0.0"`) | **Operational Metadata** | Version string identifies software release only. Shared by thousands of users. | Yes — safe, coarse. |
| `lexicon_version` (e.g. `"2026.09.01"`) | **Operational Metadata** | Identifies vocabulary release. Shared by all users with same installer. | Yes — safe. |
| `global_model_version` (e.g. `10001`) | **Operational Metadata** | Identifies which model the client operates on. Shared by all users on same model. | Yes — safe. |
| Typing timestamps (millisecond precision) | **Potentially Sensitive** | High-precision typing timing can be used for keystroke biometric fingerprinting. | **NEVER included.** |
| Application context / window title | **Sensitive** | Directly reveals what the user is working on. | **NEVER collected.** |
| Unselected candidates | **Potentially Sensitive** | Shows what alternatives were offered and rejected — reveals preference context. | **NEVER included.** |
| Session identifiers, hardware IDs | **Sensitive** | Direct or indirect user identification. | **NEVER generated or collected.** |

**Conclusion of Audit:** The contribution system transmits a small number of *potentially
identifying* fields (`roman_key`, `selected_bengali`). These require additional protection
beyond regex filtering. See §2 below for the Rarity Gate and §3 for the corrected K-threshold
statement.

---

### 1.3 IP Address Policy (Precise Definition)

This is the exact and binding IP address policy for all Stage 3 infrastructure:

| Question | Answer |
| :--- | :--- |
| Is IP stored in the application database? | **NO.** The ingestion API endpoint never writes the source IP to the contribution record. |
| Is IP persisted in logs? | **NO.** Access logs are stripped of source IPs before writing. Nginx/Envoy configured with `access_log off` or a custom log format that omits `$remote_addr`. |
| Is IP hashed before storage? | **NO.** Hashing would still allow brute-force de-anonymization on the /24 subnet space (IPv4 = ~16 million prefixes). Not collected at all. |
| Is IP retained transiently? | **YES, for TCP session duration only.** The operating system TCP stack holds the source IP for the life of the HTTP/2 connection. It is discarded immediately upon socket closure. |
| Is IP used for rate limiting? | **YES, transiently.** The edge proxy (Cloudflare Worker / Envoy) inspects the source IP in memory to apply rate-limit counters. These counters are stored as `IP -> request_count` in ephemeral Redis with a 60-second TTL. They are never joined with learning data. |
| Exact retention period | **TCP connection duration only (≤ 30 seconds for connection keepalive).** After TCP teardown, the IP is irrecoverable. |
| Can IP ever be linked to learning records? | **NO.** The edge proxy receives the IP-bearing request and strips it before forwarding the payload to the ingestion service. The ingestion service never sees the source IP. |
| Is IP included in analytics? | **NO.** No IP-based analytics, geographic reporting, or subnet heatmaps are generated. |

---

## PART 2 — PERSONAL AND RARE WORD PROTECTION

### 2.1 Limitation of Current Regex Gate

The v1.0.0 architecture documented the following client-side filter:

```
^[a-z]{1,24}$
```

**This is a necessary but insufficient protection.** The regex prevents digits, symbols,
and URL-like strings, but does not detect:

- Rare surnames (e.g. `khondokar`, `biswas`, `habibullah`)
- Organization names (e.g. `grameen`, `brac`, `bashundhara`)
- Personal name spellings typed phonetically (e.g. `pervez`, `sumaiya`, `jahangir`)
- Place names unique to a locality (e.g. `narsingdi`, `bhairab`, `netrokona`)
- Usernames typed by habit (e.g. `mdsohel`, `bdphotog`)
- Custom shortcuts unique to a single user (e.g. `myaddr`, `myfirm`)

A determined attacker could submit 49 fake contributions to just barely fail the K=50
threshold, protecting a single real submission as a de-facto singleton.

### 2.2 Rarity Gate (New Protection Layer)

Before staging any tuple for contribution, the client applies a **Rarity Gate**:

**Rule 1 — Lexical Membership Gate:**
The `selected_bengali_word` MUST appear in the base `lexicon.bin` with a frequency rank
above a defined threshold (e.g. `unigram_rank <= 100,000`).
- Words absent from the lexicon: **rejected from contribution.** They may be personal custom
  words or rare terms inappropriate for a shared public model.
- Words present in the lexicon: eligible to proceed.

**Rule 2 — Roman Key Frequency Gate:**
The `roman_key` MUST appear in a curated list of known common Bangla phonetic roots
(derived from the base lexicon's indexed roman keys). Any roman key that does not map to
any word in `lexicon.bin` is **rejected from contribution.**

**Rule 3 — Local Selection Floor:**
A candidate pair must have been selected by this user at least **5 times locally** (not 3
as in personal learning ranking) before it is eligible for contribution to the staging queue.
This eliminates accidental or one-off selections of uncommon words.

**Rule 4 — Personal Override Exclusion:**
If a word is stored in the user's personal dictionary with `FLAG_CONTRIBUTION_EXCLUDED` or
`FLAG_MANUAL_CUSTOM`, it is permanently excluded from all contribution staging.

### 2.3 Can Global Learning Improve Common Transliteration Without Publishing Private Words?

**Yes, explicitly.** The system is designed so that:

1. The global model is seeded from the curated `lexicon.bin` vocabulary — it can only
   improve rankings of words that already exist in the base lexicon.
2. The K=50 consensus threshold (see §3) combined with the 7-day span means that only words
   typed by 50+ independent users across multiple days can enter the global model.
3. Truly rare or personal vocabulary (surnames, organization names, local place names not
   in the lexicon) cannot pass the Lexical Membership Gate at the client and will never
   appear in the contribution pipeline at all.

**Rare words that should eventually be added to Likhi's vocabulary go through a separate
editorial channel** (lexicon curation), not the crowdsourced learning pipeline.

---

## PART 3 — K-ANONYMITY CLAIM CORRECTION

### 3.1 What K=50 and 7-Day Span Actually Protect Against

The v1.0.0 document used the phrase "k-Anonymity Statistical Consensus Gate" in a way that
implied privacy protection. This is **corrected here.**

K=50 / 7-day is an **abuse and aggregation threshold**, not a formal privacy guarantee.

**What K=50 and 7-day span protect against:**

| Protection | Explanation |
| :--- | :--- |
| **Frequency Inflation Attacks** | A single bad actor cannot boost a word from rank 2 to rank 1 by submitting repeated votes. |
| **Flash-Mob Coordinated Poisoning** | A group of 49 coordinated bots cannot inject a word if they all submit within a few hours — the 7-day span requirement disperses concentrated attacks. |
| **Singleton Rare-Word Publication** | A word typed by only one or two people will never accumulate 50 independent submissions and thus will never appear in the global model. |
| **New-Word Auto-Publication** | No word automatically appears in a signed model without passing both the statistical threshold AND human moderation (§8 below). |

**What K=50 and 7-day span do NOT protect against:**

| Non-Protection | Explanation |
| :--- | :--- |
| **Formal k-Anonymity Privacy** | Real k-anonymity requires that each record be indistinguishable from k-1 other records across all quasi-identifier attributes. A word pair (roman, bengali) may be unique enough that 50 submissions still identify a small user group. |
| **Sophisticated Sybil Attacks** | A well-funded attacker controlling 50 distinct IP subnets and devices can meet both thresholds. This is mitigated by IP subnet diversity checks but not eliminated. |
| **Server-Side Statistical Correlation** | The server operator could theoretically correlate submission timestamps, IP subnet clusters, and week buckets to narrow down contribution origins. Mitigation: IP is never stored; week bucket is maximum granularity. |
| **Linguistic Fingerprinting** | The combination of unusual roman key + unusual Bengali word + rare client version could be a near-unique fingerprint for a small user group. Mitigation: Lexical Membership Gate eliminates rare words before they reach the server. |

**Corrected Language:** All Stage 3 documents now use the phrase
**"statistical aggregation threshold"** rather than "k-Anonymity guarantee."

---

## PART 4 — TWO-STAGE MODERATION PIPELINE

The v1.0.0 architecture allowed automatic publication after K=50 was reached. This is
**removed.** The corrected pipeline is:

### Stage A — Automated Statistical Validation
Performed by the server without human involvement:

| Check | Criteria | Fail Action |
| :--- | :--- | :--- |
| Submission count | `count >= 50` distinct sessions | Remains in pending buffer. |
| Time span | `>= 7 distinct calendar days` | Remains in pending buffer. |
| IP subnet diversity | No single `/24` provides >10% of votes | Flagged for anti-abuse review. |
| Bengali Unicode validity | Full `IsValidBengaliSequence` check | Discarded immediately. |
| Phonetic alignment | Server-side phonetic alignment score `>= 0.45` | Discarded. |
| Length constraints | roman key 1–24 chars, Bengali word 1–40 codepoints | Discarded. |
| Toxicity filter | Against curated blocklist + ML classifier | Rejected, logged for security team. |
| Lexical membership | Bengali word must appear in current `lexicon.bin` | Rejected. |
| Duplicate detection | Normalized Levenshtein distance `>= 3` from existing entries | Flagged for merge review. |

### Stage B — Human Moderation Queue
Every candidate that passes Stage A enters a moderation queue reviewed by trained editors.
Moderators apply the following criteria:

- **Spelling validity:** Is this the accepted standard spelling?
- **Bangladesh relevance:** Is this spelling common in Bangladesh Bengali (not primarily West Bengal)?
- **Personal name detection:** Does this appear to be a surname, given name, or organization name?
- **Suspicious pattern review:** Does the submission cluster suggest coordinated injection?
- **Phonetic naturalness:** Does the phonetic mapping make intuitive sense?
- **Frequency plausibility:** Is the crowd count plausible for a word of this type?

**Decision outcomes:**
- ✅ **Approved:** Enters the next signed model build.
- ❌ **Rejected:** Permanently discarded with reason code logged.
- 🔁 **Deferred:** Returned to pending with elevated K threshold (e.g. K=200).

**For the initial beta release, ALL entries require human approval.** Partially-automated
fast-track (for common words with overwhelming evidence) will be considered only after
reviewing moderation patterns from real-world data.

---

## PART 5 — GLOBAL MODEL PRECEDENCE (FROZEN)

This precedence order is **frozen** and must not be altered without a documented
architecture review:

```
[1] Personal Pinned Word (FLAG_PINNED in user_dict.txt)
    |
[2] Personal Learned Word (frequency >= 3 in user_dict.txt)
    |
[3] Curated Base Override (kLexiconOverrideFlag in lexicon.bin)
    |
[4] Global Crowdsourced Model (global_model.bin)
    |
[5] Base Lexicon (lexicon.bin unigram frequency)
    |
[6] Phonetic Parser Output (pure rule-based transliteration)
```

**Enforcement guarantees:**

- `global_model.bin` updates: MUST NOT alter, truncate, or overwrite `user_dict.txt`.
- `global_model.bin` rollback: MUST NOT alter `user_dict.txt`.
- `lexicon.bin` updates: MUST NOT alter `user_dict.txt`.
- Application upgrade: MUST NOT alter `user_dict.txt`.
- Uninstall: MUST NOT delete `user_dict.txt` (verified in Stage 2, preserved here).

**Implementation enforcement:** The sync worker (`likhi_sync.exe`) and the installer script
are prohibited at the code level from opening a write handle to any path under
`%APPDATA%\PC-Bangla-Typing-App\`.

---

## PART 6 — NETWORK ISOLATION (PRECISE IPC BOUNDARY)

### 6.1 TSF Typing Path (Zero Network Tolerance)

`bangla_tsf.dll` MUST satisfy all of the following at build time:

- **Link check:** Binary must not statically link `ws2_32.dll`, `wininet.dll`, `winhttp.dll`,
  `urlmon.dll`, `dnsapi.dll`, or any network-capable DLL.
- **Import table audit:** The DLL import table must contain only:
  `kernel32.dll`, `user32.dll`, `advapi32.dll`, `ole32.dll`, `msctf.dll`, and
  optionally `ntdll.dll`.
- **No blocking calls:** No I/O call inside the TSF key event path may take longer than
  2 ms. Dictionary reads use memory-mapped files only.

The keystroke processing thread MUST NEVER:
- Perform DNS resolution
- Open a socket
- Call `WaitForSingleObject` on a network event
- Call any function that could block on network availability

### 6.2 IPC Boundary Between TSF and Sync Worker

The TSF DLL communicates with the sync worker using only one mechanism:

**Local filesystem staging file** (`staged_learning.dat`):
- TSF appends a binary record to `staged_learning.dat` using `CreateFile` with `FILE_APPEND_DATA` and `FILE_SHARE_READ | FILE_SHARE_WRITE`.
- This is a fire-and-forget file append: TSF never waits for the sync worker to acknowledge.
- If the file is absent or locked, TSF silently skips the append and continues normally.
- No named pipes, no COM out-of-process calls, no window messages, no shared memory.

**Why filesystem staging?**
- Zero coupling between typing performance and network activity.
- If `likhi_sync.exe` crashes, hangs, or is absent, typing is completely unaffected.
- The staging file can be inspected, deleted, or replaced by the user at any time.

---

## PART 7 — MODEL ROLLBACK CHAIN

The engine maintains a three-state model chain:

```
[State 1] global_model.bin.bak   — Previous known-good model (kept 30 days)
[State 2] global_model.bin       — Current active model (loaded read-only)
[State 3] global_model.bin.tmp   — Download-in-progress (never loaded directly)
```

**Atomic Swap Sequence:**

```
1. Download to global_model.bin.tmp
2. Verify SHA-256 of .tmp file
3. Verify Ed25519 signature of .tmp file
4. Verify header magic bytes ('LGM1')
5. Verify global_model_version > current version
6. Verify min_client_version <= installed client version
7. IF all pass:
     a. Rename current global_model.bin -> global_model.bin.bak
     b. MoveFileExW(.tmp -> global_model.bin, REPLACE_EXISTING | WRITE_THROUGH)
8. IF any check fails:
     a. Delete global_model.bin.tmp immediately
     b. Keep existing global_model.bin unchanged
     c. Log failure reason to local debug log
```

**Fallback Priority on Load Failure:**

```
Failed to load global_model.bin
    → Try global_model.bin.bak
    → If .bak also fails or absent: operate on lexicon.bin + user_dict.txt only
    → Typing continues normally in all cases
```

**Revocation Handling:**

If the server marks a model version as revoked in `/api/v1/model/version`:
- The sync worker deletes both `global_model.bin` and `global_model.bin.bak`
- Next typing session loads `lexicon.bin` + `user_dict.txt` only
- The sync worker then downloads the next safe model version

---

## PART 8 — CONSENT BEHAVIOR (PRECISE DEFINITION)

### 8.1 Settings State Machine

Four distinct settings with independent state:

```
SETTING: Personal Learning
  Default: ON
  Effect ON:  user_dict.txt is updated on every candidate commit.
  Effect OFF: user_dict.txt is NOT updated. Existing data is preserved (not deleted).
  Revoke:     Stops future writes. Does NOT delete user_dict.txt.
              User must separately use "Clear All Learned Data" to delete.

SETTING: Download Global Improvements
  Default: OFF
  Effect ON:  likhi_sync.exe wakes periodically, checks /api/v1/model/version,
              downloads and verifies new models.
  Effect OFF: likhi_sync.exe NEVER contacts api.likhi.org for model downloads.
  Revoke:     Stops future downloads. Does NOT roll back current global_model.bin.
              User may manually delete global_model.bin if desired.

SETTING: Contribute Anonymous Suggestions
  Default: OFF (MUST remain OFF by default in all future releases)
  Effect ON:  TSF appends de-identified tuples to staged_learning.dat.
              likhi_sync.exe uploads staged_learning.dat to /api/v1/contribute.
  Effect OFF: TSF does NOT append to staged_learning.dat.
              likhi_sync.exe does NOT upload anything.
  REVOKE ACTION (immediate, synchronous):
    1. TSF stops appending to staged_learning.dat immediately (in-memory flag cleared).
    2. likhi_sync.exe receives OS signal or checks consent flag on next wake cycle.
    3. likhi_sync.exe DELETES staged_learning.dat entirely (zero bytes remaining).
    4. Any in-flight HTTP POST is aborted.
    5. Personal Learning (user_dict.txt) is UNAFFECTED.
    6. global_model.bin is UNAFFECTED.

SETTING: Cloud Sync (Stage 4, not implemented in Stage 3)
  Default: OFF
  Not designed or implemented in Stage 3.
```

### 8.2 Explicit Distinction: Personal Learning vs. Global Contribution

| Aspect | Personal Learning | Global Contribution |
| :--- | :--- | :--- |
| Default | **ON** | **OFF** |
| Data storage | Local disk only (`user_dict.txt`) | Staged locally, uploaded to server |
| Data travels off-device? | **Never** | Only when explicitly opted in |
| Who benefits? | Only the current user | All users globally |
| Revoke removes local data? | No (preserved for user) | Staging queue deleted immediately |
| Revoke removes personal ranking? | No | No |
| Independent control? | Yes | Yes |

These two settings are **logically and technically independent.** Revoking Global Contribution
does not disable, degrade, or affect Personal Learning in any way.

---

## PART 9 — VERIFICATION COVENANTS (UNCHANGED FROM STAGE 2)

1. **Zero Stage 1/2 Regressions:** All 881 automated test cases must continue to pass
   without any modification.
2. **Zero In-Process Network Code:** Verified at link time by checking the DLL import table.
3. **Atomic Personal Dictionary Writes:** Verified by `test_interrupted_write` test suite.
4. **Cold-Start Resilience:** Engine boots normally if `global_model.bin` is absent.
5. **User Data Preservation:** `user_dict.txt` MUST NOT be deleted or modified by:
   - Model download
   - Model rollback
   - Lexicon update
   - Application upgrade
   - Application uninstallation
