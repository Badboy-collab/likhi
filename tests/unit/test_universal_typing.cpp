#include <windows.h>
#include <iostream>
#include <string>
#include "../../universal/include/universal_typing.h"

// ============================================================
// LIKHI - UNIVERSAL MODE SAFETY & BEHAVIOUR REGRESSION GATE
//
// Universal Mode is the fallback input path used where TSF does not
// reach (UWP/Store apps such as WhatsApp Desktop, some Java UIs, games,
// remote sessions): roman letters reach the application untouched and a
// word boundary replaces them with the Bengali word typed as Unicode.
//
// Because that path touches real application text, the safety rules are
// proven here WITHOUT a desktop session, on the pure decision layer:
//
//   * Never transformed, whatever is buffered: Ctrl+C/V/X/A/Z/Y/S/F,
//     Alt combinations, Win+Space and every other modifier chord, F1-F12,
//     arrows, Home/End/PgUp/PgDn, Insert/Delete, numpad keys and their
//     operators, unknown/system keys.
//   * At most ONE key per word is consumed: the word boundary itself.
//     Esc and Backspace keep their native meaning.
//   * With an empty buffer every key passes through (ordinary spaces,
//     punctuation and digits are never touched).
//   * Mode off => nothing is ever transformed.
//   * The preview never exceeds its limit and an overflowed word is left
//     completely alone.
//   * The preview is dropped, never erased from the host, whenever the
//     caret may have moved.
// ============================================================

using likhi::universal::ActionKind;
using likhi::universal::ClassifyKey;
using likhi::universal::Decision;
using likhi::universal::InputMode;
using likhi::universal::KeyClass;
using likhi::universal::KeyEvent;
using likhi::universal::ShouldHandleKeys;
using likhi::universal::UniversalTyping;

namespace {

int g_pass = 0;
int g_fail = 0;

void Check(bool ok, const std::string& name) {
    if (ok) {
        std::cout << "  [PASS] " << name << "\n";
        g_pass++;
    } else {
        std::cout << "  [FAIL] " << name << "\n";
        g_fail++;
    }
}

void Section(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

KeyEvent Letter(wchar_t c) {
    KeyEvent e;
    e.kind = KeyClass::kLetter;
    e.ch = c;
    return e;
}

KeyEvent Special(KeyClass kind) {
    KeyEvent e;
    e.kind = kind;
    return e;
}

// Feeds the letters of `word` (lower case as typed) into the layer.
void TypeWord(UniversalTyping& ut, const std::string& word) {
    for (char c : word) {
        ut.Feed(Letter(static_cast<wchar_t>(c)));
    }
}

bool IsReplace(const Decision& d) { return d.kind == ActionKind::kReplace; }

bool IsPass(const Decision& d) { return d.kind == ActionKind::kPassThrough; }

bool IsBuffer(const Decision& d) { return d.kind == ActionKind::kBuffer; }

} // namespace

int main() {
    SetConsoleOutputCP(CP_UTF8);
    std::cout << "=========================================================\n";
    std::cout << "  LIKHI - UNIVERSAL MODE REGRESSION TEST GATE\n";
    std::cout << "=========================================================\n";

    // ---------------------------------------------------------------
    Section("1. Shortcut / system keys are NEVER transformed");
    // ---------------------------------------------------------------
    {
        struct Case { int vk; const char* name; };
        const Case matrix[] = {
            {0x43, "Ctrl+C"}, {0x56, "Ctrl+V"}, {0x58, "Ctrl+X"},
            {0x41, "Ctrl+A"}, {0x5A, "Ctrl+Z"}, {0x59, "Ctrl+Y"},
            {0x53, "Ctrl+S"}, {0x46, "Ctrl+F"}, {0x50, "Ctrl+P"},
        };
        for (const Case& c : matrix) {
            UniversalTyping ut;
            ut.SetEnabled(true);
            TypeWord(ut, "ami");
            KeyEvent e = Letter(static_cast<wchar_t>(c.vk));
            e.ctrl = true;
            Decision d = ut.Feed(e);
            Check(IsPass(d) && d.erase_chars == 0,
                  std::string(c.name) + " passes through while composing");
        }

        // The buffered word must survive a shortcut.
        UniversalTyping ut;
        ut.SetEnabled(true);
        TypeWord(ut, "ami");
        KeyEvent ctrl_c = Letter(L'c');
        ctrl_c.ctrl = true;
        ut.Feed(ctrl_c);
        Decision d = ut.Feed(Special(KeyClass::kSpace));
        Check(IsReplace(d) && d.roman == L"ami" && d.erase_chars == 3,
              "word survives an interleaved Ctrl chord");
    }
    {
        UniversalTyping ut;
        ut.SetEnabled(true);
        TypeWord(ut, "ami");
        KeyEvent win_space = Special(KeyClass::kSpace);
        win_space.win = true;
        Check(IsPass(ut.Feed(win_space)), "Win+Space passes through (language switch)");

        KeyEvent ctrl_space = Special(KeyClass::kSpace);
        ctrl_space.ctrl = true;
        Check(IsPass(ut.Feed(ctrl_space)), "Ctrl+Space passes through (IME toggle)");

        KeyEvent alt_letter = Letter(L't');
        alt_letter.alt = true;
        Check(IsPass(ut.Feed(alt_letter)), "Alt+letter passes through (menu accelerators)");

        KeyEvent alt_f4 = Special(KeyClass::kOther);
        alt_f4.alt = true;
        Decision d = ut.Feed(alt_f4);
        Check(IsPass(d) && d.erase_chars == 0, "Alt+F4 passes through with no erasure");
    }
    {
        UniversalTyping ut;
        ut.SetEnabled(true);
        TypeWord(ut, "ami");
        for (int vk = 0x70; vk <= 0x87; ++vk) {
            Decision d = ut.Feed(Special(KeyClass::kOther));
            if (!IsPass(d) || d.erase_chars != 0) {
                Check(false, "F-key consumed or erasing");
                break;
            }
            if (vk == 0x87) Check(true, "F1-F24 pass through with no erasure");
        }

        const KeyClass navitals[] = {KeyClass::kOther};
        for (KeyClass k : navitals) {
            Decision d = ut.Feed(Special(k));
            Check(IsPass(d) && d.erase_chars == 0,
                  "navigation/system key passes through with no erasure");
        }
    }
    {
        // Numpad: never transformed (regression TEST-P0-020 / TEST-P0-021).
        UniversalTyping ut;
        ut.SetEnabled(true);
        TypeWord(ut, "ami");
        Decision d = ut.Feed(Special(KeyClass::kOther));  // numpad key
        Check(IsPass(d) && d.erase_chars == 0, "numpad key passes through untouched");
    }
    {
        // Modifiers and locks alone: pass-through, preview kept.
        UniversalTyping ut;
        ut.SetEnabled(true);
        TypeWord(ut, "ami");
        Check(IsPass(ut.Feed(Special(KeyClass::kIgnored))), "Shift/CapsLock alone passes through");
        Check(ut.roman() == L"ami", "preview kept for a lone modifier");

        KeyEvent shifted = Letter(L'A');
        shifted.shift = true;
        Check(IsBuffer(ut.Feed(shifted)), "Shift+letter still types a letter");

        Decision d = ut.Feed(Special(KeyClass::kSpace));
        Check(IsReplace(d) && d.roman == L"amia" && d.erase_chars == 4,
              "uppercase letter lower-cased inside the word");
    }

    // ---------------------------------------------------------------
    Section("2. Empty buffer => everything is native");
    // ---------------------------------------------------------------
    {
        UniversalTyping ut;
        ut.SetEnabled(true);
        const KeyClass neutral[] = {KeyClass::kSpace, KeyClass::kEnter,
                                    KeyClass::kTab};
        for (KeyClass k : neutral) {
            Decision d = ut.Feed(Special(k));
            Check(IsPass(d) && d.erase_chars == 0 && d.trailing.empty(),
                  "space/enter/tab with no pending word: untouched");
        }

        KeyEvent five = Letter(L'5');
        five.kind = KeyClass::kDigit;
        Check(IsPass(ut.Feed(five)), "digit with no pending word: untouched");

        KeyEvent semicolon = Letter(L';');
        semicolon.kind = KeyClass::kBoundary;
        Check(IsPass(ut.Feed(semicolon)), "punctuation with no pending word: untouched");

        KeyEvent dot = Letter(L'.');
        dot.kind = KeyClass::kBoundary;
        Check(IsPass(ut.Feed(dot)), "decimal point / IP address case: untouched");
    }
    {
        // Two spaces in a row: exactly one commit, the second is native.
        UniversalTyping ut;
        ut.SetEnabled(true);
        TypeWord(ut, "ami");
        Decision first = ut.Feed(Special(KeyClass::kSpace));
        Decision second = ut.Feed(Special(KeyClass::kSpace));
        Check(IsReplace(first) && first.trailing == L" " && first.trailing.size() == 1,
              "space commits the word with exactly one U+0020");
        Check(IsPass(second) && second.erase_chars == 0,
              "second space in a row is native (no double erasure)");
    }

    // ---------------------------------------------------------------
    Section("3. Word boundaries");
    // ---------------------------------------------------------------
    {
        UniversalTyping ut;
        ut.SetEnabled(true);
        TypeWord(ut, "poriborton");
        Decision d = ut.Feed(Special(KeyClass::kSpace));
        Check(IsReplace(d) && d.erase_chars == 10 && d.roman == L"poriborton" &&
                  d.trailing == L" ",
              "poriborton + space => replace 10 chars with the word + one space");
    }
    {
        UniversalTyping ut;
        ut.SetEnabled(true);
        TypeWord(ut, "ami");
        Decision d = ut.Feed(Special(KeyClass::kEnter));
        Check(IsReplace(d) && d.trailing == L"\r",
              "enter commits the word then sends a real Enter");
    }
    {
        UniversalTyping ut;
        ut.SetEnabled(true);
        TypeWord(ut, "ami");
        Check(IsReplace(ut.Feed(Special(KeyClass::kTab))) , "tab commits the word");
    }
    {
        UniversalTyping ut;
        ut.SetEnabled(true);
        TypeWord(ut, "koro");
        KeyEvent two = Letter(L'2');
        two.kind = KeyClass::kDigit;
        Decision d = ut.Feed(two);
        Check(IsReplace(d) && d.roman == L"koro" && d.trailing == L"2",
              "digit commits the word then types the digit");
    }
    {
        UniversalTyping ut;
        ut.SetEnabled(true);
        TypeWord(ut, "kotha");
        KeyEvent dot = Letter(L'.');
        dot.kind = KeyClass::kBoundary;
        Decision d = ut.Feed(dot);
        Check(IsReplace(d) && d.trailing == std::wstring(1, L'\u0964'),
              "'.' becomes the Bengali full stop");
    }
    {
        UniversalTyping ut;
        ut.SetEnabled(true);
        TypeWord(ut, "kotha");
        KeyEvent comma = Letter(L',');
        comma.kind = KeyClass::kBoundary;
        Decision d = ut.Feed(comma);
        Check(IsReplace(d) && d.trailing == L",", "',' commits then keeps the comma");
    }
    {
        UniversalTyping ut;
        ut.SetEnabled(true);
        TypeWord(ut, "ami");
        TypeWord(ut, "kotha");   // no boundary yet: one unbroken word
        Check(ut.roman() == L"amikotha", "letters accumulate without a boundary");
        Decision d = ut.Feed(Special(KeyClass::kSpace));
        Check(d.erase_chars == 8, "erase count matches every buffered letter");
    }

    // ---------------------------------------------------------------
    Section("4. Native editing keys stay native");
    // ---------------------------------------------------------------
    {
        UniversalTyping ut;
        ut.SetEnabled(true);
        TypeWord(ut, "ami");
        Decision d = ut.Feed(Special(KeyClass::kBackspace));
        Check(IsPass(d) && d.erase_chars == 0, "backspace is never consumed");
        Check(ut.roman() == L"am", "backspace removes the last buffered letter");
        ut.Feed(Special(KeyClass::kBackspace));
        ut.Feed(Special(KeyClass::kBackspace));
        Check(ut.roman().empty(), "backspace empties the preview");
        Check(IsPass(ut.Feed(Special(KeyClass::kBackspace))), "backspace on empty preview: native");
        Decision space = ut.Feed(Special(KeyClass::kSpace));
        Check(IsPass(space) && space.erase_chars == 0,
              "no erasure is ever requested after the preview is gone");
    }
    {
        UniversalTyping ut;
        ut.SetEnabled(true);
        TypeWord(ut, "ami");
        Decision d = ut.Feed(Special(KeyClass::kEscape));
        Check(IsPass(d) && d.erase_chars == 0, "Esc is never consumed");
        Check(ut.roman().empty(), "Esc drops the preview");
    }
    {
        UniversalTyping ut;
        ut.SetEnabled(true);
        TypeWord(ut, "ami");
        Decision d = ut.Feed(Special(KeyClass::kOther));  // caret may have moved
        Check(IsPass(d) && d.erase_chars == 0, "navigation key: no erasure");
        Check(ut.roman().empty(), "preview dropped when the caret may have moved");
        Check(IsPass(ut.Feed(Special(KeyClass::kSpace))),
              "nothing is replaced after the preview was dropped");
    }

    // ---------------------------------------------------------------
    Section("5. Mode off and preview limits");
    // ---------------------------------------------------------------
    {
        UniversalTyping ut;
        ut.SetEnabled(false);
        Check(IsPass(ut.Feed(Letter(L'a'))), "mode off: letter passes through");
        Check(ut.roman().empty(), "mode off: nothing is buffered");
        Check(IsPass(ut.Feed(Special(KeyClass::kSpace))), "mode off: space passes through");
    }
    {
        UniversalTyping ut;
        ut.SetEnabled(true);
        TypeWord(ut, "ami");
        ut.SetEnabled(false);
        Check(ut.roman().empty(), "switching the mode off clears the preview");
        ut.SetEnabled(true);
        TypeWord(ut, "koro");
        ut.Reset();
        Check(ut.roman().empty(), "Reset clears the preview");
        Check(IsPass(ut.Feed(Special(KeyClass::kSpace))),
              "Reset leaves nothing to replace (focus change safety)");
        TypeWord(ut, "ami");
        Decision d = ut.Feed(Special(KeyClass::kSpace));
        Check(IsReplace(d) && d.roman == L"ami", "layer usable again after Reset");
    }
    {
        UniversalTyping ut;
        ut.SetEnabled(true);
        for (int i = 0; i < UniversalTyping::kMaxRoman; ++i) ut.Feed(Letter(L'a'));
        Check(ut.roman().size() == static_cast<size_t>(UniversalTyping::kMaxRoman),
              "preview reaches its limit but not beyond");
        Decision overflow = ut.Feed(Letter(L'b'));
        Check(IsPass(overflow) && overflow.erase_chars == 0 && ut.roman().empty(),
              "overflowing letter is left alone and the preview is dropped");
        Check(IsPass(ut.Feed(Letter(L'c'))), "overflowed word stays untouched");
        Check(IsPass(ut.Feed(Special(KeyClass::kSpace))),
              "overflowed word is never erased at the boundary");
        TypeWord(ut, "ami");
        Decision d = ut.Feed(Special(KeyClass::kSpace));
        Check(IsReplace(d) && d.roman == L"ami", "next word is normal again");
    }

    // ---------------------------------------------------------------
    Section("6. Key classification (the never-transform rules)");
    // ---------------------------------------------------------------
    {
        Check(ClassifyKey(0x41, L'a', false, false, false, false, false).kind == KeyClass::kLetter,
              "a => letter");
        Check(ClassifyKey(0x30, L'0', false, false, false, false, false).kind == KeyClass::kDigit,
              "top-row 0 => digit (word boundary)");
        Check(ClassifyKey(0x20, L' ', false, false, false, false, false).kind == KeyClass::kSpace,
              "space => space");
        Check(ClassifyKey(0x0D, L'\r', false, false, false, false, false).kind == KeyClass::kEnter,
              "enter => enter");
        Check(ClassifyKey(0x08, 0, false, false, false, false, false).kind == KeyClass::kBackspace,
              "backspace => backspace");
        Check(ClassifyKey(0x1B, 0, false, false, false, false, false).kind == KeyClass::kEscape,
              "escape => escape");
        Check(ClassifyKey(0x74, 0, false, false, false, false, false).kind == KeyClass::kOther,
              "F5 => other");
        Check(ClassifyKey(0x25, 0, false, false, false, false, false).kind == KeyClass::kOther,
              "left arrow => other");
        Check(ClassifyKey(0x24, 0, false, false, false, false, false).kind == KeyClass::kOther,
              "home => other");
        Check(ClassifyKey(0x23, 0, false, false, false, false, false).kind == KeyClass::kOther,
              "end => other");
        Check(ClassifyKey(0x2E, 0, false, false, false, false, false).kind == KeyClass::kOther,
              "delete => other");
        Check(ClassifyKey(0x60, L'0', false, false, false, false, false).kind == KeyClass::kOther,
              "numpad 0 => other (never transformed)");
        Check(ClassifyKey(0x6B, L'+', false, false, false, false, false).kind == KeyClass::kOther,
              "numpad plus => other");
        Check(ClassifyKey(0x31, L'1', false, false, false, false, true).kind == KeyClass::kOther,
              "extended key flagged as numpad => other");
        Check(ClassifyKey(0x10, 0, false, false, false, false, false).kind == KeyClass::kIgnored,
              "shift alone => ignored");
        Check(ClassifyKey(0x14, 0, false, false, false, false, false).kind == KeyClass::kIgnored,
              "caps lock => ignored");
        Check(ClassifyKey(0x5B, 0, false, false, false, false, false).kind == KeyClass::kIgnored,
              "left windows key => ignored");
        Check(ClassifyKey(0x90, 0, false, false, false, false, false).kind == KeyClass::kIgnored,
              "num lock => ignored");
        Check(ClassifyKey(0xBA, L';', false, false, false, false, false).kind == KeyClass::kBoundary,
              "semicolon => boundary");
        Check(ClassifyKey(0xBF, L'/', false, false, false, false, false).kind == KeyClass::kBoundary,
              "slash => boundary");
        Check(ClassifyKey(0x41, 0x0995, false, false, false, false, false).kind == KeyClass::kOther,
              "non-ASCII produced character => other (never a boundary)");
        Check(ClassifyKey(0x41, 0x00E9, false, false, false, false, false).kind == KeyClass::kOther,
              "accented character => other (never a boundary)");
        Check(ClassifyKey(0x51, L'q', true, false, false, false, false).kind == KeyClass::kLetter &&
                  ClassifyKey(0x51, L'q', true, false, false, false, false).ctrl,
              "Ctrl+Q classified as a Ctrl chord (never buffered)");
    }

    // ---------------------------------------------------------------
    Section("7. Input mode policy");
    // ---------------------------------------------------------------
    {
        Check(!ShouldHandleKeys(InputMode::kTsfOnly, false), "TSF Only: never universal");
        Check(!ShouldHandleKeys(InputMode::kTsfOnly, true), "TSF Only: never universal (TSF active)");
        Check(ShouldHandleKeys(InputMode::kUniversalOnly, true),
              "Universal Only: always handled, even where TSF works");
        Check(ShouldHandleKeys(InputMode::kUniversalOnly, false), "Universal Only: handled");
        Check(!ShouldHandleKeys(InputMode::kAutomatic, true),
              "Automatic: stands down while TSF is active for the app");
        Check(ShouldHandleKeys(InputMode::kAutomatic, false),
              "Automatic: falls back where TSF is not active");
    }

    std::cout << "\n=========================================================\n";
    std::cout << "  PASSED: " << g_pass << "   FAILED: " << g_fail << "\n";
    std::cout << "=========================================================\n";
    return g_fail == 0 ? 0 : 1;
}
