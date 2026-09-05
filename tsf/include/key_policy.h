#ifndef BANGLA_KEY_POLICY_H
#define BANGLA_KEY_POLICY_H

#include <windows.h>

namespace bangla_tsf {

// ============================================================
// KEY POLICY — Pure, side-effect-free decision table.
//
// This module is the SINGLE source of truth for "what should
// Likhi do with this key?" It contains NO TSF calls and NO
// global state, so the exact pass-through / eat / commit
// contract can be regression-tested without a TSF runtime.
//
// GOLDEN RULE (non-negotiable):
//   Likhi NEVER consumes, swallows, blocks, modifies, or
//   reinterprets a key unless that key is explicitly required
//   for an ACTIVE Likhi composition/candidate operation.
//
// TSF ORDERING FACT (why commit_first exists):
//   OnTestKeyDown runs BEFORE the host application receives the
//   key. OnKeyDown runs AFTER the host when we return FALSE.
//   Therefore any commit that must happen before the host sees a
//   pass-through key (numbers, Shift+letter, Tab, F1-F12, arrows,
//   navigation, ...) MUST be performed in OnTestKeyDown, otherwise
//   the host's character lands BEFORE the committed Bengali word
//   (e.g. "am" + "0" would produce "0আম" instead of "আম0").
// ============================================================

enum class KeyAction {
    kPass,               // host handles the key; Likhi does nothing
    kProcessCharacter,   // eat: feed a roman letter into the composition
    kProcessSpace,       // eat: commit word + insert exactly one U+0020
    kProcessEscape,      // eat: cancel the active composition
    kProcessBackspace,   // eat: remove last roman char of the composition
    kProcessEnter,       // eat: commit word (no trailing space)
    kProcessDigit,       // eat: select the candidate for this digit
    kProcessArrow,       // eat: navigate candidates (Up/Down)
    kProcessPeriod,      // eat: commit word + insert Bengali Dāri (।)
};

struct KeyState {
    WPARAM vk = 0;
    bool ctrl = false;               // any Ctrl held
    bool alt = false;                // any Alt held
    bool win = false;                // any Win held
    bool shift = false;              // any Shift held
    bool numlock = false;            // NumLock toggle state (for numpad digits)
    bool composing = false;          // an active Likhi composition exists
    bool candidates_visible = false; // candidate window is showing candidates
    bool digit_selectable = false;   // (digits only) this digit maps to a real candidate
};

struct KeyDecision {
    bool eat = false;          // OnTestKeyDown pfEaten result
    bool commit_first = false; // if true AND composing: commit the composition in
                               // OnTestKeyDown BEFORE the host receives the key.
    KeyAction action = KeyAction::kPass; // what OnKeyDown performs when eat == true
};

// The single decision function. Pure: same input => same output.
KeyDecision DecideKey(const KeyState& s);

// Key classification helpers (shared by the TSF handlers and the tests).
//
// VK-CODE NOTE: Windows delivers KEY codes (not character codes) as wParam.
// The physical A key is always VK_A (0x41), with or without Shift. Lowercase
// character codes 0x61-0x7A never arrive in real keyboard events — they would
// collide with VK_NUMPAD0-9 (0x60-0x69) and VK_F1-F11 (0x70-0x7A), which is
// exactly why keys are classified by VK code and not by character code.
bool IsModifierKey(WPARAM vk);          // Ctrl/Alt/Shift/Win/Apps themselves
bool IsFunctionOrSystemKey(WPARAM vk);  // F1-F24, Tab, CapsLock, NumLock, Scroll, PrtSc, Pause, Insert
bool IsNumpadOperator(WPARAM vk);       // +, -, *, /, ., separator
bool IsNavigationKey(WPARAM vk);        // Left/Right/Home/End/Delete/PgUp/PgDn
bool IsDigitKey(WPARAM vk);             // '0' - '9' (top row)
bool IsNumpadDigitKey(WPARAM vk);       // VK_NUMPAD0 - VK_NUMPAD9
bool IsAlphaKey(WPARAM vk);             // 'A'-'Z' / 'a'-'z'
char DigitFromKey(WPARAM vk);           // '0'-'9' for top-row and numpad digits

} // namespace bangla_tsf

#endif // BANGLA_KEY_POLICY_H
