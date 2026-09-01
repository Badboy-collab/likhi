# Testing & Benchmark Methodology: PC Bangla Typing App

## 1. Testing Philosophy & Separation of Concerns
Testing is divided into two decoupled stages:
1. **Core Language Engine Testing (Standalone)**: Automated unit tests and CLI benchmark runners verifying phonetic parsing, candidate generation, N-gram ranking, personal dictionary CRUD, and Auto-Correct logic on arbitrary machines without Windows TSF.
2. **Windows Platform & IME Testing**: Dedicated integration tests verifying TSF composition lifecycle, caret positioning, key interception, and DirectWrite rendering.

---

## 2. Automated Test Categories

### 2.1 Unicode Orthography & Grammar Unit Tests (`tests/unit/test_unicode.cpp`)
- **Independent Vowels (Swaraborno)**: অ, আ, ই, ঈ, উ, ঊ, ঋ, এ, ঐ, ও, ঔ.
- **Consonants & Inherent Vowels**: Verification of default inherent `অ` vs explicit Kar signs (`k` → `ক`, `ka` → `কা`, `ki` → `কি`, `ke` → `কে`, `ko` → `কো`).
- **Conjunct Formation (Juktakkhor)**:
  - Consonant clusters with automatic Hasant injection (`kto` → `ক্ত`, `kkh` → `ক্ষ`, `ggo` → `জ্ঞ`, `bd` → `ব্দ`, `shch` → `শ্চ`).
  - Ref (`r` + consonant → `র্`) and Ra-fala (consonant + `r` → `্র`).
  - Ja-fala (`্য`), Ba-fala (`্ব`), Ma-fala (`্ম`).
- **Special Characters & Modifiers**:
  - Khanda-Ta (`ৎ`), Anusvara (`ং`), Visarga (`ঃ`), Chandrabindu (`ঁ`).
  - Bengali punctuation: Dari (`।` / `U+0964`) and Double Dari (`॥` / `U+0965`).
  - Zero-Width Non-Joiner (`ZWNJ`) and Zero-Width Joiner (`ZWJ`).

### 2.2 Auto-Correct Verification (HARD RULE TESTS)
- **Suite A: Auto-Correct = OFF**:
  - For any given input, test that `BanglaEngine_GetCandidates` returns candidate recommendations without modifying the active composition text automatically.
  - Assert that `auto_correct_recommended` is **strictly false** for all inputs when `auto_correct_enabled == false`.
- **Suite B: Auto-Correct = ON**:
  - For unambiguous high-confidence words (e.g. `ami` → `আমি`), assert that `auto_correct_recommended == true` when confidence score $\ge \theta_{\text{threshold}}$ ($0.85$).
  - For ambiguous or low-confidence words (e.g., polysemous prefixes or rare words), assert that `auto_correct_recommended == false` to prevent erroneous automatic replacements.

### 2.3 Buffer & Composition Operations
- Dynamic character appending (`a` → `m` → `i`).
- Backspace character deletion (`ami` + backspace → `am` → updates suggestions to `আম`, `আমার`).
- Caret navigation within composition.
- Cancellation via `Escape`.

### 2.4 Personal Dictionary Operations
- Insertion of new user words with Roman keys.
- Verification that user words receive an adaptive frequency boost without corrupting standard vocabulary ranking.
- Safe deletion and cleanup of personal entries.

---

## 3. Benchmark Suite & Performance Metrics (`tests/benchmark/`)

### 3.1 Measured vs Target Performance Table

| Metric Category | Metric | Baseline Status | Target Goal | Failure Condition |
| :--- | :--- | :--- | :--- | :--- |
| **Accuracy** | Top-1 Candidate Accuracy | *To be measured in Phase 2* | `> 85.0%` | `< 75.0%` |
| **Accuracy** | Top-3 Candidate Accuracy | *To be measured in Phase 2* | `> 95.0%` | `< 90.0%` |
| **Accuracy** | Top-5 Candidate Accuracy | *To be measured in Phase 2* | `> 98.0%` | `< 95.0%` |
| **Accuracy** | Sentence-Level Transliteration Accuracy | *To be measured in Phase 2* | `> 80.0%` | `< 70.0%` |
| **Latency** | Typing-to-Candidate Latency (P50) | *To be measured in Phase 2* | `< 0.3 ms` | `> 1.0 ms` |
| **Latency** | Typing-to-Candidate Latency (P99) | *To be measured in Phase 2* | `< 0.8 ms` | `> 2.0 ms` |
| **Latency** | Candidate List Generation Latency | *To be measured in Phase 2* | `< 0.5 ms` | `> 2.0 ms` |
| **Memory** | Working Set RAM (Idle Engine) | *To be measured in Phase 2* | `< 8 MB` | `> 15 MB` |
| **Memory** | Working Set RAM (Active Typing) | *To be measured in Phase 2* | `< 12 MB` | `> 20 MB` |
| **Size** | Lexicon Binary File Size on Disk | *To be measured in Phase 2* | `< 3.0 MB` | `> 6.0 MB` |

---

## 4. Benchmark Test Datasets (`tests/data/`)

1. `tests/data/basic_words.json`: Core high-frequency Bengali vocabulary (manually authored test set).
2. `tests/data/sentence_corpus.json`: Full sentence-level Banglish inputs with expected contextual Bengali outputs (manually authored test set).
3. `tests/data/variations.json`: Spelling variations mapping to identical canonical forms (manually authored test set).
4. `tests/data/unicode_cases.json`: Complex conjuncts, ligatures, and edge cases (manually authored test set).
5. `tests/data/corpus_metadata.json`: Provenance and licensing registry for all test datasets.
