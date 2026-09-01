# Product Specification: PC Bangla Typing App

## 1. Vision & Core Principles
**PC Bangla Typing App** is an independent, lightweight, fast, and intelligent Bengali phonetic input system for Microsoft Windows.

### 1.1 Core Tenets
1. **Natural Phonetic Input**: Users type Romanized Bengali (Banglish) naturally (e.g., `ami ajke office e jabo` → `আমি আজকে অফিসে যাব`) without memorizing cumbersome legacy keyboard layouts.
2. **Tolerance to Phonetic Variations**: Intelligently handles common spelling variants (e.g., `ami`, `aami`, `amii` → `আমি`).
3. **Decoupled Architecture**: The core language and transliteration engine is completely decoupled from the Windows OS layer, allowing exhaustive standalone CLI testing and benchmarking before IME integration.
4. **Intelligent Suggestions**: Combines phonetic similarity, dictionary frequency, and N-gram preceding-word context to rank 3–5 candidates.
5. **Strict Auto-Correct Control (HARD RULE)**:
   - **Auto Correct = OFF**: The engine **MUST NEVER** automatically modify, replace, or substitute user text. Suggestions are displayed in the candidate toolbar for explicit user selection only.
   - **Auto Correct = ON**: The engine may automatically apply high-confidence corrections only when candidate confidence exceeds a strict mathematical threshold ($\theta \ge 0.85$).
6. **Privacy & Offline First**: 100% offline processing. Zero telemetry, zero keystroke logging, and local user dictionary storage under user profile directories.
7. **Clean-Room Engineering**: 100% original code and verified open-source resources. No reverse engineering or proprietary assets.

---

## 2. Performance: Measured vs Target

All performance numbers in this specification are **design targets/hypotheses** established for benchmarking and optimization. They will be validated against measurable benchmarks in Phase 2.

| Metric | Status | Target / Hypothesis | Maximum Acceptable Threshold | Validation Method |
| :--- | :--- | :--- | :--- | :--- |
| **Typing-to-Candidate Latency** | *Target (Unverified)* | `< 0.5 ms` per keystroke | `< 2.0 ms` | Micro-benchmark timer over 10,000 keystrokes |
| **Suggestion Generation Latency** | *Target (Unverified)* | `< 1.0 ms` per word | `< 3.0 ms` | Word-level latency benchmark harness |
| **Idle Memory Footprint** | *Target (Unverified)* | `< 8 MB RAM` | `< 15 MB RAM` | Windows Performance Monitor / Task Manager |
| **Active Typing Memory Footprint** | *Target (Unverified)* | `< 12 MB RAM` | `< 20 MB RAM` | Working Set memory inspection under load |
| **Cold Startup Latency** | *Target (Unverified)* | `< 50 ms` | `< 150 ms` | High-resolution clock on process attach |
| **Lexicon Binary Storage** | *Target (Unverified)* | `< 3.0 MB` (100K words) | `< 6.0 MB` | Compiled binary file size on disk |
| **Total Installer Size** | *Target (Unverified)* | `< 6.0 MB` | `< 12.0 MB` | Final installer package size |
| **Top-1 Suggestion Accuracy** | *Target (Unverified)* | `> 85%` on common corpus | `> 75%` | Automated evaluation against benchmark corpus |
| **Top-3 Suggestion Accuracy** | *Target (Unverified)* | `> 95%` on common corpus | `> 90%` | Automated evaluation against benchmark corpus |

---

## 3. Functional Requirements

### 3.1 Phonetic Transliteration & Unicode Handling
- **Bengali Unicode Range (`U+0980` to `U+09FF`)**:
  - Independent Vowels (স্বাধীন স্বরবর্ণ): অ, আ, ই, ঈ, উ, ঊ, ঋ, এ, ঐ, ও, ঔ.
  - Consonants (ব্যঞ্জনবর্ণ): ক, খ, গ, ঘ, ঙ, চ, ছ, জ, ঝ, ঞ, ট, ঠ, ড, ঢ, ণ, ত, থ, দ, ধ, ন, প, ফ, ব, ভ, ম, য, র, ল, শ, ষ, স, হ, ড়, ঢ়, য়, ৎ, ং, ঃ, ঁ.
  - Dependent Vowel Signs / Kar Symbols (কার চিহ্ন): া, ি, ী, ু, ূ, ৃ, ে, ৈ, ো, ৌ.
  - Conjuncts & Ligatures (যুক্তাক্ষর): ক্ত, ক্স, ক্ষ, জ্ঞ, ঞ্চ, ঞ্জ, ঙ্ক, ঙ্গ, ঙ্ঘ, ত্ত, ত্র, দ্ধ, দ্ব, ন্ত, ন্দ, ম্প, ম্ব, ষ্ট, ষ্ঠ, স্ন, স্প, স্ফ, শ্র, ইত্যাদি.
  - Modifiers & Special Characters: Hasant/Virama (`্` / `U+09CD`), Khanda-Ta (`ৎ` / `U+09CE`), Anusvara (`ং` / `U+0982`), Visarga (`ঃ` / `U+0983`), Chandrabindu (`ঁ` / `U+0981`), Bengali Dari (`।` / `U+0964`), Double Dari (`॥` / `U+0965`).
  - Zero-Width Controls: ZWNJ (`U+200C`) and ZWJ (`U+200D`) for explicit ligatures and disjointed forms.
  - Numerals: Bengali digits (০, ১, ২, ৩, ৪, ৫, ৬, ৭, ৮, ৯) with optional automatic digit mapping.

### 3.2 Suggestion & Candidate System
- Generates **3 to 5 ranked candidates** for any active phonetic composition.
- Multi-tier candidate generation:
  1. **Primary Transliteration**: Exact phonetic parse match.
  2. **Spelling Variations**: Phonetic fuzzy variants (e.g., handling missing or extra vowels).
  3. **Contextually Likely Words**: Reranked candidates using N-gram transition probabilities from preceding committed words in the current sentence.
  4. **Personal Vocabulary**: Custom words and names from the user dictionary.
- **Scoring & Ranking Formula**:
  $$\text{Score}(W) = w_1 \cdot \text{PhoneticMatch}(R, W) + w_2 \cdot \log(\text{UnigramFreq}(W)) + w_3 \cdot \log(\text{BigramProb}(W \mid W_{prev})) + w_4 \cdot \text{UserPersonalBoost}(W)$$

### 3.3 Auto-Correct Layer (Strict Requirement)
- User setting: **Auto Correct (ON / OFF)**.
- **Behavior when OFF**:
  - Raw phonetic input is mapped to suggestions in the candidate bar.
  - The text buffer commits only the user's explicitly selected candidate or the top candidate on Space/Enter (depending on commit mode).
  - The engine **NEVER** silently alters or replaces committed user text.
- **Behavior when ON**:
  - Applies automated correction only when the top candidate's confidence metric strictly exceeds $\theta_{\text{autocorrect}} \ge 0.85$ AND has a significant margin over the second candidate.
  - Ambiguous or low-confidence words remain in the suggestion toolbar without force-committing.

### 3.4 Personal User Dictionary
- Local storage in `%APPDATA%\PC-Bangla-Typing-App\user_dict.sqlite`.
- Stores user-added words, proper nouns, abbreviations, and dialectal terms.
- Implements recency (MRU) and frequency (MFR) weighting to boost user-preferred words without breaking standard vocabulary ranking.
- Full CRUD support: Add, edit, search, and delete entries via Settings UI.

### 3.5 System-Wide Input Integration (Windows TSF)
- Implements Microsoft Windows **Text Services Framework (TSF)** via a native in-process COM DLL.
- Intercepts and processes keystrokes in all standard Windows applications (Notepad, MS Office, Chrome/Edge/Firefox, VS Code, Slack, WhatsApp).
- Seamless composition editing:
  - `Backspace`: Deletes the last Roman character and recomputes suggestions.
  - `Left / Right Arrow`: Navigates within the active composition.
  - `Number Keys (1..5)` or `Space / Tab / Enter`: Commits selected candidate.
  - `Escape`: Reverts composition to raw Latin characters.
  - `F12` or `Ctrl + Space`: Global hotkey to toggle between Bengali and English input modes.

### 3.6 User Personal Learning (Future Requirement & Approval Gate)
- **Status**: Deferred / Future Feature.
- **Specification Document**: [FUTURE_PERSONAL_LEARNING.md](file:///e:/Pervez/PC%20Bangla%20Typing%20App/docs/FUTURE_PERSONAL_LEARNING.md)
- **Strict Implementation Gate**: DO NOT implement or activate without asking and receiving explicit written user permission first (`"Personal Learning feature এখন implement করার অনুমতি দেবেন?"`).
- **Core Requirements**:
  - Learn repeated user candidate selections (e.g., `anwar` $\to$ `আনোয়ার`).
  - Learn context-aware selection preferences (e.g., `আমি office` $\to$ `অফিসে`).
  - Privacy-First: Zero continuous keystroke logging; only store local selection counts and timestamps.
  - User controls: Enable/Disable, View Learned Words, Clear Learned Data, Reset Personalization.

