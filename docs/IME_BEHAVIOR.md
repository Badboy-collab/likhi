# IME Behavior & Interaction Specification

## 1. Composition Behavior

- **Roman Keystroke Buffering**: As the user types Roman letters (`ami`), inline text previews the top-ranked Bengali transliteration (`আমি`).
- **Dynamic Candidate Updating**: The candidate bar automatically renders up to 5 ranked candidates.

---

## 2. Bengali Unicode Integrity Enforcement

- **No Broken Conjuncts**: All conjunct clusters maintain explicit valid sequences (`C1 + ্ + C2` or `C1 + ্ + C2 + ্ + C3`).
- **No Dangling Hasants**: Grapheme deletion, backspacing, or mid-composition cancellations never leave orphan `U+09CD` characters in the target document.

---

## 3. Keyboard Interactions

| Key / Action | IME Response |
| :--- | :--- |
| `a-z`, `A-Z` | Appends Roman character, updates candidates and inline composition preview. |
| `Space` | Commits top candidate (or user-selected candidate) + space into document. |
| `Enter` | Commits current candidate (or raw Roman) and terminates composition. |
| `1` - `5` | Commits corresponding candidate directly. |
| `Up` / `Down` Arrow | Navigates highlight across candidates in suggestion window. |
| `Backspace` | Deletes last Roman character; performs grapheme cluster rollback. |
| `Escape` | Cancels active composition without committing text. |
| `.` (Period) | Commits active word and inserts Bengali Dāri (`।`). |
| Mouse Click | Clicking any candidate directly commits it to document. |

---

## 4. Auto-Correct Behavior

- **Default State**: OFF (`auto_correct_enabled = false`).
- **When OFF**: The IME strictly respects user transliteration and never replaces user-selected words automatically.
- **When ON**: Auto-correct suggestions require confidence $\ge 0.85$ and score margin $\ge 0.03$.
