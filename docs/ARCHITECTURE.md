# System Architecture: PC Bangla Typing App

## 1. Architectural Principles & Decoupling Strategy

The system is strictly divided into two primary subsystems:
1. **Core Language Engine (Platform-Independent)**: A pure, native C++20 / C-ABI library that handles phonetic transliteration, Trie lexicon lookups, contextual N-gram ranking, personal dictionary management, and Auto-Correct confidence scoring. It has **zero dependencies on Windows OS or TSF APIs**, allowing it to be compiled, unit-tested, and benchmarked directly via standalone CLI tools.
2. **Windows Platform & IME Subsystem**: The OS integration layer containing the Windows Text Services Framework (TSF) COM InProcServer DLL and the DirectWrite floating suggestion overlay window.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    Subsystem A: Windows Platform & IME                      │
│                                                                             │
│  ┌─────────────────────────────────┐   ┌─────────────────────────────────┐  │
│  │   Windows TSF COM Provider      │   │   DirectWrite Suggestion UI     │  │
│  │   (ITfTextInputProcessorEx,     │   │   (Lightweight Win32 popup,     │  │
│  │    ITfKeyEventSink, TSF Comps)  │   │    DirectWrite OpenType fonts)  │  │
│  └────────────────┬────────────────┘   └────────────────▲────────────────┘  │
└───────────────────┼─────────────────────────────────────┼───────────────────┘
                    │ (Keystrokes / Roman buffer)         │ (Ranked Candidates)
                    ▼                                     │
┌─────────────────────────────────────────────────────────┴───────────────────┐
│                    Subsystem B: Core Language Engine (Pure C-ABI)           │
│                                                                             │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │ 1. Composition Buffer & State Manager                                 │  │
│  │    • Tracks Roman tokens, caret offset, sentence context history      │  │
│  └──────────────────────────────────┬────────────────────────────────────┘  │
│                                     ▼                                       │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │ 2. Phonetic Transliteration & Unicode Parser                          │  │
│  │    • Multi-candidate beam search graph (Banglish → Bengali phonemes)  │  │
│  │    • Full Bengali Unicode orthography (Swaraborno, Byanjon, Kar, Jukt)│  │
│  └──────────────────────────────────┬────────────────────────────────────┘  │
│                                     ▼                                       │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │ 3. Lexicon Candidate Generator & Trie Traversal                       │  │
│  │    • Memory-mapped Binary Compressed Trie (Double-Array / Radix Trie) │  │
│  │    • Prefix searches & phonetic distance variation generator          │  │
│  └──────────────────────────────────┬────────────────────────────────────┘  │
│                                     ▼                                       │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │ 4. Contextual Scorer, Ranking & Personal Dictionary                   │  │
│  │    • Log-linear model: PhoneticSim + log(Unigram) + log(Bigram)       │  │
│  │    • Local User Dictionary boost (SQLite / flat store in %APPDATA%)   │  │
│  └──────────────────────────────────┬────────────────────────────────────┘  │
│                                     ▼                                       │
│  ┌───────────────────────────────────────────────────────────────────────┐  │
│  │ 5. Auto-Correct Evaluator (Strict ON/OFF Gate)                        │  │
│  │    • Evaluates confidence margin: (S_top - S_next > threshold)        │  │
│  │    • Passes raw suggestions if OFF or below threshold                 │  │
│  └──────────────────────────────────┬────────────────────────────────────┘  │
│                                     ▼                                       │
│                          Ranked Candidates [1..5]                           │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Decoupled Development & Verification Flow

Development proceeds in two strictly sequenced stages:

### Stage 1: Core Engine & CLI Harness (Independent of Windows TSF)
```
[Banglish Test Corpus]
         │
         ▼
[Standalone CLI Harness / Unit Tests] ──▶ [Core Language Engine (C-ABI)] ──▶ [Benchmark Metrics Report]
                                                                              (Accuracy, Latency, Memory)
```
- The entire language engine is tested and proven in isolation.
- Unit tests verify Unicode combining rules, Kar/Fala/Juktakkhor handling, Auto-Correct ON/OFF guarantees, and personal dictionary operations.
- Benchmarks measure Top-1/Top-3/Top-5 accuracy and latency per keystroke.

### Stage 2: Windows TSF & Suggestion UI Integration
```
[Target Windows App] ──▶ [Windows TSF DLL] ──▶ [Core Language Engine] ──▶ [DirectWrite Suggestion UI]
```
- Only after the core engine passes all benchmark accuracy and latency targets is the native Windows TSF COM DLL integrated.

---

## 3. Detailed Component Breakdown

### 3.1 Core Language Engine Interfaces (`engine/include/bangla_engine.h`)
The core engine exposes a clean, deterministic C-ABI:

```cpp
// Core Engine Opaque Handle
typedef struct BanglaEngine BanglaEngine;

// Configuration options
typedef struct {
    bool auto_correct_enabled;
    float auto_correct_threshold;
    uint32_t max_candidates;
    const char* user_dict_path;
    const char* lexicon_binary_path;
} EngineConfig;

// Candidate representation
typedef struct {
    char bengali_text[64];       // UTF-8 encoded Bengali string
    float score;                 // Combined ranking score
    uint32_t category_flags;     // Transliteration, Variation, Context, Personal
    bool auto_correct_recommended;
} SuggestionCandidate;

// Candidate list container
typedef struct {
    SuggestionCandidate candidates[8];
    uint32_t count;
} CandidateList;

// Lifecycle & Processing API
BanglaEngine* BanglaEngine_Create(const EngineConfig* config);
void BanglaEngine_Destroy(BanglaEngine* engine);

// Input processing
void BanglaEngine_ResetComposition(BanglaEngine* engine);
void BanglaEngine_AppendChar(BanglaEngine* engine, char ch);
void BanglaEngine_DeleteChar(BanglaEngine* engine);
void BanglaEngine_SetComposition(BanglaEngine* engine, const char* roman_buffer);
void BanglaEngine_CommitWord(BanglaEngine* engine, const char* bengali_word);

// Candidate retrieval
void BanglaEngine_GetCandidates(BanglaEngine* engine, CandidateList* out_list);

// Personal dictionary
bool BanglaEngine_AddUserWord(BanglaEngine* engine, const char* roman_key, const char* bengali_word);
bool BanglaEngine_RemoveUserWord(BanglaEngine* engine, const char* roman_key, const char* bengali_word);
```

### 3.2 Lexicon & Trie Subsystem (`engine/src/dictionary/`)
- **Binary Compressed Trie Structure**:
  - The static vocabulary (100,000+ Bengali lemmas with unigram frequencies) is compiled offline into an immutable binary file (`lexicon.bin`).
  - Implemented as a Double-Array Trie or Radix Trie with compact index tables.
  - Prefix searches are performed directly in memory in $O(L)$ time where $L$ is the length of the prefix.

### 3.3 Contextual Language Model (`engine/src/ranking/`)
- Uses log-linear interpolation of:
  - **Phonetic Match Confidence**: Distance between the phonetic parse and the candidate word.
  - **Unigram Frequency**: Base lexical probability $\log P(w)$.
  - **Bigram Transition Probability**: Preceding word transition $\log P(w \mid w_{prev})$.
  - **User History**: Recency and frequency score from the local SQLite personal store.

### 3.4 Auto-Correct Confidence Evaluator (`engine/src/autocorrect/`)
- Evaluates candidate distribution:
  $$\text{AutoCorrectAllowed} = (\text{Config.AutoCorrectEnabled} == \text{true}) \land (S_1 - S_2 \ge \delta_{\text{margin}}) \land (S_1 \ge \theta_{\text{threshold}})$$
- If `AutoCorrectEnabled == false`, `auto_correct_recommended` is **always false**.

### 3.5 Windows TSF Integration Subsystem (`windows/tsf/`)
- Implements:
  - `ITfTextInputProcessorEx`
  - `ITfKeyEventSink`
  - `ITfCompositionSink`
  - `ITfContextOwnerCompositionSink`
- Directly bridges Windows composition events to the `BanglaEngine` C-ABI.
