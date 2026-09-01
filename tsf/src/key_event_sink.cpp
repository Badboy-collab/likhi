#include "../include/text_service.h"
#include <iostream>

namespace bangla_tsf {

STDMETHODIMP TextService::OnSetFocus(BOOL fForeground) {
    if (!fForeground) {
        composition_mgr_.OnFocusLost(nullptr);
    }
    return S_OK;
}

STDMETHODIMP TextService::OnTestKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;

    // 0. Modifier Keys Themselves: NEVER eat modifier key presses
    if (wParam == VK_CONTROL || wParam == VK_LCONTROL || wParam == VK_RCONTROL ||
        wParam == VK_MENU || wParam == VK_LMENU || wParam == VK_RMENU ||
        wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT ||
        wParam == VK_LWIN || wParam == VK_RWIN || wParam == VK_APPS) {
        *pfEaten = FALSE;
        return S_OK;
    }

    // 1. Shortcut & Protected Combinations
    // FIX: Removed GetAsyncKeyState to prevent hardware-polling desyncs
    bool is_ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool is_alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    bool is_win = (GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0;

    if (is_ctrl || is_alt || is_win) {
        *pfEaten = FALSE;
        return S_OK;
    }

    // 2. Shift + Key Handling
    bool is_shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    if (is_shift) {
        if ((wParam >= '0' && wParam <= '9') || (wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9)) {
            *pfEaten = FALSE;
            return S_OK;
        }
        if (wParam < 'A' || wParam > 'Z') {
            if (wParam < 'a' || wParam > 'z') {
                *pfEaten = FALSE;
                return S_OK;
            }
        }
    }

    // 3. Function Keys, Media Keys, System Keys
    if ((wParam >= VK_F1 && wParam <= VK_F24) ||
        wParam == VK_NUMLOCK || wParam == VK_SCROLL ||
        wParam == VK_SNAPSHOT || wParam == VK_PAUSE ||
        wParam == VK_INSERT || wParam == VK_CAPITAL || wParam == VK_APPS) {
        *pfEaten = FALSE;
        return S_OK;
    }

    // 4. Physical Numeric Keypad Operators
    if (wParam == VK_ADD || wParam == VK_SUBTRACT || wParam == VK_MULTIPLY ||
        wParam == VK_DIVIDE || wParam == VK_DECIMAL || wParam == VK_SEPARATOR) {
        *pfEaten = FALSE;
        return S_OK;
    }

    // 5. Physical Numeric Keypad Digits
    if (wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9) {
        bool is_numlock_on = (GetKeyState(VK_NUMLOCK) & 0x0001) != 0;
        if (is_numlock_on) {
            *pfEaten = TRUE;
            return S_OK;
        } else {
            *pfEaten = FALSE;
            return S_OK;
        }
    }

    // 6. Top-Row Numeric Keys
    if (wParam >= '0' && wParam <= '9') {
        *pfEaten = TRUE;
        return S_OK;
    }

    // 7. Alphanumeric Character Keys
    if ((wParam >= 'A' && wParam <= 'Z') || (wParam >= 'a' && wParam <= 'z')) {
        *pfEaten = TRUE;
        return S_OK;
    }

    // 8. Active Composition Control Keys
    bool is_composing = composition_mgr_.IsComposing();
    if (is_composing) {
        // FIX: Removed VK_SPACE. Space is NEVER eaten in OnTestKeyDown so the host app receives it natively.
        if (wParam == VK_RETURN ||
            wParam == VK_BACK ||
            wParam == VK_ESCAPE ||
            wParam == VK_UP ||
            wParam == VK_DOWN ||
            wParam == VK_OEM_PERIOD) {
            *pfEaten = TRUE;
            return S_OK;
        }
        
        if (wParam == VK_SPACE) {
            *pfEaten = FALSE;
            return S_OK;
        }
    }

    return S_OK;
}

STDMETHODIMP TextService::OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;

    // 0. Modifier Keys Themselves
    if (wParam == VK_CONTROL || wParam == VK_LCONTROL || wParam == VK_RCONTROL ||
        wParam == VK_MENU || wParam == VK_LMENU || wParam == VK_RMENU ||
        wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT ||
        wParam == VK_LWIN || wParam == VK_RWIN || wParam == VK_APPS) {
        *pfEaten = FALSE;
        return S_OK;
    }

    // 1. Shortcut Combinations
    bool is_ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    bool is_alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
    bool is_win = (GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0;

    if (is_ctrl || is_alt || is_win) {
        if (composition_mgr_.IsComposing()) {
            composition_mgr_.OnEscape(pic);
        }
        *pfEaten = FALSE;
        return S_OK;
    }

    // 2. Shift + Key Handling
    bool is_shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    if (is_shift) {
        if ((wParam >= '0' && wParam <= '9') || (wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9)) {
            if (composition_mgr_.IsComposing()) {
                composition_mgr_.OnEnter(pic); // Commit without space
            }
            *pfEaten = FALSE;
            return S_OK;
        }
        if (wParam < 'A' || wParam > 'Z') {
            if (wParam < 'a' || wParam > 'z') {
                if (composition_mgr_.IsComposing()) {
                    composition_mgr_.OnEnter(pic);
                }
                *pfEaten = FALSE;
                return S_OK;
            }
        }
    }

    // 3. Function Keys, Media Keys, Special System Keys
    if ((wParam >= VK_F1 && wParam <= VK_F24) ||
        wParam == VK_NUMLOCK || wParam == VK_SCROLL ||
        wParam == VK_SNAPSHOT || wParam == VK_PAUSE ||
        wParam == VK_INSERT || wParam == VK_CAPITAL || wParam == VK_APPS) {
        if (composition_mgr_.IsComposing()) {
            composition_mgr_.OnEnter(pic);
        }
        *pfEaten = FALSE;
        return S_OK;
    }

    // 4. Physical Numeric Keypad Operators
    if (wParam == VK_ADD || wParam == VK_SUBTRACT || wParam == VK_MULTIPLY ||
        wParam == VK_DIVIDE || wParam == VK_DECIMAL || wParam == VK_SEPARATOR) {
        if (composition_mgr_.IsComposing()) {
            composition_mgr_.OnEnter(pic);
        }
        *pfEaten = FALSE;
        return S_OK;
    }

    // 5. Physical Numeric Keypad Digits
    if (wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9) {
        bool is_numlock_on = (GetKeyState(VK_NUMLOCK) & 0x0001) != 0;
        if (is_numlock_on) {
            char digit = '0' + static_cast<char>(wParam - VK_NUMPAD0);
            *pfEaten = composition_mgr_.OnDigit(pic, digit, false);
            return S_OK;
        } else {
            if (composition_mgr_.IsComposing()) {
                composition_mgr_.OnEnter(pic);
            }
            *pfEaten = FALSE;
            return S_OK;
        }
    }

    // 6. Top-Row Numeric Keys
    if (wParam >= '0' && wParam <= '9') {
        char digit = static_cast<char>(wParam);
        *pfEaten = composition_mgr_.OnDigit(pic, digit, true);
        return S_OK;
    }

    // 7. Cursor navigation / edit keys
    bool is_composing = composition_mgr_.IsComposing();
    if (is_composing && (wParam == VK_LEFT || wParam == VK_RIGHT || wParam == VK_HOME || wParam == VK_END || wParam == VK_DELETE || wParam == VK_TAB || wParam == VK_PRIOR || wParam == VK_NEXT)) {
        composition_mgr_.OnEnter(pic);
        *pfEaten = FALSE;
        return S_OK;
    }

    // 8. Alphanumeric input
    if ((wParam >= 'A' && wParam <= 'Z') || (wParam >= 'a' && wParam <= 'z')) {
        char ch = static_cast<char>(wParam);
        bool is_caps = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;

        if (is_shift ^ is_caps) {
            if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
        } else {
            if (ch >= 'A' && ch <= 'Z') ch = ch - 'A' + 'a';
        }

        *pfEaten = composition_mgr_.OnCharacter(pic, ch);
        return S_OK;
    }

    // 9. Backspace
    if (wParam == VK_BACK && is_composing) {
        *pfEaten = composition_mgr_.OnBackspace(pic);
        return S_OK;
    }

    // 10. Space - FIX: Commit without space, then pass through natively
    if (wParam == VK_SPACE && is_composing) {
        composition_mgr_.OnEnter(pic); // Commit word
        *pfEaten = FALSE;              // Let host app process the actual Space keystroke
        return S_OK;
    }

    // 11. Enter
    if (wParam == VK_RETURN && is_composing) {
        *pfEaten = composition_mgr_.OnEnter(pic);
        return S_OK;
    }

    // 12. Escape
    if (wParam == VK_ESCAPE && is_composing) {
        *pfEaten = composition_mgr_.OnEscape(pic);
        return S_OK;
    }

    // 13. Arrow Navigation
    if (is_composing && (wParam == VK_UP || wParam == VK_DOWN)) {
        *pfEaten = composition_mgr_.OnArrow(pic, wParam == VK_DOWN);
        return S_OK;
    }

    // 14. Period
    if (wParam == VK_OEM_PERIOD && is_composing) {
        *pfEaten = composition_mgr_.OnPunctuation(pic, '.');
        return S_OK;
    }

    return S_OK;
}

STDMETHODIMP TextService::OnTestKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP TextService::OnKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP TextService::OnPreservedKey(ITfContext* pic, REFGUID rguid, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;
    return S_OK;
}

} // namespace bangla_tsf
