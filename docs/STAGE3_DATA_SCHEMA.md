# LIKHI — STAGE 3 DATA SCHEMA SPECIFICATION
## LOCAL DICTIONARY, STAGING QUEUE & GLOBAL MODEL BINARY FORMAT

**Document Version:** 1.0.0  
**Status:** DRAFT SPECIFICATION (Data Schema Design Only — No Implementation)  
**Target:** Likhi (লিখি) Windows Typing System  

---

## 1. PERSONAL DICTIONARY FILE FORMAT (`user_dict.txt`)

### 1.1 Current Schema (v1.0.0 - Production Active)
Stored at `%APPDATA%\PC-Bangla-Typing-App\user_dict.txt`.
Encoding: UTF-8 without BOM. Line endings: Windows CRLF (`\r\n`) or Unix LF (`\n`).
Format: Tab-Separated Values (TSV), 4 fields per line.

```tsv
<roman_key>\t<bengali_word>\t<frequency>\t<last_used_timestamp>\n
```

#### Field Specifications:

| Field Index | Field Name | Data Type | Nullable | Constraints / Validation | Description |
| :---: | :--- | :--- | :---: | :--- | :--- |
| **0** | `roman_key` | String (`std::string`) | No | `1 <= length <= 64`, lower-case ASCII `[a-z0-9_.-]` | The normalized Roman phonetic string typed by the user. |
| **1** | `bengali_word` | String (UTF-8) | No | Valid Bengali sequence (`UnicodeUtils::IsValidBengaliSequence`) | The committed Bengali text. |
| **2** | `frequency` | Unsigned 32-bit (`uint32_t`) | No | `1 <= frequency <= 4294967295`, default `1` | Number of times this word was selected for this `roman_key`. |
| **3** | `last_used_timestamp` | Unsigned 64-bit (`uint64_t`) | No | Unix epoch in seconds | Timestamp of most recent user selection. |

#### Example Entries:
```tsv
porishkar	পরিষ্কার	14	1788949737
bhalo	ভালো	42	1788949820
bhalo	ভালোই	18	1788949850
anwar	আনোয়ার	7	1788949100
```

---

### 1.2 Extended Forward-Compatible Schema (v2.0.0 Proposal)
To support custom user shortcuts, pinned entries, and recency decay without breaking v1.0.0 parsers, optional fields are appended as subsequent tab columns:

```tsv
<roman_key>\t<bengali_word>\t<frequency>\t<last_used_timestamp>\t<flags>\t<user_tag>\n
```

#### Additional Fields (Optional / Backward-Compatible):
* **Field 4 (`flags`):** Hexadecimal bitmask (`uint32_t`, default `0x00000000`).
  - Bit 0 (`0x01`): `FLAG_PINNED` — Always stay at Slot #1 regardless of frequency.
  - Bit 1 (`0x02`): `FLAG_MANUAL_CUSTOM` — Explicitly added by user in Settings UI (never decayed).
  - Bit 2 (`0x04`): `FLAG_CONTRIBUTION_EXCLUDED` — User flagged this word as private (never staged).
* **Field 5 (`user_tag`):** Optional UTF-8 label (e.g., `personal`, `medical`, `shortcut`).

#### Backward Compatibility Rule:
A v1 parser reading a v2 file reads the first 4 tokens and ignores any tokens beyond column 3. A v2 parser reading a v1 file defaults `flags = 0` and `user_tag = ""`.

---

## 2. STAGING QUEUE SCHEMA (`staged_learning.dat`)

Stored at `%LOCALAPPDATA%\Likhi\staged\staged_learning.dat`.
Accessible only to current Windows user context. Used only when `[x] Contribute Anonymous Suggestions` is enabled.
Format: Binary record stream with CRC32 integrity checks.

### Record Layout:
```
+-------------------------------------------------------+
| Magic (4 bytes): 'LKST'                               |
| Record Length (2 bytes, uint16_t)                     |
| Roman Key Length (1 byte, uint8_t)                    |
| Roman Key (N bytes, ASCII)                            |
| Bengali Word Length (2 bytes, uint16_t)               |
| Bengali Word (M bytes, UTF-8)                         |
| Selection Count (2 bytes, uint16_t, clamped <= 10)    |
| Day Bucket Code (2 bytes, uint16_t, e.g. DaysSince2026)|
| Record CRC32 (4 bytes, uint32_t)                      |
+-------------------------------------------------------+
```

---

## 3. ANONYMOUS CONTRIBUTION API PAYLOAD (JSON)

Endpoint: `POST /api/v1/contribute`  
Content-Type: `application/json; charset=utf-8`  
Payload size: $\le 32$ KB  

```json
{
  "$schema": "https://api.likhi.org/schemas/v1/contribution.json",
  "schema_version": "1.0",
  "client_meta": {
    "app_version": "1.0.0",
    "base_lexicon_version": "2026.09.01",
    "pow_nonce": "9823412",
    "batch_period_bucket": "2026-W37"
  },
  "contributions": [
    {
      "roman_key": "porishkar",
      "selected_candidate": "পরিষ্কার",
      "vote_weight": 1
    },
    {
      "roman_key": "suchi",
      "selected_candidate": "শুচি",
      "vote_weight": 1
    },
    {
      "roman_key": "suddho",
      "selected_candidate": "শুদ্ধ",
      "vote_weight": 1
    }
  ]
}
```

### Constraints:
* `contributions` array: Min 1 item, Max 20 items per 24 hours.
* `vote_weight`: Always clamped to integer `1`.
* No device IDs, machine names, usernames, or IP strings in the JSON body.

---

## 4. GLOBAL MODEL BINARY FILE FORMAT (`global_model.bin`)

Compiled and cryptographically signed on the server; downloaded to `%LOCALAPPDATA%\Programs\Likhi\data\global_model.bin`.
Designed for **Zero-Copy Memory-Mapped I/O** (`MapViewOfFile`). Instantaneous load time ($< 1$ ms) with zero memory allocations.

### Binary Layout Structure:

```
+-----------------------------------------------------------------------------+
| HEADER BLOCK (64 bytes)                                                     |
|   0x00: Magic (4 bytes): 'L', 'G', 'M', '1' ('LGM1')                       |
|   0x04: Format Version (uint32_t = 1)                                       |
|   0x08: Model Version (uint32_t = 10001 for 1.0.1)                          |
|   0x0C: Minimum Compatible Client Version (uint32_t = 10000 for 1.0.0)      |
|   0x10: Build Timestamp (uint64_t Unix Epoch)                               |
|   0x18: Total Unique Roman Keys (uint32_t)                                  |
|   0x1C: Total Bengali Candidate Entries (uint32_t)                          |
|   0x20: SHA256 Checksum of Payload (32 bytes)                               |
|   0x40: Ed25519 Cryptographic Signature of Header+Payload (64 bytes)        |
+-----------------------------------------------------------------------------+
| OFFSET INDEX TABLE (Array of uint32_t file offsets)                         |
|   Bucket index for fast O(1) hash lookup of roman_key                       |
+-----------------------------------------------------------------------------+
| CANDIDATE ENTRY RECORDS                                                     |
|   [For each roman_key]:                                                     |
|     - uint8_t  roman_key_len                                                |
|     - char[]   roman_key (ASCII)                                            |
|     - uint16_t candidate_count                                              |
|     - [For each candidate]:                                                 |
|         - uint16_t bengali_len                                              |
|         - char[]   bengali_text (UTF-8)                                     |
|         - uint16_t global_frequency_score (scaled 0-10000)                  |
+-----------------------------------------------------------------------------+
```

### Memory-Mapped Lookup Performance:
* Read latency: $O(1)$ directly mapped into host process virtual memory.
* Zero heap allocations inside host applications (`WINWORD.EXE`, `Notepad.exe`).
* Immune to memory corruption across process lifecycles.
