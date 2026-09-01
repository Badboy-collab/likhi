# Technology Decisions Record: PC Bangla Typing App

## 1. Product Vision & Gboard Clarification

### 1.1 Gboard-Quality UX vs Clean-Room Implementation
Our objective is to deliver a **Gboard-quality Bengali typing experience** on Windows: fast, fluid, natural phonetic input with intelligent candidates, context awareness, and error tolerance.

**CRITICAL PRINCIPLE**:
- We are **NOT** cloning Gboard.
- We do **NOT** have access to, nor are we attempting to reverse-engineer, extract, download, or reuse Google's proprietary Gboard code, APK components, proprietary language models, internal weights, or closed datasets.
- Instead, our system is an **independent, clean-room implementation** based on publicly observable smart keyboard principles and standard, well-documented computer science and computational linguistics methods:
  - Finite-state phonetic transliteration graphs.
  - Prefix-tree (Trie / DAWG) lexicon lookups.
  - Statistical N-gram language modeling (Unigram / Bigram transition matrices).
  - Levenshtein / Damerau-Levenshtein phonetic distance matching.
  - Windows Text Services Framework (TSF) COM architecture.

---

## 2. Technology Comparison & Evaluation

### 2.1 Core Language Engine: C++20 vs Rust

| Evaluation Criteria | Modern C++20 | Rust |
| :--- | :--- | :--- |
| **Execution Latency** | Microsecond level (< 0.1 ms per keystroke) | Microsecond level (< 0.1 ms per keystroke) |
| **Memory Overhead** | Minimal (< 10 MB total footprint) | Minimal (< 12 MB total footprint) |
| **Binary Size** | Compact static binary (< 500 KB) | Compact static binary (< 1.5 MB) |
| **Windows TSF & COM Interop** | **First-class native support** via Windows SDK (`msctf.h`, `wrl`, `wil`, C++ COM vtables). | Requires `windows-rs` or C-ABI bridge; COM boilerplate in Rust is more verbose. |
| **Memory Safety** | Manual (requires RAII, smart pointers, bounds checking). | Compiler-enforced borrow checker & memory safety. |
| **Toolchain Complexity** | Standard MSVC / Clang toolchain. | Requires `rustup` and MSVC C++ build tools for linking. |
| **Decision** | **SELECTED FOR CORE ENGINE & TSF INTEGRATION** | *Viable alternative; C++20 selected for seamless Windows COM/TSF and DirectWrite integration.* |

---

### 2.2 Windows Input Integration: TSF vs Other Microsoft Mechanisms

| Mechanism | Architecture & Support | Limitations / Risks | Decision |
| :--- | :--- | :--- | :--- |
| **Windows Text Services Framework (TSF)** | Modern COM-based input architecture supported across all Windows 10/11 apps, UWP/WinUI app containers, MS Office, Chromium/Edge, Notepad, and Win32 edit controls. | Requires in-process COM InProcServer DLL implementation. | **SELECTED (Official Standard)** |
| **Legacy IMM32 (Input Method Manager)** | Deprecated Win32 IME API (pre-Windows XP/Vista). | Fails in modern Windows 10/11 app sandboxes (Store apps, Edge, modern Office). Obsolete. | **REJECTED** |
| **Global Low-Level Hook (`WH_KEYBOARD_LL`) + `SendInput`** | User-mode keyboard message hook. | Causes input lag, blocked by game anti-cheat/security software, fails to support inline composition strings, cannot locate caret for suggestions. | **REJECTED** |

---

### 2.3 Lexicon & Trie Data Structure

| Data Structure | Lookup Latency | Memory & Compression | Dynamic Modifications | Decision |
| :--- | :--- | :--- | :--- | :--- |
| **Binary Double-Array / Radix Trie (Custom)** | `< 0.1 ms` for exact prefix lookup | High compression (**< 3 MB** for 100,000 words + unigrams). Memory-mappable. | Static / Read-only at runtime (built offline). | **SELECTED FOR MAIN LEXICON** |
| **DAWG (Directed Acyclic Word Graph)** | `< 0.15 ms` | Maximum compression (minimal node count). | Static / Read-only. Slightly more complex to attach frequency payloads to shared suffixes. | *Alternative to Trie* |
| **MARISA Trie** | `< 0.2 ms` | Excellent compression ratio. | Static / Read-only. External C++ dependency required. | *Evaluated; custom compact trie preferred for zero external dependency.* |
| **SQLite (for Main Lexicon)** | `1.0 - 5.0 ms` | High disk and memory overhead (30MB+). | Excellent for dynamic CRUD. | **REJECTED for Main Lexicon** (Used ONLY for User Dictionary) |
| **Plain Text / Hashmap (In-Memory)** | `< 0.05 ms` | Poor compression (25-50 MB RAM for 100K words). | Easy to modify in RAM. | **REJECTED** |

---

### 2.4 Context & Language Model

| Approach | Latency | Memory / Model Size | Context Capability | Feasibility for Windows IME | Decision |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Smoothed N-Gram (Unigram + Bigram)** | `< 0.2 ms` | `< 3 MB` compressed binary | Evaluates transition probability from preceding word (`ami ajke office [e]` → `অফিসে`). | Excellent; ultra-fast, offline, deterministic, zero GPU/battery drain. | **SELECTED** |
| **Higher-Order N-Gram (Trigram / 4-Gram)** | `< 0.5 ms` | `15 - 50 MB` | Better multi-word phrasing context. | Good; can be added in later optimization phases if memory targets allow. | *Deferred to Phase 3* |
| **Compact Neural Model (RNN / Transformer / ONNX)** | `20 - 80 ms` | `50 - 250 MB` | Deep sentence understanding. | **UNACCEPTABLE FOR SYSTEM IME**: Severe input lag per keystroke, heavy background CPU/RAM usage. | **REJECTED** |

---

### 2.5 Role of Python

- **Permitted Use**:
  - Offline dataset extraction, cleaning, and vocabulary compilation.
  - Binary Trie and N-gram database generation tools (`tools/corpus_builder/`).
  - Automated benchmark test runners and statistical analysis scripts (`tools/benchmark_runner/`).
- **Strict Constraint**:
  - **Python will NEVER be required or included in the runtime Windows IME or client installer.** The production application is 100% native C++.

---

## 3. License Verification & Provenance Audit Table

Every library, dataset, and code component evaluated for this project is cataloged with its license terms, commercial usability, redistribution rights, attribution obligations, and implementation decision:

| Resource | Purpose | Source | Authoritative License | Commercial Use | Redistribution | Attribution Required | Decision |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Custom Phonetic Rules** | Banglish → Bengali phonetic parsing graph | Original clean-room authored code | Apache-2.0 / Proprietary to Project | Yes | Yes | No | **APPROVED (100% Original Code)** |
| **Double-Array / Radix Trie Engine** | Fast prefix search and lexicon storage | Original clean-room C++20 implementation | Apache-2.0 / Project | Yes | Yes | No | **APPROVED (100% Original Code)** |
| **Synthetic Test Corpus (`tests/data/`)** | Initial benchmark & verification suite | Handcrafted test cases by project team | CC0-1.0 / Public Domain | Yes | Yes | No | **APPROVED (Manually Authored)** |
| **OpenBengali / Ankur Lexicon Lists** | Base Bengali word frequency & vocabulary data | Processed open wordlists (Ankur / Mozilla Bengali) | LGPL-2.1+ / MPL-2.0 / CC-BY-SA | Yes (via data extraction of unigram facts) | Permitted with attribution | Yes (in `NOTICE.txt`) | **APPROVED FOR COMPILATION (Derived Statistical Facts)** |
| **SQLite3 (Amalgamation)** | Local User Personal Dictionary storage | sqlite.org | Public Domain (Dedicated) | Yes | Yes | No | **APPROVED** |
| **Windows SDK (`msctf.h`, `dwrite.h`)** | Windows TSF COM and DirectWrite APIs | Microsoft Corporation | Microsoft SDK EULA | Yes | Yes (compiled binaries) | Standard MS terms | **APPROVED (Standard Windows Platform SDK)** |
| **Proprietary Gboard Assets** | N/A | Google LLC | Proprietary | **PROHIBITED** | **PROHIBITED** | N/A | **STRICTLY REJECTED & EXCLUDED** |

---

## 4. Final Toolchain & Technology Recommendation

For the **Phase 1 Standalone Engine Prototype**, we recommend:
1. **Primary Toolchain**: **Modern C++20** using **Clang / LLVM (`clang++`)** and/or **MSVC C++ Build Tools**, managed via standard **CMake**.
2. **Offline Data Tooling**: **Python 3.12** for dataset compilation and benchmark script generation.
3. **Phase 1 Validation Goal**: Prove that the standalone C++20 engine processes Roman phonetic inputs into accurately ranked Bengali candidates and passes all Unicode and Auto-Correct unit tests with **zero Windows TSF dependency**.
