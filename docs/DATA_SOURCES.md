# Data Sources, Licensing & Provenance Compliance

## 1. Clean-Room & Legal Compliance Policy

> [!IMPORTANT]
> The **PC Bangla Typing App** strictly complies with clean-room engineering practices:
> - **Zero Proprietary Google / Gboard Data**: No models, APK assets, decompiled bytecodes, proprietary unigram/bigram tables, or internal algorithms from Google/Gboard are used.
> - **Open-Source & Morphological Generation**: Vocabulary and datasets are generated via open-source grammatical inflections, rule-based phonetics, and legally redistributable sources.

---

## 2. Dataset Provenance Registry

| Resource Name | Source / Generator | License / Terms | Redistribution Status | Runtime File |
| :--- | :--- | :--- | :--- | :--- |
| **52,000+ Word Bengali Lexicon** | Morphological Generator (`tools/corpus_builder/build_lexicon.py`) | MIT / CC0 Compatible | Compiled in Binary format | `engine/data/lexicon.bin` |
| **520-Sentence Gold Evaluation Corpus** | Curated & Combinatorial Multi-Domain Benchmark Generator | MIT / Clean-Room | Benchmark Held-Out Set | `tests/data/evaluation/gold_sentences_500.json` |
| **Dedicated Unicode Integrity Test Suite** | Handcrafted Bengali conjunct & grapheme dataset | MIT / Project Owned | Test Data | `tests/data/handcrafted/unicode_bengali.json` |
| **Bigram Transition Model** | Canonical Bengali syntactic pair frequency table | MIT / Clean-Room | Engine Builtin Matrix | `engine/src/ranking/context_ranker.cpp` |
| **Personal Dictionary** | 100% User-Generated Local Storage | User Owned (Private) | Local SQLite / Flat File | Local AppData |

---

## 3. Data Separation & Anti-Overfitting Safeguards

1. **Strict Train / Evaluation Separation**:
   - `tests/data/evaluation/gold_sentences_500.json` is strictly held out and not indexed as unigram entries in the runtime engine.
2. **No Telemetry / Keystroke Logging**:
   - The engine contains no network code, remote API hooks, or background telemetry workers.
