# 🧪 Likhi (লিখি) — Permanent Regression Test Suite

> **“A later feature or code change MUST NOT silently remove, regress, overwrite, disable or break a previously confirmed fix.”**

---

## 1. Mandatory Test Gates Overview

| Test Suite | Binary / Script Target | Test Count | Passing Requirement |
| :--- | :--- | :--- | :--- |
| **Update Service Suite** | `build/test_update_service.exe` | 39 Cases | **100% (39/39)** |
| **P0 Native Keyboard Gate** | `build/test_key_policy.exe` | 196 Cases | **100% (196/196)** |
| **Phonetic & Engine Unit Tests** | `build/test_runner.exe` | 220 Cases | **100% (220/220)** |
| **Unicode Bengali Integrity** | `build/unicode_bengali_tests.exe` | 46 Cases | **100% (46/46)** |
| **Universal Mode Safety Gate** | `build/test_universal_typing.exe` | 86 Cases | **100% (86/86)** |
| **TSF Integration Suite** | `build/test_tsf_integration.exe` | 86 Cases | **100% (86/86)** |
| **Real-World Live QA Runner** | `build/real_world_qa_runner.exe` | 90 Cases | **100% (90/90)** |

---

## 2. Update Connection & Continuous Improvement Regression Catalog

### Version Parsing & Semantic Comparisons (`TEST-UPD-001` - `TEST-UPD-010`)
- `TEST-UPD-001`: `1.0.0` == `v1.0.0` (Identical version identity).
- `TEST-UPD-002`: `1.0.0` < `1.1.0` (Minor version update detected).
- `TEST-UPD-003`: `1.0.0` < `1.0.1` (Patch version update detected).
- `TEST-UPD-004`: `1.0.0` < `2.0.0` (Major version update detected).
- `TEST-UPD-005`: `1.0.0` > `1.0.0-test2` (Official release takes precedence over prerelease).
- `TEST-UPD-006`: `1.0.0-test1` < `1.0.0-test2` (Prerelease progression).

### GitHub Releases JSON Parsing (`TEST-UPD-011` - `TEST-UPD-020`)
- `TEST-UPD-011`: Successfully extracts `tag_name`, `name`, `body`, and `published_at`.
- `TEST-UPD-012`: Identifies `LikhiSetup.exe` asset URL, file size, and SHA-256 digest.
- `TEST-UPD-013`: Resilient against escaped JSON characters (`\r\n`, quotes).
- `TEST-UPD-014`: Malformed or empty JSON returns `false` gracefully without crashing.

### Throttling & User Control (`TEST-UPD-021` - `TEST-UPD-028`)
- `TEST-UPD-021`: Automated startup checks are throttled to 24 hours via `update_cache.json`.
- `TEST-UPD-022`: Manual "Check for Updates" bypasses cooldown on user demand.
- `TEST-UPD-023`: User selecting "Later" snoozes notifications for that version for 7 days.
- `TEST-UPD-024`: A newer release supersedes previous snoozes.

### Cryptographic Digest & Security (`TEST-UPD-029` - `TEST-UPD-034`)
- `TEST-UPD-029`: Valid SHA-256 hash match verified via Win32 CryptoAPI (`CALG_SHA_256`).
- `TEST-UPD-030`: Tampered or mismatched file digest strictly rejected.
- `TEST-UPD-031`: Case-insensitive hex string and `sha256:` prefix normalization.

### 100% User Data Preservation Guarantee (`TEST-UPD-035` - `TEST-UPD-039`)
- `TEST-UPD-035`: User settings in `%APPDATA%\PC-Bangla-Typing-App\settings.json` preserved intact.
- `TEST-UPD-036`: Personal dictionary words in `user_dict.txt` preserved intact.
- `TEST-UPD-037`: Update operations never delete or reset user learning data.

---

## 3. P0 Keyboard Regression Catalog

### Modifier Shortcuts (MUST NEVER Produce Bangla Characters)
- `TEST-P0-001`: `Ctrl + V` $\to$ **Paste** (`*pfEaten = FALSE`, never produces `ভ`).
- `TEST-P0-002`: `Ctrl + C` $\to$ **Copy** (`*pfEaten = FALSE`).
- `TEST-P0-003`: `Ctrl + X` $\to$ **Cut** (`*pfEaten = FALSE`).
- `TEST-P0-004`: `Ctrl + A` $\to$ **Select All** (`*pfEaten = FALSE`).
- `TEST-P0-005`: `Ctrl + Z` $\to$ **Undo** (`*pfEaten = FALSE`).
- `TEST-P0-006`: `Ctrl + Y` $\to$ **Redo** (`*pfEaten = FALSE`).
- `TEST-P0-007`: `Ctrl + S` $\to$ **Save** (`*pfEaten = FALSE`).
- `TEST-P0-008`: `Ctrl + F` $\to$ **Find** (`*pfEaten = FALSE`).
- `TEST-P0-009`: `Alt + Tab` / `Alt + F4` $\to$ **Window management** (`*pfEaten = FALSE`).

### Function Keys & Numpad
- `TEST-P0-010`: `F1` to `F12` $\to$ Passed directly to host app (`*pfEaten = FALSE`).
- `TEST-P0-020`: `VK_NUMPAD0` to `VK_NUMPAD9` $\to$ Produces `0`..`9` (`*pfEaten = FALSE`).
