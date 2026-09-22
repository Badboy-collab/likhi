#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <commdlg.h>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <shlobj.h>

#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

enum SectionID {
    SEC_GENERAL = 0,
    SEC_TYPING,
    SEC_SUGGESTIONS,
    SEC_BANGLISH,
    SEC_DICTIONARY,
    SEC_KEYBOARD,
    SEC_VOICE,
    SEC_APPEARANCE,
    SEC_ADVANCED,
    SEC_ABOUT,
    SEC_COUNT
};

#define IDC_NAV_BASE          1000
#define IDC_CHK_ENABLE_LIKHI  2001
#define IDC_CHK_STARTUP       2002
#define IDC_CHK_BANGLA_TYPING 2003
#define IDC_CHK_BANGLISH      2004
#define IDC_CHK_ENG_TO_BAN    2005
#define IDC_CHK_FUZZY         2006
#define IDC_CHK_SUGGESTIONS   2007
#define IDC_CHK_PREDICTION    2008
#define IDC_CHK_ENG_CANDIDATE 2009
#define IDC_CHK_AUTOCORRECT   2010
#define IDC_RADIO_CAND3       2011
#define IDC_RADIO_CAND4       2012
#define IDC_RADIO_CAND5       2013
#define IDC_RADIO_THEME_SYS   2014
#define IDC_RADIO_THEME_LIGHT 2015
#define IDC_RADIO_THEME_DARK  2016
#define IDC_LIST_DICT         2017
#define IDC_EDIT_ROMAN        2018
#define IDC_EDIT_BANGLA       2019
#define IDC_BTN_ADD_WORD      2020
#define IDC_BTN_DEL_WORD      2021
#define IDC_BTN_IMPORT        2022
#define IDC_BTN_EXPORT        2023
#define IDC_BTN_RESET_DEF     2024
#define IDC_BTN_CHECK_UPDATE  2025
#define IDC_BTN_SAVE          2026
#define IDC_BTN_CLOSE         2027
#define IDC_LBL_STATUS        2028
#define IDC_BTN_CLEAR_FIELDS  2029
#define IDC_EDIT_SEARCH_DICT  2030
#define IDC_BTN_LAUNCH_VK     2031
#define IDC_CHK_ENABLE_VOICE  2032
#define IDC_BTN_TEST_VOICE    2033

struct AppSettings {
    bool enable_likhi = true;
    bool launch_startup = false;
    bool bangla_typing = true;
    bool banglish_recog = true;
    bool eng_to_bangla = true;
    bool fuzzy_spelling = true;
    bool show_suggestions = true;
    bool word_prediction = true;
    bool show_eng_candidate = true;
    bool auto_correct = false;
    bool enable_voice = true;
    int max_candidates = 5;
    int theme = 0;
};

struct DictEntry {
    std::wstring id;
    std::wstring roman_key;
    std::wstring bengali_word;
    uint32_t frequency = 1;
    uint64_t created_at = 0;
    uint64_t updated_at = 0;
};

static AppSettings g_settings;
static std::wstring g_config_path;
static SectionID g_active_section = SEC_GENERAL;
static std::vector<DictEntry> g_dict_entries;

static HWND g_hNavButtons[SEC_COUNT];
static std::vector<HWND> g_section_controls[SEC_COUNT];
static HWND hListDict, hEditRoman, hEditBangla, hLblStatus, hEditSearchDict;
static HFONT hFontTitle = NULL;
static HFONT hFontHeader = NULL;
static HFONT hFontBody = NULL;
static HFONT hFontBold = NULL;
static HFONT hFontSub = NULL;
static HFONT hFontCredit = NULL;

static HICON hAppIcon = NULL;

static HBRUSH hBrushBg = NULL;
static HBRUSH hBrushSidebar = NULL;
static HBRUSH hBrushCard = NULL;
static HBRUSH hBrushActiveNav = NULL;
static HBRUSH hBrushHoverNav = NULL;
static HBRUSH hBrushAccent = NULL;
static HBRUSH hBrushList = NULL;
static HBRUSH hBrushEdit = NULL;
static bool g_current_is_dark = false;

static bool IsSystemDarkMode() {
    DWORD val = 1;
    DWORD size = sizeof(val);
    if (RegGetValueW(HKEY_CURRENT_USER,
                     L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                     L"AppsUseLightTheme", RRF_RT_REG_DWORD, NULL, &val, &size) == ERROR_SUCCESS) {
        return (val == 0);
    }
    return false;
}

static bool ShouldUseDarkMode(int theme) {
    if (theme == 1) return false;
    if (theme == 2) return true;
    return IsSystemDarkMode();
}

static void ApplyTheme(HWND hWnd, bool is_dark) {
    g_current_is_dark = is_dark;

    // 1. DWM Immersive Dark Mode for Title Bar (Windows 10/11)
    BOOL dark_val = is_dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hWnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark_val, sizeof(dark_val));
    DwmSetWindowAttribute(hWnd, 19 /* legacy Win10 */, &dark_val, sizeof(dark_val));

    // 2. Recreate GDI Brushes
    if (hBrushBg) DeleteObject(hBrushBg);
    if (hBrushSidebar) DeleteObject(hBrushSidebar);
    if (hBrushCard) DeleteObject(hBrushCard);
    if (hBrushActiveNav) DeleteObject(hBrushActiveNav);
    if (hBrushHoverNav) DeleteObject(hBrushHoverNav);
    if (hBrushAccent) DeleteObject(hBrushAccent);
    if (hBrushList) DeleteObject(hBrushList);
    if (hBrushEdit) DeleteObject(hBrushEdit);

    if (is_dark) {
        hBrushBg = CreateSolidBrush(RGB(24, 24, 28));
        hBrushSidebar = CreateSolidBrush(RGB(32, 32, 36));
        hBrushCard = CreateSolidBrush(RGB(40, 40, 46));
        hBrushActiveNav = CreateSolidBrush(RGB(37, 99, 235));
        hBrushHoverNav = CreateSolidBrush(RGB(50, 54, 66));
        hBrushAccent = CreateSolidBrush(RGB(96, 165, 250));
        hBrushList = CreateSolidBrush(RGB(32, 32, 36));
        hBrushEdit = CreateSolidBrush(RGB(32, 32, 36));
    } else {
        hBrushBg = CreateSolidBrush(RGB(248, 250, 252));
        hBrushSidebar = CreateSolidBrush(RGB(241, 245, 249));
        hBrushCard = CreateSolidBrush(RGB(255, 255, 255));
        hBrushActiveNav = CreateSolidBrush(RGB(37, 99, 235));
        hBrushHoverNav = CreateSolidBrush(RGB(226, 232, 240));
        hBrushAccent = CreateSolidBrush(RGB(14, 165, 233));
        hBrushList = CreateSolidBrush(RGB(255, 255, 255));
        hBrushEdit = CreateSolidBrush(RGB(255, 255, 255));
    }

    SetClassLongPtr(hWnd, GCLP_HBRBACKGROUND, (LONG_PTR)hBrushBg);

    for (int s = 0; s < SEC_COUNT; s++) {
        for (HWND hCtrl : g_section_controls[s]) {
            if (is_dark) {
                SetWindowTheme(hCtrl, L"DarkMode_Explorer", NULL);
            } else {
                SetWindowTheme(hCtrl, L"", L"");
            }
        }
    }
    if (hListDict) SetWindowTheme(hListDict, is_dark ? L"DarkMode_Explorer" : L"Explorer", NULL);
    if (hEditRoman) SetWindowTheme(hEditRoman, is_dark ? L"DarkMode_CFD" : L"", NULL);
    if (hEditBangla) SetWindowTheme(hEditBangla, is_dark ? L"DarkMode_CFD" : L"", NULL);
    if (hEditSearchDict) SetWindowTheme(hEditSearchDict, is_dark ? L"DarkMode_CFD" : L"", NULL);

    RedrawWindow(hWnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
}

const wchar_t* g_navLabels[SEC_COUNT] = {
    L"  🏠  সাধারণ (General)",
    L"  ⌨️  টাইপিং (Typing)",
    L"  💡  সাজেশন (Suggestions)",
    L"  🌐  বাংলিশ (Banglish)",
    L"  📖  অভিধান (Dictionary)",
    L"  🎛️  কীবোর্ড (Keyboard)",
    L"  🎙️  ভয়েস (Voice Typing)",
    L"  🎨  রূপ (Appearance)",
    L"  ⚙️  উন্নত (Advanced)",
    L"  ℹ️  পরিচিতি (About)"
};

std::wstring GetConfigDirectory() {
    wchar_t path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path))) {
        std::wstring dir = std::wstring(path) + L"\\PC-Bangla-Typing-App";
        CreateDirectoryW(dir.c_str(), NULL);
        return dir;
    }
    return L".";
}

static bool CheckStartupRegistry() {
    HKEY hKey = NULL;
    bool exists = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0;
        if (RegQueryValueExW(hKey, L"Likhi", NULL, &type, NULL, NULL) == ERROR_SUCCESS) {
            exists = true;
        }
        RegCloseKey(hKey);
    }
    return exists;
}

static void SetStartupRegistry(bool enable) {
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t exePath[MAX_PATH];
            if (GetModuleFileNameW(NULL, exePath, MAX_PATH) > 0) {
                std::wstring cmd = L"\"" + std::wstring(exePath) + L"\"";
                RegSetValueExW(hKey, L"Likhi", 0, REG_SZ, (const BYTE*)cmd.c_str(), (DWORD)((cmd.size() + 1) * sizeof(wchar_t)));
            }
        } else {
            RegDeleteValueW(hKey, L"Likhi");
        }
        RegCloseKey(hKey);
    }
}

void LoadSettings() {
    g_config_path = GetConfigDirectory() + L"\\settings.json";
    std::ifstream in(g_config_path.c_str());
    if (in.is_open()) {
        std::string line;
        while (std::getline(in, line)) {
            if (line.find("\"enable_likhi\": false") != std::string::npos) g_settings.enable_likhi = false;
            if (line.find("\"auto_correct\": true") != std::string::npos) g_settings.auto_correct = true;
            if (line.find("\"auto_correct\": false") != std::string::npos) g_settings.auto_correct = false;
            if (line.find("\"show_suggestions\": false") != std::string::npos) g_settings.show_suggestions = false;
            if (line.find("\"max_candidates\": 3") != std::string::npos) g_settings.max_candidates = 3;
            if (line.find("\"max_candidates\": 4") != std::string::npos) g_settings.max_candidates = 4;
            if (line.find("\"max_candidates\": 5") != std::string::npos) g_settings.max_candidates = 5;
            if (line.find("\"launch_startup\": true") != std::string::npos) g_settings.launch_startup = true;
            if (line.find("\"launch_startup\": false") != std::string::npos) g_settings.launch_startup = false;
            if (line.find("\"eng_to_bangla\": false") != std::string::npos) g_settings.eng_to_bangla = false;
            if (line.find("\"word_prediction\": false") != std::string::npos) g_settings.word_prediction = false;
            if (line.find("\"show_eng_candidate\": false") != std::string::npos) g_settings.show_eng_candidate = false;
            if (line.find("\"fuzzy_spelling\": false") != std::string::npos) g_settings.fuzzy_spelling = false;
            if (line.find("\"enable_voice\": false") != std::string::npos) g_settings.enable_voice = false;
            if (line.find("\"enable_voice\": true") != std::string::npos) g_settings.enable_voice = true;
            if (line.find("\"theme\": 1") != std::string::npos) g_settings.theme = 1;
            if (line.find("\"theme\": 2") != std::string::npos) g_settings.theme = 2;
        }
    }
    // Also synchronize with actual Windows Run registry state if present
    if (CheckStartupRegistry()) {
        g_settings.launch_startup = true;
    }
}

void SaveSettings() {
    std::ofstream out(g_config_path.c_str());
    if (!out.is_open()) return;

    out << "{\n";
    out << "  \"enable_likhi\": " << (g_settings.enable_likhi ? "true" : "false") << ",\n";
    out << "  \"launch_startup\": " << (g_settings.launch_startup ? "true" : "false") << ",\n";
    out << "  \"bangla_typing\": " << (g_settings.bangla_typing ? "true" : "false") << ",\n";
    out << "  \"banglish_recog\": " << (g_settings.banglish_recog ? "true" : "false") << ",\n";
    out << "  \"eng_to_bangla\": " << (g_settings.eng_to_bangla ? "true" : "false") << ",\n";
    out << "  \"fuzzy_spelling\": " << (g_settings.fuzzy_spelling ? "true" : "false") << ",\n";
    out << "  \"show_suggestions\": " << (g_settings.show_suggestions ? "true" : "false") << ",\n";
    out << "  \"word_prediction\": " << (g_settings.word_prediction ? "true" : "false") << ",\n";
    out << "  \"show_eng_candidate\": " << (g_settings.show_eng_candidate ? "true" : "false") << ",\n";
    out << "  \"auto_correct\": " << (g_settings.auto_correct ? "true" : "false") << ",\n";
    out << "  \"enable_voice\": " << (g_settings.enable_voice ? "true" : "false") << ",\n";
    out << "  \"max_candidates\": " << g_settings.max_candidates << ",\n";
    out << "  \"theme\": " << g_settings.theme << "\n";
    out << "}\n";
}

static std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), NULL, 0);
    std::wstring res(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &res[0], size);
    return res;
}

static std::string WideToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string res(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &res[0], size, NULL, NULL);
    return res;
}

static std::wstring ToLowerW(const std::wstring& s) {
    std::wstring res = s;
    for (auto& c : res) {
        if (c >= L'A' && c <= L'Z') c = c - L'A' + L'a';
    }
    return res;
}

static std::wstring TrimW(const std::wstring& s) {
    size_t start = 0;
    while (start < s.size() && (s[start] == L' ' || s[start] == L'\t' || s[start] == L'\r' || s[start] == L'\n')) {
        start++;
    }
    size_t end = s.size();
    while (end > start && (s[end - 1] == L' ' || s[end - 1] == L'\t' || s[end - 1] == L'\r' || s[end - 1] == L'\n')) {
        end--;
    }
    return s.substr(start, end - start);
}

static uint64_t GetCurrentUnixTimestamp() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

void LoadDictionaryEntries() {
    g_dict_entries.clear();
    std::wstring dict_file = GetConfigDirectory() + L"\\personal_dict.txt";
    std::ifstream in(WideToUtf8(dict_file).c_str());
    if (!in.is_open()) {
        std::wstring old_file = GetConfigDirectory() + L"\\user_dict.txt";
        in.open(WideToUtf8(old_file).c_str());
    }
    if (!in.is_open()) return;

    std::string line;
    uint64_t next_id = 1;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::stringstream ss(line);
        std::string token;
        std::vector<std::string> tokens;
        while (std::getline(ss, token, '\t')) {
            tokens.push_back(token);
        }
        if (tokens.size() < 2) continue;

        DictEntry entry;
        entry.roman_key = ToLowerW(TrimW(Utf8ToWide(tokens[0])));
        entry.bengali_word = TrimW(Utf8ToWide(tokens[1]));
        if (entry.roman_key.empty() || entry.bengali_word.empty()) continue;

        entry.frequency = (tokens.size() >= 3) ? (uint32_t)std::strtoul(tokens[2].c_str(), nullptr, 10) : 1;
        if (entry.frequency == 0) entry.frequency = 1;
        entry.created_at = (tokens.size() >= 4) ? std::strtoull(tokens[3].c_str(), nullptr, 10) : 0;
        entry.updated_at = (tokens.size() >= 5) ? std::strtoull(tokens[4].c_str(), nullptr, 10) : entry.created_at;
        entry.id = (tokens.size() >= 6) ? Utf8ToWide(tokens[5]) : std::to_wstring(next_id++);

        g_dict_entries.push_back(entry);
    }
}

bool SaveDictionaryEntries() {
    std::wstring dict_file = GetConfigDirectory() + L"\\personal_dict.txt";
    std::string temp_file = WideToUtf8(dict_file) + ".tmp";
    {
        std::ofstream out(temp_file.c_str(), std::ios::trunc);
        if (!out.is_open()) return false;
        for (const auto& e : g_dict_entries) {
            out << WideToUtf8(e.roman_key) << "\t"
                << WideToUtf8(e.bengali_word) << "\t"
                << e.frequency << "\t"
                << e.created_at << "\t"
                << e.updated_at << "\t"
                << WideToUtf8(e.id) << "\n";
        }
        out.flush();
    }
    std::wstring wtemp = dict_file + L".tmp";
    if (!MoveFileExW(wtemp.c_str(), dict_file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(dict_file.c_str());
        MoveFileW(wtemp.c_str(), dict_file.c_str());
    }
    return true;
}

void RefreshDictionaryList(const std::wstring& filter = L"") {
    if (!hListDict) return;
    SendMessage(hListDict, LB_RESETCONTENT, 0, 0);

    std::wstring lower_filter = ToLowerW(TrimW(filter));

    for (size_t i = 0; i < g_dict_entries.size(); i++) {
        const auto& e = g_dict_entries[i];
        if (!lower_filter.empty()) {
            if (e.roman_key.find(lower_filter) == std::wstring::npos &&
                e.bengali_word.find(lower_filter) == std::wstring::npos) {
                continue;
            }
        }
        std::wstring display = e.roman_key + L"    →    " + e.bengali_word;
        int idx = (int)SendMessageW(hListDict, LB_ADDSTRING, 0, (LPARAM)display.c_str());
        SendMessage(hListDict, LB_SETITEMDATA, idx, (LPARAM)i);
    }
}

void SwitchSection(HWND hWnd, SectionID sec) {
    g_active_section = sec;
    for (int i = 0; i < SEC_COUNT; i++) {
        int show = (i == sec) ? SW_SHOW : SW_HIDE;
        for (HWND h : g_section_controls[i]) {
            ShowWindow(h, show);
        }
        InvalidateRect(g_hNavButtons[i], NULL, TRUE);
    }
    if (sec == SEC_DICTIONARY) {
        LoadDictionaryEntries();
        if (hEditSearchDict) {
            SetWindowTextW(hEditSearchDict, L"");
        }
        RefreshDictionaryList();
    }
    InvalidateRect(hWnd, NULL, TRUE);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            LoadSettings();

            HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

            // Load Application Icon (High-res for UI rendering)
            hAppIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(1), IMAGE_ICON, 256, 256, LR_DEFAULTCOLOR);
            if (!hAppIcon) {
                hAppIcon = (HICON)LoadImageW(NULL, L"assets/icon.ico", IMAGE_ICON, 256, 256, LR_LOADFROMFILE);
            }
            if (!hAppIcon) {
                hAppIcon = (HICON)LoadImageW(NULL, L"release_package/icon.ico", IMAGE_ICON, 256, 256, LR_LOADFROMFILE);
            }

            // Fonts: Nirmala UI for perfect Bengali, Segoe UI for English
            hFontTitle = CreateFontW(-22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Nirmala UI");
            hFontHeader = CreateFontW(-19, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Nirmala UI");
            hFontBody = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Nirmala UI");
            hFontBold = CreateFontW(-14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Nirmala UI");
            hFontSub = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            hFontCredit = CreateFontW(-16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            // Initial Palette
            ApplyTheme(hWnd, ShouldUseDarkMode(g_settings.theme));

            // ==============================================================
            // OWNER-DRAWN SIDEBAR NAVIGATION BUTTONS
            // ==============================================================
            for (int i = 0; i < SEC_COUNT; i++) {
                g_hNavButtons[i] = CreateWindowW(
                    L"BUTTON", g_navLabels[i],
                    WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                    15, 78 + (i * 37), 205, 33,
                    hWnd, (HMENU)(intptr_t)(IDC_NAV_BASE + i), hInst, NULL
                );
            }

            // ==============================================================
            // SECTION 0: GENERAL
            // ==============================================================
            HWND hG_Title = CreateWindowW(L"STATIC", L"Likhi — সাধারণ পছন্দসমূহ", WS_CHILD | SS_LEFT, 250, 30, 520, 28, hWnd, NULL, NULL, NULL);
            SendMessage(hG_Title, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

            HWND hG_Status = CreateWindowW(L"STATIC", L"🟢 Likhi স্ট্যাটাস: সক্রিয় ও প্রস্তুত (Active & Ready)", WS_CHILD | SS_LEFT, 250, 75, 520, 24, hWnd, NULL, NULL, NULL);
            SendMessage(hG_Status, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            HWND hChkLikhi = CreateWindowW(L"BUTTON", L"Likhi টাইপিং ইঞ্জিন সক্রিয় রাখুন (Enable Likhi Service)", WS_CHILD | BS_AUTOCHECKBOX, 250, 115, 520, 24, hWnd, (HMENU)IDC_CHK_ENABLE_LIKHI, NULL, NULL);
            SendMessage(hChkLikhi, WM_SETFONT, (WPARAM)hFontBody, TRUE);
            SendMessage(hChkLikhi, BM_SETCHECK, g_settings.enable_likhi ? BST_CHECKED : BST_UNCHECKED, 0);

            HWND hChkStart = CreateWindowW(L"BUTTON", L"উইন্ডোজ চালুর সাথে Likhi চালু করুন (Start with Windows)", WS_CHILD | BS_AUTOCHECKBOX, 250, 155, 520, 24, hWnd, (HMENU)IDC_CHK_STARTUP, NULL, NULL);
            SendMessage(hChkStart, WM_SETFONT, (WPARAM)hFontBody, TRUE);
            SendMessage(hChkStart, BM_SETCHECK, g_settings.launch_startup ? BST_CHECKED : BST_UNCHECKED, 0);

            HWND hG_LangCard = CreateWindowW(L"STATIC", L"📌 সক্রিয় ইনপুট প্রোফাইল: বাংলা (বাংলাদেশ) — Likhi\n\n• উইন্ডোজ ভাষা পরিবর্তন: Win + Space\n• বাংলা ভয়েস টাইピング: Win + H\n• Likhi Virtual Keyboard (অন-স্ক্রিন কীবোর্ড) উপলব্ধ", WS_CHILD | SS_LEFT, 250, 205, 520, 95, hWnd, NULL, NULL, NULL);
            SendMessage(hG_LangCard, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            HWND hG_BtnVK = CreateWindowW(L"BUTTON", L"⌨️ Likhi Virtual Keyboard খুলুন", WS_CHILD | BS_PUSHBUTTON, 250, 310, 360, 38, hWnd, (HMENU)IDC_BTN_LAUNCH_VK, NULL, NULL);
            SendMessage(hG_BtnVK, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            g_section_controls[SEC_GENERAL].push_back(hG_Title);
            g_section_controls[SEC_GENERAL].push_back(hG_Status);
            g_section_controls[SEC_GENERAL].push_back(hChkLikhi);
            g_section_controls[SEC_GENERAL].push_back(hChkStart);
            g_section_controls[SEC_GENERAL].push_back(hG_LangCard);
            g_section_controls[SEC_GENERAL].push_back(hG_BtnVK);

            // ==============================================================
            // SECTION 1: TYPING
            // ==============================================================
            HWND hT_Title = CreateWindowW(L"STATIC", L"টাইপিং ও ফোনেটিক সেটিংস", WS_CHILD | SS_LEFT, 250, 30, 520, 28, hWnd, NULL, NULL, NULL);
            SendMessage(hT_Title, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

            HWND hChkBng = CreateWindowW(L"BUTTON", L"ফোনেটিক বাংলা টাইপিং চালু রাখুন (Bangla Typing)", WS_CHILD | BS_AUTOCHECKBOX, 250, 80, 520, 24, hWnd, (HMENU)IDC_CHK_BANGLA_TYPING, NULL, NULL);
            SendMessage(hChkBng, WM_SETFONT, (WPARAM)hFontBody, TRUE);
            SendMessage(hChkBng, BM_SETCHECK, g_settings.bangla_typing ? BST_CHECKED : BST_UNCHECKED, 0);

            HWND hChkBngl = CreateWindowW(L"BUTTON", L"বাংলিশ স্বীকৃতি ও উচ্চারণ ম্যাচিং (Banglish Recognition)", WS_CHILD | BS_AUTOCHECKBOX, 250, 120, 520, 24, hWnd, (HMENU)IDC_CHK_BANGLISH, NULL, NULL);
            SendMessage(hChkBngl, WM_SETFONT, (WPARAM)hFontBody, TRUE);
            SendMessage(hChkBngl, BM_SETCHECK, g_settings.banglish_recog ? BST_CHECKED : BST_UNCHECKED, 0);

            HWND hChkE2B = CreateWindowW(L"BUTTON", L"ইংরেজি শব্দের বাংলা রূপান্তর (control -> কন্ট্রোল, office -> অফিস)", WS_CHILD | BS_AUTOCHECKBOX, 250, 160, 520, 24, hWnd, (HMENU)IDC_CHK_ENG_TO_BAN, NULL, NULL);
            SendMessage(hChkE2B, WM_SETFONT, (WPARAM)hFontBody, TRUE);
            SendMessage(hChkE2B, BM_SETCHECK, g_settings.eng_to_bangla ? BST_CHECKED : BST_UNCHECKED, 0);

            HWND hChkFuz = CreateWindowW(L"BUTTON", L"বানান ভুলের সহনশীলতা (Fuzzy Spelling Tolerance)", WS_CHILD | BS_AUTOCHECKBOX, 250, 200, 520, 24, hWnd, (HMENU)IDC_CHK_FUZZY, NULL, NULL);
            SendMessage(hChkFuz, WM_SETFONT, (WPARAM)hFontBody, TRUE);
            SendMessage(hChkFuz, BM_SETCHECK, g_settings.fuzzy_spelling ? BST_CHECKED : BST_UNCHECKED, 0);

            g_section_controls[SEC_TYPING].push_back(hT_Title);
            g_section_controls[SEC_TYPING].push_back(hChkBng);
            g_section_controls[SEC_TYPING].push_back(hChkBngl);
            g_section_controls[SEC_TYPING].push_back(hChkE2B);
            g_section_controls[SEC_TYPING].push_back(hChkFuz);

            // ==============================================================
            // SECTION 2: SUGGESTIONS
            // ==============================================================
            HWND hS_Title = CreateWindowW(L"STATIC", L"স্মার্ট সাজেশন ও শব্দ অনুমান", WS_CHILD | SS_LEFT, 250, 30, 520, 28, hWnd, NULL, NULL, NULL);
            SendMessage(hS_Title, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

            HWND hChkSug = CreateWindowW(L"BUTTON", L"টাইপ করার সময় সাজেশন বার প্রদর্শন করুন", WS_CHILD | BS_AUTOCHECKBOX, 250, 75, 520, 24, hWnd, (HMENU)IDC_CHK_SUGGESTIONS, NULL, NULL);
            SendMessage(hChkSug, WM_SETFONT, (WPARAM)hFontBody, TRUE);
            SendMessage(hChkSug, BM_SETCHECK, g_settings.show_suggestions ? BST_CHECKED : BST_UNCHECKED, 0);

            HWND hChkPred = CreateWindowW(L"BUTTON", L"প্রাসঙ্গিক পরবর্তী শব্দ অনুমান (Contextual Prediction)", WS_CHILD | BS_AUTOCHECKBOX, 250, 110, 520, 24, hWnd, (HMENU)IDC_CHK_PREDICTION, NULL, NULL);
            SendMessage(hChkPred, WM_SETFONT, (WPARAM)hFontBody, TRUE);
            SendMessage(hChkPred, BM_SETCHECK, g_settings.word_prediction ? BST_CHECKED : BST_UNCHECKED, 0);

            HWND hChkEngCand = CreateWindowW(L"BUTTON", L"সাজেশন বারে আসল ইংরেজি রূপ বিকল্প হিসেবে রাখুন (Original English)", WS_CHILD | BS_AUTOCHECKBOX, 250, 145, 520, 24, hWnd, (HMENU)IDC_CHK_ENG_CANDIDATE, NULL, NULL);
            SendMessage(hChkEngCand, WM_SETFONT, (WPARAM)hFontBody, TRUE);
            SendMessage(hChkEngCand, BM_SETCHECK, g_settings.show_eng_candidate ? BST_CHECKED : BST_UNCHECKED, 0);

            HWND hChkAuto = CreateWindowW(L"BUTTON", L"অটো-কারেক্ট চালু করুন (Auto Correct)", WS_CHILD | BS_AUTOCHECKBOX, 250, 185, 520, 24, hWnd, (HMENU)IDC_CHK_AUTOCORRECT, NULL, NULL);
            SendMessage(hChkAuto, WM_SETFONT, (WPARAM)hFontBody, TRUE);
            SendMessage(hChkAuto, BM_SETCHECK, g_settings.auto_correct ? BST_CHECKED : BST_UNCHECKED, 0);

            HWND hS_Expl = CreateWindowW(L"STATIC", L"💡 Auto Correct বন্ধ থাকলে Likhi শুধু সাজেশন দেখাবে, নিজে থেকে আপনার লেখা পরিবর্তন করবে না।", WS_CHILD | SS_LEFT, 250, 215, 520, 36, hWnd, NULL, NULL, NULL);
            SendMessage(hS_Expl, WM_SETFONT, (WPARAM)hFontSub, TRUE);

            HWND hS_CandLbl = CreateWindowW(L"STATIC", L"প্রস্তাবিত শব্দের সংখ্যা:", WS_CHILD | SS_LEFT, 250, 265, 150, 24, hWnd, NULL, NULL, NULL);
            SendMessage(hS_CandLbl, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            HWND hR3 = CreateWindowW(L"BUTTON", L"3", WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP, 400, 265, 45, 24, hWnd, (HMENU)IDC_RADIO_CAND3, NULL, NULL);
            HWND hR4 = CreateWindowW(L"BUTTON", L"4", WS_CHILD | BS_AUTORADIOBUTTON, 455, 265, 45, 24, hWnd, (HMENU)IDC_RADIO_CAND4, NULL, NULL);
            HWND hR5 = CreateWindowW(L"BUTTON", L"5", WS_CHILD | BS_AUTORADIOBUTTON, 510, 265, 45, 24, hWnd, (HMENU)IDC_RADIO_CAND5, NULL, NULL);

            SendMessage(hR3, WM_SETFONT, (WPARAM)hFontBody, TRUE);
            SendMessage(hR4, WM_SETFONT, (WPARAM)hFontBody, TRUE);
            SendMessage(hR5, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            if (g_settings.max_candidates == 3) SendMessage(hR3, BM_SETCHECK, BST_CHECKED, 0);
            else if (g_settings.max_candidates == 4) SendMessage(hR4, BM_SETCHECK, BST_CHECKED, 0);
            else SendMessage(hR5, BM_SETCHECK, BST_CHECKED, 0);

            g_section_controls[SEC_SUGGESTIONS].push_back(hS_Title);
            g_section_controls[SEC_SUGGESTIONS].push_back(hChkSug);
            g_section_controls[SEC_SUGGESTIONS].push_back(hChkPred);
            g_section_controls[SEC_SUGGESTIONS].push_back(hChkEngCand);
            g_section_controls[SEC_SUGGESTIONS].push_back(hChkAuto);
            g_section_controls[SEC_SUGGESTIONS].push_back(hS_Expl);
            g_section_controls[SEC_SUGGESTIONS].push_back(hS_CandLbl);
            g_section_controls[SEC_SUGGESTIONS].push_back(hR3);
            g_section_controls[SEC_SUGGESTIONS].push_back(hR4);
            g_section_controls[SEC_SUGGESTIONS].push_back(hR5);

            // ==============================================================
            // SECTION 3: BANGLISH
            // ==============================================================
            HWND hB_Title = CreateWindowW(L"STATIC", L"বাংলিশ ও ঋণশব্দ ডিকশনারি", WS_CHILD | SS_LEFT, 250, 30, 520, 28, hWnd, NULL, NULL, NULL);
            SendMessage(hB_Title, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

            HWND hB_Desc = CreateWindowW(L"STATIC", L"Likhi আধুনিক বাংলিশ এবং দৈনন্দিন প্রযুক্তি ও অফিসের শব্দগুলো স্বয়ংক্রিয়ভাবে বাংলায় রূপান্তর করে।\n\nউদাহরণ:\n• battery -> ব্যাটারি | charger -> চার্জার\n• control -> কন্ট্রোল | space -> স্পেস\n• meeting -> মিটিং | project -> প্রজেক্ট\n• wifi -> ওয়াই-ফাই | internet -> ইন্টারনেট\n• table -> টেবিল | chair / cher -> চেয়ার\n• google -> গুগল | Google\n• anwar / anoyar -> আনোয়ার", WS_CHILD | SS_LEFT, 250, 75, 520, 280, hWnd, NULL, NULL, NULL);
            SendMessage(hB_Desc, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            g_section_controls[SEC_BANGLISH].push_back(hB_Title);
            g_section_controls[SEC_BANGLISH].push_back(hB_Desc);

            // ==============================================================
            // SECTION 4: DICTIONARY
            // ==============================================================
            HWND hD_Title = CreateWindowW(L"STATIC", L"ব্যক্তিগত শব্দভাণ্ডার (Personal Dictionary)", WS_CHILD | SS_LEFT, 250, 25, 520, 26, hWnd, NULL, NULL, NULL);
            SendMessage(hD_Title, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

            HWND hD_Sub = CreateWindowW(L"STATIC", L"আপনার নিজস্ব কাস্টম শব্দের ফোনেটিক বানান যুক্ত ও পরিচালনা করুন", WS_CHILD | SS_LEFT, 250, 52, 520, 18, hWnd, NULL, NULL, NULL);
            SendMessage(hD_Sub, WM_SETFONT, (WPARAM)hFontSub, TRUE);

            HWND hD_SearchLbl = CreateWindowW(L"STATIC", L"অনুসন্ধান:", WS_CHILD | SS_LEFT, 250, 74, 55, 22, hWnd, NULL, NULL, NULL);
            SendMessage(hD_SearchLbl, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            hEditSearchDict = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 310, 72, 180, 24, hWnd, (HMENU)IDC_EDIT_SEARCH_DICT, NULL, NULL);
            SendMessage(hEditSearchDict, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            hListDict = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VSCROLL | LBS_NOTIFY, 250, 102, 240, 340, hWnd, (HMENU)IDC_LIST_DICT, NULL, NULL);
            SendMessage(hListDict, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            HWND hD_L1 = CreateWindowW(L"STATIC", L"ইংরেজি / ফোনেটিক ইনপুট (English Key):", WS_CHILD | SS_LEFT, 510, 74, 265, 20, hWnd, NULL, NULL, NULL);
            SendMessage(hD_L1, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            hEditRoman = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 510, 96, 265, 26, hWnd, (HMENU)IDC_EDIT_ROMAN, NULL, NULL);
            SendMessage(hEditRoman, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            HWND hD_L2 = CreateWindowW(L"STATIC", L"বাংলা শব্দ (Bangla Word):", WS_CHILD | SS_LEFT, 510, 128, 265, 20, hWnd, NULL, NULL, NULL);
            SendMessage(hD_L2, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            hEditBangla = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 510, 150, 265, 26, hWnd, (HMENU)IDC_EDIT_BANGLA, NULL, NULL);
            SendMessage(hEditBangla, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            HWND hBtnA = CreateWindowW(L"BUTTON", L"সংরক্ষণ (Save)", WS_CHILD | BS_PUSHBUTTON, 510, 186, 128, 32, hWnd, (HMENU)IDC_BTN_ADD_WORD, NULL, NULL);
            SendMessage(hBtnA, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            HWND hBtnC = CreateWindowW(L"BUTTON", L"নতুন (+ New)", WS_CHILD | BS_PUSHBUTTON, 647, 186, 128, 32, hWnd, (HMENU)IDC_BTN_CLEAR_FIELDS, NULL, NULL);
            SendMessage(hBtnC, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            HWND hBtnD = CreateWindowW(L"BUTTON", L"মুছে ফেলুন (Delete)", WS_CHILD | BS_PUSHBUTTON, 510, 226, 265, 30, hWnd, (HMENU)IDC_BTN_DEL_WORD, NULL, NULL);
            SendMessage(hBtnD, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            HWND hBtnI = CreateWindowW(L"BUTTON", L"ইমপোর্ট (.txt)", WS_CHILD | BS_PUSHBUTTON, 510, 264, 128, 30, hWnd, (HMENU)IDC_BTN_IMPORT, NULL, NULL);
            SendMessage(hBtnI, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            HWND hBtnE = CreateWindowW(L"BUTTON", L"এক্সপোর্ট (.txt)", WS_CHILD | BS_PUSHBUTTON, 647, 264, 128, 30, hWnd, (HMENU)IDC_BTN_EXPORT, NULL, NULL);
            SendMessage(hBtnE, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            HWND hD_Tips = CreateWindowW(L"STATIC",
                L"💡 ব্যবহারবিধি:\n"
                L"• যেমন: English [ sakkho ] -> Bangla [ সাক্ষ্য ]\n"
                L"• সংরক্ষণ করলে টাইপিং সাজেশনে এটি ১ম স্থানে আসবে।\n"
                L"• Auto Correct চালু থাকলে স্পেস চাপলে স্বয়ংক্রিয়ভাবে কাঙ্ক্ষিত শব্দ বসে যাবে।\n"
                L"• তালিকা থেকে শব্দে ক্লিক করে সরাসরি এডিট বা ডিলিট করতে পারেন।",
                WS_CHILD | SS_LEFT, 510, 304, 265, 138, hWnd, NULL, NULL, NULL);
            SendMessage(hD_Tips, WM_SETFONT, (WPARAM)hFontSub, TRUE);

            g_section_controls[SEC_DICTIONARY].push_back(hD_Title);
            g_section_controls[SEC_DICTIONARY].push_back(hD_Sub);
            g_section_controls[SEC_DICTIONARY].push_back(hD_SearchLbl);
            g_section_controls[SEC_DICTIONARY].push_back(hEditSearchDict);
            g_section_controls[SEC_DICTIONARY].push_back(hListDict);
            g_section_controls[SEC_DICTIONARY].push_back(hD_L1);
            g_section_controls[SEC_DICTIONARY].push_back(hEditRoman);
            g_section_controls[SEC_DICTIONARY].push_back(hD_L2);
            g_section_controls[SEC_DICTIONARY].push_back(hEditBangla);
            g_section_controls[SEC_DICTIONARY].push_back(hBtnA);
            g_section_controls[SEC_DICTIONARY].push_back(hBtnC);
            g_section_controls[SEC_DICTIONARY].push_back(hBtnD);
            g_section_controls[SEC_DICTIONARY].push_back(hBtnI);
            g_section_controls[SEC_DICTIONARY].push_back(hBtnE);
            g_section_controls[SEC_DICTIONARY].push_back(hD_Tips);

            // ==============================================================
            // SECTION 5: KEYBOARD
            // ==============================================================
            HWND hK_Title = CreateWindowW(L"STATIC", L"কীবোর্ড ও শর্টকাট আচরণ", WS_CHILD | SS_LEFT, 250, 30, 520, 28, hWnd, NULL, NULL, NULL);
            SendMessage(hK_Title, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

            HWND hK_Desc = CreateWindowW(L"STATIC", L"Likhi উইন্ডোজের সকল নেটিভ কীবোর্ড শর্টকাট সম্পূর্ণ অক্ষত রাখে।\n\n• Ctrl+C (Copy), Ctrl+V (Paste), Ctrl+A, Ctrl+Z ইত্যাদি সরাসরি কাজ করে\n• Numpad (0-9, +, -, *, .) সাধারণ সংখ্যার জন্য সংরক্ষিত\n• F1-F12 এবং অ্যারো কী স্বাভাবিকভাবে কাজ করে\n• উইন্ডোজ ভাষা পরিবর্তন: Win + Space\n\nসক্রিয় প্রোফাইল: Bangla (Bangladesh) — Likhi (লিখি)", WS_CHILD | SS_LEFT, 250, 75, 520, 165, hWnd, NULL, NULL, NULL);
            SendMessage(hK_Desc, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            HWND hK_BtnVK = CreateWindowW(L"BUTTON", L"⌨️ Likhi Virtual Keyboard চালু করুন (Launch Virtual Keyboard)", WS_CHILD | BS_PUSHBUTTON, 250, 255, 520, 38, hWnd, (HMENU)IDC_BTN_LAUNCH_VK, NULL, NULL);
            SendMessage(hK_BtnVK, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            HWND hK_VKDesc = CreateWindowW(L"STATIC", L"💡 Likhi Virtual Keyboard অন-স্ক্রিন কীবোর্ড দিয়ে মাউস ক্লিকের মাধ্যমেই সরাসরি যেকোনো অ্যাপে (Word, Notepad, Browser) বাংলা টাইপ করতে পারবেন। মাউস ক্লিকে ফোকাস হারাবে না।", WS_CHILD | SS_LEFT, 250, 305, 520, 48, hWnd, NULL, NULL, NULL);
            SendMessage(hK_VKDesc, WM_SETFONT, (WPARAM)hFontSub, TRUE);

            g_section_controls[SEC_KEYBOARD].push_back(hK_Title);
            g_section_controls[SEC_KEYBOARD].push_back(hK_Desc);
            g_section_controls[SEC_KEYBOARD].push_back(hK_BtnVK);
            g_section_controls[SEC_KEYBOARD].push_back(hK_VKDesc);

            // ==============================================================
            // SECTION 6: VOICE (VOICE TYPING)
            // ==============================================================
            HWND hV_Title = CreateWindowW(L"STATIC", L"ভয়েস টাইপিং (Voice Typing)", WS_CHILD | SS_LEFT, 250, 30, 520, 28, hWnd, NULL, NULL, NULL);
            SendMessage(hV_Title, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

            HWND hV_Desc = CreateWindowW(L"STATIC",
                L"Likhi-তে সরাসরি ভয়েস টাইপিং সমর্থিত। মুখে কথা বলুন, উইন্ডোজ নিজে থেকেই বাংলা বা ইংরেজিতে নির্ভুল টাইপ করে দেবে।\n\n"
                L"• ভয়েস টাইপিং শর্টকাট: Win + H (উইন্ডোজ কী চেপে ধরে H চাপুন)\n"
                L"• যেকোনো টেক্সট ফিল্ডে (Notepad, Word, Browser, WhatsApp) কার্সর রেখে Win + H চাপলেই ভয়েস টাইপিং শুরু হবে।\n"
                L"• উইন্ডোজ স্পিচ রিকগনিশনের মাধ্যমে সম্পূর্ণ নিখুঁতভাবে বাংলা উচ্চারণ গ্রহণ করা হয়।",
                WS_CHILD | SS_LEFT, 250, 70, 520, 140, hWnd, NULL, NULL, NULL);
            SendMessage(hV_Desc, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            HWND hChkVoi = CreateWindowW(L"BUTTON", L"ভয়েস টাইপিং নির্দেশক চালু রাখুন (Enable Voice Typing Shortcut Hint)", WS_CHILD | BS_AUTOCHECKBOX, 250, 220, 520, 24, hWnd, (HMENU)IDC_CHK_ENABLE_VOICE, NULL, NULL);
            SendMessage(hChkVoi, WM_SETFONT, (WPARAM)hFontBody, TRUE);
            SendMessage(hChkVoi, BM_SETCHECK, g_settings.enable_voice ? BST_CHECKED : BST_UNCHECKED, 0);

            HWND hBtnVoiceTest = CreateWindowW(L"BUTTON", L"🎙️ এখনই ভয়েস টাইপিং পরীক্ষা করুন (Win + H)", WS_CHILD | BS_PUSHBUTTON, 250, 260, 420, 38, hWnd, (HMENU)IDC_BTN_TEST_VOICE, NULL, NULL);
            SendMessage(hBtnVoiceTest, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            HWND hV_Tip = CreateWindowW(L"STATIC", L"💡 টিপস: ভয়েস টাইপিং শুরুর পূর্বে মাইক্রোফোন উইন্ডোজে সংযুক্ত আছে কিনা নিশ্চিত করুন।", WS_CHILD | SS_LEFT, 250, 310, 520, 35, hWnd, NULL, NULL, NULL);
            SendMessage(hV_Tip, WM_SETFONT, (WPARAM)hFontSub, TRUE);

            g_section_controls[SEC_VOICE].push_back(hV_Title);
            g_section_controls[SEC_VOICE].push_back(hV_Desc);
            g_section_controls[SEC_VOICE].push_back(hChkVoi);
            g_section_controls[SEC_VOICE].push_back(hBtnVoiceTest);
            g_section_controls[SEC_VOICE].push_back(hV_Tip);

            // ==============================================================
            // SECTION 6: APPEARANCE
            // ==============================================================
            HWND hA_Title = CreateWindowW(L"STATIC", L"থিম ও রূপ (Appearance)", WS_CHILD | SS_LEFT, 250, 30, 520, 28, hWnd, NULL, NULL, NULL);
            SendMessage(hA_Title, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

            HWND hA_ThemeLbl = CreateWindowW(L"STATIC", L"অ্যাপ্লিকেশন থিম নির্বাচন করুন:", WS_CHILD | SS_LEFT, 250, 80, 520, 24, hWnd, NULL, NULL, NULL);
            SendMessage(hA_ThemeLbl, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            HWND hRT1 = CreateWindowW(L"BUTTON", L"উইন্ডোজ সিস্টেম ডিফল্ট (System Default)", WS_CHILD | BS_AUTORADIOBUTTON | WS_GROUP, 250, 115, 320, 24, hWnd, (HMENU)IDC_RADIO_THEME_SYS, NULL, NULL);
            HWND hRT2 = CreateWindowW(L"BUTTON", L"হালকা থিম (Light Mode)", WS_CHILD | BS_AUTORADIOBUTTON, 250, 150, 320, 24, hWnd, (HMENU)IDC_RADIO_THEME_LIGHT, NULL, NULL);
            HWND hRT3 = CreateWindowW(L"BUTTON", L"ডার্ক থিম (Dark Mode)", WS_CHILD | BS_AUTORADIOBUTTON, 250, 185, 320, 24, hWnd, (HMENU)IDC_RADIO_THEME_DARK, NULL, NULL);

            SendMessage(hRT1, WM_SETFONT, (WPARAM)hFontBody, TRUE);
            SendMessage(hRT2, WM_SETFONT, (WPARAM)hFontBody, TRUE);
            SendMessage(hRT3, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            if (g_settings.theme == 1) SendMessage(hRT2, BM_SETCHECK, BST_CHECKED, 0);
            else if (g_settings.theme == 2) SendMessage(hRT3, BM_SETCHECK, BST_CHECKED, 0);
            else SendMessage(hRT1, BM_SETCHECK, BST_CHECKED, 0);

            g_section_controls[SEC_APPEARANCE].push_back(hA_Title);
            g_section_controls[SEC_APPEARANCE].push_back(hA_ThemeLbl);
            g_section_controls[SEC_APPEARANCE].push_back(hRT1);
            g_section_controls[SEC_APPEARANCE].push_back(hRT2);
            g_section_controls[SEC_APPEARANCE].push_back(hRT3);

            // ==============================================================
            // SECTION 7: ADVANCED
            // ==============================================================
            HWND hAdv_Title = CreateWindowW(L"STATIC", L"উন্নত সেটিংস ও ডায়াগনস্টিকস", WS_CHILD | SS_LEFT, 250, 30, 520, 28, hWnd, NULL, NULL, NULL);
            SendMessage(hAdv_Title, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

            HWND hAdv_Desc = CreateWindowW(L"STATIC", L"• মেমোরি পদচিহ্ন: ~১৩.১ MB (অত্যন্ত হালকা)\n• টাইপিং লেটেন্সি: ~৪০ মাইক্রোসেকেন্ড / কীস্ট্রোক\n• ডিকশনারি এন্ট্রি: ৫২,৪১২টি ভ্যালিডেটেড শব্দ\n• আর্কিটেকচার: নেটিভ C++20 + Windows TSF", WS_CHILD | SS_LEFT, 250, 75, 520, 120, hWnd, NULL, NULL, NULL);
            SendMessage(hAdv_Desc, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            HWND hBtnReset = CreateWindowW(L"BUTTON", L"ফ্যাক্টরি রিসেট করুন (Reset to Defaults)", WS_CHILD | BS_PUSHBUTTON, 250, 210, 400, 38, hWnd, (HMENU)IDC_BTN_RESET_DEF, NULL, NULL);
            SendMessage(hBtnReset, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            g_section_controls[SEC_ADVANCED].push_back(hAdv_Title);
            g_section_controls[SEC_ADVANCED].push_back(hAdv_Desc);
            g_section_controls[SEC_ADVANCED].push_back(hBtnReset);

            // ==============================================================
            // SECTION 8: ABOUT (WITH LOGO & AH CREATIONS CREDIT)
            // ==============================================================
            HWND hAb_Title = CreateWindowW(L"STATIC", L"Likhi (লিখি) — সংস্করণ ১.০.০", WS_CHILD | SS_LEFT, 370, 30, 400, 28, hWnd, NULL, NULL, NULL);
            SendMessage(hAb_Title, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

            HWND hAb_Tag = CreateWindowW(L"STATIC", L"“Fast • Smart • Natural — Bangla Typing for Windows”", WS_CHILD | SS_LEFT, 370, 65, 400, 24, hWnd, NULL, NULL, NULL);
            SendMessage(hAb_Tag, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            HWND hAb_Credit = CreateWindowW(L"STATIC", L"✨ Developed by AH Creations", WS_CHILD | SS_LEFT, 370, 95, 400, 26, hWnd, NULL, NULL, NULL);
            SendMessage(hAb_Credit, WM_SETFONT, (WPARAM)hFontCredit, TRUE);

            HWND hAb_Desc = CreateWindowW(L"STATIC", L"• ১০০% অফলাইন ও ব্যক্তিগত (০ ট্র্যাকিং / ক্লাউডমুক্ত)\n• সম্পূর্ণ স্বাধীন ও আধুনিক C++20 ল্যাঙ্গুয়েজ ইঞ্জিন\n• লাইসেন্স: MIT License\n• ক্রিয়েটর ও ডেভেলপার: AH Creations", WS_CHILD | SS_LEFT, 250, 155, 520, 120, hWnd, NULL, NULL, NULL);
            SendMessage(hAb_Desc, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            HWND hBtnUpd = CreateWindowW(L"BUTTON", L"আপডেট পরীক্ষা করুন (Check Updates)", WS_CHILD | BS_PUSHBUTTON, 250, 285, 360, 38, hWnd, (HMENU)IDC_BTN_CHECK_UPDATE, NULL, NULL);
            SendMessage(hBtnUpd, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            g_section_controls[SEC_ABOUT].push_back(hAb_Title);
            g_section_controls[SEC_ABOUT].push_back(hAb_Tag);
            g_section_controls[SEC_ABOUT].push_back(hAb_Credit);
            g_section_controls[SEC_ABOUT].push_back(hAb_Desc);
            g_section_controls[SEC_ABOUT].push_back(hBtnUpd);

            // ==============================================================
            // BOTTOM BAR CONTROLS
            // ==============================================================
            hLblStatus = CreateWindowW(L"STATIC", L"Likhi প্রস্তুত।", WS_VISIBLE | WS_CHILD | SS_LEFT, 25, 485, 470, 28, hWnd, (HMENU)IDC_LBL_STATUS, NULL, NULL);
            SendMessage(hLblStatus, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            HWND hBtnSave = CreateWindowW(L"BUTTON", L"সংরক্ষণ করুন (Save)", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 505, 480, 185, 38, hWnd, (HMENU)IDC_BTN_SAVE, NULL, NULL);
            HWND hBtnClose = CreateWindowW(L"BUTTON", L"বন্ধ করুন", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 700, 480, 100, 38, hWnd, (HMENU)IDC_BTN_CLOSE, NULL, NULL);

            SendMessage(hBtnSave, WM_SETFONT, (WPARAM)hFontBold, TRUE);
            SendMessage(hBtnClose, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            SwitchSection(hWnd, SEC_GENERAL);
            ApplyTheme(hWnd, ShouldUseDarkMode(g_settings.theme));
            break;
        }

        case WM_DRAWITEM: {
            LPDRAWITEMSTRUCT pDIS = (LPDRAWITEMSTRUCT)lParam;
            if (pDIS->CtlID >= IDC_NAV_BASE && pDIS->CtlID < IDC_NAV_BASE + SEC_COUNT) {
                int idx = pDIS->CtlID - IDC_NAV_BASE;
                bool is_selected = (g_active_section == idx);
                HDC hdc = pDIS->hDC;
                RECT rc = pDIS->rcItem;

                // Fill background
                if (is_selected) {
                    FillRect(hdc, &rc, hBrushActiveNav);
                    SetTextColor(hdc, RGB(255, 255, 255));
                } else if (pDIS->itemState & ODS_SELECTED) {
                    FillRect(hdc, &rc, hBrushHoverNav);
                    SetTextColor(hdc, g_current_is_dark ? RGB(147, 197, 253) : RGB(37, 99, 235));
                } else {
                    FillRect(hdc, &rc, hBrushSidebar);
                    SetTextColor(hdc, g_current_is_dark ? RGB(226, 232, 240) : RGB(51, 65, 85));
                }

                SetBkMode(hdc, TRANSPARENT);
                SelectObject(hdc, is_selected ? hFontBold : hFontBody);
                
                rc.left += 10;
                DrawTextW(hdc, g_navLabels[idx], -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                return TRUE;
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            // 1. Fill Sidebar Area (Left 235px)
            RECT rcSidebar = {0, 0, 235, 580};
            FillRect(hdc, &rcSidebar, hBrushSidebar);

            // 2. Draw Sidebar Top Header / Logo
            if (hAppIcon) {
                DrawIconEx(hdc, 18, 18, hAppIcon, 48, 48, 0, NULL, DI_NORMAL);
            }
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, g_current_is_dark ? RGB(147, 197, 253) : RGB(30, 58, 138));
            SelectObject(hdc, hFontTitle);
            TextOutW(hdc, 76, 18, L"Likhi (লিখি)", 12);

            SetTextColor(hdc, g_current_is_dark ? RGB(156, 163, 175) : RGB(100, 116, 139));
            SelectObject(hdc, hFontSub);
            TextOutW(hdc, 76, 46, L"বাংলা লিখুন, সহজেই।", 19);

            // 3. Draw Vertical Divider
            HPEN hPenDivider = CreatePen(PS_SOLID, 1, g_current_is_dark ? RGB(60, 60, 68) : RGB(226, 232, 240));
            SelectObject(hdc, hPenDivider);
            MoveToEx(hdc, 235, 0, NULL);
            LineTo(hdc, 235, 580);

            // 4. Draw Content Card Area
            RECT rcCard = {245, 15, 800, 465};
            FillRect(hdc, &rcCard, hBrushCard);
            FrameRect(hdc, &rcCard, g_current_is_dark ? hBrushSidebar : hBrushBg);

            // 5. Draw About Logo if in About Section
            if (g_active_section == SEC_ABOUT && hAppIcon) {
                DrawIconEx(hdc, 255, 25, hAppIcon, 96, 96, 0, NULL, DI_NORMAL);
            }

            DeleteObject(hPenDivider);
            EndPaint(hWnd, &ps);
            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);

            if (wmId >= IDC_NAV_BASE && wmId < IDC_NAV_BASE + SEC_COUNT) {
                SectionID target = static_cast<SectionID>(wmId - IDC_NAV_BASE);
                SwitchSection(hWnd, target);
                break;
            }

            if (wmId == IDC_RADIO_THEME_SYS) {
                g_settings.theme = 0;
                ApplyTheme(hWnd, ShouldUseDarkMode(0));
                SaveSettings();
                SetWindowTextW(hLblStatus, L"🎨 থিম: উইন্ডোজ সিস্টেম ডিফল্ট প্রয়োগ করা হয়েছে।");
                break;
            } else if (wmId == IDC_RADIO_THEME_LIGHT) {
                g_settings.theme = 1;
                ApplyTheme(hWnd, ShouldUseDarkMode(1));
                SaveSettings();
                SetWindowTextW(hLblStatus, L"🎨 থিম: হালকা থিম (Light Mode) প্রয়োগ করা হয়েছে।");
                break;
            } else if (wmId == IDC_RADIO_THEME_DARK) {
                g_settings.theme = 2;
                ApplyTheme(hWnd, ShouldUseDarkMode(2));
                SaveSettings();
                SetWindowTextW(hLblStatus, L"🎨 থিম: ডার্ক থিম (Dark Mode) প্রয়োগ করা হয়েছে।");
                break;
            }

            if (wmId == IDC_BTN_LAUNCH_VK) {
                wchar_t exePath[MAX_PATH];
                GetModuleFileNameW(NULL, exePath, MAX_PATH);
                wchar_t* lastSlash = wcsrchr(exePath, L'\\');
                if (lastSlash) {
                    *(lastSlash + 1) = L'\0';
                    std::wstring vkPath = std::wstring(exePath) + L"likhi_virtual_keyboard.exe";
                    HINSTANCE hRes = ShellExecuteW(NULL, L"open", vkPath.c_str(), NULL, exePath, SW_SHOWNORMAL);
                    if ((intptr_t)hRes <= 32) {
                        ShellExecuteW(NULL, L"open", L"likhi_virtual_keyboard.exe", NULL, NULL, SW_SHOWNORMAL);
                    }
                } else {
                    ShellExecuteW(NULL, L"open", L"likhi_virtual_keyboard.exe", NULL, NULL, SW_SHOWNORMAL);
                }
                SetWindowTextW(hLblStatus, L"⌨️ Likhi Virtual Keyboard চালু করা হয়েছে।");
                break;
            } else if (wmId == IDC_BTN_TEST_VOICE) {
                // Simulate Win + H
                INPUT inputs[4] = {};
                inputs[0].type = INPUT_KEYBOARD;
                inputs[0].ki.wVk = VK_LWIN;
                inputs[1].type = INPUT_KEYBOARD;
                inputs[1].ki.wVk = 'H';
                inputs[2].type = INPUT_KEYBOARD;
                inputs[2].ki.wVk = 'H';
                inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
                inputs[3].type = INPUT_KEYBOARD;
                inputs[3].ki.wVk = VK_LWIN;
                inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
                SendInput(4, inputs, sizeof(INPUT));

                SetWindowTextW(hLblStatus, L"🎙️ উইন্ডোজ ভয়েস টাইপিং (Win + H) সক্রিয় করা হয়েছে!");
                break;
            }

            if (wmId == IDC_BTN_SAVE) {
                HWND hChkLikhi = GetDlgItem(hWnd, IDC_CHK_ENABLE_LIKHI);
                HWND hChkStart = GetDlgItem(hWnd, IDC_CHK_STARTUP);
                HWND hChkBng = GetDlgItem(hWnd, IDC_CHK_BANGLA_TYPING);
                HWND hChkBngl = GetDlgItem(hWnd, IDC_CHK_BANGLISH);
                HWND hChkE2B = GetDlgItem(hWnd, IDC_CHK_ENG_TO_BAN);
                HWND hChkFuz = GetDlgItem(hWnd, IDC_CHK_FUZZY);
                HWND hChkSug = GetDlgItem(hWnd, IDC_CHK_SUGGESTIONS);
                HWND hChkPred = GetDlgItem(hWnd, IDC_CHK_PREDICTION);
                HWND hChkEngCand = GetDlgItem(hWnd, IDC_CHK_ENG_CANDIDATE);
                HWND hChkAuto = GetDlgItem(hWnd, IDC_CHK_AUTOCORRECT);
                HWND hChkVoice = GetDlgItem(hWnd, IDC_CHK_ENABLE_VOICE);

                g_settings.enable_likhi = (SendMessage(hChkLikhi, BM_GETCHECK, 0, 0) == BST_CHECKED);
                g_settings.launch_startup = (SendMessage(hChkStart, BM_GETCHECK, 0, 0) == BST_CHECKED);
                g_settings.bangla_typing = (SendMessage(hChkBng, BM_GETCHECK, 0, 0) == BST_CHECKED);
                g_settings.banglish_recog = (SendMessage(hChkBngl, BM_GETCHECK, 0, 0) == BST_CHECKED);
                g_settings.eng_to_bangla = (SendMessage(hChkE2B, BM_GETCHECK, 0, 0) == BST_CHECKED);
                g_settings.fuzzy_spelling = (SendMessage(hChkFuz, BM_GETCHECK, 0, 0) == BST_CHECKED);
                g_settings.show_suggestions = (SendMessage(hChkSug, BM_GETCHECK, 0, 0) == BST_CHECKED);
                g_settings.word_prediction = (SendMessage(hChkPred, BM_GETCHECK, 0, 0) == BST_CHECKED);
                g_settings.show_eng_candidate = (SendMessage(hChkEngCand, BM_GETCHECK, 0, 0) == BST_CHECKED);
                g_settings.auto_correct = (SendMessage(hChkAuto, BM_GETCHECK, 0, 0) == BST_CHECKED);
                if (hChkVoice) g_settings.enable_voice = (SendMessage(hChkVoice, BM_GETCHECK, 0, 0) == BST_CHECKED);

                if (SendMessage(GetDlgItem(hWnd, IDC_RADIO_CAND3), BM_GETCHECK, 0, 0) == BST_CHECKED) g_settings.max_candidates = 3;
                else if (SendMessage(GetDlgItem(hWnd, IDC_RADIO_CAND4), BM_GETCHECK, 0, 0) == BST_CHECKED) g_settings.max_candidates = 4;
                else g_settings.max_candidates = 5;

                if (SendMessage(GetDlgItem(hWnd, IDC_RADIO_THEME_LIGHT), BM_GETCHECK, 0, 0) == BST_CHECKED) g_settings.theme = 1;
                else if (SendMessage(GetDlgItem(hWnd, IDC_RADIO_THEME_DARK), BM_GETCHECK, 0, 0) == BST_CHECKED) g_settings.theme = 2;
                else g_settings.theme = 0;

                ApplyTheme(hWnd, ShouldUseDarkMode(g_settings.theme));
                SaveSettings();
                SetStartupRegistry(g_settings.launch_startup);
                SetWindowTextW(hLblStatus, L"✅ সেটিংস সফলভাবে সংরক্ষিত হয়েছে!");
            } else if (wmId == IDC_BTN_CLOSE) {
                PostQuitMessage(0);
            } else if (LOWORD(wParam) == IDC_LIST_DICT && HIWORD(wParam) == LBN_SELCHANGE) {
                int sel = (int)SendMessage(hListDict, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR) {
                    int entry_idx = (int)SendMessage(hListDict, LB_GETITEMDATA, sel, 0);
                    if (entry_idx >= 0 && entry_idx < (int)g_dict_entries.size()) {
                        SetWindowTextW(hEditRoman, g_dict_entries[entry_idx].roman_key.c_str());
                        SetWindowTextW(hEditBangla, g_dict_entries[entry_idx].bengali_word.c_str());
                    }
                }
            } else if (LOWORD(wParam) == IDC_EDIT_SEARCH_DICT && HIWORD(wParam) == EN_CHANGE) {
                wchar_t filter[128] = {0};
                GetWindowTextW(hEditSearchDict, filter, 128);
                RefreshDictionaryList(filter);
            } else if (wmId == IDC_BTN_CLEAR_FIELDS) {
                SetWindowTextW(hEditRoman, L"");
                SetWindowTextW(hEditBangla, L"");
                SendMessage(hListDict, LB_SETCURSEL, (WPARAM)-1, 0);
                SetFocus(hEditRoman);
                SetWindowTextW(hLblStatus, L"নতুন শব্দ যোগ করার জন্য প্রস্তুত।");
            } else if (wmId == IDC_BTN_ADD_WORD) {
                wchar_t r_buf[128] = {0}, b_buf[128] = {0};
                GetWindowTextW(hEditRoman, r_buf, 128);
                GetWindowTextW(hEditBangla, b_buf, 128);
                std::wstring r_str = ToLowerW(TrimW(r_buf));
                std::wstring b_str = TrimW(b_buf);

                if (r_str.empty() || b_str.empty()) {
                    SetWindowTextW(hLblStatus, L"⚠️ অনুগ্রহ করে ইংরেজি কী এবং বাংলা শব্দ দুটিই পূরণ করুন।");
                    break;
                }

                // Check if user has an entry selected in list
                int sel = (int)SendMessage(hListDict, LB_GETCURSEL, 0, 0);
                int selected_idx = -1;
                if (sel != LB_ERR) {
                    selected_idx = (int)SendMessage(hListDict, LB_GETITEMDATA, sel, 0);
                }

                // Check exact duplicate
                bool duplicate_found = false;
                for (size_t i = 0; i < g_dict_entries.size(); i++) {
                    if ((int)i != selected_idx && g_dict_entries[i].roman_key == r_str && g_dict_entries[i].bengali_word == b_str) {
                        duplicate_found = true;
                        break;
                    }
                }
                if (duplicate_found) {
                    std::wstring msg = L"⚠️ '" + r_str + L"' → '" + b_str + L"' ইতিমধ্যে যুক্ত আছে!";
                    SetWindowTextW(hLblStatus, msg.c_str());
                    break;
                }

                uint64_t now = GetCurrentUnixTimestamp();
                if (selected_idx >= 0 && selected_idx < (int)g_dict_entries.size()) {
                    // Update existing selected entry
                    g_dict_entries[selected_idx].roman_key = r_str;
                    g_dict_entries[selected_idx].bengali_word = b_str;
                    g_dict_entries[selected_idx].updated_at = now;
                    SaveDictionaryEntries();
                    wchar_t filter[128] = {0};
                    if (hEditSearchDict) GetWindowTextW(hEditSearchDict, filter, 128);
                    RefreshDictionaryList(filter);
                    std::wstring msg = L"✅ '" + r_str + L"' → '" + b_str + L"' আপডেট সম্পন্ন হয়েছে!";
                    SetWindowTextW(hLblStatus, msg.c_str());
                } else {
                    // Add new entry
                    DictEntry entry;
                    entry.id = std::to_wstring(g_dict_entries.size() + 1);
                    entry.roman_key = r_str;
                    entry.bengali_word = b_str;
                    entry.frequency = 1;
                    entry.created_at = now;
                    entry.updated_at = now;
                    g_dict_entries.push_back(entry);
                    SaveDictionaryEntries();
                    wchar_t filter[128] = {0};
                    if (hEditSearchDict) GetWindowTextW(hEditSearchDict, filter, 128);
                    RefreshDictionaryList(filter);
                    std::wstring msg = L"✅ নতুন শব্দ '" + r_str + L"' → '" + b_str + L"' সফলভাবে যুক্ত হয়েছে!";
                    SetWindowTextW(hLblStatus, msg.c_str());
                    SetWindowTextW(hEditRoman, L"");
                    SetWindowTextW(hEditBangla, L"");
                }
            } else if (wmId == IDC_BTN_DEL_WORD) {
                int sel = (int)SendMessage(hListDict, LB_GETCURSEL, 0, 0);
                int target_idx = -1;
                if (sel != LB_ERR) {
                    target_idx = (int)SendMessage(hListDict, LB_GETITEMDATA, sel, 0);
                } else {
                    wchar_t r_buf[128] = {0}, b_buf[128] = {0};
                    GetWindowTextW(hEditRoman, r_buf, 128);
                    GetWindowTextW(hEditBangla, b_buf, 128);
                    std::wstring r_str = ToLowerW(TrimW(r_buf));
                    std::wstring b_str = TrimW(b_buf);
                    if (!r_str.empty() && !b_str.empty()) {
                        for (size_t i = 0; i < g_dict_entries.size(); i++) {
                            if (g_dict_entries[i].roman_key == r_str && g_dict_entries[i].bengali_word == b_str) {
                                target_idx = (int)i;
                                break;
                            }
                        }
                    }
                }

                if (target_idx >= 0 && target_idx < (int)g_dict_entries.size()) {
                    std::wstring r_str = g_dict_entries[target_idx].roman_key;
                    std::wstring b_str = g_dict_entries[target_idx].bengali_word;
                    g_dict_entries.erase(g_dict_entries.begin() + target_idx);
                    SaveDictionaryEntries();
                    wchar_t filter[128] = {0};
                    if (hEditSearchDict) GetWindowTextW(hEditSearchDict, filter, 128);
                    RefreshDictionaryList(filter);
                    SetWindowTextW(hEditRoman, L"");
                    SetWindowTextW(hEditBangla, L"");
                    SendMessage(hListDict, LB_SETCURSEL, (WPARAM)-1, 0);
                    std::wstring msg = L"🗑️ '" + r_str + L"' → '" + b_str + L"' মুছে ফেলা হয়েছে।";
                    SetWindowTextW(hLblStatus, msg.c_str());
                } else {
                    SetWindowTextW(hLblStatus, L"⚠️ মুছে ফেলার জন্য তালিকা থেকে একটি শব্দ নির্বাচন করুন।");
                }
            } else if (wmId == IDC_BTN_IMPORT) {
                wchar_t szFile[MAX_PATH] = {0};
                OPENFILENAMEW ofn;
                memset(&ofn, 0, sizeof(ofn));
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hWnd;
                ofn.lpstrFilter = L"Text Files (*.txt;*.tsv)\0*.txt;*.tsv\0All Files (*.*)\0*.*\0";
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

                if (GetOpenFileNameW(&ofn)) {
                    std::ifstream in(WideToUtf8(szFile).c_str());
                    if (in.is_open()) {
                        std::string line;
                        int imported_count = 0;
                        uint64_t now = GetCurrentUnixTimestamp();
                        while (std::getline(in, line)) {
                            if (line.empty()) continue;
                            if (!line.empty() && line.back() == '\r') line.pop_back();
                            if (line.empty()) continue;

                            std::stringstream ss(line);
                            std::string token;
                            std::vector<std::string> tokens;
                            while (std::getline(ss, token, '\t')) {
                                tokens.push_back(token);
                            }
                            if (tokens.size() < 2) continue;

                            std::wstring r_key = ToLowerW(TrimW(Utf8ToWide(tokens[0])));
                            std::wstring b_word = TrimW(Utf8ToWide(tokens[1]));
                            if (r_key.empty() || b_word.empty()) continue;

                            bool exists = false;
                            for (const auto& e : g_dict_entries) {
                                if (e.roman_key == r_key && e.bengali_word == b_word) {
                                    exists = true;
                                    break;
                                }
                            }
                            if (!exists) {
                                DictEntry entry;
                                entry.id = std::to_wstring(g_dict_entries.size() + 1);
                                entry.roman_key = r_key;
                                entry.bengali_word = b_word;
                                entry.frequency = (tokens.size() >= 3) ? (uint32_t)std::strtoul(tokens[2].c_str(), nullptr, 10) : 1;
                                if (entry.frequency == 0) entry.frequency = 1;
                                entry.created_at = (tokens.size() >= 4) ? std::strtoull(tokens[3].c_str(), nullptr, 10) : now;
                                entry.updated_at = (tokens.size() >= 5) ? std::strtoull(tokens[4].c_str(), nullptr, 10) : now;
                                g_dict_entries.push_back(entry);
                                imported_count++;
                            }
                        }
                        SaveDictionaryEntries();
                        wchar_t filter[128] = {0};
                        if (hEditSearchDict) GetWindowTextW(hEditSearchDict, filter, 128);
                        RefreshDictionaryList(filter);
                        std::wstring msg = L"✅ সফলভাবে " + std::to_wstring(imported_count) + L" টি শব্দ ইমপোর্ট করা হয়েছে!";
                        SetWindowTextW(hLblStatus, msg.c_str());
                    }
                }
            } else if (wmId == IDC_BTN_EXPORT) {
                wchar_t szFile[MAX_PATH] = L"likhi_personal_dict_export.txt";
                OPENFILENAMEW ofn;
                memset(&ofn, 0, sizeof(ofn));
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hWnd;
                ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

                if (GetSaveFileNameW(&ofn)) {
                    std::ofstream out(WideToUtf8(szFile).c_str(), std::ios::trunc);
                    if (out.is_open()) {
                        for (const auto& e : g_dict_entries) {
                            out << WideToUtf8(e.roman_key) << "\t"
                                << WideToUtf8(e.bengali_word) << "\t"
                                << e.frequency << "\t"
                                << e.created_at << "\t"
                                << e.updated_at << "\t"
                                << WideToUtf8(e.id) << "\n";
                        }
                        out.flush();
                        SetWindowTextW(hLblStatus, L"✅ ব্যক্তিগত শব্দভাণ্ডার এক্সপোর্ট সম্পন্ন হয়েছে!");
                    }
                }
            } else if (wmId == IDC_BTN_RESET_DEF) {
                g_settings = AppSettings();
                SaveSettings();
                SetStartupRegistry(false);
                ApplyTheme(hWnd, ShouldUseDarkMode(g_settings.theme));
                SendMessage(GetDlgItem(hWnd, IDC_CHK_ENABLE_LIKHI), BM_SETCHECK, g_settings.enable_likhi ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hWnd, IDC_CHK_STARTUP), BM_SETCHECK, g_settings.launch_startup ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hWnd, IDC_CHK_BANGLA_TYPING), BM_SETCHECK, g_settings.bangla_typing ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hWnd, IDC_CHK_BANGLISH), BM_SETCHECK, g_settings.banglish_recog ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hWnd, IDC_CHK_ENG_TO_BAN), BM_SETCHECK, g_settings.eng_to_bangla ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hWnd, IDC_CHK_FUZZY), BM_SETCHECK, g_settings.fuzzy_spelling ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hWnd, IDC_CHK_SUGGESTIONS), BM_SETCHECK, g_settings.show_suggestions ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hWnd, IDC_CHK_PREDICTION), BM_SETCHECK, g_settings.word_prediction ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hWnd, IDC_CHK_ENG_CANDIDATE), BM_SETCHECK, g_settings.show_eng_candidate ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hWnd, IDC_CHK_AUTOCORRECT), BM_SETCHECK, g_settings.auto_correct ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hWnd, IDC_CHK_ENABLE_VOICE), BM_SETCHECK, g_settings.enable_voice ? BST_CHECKED : BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hWnd, IDC_RADIO_THEME_SYS), BM_SETCHECK, BST_CHECKED, 0);
                SendMessage(GetDlgItem(hWnd, IDC_RADIO_THEME_LIGHT), BM_SETCHECK, BST_UNCHECKED, 0);
                SendMessage(GetDlgItem(hWnd, IDC_RADIO_THEME_DARK), BM_SETCHECK, BST_UNCHECKED, 0);
                SetWindowTextW(hLblStatus, L"🔄 ফ্যাক্টরি ডিফল্টে রিসেট করা হয়েছে।");
            } else if (wmId == IDC_BTN_CHECK_UPDATE) {
                SetWindowTextW(hLblStatus, L"✨ আপনি Likhi-এর সর্বশেষ সংস্করণ (v1.0.0) ব্যবহার করছেন।");
            }
            break;
        }

        case WM_SETTINGCHANGE: {
            if (g_settings.theme == 0) {
                ApplyTheme(hWnd, ShouldUseDarkMode(0));
            }
            break;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            HWND hCtl = (HWND)lParam;
            SetBkMode(hdc, TRANSPARENT);
            
            if (g_current_is_dark) {
                if (hCtl == hLblStatus) {
                    SetTextColor(hdc, RGB(200, 210, 225));
                    return (LRESULT)hBrushBg;
                }
                if (GetDlgCtrlID(hCtl) == 0 && g_active_section == SEC_ABOUT) {
                    SetTextColor(hdc, RGB(56, 189, 248)); // Bright Sky Blue
                } else {
                    SetTextColor(hdc, RGB(241, 245, 249));
                }
                return (LRESULT)hBrushCard;
            } else {
                if (hCtl == hLblStatus) {
                    SetTextColor(hdc, RGB(71, 85, 105));
                    return (LRESULT)hBrushBg;
                }
                if (GetDlgCtrlID(hCtl) == 0 && g_active_section == SEC_ABOUT) {
                    SetTextColor(hdc, RGB(2, 132, 199)); // Bright Sky Blue
                } else {
                    SetTextColor(hdc, RGB(30, 41, 59));
                }
                return (LRESULT)hBrushCard;
            }
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            if (g_current_is_dark) {
                SetTextColor(hdc, RGB(241, 245, 249));
                SetBkColor(hdc, RGB(32, 32, 36));
                return (LRESULT)hBrushEdit;
            } else {
                SetTextColor(hdc, RGB(15, 23, 42));
                SetBkColor(hdc, RGB(255, 255, 255));
                return (LRESULT)hBrushEdit;
            }
        }

        case WM_CTLCOLORLISTBOX: {
            HDC hdc = (HDC)wParam;
            if (g_current_is_dark) {
                SetTextColor(hdc, RGB(241, 245, 249));
                SetBkColor(hdc, RGB(32, 32, 36));
                return (LRESULT)hBrushList;
            } else {
                SetTextColor(hdc, RGB(15, 23, 42));
                SetBkColor(hdc, RGB(255, 255, 255));
                return (LRESULT)hBrushList;
            }
        }

        case WM_CTLCOLORBTN: {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
            if (g_current_is_dark) {
                SetTextColor(hdc, RGB(241, 245, 249));
                return (LRESULT)hBrushCard;
            } else {
                SetTextColor(hdc, RGB(30, 41, 59));
                return (LRESULT)hBrushCard;
            }
        }

        case WM_DESTROY:
            if (hFontTitle) DeleteObject(hFontTitle);
            if (hFontHeader) DeleteObject(hFontHeader);
            if (hFontBody) DeleteObject(hFontBody);
            if (hFontBold) DeleteObject(hFontBold);
            if (hFontSub) DeleteObject(hFontSub);
            if (hFontCredit) DeleteObject(hFontCredit);
            if (hBrushBg) DeleteObject(hBrushBg);
            if (hBrushSidebar) DeleteObject(hBrushSidebar);
            if (hBrushCard) DeleteObject(hBrushCard);
            if (hBrushActiveNav) DeleteObject(hBrushActiveNav);
            if (hBrushHoverNav) DeleteObject(hBrushHoverNav);
            if (hBrushAccent) DeleteObject(hBrushAccent);
            if (hBrushList) DeleteObject(hBrushList);
            if (hBrushEdit) DeleteObject(hBrushEdit);
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    INITCOMMONCONTROLSEX icex;
    memset(&icex, 0, sizeof(INITCOMMONCONTROLSEX));
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);

    HICON hIconBig = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    HICON hIconSm = (HICON)LoadImageW(hInstance, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);

    if (!hIconBig) {
        hIconBig = (HICON)LoadImageW(NULL, L"assets/icon.ico", IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_LOADFROMFILE);
    }
    if (!hIconSm) {
        hIconSm = (HICON)LoadImageW(NULL, L"assets/icon.ico", IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE);
    }

    WNDCLASSEXW wcex;
    memset(&wcex, 0, sizeof(WNDCLASSEXW));
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = hIconBig;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = CreateSolidBrush(RGB(248, 250, 252));
    wcex.lpszClassName = L"LikhiFluentSettingsClass";
    wcex.hIconSm = hIconSm;

    RegisterClassExW(&wcex);

    HWND hWnd = CreateWindowW(
        L"LikhiFluentSettingsClass",
        L"Likhi (লিখি) — সেটিংস ও ড্যাশবোর্ড",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 830, 570,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd) return FALSE;

    if (hIconBig) {
        SendMessageW(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
    }
    if (hIconSm) {
        SendMessageW(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSm);
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
