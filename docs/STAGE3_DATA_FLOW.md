# LIKHI — STAGE 3 DATA FLOW DIAGRAM
## Version 1.0.0 | Hardened Architecture

**Document Version:** 1.0.0
**Status:** HARDENED DRAFT — Awaiting Implementation Approval
**Purpose:** End-to-end data flow map showing the location and state of every piece of
data at every stage of the Likhi system lifecycle.

---

## LEGEND

Each data item is tagged with one of the following states:

| Tag | Meaning |
| :---: | :--- |
| 🖥️ LOCAL | Exists only on the user's device. Never leaves the device. |
| 💨 TRANSIENT | Exists only in memory or for a bounded short duration (TCP session, OS buffer). Irrecoverable after the session ends. |
| ⬆️ UPLOADED | Sent to the Likhi server. Exists on both client and server temporarily. |
| 📊 AGGREGATED | Merged into a statistical aggregate on the server. Individual record no longer separable. |
| 📦 PUBLISHED | Incorporated into the signed public global model. Distributed to all clients. |
| 🚫 NEVER | This data never exists in the system at any point. |

---

## DATA FLOW MAP

### ZONE 1 — USER KEYSTROKE (Inside Host Application)

```
User presses keys in Microsoft Word / Notepad / Chrome
│
├── Raw keystroke events
│   State: 💨 TRANSIENT
│   Location: Windows Input Stack → ITfTextInputProcessor::OnKeyDown
│   Fate: Processed in memory by bangla_tsf.dll. Not stored. Not logged.
│
├── Current composition buffer (e.g. "porishkar")
│   State: 💨 TRANSIENT
│   Location: bangla_tsf.dll internal buffer (heap)
│   Fate: Cleared on commit or cancel. Not persisted. Not logged.
│
├── Inter-keystroke timing intervals
│   State: 🚫 NEVER
│   bangla_tsf.dll does not read QueryPerformanceCounter for timing.
│   No biometric keystroke data is ever captured.
│
└── Window title / application name / document content
    State: 🚫 NEVER
    bangla_tsf.dll does not call GetWindowText, GetForegroundWindow for context,
    or access clipboard. No document context is read.
```

---

### ZONE 2 — CANDIDATE GENERATION (Inside bangla_tsf.dll)

```
BanglaEngine::GetCandidates("porishkar")
│
├── lexicon.bin (Read-Only)
│   State: 🖥️ LOCAL
│   Location: %LOCALAPPDATA%\Programs\Likhi\data\lexicon.bin
│   Access: Memory-mapped, read-only (MapViewOfFile with PAGE_READONLY)
│   Leaves device: NEVER
│
├── global_model.bin (Read-Only)
│   State: 🖥️ LOCAL
│   Location: %LOCALAPPDATA%\Programs\Likhi\data\global_model.bin
│   Access: Memory-mapped, read-only
│   Leaves device: NEVER
│   Updated by: likhi_sync.exe (atomic swap only, verified before use)
│
├── user_dict.txt (Read/Write)
│   State: 🖥️ LOCAL
│   Location: %APPDATA%\PC-Bangla-Typing-App\user_dict.txt
│   Contents: roman_key, bengali_word, frequency, last_used_timestamp
│   Leaves device: NEVER
│
└── Candidate list presented to user (in-memory)
    State: 💨 TRANSIENT
    Candidate window rendered → user picks → window dismissed
    The list of what was shown (and what was rejected) is NEVER stored
    or transmitted anywhere.
```

---

### ZONE 3 — CANDIDATE COMMIT (Word Selected by User)

```
User selects candidate "পরিষ্কার" for input "porishkar"
│
├── Committed Bengali text
│   State: 💨 TRANSIENT → inserted into host application document
│   Location: Host application's text buffer (Word, Notepad, Chrome)
│   Likhi has no access to this after commit. No logging.
│
├── Personal Dictionary Update
│   State: 🖥️ LOCAL
│   Written to: user_dict.txt (atomic write via .tmp + MoveFileExW)
│   Entry: "porishkar\tপরিষ্কার\t<freq+1>\t<timestamp>"
│   Leaves device: NEVER
│   Updated: Unconditionally on every commit (Personal Learning ON)
│
└── Contribution Staging (ONLY if all conditions met)
    ┌─ Condition 1: "Contribute Anonymous Suggestions" = ON (default: OFF)
    ├─ Condition 2: Word passes Rarity Gate (all 4 rules)
    │     R1: পরিষ্কার in lexicon.bin with rank ≤ 100,000
    │     R2: "porishkar" maps to lexicon entry
    │     R3: local frequency for this pair ≥ 5
    │     R4: not marked as manual custom / excluded
    ├─ Condition 3: Contribution queue not full (< max daily limit)
    └─ IF all conditions met:
         State: 🖥️ LOCAL (staged, not yet uploaded)
         Written to: staged_learning.dat (binary append, CRC32 per record)
         Contents: (roman_key="porishkar", selected_bengali="পরিষ্কার",
                    vote_weight=1, week_bucket="2026-W37")
         NOT included: timestamp, frequency, personal_score, rejected candidates,
                       device_id, session_id, IP, user_account, window_title
```

---

### ZONE 4 — STAGING QUEUE LIFECYCLE

```
staged_learning.dat
│
├── State: 🖥️ LOCAL
│   Location: %APPDATA%\PC-Bangla-Typing-App\staged_learning.dat
│   Only readable by current Windows user account (NTFS ACL)
│   Visible to user — deleteable at any time
│
├── Accumulation phase (between sync cycles)
│   TSF appends records continuously while consent is ON and Rarity Gate passes
│   Maximum age per record: 7 days (older records discarded by sync worker)
│   Maximum batch size: 20 pairs per 24 hours (clamped server-side too)
│
├── IF consent revoked:
│   State: 🖥️ LOCAL → 🚫 DELETED
│   TSF stops appending immediately (in-memory flag)
│   likhi_sync.exe deletes staged_learning.dat on next wake (≤ 10 minutes)
│   Data is permanently unrecoverable after deletion
│
└── IF consent active and sync conditions met:
    → Proceeds to ZONE 5 (Upload)
```

---

### ZONE 5 — SYNC WORKER UPLOAD (likhi_sync.exe)

```
likhi_sync.exe wakes (scheduled, idle, or manually triggered)
│
├── Consent check: reads consent flag from HKCU registry
│   IF consent OFF → sleep, do nothing → EXIT
│
├── Network check: InternetGetConnectedState or equivalent
│   IF no network → sleep, retry later → EXIT
│
├── Batch preparation:
│   Reads staged_learning.dat
│   Validates CRC32 per record (corrupt records skipped)
│   Selects records not older than 7 days
│   Groups into batch of ≤ 20 pairs
│
├── Upload delay: random jitter 0–4 hours before sending
│   (Prevents timing-based correlation attacks)
│
├── PoW challenge/response:
│   GET /api/v1/challenge → receives nonce + difficulty
│   Computes: find X such that SHA-256(nonce || X) has D leading zero bits
│   Time: < 200ms on typical hardware
│
├── HTTP POST /api/v1/contribute
│   Connection: TLS 1.3, certificate-pinned to api.likhi.org
│   Payload: JSON array of (roman_key, selected_bengali, vote_weight, week_bucket)
│            + client_version, lexicon_version, global_model_version, pow_response
│   IP address: TCP layer only → NEVER included in payload
│                             → NEVER stored by server
│
│   [DATA STATE ON TRANSMISSION]
│   roman_key:           ⬆️ UPLOADED (temporarily, then aggregated)
│   selected_bengali:    ⬆️ UPLOADED (temporarily, then aggregated)
│   vote_weight:         ⬆️ UPLOADED
│   week_bucket:         ⬆️ UPLOADED
│   client_version:      ⬆️ UPLOADED
│   IP address:          💨 TRANSIENT (TCP only, never written to server storage)
│   timestamps:          🚫 NEVER
│   session/device IDs:  🚫 NEVER
│   rejected candidates: 🚫 NEVER
│   full sentences:      🚫 NEVER
│
└── On success: staged_learning.dat is flushed (records cleared or file deleted)
```

---

### ZONE 6 — SERVER-SIDE INGESTION + AGGREGATION

```
Server receives POST /api/v1/contribute
│
├── IP address:
│   State: 💨 TRANSIENT
│   Used by edge proxy for rate limiting (in-memory counter, 60s TTL)
│   NEVER written to application database
│   NEVER forwarded to ingestion service
│   Discarded on TCP connection close
│
├── Raw contribution records (pre-aggregation):
│   State: ⬆️ UPLOADED → 📊 AGGREGATED
│   Stored temporarily in ingestion buffer
│   Aggregated into per-(roman_key, selected_bengali) frequency counts
│   Individual records deleted within 48 hours of aggregation
│
├── Aggregation store (pending threshold):
│   State: 📊 AGGREGATED
│   Contents: (roman_key, selected_bengali, crowd_count, distinct_days, subnet_diversity)
│   Individual user contributions are IRRECOVERABLE from this aggregate
│   Retained for up to 90 days (pairs that never reach K=50 are purged at 90 days)
│
├── Stage A automated validation:
│   crowd_count >= 50 + distinct_days >= 7 + subnet_diversity OK + unicode valid +
│   phonetic alignment + length + toxicity filter + lexical membership
│   IF pass → enters human moderation queue
│   IF fail → stays in pending buffer or discarded
│
└── Stage B human moderation:
    IF approved → enters next model build (see ZONE 7)
    IF rejected → permanently discarded with reason code
    IF deferred → returned to pending with elevated threshold
```

---

### ZONE 7 — GLOBAL MODEL BUILD + PUBLICATION

```
Approved vocabulary from moderation queue
│
├── Model build process (isolated, air-gapped signing system):
│   Combines: existing model vocabulary + newly approved pairs
│   Assigns: frequency scores, phonetic alignment scores
│   Validates: full Bengali Unicode validity, phonetic naturalness
│   Generates: global_model.bin with LGM1 header
│   Signs: Ed25519 signature (private key in HSM, never on server)
│   Hashes: SHA-256 of final binary
│
├── Published artifact:
│   State: 📦 PUBLISHED
│   Contents: roman_key → (selected_bengali, frequency_score) lookup table
│   What is NOT in the binary: any user identifiers, timestamps, IP, session data
│   Available at: GET /api/v1/model/download
│
└── Distribution to clients: → ZONE 8
```

---

### ZONE 8 — MODEL DOWNLOAD + VERIFICATION (Client)

```
likhi_sync.exe checks /api/v1/model/version
│
├── If new version available:
│   Downloads to: global_model.bin.tmp (State: 🖥️ LOCAL, temporary)
│   Verifies: SHA-256 hash matches server-published hash
│   Verifies: Ed25519 signature valid (public key embedded in likhi_sync.exe)
│   Verifies: LGM1 magic header present
│   Verifies: model_version > current_version
│   Verifies: min_client_version <= installed version
│
│   IF all pass:
│     Renames: global_model.bin → global_model.bin.bak  (🖥️ LOCAL, 30-day retention)
│     Moves:   global_model.bin.tmp → global_model.bin   (atomic, WRITE_THROUGH)
│     State:   global_model.bin = 🖥️ LOCAL, Read-Only
│
│   IF any check fails:
│     Deletes: global_model.bin.tmp immediately
│     Keeps:   current global_model.bin unchanged
│     Logs:    failure reason to local debug log
│
└── global_model.bin is memory-mapped by bangla_tsf.dll (Read-Only)
    This file NEVER alters user_dict.txt
    This file NEVER alters lexicon.bin
```

---

### ZONE 9 — CANDIDATE PRECEDENCE AT RUNTIME

```
bangla_tsf.dll evaluates candidates for input "porishkar"
│
│  [1] Personal Pinned Word (FLAG_PINNED)      🖥️ LOCAL → user_dict.txt
│  [2] Personal Learned (freq >= 3)            🖥️ LOCAL → user_dict.txt
│  [3] Curated Override (kLexiconOverrideFlag) 🖥️ LOCAL → lexicon.bin
│  [4] Global Model (global_model.bin)         🖥️ LOCAL → global_model.bin (read-only)
│  [5] Base Lexicon unigram                    🖥️ LOCAL → lexicon.bin
│  [6] Phonetic Parser output                  💨 TRANSIENT → in-memory only
│
│  global_model.bin can NEVER:
│    - Modify user_dict.txt
│    - Modify lexicon.bin
│    - Override a personal entry with freq >= 3
│    - Execute any code
│    - Trigger a network request
│
└── Final candidate list presented to user in UI window
    State: 💨 TRANSIENT
    Cleared after user makes selection or dismisses window
    Not logged, not stored, not transmitted
```

---

## SUMMARY TABLE — DATA LOCATION BY STATE

| Data | 🖥️ LOCAL | 💨 TRANSIENT | ⬆️ UPLOADED | 📊 AGGREGATED | 📦 PUBLISHED | 🚫 NEVER |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: |
| Raw keystrokes | | ✅ | | | | |
| Composition buffer | | ✅ | | | | |
| Keystroke timing | | | | | | ✅ |
| Window title / app name | | | | | | ✅ |
| Document content | | | | | | ✅ |
| Clipboard contents | | | | | | ✅ |
| Unselected candidates | | | | | | ✅ |
| user_dict.txt (full) | ✅ | | | | | |
| staged_learning.dat | ✅ | | | | | |
| global_model.bin | ✅ | | | | | |
| lexicon.bin | ✅ | | | | | |
| IP address | | ✅ | | | | |
| Session / device IDs | | | | | | ✅ |
| roman_key (common) | ✅ | | ✅ | ✅ | ✅ | |
| selected_bengali (common) | ✅ | | ✅ | ✅ | ✅ | |
| vote_weight | ✅ | | ✅ | ✅ | | |
| week_bucket | ✅ | | ✅ | ✅ | | |
| client_version | ✅ | | ✅ | | | |
| Frequency scores (aggregated) | | | | ✅ | ✅ | |
| Any user identifier | | | | | | ✅ |

---

## ARCHITECTURAL INVARIANTS

The following are non-negotiable invariants enforced at every stage:

```
INVARIANT 1: bangla_tsf.dll has zero network capability.
  Verified by: DLL import table check at build time.

INVARIANT 2: user_dict.txt is never modified by likhi_sync.exe, model updates, or installer.
  Verified by: Stage 2 uninstall preservation test (SHA-256 identical).

INVARIANT 3: IP address is never written to any server storage.
  Verified by: Server-side log configuration audit before production deployment.

INVARIANT 4: Contribution data never includes raw typing sequences, full words in context,
  document text, application names, or timing data.
  Verified by: Code review of staged_learning.dat record construction.

INVARIANT 5: Global model updates never alter user_dict.txt.
  Verified by: Unit test — update model, confirm user_dict.txt unchanged (byte-for-byte).

INVARIANT 6: A failed model verification never loads the model.
  Verified by: Unit test — corrupt signature → model not loaded → lexicon fallback.

INVARIANT 7: Contribution staging stops immediately on consent revocation.
  Verified by: Unit test — revoke consent → staging file deleted → no new appends.
```
