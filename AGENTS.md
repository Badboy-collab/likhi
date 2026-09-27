# 🤖 Likhi Agent & Architecture Master Guide

> **Repository Root**: `E:\Pervez\PC Bangla Typing App\`  
> **Toolchain**: MinGW64 GCC 14.2 / CMake 3.30.4 / Ninja (`C:\tools\mingw64\bin\`)  
> **Target OS**: Windows 10 & 11 (x64 / x86)

---

## 1. System Architecture Overview

```
E:\Pervez\PC Bangla Typing App\
├── engine/          -> Core Bangla Phonetic Parser, 52k Lexicon Trie, Context Ranker
│   ├── include/     -> bangla_engine.h, likhi_version.h, update_service.h
│   └── src/         -> unicode, transliteration, dictionary, ranking, update
├── tsf/             -> Windows Text Services Framework In-Process TIP (bangla_tsf.dll)
├── universal/       -> Non-TSF Universal Typing Core (likhi_universal_core)
├── tools/           -> Settings App (bangla_settings.exe), Setup (LikhiSetup.exe), Universal Host (likhi_universal.exe)
├── tests/           -> 501 Automated Unit & Integration Tests (100% Passing)
└── website/         -> WordPress Theme (likhi/) & Static Site
```

---

## 2. Update Connection & Continuous Improvement System

### Core Invariants
1. **Zero IME Pollution**:
   `bangla_tsf.dll` is an in-process input method DLL running inside client apps (Notepad, Word, Chrome). It **never** executes network requests for update checks.
2. **Background Throttled Checking**:
   `likhi_universal.exe` runs in the user session. It executes a delayed update check 15 seconds after launch, throttled to at most once per 24 hours via `%APPDATA%\PC-Bangla-Typing-App\update_cache.json`.
3. **Manual Check & User Dialog**:
   `bangla_settings.exe` (About section) runs asynchronous on-demand checks using WinHTTP and displays the update dialog with `[ Update Now ]` and `[ Later ]`.
4. **Official Portals**:
   - Technical source of truth: GitHub Releases API (`Badboy-collab/likhi`).
   - Official update page: `https://getlikhi.com/update/`.
5. **100% User Data Protection**:
   `%APPDATA%\PC-Bangla-Typing-App\` user files (`settings.json`, `user_dict.txt`, `personal_dict.txt`, `user_learning.db`) are strictly preserved across installations and updates.
6. **Offline Resilient**:
   If network is unavailable or times out (>3000ms), Likhi silently continues typing with zero errors.

---

## 3. Build & Test Commands

```powershell
# Configure & compile with Ninja
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build

# Execute automated regression test suites (501 tests)
& ".\build\test_update_service.exe"
& ".\build\test_key_policy.exe"
& ".\build\test_runner.exe"
& ".\build\unicode_bengali_tests.exe"
```
