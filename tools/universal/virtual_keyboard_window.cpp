#include "virtual_keyboard_window.h"
#include <algorithm>

namespace likhi {
namespace universal {

namespace {

const wchar_t kVirtualKeyboardClass[] = L"LikhiVirtualKeyboardClass";

} // namespace

VirtualKeyboardWindow::VirtualKeyboardWindow()
    : hinst_(nullptr),
      hwnd_(nullptr),
      hwnd_parent_(nullptr),
      is_visible_(false),
      width_(628),
      height_(292),
      pos_x_(-1),
      pos_y_(-1),
      current_tab_(OskTab::kBangla), // Default to Bangla as shown in image 2
      is_shift_active_(false),
      is_caps_active_(false),
      hovered_key_index_(-1),
      pressed_key_index_(-1),
      hovered_tab_(-1),
      font_tab_(nullptr),
      font_key_en_(nullptr),
      font_key_bn_(nullptr),
      font_key_small_(nullptr),
      font_emoji_(nullptr) {

    // Tab Header Rectangles
    rc_header_       = {0, 0, width_, 36};
    rc_tab_english_  = {8,   6, 80,  30};
    rc_tab_bangla_   = {84,  6, 154, 30};
    rc_tab_chars_    = {158, 6, 244, 30};
    rc_tab_emoji_    = {248, 6, 314, 30};

    // Window controls at right
    rc_btn_min_      = {width_ - 78, 6, width_ - 54, 30};
    rc_btn_max_      = {width_ - 52, 6, width_ - 28, 30};
    rc_btn_close_    = {width_ - 26, 6, width_ - 4,  30};

    // Emoji search bar
    rc_search_bar_   = {16, 42, width_ - 110, 68};
    rc_search_btn_   = {width_ - 102, 42, width_ - 16, 68};
}

VirtualKeyboardWindow::~VirtualKeyboardWindow() {
    Destroy();
}

bool VirtualKeyboardWindow::Initialize(HINSTANCE hInstance, HWND parentHwnd) {
    hinst_ = hInstance;
    hwnd_parent_ = parentHwnd;

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS | CS_DROPSHADOW;
    wc.lpfnWndProc = VirtualKeyboardWindow::WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kVirtualKeyboardClass;
    RegisterClassExW(&wc);

    // Initial position: center-bottom of screen above taskbar
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    pos_x_ = (screen_w - width_) / 2;
    pos_y_ = screen_h - height_ - 60;
    if (pos_y_ < 40) pos_y_ = 40;

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kVirtualKeyboardClass,
        L"Likhi Virtual Keyboard",
        WS_POPUP,
        pos_x_, pos_y_, width_, height_,
        nullptr, nullptr, hInstance, this
    );

    if (!hwnd_) return false;

    // Apply rounded window region
    HRGN rgn = CreateRoundRectRgn(0, 0, width_ + 1, height_ + 1, 8, 8);
    SetWindowRgn(hwnd_, rgn, TRUE);

    // Create crisp typography
    font_tab_ = CreateFontW(
        -13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );

    font_key_en_ = CreateFontW(
        -14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );

    font_key_bn_ = CreateFontW(
        -15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Nirmala UI"
    );

    font_key_small_ = CreateFontW(
        -11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );

    font_emoji_ = CreateFontW(
        -18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI Emoji"
    );

    InitLayouts();
    return true;
}

void VirtualKeyboardWindow::Show(bool show) {
    if (!hwnd_) return;
    is_visible_ = show;
    ShowWindow(hwnd_, show ? SW_SHOWNOACTIVATE : SW_HIDE);
    if (show) {
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void VirtualKeyboardWindow::ToggleVisibility() {
    Show(!is_visible_);
}

bool VirtualKeyboardWindow::IsVisible() const {
    return is_visible_;
}

void VirtualKeyboardWindow::Destroy() {
    if (font_tab_)       { DeleteObject(font_tab_); font_tab_ = nullptr; }
    if (font_key_en_)    { DeleteObject(font_key_en_); font_key_en_ = nullptr; }
    if (font_key_bn_)    { DeleteObject(font_key_bn_); font_key_bn_ = nullptr; }
    if (font_key_small_) { DeleteObject(font_key_small_); font_key_small_ = nullptr; }
    if (font_emoji_)     { DeleteObject(font_emoji_); font_emoji_ = nullptr; }
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
}

void VirtualKeyboardWindow::SetTab(OskTab tab) {
    current_tab_ = tab;
    hovered_key_index_ = -1;
    pressed_key_index_ = -1;
    if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
}

void VirtualKeyboardWindow::InitLayouts() {
    BuildEnglishLayout();
    BuildBanglaLayout();
    BuildCharactersLayout();
    BuildEmojiLayout();
}

void VirtualKeyboardWindow::BuildEnglishLayout() {
    keys_english_.clear();
    int start_y = 38;
    int key_h = 36;
    int gap = 3;

    // Row 0: F1 - F12, Vol-, Vol+, Prev, Play/Pause, Next
    int y0 = start_y;
    int x0 = 8;
    int f_w = 28;
    for (int i = 1; i <= 12; ++i) {
        OskKey k;
        k.rc = {x0, y0, x0 + f_w, y0 + 26};
        k.label = L"F" + std::to_wstring(i);
        k.vk_code = (WORD)(VK_F1 + (i - 1));
        k.is_special = true;
        k.is_wide = false;
        keys_english_.push_back(k);
        x0 += f_w + gap;
    }
    x0 += 6;
    // Media keys
    struct MediaDef { const wchar_t* lbl; WORD vk; };
    MediaDef media[] = {
        {L"\U0001F508", VK_VOLUME_DOWN},
        {L"\U0001F50A", VK_VOLUME_UP},
        {L"\u23EE",     VK_MEDIA_PREV_TRACK},
        {L"\u23EF",     VK_MEDIA_PLAY_PAUSE},
        {L"\u23ED",     VK_MEDIA_NEXT_TRACK}
    };
    for (auto& m : media) {
        OskKey k;
        k.rc = {x0, y0, x0 + 32, y0 + 26};
        k.label = m.lbl;
        k.vk_code = m.vk;
        k.is_special = true;
        k.is_wide = false;
        keys_english_.push_back(k);
        x0 += 32 + gap;
    }

    // Row 1: Esc, `, 1..0, -, =, Backspace
    int y1 = y0 + 28 + gap;
    int x1 = 8;
    int norm_w = 34;

    OskKey k_esc = {{x1, y1, x1 + 38, y1 + key_h}, L"Esc.", L"Esc.", L"", VK_ESCAPE, true, false};
    keys_english_.push_back(k_esc);
    x1 += 38 + gap;

    const wchar_t* r1_chars = L"`1234567890-=";
    const wchar_t* r1_shift = L"~!@#$%^&*()_+";
    for (int i = 0; r1_chars[i]; ++i) {
        OskKey k;
        k.rc = {x1, y1, x1 + norm_w, y1 + key_h};
        k.label = std::wstring(1, r1_chars[i]);
        k.shift_label = std::wstring(1, r1_shift[i]);
        k.type_text = k.label;
        k.vk_code = 0;
        k.is_special = false;
        k.is_wide = false;
        keys_english_.push_back(k);
        x1 += norm_w + gap;
    }

    OskKey k_bksp = {{x1, y1, width_ - 8, y1 + key_h}, L"\u232B", L"\u232B", L"", VK_BACK, true, true};
    keys_english_.push_back(k_bksp);

    // Row 2: Tab, q..p, [, ], \, Del
    int y2 = y1 + key_h + gap;
    int x2 = 8;
    OskKey k_tab = {{x2, y2, x2 + 46, y2 + key_h}, L"Tab", L"Tab", L"", VK_TAB, true, true};
    keys_english_.push_back(k_tab);
    x2 += 46 + gap;

    const wchar_t* r2_chars = L"qwertyuiop[]\\";
    const wchar_t* r2_shift = L"QWERTYUIOP{}|";
    for (int i = 0; r2_chars[i]; ++i) {
        OskKey k;
        k.rc = {x2, y2, x2 + norm_w, y2 + key_h};
        k.label = std::wstring(1, r2_chars[i]);
        k.shift_label = std::wstring(1, r2_shift[i]);
        k.type_text = k.label;
        k.vk_code = 0;
        k.is_special = false;
        k.is_wide = false;
        keys_english_.push_back(k);
        x2 += norm_w + gap;
    }
    OskKey k_del = {{x2, y2, width_ - 8, y2 + key_h}, L"Del.", L"Del.", L"", VK_DELETE, true, true};
    keys_english_.push_back(k_del);

    // Row 3: Caps, a..l, ;, ', Enter, Home
    int y3 = y2 + key_h + gap;
    int x3 = 8;
    OskKey k_caps = {{x3, y3, x3 + 52, y3 + key_h}, L"Caps", L"Caps", L"", VK_CAPITAL, true, true};
    keys_english_.push_back(k_caps);
    x3 += 52 + gap;

    const wchar_t* r3_chars = L"asdfghjkl;'";
    const wchar_t* r3_shift = L"ASDFGHJKL:\"";
    for (int i = 0; r3_chars[i]; ++i) {
        OskKey k;
        k.rc = {x3, y3, x3 + norm_w, y3 + key_h};
        k.label = std::wstring(1, r3_chars[i]);
        k.shift_label = std::wstring(1, r3_shift[i]);
        k.type_text = k.label;
        k.vk_code = 0;
        k.is_special = false;
        k.is_wide = false;
        keys_english_.push_back(k);
        x3 += norm_w + gap;
    }
    OskKey k_enter = {{x3, y3, x3 + 58, y3 + key_h}, L"Enter", L"Enter", L"", VK_RETURN, true, true};
    keys_english_.push_back(k_enter);
    x3 += 58 + gap;
    OskKey k_home = {{x3, y3, width_ - 8, y3 + key_h}, L"Home", L"Home", L"", VK_HOME, true, false};
    keys_english_.push_back(k_home);

    // Row 4: Shift, z..m, ,, ., /, ↑, Shift, End
    int y4 = y3 + key_h + gap;
    int x4 = 8;
    OskKey k_lshift = {{x4, y4, x4 + 58, y4 + key_h}, L"Shift", L"Shift", L"", VK_SHIFT, true, true};
    keys_english_.push_back(k_lshift);
    x4 += 58 + gap;

    const wchar_t* r4_chars = L"zxcvbnm,./";
    const wchar_t* r4_shift = L"ZXCVBNM<>?";
    for (int i = 0; r4_chars[i]; ++i) {
        OskKey k;
        k.rc = {x4, y4, x4 + norm_w, y4 + key_h};
        k.label = std::wstring(1, r4_chars[i]);
        k.shift_label = std::wstring(1, r4_shift[i]);
        k.type_text = k.label;
        k.vk_code = 0;
        k.is_special = false;
        k.is_wide = false;
        keys_english_.push_back(k);
        x4 += norm_w + gap;
    }
    OskKey k_up = {{x4, y4, x4 + 32, y4 + key_h}, L"\u2191", L"\u2191", L"", VK_UP, true, false};
    keys_english_.push_back(k_up);
    x4 += 32 + gap;
    OskKey k_rshift = {{x4, y4, x4 + 48, y4 + key_h}, L"Shift", L"Shift", L"", VK_SHIFT, true, true};
    keys_english_.push_back(k_rshift);
    x4 += 48 + gap;
    OskKey k_end = {{x4, y4, width_ - 8, y4 + key_h}, L"End", L"End", L"", VK_END, true, false};
    keys_english_.push_back(k_end);

    // Row 5: Ctrl, Win, Alt, Space, Alt, ←, ↓, →, Menu
    int y5 = y4 + key_h + gap;
    int x5 = 8;
    OskKey k_ctrl = {{x5, y5, x5 + 46, y5 + key_h}, L"Ctrl", L"Ctrl", L"", VK_CONTROL, true, false};
    keys_english_.push_back(k_ctrl);
    x5 += 46 + gap;
    OskKey k_win = {{x5, y5, x5 + 32, y5 + key_h}, L"\u229E", L"\u229E", L"", VK_LWIN, true, false};
    keys_english_.push_back(k_win);
    x5 += 32 + gap;
    OskKey k_alt = {{x5, y5, x5 + 34, y5 + key_h}, L"Alt", L"Alt", L"", VK_MENU, true, false};
    keys_english_.push_back(k_alt);
    x5 += 34 + gap;

    int space_w = 210;
    OskKey k_space = {{x5, y5, x5 + space_w, y5 + key_h}, L"", L"", L" ", VK_SPACE, false, true};
    keys_english_.push_back(k_space);
    x5 += space_w + gap;

    OskKey k_alt2 = {{x5, y5, x5 + 34, y5 + key_h}, L"Alt", L"Alt", L"", VK_MENU, true, false};
    keys_english_.push_back(k_alt2);
    x5 += 34 + gap;

    OskKey k_left = {{x5, y5, x5 + 32, y5 + key_h}, L"\u2190", L"\u2190", L"", VK_LEFT, true, false};
    keys_english_.push_back(k_left);
    x5 += 32 + gap;
    OskKey k_down = {{x5, y5, x5 + 32, y5 + key_h}, L"\u2193", L"\u2193", L"", VK_DOWN, true, false};
    keys_english_.push_back(k_down);
    x5 += 32 + gap;
    OskKey k_right = {{x5, y5, x5 + 32, y5 + key_h}, L"\u2192", L"\u2192", L"", VK_RIGHT, true, false};
    keys_english_.push_back(k_right);
    x5 += 32 + gap;

    OskKey k_menu = {{x5, y5, width_ - 8, y5 + key_h}, L"\u2630", L"\u2630", L"", VK_APPS, true, false};
    keys_english_.push_back(k_menu);
}

void VirtualKeyboardWindow::BuildBanglaLayout() {
    keys_bangla_.clear();
    int start_y = 38;
    int key_h = 32;
    int gap = 3;

    // Left Grid: 11 columns
    // Right Grid: 5 columns
    int left_col_w = 34;
    int right_col_w = 34;
    int left_x_start = 8;
    int right_x_start = left_x_start + (11 * (left_col_w + gap)) + 12;

    // --- LEFT GRID ---
    // Row 1: Sworoborno (স্বরবর্ণ)
    const wchar_t* vowels[] = {
        L"\u0985", L"\u0986", L"\u0987", L"\u0988", L"\u0989", L"\u098A",
        L"\u098B", L"\u098F", L"\u0990", L"\u0993", L"\u0994"
    };
    int y1 = start_y;
    for (int col = 0; col < 11; ++col) {
        int x = left_x_start + col * (left_col_w + gap);
        OskKey k;
        k.rc = {x, y1, x + left_col_w, y1 + key_h};
        k.label = vowels[col];
        k.type_text = vowels[col];
        k.vk_code = 0;
        k.is_special = false;
        k.is_wide = false;
        keys_bangla_.push_back(k);
    }

    // Row 2: Kar Signs (কার চিহ্ন)
    const wchar_t* kars[] = {
        L"\u09BE", L"\u09BF", L"\u09C0", L"\u09C1", L"\u09C2", L"\u09C3",
        L"\u09C7", L"\u09C8", L"\u09CB", L"\u09CC", L"\u09CD"
    };
    int y2 = y1 + key_h + gap;
    for (int col = 0; col < 11; ++col) {
        int x = left_x_start + col * (left_col_w + gap);
        OskKey k;
        k.rc = {x, y2, x + left_col_w, y2 + key_h};
        k.label = kars[col];
        k.type_text = kars[col];
        k.vk_code = 0;
        k.is_special = false;
        k.is_wide = false;
        keys_bangla_.push_back(k);
    }

    // Row 3: Byanjonborno 1 (ক - ঞ)
    const wchar_t* cons1[] = {
        L"\u0995", L"\u0996", L"\u0997", L"\u0998", L"\u0999",
        L"\u099A", L"\u099B", L"\u099C", L"\u099D", L"\u099E", L"\u09CE"
    };
    int y3 = y2 + key_h + gap;
    for (int col = 0; col < 11; ++col) {
        int x = left_x_start + col * (left_col_w + gap);
        OskKey k;
        k.rc = {x, y3, x + left_col_w, y3 + key_h};
        k.label = cons1[col];
        k.type_text = cons1[col];
        k.vk_code = 0;
        k.is_special = false;
        k.is_wide = false;
        keys_bangla_.push_back(k);
    }

    // Row 4: Byanjonborno 2 (ট - ন)
    const wchar_t* cons2[] = {
        L"\u099F", L"\u09A0", L"\u09A1", L"\u09A2", L"\u09A3",
        L"\u09A4", L"\u09A5", L"\u09A6", L"\u09A7", L"\u09A8", L"\u09DC"
    };
    int y4 = y3 + key_h + gap;
    for (int col = 0; col < 11; ++col) {
        int x = left_x_start + col * (left_col_w + gap);
        OskKey k;
        k.rc = {x, y4, x + left_col_w, y4 + key_h};
        k.label = cons2[col];
        k.type_text = cons2[col];
        k.vk_code = 0;
        k.is_special = false;
        k.is_wide = false;
        keys_bangla_.push_back(k);
    }

    // Row 5: Byanjonborno 3 (প - ষ)
    const wchar_t* cons3[] = {
        L"\u09AA", L"\u09AB", L"\u09AC", L"\u09AD", L"\u09AE",
        L"\u09AF", L"\u09B0", L"\u09B2", L"\u09B6", L"\u09B7", L"\u09DD"
    };
    int y5 = y4 + key_h + gap;
    for (int col = 0; col < 11; ++col) {
        int x = left_x_start + col * (left_col_w + gap);
        OskKey k;
        k.rc = {x, y5, x + left_col_w, y5 + key_h};
        k.label = cons3[col];
        k.type_text = cons3[col];
        k.vk_code = 0;
        k.is_special = false;
        k.is_wide = false;
        keys_bangla_.push_back(k);
    }

    // Row 6: Byanjonborno 4 (স - হসন্ত)
    const wchar_t* cons4[] = {
        L"\u09B8", L"\u09B9", L"\u09DC", L"\u09DD", L"\u09DF",
        L"\u09CE", L"\u0982", L"\u0983", L"\u0981", L"\u09CD", L"\u09BC"
    };
    int y6 = y5 + key_h + gap;
    for (int col = 0; col < 11; ++col) {
        int x = left_x_start + col * (left_col_w + gap);
        OskKey k;
        k.rc = {x, y6, x + left_col_w, y6 + key_h};
        k.label = cons4[col];
        k.type_text = cons4[col];
        k.vk_code = 0;
        k.is_special = false;
        k.is_wide = false;
        keys_bangla_.push_back(k);
    }

    // Row 7: Conjuncts (যুক্তবর্ণ)
    const wchar_t* jukto[] = {
        L"\u0995\u09CD\u09B7",         // ক্ষ
        L"\u099C\u09CD\u099E",         // জ্ঞ
        L"\u099E\u09CD\u099A",         // ঞ্চ
        L"\u099E\u09CD\u099B",         // ঞ্ছ
        L"\u099E\u09CD\u099C",         // ঞ্জ
        L"\u0999\u09CD\u0995",         // ঙ্ক
        L"\u0999\u09CD\u0997",         // ঙ্গ
        L"\u09AE\u09CD\u09AA",         // ম্প
        L"\u09B7\u09CD\u0995",         // ষ্ক
        L"\u09B7\u09CD\u09A3",         // ষ্ণ
        L"\u09A4\u09CD\u09B0"          // ত্র
    };
    int y7 = y6 + key_h + gap;
    for (int col = 0; col < 11; ++col) {
        int x = left_x_start + col * (left_col_w + gap);
        OskKey k;
        k.rc = {x, y7, x + left_col_w, y7 + key_h};
        k.label = jukto[col];
        k.type_text = jukto[col];
        k.vk_code = 0;
        k.is_special = false;
        k.is_wide = false;
        keys_bangla_.push_back(k);
    }

    // --- RIGHT GRID ---
    // Row 1: ০, ১, ২, ৩, ৪
    const wchar_t* num1[] = {L"\u09E6", L"\u09E7", L"\u09E8", L"\u09E9", L"\u09EA"};
    for (int col = 0; col < 5; ++col) {
        int x = right_x_start + col * (right_col_w + gap);
        OskKey k;
        k.rc = {x, y1, x + right_col_w, y1 + key_h};
        k.label = num1[col];
        k.type_text = num1[col];
        k.vk_code = 0;
        k.is_special = false;
        k.is_wide = false;
        keys_bangla_.push_back(k);
    }

    // Row 2: ৫, ৬, ৭, ৮, ৯
    const wchar_t* num2[] = {L"\u09EB", L"\u09EC", L"\u09ED", L"\u09EE", L"\u09EF"};
    for (int col = 0; col < 5; ++col) {
        int x = right_x_start + col * (right_col_w + gap);
        OskKey k;
        k.rc = {x, y2, x + right_col_w, y2 + key_h};
        k.label = num2[col];
        k.type_text = num2[col];
        k.vk_code = 0;
        k.is_special = false;
        k.is_wide = false;
        keys_bangla_.push_back(k);
    }

    // Row 3: ।, ,, ?, ;, :
    const wchar_t* punc1[] = {L"\u0964", L",", L"?", L";", L":"};
    for (int col = 0; col < 5; ++col) {
        int x = right_x_start + col * (right_col_w + gap);
        OskKey k;
        k.rc = {x, y3, x + right_col_w, y3 + key_h};
        k.label = punc1[col];
        k.type_text = punc1[col];
        k.vk_code = 0;
        k.is_special = false;
        k.is_wide = false;
        keys_bangla_.push_back(k);
    }

    // Row 4: ৳, !, @, (, )
    const wchar_t* punc2[] = {L"\u09F3", L"!", L"@", L"(", L")"};
    for (int col = 0; col < 5; ++col) {
        int x = right_x_start + col * (right_col_w + gap);
        OskKey k;
        k.rc = {x, y4, x + right_col_w, y4 + key_h};
        k.label = punc2[col];
        k.type_text = punc2[col];
        k.vk_code = 0;
        k.is_special = false;
        k.is_wide = false;
        keys_bangla_.push_back(k);
    }

    // Row 5: Tab, Backspace, Enter
    int rx5 = right_x_start;
    OskKey b_tab = {{rx5, y5, rx5 + 50, y5 + key_h}, L"Tab", L"Tab", L"", VK_TAB, true, true};
    keys_bangla_.push_back(b_tab);
    rx5 += 50 + gap;

    OskKey b_bksp = {{rx5, y5, rx5 + 56, y5 + key_h}, L"\u232B", L"\u232B", L"", VK_BACK, true, true};
    keys_bangla_.push_back(b_bksp);
    rx5 += 56 + gap;

    OskKey b_enter = {{rx5, y5, width_ - 8, y5 + key_h}, L"Enter", L"Enter", L"", VK_RETURN, true, true};
    keys_bangla_.push_back(b_enter);

    // Row 6: ZWJ, ZWNJ
    int rx6 = right_x_start;
    int half_w = (width_ - 8 - rx6 - gap) / 2;
    OskKey b_zwj = {{rx6, y6, rx6 + half_w, y6 + key_h}, L"ZWJ", L"ZWJ", L"\u200D", 0, true, true};
    keys_bangla_.push_back(b_zwj);
    rx6 += half_w + gap;

    OskKey b_zwnj = {{rx6, y6, width_ - 8, y6 + key_h}, L"ZWNJ", L"ZWNJ", L"\u200C", 0, true, true};
    keys_bangla_.push_back(b_zwnj);

    // Row 7: Space Bar
    int rx7 = right_x_start;
    OskKey b_space = {{rx7, y7, width_ - 8, y7 + key_h}, L"", L"", L" ", VK_SPACE, false, true};
    keys_bangla_.push_back(b_space);
}

void VirtualKeyboardWindow::BuildCharactersLayout() {
    keys_characters_.clear();
    int start_y = 44;
    int key_h = 34;
    int key_w = 36;
    int gap = 3;

    // Rich grid of symbols & international characters
    const wchar_t* chars_grid[] = {
        L"\u09F3", L"$", L"\u20AC", L"\u00A3", L"\u00A5", L"\u20B9", L"\u00A2", L"+", L"-", L"\u00D7", L"\u00F7", L"=", L"\u2260", L"\u00B1", L"\u2264", L"\u2265",
        L"%", L"\u2030", L"\u00B0", L"#", L"&", L"@", L"*", L"^", L"_", L"~", L"|", L"/", L"\\", L"\u221A", L"\u221E", L"\u03C0",
        L"\u0964", L"\u0965", L"\u2026", L"\u00B7", L"\u2022", L"\u2014", L"\u2013", L"\u00A9", L"\u00AE", L"\u2122", L"\u00A7", L"\u00B6", L"!", L"?", L";", L":",
        L"(", L")", L"[", L"]", L"{", L"}", L"<", L">", L"\u00AB", L"\u00BB", L"\u201C", L"\u201D", L"\u2018", L"\u2019", L"`", L"'"
    };

    int total_chars = sizeof(chars_grid) / sizeof(chars_grid[0]);
    int cols = 16;
    for (int i = 0; i < total_chars; ++i) {
        int r = i / cols;
        int c = i % cols;
        int x = 8 + c * (key_w + gap);
        int y = start_y + r * (key_h + gap);
        if (x + key_w > width_ - 8) continue;

        OskKey k;
        k.rc = {x, y, x + key_w, y + key_h};
        k.label = chars_grid[i];
        k.type_text = chars_grid[i];
        k.vk_code = 0;
        k.is_special = false;
        k.is_wide = false;
        keys_characters_.push_back(k);
    }

    // Bottom action row: Backspace and Space
    int by = start_y + 4 * (key_h + gap) + 6;
    OskKey k_sp = {{8, by, width_ - 120, by + 36}, L"Space", L"Space", L" ", VK_SPACE, false, true};
    keys_characters_.push_back(k_sp);

    OskKey k_bk = {{width_ - 116, by, width_ - 8, by + 36}, L"\u232B Backspace", L"", L"", VK_BACK, true, true};
    keys_characters_.push_back(k_bk);
}

void VirtualKeyboardWindow::BuildEmojiLayout() {
    keys_emoji_.clear();
    int start_y = 78;
    int key_h = 34;
    int key_w = 36;
    int gap = 3;

    // Common and popular emoji list matching Image 3
    const wchar_t* emojis[] = {
        L"\u2753", L"\U0001F644", L"\U0001F611", L"\u26C8", L"\U0001F1E7\U0001F1E9", L"\u261D", L"\U0001F973", L"\u274C", L"\U0001F4B9", L"\U0001F539", L"\U0001F40D", L"\U0001F433", L"\U0001F929", L"\U0001F61C", L"\U0001F636", L"\u2728",
        L"\U0001F609", L"\U0001F44E", L"\U0001F44D", L"\U0001F976", L"\U0001F1E7\U0001F1E9", L"\U0001F329", L"\U0001F4C8", L"\U0001F984", L"\U0001F916", L"\U0001F327", L"\U0001F981", L"\U0001F496", L"\U0001F525", L"\U0001F389", L"\U0001F4AF", L"\u2B50",
        L"\U0001F603", L"\U0001F606", L"\U0001F605", L"\U0001F923", L"\U0001F602", L"\U0001F62D", L"\U0001F60B", L"\U0001F61B", L"\U0001F61C", L"\U0001F92A", L"\U0001F607", L"\U0001F970", L"\U0001F60D", L"\U0001F929", L"\U0001F618", L"\U0001F61A",
        L"\U0001F617", L"\U0001F619", L"\U0001F924", L"\U0001F911", L"\U0001F917", L"\U0001F92D", L"\U0001F92B", L"\U0001F914", L"\U0001F910", L"\U0001F928", L"\U0001F610", L"\U0001F611", L"\U0001F636", L"\U0001F60F", L"\U0001F612", L"\U0001F644",
        L"\U0001F62C", L"\U0001F925", L"\U0001F60C", L"\U0001F614", L"\U0001F62A", L"\U0001F634", L"\U0001F637", L"\U0001F912", L"\U0001F915", L"\U0001F922", L"\U0001F92E", L"\U0001F975", L"\U0001F976", L"\U0001F631", L"\U0001F92F", L"\U0001F973",
        L"\U0001F64F", L"\U0001F44F", L"\U0001F91D", L"\U0001F64C", L"\u270C", L"\U0001F91E", L"\U0001F91F", L"\U0001F918", L"\U0001F44C", L"\U0001F90C", L"\U0001F44B", L"\U0001F919", L"\U0001F4AA", L"\U0001F9E0", L"\U0001F451", L"\u2764\uFE0F"
    };

    int total_emojis = sizeof(emojis) / sizeof(emojis[0]);
    int cols = 16;
    for (int i = 0; i < total_emojis; ++i) {
        int r = i / cols;
        int c = i % cols;
        int x = 8 + c * (key_w + gap);
        int y = start_y + r * (key_h + gap);
        if (y + key_h > height_ - 4) continue;

        OskKey k;
        k.rc = {x, y, x + key_w, y + key_h};
        k.label = emojis[i];
        k.type_text = emojis[i];
        k.vk_code = 0;
        k.is_special = false;
        k.is_wide = false;
        keys_emoji_.push_back(k);
    }
}

LRESULT CALLBACK VirtualKeyboardWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    VirtualKeyboardWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = reinterpret_cast<VirtualKeyboardWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<VirtualKeyboardWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self) {
        return self->HandleMessage(hwnd, msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT VirtualKeyboardWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT:
            OnPaint(hwnd);
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_LBUTTONDOWN:
            OnLButtonDown(hwnd, LOWORD(lp), HIWORD(lp));
            return 0;

        case WM_LBUTTONUP:
            OnLButtonUp(hwnd, LOWORD(lp), HIWORD(lp));
            return 0;

        case WM_MOUSEMOVE:
            OnMouseMove(hwnd, LOWORD(lp), HIWORD(lp));
            return 0;

        case WM_MOUSELEAVE:
            OnMouseLeave(hwnd);
            return 0;

        case WM_DESTROY:
            hwnd_ = nullptr;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void VirtualKeyboardWindow::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT cr;
    GetClientRect(hwnd, &cr);

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, cr.right, cr.bottom);
    HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

    // Modern clean background fill
    HBRUSH bgBrush = CreateSolidBrush(RGB(246, 248, 252));
    FillRect(memDC, &cr, bgBrush);
    DeleteObject(bgBrush);

    // Subtle outer frame border
    HPEN borderPen = CreatePen(PS_SOLID, 1, RGB(220, 226, 235));
    HGDIOBJ oldPen = SelectObject(memDC, borderPen);
    SelectObject(memDC, GetStockObject(NULL_BRUSH));
    RoundRect(memDC, cr.left, cr.top, cr.right, cr.bottom, 8, 8);
    SelectObject(memDC, oldPen);
    DeleteObject(borderPen);

    SetBkMode(memDC, TRANSPARENT);

    // --- 1. Draw Tab Buttons in Header ---
    auto DrawTab = [this, memDC](const RECT& r, const wchar_t* text, bool is_active) {
        COLORREF bg_col = is_active ? RGB(255, 255, 255) : RGB(240, 243, 248);
        COLORREF border_col = is_active ? RGB(14, 116, 222) : RGB(218, 224, 233);
        COLORREF text_col = is_active ? RGB(14, 116, 222) : RGB(71, 85, 105);

        HBRUSH br = CreateSolidBrush(bg_col);
        HPEN pn = CreatePen(PS_SOLID, is_active ? 2 : 1, border_col);
        HGDIOBJ ob = SelectObject(memDC, br);
        HGDIOBJ op = SelectObject(memDC, pn);
        RoundRect(memDC, r.left, r.top, r.right, r.bottom, 6, 6);
        SelectObject(memDC, ob);
        SelectObject(memDC, op);
        DeleteObject(br);
        DeleteObject(pn);

        SelectObject(memDC, font_tab_);
        SetTextColor(memDC, text_col);
        RECT tr = r;
        DrawTextW(memDC, text, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    };

    DrawTab(rc_tab_english_, L"English",    current_tab_ == OskTab::kEnglish);
    DrawTab(rc_tab_bangla_,  L"Bangla",     current_tab_ == OskTab::kBangla);
    DrawTab(rc_tab_chars_,   L"Characters", current_tab_ == OskTab::kCharacters);
    DrawTab(rc_tab_emoji_,   L"Emoji",      current_tab_ == OskTab::kEmoji);

    // Window controls (—, □, ✕)
    auto DrawWinCtrl = [this, memDC](const RECT& r, const wchar_t* glyph, bool is_close) {
        HBRUSH br = CreateSolidBrush(RGB(255, 255, 255));
        HPEN pn = CreatePen(PS_SOLID, 1, RGB(220, 226, 235));
        HGDIOBJ ob = SelectObject(memDC, br);
        HGDIOBJ op = SelectObject(memDC, pn);
        RoundRect(memDC, r.left, r.top, r.right, r.bottom, 4, 4);
        SelectObject(memDC, ob);
        SelectObject(memDC, op);
        DeleteObject(br);
        DeleteObject(pn);

        SelectObject(memDC, font_tab_);
        SetTextColor(memDC, is_close ? RGB(225, 29, 72) : RGB(71, 85, 105));
        RECT tr = r;
        DrawTextW(memDC, glyph, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    };

    DrawWinCtrl(rc_btn_min_, L"—", false);
    DrawWinCtrl(rc_btn_max_, L"□", false);
    DrawWinCtrl(rc_btn_close_, L"✕", true);

    // --- 2. Search Bar in Emoji Tab ---
    if (current_tab_ == OskTab::kEmoji) {
        HBRUSH sBr = CreateSolidBrush(RGB(255, 255, 255));
        HPEN sPn = CreatePen(PS_SOLID, 1, RGB(203, 213, 225));
        HGDIOBJ ob = SelectObject(memDC, sBr);
        HGDIOBJ op = SelectObject(memDC, sPn);
        RoundRect(memDC, rc_search_bar_.left, rc_search_bar_.top, rc_search_bar_.right, rc_search_bar_.bottom, 6, 6);
        SelectObject(memDC, ob);
        SelectObject(memDC, op);
        DeleteObject(sBr);
        DeleteObject(sPn);

        SelectObject(memDC, font_tab_);
        SetTextColor(memDC, RGB(148, 163, 184));
        RECT str_rc = rc_search_bar_;
        str_rc.left += 10;
        DrawTextW(memDC, L"Click and type to search emojis...", -1, &str_rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // AI Search Button
        HBRUSH btnBr = CreateSolidBrush(RGB(14, 165, 233));
        HPEN btnPn = CreatePen(PS_SOLID, 1, RGB(2, 132, 199));
        HGDIOBJ ob2 = SelectObject(memDC, btnBr);
        HGDIOBJ op2 = SelectObject(memDC, btnPn);
        RoundRect(memDC, rc_search_btn_.left, rc_search_btn_.top, rc_search_btn_.right, rc_search_btn_.bottom, 6, 6);
        SelectObject(memDC, ob2);
        SelectObject(memDC, op2);
        DeleteObject(btnBr);
        DeleteObject(btnPn);

        SetTextColor(memDC, RGB(255, 255, 255));
        RECT btr = rc_search_btn_;
        DrawTextW(memDC, L"AI Search", -1, &btr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // --- 3. Draw Active Tab's Keys ---
    const std::vector<OskKey>& keys = (current_tab_ == OskTab::kEnglish) ? keys_english_
                                    : (current_tab_ == OskTab::kBangla)  ? keys_bangla_
                                    : (current_tab_ == OskTab::kCharacters) ? keys_characters_
                                    : keys_emoji_;

    for (size_t i = 0; i < keys.size(); ++i) {
        const OskKey& k = keys[i];
        bool is_hovered = ((int)i == hovered_key_index_);
        bool is_pressed = ((int)i == pressed_key_index_);

        COLORREF key_bg = is_pressed ? RGB(14, 116, 222)
                        : is_hovered ? RGB(236, 244, 255)
                                     : RGB(255, 255, 255);

        COLORREF key_border = is_pressed ? RGB(14, 116, 222)
                            : is_hovered ? RGB(96, 165, 250)
                                         : RGB(226, 232, 240);

        COLORREF key_text = is_pressed ? RGB(255, 255, 255)
                          : k.is_special ? RGB(51, 65, 85)
                                         : RGB(15, 23, 42);

        HBRUSH kBr = CreateSolidBrush(key_bg);
        HPEN kPn = CreatePen(PS_SOLID, 1, key_border);
        HGDIOBJ ob = SelectObject(memDC, kBr);
        HGDIOBJ op = SelectObject(memDC, kPn);
        RoundRect(memDC, k.rc.left, k.rc.top, k.rc.right, k.rc.bottom, 4, 4);
        SelectObject(memDC, ob);
        SelectObject(memDC, op);
        DeleteObject(kBr);
        DeleteObject(kPn);

        // Select appropriate font
        if (current_tab_ == OskTab::kEmoji) {
            SelectObject(memDC, font_emoji_);
        } else if (current_tab_ == OskTab::kBangla) {
            SelectObject(memDC, font_key_bn_);
        } else {
            SelectObject(memDC, k.label.size() > 2 ? font_key_small_ : font_key_en_);
        }

        SetTextColor(memDC, key_text);
        RECT tr = k.rc;
        const std::wstring& display_text = (is_shift_active_ && !k.shift_label.empty())
                                               ? k.shift_label : k.label;
        DrawTextW(memDC, display_text.c_str(), -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }

    // Blit to screen
    BitBlt(hdc, 0, 0, cr.right, cr.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);

    EndPaint(hwnd, &ps);
}

void VirtualKeyboardWindow::OnLButtonDown(HWND hwnd, int x, int y) {
    POINT pt = {x, y};

    // Header dragging
    if (PtInRect(&rc_header_, pt)) {
        if (PtInRect(&rc_tab_english_, pt)) {
            SetTab(OskTab::kEnglish);
            return;
        }
        if (PtInRect(&rc_tab_bangla_, pt)) {
            SetTab(OskTab::kBangla);
            return;
        }
        if (PtInRect(&rc_tab_chars_, pt)) {
            SetTab(OskTab::kCharacters);
            return;
        }
        if (PtInRect(&rc_tab_emoji_, pt)) {
            SetTab(OskTab::kEmoji);
            return;
        }
        if (PtInRect(&rc_btn_min_, pt) || PtInRect(&rc_btn_close_, pt)) {
            Show(false);
            return;
        }

        // Dragging window by header
        ReleaseCapture();
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return;
    }

    // Keys interaction
    const std::vector<OskKey>& keys = (current_tab_ == OskTab::kEnglish) ? keys_english_
                                    : (current_tab_ == OskTab::kBangla)  ? keys_bangla_
                                    : (current_tab_ == OskTab::kCharacters) ? keys_characters_
                                    : keys_emoji_;

    for (size_t i = 0; i < keys.size(); ++i) {
        if (PtInRect(&keys[i].rc, pt)) {
            pressed_key_index_ = (int)i;
            InvalidateRect(hwnd, &keys[i].rc, FALSE);
            PressKey(keys[i]);
            return;
        }
    }
}

void VirtualKeyboardWindow::OnLButtonUp(HWND hwnd, int x, int y) {
    if (pressed_key_index_ >= 0) {
        pressed_key_index_ = -1;
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

void VirtualKeyboardWindow::OnMouseMove(HWND hwnd, int x, int y) {
    TRACKMOUSEEVENT tme = {sizeof(tme)};
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = hwnd;
    TrackMouseEvent(&tme);

    POINT pt = {x, y};
    int new_hover = -1;

    const std::vector<OskKey>& keys = (current_tab_ == OskTab::kEnglish) ? keys_english_
                                    : (current_tab_ == OskTab::kBangla)  ? keys_bangla_
                                    : (current_tab_ == OskTab::kCharacters) ? keys_characters_
                                    : keys_emoji_;

    for (size_t i = 0; i < keys.size(); ++i) {
        if (PtInRect(&keys[i].rc, pt)) {
            new_hover = (int)i;
            break;
        }
    }

    if (new_hover != hovered_key_index_) {
        hovered_key_index_ = new_hover;
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

void VirtualKeyboardWindow::OnMouseLeave(HWND hwnd) {
    hovered_key_index_ = -1;
    pressed_key_index_ = -1;
    InvalidateRect(hwnd, nullptr, FALSE);
}

void VirtualKeyboardWindow::PressKey(const OskKey& k) {
    if (k.vk_code == VK_SHIFT) {
        is_shift_active_ = !is_shift_active_;
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }
    if (k.vk_code == VK_CAPITAL) {
        is_caps_active_ = !is_caps_active_;
        SendVirtualKey(VK_CAPITAL);
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
    }
    if (k.vk_code != 0) {
        SendVirtualKey(k.vk_code);
        return;
    }

    // Type Unicode character directly
    std::wstring to_type = (is_shift_active_ && !k.shift_label.empty()) ? k.shift_label : k.type_text;
    if (to_type.empty()) to_type = k.label;
    TypeText(to_type);

    if (is_shift_active_) {
        is_shift_active_ = false;
        InvalidateRect(hwnd_, nullptr, FALSE);
    }
}

void VirtualKeyboardWindow::TypeText(const std::wstring& text) {
    if (text.empty()) return;
    std::vector<INPUT> inputs;
    for (wchar_t ch : text) {
        INPUT down = {0};
        down.type = INPUT_KEYBOARD;
        down.ki.wScan = ch;
        down.ki.dwFlags = KEYEVENTF_UNICODE;
        inputs.push_back(down);
        INPUT up = down;
        up.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        inputs.push_back(up);
    }
    SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));
}

void VirtualKeyboardWindow::SendVirtualKey(WORD vk) {
    INPUT inputs[2] = {0};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vk;
    inputs[1] = inputs[0];
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

} // namespace universal
} // namespace likhi
