#include "universal_typing.h"

namespace likhi {
namespace universal {

namespace {

bool IsAsciiLetter(wchar_t c) {
    return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z');
}

bool IsAsciiDigit(wchar_t c) { return c >= L'0' && c <= L'9'; }

wchar_t ToLowerAscii(wchar_t c) {
    return (c >= L'A' && c <= L'Z') ? static_cast<wchar_t>(c - L'A' + L'a') : c;
}

} // namespace

KeyEvent ClassifyKey(int vk, wchar_t produced_char, bool ctrl, bool alt, bool win,
                     bool shift, bool numpad) {
    KeyEvent event;
    event.ch = produced_char;
    event.ctrl = ctrl;
    event.alt = alt;
    event.win = win;
    event.shift = shift;

    // Modifiers and locks on their own: harmless, the preview is kept.
    switch (vk) {
        case kVkShift: case kVkControl: case kVkMenu:
        case kVkLShift: case kVkRShift: case kVkLControl: case kVkRControl:
        case kVkLMenu: case kVkRMenu:
        case kVkCapital: case kVkNumlock: case kVkScroll:
        case kVkLWin: case kVkRWin: case kVkApps:
            event.kind = KeyClass::kIgnored;
            return event;
        default:
            break;
    }

    // Numpad keys are never transformed: the numeric keypad belongs to the
    // application (regression TEST-P0-020 / TEST-P0-021).
    if (numpad || (vk >= kVkNumpad0 && vk <= kVkNumpad9) ||
        (vk >= kVkMultiply && vk <= kVkDivide)) {
        event.kind = KeyClass::kOther;
        return event;
    }

    // Function keys, navigation and other system keys.
    if ((vk >= kVkF1 && vk <= kVkF24) ||
        (vk >= kVkPrior && vk <= kVkHelp) ||
        vk == kVkClear || vk == kVkPause) {
        event.kind = KeyClass::kOther;
        return event;
    }

    switch (vk) {
        case kVkBack:    event.kind = KeyClass::kBackspace; return event;
        case kVkTab:     event.kind = KeyClass::kTab;       return event;
        case kVkReturn:  event.kind = KeyClass::kEnter;     return event;
        case kVkEscape:  event.kind = KeyClass::kEscape;    return event;
        case kVkSpace:   event.kind = KeyClass::kSpace;     return event;
        default: break;
    }

    if (IsAsciiLetter(produced_char)) {
        event.kind = KeyClass::kLetter;
        return event;
    }
    if (IsAsciiDigit(produced_char)) {
        event.kind = KeyClass::kDigit;
        return event;
    }

    // Printable punctuation / symbols act as word boundaries. Letters are
    // resolved from `produced_char`, so layout and Shift handling stay with the
    // host (a Shift+digit symbol therefore arrives as kBoundary, not kDigit).
    if (produced_char >= 0x21 && produced_char != 0x7F) {
        event.kind = KeyClass::kBoundary;
        return event;
    }

    event.kind = KeyClass::kOther;
    return event;
}

bool ShouldHandleKeys(InputMode mode, bool tsf_active_for_app) {
    switch (mode) {
        case InputMode::kTsfOnly:       return false;
        case InputMode::kUniversalOnly: return true;
        case InputMode::kAutomatic:
        default:                        return !tsf_active_for_app;
    }
}

void UniversalTyping::SetEnabled(bool enabled) {
    enabled_ = enabled;
    if (!enabled_) {
        roman_.clear();
        skip_word_ = false;
    }
}

void UniversalTyping::Reset() {
    roman_.clear();
    skip_word_ = false;
}

Decision UniversalTyping::Boundary(const std::wstring& trailing) {
    Decision decision;
    skip_word_ = false;
    if (roman_.empty()) {
        return decision;  // safety contract rule 3: plain pass-through
    }
    decision.kind = ActionKind::kReplace;
    decision.erase_chars = static_cast<int>(roman_.size());
    decision.roman = roman_;
    decision.trailing = trailing;
    roman_.clear();
    return decision;
}

Decision UniversalTyping::Feed(const KeyEvent& event) {
    Decision decision;

    if (!enabled_) {
        roman_.clear();
        skip_word_ = false;
        return decision;  // safety contract rule 4
    }

    // Any modifier combination belongs to Windows and the application. The
    // buffered word is deliberately kept: a shortcut in the middle of a word
    // must not destroy the preview.
    if (event.ctrl || event.alt || event.win || event.kind == KeyClass::kIgnored) {
        return decision;  // safety contract rule 1
    }

    switch (event.kind) {
        case KeyClass::kLetter: {
            if (!IsAsciiLetter(event.ch)) return decision;
            if (skip_word_) return decision;  // overflowed word: never touched
            if (static_cast<int>(roman_.size()) >= kMaxRoman) {
                // Overflow: drop the preview and leave the rest of this word
                // completely alone - no erasure can ever target a character we
                // did not account for (safety contract rule 5).
                roman_.clear();
                skip_word_ = true;
                return decision;
            }
            roman_.push_back(ToLowerAscii(event.ch));
            decision.kind = ActionKind::kBuffer;
            return decision;
        }

        case KeyClass::kBackspace:
            // Native editing: the application deletes its own character, we only
            // keep the preview in sync (regression TEST-P0-031).
            if (!roman_.empty()) roman_.pop_back();
            return decision;

        case KeyClass::kEscape:
            // Esc always keeps its native meaning (close menu, clear field);
            // it only drops our preview.
            roman_.clear();
            return decision;

        case KeyClass::kSpace:
            return Boundary(L" ");

        case KeyClass::kEnter:
            return Boundary(L"\r");

        case KeyClass::kTab:
            return Boundary(L"\t");

        case KeyClass::kDigit:
            if (!IsAsciiDigit(event.ch)) return decision;
            return Boundary(std::wstring(1, event.ch));

        case KeyClass::kBoundary: {
            if (roman_.empty() || event.ch < 0x21 || event.ch == 0x7F) {
                return decision;
            }
            // "." becomes the Bengali full stop, matching the TSF path.
            const wchar_t out = (event.ch == L'.') ? L'\u0964' : event.ch;
            return Boundary(std::wstring(1, out));
        }

        case KeyClass::kOther:
            // The caret may have moved, so the buffered word no longer describes
            // the text before it. Drop the preview; never erase host text.
            roman_.clear();
            skip_word_ = false;
            return decision;

        default:
            return decision;
    }
}

} // namespace universal
} // namespace likhi
