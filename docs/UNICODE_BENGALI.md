# Bengali Unicode Integrity & Editing Architecture: PC Bangla Typing App

## 1. Executive Summary & Hard Requirement
Bengali Unicode integrity is a **RELEASE-BLOCKING HARD REQUIREMENT** across the entire PC Bangla Typing App project:

> **বাংলা টাইপ করার সময় কোনো অবস্থাতেই অক্ষর ভেঙে যাওয়া, ভুল Unicode sequence তৈরি হওয়া, বা যুক্তাক্ষর/কার/ফলা বিচ্ছিন্ন হয়ে যাওয়া যাবে না।**

This applies not only to static character rendering, but throughout dynamic text editing: typing, backspacing, cursor navigation, middle-of-word insertions, deletions, candidate selection, auto-correction, and OS-level application commits.

---

## 2. Bengali Unicode Orthography & Canonical Sequences

Bengali script in Unicode (`U+0980` to `U+09FF`) relies on a strict combining grammar. Every Bengali syllable / grapheme cluster follows well-defined canonical code-point orders.

### 2.1 Canonical Syllable Structure
A valid Bengali orthographic syllable consists of:
$$[\text{Consonant} \ (+ \ \text{Hasant} \ + \ \text{Consonant})^*] \ (+ \ \text{Hasant} \ + \ \text{Fala})^* \ (+ \ \text{Kar})? \ (+ \ \text{Anusvara} \mid \text{Visarga} \mid \text{Chandrabindu})?$$

| Component | Code Points | Examples |
| :--- | :--- | :--- |
| **Independent Vowels (স্বরবর্ণ)** | `0x0985` .. `0x0994` | অ, আ, ই, ঈ, উ, ঊ, ঋ, এ, ঐ, ও, ঔ |
| **Consonants (ব্যঞ্জনবর্ণ)** | `0x0995` .. `0x09B9`, `0x09DC` .. `0x09DF` | ক .. হ, ড়, ঢ়, য় |
| **Hasant / Virama (হসন্ত)** | `0x09CD` | `্` (Suppresses inherent vowel, binds consonants) |
| **Dependent Vowel Signs (কার)** | `0x09BE` .. `0x09CC` | া, ি, ী, ু, ূ, ৃ, ে, ৈ, ো, ৌ |
| **Modifiers / Nasals** | `0x0981` (ঁ), `0x0982` (ং), `0x0983` (ঃ), `0x09CE` (ৎ) | চাঁদ, রং, দুঃখ, উৎসব |
| **Punctuation** | `0x0964` (।), `0x0965` (॥) | বাংলা দাঁড়ি |
| **Zero-Width Controls** | `0x200C` (ZWNJ), `0x200D` (ZWJ) | Explicit ligature breaking / joining |

---

## 3. Conjuncts & Ligatures Specification

The engine guarantees exact Unicode canonical representations for all simple, complex, and multiple conjunct clusters:

### 3.1 Standard & Complex Conjuncts (যুক্তাক্ষর)
- **ক্ষ (Khyo)**: `ক` (`0x0995`) + `্` (`0x09CD`) + `ষ` (`0x09B7`)
- **জ্ঞ (Ggyo)**: `জ` (`0x099C`) + `্` (`0x09CD`) + `ঞ` (`0x099E`)
- **ঞ্চ (Ncho)**: `ঞ` (`0x099E`) + `্` (`0x09CD`) + `চ` (`0x099A`)
- **ঞ্জ (Njo)**: `ঞ` (`0x099E`) + `্` (`0x09CD`) + `জ` (`0x099C`)
- **ঙ্ক (Ngko)**: `ঙ` (`0x0999`) + `্` (`0x09CD`) + `ক` (`0x0995`)
- **ঙ্গ (Nggo)**: `ঙ` (`0x0999`) + `্` (`0x09CD`) + `গ` (`0x0997`)
- **চ্ছ (Ccho)**: `চ` (`0x099A`) + `্` (`0x09CD`) + `ছ` (`0x099B`)
- **জ্জ (Jjo)**: `জ` (`0x099C`) + `্` (`0x09CD`) + `জ` (`0x099C`)
- **ট্ট (Tto)**: `ট` (`0x099F`) + `্` (`0x09CD`) + `ট` (`0x099F`)
- **ণ্ড (Ndo)**: `ণ` (`0x09A3`) + `্` (`0x09CD`) + `ড` (`0x09A1`)
- **ন্ত (Nto)**: `ন` (`0x09A8`) + `্` (`0x09CD`) + `ত` (`0x09A4`)
- **ন্ত্র (Ntro)**: `ন` (`0x09A8`) + `্` (`0x09CD`) + `ত` (`0x09A4`) + `্` (`0x09CD`) + `র` (`0x09B0`)
- **ন্দ (Ndo)**: `ন` (`0x09A8`) + `্` (`0x09CD`) + `দ` (`0x09A6`)
- **ন্ধ (Ndho)**: `ন` (`0x09A8`) + `্` (`0x09CD`) + `ধ` (`0x09A7`)
- **ম্প (Mpo)**: `ম` (`0x09AE`) + `্` (`0x09CD`) + `প` (`0x09AA`)
- **ম্ব (Mbo)**: `ম` (`0x09AE`) + `্` (`0x09CD`) + `ব` (`0x09AC`)
- **ম্ভ (Mbho)**: `ম` (`0x09AE`) + `্` (`0x09CD`) + `ভ` (`0x09AD`)
- **ষ্ট (Shto)**: `ষ` (`0x09B7`) + `্` (`0x09CD`) + `ট` (`0x099F`)
- **ষ্ঠ (Shtho)**: `ষ` (`0x09B7`) + `্` (`0x09CD`) + `ঠ` (`0x09A0`)
- **স্ক (Sko)**: `স` (`0x09B8`) + `্` (`0x09CD`) + `ক` (`0x0995`)
- **স্ন (Sno)**: `স` (`0x09B8`) + `্` (`0x09CD`) + `ন` (`0x09A8`)
- **স্প (Spo)**: `স` (`0x09B8`) + `্` (`0x09CD`) + `প` (`0x09AA`)
- **স্ত (Sto)**: `স` (`0x09B8`) + `্` (`0x09CD`) + `ত` (`0x09A4`)
- **স্থ (Stho)**: `স` (`0x09B8`) + `্` (`0x09CD`) + `থ` (`0x09A5`)
- **ত্র (Tro)**: `ত` (`0x09A4`) + `্` (`0x09CD`) + `র` (`0x09B0`)
- **দ্র (Dro)**: `দ` (`0x09A6`) + `্` (`0x09CD`) + `র` (`0x09B0`)
- **শ্র (Shro)**: `শ` (`0x09B6`) + `্` (`0x09CD`) + `র` (`0x09B0`)

### 3.2 Ref (রেফ) vs Ra-fala (র-ফলা)
- **Ref (`র্`)**: `র` (`0x09B0`) + `্` (`0x09CD`) + following consonant. Example: `বর্ম` = `ব` + `র` + `্` + `ম`.
- **Ra-fala (`্র`)**: Preceding consonant + `্` (`0x09CD`) + `র` (`0x09B0`). Example: `প্রথম` = `প` + `্` + `র` + `থ` + `ম`.

---

## 4. Grapheme-Aware Editing & Backspace Logic

### 4.1 The Backspace Problem
In simple byte or single code-point deletion:
- Deleting from `ক্ষ্ম` (`ক` + `্` + `ষ` + `্` + `ম`) could leave an illegal dangling Hasant (`ক` + `্` + `ষ` + `্`), causing standard Windows OpenType shapers (DirectWrite / Uniscribe) to display a broken dotted-circle (`্`).
- Deleting from a Kar sign on a conjunct must cleanly remove the Kar without corrupting the conjunct base.

### 4.2 Grapheme Cluster State Machine
The engine decomposes Bengali strings into atomic **Grapheme Clusters**:
1. **Atomic Base**: Consonant Cluster (with intermediate Hasants).
2. **Dependent Affixes**: Following Kar and Modifiers (Anusvara / Chandrabindu).
3. **Backspace Transitions**:
   - If trailing character is a Kar sign or modifier (`ং`, `ঃ`, `ঁ`, `া`, `ি`..), backspace removes the modifier, leaving the base intact.
   - If trailing character is a consonant preceded by a Hasant (`্`), backspace removes BOTH the consonant AND the preceding Hasant, reverting to the prior consonant cleanly.
   - If the composition is in Roman buffer mode, backspace removes the last Roman character and re-evaluates the candidate lattice from scratch.

---

## 5. Unicode Normalization & Windows Compatibility
- **Normalization Policy**: The engine generates canonical Unicode conforming to NFC.
- **Rendering Verification**: All test suites verify both the **byte-level Unicode code-point sequence** and rendering across DirectWrite, Notepad, Word, and Chromium text engines.
- **Zero Dangling Hasants**: No committed text shall ever end with an unjoined Hasant (`0x09CD`) unless explicitly followed by ZWNJ.
