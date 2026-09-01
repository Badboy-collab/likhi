# 🧪 Likhi (লিখি) — Permanent Regression Test Suite

> **“A later feature or code change MUST NOT silently remove, regress, overwrite, disable or break a previously confirmed fix.”**

---

## 1. Mandatory Test Gates Overview

| Test Suite | Binary / Script Target | Test Count | Passing Requirement |
| :--- | :--- | :--- | :--- |
| **P0 Native Keyboard Gate** | `build/test_p0_keyboard.exe` | 74 Cases | **100% (74/74)** |
| **Unicode Bengali Integrity** | `build/unicode_bengali_tests.exe` | 46 Cases | **100% (46/46)** |
| **Phonetic & Engine Unit Tests** | `build/test_runner.exe` | 220 Cases | **100% (220/220)** |
| **TSF Integration Suite** | `build/test_tsf_integration.exe` | 86 Cases | **100% (86/86)** |
| **Real-World Live QA Runner** | `build/real_world_qa_runner.exe` | 90 Cases | **100% (90/90)** |
| **520 Gold Sentences Benchmark** | `tools/benchmark_runner/benchmark_expanded.py` | 520 Sentences | **$\ge 98.0\%$ Accuracy** |
| **Banglish Spelling Variations** | `tools/benchmark_runner/benchmark_expanded.py` | 45 Variations | **100% (45/45)** |

---

## 2. P0 Keyboard Regression Catalog

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

### Function Keys (MUST NEVER Enter Transliteration)
- `TEST-P0-010`: `F1` to `F12` $\to$ Passed directly to host app (`*pfEaten = FALSE`).
- `TEST-P0-011`: `F5` $\to$ Application refresh (`*pfEaten = FALSE`).

### Numpad Numeric Keypad (MUST NEVER Produce Bangla Characters)
- `TEST-P0-020`: `VK_NUMPAD0` to `VK_NUMPAD9` $\to$ Produces `0`..`9` (`*pfEaten = FALSE`).
- `TEST-P0-021`: `VK_ADD`, `VK_SUBTRACT`, `VK_MULTIPLY`, `VK_DIVIDE`, `VK_DECIMAL` $\to$ Produces `+`, `-`, `*`, `/`, `.` (`*pfEaten = FALSE`).

### Navigation & Cursor Editing Keys
- `TEST-P0-030`: `Left`, `Right`, `Up`, `Down`, `Home`, `End`, `Page Up`, `Page Down` $\to$ Move cursor normally.
- `TEST-P0-031`: `Backspace` $\to$ Grapheme-aware deletion during composition, standard deletion otherwise.
- `TEST-P0-032`: `Delete` $\to$ Standard forward character deletion (`*pfEaten = FALSE`).
- `TEST-P0-033`: `Tab` / `Esc` / `Enter` $\to$ Standard control behavior.

---

## 3. Dynamic Composition & Vocabulary Regression

- `TEST-COMP-001`: `ANO` $\to$ `আনো` (Active buffer).
- `TEST-COMP-002`: `ANOY` $\to$ `আনোয়`, `ANOYA` $\to$ `আনোয়া`, `ANOYAR` $\to$ `আনোয়ার` (Unbroken buffer).
- `TEST-VOCAB-001`: `battery` / `battary` / `batery` $\to$ `ব্যাটারি`.
- `TEST-VOCAB-002`: `fan` $\to$ `ফ্যান`, `table` $\to$ `টেবিল`, `chair` $\to$ `চেয়ার`, `computer` $\to$ `কম্পিউটার`, `mouse` $\to$ `মাউস`, `control` $\to$ `কন্ট্রোল`, `office` $\to$ `অফিস`.
- `TEST-ENG-001`: `office` $\to$ `অফিস | office | অফিসে` (Original English preserved).
