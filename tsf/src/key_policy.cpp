#include "../include/key_policy.h"

namespace bangla_tsf {

bool IsModifierKey(WPARAM vk) {
    return vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
           vk == VK_MENU    || vk == VK_LMENU    || vk == VK_RMENU    ||
           vk == VK_SHIFT   || vk == VK_LSHIFT   || vk == VK_RSHIFT   ||
           vk == VK_LWIN    || vk == VK_RWIN     || vk == VK_APPS;
}

bool IsFunctionOrSystemKey(WPARAM vk) {
    return (vk >= VK_F1 && vk <= VK_F24) ||
           vk == VK_TAB      || vk == VK_CAPITAL  ||
           vk == VK_NUMLOCK  || vk == VK_SCROLL   ||
           vk == VK_SNAPSHOT || vk == VK_PAUSE    ||
           vk == VK_INSERT;
}

bool IsNumpadOperator(WPARAM vk) {
    return vk == VK_ADD     || vk == VK_SUBTRACT ||
           vk == VK_MULTIPLY || vk == VK_DIVIDE  ||
           vk == VK_DECIMAL  || vk == VK_SEPARATOR;
}

bool IsNavigationKey(WPARAM vk) {
    return vk == VK_LEFT  || vk == VK_RIGHT ||
           vk == VK_HOME  || vk == VK_END   ||
           vk == VK_DELETE || vk == VK_PRIOR || vk == VK_NEXT;
}

bool IsDigitKey(WPARAM vk) {
    return vk >= '0' && vk <= '9';
}

bool IsNumpadDigitKey(WPARAM vk) {
    return vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9;
}

bool IsAlphaKey(WPARAM vk) {
    return (vk >= 'A' && vk <= 'Z') || (vk >= 'a' && vk <= 'z');
}

char DigitFromKey(WPARAM vk) {
    if (IsDigitKey(vk)) return static_cast<char>(vk);
    if (IsNumpadDigitKey(vk)) return '0' + static_cast<char>(vk - VK_NUMPAD0);
    return '\0';
}

KeyDecision DecideKey(const KeyState& s) {
    KeyDecision d;

    // 1. Modifier keys themselves (Ctrl/Alt/Shift/Win/Apps): never eaten.
    //    Their own keydown/keyup must reach the application untouched.
    if (IsModifierKey(s.vk)) {
        return d; // eat=false, action=kPass
    }

    // 2. Ctrl / Alt / Win combinations (Ctrl+C, Alt+Tab, Win+D, ...):
    //    full pass-through. If composing, commit first so no text is lost
    //    and the shortcut acts on committed text.
    if (s.ctrl || s.alt || s.win) {
        d.commit_first = s.composing;
        return d;
    }

    // 3. Shift combinations:
    //    - Shift + pure alpha key (A-Z) is phonetic composition input (e.g. Shift+B, Shift+N, Shift+T).
    //      Do not pass through; let it fall through to step 10 so it is eaten as kProcessCharacter.
    //    - All other Shift combinations (Shift+digit e.g. Shift+1="!", Shift+nav, Shift+symbols)
    //      pass through natively. Commit first if composing.
    if (s.shift && !IsAlphaKey(s.vk)) {
        d.commit_first = s.composing;
        return d;
    }

    // 4. Function / system keys (F1-F24, Tab, CapsLock, NumLock, ScrollLock,
    //    PrtSc, Pause, Insert): never eaten. F1-F12 must keep their original
    //    application function. Commit first if composing.
    if (IsFunctionOrSystemKey(s.vk)) {
        d.commit_first = s.composing;
        return d;
    }

    // 5. NumPad operators (+, -, *, /, .): never eaten.
    if (IsNumpadOperator(s.vk)) {
        d.commit_first = s.composing;
        return d;
    }

    // 6. Space: composing → we handle it (commit word + exactly one space).
    //    Not composing → pass through (host inserts one normal space).
    if (s.vk == VK_SPACE) {
        if (s.composing) {
            d.eat = true;
            d.action = KeyAction::kProcessSpace;
        }
        return d;
    }

    // 7. Escape: composing → we cancel the composition. Not composing → pass.
    if (s.vk == VK_ESCAPE) {
        if (s.composing) {
            d.eat = true;
            d.action = KeyAction::kProcessEscape;
        }
        return d;
    }

    // 8. Digits (top row '0'-'9', and numpad digits while NumLock is ON):
    //    eaten ONLY when they select a real candidate. Otherwise the number
    //    must reach the application natively — commit first if composing so
    //    the digit lands AFTER the committed word ("am"+"0" → "আম0").
    if (IsDigitKey(s.vk) || (IsNumpadDigitKey(s.vk) && s.numlock)) {
        if (s.composing && s.digit_selectable) {
            d.eat = true;
            d.action = KeyAction::kProcessDigit;
        } else {
            d.commit_first = s.composing;
        }
        return d;
    }

    // 9. NumPad digit with NumLock OFF → native keypad navigation
    //    (Home/End/PgUp/PgDn/Arrows/Ins/Del). Never eaten.
    if (IsNumpadDigitKey(s.vk)) {
        d.commit_first = s.composing;
        return d;
    }

    // 10. Roman letters: eaten as phonetic composition input
    //     (they build/extend the active composition).
    if (IsAlphaKey(s.vk)) {
        d.eat = true;
        d.action = KeyAction::kProcessCharacter;
        return d;
    }

    // 11. Backspace: eaten only while composing (remove last roman char).
    if (s.vk == VK_BACK) {
        if (s.composing) {
            d.eat = true;
            d.action = KeyAction::kProcessBackspace;
        }
        return d;
    }

    // 12. Enter: eaten only while composing (commit word, no trailing space).
    if (s.vk == VK_RETURN) {
        if (s.composing) {
            d.eat = true;
            d.action = KeyAction::kProcessEnter;
        }
        return d;
    }

    // 13. Up/Down arrows: candidate navigation ONLY while the candidate
    //     window is actually visible. Otherwise (idle, or composing without
    //     candidates) they must pass through to the application.
    if (s.vk == VK_UP || s.vk == VK_DOWN) {
        if (s.composing && s.candidates_visible) {
            d.eat = true;
            d.action = KeyAction::kProcessArrow;
        } else {
            d.commit_first = s.composing;
        }
        return d;
    }

    // 14. Left/Right/Home/End/Delete/PgUp/PgDn: never eaten; commit first
    //     if composing so the caret move happens after the word is committed.
    if (IsNavigationKey(s.vk)) {
        d.commit_first = s.composing;
        return d;
    }

    // 15. Period → Bengali Dāri (।) both while composing and in idle Bangla mode
    if (s.vk == VK_OEM_PERIOD) {
        if (!s.shift && !s.ctrl && !s.alt) {
            d.eat = true;
            d.action = KeyAction::kProcessPeriod;
        } else if (s.shift) {
            d.commit_first = s.composing;
        }
        return d;
    }

    // 16. Everything else: pass through untouched.
    return d;
}

} // namespace bangla_tsf
