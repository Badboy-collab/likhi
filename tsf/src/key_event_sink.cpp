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
    // 0. Modifier Keys Themselves: NEVER eat modifier key presses
    if (wParam == VK_CONTROL || wParam == VK_LCONTROL || wParam == VK_RCONTROL ||
        wParam == VK_MENU || wParam == VK_LMENU || wParam == VK_RMENU ||
        wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT ||
        wParam == VK_LWIN || wParam == VK_RWIN) {
        *pfEaten = FALSE;
        return S_OK;
    }

    // 1. Shortcut Combinations: If Ctrl, Alt, or Win is held, MUST pass directly to host app
    bool is_ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0 || (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool is_alt = (GetKeyState(VK_MENU) & 0x8000) != 0 || (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    bool is_win = (GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0 ||
                  (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 || (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;

    if (is_ctrl || is_alt || is_win) {
        *pfEaten = FALSE;
        return S_OK;
    }

    // 1. Function Keys (F1-F24), Media Keys, Numpad, and Special Keys: NEVER eat
    if ((wParam >= VK_F1 && wParam <= VK_F24) ||
        (wParam >= VK_NUMPAD0 && wParam <= VK_DIVIDE) ||
        wParam == VK_NUMLOCK ||
        wParam == VK_SCROLL ||
        wParam == VK_SNAPSHOT ||
        wParam == VK_PAUSE ||
        wParam == VK_INSERT ||
        wParam == VK_CAPITAL) {
        *pfEaten = FALSE;
        return S_OK;
    }

    bool is_composing = composition_mgr_.IsComposing();

    // 2. Alphanumeric Keys (A-Z)
    if ((wParam >= 'A' && wParam <= 'Z') || (wParam >= 'a' && wParam <= 'z')) {
        *pfEaten = TRUE;
        return S_OK;
    }

    // 3. Active Composition Control Keys
    if (is_composing) {
        if (wParam == VK_SPACE ||
            wParam == VK_RETURN ||
            wParam == VK_BACK ||
            wParam == VK_ESCAPE ||
            wParam == VK_UP ||
            wParam == VK_DOWN ||
            (wParam >= '1' && wParam <= '5') ||
            wParam == VK_OEM_PERIOD) {
            *pfEaten = TRUE;
            return S_OK;
        }
    }

    return S_OK;
}

STDMETHODIMP TextService::OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;

    // 0. Modifier Keys Themselves: NEVER eat modifier key presses
    if (wParam == VK_CONTROL || wParam == VK_LCONTROL || wParam == VK_RCONTROL ||
        wParam == VK_MENU || wParam == VK_LMENU || wParam == VK_RMENU ||
        wParam == VK_SHIFT || wParam == VK_LSHIFT || wParam == VK_RSHIFT ||
        wParam == VK_LWIN || wParam == VK_RWIN) {
        *pfEaten = FALSE;
        return S_OK;
    }

    // 1. Shortcut Combinations: If Ctrl, Alt, or Win is held, cancel active composition and pass to host
    bool is_ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0 || (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    bool is_alt = (GetKeyState(VK_MENU) & 0x8000) != 0 || (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    bool is_win = (GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0 ||
                  (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0 || (GetAsyncKeyState(VK_RWIN) & 0x8000) != 0;

    if (is_ctrl || is_alt || is_win) {
        if (composition_mgr_.IsComposing()) {
            composition_mgr_.OnEscape(pic);
        }
        *pfEaten = FALSE;
        return S_OK;
    }

    // 1. Function Keys (F1-F24), Numpad, and Special Keys: Pass through directly
    if ((wParam >= VK_F1 && wParam <= VK_F24) ||
        (wParam >= VK_NUMPAD0 && wParam <= VK_DIVIDE) ||
        wParam == VK_NUMLOCK ||
        wParam == VK_SCROLL ||
        wParam == VK_SNAPSHOT ||
        wParam == VK_PAUSE ||
        wParam == VK_INSERT ||
        wParam == VK_CAPITAL) {
        if (composition_mgr_.IsComposing()) {
            composition_mgr_.OnSpace(pic);
        }
        *pfEaten = FALSE;
        return S_OK;
    }

    // 2. Cursor navigation / edit keys during composition: Commit word then allow movement
    bool is_composing = composition_mgr_.IsComposing();
    if (is_composing && (wParam == VK_LEFT || wParam == VK_RIGHT || wParam == VK_HOME || wParam == VK_END || wParam == VK_DELETE || wParam == VK_TAB || wParam == VK_PRIOR || wParam == VK_NEXT)) {
        composition_mgr_.OnSpace(pic);
        *pfEaten = FALSE;
        return S_OK;
    }

    // 3. Alphanumeric input (A-Z)
    if ((wParam >= 'A' && wParam <= 'Z') || (wParam >= 'a' && wParam <= 'z')) {
        char ch = static_cast<char>(wParam);
        bool is_shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        bool is_caps = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;

        if (is_shift ^ is_caps) {
            if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
        } else {
            if (ch >= 'A' && ch <= 'Z') ch = ch - 'A' + 'a';
        }

        *pfEaten = composition_mgr_.OnCharacter(pic, ch);
        return S_OK;
    }

    // 4. Number keys (1..5 candidate selection)
    if (is_composing && wParam >= '1' && wParam <= '5') {
        int num = static_cast<int>(wParam - '0');
        *pfEaten = composition_mgr_.OnNumberSelection(pic, num);
        return S_OK;
    }

    // 5. Backspace
    if (wParam == VK_BACK && is_composing) {
        *pfEaten = composition_mgr_.OnBackspace(pic);
        return S_OK;
    }

    // 6. Space
    if (wParam == VK_SPACE && is_composing) {
        *pfEaten = composition_mgr_.OnSpace(pic);
        return S_OK;
    }

    // 7. Enter
    if (wParam == VK_RETURN && is_composing) {
        *pfEaten = composition_mgr_.OnEnter(pic);
        return S_OK;
    }

    // 8. Escape
    if (wParam == VK_ESCAPE && is_composing) {
        *pfEaten = composition_mgr_.OnEscape(pic);
        return S_OK;
    }

    // 9. Arrow Navigation (Up / Down)
    if (is_composing && (wParam == VK_UP || wParam == VK_DOWN)) {
        *pfEaten = composition_mgr_.OnArrow(pic, wParam == VK_DOWN);
        return S_OK;
    }

    // 10. Period (Bengali Dāri transliteration during active composition)
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
