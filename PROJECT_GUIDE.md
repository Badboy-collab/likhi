# 🧭 Likhi (লিখি) — Master Project Guide & Development Roadmap

> **"বাংলা লিখুন, সহজেই।"**  
> **Repository Root**: `E:\Pervez\PC Bangla Typing App\`  
> **Last Updated**: 2026-09-27  

---

## 1. Project Organization: The 3 Main Tracks

Our work across this repository is structured into **3 distinct, independent tracks**:

```
E:\Pervez\PC Bangla Typing App\
│
├── 🖥️ [TRACK 1] LIKHI DESKTOP APP (Windows C++20 Core)
│   ├── engine/          -> Core Bangla Phonetic & Transliteration Engine
│   ├── tsf/             -> Windows Text Services Framework TIP (bangla_tsf.dll)
│   ├── universal/       -> Universal Fallback Typing Injector (likhi_universal.exe)
│   ├── tools/           -> Settings UI (bangla_settings.exe), Setup Builder (LikhiSetup.exe)
│   ├── tests/           -> 462 Automated Unit & Integration Tests (100% Passing)
│   ├── release_package/ -> Standalone Installer & Binary Release Package
│   └── CMakeLists.txt   -> MinGW64 / CMake C++20 Build System
│
├── 🌐 [TRACK 2] LIKHI WEBSITE & WORDPRESS THEME
│   └── website/
│       ├── (Static Site & HTML/JS Assets)
│       └── wordpress-theme/
│           ├── likhi/       -> Active WordPress Theme (Current Version: 1.0.7)
│           ├── likhi.zip    -> Production-Ready Theme ZIP
│           └── inc/         -> Modular Customizer Settings & Helpers
│
└── 📰 [TRACK 3] LIKHI BLOGGER THEME
    └── website/
        └── blogger-theme/
            └── likhi-blogger-theme.xml -> Clean, SEO-Optimized XML Template for Blogger/Blogspot
```

---

## 2. 🖥️ TRACK 1: Likhi Desktop App (Where We Left Off)

### A. Current Status (As of September 22, 2026)
The desktop application is built with modern **C++20** and features a **Dual Input Backend** designed to work everywhere in Windows:

1. **Backend 1: Windows TSF (`bangla_tsf.dll`)**:
   - Registered COM In-Process Server for native Windows Text Services Framework.
   - P0 Keyboard Passthrough verified: `Ctrl+V` pastes cleanly without typing `ভ`; `Ctrl+C/X/A/Z/S`, Function keys `F1-F12`, and Numpad pass through natively.
   - Continuous composition buffer with interactive candidate suggestion bar.
2. **Backend 2: Universal Fallback (`likhi_universal.exe`)**:
   - Low-level keyboard hook + Unicode keystroke injection.
   - Designed for apps where TSF does not reach (Store/UWP apps, legacy Java UIs, remote sessions).
   - Only transforms when Bengali (0x0845) is active; hotkey `Ctrl+Alt+L` to pause/resume.
3. **Core Engine (`engine/`)**:
   - 52,412 word validated Lexicon Trie (`lexicon.bin`).
   - Context-aware bigram ranker and fuzzy Banglish phonetic parser.
   - Personal User Dictionary CRUD support.
4. **Settings Application (`bangla_settings.exe`)**:
   - Modern Win32 Fluent UI sidebar navigation.
5. **Standalone Installer (`LikhiSetup.exe`)**:
   - One-click installer with embedded payload and administrative manifest (17.5 MB).

---

### B. Automated Test Health (462 Tests — 100% Passing)

All automated C++ test suites pass with zero failures:
- **Engine Unit Tests (`build/test_runner.exe`)**: **220 / 220 Passed**
- **Key Policy Regression (`build/test_key_policy.exe`)**: **196 / 196 Passed**
- **Unicode Bengali Integrity (`build/unicode_bengali_tests.exe`)**: **46 / 46 Passed**

---

### C. Where Development Left Off (Immediate Tasks for Desktop App)

According to `docs/LIKHI_MASTER_SPEC.md` (§ 5.3):
1. **Wire `likhi_universal.exe` into Installer**:
   - Ensure `LikhiSetup.exe` installs and configures `likhi_universal.exe` in the startup registry / service.
2. **Add Input Mode Switch in Settings App (`bangla_settings.exe`)**:
   - Add UI toggle between: `Automatic (Recommended)` | `TSF Only` | `Universal Mode`.
3. **Feature Parity**:
   - Surface online/cloud suggestions in the Universal host matching the TSF path.
4. **Testing on Target Windows OS**:
   - Live testing in Windows 10 & 11 across Notepad, Microsoft Word, Chrome, and Store Apps.

---

### D. How to Build the Desktop App

The toolchain is located at `C:\tools\mingw64\bin\`. To build:

```powershell
# 1. Open PowerShell in E:\Pervez\PC Bangla Typing App
cd "E:\Pervez\PC Bangla Typing App"

# 2. Configure build with CMake & Ninja
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# 3. Compile all targets
ninja -C build

# 4. Run automated test suites to verify integrity
& ".\build\test_key_policy.exe"
& ".\build\test_runner.exe"
& ".\build\unicode_bengali_tests.exe"
```

---

## 3. 🌐 TRACK 2: Likhi Website & WordPress Theme (`getlikhi.com`)

### A. What Was Built & Solved
- **Official Landing Page**: Features, live typing simulator, 3D icon, download counters, system telemetry.
- **SEO Dedicated Landing Pages**: `/avro-alternative/`, `/bijoy-alternative/`, `/bangla-typing-software/`, `/why-likhi/`.
- **WordPress Theme Customizer**: 35+ controls in **Appearance → Customize → Likhi Theme Options** (Colors, Fonts, Layout, Header, Footer, Blog elements).
- **Blog Architecture**:
  - Archive: `https://getlikhi.com/blog/` (Page ID 39, `page-blog.php`).
  - Single Posts: `https://getlikhi.com/blog/post-slug/` (v1.0.7).
  - Automatic 301 Redirect from legacy `/post-slug/` to `/blog/post-slug/`.
  - Markdown link converter + database auto-sanitizer.
  - Image alignment system: Center, Left, Right, and Full-width.

### B. Deployment Package
- ZIP File: `E:\Pervez\PC Bangla Typing App\website\wordpress-theme\likhi.zip`
- Upload via: **Appearance → Themes → Add New Theme → Upload Theme** &rarr; **Replace active with uploaded**.

---

## 4. 📰 TRACK 3: Likhi Blogger Theme

- **File Location**: `E:\Pervez\PC Bangla Typing App\website\blogger-theme\likhi-blogger-theme.xml`
- **Purpose**: Ready-to-use Blogger/Blogspot theme for alternative blogs or landing mirror sites.
- **Features**: Responsive design, Banglish typing guides styling, Inter + Hind Siliguri fonts, fast loading.
