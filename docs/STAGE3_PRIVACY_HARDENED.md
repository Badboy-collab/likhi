# LIKHI — STAGE 3 PRIVACY SPECIFICATION (HARDENED)
## Version 1.1.0 | Hardening Review Applied

**Document Version:** 1.1.0
**Status:** HARDENED DRAFT — Awaiting Implementation Approval
**Supersedes:** STAGE3_PRIVACY.md v1.0.0
**Key Changes:**
- Removed all unqualified "anonymous" claims
- Added formal Data Classification Taxonomy v2 with honest identifiability ratings
- Added Rarity Gate specification
- Added precise revocation behavior
- Added Personal/Unique Word Protection section
- Added separate consent independence guarantee

---

## SECTION 1 — PRIVACY PRINCIPLES (HARDENED)

### 1.1 What This Document Claims (and Does Not Claim)

**Likhi's contribution system does NOT claim to be:**
- Fully anonymous (formal definition: cannot be distinguished from the population)
- Formally k-anonymous (as defined in Sweeney 2002)
- Differential-private (no noise injection in v1.0.0 pipeline)
- GDPR Article 89-compliant "fully anonymized" data

**Likhi's contribution system IS designed to be:**
- **Non-identifying by design:** The system is engineered to minimize the amount of personal
  information that could be inferred from any uploaded data.
- **Low-linkability:** The combination of fields in a single contribution tuple has been
  designed to resist linking to a specific device, user, or session.
- **Rarity-gated:** Rare, unusual, or personal-sounding vocabulary is filtered at the client
  before any data leaves the device.
- **Aggregation-threshold protected:** No individual pair becomes visible in the public model
  until a statistically meaningful number of independent contributions exist.
- **Human-moderation reviewed:** No vocabulary is published without human review, regardless
  of statistical threshold.

### 1.2 Privacy Hierarchy

```
[Highest Protection]
│
├─ Raw typing sequences / full words typed         → NEVER collected
├─ Application context / window titles             → NEVER collected
├─ Document content                                → NEVER collected
├─ Typing timestamps / inter-key intervals         → NEVER collected
├─ IP address                                      → NEVER stored (transient TCP only)
├─ Session identifiers / device IDs                → NEVER generated
├─ Clipboard contents                              → NEVER accessed
│
├─ Personal vocabulary / rare words / surnames     → Filtered before staging
├─ Custom shortcuts / user-unique mappings         → Filtered before staging
│
├─ Selected Bengali word (lexicon-verified)        → Eligible for staging (opt-in only)
├─ Roman phonetic key (common, lexicon-matched)    → Eligible for staging (opt-in only)
├─ Vote weight (always = 1)                        → Eligible for staging
├─ Week bucket (coarse weekly granularity)         → Eligible for staging
├─ Client version / model version                  → Operational metadata, eligible
│
[Lowest Protection Requirement — Shared Metadata]
```

---

## SECTION 2 — DATA CLASSIFICATION TAXONOMY v2

### 2.1 Local-Only Data (Never Leaves Device)

| Data Item | Storage | Protection Mechanism | User Accessible? |
| :--- | :--- | :--- | :---: |
| Full `roman_key → bengali_word` pair with frequency + timestamp | `user_dict.txt` | File stored in `%APPDATA%`, readable only by current user account (NTFS ACL) | Yes — directly editable |
| All committed words (frequency history) | `user_dict.txt` | Same as above | Yes |
| Bigram context history | In-memory only (`ContextRanker`) | Discarded on engine unload | No |
| Staging queue before upload | `staged_learning.dat` | File in `%APPDATA%`, readable only by current user | Yes — deleteable |
| Consent / settings flags | Registry under `HKCU\Software\Likhi\` | User-writable, not accessible to other users | Yes |
| Debug logs | `%APPDATA%\PC-Bangla-Typing-App\debug.log` | User-readable only | Yes |

### 2.2 Uploaded Data (Opt-In Only, Non-Identifying by Design)

| Field | Type | Classif. | Notes |
| :--- | :--- | :---: | :--- |
| `roman_key` | string | ⚠️ Potentially Identifying | Must pass Rarity Gate. Rare/personal words rejected. |
| `selected_bengali` | string | ⚠️ Potentially Identifying | Must be in lexicon. Rare/personal Bengali words rejected. |
| `vote_weight` | int (always 1) | ✅ Non-Sensitive | Fixed value, conveys no identity. |
| `week_bucket` | string (e.g. `2026-W37`) | ✅ Operational Metadata | Weekly granularity — not identifiable. |
| `client_version` | string (e.g. `1.0.0`) | ✅ Operational Metadata | Shared by all users on same release. |
| `lexicon_version` | string | ✅ Operational Metadata | Shared by all users with same installer. |
| `global_model_version` | uint32 | ✅ Operational Metadata | Shared by all users on same model. |

### 2.3 Never-Collected Data

The following data is **architecturally excluded** — no code path in `bangla_tsf.dll` or
`likhi_sync.exe` accesses or transmits this data:

- Raw keystroke sequences
- Timing between keystrokes
- Unselected candidates (what was rejected)
- Complete word or sentence context around the committed word
- Window titles or application names
- Any clipboard content
- Screenshot or screen region captures
- Device hardware identifiers (MAC address, CPU ID, disk serial)
- Precise geolocation
- Any data from other applications

---

## SECTION 3 — RARITY GATE SPECIFICATION

### 3.1 Purpose

The Rarity Gate is a mandatory client-side filter applied before any tuple is appended to
`staged_learning.dat`. It prevents personal, rare, and sensitive vocabulary from ever
entering the contribution pipeline.

### 3.2 Four-Rule Rarity Gate

**Rule R1 — Bengali Lexical Membership:**

```cpp
bool PassesR1(const wstring& bengali_word) {
    // Must be present in lexicon.bin with unigram rank <= 100,000
    return lexicon_.Contains(bengali_word) &&
           lexicon_.GetUnigramRank(bengali_word) <= kMaxLexiconRankForContribution;
}
```

Words absent from the lexicon, or present only with very low frequency rank, are rejected.
This removes personal names, surnames, place names not in the vocabulary, and custom words.

**Rule R2 — Roman Key Lexical Match:**

```cpp
bool PassesR2(const string& roman_key) {
    // roman_key must match at least one word in lexicon.bin
    return phonetic_index_.HasMapping(roman_key);
}
```

A roman key that has no entries in the phonetic-to-Bengali index is rejected. This filters
out typed usernames, abbreviations, and non-standard roman sequences.

**Rule R3 — Local Selection Floor:**

```cpp
bool PassesR3(const string& roman_key, const wstring& bengali_word) {
    // Must have been selected locally at least 5 times (not just 3)
    auto entry = personal_dict_.Find(roman_key, bengali_word);
    return entry != nullptr && entry->frequency >= kMinFrequencyForContribution;  // = 5
}
```

Single or occasional selections are excluded. Only consistently-used words contribute,
reducing accidental selection noise.

**Rule R4 — Manual Custom Exclusion:**

```cpp
bool PassesR4(const string& roman_key, const wstring& bengali_word) {
    // Reject if marked as manual custom or contribution-excluded
    auto entry = personal_dict_.Find(roman_key, bengali_word);
    if (entry == nullptr) return true;  // Not in personal dict: allow R1-R3 to decide
    return !(entry->flags & (kFlagManualCustom | kFlagContributionExcluded));
}
```

Words explicitly added by the user as custom shortcuts, or marked as private, are never
contributed.

**Combined Gate:**

```cpp
bool EligibleForContribution(const string& roman_key, const wstring& bengali_word) {
    return PassesR1(bengali_word) &&
           PassesR2(roman_key) &&
           PassesR3(roman_key, bengali_word) &&
           PassesR4(roman_key, bengali_word);
}
```

Only a tuple that passes ALL four rules is appended to `staged_learning.dat`.

### 3.3 Consequence of Rarity Gate on Personal Names

A user who regularly types their colleague's name (e.g. phonetically `pervez → পারভেজ`)
will not have this pair contributed, because:
- `পারভেজ` is likely absent from the common-vocabulary lexicon (R1 fails), **OR**
- `pervez` maps to no standard lexicon entry (R2 fails).

Even if both rules pass (the name happens to be in the lexicon), the word is flagged by
the moderation team as a personal name during Stage B human review.

---

## SECTION 4 — USER CONTROL AND CONSENT

### 4.1 Informed Consent UI Requirements

The following consent disclosure MUST appear before enabling Global Contribution:

```
CONTRIBUTE ANONYMOUS SUGGESTIONS

When this setting is ON, Likhi will occasionally send a small amount
of de-identified typing data to help improve Bangla typing for all users.

WHAT IS SENT:
  ✓ Common phonetic spellings you typed (e.g. 'porishkar')
  ✓ Which word you selected (e.g. 'পরিষ্কার')
  ✓ The week in which you typed it (not the exact time)
  ✓ App version information

WHAT IS NEVER SENT:
  ✗ Your full sentences or documents
  ✗ Rare or unusual words unique to you
  ✗ Names, addresses, or company names
  ✗ Typing timestamps
  ✗ Your IP address (it is discarded immediately)
  ✗ Any information that identifies you

HOW TO STOP:
  Turn this setting OFF at any time. Your previously sent data
  cannot be individually recalled (it has been merged into aggregate
  statistics), but no new data will be sent.

Your personal learning data STAYS ON YOUR DEVICE regardless of this setting.

[ Enable Contribution ]   [ Cancel ]
```

All text in the consent dialog must be:
- In the user's selected language (Bengali or English)
- Rendered in at least 11pt font
- Not pre-checked or enabled by default
- Presented before the first contribution is staged

### 4.2 Consent Revocation (Synchronous, Guaranteed)

When the user turns off "Contribute Anonymous Suggestions":

```
Immediate (within current process, synchronous):
  1. In-memory contribution flag set to false.
  2. No further tuples appended to staged_learning.dat.

Next likhi_sync.exe wake cycle (≤ 10 minutes):
  3. Sync worker detects consent_revoked flag in settings.
  4. Sync worker aborts any in-flight HTTP POST.
  5. Sync worker deletes staged_learning.dat entirely.
  6. Sync worker clears contribution_session_id (if any).
  7. Sync worker writes revocation_confirmed=true to settings.

Permanent:
  8. No data from staged_learning.dat will ever be uploaded.
  9. If the deletion fails (file locked), retry every 30 seconds until success.
```

**Personal dictionary (`user_dict.txt`) is untouched in all steps above.**

### 4.3 "Clear All Learned Data" Feature (Separate from Revocation)

This is a distinct, explicit action requiring a second confirmation:

```
CLEAR YOUR PERSONAL LEARNING DATA

This will permanently delete your personal Bangla typing history.
Likhi will no longer remember your preferred word choices.
This action CANNOT be undone.

Your contribution settings will NOT be changed.
Words you have contributed previously cannot be recalled from the shared model.

[ Delete Personal Learning Data ]   [ Cancel ]
```

Effect: Deletes `user_dict.txt`. Personal Learning setting remains enabled unless separately
disabled. The engine returns to base vocabulary behavior immediately.

---

## SECTION 5 — DATA RETENTION AND DELETION

### 5.1 Client-Side Retention

| Data | Retention Rule |
| :--- | :--- |
| `user_dict.txt` | Kept until user explicitly deletes via "Clear All Learned Data" or manual file deletion. Preserved across app updates and uninstalls (Windows uninstaller behavior per Stage 2 verified behavior). |
| `staged_learning.dat` | Deleted immediately when contribution consent is revoked. Uploaded and cleared within 24 hours of creation when consent is active. Maximum age: 7 days (older un-uploaded records are discarded). |
| `global_model.bin` | Kept until replaced by a newer verified model or the user manually deletes it. |
| `global_model.bin.bak` | Kept for 30 days, then deleted by `likhi_sync.exe` cleanup routine. |
| Debug logs | Rotated after 7 days. |

### 5.2 Server-Side Retention

| Data | Retention Rule |
| :--- | :--- |
| Received contribution tuples (pre-aggregation) | Deleted from raw ingestion buffer within 48 hours of aggregation. Not persisted long-term. |
| Aggregated frequency counts (pre-threshold) | Retained in aggregation store until K=50 threshold is either met (proceeds to moderation) or the 90-day collection window expires (purged). |
| Published global model | Retained as a versioned artifact. Only linguistic content, no user data. |
| IP addresses | Not stored. |
| Request logs | Stripped of IP before write. Retained 7 days for debugging only. |

---

## SECTION 6 — GDPR / PDPA CONSIDERATIONS

### 6.1 Applicability Assessment

Likhi is a local Windows application. The primary legal question is whether the uploaded
contribution data constitutes "personal data" under GDPR Article 4(1) or Bangladesh's
Personal Data Protection Act.

**Assessment for uploaded contribution tuples:**
- No name, no email, no address, no device ID, no IP, no precise timestamp.
- The tuple `(roman_key, selected_bengali, week_bucket)` alone is unlikely to constitute
  personal data for common words (e.g. `"ami" → "আমি"`, `"porishkar" → "পরিষ্কার"`).
- For rare words that pass through the gate (if the gate is misconfigured), the tuple
  COULD constitute personal data if it is uniquely identifying.
- **Conservative approach:** Treat all server-side contribution data as personal data until
  a formal legal assessment is completed. Implement all retention and deletion controls
  accordingly.

### 6.2 Legal Basis for Processing

If global contribution is treated as personal data processing:
- **Legal basis:** Freely given, specific, informed, and unambiguous consent (GDPR Art. 6(1)(a)).
- **Consent withdrawal:** User can withdraw at any time with no detriment.
- **Data subject rights:** Right to access, rectification, erasure, and portability.
- **Erasure complexity:** Because data is aggregated into shared statistics, individual
  erasure from the server-side aggregated counts may not be technically feasible after
  aggregation. This must be disclosed in the privacy notice.

### 6.3 Privacy Notice Requirements

A privacy notice reachable from the application Settings must disclose:
1. What data is collected (the exact fields listed in §2.2)
2. How long data is retained (per §5.2)
3. Who processes the data (Likhi operator)
4. User rights (access, deletion, portability)
5. Contact information for data subject requests
6. The aggregation-based erasure limitation

---

## SECTION 7 — PRIVACY REVIEW CHECKLIST

Before any Stage 3 production code is deployed, the following items must be confirmed:

| # | Item | Status |
| :-- | :--- | :---: |
| P1 | `bangla_tsf.dll` has zero network imports (link table verified) | STAGE 2 ✅ |
| P2 | Consent dialog shown before first contribution staged | TO IMPLEMENT |
| P3 | Global Contribution defaults to OFF in installer and settings | TO IMPLEMENT |
| P4 | Revocation deletes `staged_learning.dat` (tested with interrupt) | TO IMPLEMENT |
| P5 | Rarity Gate passes only lexicon-verified, common words | TO IMPLEMENT |
| P6 | IP address never written to server logs (server config audited) | TO IMPLEMENT |
| P7 | No typing timestamps included in contribution tuples | TO IMPLEMENT |
| P8 | Two-stage moderation before model publication | TO IMPLEMENT |
| P9 | "Clear Personal Data" action is distinct from contribution revocation | TO IMPLEMENT |
| P10 | `user_dict.txt` preserved through model updates and uninstall | STAGE 2 ✅ |
| P11 | Legal privacy notice reachable from app settings | TO IMPLEMENT |
| P12 | Server-side IP stripping configured before server goes live | TO IMPLEMENT |
