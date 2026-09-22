# Likhi (লিখি)

> **“বাংলা লিখুন, সহজেই।”**

A modern, fast, intelligent, and lightweight Bengali phonetic typing application and Windows Text Services Framework (TSF) IME for Windows.

- **App Name**: লিখি
- **English Name**: Likhi
- **Display Name**: Likhi (লিখি)
- **Tagline**: *“বাংলা লিখুন, সহজেই।”*

## Key Features

- **Windows TSF IME (`bangla_tsf.dll` & `bangla_tsf32.dll`)**: Native 64-bit and 32-bit in-process COM Text Service supporting all Windows applications (WhatsApp Desktop, Notepad, MS Word, Edge, Chrome, VS Code).
- **Likhi Virtual Keyboard (`likhi_virtual_keyboard.exe`)**: Modern Acrylic Glass on-screen keyboard for easy mouse click typing with zero focus loss (`WS_EX_NOACTIVATE`).
- **Voice Typing Integration**: Direct shortcut trigger (`Win + H`) and in-app voice guide for Windows native speech typing.
- **Dynamic Dark & Light Mode**: Real-time theme synchronization across Settings and candidate suggestion window.
- **Sub-Millisecond Speed**: ~19 µs keystroke transliteration, 1.4 µs next-word prediction, ~3.8 ms cold startup.
- **Ultra-Lightweight Memory**: ~13.4 MB active RAM footprint.
- **100% Bengali Unicode Integrity**: Zero broken conjuncts, zero dangling Hasants, full grapheme-aware backspacing.
- **Smart Transliteration & Ranking**: 52,161 word lexicon, bigram context scoring, 98.08% sentence accuracy on 520 held-out gold test cases.
- **Strict Auto-Correct Gate**: Default OFF. When enabled, requires $\ge 0.85$ confidence score.
- **Native Settings App (`bangla_settings.exe`)**: Modern Win32 tool for user preferences, custom personal dictionary, and Windows startup control.
- **One-Click Installer (`Likhi_Setup_v1.0.0.exe`)**: Inno Setup installer with automatic registration, desktop shortcuts, and clean uninstaller.
- **100% Offline & Private**: Zero cloud dependency, zero telemetry, zero network imports, zero keystroke logging.

---

## Quick Start & Build

```powershell
# Build all targets in Release mode
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Register the TSF IME (Administrator required)
regsvr32.exe build/bangla_tsf.dll

# Run automated test suites
./build/unicode_bengali_tests.exe
./build/test_runner.exe
./build/test_tsf_integration.exe
```

---

## Architecture Overview

```
Keyboard Input
      ↓
Windows TSF Input Layer (COM InProcServer DLL)
      ↓
Input Composition Buffer
      ↓
Bengali Transliteration Engine (Phonetic Parser & Unicode Grammar)
      ↓
Candidate Generator (Compact Binary Trie Lexicon & Frequency Data)
      ↓
Context & Linguistic Ranker (Unigram + Bigram Transitions + Personal Dict)
      ↓
Suggestion UI (Ultra-lightweight DirectWrite / Win32 Overlay)
      ↓
Bengali Text Commit to Active Application
```

---

## Verification & Benchmark Metrics (Phase 3 Empirical Results)

| Metric | Measured Result | Target Threshold | Status |
| :--- | :--- | :--- | :--- |
| **Lexicon Entry Count** | **`52,161 words (3.08 MB)`** | `> 50,000 words` | **PASS** |
| **Held-Out Gold Sentence Accuracy** | **`98.08% (510/520)`** | `> 85.0%` | **PASS** |
| **Held-Out Gold Word Accuracy** | **`98.92% (2,098/2,121)`** | `> 95.0%` | **PASS** |
| **Invalid Candidate Rate** | **`0.00% (0/2,121)`** | `< 1.0%` | **PASS (Zero Junk)** |
| **Banglish Variation Accuracy** | **`100.00% (45/45)`** | `> 95.0%` | **PASS** |
| **Average Keystroke Latency** | **`19.34 µs (0.019 ms)`** | `< 300 µs` | **PASS** |
| **P50 (Median) Keystroke Latency** | **`7.50 µs (0.007 ms)`** | `< 200 µs` | **PASS** |
| **P95 Keystroke Latency** | **`85.00 µs (0.085 ms)`** | `< 500 µs` | **PASS** |
| **P99 Keystroke Latency** | **`193.80 µs (0.194 ms)`** | `< 800 µs` | **PASS** |
| **Sentence Transliteration Speed** | **`185.49 µs (0.185 ms)`** | `< 1,000 µs` | **PASS** |
| **Dedicated Unicode Integrity Suite** | **`46/46 Passed (100%)`** | `100.0%` | **PASS** |
| **Engine Unit Tests** | **`197/197 Passed (100%)`** | `100.0%` | **PASS** |

---

## Building & Running

### Prerequisites
- Windows 10 / 11 (x64)
- MinGW-w64 (GCC 14+ supporting C++20) or MSVC 2022
- CMake 3.20+
- Ninja build tool
- Python 3.10+ (for corpus builder and benchmarks)

### Build Steps
```powershell
# 1. Compile Lexicon & Benchmark Datasets
python tools/corpus_builder/build_lexicon.py

# 2. Configure and Build C++ Engine
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# 3. Run Automated Unit & Unicode Tests
./build/unicode_bengali_tests.exe
./build/test_runner.exe

# 4. Run Real-World 520-Sentence Benchmark
python tools/benchmark_runner/benchmark_expanded.py

# 5. Run 500k+ Keystroke Performance Benchmark
./build/benchmark_perf.exe
```

---

## Documentation

- [Product Specification](docs/PRODUCT_SPEC.md)
- [System Architecture](docs/ARCHITECTURE.md)
- [Bengali Language Model & Next-Word Prediction](docs/PHASE3_LANGUAGE_MODEL.md)
- [Real-World Benchmark Evaluation](docs/BENCHMARK.md)
- [Scoring Formula & Auto-Correct Policy](docs/ENGINE_SCORING.md)
- [Bengali Unicode Integrity Specification](docs/UNICODE_BENGALI.md)
- [Data Sources & Provenance](docs/DATA_SOURCES.md)
- [Technology Decisions](docs/TECHNOLOGY_DECISIONS.md)
- [Testing & Quality Assurance](docs/TESTING.md)
- [Privacy Policy](docs/PRIVACY.md)
- [Release Management](docs/RELEASE.md)

---

## License

This project is licensed under the Apache License 2.0. See the [LICENSE](LICENSE) file for details.
