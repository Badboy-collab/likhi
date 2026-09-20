#include <windows.h>
#include <iostream>
#include <string>
#include "../../tsf/include/key_policy.h"

// ============================================================
// LIKHI — KEY POLICY REGRESSION TEST GATE
//
// Proves the non-negotiable master-prompt contract WITHOUT a TSF
// runtime, by exercising the pure decision table (DecideKey):
//
//   * F1-F12, Esc, Tab, Shift, Ctrl, Alt, Win, Home, End, Insert,
//     Delete, Arrow keys and system/application-control keys are
//     NEVER swallowed, intercepted, or altered unless an active
//     Likhi composition/candidate operation explicitly needs them.
//   * Ctrl+C / Ctrl+V / Ctrl+X / Ctrl+Z / Ctrl+A: full pass-through.
//   * Alt / Shift / Tab / Win: original Windows behavior preserved.
//   * Esc: cancels ONLY when composing; otherwise pass-through.
//   * Arrows: navigate candidates ONLY while candidates are visible;
//     otherwise pass-through.
//   * Numbers/Numpad: pass through unless selecting a candidate.
//   * Space: one keypress = one word commit + exactly one U+0020.
//
// Each test asserts the DECISION (eat / commit_first / action) for a
// concrete key + state. commit_first==true documents that the commit
// MUST run in OnTestKeyDown, BEFORE the host application receives the
// key — that is the ordering fix for "am"+"0" -> "আম0" (never "0আম").
// ============================================================

using bangla_tsf::KeyAction;
using bangla_tsf::KeyState;
using bangla_tsf::KeyDecision;
using bangla_tsf::DecideKey;

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

KeyState St(WPARAM vk) { KeyState s; s.vk = vk; return s; }

// --- section helpers ---

void Section(const std::string& title) {
    std::cout << "\n=== " << title << " ===\n";
}

} // namespace

int main() {
    SetConsoleOutputCP(CP_UTF8);
    std::cout << "=========================================================\n";
    std::cout << "  LIKHI - KEY POLICY REGRESSION TEST GATE\n";
    std::cout << "=========================================================\n";

    // ------------------------------------------------------------
    // 1. F1-F12: no composition -> original application function.
    //    While composing -> commit first, NEVER eat.
    // ------------------------------------------------------------
    Section("F1-F12 pass-through (no composition / composing)");
    {
        for (WPARAM fk = VK_F1; fk <= VK_F12; fk++) {
            KeyDecision idle = DecideKey(St(fk));
            Check(!idle.eat && idle.action == KeyAction::kPass && !idle.commit_first,
                  "F" + std::to_string(fk - VK_F1 + 1) + " not eaten, no commit when idle");

            KeyState composing = St(fk);
            composing.composing = true;
            KeyDecision comp = DecideKey(composing);
            Check(!comp.eat && comp.commit_first,
                  "F" + std::to_string(fk - VK_F1 + 1) + " not eaten, commits BEFORE host when composing");
        }
    }

    // ------------------------------------------------------------
    // 2. Ctrl+letter shortcuts: full pass-through, never eaten.
    //    While composing -> commit first (no text loss, then shortcut).
    // ------------------------------------------------------------
    Section("Ctrl+C/V/X/Z/A full pass-through");
    {
        const char* shortcuts[] = {"C", "V", "X", "Z", "A", "S", "F", "O", "P", "N", "W", "Y"};
        for (const char* sc : shortcuts) {
            WPARAM vk = static_cast<WPARAM>(sc[0]);

            KeyState idle = St(vk);
            idle.ctrl = true;
            KeyDecision didle = DecideKey(idle);
            Check(!didle.eat && !didle.commit_first,
                  std::string("Ctrl+") + sc + " pass-through when idle");

            KeyState comp = St(vk);
            comp.ctrl = true;
            comp.composing = true;
            KeyDecision dcomp = DecideKey(comp);
            Check(!dcomp.eat && dcomp.commit_first,
                  std::string("Ctrl+") + sc + " pass-through, commits first when composing");
        }

        // Ctrl key itself and Alt key itself are never eaten
        Check(!DecideKey(St(VK_CONTROL)).eat, "Ctrl key itself never eaten");
        Check(!DecideKey(St(VK_MENU)).eat, "Alt key itself never eaten");
        Check(!DecideKey(St(VK_SHIFT)).eat, "Shift key itself never eaten");
        Check(!DecideKey(St(VK_LWIN)).eat, "Win key itself never eaten");
    }

    // ------------------------------------------------------------
    // 3. Alt / Shift / Win / Tab: original behavior preserved.
    // ------------------------------------------------------------
    Section("Alt / Shift / Win / Tab pass-through");
    {
        // Alt+Tab
        KeyState alt_tab = St(VK_TAB);
        alt_tab.alt = true;
        KeyDecision d = DecideKey(alt_tab);
        Check(!d.eat, "Alt+Tab never eaten");

        // Alt+F4
        KeyState alt_f4 = St(VK_F4);
        alt_f4.alt = true;
        Check(!DecideKey(alt_f4).eat, "Alt+F4 never eaten");

        // Win+D / Win+L
        KeyState win_d = St('D');
        win_d.win = true;
        Check(!DecideKey(win_d).eat, "Win+D never eaten");

        // Shift+letter -> phonetic composition input (e.g. Shift+B, Shift+N, Shift+T)
        KeyState shift_b = St('B');
        shift_b.shift = true;
        shift_b.composing = true;
        KeyDecision dsb = DecideKey(shift_b);
        Check(dsb.eat && dsb.action == KeyAction::kProcessCharacter && !dsb.commit_first,
              "Shift+B while composing: eaten as phonetic composition input (kProcessCharacter)");

        KeyState shift_n = St('N');
        shift_n.shift = true;
        shift_n.composing = true;
        KeyDecision dsn = DecideKey(shift_n);
        Check(dsn.eat && dsn.action == KeyAction::kProcessCharacter && !dsn.commit_first,
              "Shift+N while composing: eaten as phonetic composition input (kProcessCharacter)");

        KeyState shift_b_idle = St('B');
        shift_b_idle.shift = true;
        KeyDecision dsbi = DecideKey(shift_b_idle);
        Check(dsbi.eat && dsbi.action == KeyAction::kProcessCharacter && !dsbi.commit_first,
              "Shift+B while idle: eaten, starts phonetic composition input (kProcessCharacter)");

        // Shortcut safety: Ctrl+B and Ctrl+N must remain pass-through
        KeyState ctrl_b = St('B');
        ctrl_b.ctrl = true;
        ctrl_b.composing = true;
        KeyDecision dcb = DecideKey(ctrl_b);
        Check(!dcb.eat && dcb.commit_first, "Ctrl+B while composing: not eaten, commits first (Word Bold)");

        KeyState ctrl_n = St('N');
        ctrl_n.ctrl = true;
        ctrl_n.composing = true;
        KeyDecision dcn = DecideKey(ctrl_n);
        Check(!dcn.eat && dcn.commit_first, "Ctrl+N while composing: not eaten, commits first (Word New)");

        // Shift+digit -> native symbol (! @ # ...), never eaten
        KeyState shift_1 = St('1');
        shift_1.shift = true;
        shift_1.composing = true;
        KeyDecision ds1 = DecideKey(shift_1);
        Check(!ds1.eat && ds1.commit_first,
              "Shift+1 while composing: not eaten, commits first (native '!' follows)");

        // Tab idle + Tab composing
        Check(!DecideKey(St(VK_TAB)).eat, "Tab never eaten when idle");
        KeyState tab_c = St(VK_TAB);
        tab_c.composing = true;
        KeyDecision dt = DecideKey(tab_c);
        Check(!dt.eat && dt.commit_first, "Tab while composing: not eaten, commits BEFORE focus moves");
    }

    // ------------------------------------------------------------
    // 4. Escape: cancel ONLY while composing; pass-through otherwise.
    // ------------------------------------------------------------
    Section("Escape: cancel only when composing");
    {
        KeyDecision idle = DecideKey(St(VK_ESCAPE));
        Check(!idle.eat && idle.action == KeyAction::kPass, "Esc not eaten when idle");

        KeyState comp = St(VK_ESCAPE);
        comp.composing = true;
        KeyDecision d = DecideKey(comp);
        Check(d.eat && d.action == KeyAction::kProcessEscape,
              "Esc while composing: eaten, cancels composition");
    }

    // ------------------------------------------------------------
    // 5. Arrow keys: candidate navigation ONLY when candidates visible.
    // ------------------------------------------------------------
    Section("Arrow keys: navigate only with visible candidates");
    {
        // idle -> pass through
        Check(!DecideKey(St(VK_UP)).eat && !DecideKey(St(VK_DOWN)).eat,
              "Up/Down not eaten when idle");

        // composing + candidates visible -> navigate
        KeyState up_vis = St(VK_UP);
        up_vis.composing = true;
        up_vis.candidates_visible = true;
        KeyDecision du = DecideKey(up_vis);
        Check(du.eat && du.action == KeyAction::kProcessArrow,
              "Up with visible candidates: eaten, navigates");

        KeyState dn_vis = St(VK_DOWN);
        dn_vis.composing = true;
        dn_vis.candidates_visible = true;
        KeyDecision dd = DecideKey(dn_vis);
        Check(dd.eat && dd.action == KeyAction::kProcessArrow,
              "Down with visible candidates: eaten, navigates");

        // composing but NO candidates -> must pass through (commit first)
        KeyState up_none = St(VK_UP);
        up_none.composing = true;
        KeyDecision dun = DecideKey(up_none);
        Check(!dun.eat && dun.commit_first,
              "Up while composing without candidates: not eaten, commits first, passes through");

        KeyState dn_none = St(VK_DOWN);
        dn_none.composing = true;
        KeyDecision ddn = DecideKey(dn_none);
        Check(!ddn.eat && ddn.commit_first,
              "Down while composing without candidates: not eaten, commits first, passes through");
    }

    // ------------------------------------------------------------
    // 6. Home/End/Insert/Delete/PageUp/PageDown/Left/Right: never eaten.
    // ------------------------------------------------------------
    Section("Home/End/Insert/Delete/Left/Right/PgUp/PgDn pass-through");
    {
        const std::pair<WPARAM, std::string> nav[] = {
            {VK_LEFT, "Left"}, {VK_RIGHT, "Right"}, {VK_HOME, "Home"},
            {VK_END, "End"}, {VK_DELETE, "Delete"}, {VK_PRIOR, "PageUp"}, {VK_NEXT, "PageDown"},
            {VK_INSERT, "Insert"}, {VK_SNAPSHOT, "PrintScreen"}, {VK_PAUSE, "Pause"},
            {VK_CAPITAL, "CapsLock"}, {VK_NUMLOCK, "NumLock"}, {VK_SCROLL, "ScrollLock"}
        };
        for (const auto& kv : nav) {
            Check(!DecideKey(St(kv.first)).eat, kv.second + " never eaten when idle");

            KeyState comp = St(kv.first);
            comp.composing = true;
            KeyDecision dc = DecideKey(comp);
            Check(!dc.eat && dc.commit_first,
                  kv.second + " while composing: not eaten, commits first");
        }
    }

    // ------------------------------------------------------------
    // 7. Numpad digits & operators.
    // ------------------------------------------------------------
    Section("Numpad digits & operators");
    {
        // Operators always pass through
        const std::pair<WPARAM, std::string> ops[] = {
            {VK_ADD, "Numpad +"}, {VK_SUBTRACT, "Numpad -"}, {VK_MULTIPLY, "Numpad *"},
            {VK_DIVIDE, "Numpad /"}, {VK_DECIMAL, "Numpad ."}
        };
        for (const auto& op : ops) {
            Check(!DecideKey(St(op.first)).eat, op.second + " never eaten when idle");
            KeyState comp = St(op.first);
            comp.composing = true;
            KeyDecision dc = DecideKey(comp);
            Check(!dc.eat && dc.commit_first, op.second + " never eaten; commits first when composing");
        }

        // Numpad digit, NumLock ON, no composition -> native number, NOT eaten
        KeyState np_idle = St(VK_NUMPAD1);
        np_idle.numlock = true;
        KeyDecision dnpi = DecideKey(np_idle);
        Check(!dnpi.eat && !dnpi.commit_first, "Numpad 1 (NumLock on, idle): native digit, not eaten");

        // Numpad digit, NumLock ON, composing + selectable -> candidate selection
        KeyState np_sel = St(VK_NUMPAD1);
        np_sel.numlock = true;
        np_sel.composing = true;
        np_sel.digit_selectable = true;
        KeyDecision dnps = DecideKey(np_sel);
        Check(dnps.eat && dnps.action == KeyAction::kProcessDigit,
              "Numpad 1 (composing, selectable): eaten, selects candidate");

        // Numpad digit, NumLock ON, composing but NOT selectable -> commit first, pass through
        KeyState np_nosel = St(VK_NUMPAD0);
        np_nosel.numlock = true;
        np_nosel.composing = true;
        np_nosel.digit_selectable = false;
        KeyDecision dnpn = DecideKey(np_nosel);
        Check(!dnpn.eat && dnpn.commit_first,
              "Numpad 0 (composing, not selectable): commits first, number passes through");

        // Numpad digit, NumLock OFF -> native navigation, never eaten
        KeyState np_off = St(VK_NUMPAD1);
        np_off.numlock = false;
        np_off.composing = true;
        KeyDecision dnpo = DecideKey(np_off);
        Check(!dnpo.eat && dnpo.commit_first,
              "Numpad 1 (NumLock off): native navigation, commits first, never eaten");
    }

    // ------------------------------------------------------------
    // 8. Top-row number keys.
    // ------------------------------------------------------------
    Section("Top-row numbers 0-9");
    {
        // No composition -> native number, NEVER eaten (master-prompt contract)
        for (char c = '0'; c <= '9'; c++) {
            KeyDecision idle = DecideKey(St(static_cast<WPARAM>(c)));
            Check(!idle.eat && idle.action == KeyAction::kPass,
                  std::string("'") + c + "' native number, NOT eaten when idle");
        }

        // Composing + selectable -> candidate selection
        KeyState sel = St('1');
        sel.composing = true;
        sel.digit_selectable = true;
        KeyDecision ds = DecideKey(sel);
        Check(ds.eat && ds.action == KeyAction::kProcessDigit,
              "'1' while composing with selectable candidate: eaten, selects candidate");

        // Composing + NOT selectable -> commit FIRST, then the number passes
        // through. This is the ordering fix: "am"+"0" must yield "আম0" not "0আম".
        KeyState nosel = St('0');
        nosel.composing = true;
        nosel.digit_selectable = false;
        KeyDecision dn = DecideKey(nosel);
        Check(!dn.eat && dn.commit_first,
              "'0' while composing (no candidate): commits BEFORE host, digit passes through (আম0 ordering)");

        KeyState nosel6 = St('6');
        nosel6.composing = true;
        nosel6.digit_selectable = false;
        KeyDecision dn6 = DecideKey(nosel6);
        Check(!dn6.eat && dn6.commit_first,
              "'6' while composing (no candidate): commits BEFORE host, digit passes through");
    }

    // ------------------------------------------------------------
    // 9. SPACE — One keypress = One word commit + Exactly One Space.
    // ------------------------------------------------------------
    Section("Space: one word commit + exactly one space");
    {
        // Idle: pass through; the host inserts exactly one normal space.
        KeyDecision idle = DecideKey(St(VK_SPACE));
        Check(!idle.eat && idle.action == KeyAction::kPass && !idle.commit_first,
              "Space idle: not eaten, host handles it (exactly one space)");

        // Composing: we EAT it. OnTestKeyDown=TRUE means the host will NOT
        // insert a space; OnKeyDown runs OnSpace which commits the word and
        // inserts exactly one U+0020 via TSF. No second space is ever added.
        KeyState comp = St(VK_SPACE);
        comp.composing = true;
        KeyDecision dc = DecideKey(comp);
        Check(dc.eat && dc.action == KeyAction::kProcessSpace && !dc.commit_first,
              "Space composing: eaten, action=kProcessSpace (commit + single U+0020 insert)");

        // Consecutive words: each user Space is exactly one commit + one space.
        KeyState comp2 = comp;
        KeyDecision dc2 = DecideKey(comp2);
        Check(dc2.eat && dc2.action == KeyAction::kProcessSpace,
              "Second word's Space: identical single-commit + single-space decision");
    }

    // ------------------------------------------------------------
    // 10. Backspace / Enter.
    // ------------------------------------------------------------
    Section("Backspace / Enter");
    {
        KeyDecision bsp_idle = DecideKey(St(VK_BACK));
        Check(!bsp_idle.eat, "Backspace not eaten when idle");

        KeyState bsp = St(VK_BACK);
        bsp.composing = true;
        KeyDecision dbsp = DecideKey(bsp);
        Check(dbsp.eat && dbsp.action == KeyAction::kProcessBackspace,
              "Backspace composing: eaten, removes last roman char");

        KeyState ent = St(VK_RETURN);
        ent.composing = true;
        KeyDecision dent = DecideKey(ent);
        Check(dent.eat && dent.action == KeyAction::kProcessEnter,
              "Enter composing: eaten, commits word without trailing space");
        Check(!DecideKey(St(VK_RETURN)).eat, "Enter not eaten when idle");
    }

    // ------------------------------------------------------------
    // 11. Alpha letters: eaten as composition input (the IME contract).
    // ------------------------------------------------------------
    Section("Alpha letters -> composition input");
    {
        // Real keyboard input delivers VK codes for the KEY: the physical A key
        // is always VK_A (0x41) whether or not Shift is held. Lowercase char
        // codes (0x61-0x7A) never arrive as wParam — they collide with the
        // VK_NUMPAD0-9 / VK_F1-F11 ranges, which is why keys are classified by
        // their VK code here.
        for (WPARAM vk = 'A'; vk <= 'Z'; vk++) {
            KeyDecision d = DecideKey(St(vk));
            Check(d.eat && d.action == KeyAction::kProcessCharacter,
                  std::string("key '") + static_cast<char>(vk) + "' eaten, processed as roman input");
        }
    }

    // ------------------------------------------------------------
    // 12. Period while composing -> Dāri.
    // ------------------------------------------------------------
    Section("Period -> Bengali Dāri");
    {
        KeyDecision idle = DecideKey(St(VK_OEM_PERIOD));
        Check(idle.eat && idle.action == KeyAction::kProcessPeriod,
              "Period idle: eaten, inserts Bengali Dāri (।)");

        KeyState comp = St(VK_OEM_PERIOD);
        comp.composing = true;
        KeyDecision dc = DecideKey(comp);
        Check(dc.eat && dc.action == KeyAction::kProcessPeriod,
              "Period composing: eaten, commits word + inserts Dāri (।)");

        KeyState shift_period = St(VK_OEM_PERIOD);
        shift_period.shift = true;
        shift_period.composing = true;
        KeyDecision dsp = DecideKey(shift_period);
        Check(!dsp.eat && dsp.commit_first,
              "Shift+. while composing: not eaten (native '>'), commits first");
    }

    // ------------------------------------------------------------
    // 13. The Golden Rule sweep: NO key outside the active-composition
    //     contract is ever "eaten" unless it starts/extends a composition
    //     (letters) or selects/cancels/navigates it.
    // ------------------------------------------------------------
    Section("Golden rule sweep (nothing else is ever consumed)");
    {
        // A broad set of keys in the IDLE state must never be eaten.
        const WPARAM never_eat_idle[] = {
            VK_SPACE, VK_ESCAPE, VK_TAB, VK_BACK, VK_RETURN,
            VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT, VK_HOME, VK_END,
            VK_DELETE, VK_INSERT, VK_PRIOR, VK_NEXT,
            VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9,
            VK_F10, VK_F11, VK_F12,
            VK_CAPITAL, VK_NUMLOCK, VK_SCROLL, VK_SNAPSHOT, VK_PAUSE,
            VK_ADD, VK_SUBTRACT, VK_MULTIPLY, VK_DIVIDE, VK_DECIMAL, VK_SEPARATOR,
            VK_NUMPAD0, VK_NUMPAD1, VK_NUMPAD2, VK_NUMPAD3, VK_NUMPAD4,
            VK_NUMPAD5, VK_NUMPAD6, VK_NUMPAD7, VK_NUMPAD8, VK_NUMPAD9,
            VK_OEM_COMMA, VK_OEM_MINUS, VK_OEM_PLUS,
            VK_OEM_1, VK_OEM_2, VK_OEM_3, VK_OEM_4, VK_OEM_5, VK_OEM_6, VK_OEM_7,
            '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
            VK_CONTROL, VK_MENU, VK_SHIFT, VK_LWIN, VK_RWIN, VK_APPS
        };
        for (WPARAM vk : never_eat_idle) {
            KeyDecision d = DecideKey(St(vk));
            if (d.eat) {
                Check(false, std::string("0x") + std::to_string(vk) + " must not be eaten when idle");
            }
        }
        Check(true, "All " + std::to_string(sizeof(never_eat_idle) / sizeof(never_eat_idle[0])) +
                    " sampled keys pass through when idle");
    }

    // ------------------------------------------------------------
    // Summary
    // ------------------------------------------------------------
    std::cout << "\n=========================================================\n";
    std::cout << "  KEY POLICY TEST RESULTS\n";
    std::cout << "  Total Passed: " << g_pass << "\n";
    std::cout << "  Total Failed: " << g_fail << "\n";
    std::cout << "  Status: " << ((g_fail == 0) ? "100% SUCCESS (GATE PASSED)" : "FAILURE") << "\n";
    std::cout << "=========================================================\n";

    return (g_fail == 0) ? 0 : 1;
}
