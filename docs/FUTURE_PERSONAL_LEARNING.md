# Likhi (লিখি) — Future Feature Requirement: User Personal Learning

> **STATUS: DEFERRED / FUTURE ROADMAP**  
> **GATE POLICY: EXPLICIT USER APPROVAL REQUIRED BEFORE IMPLEMENTATION**

---

## 1. Core Concept & Vision

Likhi should eventually be able to intelligently adapt to individual user typing habits and vocabulary preferences via **Local-First, Privacy-Preserving Personal Learning**.

---

## 2. Strict Implementation Gate Rule

> [!IMPORTANT]
> **MANDATORY APPROVAL GATE:**  
> When the project roadmap reaches the **Personal Learning / User Typing Pattern** stage:
> 1. **STOP immediately before writing or enabling any personal learning code.**
> 2. Ask the user the exact prompt:
>    > *"Personal Learning feature এখন implement করার অনুমতি দেবেন?"*
> 3. **DO NOT proceed** under any circumstances until the user provides explicit written approval.

---

## 3. Required Future Behavior Specifications

### A. Learning from Suggestion Selections
- When a user types a phonetic string (e.g. `anwar`) and chooses a specific candidate (e.g. `আনোয়ার` over `anwar`), the engine records the preference:
  $$\text{input: } \texttt{"anwar"} \longrightarrow \text{selected: } \texttt{"আনোয়ার"}$$
- Subsequent inputs of `"anwar"` will prioritize `"আনোয়ার"` with highest ranking.

### B. Frequency-Weighted Repeated Preferences
- Dynamic frequency scoring based on user selections:
  $$\text{Score}(\text{candidate}) \propto \text{SelectionCount}(\text{candidate})$$
- Example:
  - `"anwar" \to \text{"আনোয়ার"}` (selected 20 times)
  - `"anwar" \to \text{"anwar"}` (selected 2 times)
  - Result: `"আনোয়ার"` maintains top ranking.

### C. Contextual N-Gram Learning
- If a user repeatedly types a phrase or context pair:
  $$\text{Context: } \texttt{"আমি"} + \text{Input: } \texttt{"office"} \longrightarrow \text{Selected: } \texttt{"অফিসে"}$$
- The contextual bigram ranker boosts the learned candidate in that specific context.

### D. Privacy-Preserving Architecture (Zero Keystroke Logging)
- **Hard Rule**: Never blindly store arbitrary keystrokes, passwords, or continuous body text.
- Only record structured statistical tuples:
  $$\langle \text{roman\_input}, \text{bengali\_word}, \text{count}, \text{last\_used\_timestamp} \rangle$$
- 100% offline, stored in local SQLite/binary `%APPDATA%\PC-Bangla-Typing-App\user_learning.db`.
- Zero cloud transmission, zero analytics, zero telemetry.

### E. User Controls & Settings Integration
The Settings Application must provide:
1. **Personal Learning Toggle** (ON / OFF, default OFF).
2. **View Learned Words**: Live inspection of recorded preferences.
3. **Clear Learned Data**: Instant purge of all dynamic learned frequencies.
4. **Reset Personalization**: Restore engine ranking to clean factory defaults.
