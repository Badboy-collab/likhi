#include "../include/text_service.h"
#include "../include/key_policy.h"
#include "../include/tsf_log.h"
#include <iostream>

namespace bangla_tsf {

// ============================================================
// TSF KEY EVENT SINK — DEFINITIVE IMPLEMENTATION
//
// TSF CONTRACT (per MSDN ITfKeyEventSink):
//   1. OnTestKeyDown is called FIRST.
//      - pfEaten=TRUE  → TSF calls OnKeyDown; host does NOT see the key.
//      - pfEaten=FALSE → Host processes the key; TSF still calls OnKeyDown after.
//
//   2. When OnTestKeyDown=FALSE the HOST processes the key BEFORE OnKeyDown.
//      Therefore any commit that must precede a pass-through key (numbers,
//      Shift+letter, Tab, F1-F12, navigation keys, ...) is performed in
//      OnTestKeyDown via the policy's commit_first flag. OnKeyDown then only
//      acts on keys the policy marked "eat".
//
// GOLDEN RULE (non-negotiable):
//   Likhi NEVER consumes, swallows, blocks, modifies, or reinterprets a key
//   unless that key is explicitly required for an ACTIVE Likhi composition/
//   candidate operation.
//
// All per-key decisions live in the pure, unit-tested module key_policy.{h,cpp}.
// The two functions below only translate physical keyboard state into a
// KeyState, execute the decision, and (for eat keys) call the composition
// manager. Regression tests live in tests/unit/test_key_policy.cpp.
// ============================================================

STDMETHODIMP TextService::OnSetFocus(BOOL fForeground) {
    if (!fForeground) {
        composition_mgr_.OnFocusLost(nullptr);
    }
    return S_OK;
}

// Read the CURRENT physical modifier state.
// GetKeyState() alone is unreliable for fast chord presses (its value comes
// from the thread message queue and can lag one message behind), so every
// modifier check ORs in GetAsyncKeyState() — the live physical keyboard state.
static KeyState ReadKeyState(WPARAM vk) {
    KeyState s;
    s.vk = vk;
    s.ctrl   = ((GetKeyState(VK_CONTROL) & 0x8000) != 0) ||
               ((GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
    s.alt    = ((GetKeyState(VK_MENU)    & 0x8000) != 0) ||
               ((GetAsyncKeyState(VK_MENU)    & 0x8000) != 0);
    s.win    = ((GetKeyState(VK_LWIN)    & 0x8000) != 0) || ((GetAsyncKeyState(VK_LWIN) & 0x8000) != 0) ||
               ((GetKeyState(VK_RWIN)    & 0x8000) != 0) || ((GetAsyncKeyState(VK_RWIN) & 0x8000) != 0);
    s.shift  = ((GetKeyState(VK_SHIFT)   & 0x8000) != 0) ||
               ((GetAsyncKeyState(VK_SHIFT)   & 0x8000) != 0);
    s.numlock = (GetKeyState(VK_NUMLOCK) & 0x0001) != 0;
    return s;
}

// For digit keys only: is this digit a valid candidate selection right now?
static bool ComputeDigitSelectable(WPARAM vk, bool numlock, CompositionManager& mgr) {
    if (IsDigitKey(vk)) {
        return mgr.CanSelectCandidate(static_cast<char>(vk));
    }
    if (IsNumpadDigitKey(vk) && numlock) {
        return mgr.CanSelectCandidate('0' + static_cast<char>(vk - VK_NUMPAD0));
    }
    return false;
}

// ============================================================
// OnTestKeyDown — Declare intent: will we eat this key?
// Side effect: when the policy says commit_first and a composition
// is active, commit the Bengali word HERE — this is the ONLY hook
// that runs BEFORE the host application receives the key.
// ============================================================
STDMETHODIMP TextService::OnTestKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    (void)lParam;
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;

    KeyState st = ReadKeyState(wParam);
    st.composing          = composition_mgr_.IsComposing();
    st.candidates_visible = composition_mgr_.HasVisibleCandidates();
    st.digit_selectable   = ComputeDigitSelectable(wParam, st.numlock, composition_mgr_);

    KeyDecision d = DecideKey(st);

    TsfLog("OnTestKeyDown: pic=%p vk=0x%X ('%c') comp=%d eat=%d act=%d comm_first=%d",
           pic, (UINT)wParam, (wParam >= 32 && wParam <= 126) ? (char)wParam : '?',
           st.composing, d.eat, (int)d.action, d.commit_first);

    // Pass-through keys that break composition: commit BEFORE the host sees
    // the key so the number/Shift+letter/Tab/F-key/navigation lands AFTER the
    // committed Bengali word (e.g. "am"+"0" → "আম0", never "0আম").
    if (d.commit_first && st.composing) {
        composition_mgr_.OnEnter(pic);
    }

    *pfEaten = d.eat ? TRUE : FALSE;
    return S_OK;
}

// ============================================================
// OnKeyDown — Perform the action ONLY for keys the policy marked "eat".
// For pass-through keys, nothing happens here: the commit already
// happened in OnTestKeyDown (if needed) and the host processes the key.
// ============================================================
STDMETHODIMP TextService::OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) {
    (void)lParam;
    if (!pfEaten) return E_INVALIDARG;
    *pfEaten = FALSE;

    KeyState st = ReadKeyState(wParam);
    st.composing          = composition_mgr_.IsComposing();
    st.candidates_visible = composition_mgr_.HasVisibleCandidates();
    st.digit_selectable   = ComputeDigitSelectable(wParam, st.numlock, composition_mgr_);

    KeyDecision d = DecideKey(st);
    TsfLog("OnKeyDown: pic=%p vk=0x%X ('%c') eat=%d act=%d",
           pic, (UINT)wParam, (wParam >= 32 && wParam <= 126) ? (char)wParam : '?',
           d.eat, (int)d.action);

    if (!d.eat) {
        // Host processes the key natively; we never touched it in OnKeyDown.
        *pfEaten = FALSE;
        return S_OK;
    }

    switch (d.action) {
        case KeyAction::kProcessCharacter: {
            char ch = static_cast<char>(wParam);
            bool caps_lock = (GetKeyState(VK_CAPITAL) & 0x0001) != 0;
            bool is_upper = (st.shift ^ caps_lock);
            if (is_upper) {
                if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
            } else {
                if (ch >= 'A' && ch <= 'Z') ch = ch - 'A' + 'a';
            }
            *pfEaten = composition_mgr_.OnCharacter(pic, ch) ? TRUE : FALSE;
            return S_OK;
        }

        case KeyAction::kProcessSpace:
            // OnTestKeyDown said TRUE, so the host has NOT inserted a space.
            // OnSpace commits the word AND inserts exactly one U+0020 via TSF.
            *pfEaten = composition_mgr_.OnSpace(pic) ? TRUE : FALSE;
            return S_OK;

        case KeyAction::kProcessEscape:
            *pfEaten = composition_mgr_.OnEscape(pic) ? TRUE : FALSE;
            return S_OK;

        case KeyAction::kProcessBackspace:
            *pfEaten = composition_mgr_.OnBackspace(pic) ? TRUE : FALSE;
            return S_OK;

        case KeyAction::kProcessEnter:
            *pfEaten = composition_mgr_.OnEnter(pic) ? TRUE : FALSE;
            return S_OK;

        case KeyAction::kProcessDigit: {
            char digit = DigitFromKey(wParam);
            *pfEaten = (digit && composition_mgr_.OnDigit(pic, digit)) ? TRUE : FALSE;
            return S_OK;
        }

        case KeyAction::kProcessArrow:
            *pfEaten = composition_mgr_.OnArrow(pic, wParam == VK_DOWN) ? TRUE : FALSE;
            return S_OK;

        case KeyAction::kProcessPeriod:
            *pfEaten = composition_mgr_.OnPunctuation(pic, '.') ? TRUE : FALSE;
            return S_OK;

        default:
            *pfEaten = FALSE;
            return S_OK;
    }
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
