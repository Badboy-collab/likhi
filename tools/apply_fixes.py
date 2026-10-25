import os
import json

# 1. Update likhi_universal.cpp
with open("tools/universal/likhi_universal.cpp", "r", encoding="utf-8") as f:
    c = f.read()

# Add g_last_click_pt
decl_old = "std::vector<std::wstring> g_current_candidates;"
decl_new = """std::vector<std::wstring> g_current_candidates;
static POINT g_last_click_pt = {0, 0};"""
assert decl_old in c, "decl_old not found"
c = c.replace(decl_old, decl_new, 1)

# Update MouseHookProc
hook_old = """        switch (wparam) {
            case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
            case WM_MOUSEWHEEL:  case WM_MOUSEHWHEEL: {"""
hook_new = """        switch (wparam) {
            case WM_LBUTTONDOWN:
                if (ms) {
                    g_last_click_pt = ms->pt;
                }
                // fall through
            case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
            case WM_MOUSEWHEEL:  case WM_MOUSEHWHEEL: {"""
assert hook_old in c, "hook_old not found"
c = c.replace(hook_old, hook_new, 1)

# Update GetCurrentCaretRect
caret_old = """RECT GetCurrentCaretRect() {
    RECT rc = {0, 0, 0, 0};
    HWND fg = GetForegroundWindow();
    if (fg) {
        DWORD thread_id = GetWindowThreadProcessId(fg, nullptr);
        GUITHREADINFO gti = {sizeof(GUITHREADINFO)};
        if (GetGUIThreadInfo(thread_id, &gti)) {
            if (gti.hwndCaret) {
                rc = gti.rcCaret;
                ClientToScreen(gti.hwndCaret, (LPPOINT)&rc.left);
                ClientToScreen(gti.hwndCaret, (LPPOINT)&rc.right);
            } else if (gti.hwndFocus) {
                GetWindowRect(gti.hwndFocus, &rc);
                rc.bottom = rc.top + 20;
                rc.right = rc.left + 5;
            }
        }
    }
    if (rc.left == 0 && rc.right == 0 && rc.top == 0 && rc.bottom == 0) {
        POINT pt;
        GetCursorPos(&pt);
        rc.left = pt.x;
        rc.top = pt.y;
        rc.right = pt.x + 2;
        rc.bottom = pt.y + 20;
    }
    return rc;
}"""

caret_new = """RECT GetCurrentCaretRect() {
    RECT rc = {0, 0, 0, 0};
    HWND fg = GetForegroundWindow();
    if (fg) {
        DWORD thread_id = GetWindowThreadProcessId(fg, nullptr);
        GUITHREADINFO gti = {sizeof(GUITHREADINFO)};
        if (GetGUIThreadInfo(thread_id, &gti)) {
            // 1. Genuine Win32 Caret (Notepad, Word, native inputs)
            if (gti.hwndCaret && (gti.rcCaret.left != 0 || gti.rcCaret.top != 0 ||
                                  gti.rcCaret.right != 0 || gti.rcCaret.bottom != 0)) {
                rc = gti.rcCaret;
                ClientToScreen(gti.hwndCaret, (LPPOINT)&rc.left);
                ClientToScreen(gti.hwndCaret, (LPPOINT)&rc.right);
                return rc;
            } else if (gti.hwndFocus) {
                RECT rcFocus = {0};
                GetWindowRect(gti.hwndFocus, &rcFocus);
                int fw = rcFocus.right - rcFocus.left;
                int fh = rcFocus.bottom - rcFocus.top;
                // Only treat hwndFocus as an edit box if it has reasonable control dimensions!
                // In Chrome, Edge, Electron, hwndFocus is the whole 1920x1000 web canvas!
                if (fw > 15 && fh > 10 && !(fw > 400 && fh > 250)) {
                    rc = rcFocus;
                    rc.bottom = rc.top + (fh > 40 ? 25 : fh);
                    rc.right = rc.left + 5;
                    return rc;
                }
            }
        }
    }

    // 2. Chrome / Edge / Gemini / Web fallback: Use the exact click point where the user focused the input!
    if (fg && (g_last_click_pt.x != 0 || g_last_click_pt.y != 0)) {
        RECT fg_rc = {0};
        GetWindowRect(fg, &fg_rc);
        if (PtInRect(&fg_rc, g_last_click_pt)) {
            rc.left = g_last_click_pt.x;
            rc.top = g_last_click_pt.y;
            rc.right = g_last_click_pt.x + 2;
            rc.bottom = g_last_click_pt.y + 24;
            return rc;
        }
    }

    // 3. Fallback to current cursor position
    POINT pt;
    if (GetCursorPos(&pt)) {
        rc.left = pt.x;
        rc.top = pt.y;
        rc.right = pt.x + 2;
        rc.bottom = pt.y + 24;
    }
    return rc;
}"""
assert caret_old in c, "caret_old not found"
c = c.replace(caret_old, caret_new, 1)

with open("tools/universal/likhi_universal.cpp", "w", encoding="utf-8", newline="\r\n") as f:
    f.write(c)
print("Updated likhi_universal.cpp successfully")


# 2. Update topbar_window.cpp
with open("tools/universal/topbar_window.cpp", "r", encoding="utf-8") as f:
    tb = f.read()

# Update CheckAutoHide to not collapse when auto-hide is off
autohide_old = """void TopBarWindow::CheckAutoHide() {
    if (!is_visible_ || is_dragging_) return;"""

autohide_new = """void TopBarWindow::CheckAutoHide() {
    if (!is_visible_ || is_dragging_) return;
    if (!auto_hide_enabled_ && !is_compact_mode_) {
        if (is_collapsed_ || width_ < kNormalWidth) {
            ExpandToFull();
        }
        return;
    }"""
assert autohide_old in tb, "autohide_old not found"
tb = tb.replace(autohide_old, autohide_new, 1)

# Ensure Initialize expands to full
init_old = """    InitTooltips(hwnd_);
    SetTimer(hwnd_, kTimerAutoHide, 100, nullptr);

    return true;"""

init_new = """    InitTooltips(hwnd_);
    SetTimer(hwnd_, kTimerAutoHide, 100, nullptr);
    ExpandToFull();

    return true;"""
assert init_old in tb, "init_old not found"
tb = tb.replace(init_old, init_new, 1)

with open("tools/universal/topbar_window.cpp", "w", encoding="utf-8", newline="\r\n") as f:
    f.write(tb)
print("Updated topbar_window.cpp successfully")

# 3. Update settings.json
settings_path = os.path.expandvars(r"%APPDATA%\PC-Bangla-Typing-App\settings.json")
if os.path.exists(settings_path):
    with open(settings_path, "r", encoding="utf-8") as f:
        cfg = json.load(f)
    cfg["topbar_autohide"] = False
    cfg["topbar_compact"] = False
    cfg["show_topbar"] = True
    with open(settings_path, "w", encoding="utf-8") as f:
        json.dump(cfg, f, indent=2, ensure_ascii=False)
    print("Updated settings.json (autohide=False, compact=False)")
