# Likhi (লিখি) — Master Specification & Technical Reference

> **"বাংলা লিখুন, সহজেই।"**  
> Native Windows Bangla Phonetic Input Method & Universal Typing Engine  
> Target OS: Windows 10 & Windows 11 (64-bit and 32-bit)  
> Version: 1.0.0

---

## 1. Architectural Foundation
- **Language**: Modern C++20 with strict deterministic memory management.
- **Subsystems**:
  1. **Core Language Engine (`engine/`)**: Pure phonetic parser, 52,412 word Flat Binary Lexicon Trie (v2), context ranker, personal dictionary.
  2. **Text Services Framework TIP (`tsf/`)**: Windows COM In-Process Server (`bangla_tsf.dll`).
  3. **Universal Typing Host (`universal/` & `tools/universal/`)**: Low-level hook and Unicode injection (`likhi_universal.exe`) for Store/UWP apps.
  4. **Configuration & Settings (`tools/settings_app/`)**: Win32 Fluent UI application (`bangla_settings.exe`).
  5. **Standalone Installer (`tools/setup/`)**: Single-file one-click installer (`LikhiSetup.exe`).
  6. **Update Service (`engine/src/update/`)**: Continuous improvement update connection via GitHub Releases and GetLikhi.com.

---

## 2. P0 Keyboard Passthrough & Native Integrity Rules
- `Ctrl+V` **MUST PASTE** natively without ever emitting the character `ভ`.
- `Ctrl+C`, `Ctrl+X`, `Ctrl+Z`, `Ctrl+A`, `Ctrl+S`, `Ctrl+F` pass through unaltered.
- Function keys `F1`–`F12` and Numpad digits/operators pass through untouched.
- English typing is never intercepted unless Bengali (0x0845) is actively selected.

---

## 3. Dual Input Backend Architecture
TSF cannot reach every Windows host, so Likhi has a second, explicit input path. Both share ONE language engine, dictionary, fuzzy matcher, personal dictionary and learning file.

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

### 3.1 Universal Mode Rules
- Nothing is transformed unless the focused window has Bengali (0x0845) selected.
- Automatic mode stands down whenever `bangla_tsf.dll` is loaded in the focused process.
- No typed text is logged. `universal.log` holds mode changes and counters only.

---

## 4. Update Connection & Continuous Improvement System

### 4.1 Purpose & Core Requirement
Installed Likhi applications maintain a lightweight, privacy-conscious update connection to official releases so users receive typing improvements, bug fixes, and security patches without manual intervention.

### 4.2 Architecture & Source of Truth
- **Technical Source of Truth**: GitHub Releases API (`https://api.github.com/repos/Badboy-collab/likhi/releases/latest`).
- **Official User-Facing Portal**: `https://getlikhi.com/update/`.
- **In-Process IME Isolation**: `bangla_tsf.dll` never performs network update checks inside client host processes (Notepad, Word, Chrome).
- **Background Startup Check**: `likhi_universal.exe` executes a delayed check (15 seconds after boot) throttled to once every 24 hours.
- **Manual On-Demand Check**: `bangla_settings.exe` (About section) provides immediate user-initiated update verification.

### 4.3 Offline-First Behavior
- WinHTTP timeouts are capped at 3000ms.
- If network is unavailable, update checks silently abort with zero UI popups, zero error dialogs, and zero impact on typing latency.

### 4.4 User Control & Anti-Nagging
- Updates are strictly voluntary: `[ Update Now ]` or `[ Later ]`.
- Selecting `Later` snoozes reminders for that specific version for 7 days.

### 4.5 100% User Data Preservation Guarantee
- User files in `%APPDATA%\PC-Bangla-Typing-App\` (`settings.json`, `user_dict.txt`, `personal_dict.txt`, `voice_config.json`, `user_learning.db`) are **never deleted or overwritten** during updates.
- In-place updates replace only binary files in `%ProgramFiles%\Likhi\`.

### 4.6 Cryptographic Security Verification
- Release assets are cryptographically verified against official SHA-256 digests using Win32 CryptoAPI (`CALG_SHA_256`).
- Tampered or mismatched binaries are strictly blocked from installation.
