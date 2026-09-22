# 📘 Likhi (লিখি) — Master Development Specification & Source of Truth

> **“বাংলা লিখুন, সহজেই।”**
> **Tagline**: Fast • Smart • Natural — Bangla Typing for Windows
> **Developer**: AH Creations
> **Version**: 1.0.0

---

## 1. Product Overview & Vision
Likhi (লিখি) is a lightweight, ultra-fast, modern Bangla typing application designed for Microsoft Windows. It bridges the gap between modern smartphone typing and desktop PC productivity with an independent phonetic engine, rich Banglish recognition, fuzzy spelling tolerance, smart suggestion bar, and native Windows TSF integration.

---

## 2. Architectural Pillars

```
+-------------------------------------------------------------+
|              Likhi Settings Application (Win32)             |
+-------------------------------------------------------------+
                              |
+-------------------------------------------------------------+
|        Windows Text Services Framework (TSF) Layer          |
|    (ITfTextInputProcessorEx, In-Process COM Server)         |
+-------------------------------------------------------------+
                              |
+-------------------------------------------------------------+
|              Native Keyboard Event Classifier               |
|      (P0 Bypass for Ctrl, Alt, Win, F1-F12, Numpad, Nav)    |
+-------------------------------------------------------------+
                              | (Only normal characters)
+-------------------------------------------------------------+
|                  Dynamic Composition Engine                 |
|             (Continuous unbroken raw buffer)                |
+-------------------------------------------------------------+
                              |
+-------------------------------------------------------------+
|              Bangla Phonetic & Ranking Engine               |
|       - 52,412 Validated Lexicon Trie                       |
|       - Bigram Context Model                                |
|       - Fuzzy Banglish & Edit-Distance Matching             |
|       - English Candidate Preservation                      |
|       - Personal Dictionary CRUD                            |
|       - (Gated) Personal User Learning                      |
+-------------------------------------------------------------+
```

---

## 3. Cumulative Feature Set

### 1. P0 Native Keyboard Stability (Permanent)
- `Ctrl+V` pastes cleanly without producing `ভ`.
- `Ctrl+C`, `Ctrl+X`, `Ctrl+A`, `Ctrl+Z`, `Ctrl+Y`, `Ctrl+S`, `Ctrl+F` operate natively.
- `F1`-`F12`, `Numpad (0-9, operators)`, `Arrows`, `Home`, `End`, `Delete`, `Tab`, `Esc` pass through directly to host applications.
- Native Windows `Win + Space` language switching with exclusive `Bangla (Bangladesh)` (0x0845) registration.

### 2. P1 Continuous Composition Buffer
- Active composition buffer dynamically updates without premature splitting (`ANOYAR` $\to$ `আনোয়ার`).

### 3. P2/P3/P4 Banglish & Fuzzy Spelling
- Recognizes 52,412 modern technology, daily objects, and loanwords (`fan`, `table`, `chair`, `computer`, `mouse`, `control`, `battery`, `office`, `wifi`, `charger`).
- Tolerates minor typos (`battary`/`batteri`/`batery` $\to$ `ব্যাটারি`).

### 4. P5/P6 Suggestion Engine & English Candidate
- Interactive multi-candidate bar with selectable numbers (`1` to `5`).
- Exact English word is preserved in candidate list for mixed typing (`Google`, `Facebook`, `office`).

### 5. P7 Personal Dictionary
- User-defined mappings stored locally in `%APPDATA%\PC-Bangla-Typing-App\personal_dict.txt` with Add/Delete/Import/Export.

### 6. P8 Personal Learning (Gated)
- Gated until explicit user approval.

### 7. P10 Settings Application
- Modern Fluent UI with Sidebar Navigation (General, Typing, Suggestions, Banglish, Dictionary, Keyboard, Appearance, Advanced, About with AH Creations branding).

---

## 4. Maintenance & Cumulative Development Rule
Any future enhancement must pass all 6 automated test suites before deployment. No change may remove or degrade a previously approved feature.

---

## 5. Dual input backend (2026-09-21)

TSF cannot reach every Windows host, so Likhi now has a second, explicit input
path. Both share ONE language engine, dictionary, fuzzy matcher, personal
dictionary and learning file - there is no second transliteration engine.

```
                 LIKHI CORE (engine/)
                        |
        +---------------+---------------+
        |                               |
   TSF backend                     Universal backend
   tsf/bangla_tsf.dll              tools/universal/likhi_universal.exe
   (composition, candidate         (roman letters reach the app, a word
    strip, display attributes)      boundary replaces them with Unicode
                                    keystrokes)
```

### 5.1 Universal Mode rules (permanent)
- Nothing is transformed unless the focused window really has Bengali (0x0845)
  selected, so English typing, digits and punctuation are never touched.
- Every modifier chord, function key, navigation key and numpad key passes
  through; at most one key per word (the boundary) is consumed.
- `Automatic` stands down whenever `bangla_tsf.dll` is loaded in the focused
  process, or when that cannot be determined (safe default).
- The preview is dropped - never erased from the host - on focus/caret changes.
- No typed text is logged. `universal.log` holds mode changes and counters only.
- Diagnostics: `likhi_universal.exe --check` writes
  `%APPDATA%\PC-Bangla-Typing-App\universal_check.txt`.

### 5.2 Input Mode setting
`settings.json` key `input_mode`: `automatic` (default) | `tsf_only` |
`universal`; `universal_mode: false` pauses the universal host entirely.
Hotkey `Ctrl+Alt+L` pauses/resumes it; the tray tooltip shows the live mode.

### 5.3 Still to wire (see AGENTS.md assessment)
Installer must ship and autostart `likhi_universal.exe`, the Settings app needs
the Input Mode control, and the universal host should surface the same cloud
suggestions as the TSF path.
