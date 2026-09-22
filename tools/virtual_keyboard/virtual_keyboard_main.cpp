#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <string>
#include <vector>
#include <fstream>
#include <shlobj.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "comctl32.lib")

namespace {

// Undocumented Win32 Accent Policy for native hardware Acrylic / Aero Glass
typedef enum _ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_GRADIENT = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
    ACCENT_ENABLE_HOSTBACKDROP = 5,
    ACCENT_INVALID_STATE = 6
} ACCENT_STATE;

typedef struct _ACCENT_POLICY {
    ACCENT_STATE AccentState;
    DWORD AccentFlags;
    DWORD GradientColor; // AABBGGRR
    DWORD AnimationId;
} ACCENT_POLICY;

typedef struct _WINDOWCOMPOSITIONATTRIBDATA {
    DWORD Attribute;
    PVOID Data;
    ULONG SizeOfData;
} WINDOWCOMPOSITIONATTRIBDATA;

typedef BOOL(WINAPI* PFN_SetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);

struct KeyButton {
    RECT rect;
    std::wstring label;       // Text displayed on the button
    std::wstring send_text;   // Unicode text to send
    int special_vk = 0;       // VK_BACK, VK_RETURN, VK_SPACE, VK_TAB, etc.
    bool is_special = false;  // Special key styling (Space, Backspace, Tab, Enter)
    bool is_fala = false;     // Fala/Modifier button styling
    bool is_theme_btn = false;// Theme switch button
    bool is_transp_btn = false;// Transparency switch button
};

// Glass Modes:
// 0 = Hardware Acrylic Glass (Native DWM Acrylic, 100% crisp buttons)
// 1 = Frosted Glass (92% Layered translucency)
// 2 = Solid (100% Opaque modern mode)
static int g_glass_mode = 0;
static bool g_is_dark = true; // Default: Dark Glass
static HWND g_hWnd = NULL;
static HICON g_hAppIcon = NULL;
static HFONT g_hFontBn = NULL;
static HFONT g_hFontBnLarge = NULL;
static HFONT g_hFontEn = NULL;
static HFONT g_hFontEnBold = NULL;
static HFONT g_hFontSection = NULL;
static HFONT g_hFontTitle = NULL;
static int g_hover_key_idx = -1;
static int g_pressed_key_idx = -1;

static std::vector<KeyButton> g_keys;

bool LoadVkThemeDark() {
    wchar_t appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appdata))) {
        std::wstring p = std::wstring(appdata) + L"\\PC-Bangla-Typing-App\\settings.json";
        std::ifstream in(p.c_str());
        if (in.is_open()) {
            std::string line;
            while (std::getline(in, line)) {
                if (line.find("\"vk_dark\": false") != std::string::npos) return false;
                if (line.find("\"vk_dark\": true") != std::string::npos) return true;
            }
        }
    }
    return true; // Default to Dark Glass
}

void SaveVkThemeDark(bool dark) {
    wchar_t appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appdata))) {
        std::wstring dir = std::wstring(appdata) + L"\\PC-Bangla-Typing-App";
        CreateDirectoryW(dir.c_str(), NULL);
        std::wstring p = dir + L"\\settings.json";

        std::string content;
        std::ifstream in(p.c_str());
        if (in.is_open()) {
            std::string line;
            while (std::getline(in, line)) {
                if (line.find("\"vk_dark\"") != std::string::npos) continue;
                content += line + "\n";
            }
        }
        if (!content.empty() && content.back() == '\n') content.pop_back();
        size_t last_brace = content.rfind('}');
        if (last_brace != std::string::npos) {
            content.insert(last_brace, std::string("  \"vk_dark\": ") + (dark ? "true" : "false") + ",\n");
        } else {
            content = std::string("{\n  \"vk_dark\": ") + (dark ? "true" : "false") + "\n}";
        }
        std::ofstream out(p.c_str(), std::ios::trunc);
        if (out.is_open()) {
            out << content;
        }
    }
}

int LoadGlassModeSetting() {
    wchar_t appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appdata))) {
        std::wstring p = std::wstring(appdata) + L"\\PC-Bangla-Typing-App\\settings.json";
        std::ifstream in(p.c_str());
        if (in.is_open()) {
            std::string line;
            while (std::getline(in, line)) {
                if (line.find("\"vk_glass_mode\": 1") != std::string::npos) return 1;
                if (line.find("\"vk_glass_mode\": 2") != std::string::npos) return 2;
                if (line.find("\"vk_glass_mode\": 0") != std::string::npos) return 0;
            }
        }
    }
    return 0; // Default: Hardware Acrylic Glass!
}

void SaveGlassModeSetting(int mode) {
    wchar_t appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appdata))) {
        std::wstring dir = std::wstring(appdata) + L"\\PC-Bangla-Typing-App";
        CreateDirectoryW(dir.c_str(), NULL);
        std::wstring p = dir + L"\\settings.json";

        std::string content;
        std::ifstream in(p.c_str());
        if (in.is_open()) {
            std::string line;
            while (std::getline(in, line)) {
                if (line.find("\"vk_glass_mode\"") != std::string::npos) continue;
                content += line + "\n";
            }
        }
        if (!content.empty() && content.back() == '\n') content.pop_back();
        size_t last_brace = content.rfind('}');
        if (last_brace != std::string::npos) {
            content.insert(last_brace, "  \"vk_glass_mode\": " + std::to_string(mode) + ",\n");
        } else {
            content = "{\n  \"vk_glass_mode\": " + std::to_string(mode) + "\n}";
        }
        std::ofstream out(p.c_str(), std::ios::trunc);
        if (out.is_open()) {
            out << content;
        }
    }
}

void SendUnicodeChar(wchar_t ch) {
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wScan = ch;
    inputs[0].ki.dwFlags = KEYEVENTF_UNICODE;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wScan = ch;
    inputs[1].ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;

    SendInput(2, inputs, sizeof(INPUT));
}

void SendSpecialKey(WORD vk) {
    INPUT inputs[2] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vk;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = vk;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;

    SendInput(2, inputs, sizeof(INPUT));
}

void SendString(const std::wstring& str) {
    for (wchar_t c : str) {
        SendUnicodeChar(c);
    }
}

void BuildLayout() {
    g_keys.clear();

    // -------------------------------------------------------------
    // 0. Top Right: Transparent Theme & Dark/Light Switch Buttons
    // -------------------------------------------------------------
    {
        KeyButton btn;
        btn.rect = { 510, 7, 646, 31 };
        if (g_glass_mode == 0) {
            btn.label = L"🔮 Acrylic Glass";
        } else if (g_glass_mode == 1) {
            btn.label = L"🔮 Frosted (92%)";
        } else {
            btn.label = L"🔮 Solid (100%)";
        }
        btn.is_transp_btn = true;
        g_keys.push_back(btn);
    }
    {
        KeyButton btn;
        btn.rect = { 652, 7, 762, 31 };
        btn.label = g_is_dark ? L"🌙 Dark Glass" : L"☀️ Light Glass";
        btn.is_theme_btn = true;
        g_keys.push_back(btn);
    }

    // -------------------------------------------------------------
    // 1. Consonants: 5x5 Grid (ক-ম)
    // -------------------------------------------------------------
    const wchar_t* varga[5][5] = {
        { L"ক", L"খ", L"গ", L"ঘ", L"ঙ" },
        { L"চ", L"ছ", L"জ", L"ঝ", L"ঞ" },
        { L"ট", L"ঠ", L"ড", L"ঢ", L"ণ" },
        { L"ত", L"থ", L"দ", L"ধ", L"ন" },
        { L"প", L"ফ", L"ব", L"ভ", L"ম" }
    };

    const int v_start_x = 14;
    const int v_start_y = 42;
    const int k_w = 38;
    const int k_h = 32;
    const int gap = 4;

    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 5; c++) {
            KeyButton btn;
            int x = v_start_x + c * (k_w + gap);
            int y = v_start_y + r * (k_h + gap);
            btn.rect = { x, y, x + k_w, y + k_h };
            btn.label = varga[r][c];
            btn.send_text = varga[r][c];
            g_keys.push_back(btn);
        }
    }

    // -------------------------------------------------------------
    // 2. Middle Consonants & Modifiers (য - ঁ) [3 cols x 5 rows]
    // -------------------------------------------------------------
    const wchar_t* mid_cons[5][3] = {
        { L"য", L"র", L"ল" },
        { L"শ", L"ষ", L"স" },
        { L"হ", L"ড়", L"ঢ়" },
        { L"য়", L"ৎ", L"ং" },
        { L"ঃ", L"ঁ", L"্" }
    };

    const int mid_start_x = 226;
    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 3; c++) {
            KeyButton btn;
            int x = mid_start_x + c * (k_w + gap);
            int y = v_start_y + r * (k_h + gap);
            btn.rect = { x, y, x + k_w, y + k_h };
            btn.label = mid_cons[r][c];
            btn.send_text = mid_cons[r][c];
            if (r == 4 && c == 2) btn.is_fala = true; // Hasanta accent
            g_keys.push_back(btn);
        }
    }

    // -------------------------------------------------------------
    // 3. Bengali Digits (০ - ৯) [2 cols x 5 rows]
    // -------------------------------------------------------------
    const wchar_t* digits[5][2] = {
        { L"০", L"৫" },
        { L"১", L"৬" },
        { L"২", L"৭" },
        { L"৩", L"৮" },
        { L"৪", L"৯" }
    };

    const int num_start_x = 356;
    const int num_w = 34;
    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 2; c++) {
            KeyButton btn;
            int x = num_start_x + c * (num_w + gap);
            int y = v_start_y + r * (k_h + gap);
            btn.rect = { x, y, x + num_w, y + k_h };
            btn.label = digits[r][c];
            btn.send_text = digits[r][c];
            g_keys.push_back(btn);
        }
    }

    // -------------------------------------------------------------
    // 4. Kar & Vowels Paired Row (11 columns)
    // -------------------------------------------------------------
    const struct KarVowelPair {
        const wchar_t* kar_lbl;
        const wchar_t* kar_text;
        const wchar_t* vowel;
    } kv_pairs[11] = {
        { L"্", L"\u09CD", L"অ" }, // Above 'অ', hasanta/virama!
        { L"া", L"\u09BE", L"আ" },
        { L"ি", L"\u09BF", L"ই" },
        { L"ী", L"\u09C0", L"ঈ" },
        { L"ু", L"\u09C1", L"উ" },
        { L"ূ", L"\u09C2", L"ঊ" },
        { L"ৃ", L"\u09C3", L"ঋ" },
        { L"ে", L"\u09C7", L"এ" },
        { L"ৈ", L"\u09C8", L"ঐ" },
        { L"ো", L"\u09CB", L"ও" },
        { L"ৌ", L"\u09CC", L"ঔ" }
    };

    const int kv_start_x = 14;
    const int kv_w = 34;
    const int kar_y = 226;
    const int vowel_y = 260;

    for (int i = 0; i < 11; i++) {
        int x = kv_start_x + i * (kv_w + gap);
        // Kar button
        {
            KeyButton btn;
            btn.rect = { x, kar_y, x + kv_w, kar_y + 30 };
            btn.label = kv_pairs[i].kar_lbl;
            btn.send_text = kv_pairs[i].kar_text;
            btn.is_fala = (i == 0); // Hasanta accent
            g_keys.push_back(btn);
        }
        // Vowel button
        {
            KeyButton btn;
            btn.rect = { x, vowel_y, x + kv_w, vowel_y + 32 };
            btn.label = kv_pairs[i].vowel;
            btn.send_text = kv_pairs[i].vowel;
            g_keys.push_back(btn);
        }
    }

    // -------------------------------------------------------------
    // 5. Right Section: Few conjuncts (যুক্তবর্ণ) [6 cols x 2 rows]
    // -------------------------------------------------------------
    const struct ConjunctDef {
        const wchar_t* label;
        const wchar_t* send;
    } conjuncts[2][6] = {
        {
            { L"ক্ষ", L"\u0995\u09CD\u09B7" },
            { L"জ্ঞ", L"\u099C\u09CD\u099E" },
            { L"ঞ্চ", L"\u099E\u09CD\u099A" },
            { L"ঞ্জ", L"\u099E\u09CD\u099C" },
            { L"ণ্ড", L"\u09A3\u09CD\u09A1" },
            { L"ণ্ট", L"\u09A3\u09CD\u099F" }
        },
        {
            { L"ন্দ", L"\u09A8\u09CD\u09A6" },
            { L"ন্ত", L"\u09A8\u09CD\u09A4" },
            { L"ম্প", L"\u09AE\u09CD\u09AA" },
            { L"ল্ক", L"\u09B2\u09CD\u0995" },
            { L"ষ্ঠ", L"\u09B7\u09CD\u09A0" },
            { L"ত্র", L"\u09A4\u09CD\u09B0" }
        }
    };

    const int c_start_x = 448;
    const int c_w = 48;
    const int c_h = 32;
    const int c_gap = 5;

    for (int r = 0; r < 2; r++) {
        for (int c = 0; c < 6; c++) {
            KeyButton btn;
            int x = c_start_x + c * (c_w + c_gap);
            int y = 62 + r * (c_h + 4);
            btn.rect = { x, y, x + c_w, y + c_h };
            btn.label = conjuncts[r][c].label;
            btn.send_text = conjuncts[r][c].send;
            g_keys.push_back(btn);
        }
    }

    // -------------------------------------------------------------
    // 6. Fala & Modifiers (য-ফলা, র-ফলা, রেফ, হসন্ত)
    // -------------------------------------------------------------
    {
        KeyButton btn;
        btn.rect = { 448, 136, 600, 168 };
        btn.label = L"্য - য ফলা";
        btn.send_text = L"\u09CD\u09AF";
        btn.is_fala = true;
        g_keys.push_back(btn);
    }
    {
        KeyButton btn;
        btn.rect = { 608, 136, 761, 168 };
        btn.label = L"্র - র ফলা";
        btn.send_text = L"\u09CD\u09B0";
        btn.is_fala = true;
        g_keys.push_back(btn);
    }
    {
        KeyButton btn;
        btn.rect = { 448, 172, 600, 204 };
        btn.label = L"র্ - রেফ";
        btn.send_text = L"\u09B0\u09CD";
        btn.is_fala = true;
        g_keys.push_back(btn);
    }
    {
        KeyButton btn;
        btn.rect = { 608, 172, 761, 204 };
        btn.label = L"্ - হসন্ত";
        btn.send_text = L"\u09CD";
        btn.is_fala = true;
        g_keys.push_back(btn);
    }

    // -------------------------------------------------------------
    // 7. Punctuation & Controls (। ৳ ঽ ৲ ZWJ ZWNJ)
    // -------------------------------------------------------------
    const struct PuncDef {
        const wchar_t* label;
        const wchar_t* send;
    } puncs[6] = {
        { L"।", L"\u09F7" },
        { L"৳", L"\u09F3" },
        { L"ঽ", L"\u09BD" },
        { L"৲", L"\u09F2" },
        { L"ZWJ", L"\u200D" },
        { L"ZWNJ", L"\u200C" }
    };

    for (int i = 0; i < 6; i++) {
        KeyButton btn;
        int x = c_start_x + i * (c_w + c_gap);
        btn.rect = { x, 208, x + c_w, 238 };
        btn.label = puncs[i].label;
        btn.send_text = puncs[i].send;
        g_keys.push_back(btn);
    }

    // -------------------------------------------------------------
    // 8. Navigation & Editing (Tab, Backspace, Space, Enter)
    // -------------------------------------------------------------
    {
        KeyButton btn;
        btn.rect = { 448, 242, 600, 274 };
        btn.label = L"Tab --->";
        btn.special_vk = VK_TAB;
        btn.is_special = true;
        g_keys.push_back(btn);
    }
    {
        KeyButton btn;
        btn.rect = { 608, 242, 761, 274 };
        btn.label = L"<--- Backspace";
        btn.special_vk = VK_BACK;
        btn.is_special = true;
        g_keys.push_back(btn);
    }
    {
        KeyButton btn;
        btn.rect = { 448, 278, 634, 312 };
        btn.label = L"S  P  A  C  E";
        btn.special_vk = VK_SPACE;
        btn.is_special = true;
        g_keys.push_back(btn);
    }
    {
        KeyButton btn;
        btn.rect = { 642, 278, 761, 312 };
        btn.label = L"Enter";
        btn.special_vk = VK_RETURN;
        btn.is_special = true;
        g_keys.push_back(btn);
    }

    // -------------------------------------------------------------
    // 9. Extra Characters (Less used) - 16 buttons at the bottom
    // -------------------------------------------------------------
    const struct ExtraDef {
        const wchar_t* label;
        const wchar_t* send;
    } extras[16] = {
        { L"২", L"\u09E8" },
        { L"॥", L"\u09F8" },
        { L"ৄ", L"\u09C4" },
        { L"ৠ", L"\u09E0" },
        { L"ঌ", L"\u098C" },
        { L"ৡ", L"\u09E1" },
        { L"ৢ", L"\u09E2" },
        { L"ৣ", L"\u09E3" },
        { L"!", L"!" },
        { L"?", L"?" },
        { L",", L"," },
        { L";", L";" },
        { L":", L":" },
        { L"-", L"-" },
        { L"(", L"(" },
        { L")", L")" }
    };

    const int ex_start_x = 14;
    const int ex_w = 42;
    const int ex_gap = 5;
    const int ex_y = 338;

    for (int i = 0; i < 16; i++) {
        KeyButton btn;
        int x = ex_start_x + i * (ex_w + ex_gap);
        btn.rect = { x, ex_y, x + ex_w, ex_y + 30 };
        btn.label = extras[i].label;
        btn.send_text = extras[i].send;
        g_keys.push_back(btn);
    }
}

int HitTestKey(POINT pt) {
    for (size_t i = 0; i < g_keys.size(); i++) {
        if (PtInRect(&g_keys[i].rect, pt)) {
            return (int)i;
        }
    }
    return -1;
}

void UpdateThemeDWM(HWND hWnd) {
    BOOL use_dark = g_is_dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hWnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &use_dark, sizeof(use_dark));
    DwmSetWindowAttribute(hWnd, 19 /* legacy Win10 */, &use_dark, sizeof(use_dark));

    LONG_PTR exStyle = GetWindowLongPtrW(hWnd, GWL_EXSTYLE);

    if (g_glass_mode == 0) {
        // Mode 0: Hardware Acrylic Glass (DWM system backdrop)
        // Clean non-layered for 100% crisp buttons and true hardware frosted glass
        if (exStyle & WS_EX_LAYERED) {
            SetWindowLongPtrW(hWnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
        }

        MARGINS margins = { -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea(hWnd, &margins);

        int backdrop = 3; // DWMSBT_TRANSIENTWINDOW (Desktop Acrylic)
        DwmSetWindowAttribute(hWnd, 38 /* DWMWA_SYSTEMBACKDROP_TYPE */, &backdrop, sizeof(backdrop));
    } else if (g_glass_mode == 1) {
        // Mode 1: Frosted Glass with Layered Translucency (92% opacity)
        if (!(exStyle & WS_EX_LAYERED)) {
            SetWindowLongPtrW(hWnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
        }

        MARGINS margins = { -1, -1, -1, -1 };
        DwmExtendFrameIntoClientArea(hWnd, &margins);

        int backdrop = 3; // Acrylic backdrop
        DwmSetWindowAttribute(hWnd, 38, &backdrop, sizeof(backdrop));

        SetLayeredWindowAttributes(hWnd, 0, 235 /* 92% alpha */, LWA_ALPHA);
    } else {
        // Mode 2: Solid (100% Opaque modern mode)
        if (exStyle & WS_EX_LAYERED) {
            SetWindowLongPtrW(hWnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
        }

        MARGINS margins = { 0, 0, 0, 0 };
        DwmExtendFrameIntoClientArea(hWnd, &margins);

        int backdrop = 1; // DWMSBT_NONE
        DwmSetWindowAttribute(hWnd, 38, &backdrop, sizeof(backdrop));
    }

    // Refresh window frame and caption
    SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hWnd = hWnd;

            g_is_dark = LoadVkThemeDark();
            g_glass_mode = LoadGlassModeSetting();

            UpdateThemeDWM(hWnd);

            HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;
            g_hAppIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
            if (!g_hAppIcon) {
                g_hAppIcon = (HICON)LoadImageW(NULL, L"assets/icon.ico", IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
            }

            g_hFontBn = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Nirmala UI");
            g_hFontBnLarge = CreateFontW(-16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Nirmala UI");
            g_hFontEn = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontEnBold = CreateFontW(-12, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontSection = CreateFontW(-11, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontTitle = CreateFontW(-13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                      CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            BuildLayout();
            break;
        }

        case WM_MOUSEACTIVATE:
            // Critical: Never steal focus from active document (Word, Notepad, WhatsApp, etc.)
            return MA_NOACTIVATE;

        case WM_LBUTTONDOWN: {
            POINT pt = { (short)LOWORD(lParam), (short)HIWORD(lParam) };
            int idx = HitTestKey(pt);
            if (idx >= 0) {
                g_pressed_key_idx = idx;
                SetCapture(hWnd);
                InvalidateRect(hWnd, &g_keys[idx].rect, FALSE);
            } else if (pt.y <= 38 && pt.x < 505) {
                ReleaseCapture();
                SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            if (g_pressed_key_idx >= 0) {
                POINT pt = { LOWORD(lParam), HIWORD(lParam) };
                int idx = HitTestKey(pt);
                int act_idx = g_pressed_key_idx;
                g_pressed_key_idx = -1;
                ReleaseCapture();

                if (idx == act_idx && idx >= 0 && idx < (int)g_keys.size()) {
                    const auto& btn = g_keys[idx];
                    if (btn.is_theme_btn) {
                        g_is_dark = !g_is_dark;
                        SaveVkThemeDark(g_is_dark);
                        UpdateThemeDWM(hWnd);
                        BuildLayout();
                        InvalidateRect(hWnd, NULL, FALSE);
                    } else if (btn.is_transp_btn) {
                        g_glass_mode = (g_glass_mode + 1) % 3;
                        SaveGlassModeSetting(g_glass_mode);
                        UpdateThemeDWM(hWnd);
                        BuildLayout();
                        InvalidateRect(hWnd, NULL, FALSE);
                    } else if (btn.special_vk != 0) {
                        SendSpecialKey((WORD)btn.special_vk);
                    } else if (!btn.send_text.empty()) {
                        SendString(btn.send_text);
                    }
                }
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            POINT pt = { LOWORD(lParam), HIWORD(lParam) };
            int idx = HitTestKey(pt);
            if (idx != g_hover_key_idx) {
                int old_idx = g_hover_key_idx;
                g_hover_key_idx = idx;

                if (old_idx >= 0 && old_idx < (int)g_keys.size()) {
                    InvalidateRect(hWnd, &g_keys[old_idx].rect, FALSE);
                }
                if (idx >= 0 && idx < (int)g_keys.size()) {
                    InvalidateRect(hWnd, &g_keys[idx].rect, FALSE);
                }

                TRACKMOUSEEVENT tme = {};
                tme.cbSize = sizeof(TRACKMOUSEEVENT);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = hWnd;
                TrackMouseEvent(&tme);
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (g_hover_key_idx >= 0) {
                int old_idx = g_hover_key_idx;
                g_hover_key_idx = -1;
                if (old_idx >= 0 && old_idx < (int)g_keys.size()) {
                    InvalidateRect(hWnd, &g_keys[old_idx].rect, FALSE);
                }
            }
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            RECT client_rc;
            GetClientRect(hWnd, &client_rc);
            int w = client_rc.right;
            int h = client_rc.bottom;

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBitmap = CreateCompatibleBitmap(hdc, w, h);
            HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

            if (g_glass_mode != 2) {
                // In extended frame mode, painting BLACK_BRUSH instructs DWM to reveal the Acrylic Frosted Glass!
                FillRect(memDC, &client_rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
            } else {
                // Solid mode background
                COLORREF colBg = g_is_dark ? RGB(18, 22, 30) : RGB(238, 242, 248);
                HBRUSH bgBrush = CreateSolidBrush(colBg);
                FillRect(memDC, &client_rc, bgBrush);
                DeleteObject(bgBrush);
            }

            SetBkMode(memDC, TRANSPARENT);

            // 1. Top Header
            if (g_hAppIcon) {
                DrawIconEx(memDC, 14, 9, g_hAppIcon, 18, 18, 0, NULL, DI_NORMAL);
            }
            SelectObject(memDC, g_hFontTitle);
            SetTextColor(memDC, g_is_dark ? RGB(147, 197, 253) : RGB(29, 78, 216));
            TextOutW(memDC, 38, 9, L"Likhi Virtual Keyboard", 22);

            SelectObject(memDC, g_hFontSection);
            SetTextColor(memDC, g_is_dark ? RGB(186, 230, 253) : RGB(71, 85, 105));
            TextOutW(memDC, 210, 11, L"• অ্যাক্রিলিক গ্লাস কীবোর্ড (Glass Edition)", 42);

            // 2. Section Labels
            SetTextColor(memDC, g_is_dark ? RGB(226, 232, 240) : RGB(51, 65, 85));
            TextOutW(memDC, 448, 44, L"Few conjuncts (সাধারণ যুক্তবর্ণ):", 33);
            TextOutW(memDC, 14, 320, L"Extra Characters (Less used) / অতিরিক্ত চিহ্ন ও বিরামচিহ্ন:", 60);

            // Frosted glass panel around Conjuncts
            HPEN boxPen = CreatePen(PS_SOLID, 1, g_is_dark ? RGB(65, 80, 110) : RGB(180, 195, 215));
            HBRUSH boxBrush = CreateSolidBrush(g_is_dark ? RGB(22, 28, 38) : RGB(245, 248, 252));
            SelectObject(memDC, boxPen);
            SelectObject(memDC, boxBrush);
            RoundRect(memDC, 444, 40, 765, 132, 6, 6);
            DeleteObject(boxBrush);

            // Frosted glass separator line above Extra Characters
            MoveToEx(memDC, 14, 316, NULL);
            LineTo(memDC, 761, 316);
            DeleteObject(boxPen);

            // 3. Render All Buttons as Cut-Glass Tiles
            for (size_t i = 0; i < g_keys.size(); i++) {
                const auto& btn = g_keys[i];
                bool is_pressed = (g_pressed_key_idx == (int)i);
                bool is_hover = (g_hover_key_idx == (int)i && !is_pressed);

                COLORREF colBtnBg, colBorder, colSheen, colText;

                if (g_is_dark) {
                    if (btn.is_fala) {
                        // Ruby Glass Tile
                        colBtnBg = is_pressed ? RGB(85, 30, 42) : (is_hover ? RGB(75, 28, 38) : RGB(52, 22, 30));
                        colBorder = is_hover ? RGB(248, 113, 113) : RGB(180, 45, 60);
                        colSheen = RGB(220, 80, 100);
                        colText = RGB(255, 225, 230);
                    } else if (btn.is_transp_btn) {
                        // Cyan Crystal Glass Tile
                        colBtnBg = is_pressed ? RGB(20, 55, 95) : (is_hover ? RGB(30, 68, 110) : RGB(22, 45, 75));
                        colBorder = is_hover ? RGB(125, 211, 252) : RGB(56, 189, 248);
                        colSheen = RGB(120, 200, 255);
                        colText = RGB(224, 242, 254);
                    } else if (btn.is_special || btn.is_theme_btn) {
                        // Sapphire Slate Glass Tile
                        colBtnBg = is_pressed ? RGB(28, 55, 105) : (is_hover ? RGB(42, 62, 95) : RGB(30, 44, 68));
                        colBorder = is_hover ? RGB(96, 165, 250) : RGB(65, 95, 140);
                        colSheen = RGB(100, 145, 210);
                        colText = RGB(241, 245, 249);
                    } else {
                        // Regular Consonant / Digit / Vowel Frosted Glass Tile
                        colBtnBg = is_pressed ? RGB(24, 45, 80) : (is_hover ? RGB(45, 54, 72) : RGB(32, 38, 52));
                        colBorder = is_hover ? RGB(96, 165, 250) : RGB(68, 82, 108);
                        colSheen = RGB(100, 122, 155);
                        colText = RGB(255, 255, 255); // 100% PURE SNOW-WHITE BOLD
                    }
                } else {
                    // Light Glass (Frosted Ice)
                    if (btn.is_fala) {
                        // Rose Glass Tile
                        colBtnBg = is_pressed ? RGB(254, 205, 205) : (is_hover ? RGB(254, 226, 226) : RGB(255, 241, 242));
                        colBorder = is_hover ? RGB(225, 29, 72) : RGB(244, 63, 94);
                        colSheen = RGB(255, 255, 255);
                        colText = RGB(159, 18, 57);
                    } else if (btn.is_transp_btn) {
                        // Cyan Ice Glass Tile
                        colBtnBg = is_pressed ? RGB(219, 234, 254) : (is_hover ? RGB(224, 242, 254) : RGB(240, 249, 255));
                        colBorder = is_hover ? RGB(37, 99, 235) : RGB(56, 189, 248);
                        colSheen = RGB(255, 255, 255);
                        colText = RGB(2, 132, 199);
                    } else if (btn.is_special || btn.is_theme_btn) {
                        // Slate Ice Tile
                        colBtnBg = is_pressed ? RGB(219, 234, 254) : (is_hover ? RGB(230, 238, 248) : RGB(241, 245, 249));
                        colBorder = is_hover ? RGB(37, 99, 235) : RGB(148, 163, 184);
                        colSheen = RGB(255, 255, 255);
                        colText = RGB(30, 41, 59);
                    } else {
                        // Regular Frosted White Tile with Crisp Jet Black Text
                        colBtnBg = is_pressed ? RGB(219, 234, 254) : (is_hover ? RGB(240, 249, 255) : RGB(255, 255, 255));
                        colBorder = is_hover ? RGB(37, 99, 235) : RGB(156, 163, 175);
                        colSheen = RGB(255, 255, 255);
                        colText = RGB(15, 23, 42); // 100% JET BLACK (Zero fading!)
                    }
                }

                // 1. Draw Glass Tile Body
                HBRUSH btnBrush = CreateSolidBrush(colBtnBg);
                HPEN btnPen = CreatePen(PS_SOLID, 1, colBorder);
                SelectObject(memDC, btnBrush);
                SelectObject(memDC, btnPen);

                RoundRect(memDC, btn.rect.left, btn.rect.top, btn.rect.right, btn.rect.bottom, 6, 6);

                DeleteObject(btnPen);
                DeleteObject(btnBrush);

                // 2. Draw Glass Specular Top Highlight (Sheen Reflection)
                if (!is_pressed) {
                    HPEN sheenPen = CreatePen(PS_SOLID, 1, colSheen);
                    SelectObject(memDC, sheenPen);
                    MoveToEx(memDC, btn.rect.left + 3, btn.rect.top + 1, NULL);
                    LineTo(memDC, btn.rect.right - 3, btn.rect.top + 1);
                    DeleteObject(sheenPen);
                }

                // 3. Select Font & Draw Crisp Bengali / English Text
                if (btn.is_special || btn.is_theme_btn || btn.is_transp_btn) {
                    SelectObject(memDC, g_hFontEnBold);
                } else if (btn.is_fala) {
                    SelectObject(memDC, g_hFontBn);
                } else {
                    SelectObject(memDC, g_hFontBnLarge);
                }

                SetTextColor(memDC, colText);
                RECT rcText = btn.rect;
                if (is_pressed) {
                    rcText.top += 1;
                    rcText.left += 1;
                }
                DrawTextW(memDC, btn.label.c_str(), -1, &rcText, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            // Blit to screen
            BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);

            SelectObject(memDC, oldBitmap);
            DeleteObject(memBitmap);
            DeleteDC(memDC);

            EndPaint(hWnd, &ps);
            break;
        }

        case WM_DESTROY:
            if (g_hFontBn) DeleteObject(g_hFontBn);
            if (g_hFontBnLarge) DeleteObject(g_hFontBnLarge);
            if (g_hFontEn) DeleteObject(g_hFontEn);
            if (g_hFontEnBold) DeleteObject(g_hFontEnBold);
            if (g_hFontSection) DeleteObject(g_hFontSection);
            if (g_hFontTitle) DeleteObject(g_hFontTitle);
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

} // namespace

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icex);

    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
    wcex.lpszClassName = L"LikhiVirtualKeyboardClass";

    RegisterClassExW(&wcex);

    RECT rc = { 0, 0, 776, 376 };
    AdjustWindowRectEx(&rc, WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE, WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE);
    int win_w = rc.right - rc.left;
    int win_h = rc.bottom - rc.top;

    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    int start_x = (screen_w - win_w) / 2;
    int start_y = screen_h - win_h - 70;

    HWND hWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        L"LikhiVirtualKeyboardClass",
        L"Likhi Virtual Keyboard — Glass Edition",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        start_x, start_y, win_w, win_h,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd) return FALSE;

    ShowWindow(hWnd, SW_SHOWNOACTIVATE);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
