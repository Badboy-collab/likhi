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
