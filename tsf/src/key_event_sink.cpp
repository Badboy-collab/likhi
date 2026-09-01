#include "../include/text_service.h"
#include <iostream>

namespace bangla_tsf {

STDMETHODIMP TextService::OnSetFocus(BOOL fForeground) {
    if (!fForeground) {
        composition_mgr_.OnFocusLost(nullptr);
    }
    return S_OK;
}

// ============================================================
// OnTestKeyDown — TSF calls this FIRST to ask "will you eat this key?"
// RULE: Return pfEaten=TRUE ONLY for keys we will definitively handle in OnKeyDown.
//       Anything else MUST return FALSE so the host app processes it natively.
// ============================================================
STDMETHODIMP TextService::OnTestKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    (void)pic; (void)lParam;
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;

    // --- RULE 0: Modifier keys themselves are never eaten ---
    if (wParam == VK_CONTROL || wParam == VK_LCONTROL || wParam == VK_RCONTROL ||
        wParam == VK_MENU   || wParam == VK_LMENU   || wParam == VK_RMENU   ||
        wParam == VK_SHIFT  || wParam == VK_LSHIFT  || wParam == VK_RSHIFT  ||
        wParam == VK_LWIN   || wParam == VK_RWIN    || wParam == VK_APPS) {
        *pfEaten = FALSE;
        return S_OK;
    }

    // --- RULE 1: Ctrl / Alt / Win combinations → always pass through ---
    bool is_ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool is_alt  = (GetKeyState(VK_MENU)    & 0x8000) != 0;
    bool is_win  = (GetKeyState(VK_LWIN)    & 0x8000) != 0 ||
                   (GetKeyState(VK_RWIN)    & 0x8000) != 0;
    if (is_ctrl || is_alt || is_win) {
        *pfEaten = FALSE;
        return S_OK;
    }

    // --- RULE 2: Shift combinations → always pass through ---
    bool is_shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    if (is_shift) {
        *pfEaten = FALSE;
        return S_OK;
    }

    // --- RULE 3: Function keys, system keys, Tab, Caps Lock → never eat ---
    if ((wParam >= VK_F1 && wParam <= VK_F24) ||
        wParam == VK_TAB || wParam == VK_CAPITAL ||
        wParam == VK_NUMLOCK || wParam == VK_SCROLL ||
        wParam == VK_SNAPSHOT || wParam == VK_PAUSE ||
        wParam == VK_INSERT) {
        *pfEaten = FALSE;
        return S_OK;
    }

    // --- RULE 4: NumPad operators → never eat ---
    if (wParam == VK_ADD || wParam == VK_SUBTRACT || wParam == VK_MULTIPLY ||
        wParam == VK_DIVIDE || wParam == VK_DECIMAL || wParam == VK_SEPARATOR) {
        *pfEaten = FALSE;
        return S_OK;
    }

    // --- RULE 5: Space → NEVER eat in TestKeyDown ---
    // We commit in OnKeyDown and then return pfEaten=FALSE so the host inserts the real space.
    if (wParam == VK_SPACE) {
        *pfEaten = FALSE;
        return S_OK;
    }

    // --- RULE 6: Escape → eat only if composing (we will cancel composition) ---
    if (wParam == VK_ESCAPE) {
        *pfEaten = composition_mgr_.IsComposing() ? TRUE : FALSE;
        return S_OK;
    }

    // --- RULE 7: Number keys (top-row and numpad) ---
    if (wParam >= '0' && wParam <= '9') {
        // Only eat if candidate popup is active and this digit selects a real candidate
        if (composition_mgr_.CanSelectCandidate(static_cast<char>(wParam))) {
            *pfEaten = TRUE;
        } else {
            *pfEaten = FALSE; // No composition or no matching candidate → native number
        }
        return S_OK;
    }
    if (wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9) {
        bool numlock = (GetKeyState(VK_NUMLOCK) & 0x0001) != 0;
        if (numlock) {
            char digit = '0' + static_cast<char>(wParam - VK_NUMPAD0);
            if (composition_mgr_.CanSelectCandidate(digit)) {
                *pfEaten = TRUE;
            } else {
                *pfEaten = FALSE;
            }
        } else {
            *pfEaten = FALSE; // NumLock OFF → navigation keys, never eat
        }
        return S_OK;
    }

    // --- RULE 8: Alpha letters → eat (we process them as Roman input) ---
    if ((wParam >= 'A' && wParam <= 'Z') || (wParam >= 'a' && wParam <= 'z')) {
        *pfEaten = TRUE;
        return S_OK;
    }

    // --- RULE 9: Composition-only control keys ---
    if (composition_mgr_.IsComposing()) {
        if (wParam == VK_BACK || wParam == VK_RETURN ||
            wParam == VK_UP   || wParam == VK_DOWN   ||
            wParam == VK_OEM_PERIOD) {
            *pfEaten = TRUE;
            return S_OK;
        }
    }

    // Default: pass through
    *pfEaten = FALSE;
    return S_OK;
}

// ============================================================
// OnKeyDown — The actual key handler. TSF calls this only after
//             OnTestKeyDown returns TRUE. However, for keys where
//             we said FALSE in OnTestKeyDown but still want to react
//             (like Space to commit), we handle the commit here and
//             return pfEaten=FALSE so the host still gets the key.
// ============================================================
STDMETHODIMP TextService::OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    (void)lParam;
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;

    // --- RULE 0: Modifier keys themselves ---
    if (wParam == VK_CONTROL || wParam == VK_LCONTROL || wParam == VK_RCONTROL ||
        wParam == VK_MENU   || wParam == VK_LMENU   || wParam == VK_RMENU   ||
        wParam == VK_SHIFT  || wParam == VK_LSHIFT  || wParam == VK_RSHIFT  ||
        wParam == VK_LWIN   || wParam == VK_RWIN    || wParam == VK_APPS) {
        *pfEaten = FALSE;
        return S_OK;
    }

    // --- RULE 1: Ctrl / Alt / Win combinations ---
    bool is_ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool is_alt  = (GetKeyState(VK_MENU)    & 0x8000) != 0;
    bool is_win  = (GetKeyState(VK_LWIN)    & 0x8000) != 0 ||
                   (GetKeyState(VK_RWIN)    & 0x8000) != 0;
    if (is_ctrl || is_alt || is_win) {
        // Abort any active composition (e.g., user pressed Ctrl+Z while typing)
        if (composition_mgr_.IsComposing()) {
            composition_mgr_.OnEscape(pic);
        }
        *pfEaten = FALSE;
        return S_OK;
    }

    bool is_shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    // --- RULE 2: Shift combinations ---
    if (is_shift) {
        // Commit composition first, then let the Shift+key pass natively
        if (composition_mgr_.IsComposing()) {
            composition_mgr_.OnEnter(pic);
        }
        *pfEaten = FALSE;
        return S_OK;
    }

    // --- RULE 3: Function keys, system keys ---
    if ((wParam >= VK_F1 && wParam <= VK_F24) ||
        wParam == VK_TAB || wParam == VK_CAPITAL ||
        wParam == VK_NUMLOCK || wParam == VK_SCROLL ||
        wParam == VK_SNAPSHOT || wParam == VK_PAUSE ||
        wParam == VK_INSERT) {
        if (composition_mgr_.IsComposing()) {
            composition_mgr_.OnEnter(pic);
        }
        *pfEaten = FALSE;
        return S_OK;
    }

    // --- RULE 4: NumPad operators ---
    if (wParam == VK_ADD || wParam == VK_SUBTRACT || wParam == VK_MULTIPLY ||
        wParam == VK_DIVIDE || wParam == VK_DECIMAL || wParam == VK_SEPARATOR) {
        if (composition_mgr_.IsComposing()) {
            composition_mgr_.OnEnter(pic);
        }
        *pfEaten = FALSE;
        return S_OK;
    }

    // --- RULE 5: Space — the most critical key ---
    // BEHAVIOR: Commit Bengali word WITHOUT eating the Space.
    //           The host app receives the Space natively and inserts exactly 1 space.
    // This is the ONLY reliable way to guarantee exactly one space in all host apps.
    if (wParam == VK_SPACE) {
        if (composition_mgr_.IsComposing()) {
            composition_mgr_.OnEnter(pic); // Commit the word cleanly (no space)
        }
        *pfEaten = FALSE; // Pass Space to host — host inserts the real space
        return S_OK;
    }

    // --- RULE 6: Escape ---
    if (wParam == VK_ESCAPE) {
        if (composition_mgr_.IsComposing()) {
            composition_mgr_.OnEscape(pic);
            *pfEaten = TRUE;
        } else {
            *pfEaten = FALSE;
        }
        return S_OK;
    }

    // --- RULE 7: Top-row number keys ---
    if (wParam >= '0' && wParam <= '9') {
        char digit = static_cast<char>(wParam);
        if (composition_mgr_.CanSelectCandidate(digit)) {
            // Candidate popup active + valid digit → select candidate
            *pfEaten = composition_mgr_.OnDigit(pic, digit);
        } else {
            // No composition or no matching candidate → commit any composition, pass number
            if (composition_mgr_.IsComposing()) {
                composition_mgr_.OnEnter(pic);
            }
            *pfEaten = FALSE;
        }
        return S_OK;
    }

    // --- RULE 8: NumPad digit keys ---
    if (wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9) {
        bool numlock = (GetKeyState(VK_NUMLOCK) & 0x0001) != 0;
        if (numlock) {
            char digit = '0' + static_cast<char>(wParam - VK_NUMPAD0);
            if (composition_mgr_.CanSelectCandidate(digit)) {
                *pfEaten = composition_mgr_.OnDigit(pic, digit);
            } else {
                if (composition_mgr_.IsComposing()) {
                    composition_mgr_.OnEnter(pic);
                }
                *pfEaten = FALSE;
            }
        } else {
            // NumLock OFF → navigation keys (Home, End, Arrow, etc.) — always pass through
            if (composition_mgr_.IsComposing()) {
                composition_mgr_.OnEnter(pic);
            }
            *pfEaten = FALSE;
        }
        return S_OK;
    }

    bool is_composing = composition_mgr_.IsComposing();

    // --- RULE 9: Cursor / navigation keys while composing ---
    if (wParam == VK_LEFT || wParam == VK_RIGHT ||
        wParam == VK_HOME || wParam == VK_END   ||
        wParam == VK_DELETE || wParam == VK_PRIOR || wParam == VK_NEXT) {
        if (is_composing) {
            composition_mgr_.OnEnter(pic);
        }
        *pfEaten = FALSE;
        return S_OK;
    }

    // --- RULE 10: Alpha letters → Roman phonetic input ---
    if ((wParam >= 'A' && wParam <= 'Z') || (wParam >= 'a' && wParam <= 'z')) {
        char ch = static_cast<char>(wParam);
        bool is_caps = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
        // is_shift is already false here (handled above), only caps matters
        if (is_caps) {
            if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
        } else {
            if (ch >= 'A' && ch <= 'Z') ch = ch - 'A' + 'a';
        }
        *pfEaten = composition_mgr_.OnCharacter(pic, ch);
        return S_OK;
    }

    // --- RULE 11: Backspace ---
    if (wParam == VK_BACK) {
        if (is_composing) {
            *pfEaten = composition_mgr_.OnBackspace(pic);
        } else {
            *pfEaten = FALSE;
        }
        return S_OK;
    }

    // --- RULE 12: Enter ---
    if (wParam == VK_RETURN) {
        if (is_composing) {
            *pfEaten = composition_mgr_.OnEnter(pic);
        } else {
            *pfEaten = FALSE;
        }
        return S_OK;
    }

    // --- RULE 13: Arrow up/down for candidate navigation ---
    if (wParam == VK_UP || wParam == VK_DOWN) {
        if (is_composing) {
            *pfEaten = composition_mgr_.OnArrow(pic, wParam == VK_DOWN);
        } else {
            *pfEaten = FALSE;
        }
        return S_OK;
    }

    // --- RULE 14: Period / Daari ---
    if (wParam == VK_OEM_PERIOD) {
        if (is_composing) {
            *pfEaten = composition_mgr_.OnPunctuation(pic, '.');
        } else {
            *pfEaten = FALSE;
        }
        return S_OK;
    }

    // Default: pass everything else through
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP TextService::OnTestKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    (void)pic; (void)wParam; (void)lParam;
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP TextService::OnKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    (void)pic; (void)wParam; (void)lParam;
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP TextService::OnPreservedKey(ITfContext* pic, REFGUID rguid, BOOL* pfEaten) {
    (void)pic; (void)rguid;
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;
    return S_OK;
}

} // namespace bangla_tsf
