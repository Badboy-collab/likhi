#ifndef LIKHI_UNIVERSAL_TYPING_H
#define LIKHI_UNIVERSAL_TYPING_H

// ============================================================================
// LIKHI - UNIVERSAL MODE : input decision layer (pure, no Windows, no engine)
// ============================================================================
//
// WHY THIS EXISTS
//   TSF is Likhi's primary input path, but some hosts cannot be reached through
//   it: UWP/Store applications (for example WhatsApp Desktop), a few Java UIs,
//   game engines and remote sessions. Universal Mode is the explicit fallback:
//   the roman letters travel to the host unchanged, and when a word boundary
//   arrives the roman run is erased with synthetic backspaces and replaced by
//   the Bengali word typed as Unicode keys - so any application that accepts
//   keyboard input can be typed into.
//
// WHAT THIS FILE IS
//   Pure decision logic only. The Win32 host converts raw key information into
//   a KeyEvent, feeds it here and performs the returned Decision. No windows.h,
//   no engine call, no file or network access, no global state - which makes the
//   safety rules below unit-testable without a desktop session
//   (tests/unit/test_universal_typing.cpp).
//
// SAFETY CONTRACT (regression-protected)
//   1. Only plain character input is ever transformed. Every modifier
//      combination (Ctrl / Alt / Win), function key, navigation key, numpad
//      key and any unknown/system key is reported as pass-through - whatever
//      the mode is and whatever is buffered.
//   2. At most ONE key per word is consumed: the word boundary itself, which
//      the host then synthesises. Nothing else is swallowed. Backspace and Esc
//      keep their native meaning and only adjust the preview.
//   3. With an empty buffer every key passes through, so ordinary spaces,
//      punctuation and digits are never touched.
//   4. With the mode off the buffer stays empty and every event passes through.
//   5. The buffer never grows past kMaxRoman characters.
//   6. The buffer is dropped (never erased from the host) whenever the caret may
//      have moved or focus changed, so the layer can never delete foreign text.
//
// The transliteration itself is NOT done here: Decision carries the roman word
// and the host asks the one shared language engine for the Bengali text, so TSF
// and Universal mode always use the same dictionary, fuzzy matcher, personal
// dictionary and personalisation model.

#include <string>

namespace likhi {
namespace universal {

// ---------------------------------------------------------------------------
// Input mode (Settings -> Input Mode)
// ---------------------------------------------------------------------------
enum class InputMode {
    kAutomatic = 0,   // prefer TSF, use the universal backend only where TSF is
                      // not the active input for the focused application
    kTsfOnly = 1,     // never use the universal backend
    kUniversalOnly = 2// always use the universal backend
};

// ---------------------------------------------------------------------------
// Key classification
// ---------------------------------------------------------------------------
enum class KeyClass {
    kLetter,     // ASCII letter typed without Ctrl/Alt/Win
    kDigit,      // top-row digit, word boundary
    kSpace,      // word boundary
    kEnter,      // word boundary
    kTab,        // word boundary
    kBackspace,  // native editing; adjusts the preview only
    kEscape,     // native behaviour; drops the preview only
    kBoundary,   // punctuation / symbol ending the word
    kIgnored,    // harmless on its own: modifiers, locks, the Menu key
    kOther       // navigation / function / numpad / system keys
};

// Win32 virtual-key codes, repeated here so this layer stays independent of
// <windows.h> and its tests do not need a Windows header either.
enum : int {
    kVkBack = 0x08, kVkTab = 0x09, kVkClear = 0x0C, kVkReturn = 0x0D,
    kVkShift = 0x10, kVkControl = 0x11, kVkMenu = 0x12, kVkPause = 0x13,
    kVkCapital = 0x14, kVkEscape = 0x1B, kVkSpace = 0x20,
    kVkPrior = 0x21, kVkNext = 0x22, kVkEnd = 0x23, kVkHome = 0x24,
    kVkLeft = 0x25, kVkUp = 0x26, kVkRight = 0x27, kVkDown = 0x28,
    kVkSelect = 0x29, kVkPrint = 0x2A, kVkExecute = 0x2B, kVkSnapshot = 0x2C,
    kVkInsert = 0x2D, kVkDelete = 0x2E, kVkHelp = 0x2F,
    kVkLWin = 0x5B, kVkRWin = 0x5C, kVkApps = 0x5D,
    kVkNumpad0 = 0x60, kVkNumpad9 = 0x69, kVkMultiply = 0x6A, kVkAdd = 0x6B,
    kVkSeparator = 0x6C, kVkSubtract = 0x6D, kVkDecimal = 0x6E, kVkDivide = 0x6F,
    kVkF1 = 0x70, kVkF24 = 0x87, kVkNumlock = 0x90, kVkScroll = 0x91,
    kVkLShift = 0xA0, kVkRShift = 0xA1, kVkLControl = 0xA2, kVkRControl = 0xA3,
    kVkLMenu = 0xA4, kVkRMenu = 0xA5
};

struct KeyEvent {
    KeyClass kind = KeyClass::kOther;
    wchar_t ch = 0;        // produced character for kLetter/kDigit/kBoundary
    bool ctrl = false;
    bool alt = false;
    bool win = false;
    bool shift = false;
};

// What the host must do with the key it has just received.
enum class ActionKind {
    kPassThrough = 0,  // let the key reach the application untouched
    kBuffer = 1,       // let the key reach the application AND remember it
    kReplace = 2       // consume the key: erase `erase_chars`, type the word,
                       // then type `trailing`
};

struct Decision {
    ActionKind kind = ActionKind::kPassThrough;
    int erase_chars = 0;        // kReplace: roman letters to erase (backspaces)
    std::wstring roman;         // kReplace: the word to transliterate
    std::wstring trailing;      // kReplace: boundary text typed after the word
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Classifies one raw key. `produced_char` is the character the key would insert
// (0 when unknown); `numpad` marks keys reported with the extended-key flag by
// the numpad. Never transforms anything by itself.
KeyEvent ClassifyKey(int vk, wchar_t produced_char, bool ctrl, bool alt, bool win,
                     bool shift, bool numpad);

// True when the universal backend should process keys for the focused
// application. `tsf_active_for_app` is true when the focused thread's keyboard
// layout is Likhi's TSF profile (the host checks that through GetKeyboardLayout).
bool ShouldHandleKeys(InputMode mode, bool tsf_active_for_app);

// ---------------------------------------------------------------------------
// The decision layer
// ---------------------------------------------------------------------------
class UniversalTyping {
public:
    // Longer input drops the preview, so a stuck key can never grow it without
    // limit (the extra letter then starts a new word).
    static constexpr int kMaxRoman = 32;

    void SetEnabled(bool enabled);
    bool enabled() const { return enabled_; }

    // The roman word currently buffered (the host may show it as a preview).
    const std::wstring& roman() const { return roman_; }
    bool has_pending() const { return !roman_.empty(); }

    // Drops the preview without touching the application's text. Called on focus
    // change, mouse clicks, clipboard operations and anything else that makes the
    // buffered word unreliable (safety contract rule 6).
    void Reset();

    // Feeds one key. The returned Decision is the complete instruction set: the
    // layer itself performs no I/O and keeps no state the host cannot see.
    Decision Feed(const KeyEvent& event);

private:
    // Builds a word-boundary decision; with an empty buffer it stays a plain
    // pass-through (safety contract rule 3).
    Decision Boundary(const std::wstring& trailing);

    bool enabled_ = false;
    std::wstring roman_;
    // Set when the preview overflows: the rest of that word is left completely
    // alone (letters pass through untouched, nothing is ever erased for it).
    bool skip_word_ = false;
};

} // namespace universal
} // namespace likhi

#endif // LIKHI_UNIVERSAL_TYPING_H
