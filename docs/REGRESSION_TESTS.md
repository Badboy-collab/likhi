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
| **Universal Mode Safety Gate** | `build/test_universal_typing.exe` | 86 Cases | **100% (86/86)** |
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

---

## 4. Universal Mode Regression Catalog (non-TSF fallback path)

Universal Mode exists because TSF cannot reach every host (UWP/Store apps such as
WhatsApp Desktop, some Java UIs, games, remote sessions). It buffers the roman
letters while they travel to the application and, at a word boundary, replaces
them with the Bengali word typed as Unicode. Because that touches real text, the
safety rules are a permanent gate (`build/test_universal_typing.exe`):

### Never transformed (whatever is buffered, whatever the mode)
- `TEST-UNI-001`: `Ctrl+C/V/X/A/Z/Y/S/F/P` $\to$ pass-through, **zero erasures**, and the pending word survives the chord.
- `TEST-UNI-002`: `Ctrl+Space` (IME toggle) and `Win+Space` (language switch) $\to$ pass-through.
- `TEST-UNI-003`: `Alt+letter`, `Alt+F4` $\to$ pass-through with no erasure.
- `TEST-UNI-004`: `F1`-`F24` $\to$ pass-through, no erasure.
- `TEST-UNI-005`: Arrows, `Home`, `End`, `Page Up/Down`, `Insert`, `Delete`, unknown/system keys $\to$ pass-through, no erasure, preview dropped (the caret may have moved).
- `TEST-UNI-006`: Numpad digits and operators $\to$ pass-through, never transformed.
- `TEST-UNI-007`: Modifiers and locks alone (`Shift`, `Ctrl`, `Alt`, `Caps Lock`, `Num Lock`, `Win`, Menu) $\to$ pass-through, preview kept.

### Minimal-interference composition
- `TEST-UNI-010`: With an empty preview, space/enter/tab/digits/punctuation are native (`12.5`, IP addresses and English text stay untouched).
- `TEST-UNI-011`: Exactly one key per word is consumed: the word boundary.
- `TEST-UNI-012`: `Backspace`/`Esc` are never consumed; they keep native behaviour and only adjust the preview (`TEST-P0-031` parity).
- `TEST-UNI-013`: `space` commits the word with exactly one `U+0020`; a second space is native.
- `TEST-UNI-014`: `.` after a word becomes the Bengali full stop `।`; other punctuation/digits are kept after the word.
- `TEST-UNI-015`: Preview overflow (32 letters) leaves the rest of that word completely alone and requests no erasure.
- `TEST-UNI-016`: Mode off, `Reset()` and caret/focus changes always leave nothing to replace.
- `TEST-UNI-017`: Input mode policy — `TSF Only` never uses the universal path, `Universal Only` always does, `Automatic` stands down while TSF is active for the focused application.

### Same engine, one personalisation
- `TEST-UNI-020`: Universal Mode must transliterate through the shared `BanglaEngine` (same lexicon, fuzzy matcher, personal dictionary, learning) — it must never introduce a second transliteration engine.
