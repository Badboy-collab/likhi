# 📜 Likhi (লিখি) — Permanent Project Decisions & Architectural Records

> **“বাংলা লিখুন, সহজেই।”**
> **Product Name**: Likhi — লিখি
> **Developer**: AH Creations
> **Architecture**: Native Windows Text Services Framework (TSF) In-Process COM Server + Independent C++20 Phonetic Engine

---

## 1. Core Architectural Decisions (LOCKED)

### DEC-001: Native Keyboard Event Classification Layer
- **Decision**: The Native Keyboard Event Layer is completely decoupled from the phonetic language engine.
- **Rule**: Modifier combinations (`Ctrl`, `Alt`, `Win`), Function keys (`F1`-`F24`), Numpad keys (`0`-`9`, `+`, `-`, `*`, `/`, `.`), System keys (`PrintScreen`, `Pause`, `Insert`, `CapsLock`), and non-composing navigation keys (`Arrows`, `Home`, `End`, `PageUp`, `PageDown`, `Delete`, `Tab`) **MUST NEVER** enter the Bangla transliteration engine and **MUST ALWAYS** pass through to the host application (`*pfEaten = FALSE`).
- **Status**: **`PERMANENT & LOCKED`**.

### DEC-002: Continuous Composition Buffer (P1)
- **Decision**: The composition buffer maintains the full raw phonetic stream until an explicit boundary occurs (`Space`, `Enter`, punctuation, mouse click, focus loss).
- **Rule**: Keystroke sequences (e.g. `ANO` $\to$ `ANOY` $\to$ `ANOYA` $\to$ `ANOYAR` $\to$ `আনোয়ার`) re-evaluate dynamically as a whole without premature partial commits.
- **Status**: **`PERMANENT & LOCKED`**.

### DEC-003: Clean Input Profile Registration (Bangla Bangladesh Preferred)
- **Decision**: Likhi registers exclusively under `Bangla (Bangladesh)` (`0x0845`).
- **Rule**: Eliminates confusing duplicate profile entries (`0x0445` India and `0x0409` US are removed from default registration). Likhi integrates cleanly with native Windows `Win + Space` language switching without hijacking the shortcut.
- **Status**: **`PERMANENT & LOCKED`**.

### DEC-004: Original English Candidate Preservation
- **Decision**: For mixed-language typing and proper nouns (`Google`, `Facebook`, `Windows`, `YouTube`, `office`, `CPU`, `USB`), the exact ASCII string is always provided as an selectable candidate in the candidate list.
- **Status**: **`PERMANENT & LOCKED`**.

### DEC-005: Auto-Correct Safety Policy
- **Decision**: Auto-Correct is an optional feature and is **OFF** by default. When OFF, suggestions are shown but never force-replace user text. When ON, changes require strict confidence ($\ge 0.85$).
- **Status**: **`PERMANENT & LOCKED`**.

### DEC-006: Personal Learning Implementation Gate (STRICT)
- **Decision**: Personal User Learning / Typing Pattern Tracking is **GATED**.
- **Rule**: **DO NOT** implement or activate Personal Learning without asking the user for explicit permission first:
  > *"Personal Learning feature এখন implement করার অনুমতি দেবেন?"*
- **Status**: **`GATED — WAITING FOR EXPLICIT USER APPROVAL`**.

---

## 2. Decision History & Changelog

| Date | ID | Summary | Author | Status |
| :--- | :--- | :--- | :--- | :--- |
| 2026-09-01 | DEC-001 | P0 Native keyboard event bypass & modifier handling | AH Creations | LOCKED |
| 2026-09-01 | DEC-002 | Continuous dynamic composition buffer (`ANOYAR` $\to$ `আনোয়ার`) | AH Creations | LOCKED |
| 2026-09-01 | DEC-003 | Exclusive `0x0845` profile registration & clean `Win+Space` | AH Creations | LOCKED |
| 2026-09-01 | DEC-004 | Original English candidate preservation in suggestion list | AH Creations | LOCKED |
| 2026-09-01 | DEC-005 | Auto-Correct opt-in policy with 0.85 threshold margin | AH Creations | LOCKED |
| 2026-09-01 | DEC-006 | Strict Personal Learning gate requiring explicit approval | AH Creations | GATED |
| 2026-09-01 | DEC-007 | Fluent sidebar UI redesign with official 3D branding | AH Creations | COMPLETED |
