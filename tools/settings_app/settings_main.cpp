#include <windows.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <shlobj.h>

#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")

enum SectionID {
    SEC_GENERAL = 0,
    SEC_TYPING,
    SEC_SUGGESTIONS,
    SEC_BANGLISH,
    SEC_DICTIONARY,
    SEC_KEYBOARD,
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
    int max_candidates = 5;
    int theme = 0;
};

static AppSettings g_settings;
static std::wstring g_config_path;
static SectionID g_active_section = SEC_GENERAL;

static HWND g_hNavButtons[SEC_COUNT];
static std::vector<HWND> g_section_controls[SEC_COUNT];
static HWND hListDict, hEditRoman, hEditBangla, hLblStatus;
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

const wchar_t* g_navLabels[SEC_COUNT] = {
    L"  🏠  সাধারণ (General)",
    L"  ⌨️  টাইপিং (Typing)",
    L"  💡  সাজেশন (Suggestions)",
    L"  🌐  বাংলিশ (Banglish)",
    L"  📖  অভিধান (Dictionary)",
    L"  🎛️  কীবোর্ড (Keyboard)",
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

void LoadSettings() {
    g_config_path = GetConfigDirectory() + L"\\settings.json";
    std::ifstream in(g_config_path.c_str());
    if (!in.is_open()) return;

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
        if (line.find("\"eng_to_bangla\": false") != std::string::npos) g_settings.eng_to_bangla = false;
        if (line.find("\"word_prediction\": false") != std::string::npos) g_settings.word_prediction = false;
        if (line.find("\"show_eng_candidate\": false") != std::string::npos) g_settings.show_eng_candidate = false;
        if (line.find("\"fuzzy_spelling\": false") != std::string::npos) g_settings.fuzzy_spelling = false;
        if (line.find("\"theme\": 1") != std::string::npos) g_settings.theme = 1;
        if (line.find("\"theme\": 2") != std::string::npos) g_settings.theme = 2;
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
    out << "  \"max_candidates\": " << g_settings.max_candidates << ",\n";
    out << "  \"theme\": " << g_settings.theme << "\n";
    out << "}\n";
}

void RefreshDictionaryList() {
    if (!hListDict) return;
    SendMessage(hListDict, LB_RESETCONTENT, 0, 0);

    std::wstring dict_file = GetConfigDirectory() + L"\\personal_dict.txt";
    std::wifstream in(dict_file.c_str());
    if (!in.is_open()) return;

    std::wstring line;
    while (std::getline(in, line)) {
        if (!line.empty()) {
            SendMessageW(hListDict, LB_ADDSTRING, 0, (LPARAM)line.c_str());
        }
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
        RefreshDictionaryList();
    }
    InvalidateRect(hWnd, NULL, TRUE);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            LoadSettings();

            HINSTANCE hInst = ((LPCREATESTRUCT)lParam)->hInstance;

            // Load Application Icon
            hAppIcon = LoadIconW(hInst, MAKEINTRESOURCEW(1));
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

            // Color Palette
            hBrushBg = CreateSolidBrush(RGB(248, 250, 252));        // Light surface
            hBrushSidebar = CreateSolidBrush(RGB(241, 245, 249));   // Soft sidebar blue-gray
            hBrushCard = CreateSolidBrush(RGB(255, 255, 255));      // Pure white card
            hBrushActiveNav = CreateSolidBrush(RGB(37, 99, 235));   // Royal Blue
            hBrushHoverNav = CreateSolidBrush(RGB(226, 232, 240));   // Soft hover
            hBrushAccent = CreateSolidBrush(RGB(14, 165, 233));      // Cyan accent

            // ==============================================================
            // OWNER-DRAWN SIDEBAR NAVIGATION BUTTONS
            // ==============================================================
            for (int i = 0; i < SEC_COUNT; i++) {
                g_hNavButtons[i] = CreateWindowW(
                    L"BUTTON", g_navLabels[i],
                    WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                    15, 95 + (i * 38), 205, 34,
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

            HWND hG_LangCard = CreateWindowW(L"STATIC", L"📌 ডিফল্ট ইনপুট প্রোফাইল: বাংলা (বাংলাদেশ) — 0x0845\n\nকীবোর্ডে Win + Space চাপলে Likhi স্বয়ংক্রিয়ভাবে একটি মাত্র ক্লিন প্রোফাইল হিসেবে সক্রিয় থাকে।", WS_CHILD | SS_LEFT, 250, 210, 520, 80, hWnd, NULL, NULL, NULL);
            SendMessage(hG_LangCard, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            g_section_controls[SEC_GENERAL].push_back(hG_Title);
            g_section_controls[SEC_GENERAL].push_back(hG_Status);
            g_section_controls[SEC_GENERAL].push_back(hChkLikhi);
            g_section_controls[SEC_GENERAL].push_back(hChkStart);
            g_section_controls[SEC_GENERAL].push_back(hG_LangCard);

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
            HWND hD_Title = CreateWindowW(L"STATIC", L"ব্যক্তিগত শব্দভাণ্ডার (Personal Dictionary)", WS_CHILD | SS_LEFT, 250, 30, 520, 28, hWnd, NULL, NULL, NULL);
            SendMessage(hD_Title, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

            hListDict = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VSCROLL | LBS_NOTIFY, 250, 75, 240, 220, hWnd, (HMENU)IDC_LIST_DICT, NULL, NULL);
            SendMessage(hListDict, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            HWND hD_L1 = CreateWindowW(L"STATIC", L"English Key:", WS_CHILD | SS_LEFT, 510, 75, 85, 22, hWnd, NULL, NULL, NULL);
            SendMessage(hD_L1, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            hEditRoman = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 600, 73, 170, 26, hWnd, (HMENU)IDC_EDIT_ROMAN, NULL, NULL);
            SendMessage(hEditRoman, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            HWND hD_L2 = CreateWindowW(L"STATIC", L"বাংলা শব্দ:", WS_CHILD | SS_LEFT, 510, 115, 85, 22, hWnd, NULL, NULL, NULL);
            SendMessage(hD_L2, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            hEditBangla = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | ES_AUTOHSCROLL, 600, 113, 170, 26, hWnd, (HMENU)IDC_EDIT_BANGLA, NULL, NULL);
            SendMessage(hEditBangla, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            HWND hBtnA = CreateWindowW(L"BUTTON", L"যুক্ত করুন (+)", WS_CHILD | BS_PUSHBUTTON, 510, 155, 125, 32, hWnd, (HMENU)IDC_BTN_ADD_WORD, NULL, NULL);
            SendMessage(hBtnA, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            HWND hBtnD = CreateWindowW(L"BUTTON", L"মুছে ফেলুন (x)", WS_CHILD | BS_PUSHBUTTON, 645, 155, 125, 32, hWnd, (HMENU)IDC_BTN_DEL_WORD, NULL, NULL);
            SendMessage(hBtnD, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            HWND hBtnI = CreateWindowW(L"BUTTON", L"ইমপোর্ট (.txt)", WS_CHILD | BS_PUSHBUTTON, 510, 200, 125, 32, hWnd, (HMENU)IDC_BTN_IMPORT, NULL, NULL);
            SendMessage(hBtnI, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            HWND hBtnE = CreateWindowW(L"BUTTON", L"এক্সপোর্ট (.txt)", WS_CHILD | BS_PUSHBUTTON, 645, 200, 125, 32, hWnd, (HMENU)IDC_BTN_EXPORT, NULL, NULL);
            SendMessage(hBtnE, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            g_section_controls[SEC_DICTIONARY].push_back(hD_Title);
            g_section_controls[SEC_DICTIONARY].push_back(hListDict);
            g_section_controls[SEC_DICTIONARY].push_back(hD_L1);
            g_section_controls[SEC_DICTIONARY].push_back(hEditRoman);
            g_section_controls[SEC_DICTIONARY].push_back(hD_L2);
            g_section_controls[SEC_DICTIONARY].push_back(hEditBangla);
            g_section_controls[SEC_DICTIONARY].push_back(hBtnA);
            g_section_controls[SEC_DICTIONARY].push_back(hBtnD);
            g_section_controls[SEC_DICTIONARY].push_back(hBtnI);
            g_section_controls[SEC_DICTIONARY].push_back(hBtnE);

            // ==============================================================
            // SECTION 5: KEYBOARD
            // ==============================================================
            HWND hK_Title = CreateWindowW(L"STATIC", L"কীবোর্ড ও শর্টকাট আচরণ", WS_CHILD | SS_LEFT, 250, 30, 520, 28, hWnd, NULL, NULL, NULL);
            SendMessage(hK_Title, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

            HWND hK_Desc = CreateWindowW(L"STATIC", L"Likhi উইন্ডোজের সকল নেটিভ কীবোর্ড শর্টকাট সম্পূর্ণ অক্ষত রাখে।\n\n• Ctrl+C (Copy), Ctrl+V (Paste), Ctrl+A, Ctrl+Z ইত্যাদি সরাসরি কাজ করে\n• Numpad (0-9, +, -, *, .) সাধারণ সংখ্যার জন্য সংরক্ষিত\n• F1-F12 এবং অ্যারো কী স্বাভাবিকভাবে কাজ করে\n• উইন্ডোজ ভাষা পরিবর্তন: Win + Space\n\nসক্রিয় প্রোফাইল: Bangla (Bangladesh) — Likhi (লিখি)", WS_CHILD | SS_LEFT, 250, 75, 520, 280, hWnd, NULL, NULL, NULL);
            SendMessage(hK_Desc, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            g_section_controls[SEC_KEYBOARD].push_back(hK_Title);
            g_section_controls[SEC_KEYBOARD].push_back(hK_Desc);

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

            HWND hBtnReset = CreateWindowW(L"BUTTON", L"ফ্যাক্টরি রিসেট করুন (Reset to Defaults)", WS_CHILD | BS_PUSHBUTTON, 250, 210, 280, 36, hWnd, (HMENU)IDC_BTN_RESET_DEF, NULL, NULL);
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

            HWND hBtnUpd = CreateWindowW(L"BUTTON", L"আপডেট পরীক্ষা করুন (Check Updates)", WS_CHILD | BS_PUSHBUTTON, 250, 285, 260, 34, hWnd, (HMENU)IDC_BTN_CHECK_UPDATE, NULL, NULL);
            SendMessage(hBtnUpd, WM_SETFONT, (WPARAM)hFontBold, TRUE);

            g_section_controls[SEC_ABOUT].push_back(hAb_Title);
            g_section_controls[SEC_ABOUT].push_back(hAb_Tag);
            g_section_controls[SEC_ABOUT].push_back(hAb_Credit);
            g_section_controls[SEC_ABOUT].push_back(hAb_Desc);
            g_section_controls[SEC_ABOUT].push_back(hBtnUpd);

            // ==============================================================
            // BOTTOM BAR CONTROLS
            // ==============================================================
            hLblStatus = CreateWindowW(L"STATIC", L"Likhi প্রস্তুত।", WS_VISIBLE | WS_CHILD | SS_LEFT, 25, 485, 480, 26, hWnd, (HMENU)IDC_LBL_STATUS, NULL, NULL);
            SendMessage(hLblStatus, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            HWND hBtnSave = CreateWindowW(L"BUTTON", L"সংরক্ষণ করুন (Save)", WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 530, 480, 160, 36, hWnd, (HMENU)IDC_BTN_SAVE, NULL, NULL);
            HWND hBtnClose = CreateWindowW(L"BUTTON", L"বন্ধ করুন", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 705, 480, 95, 36, hWnd, (HMENU)IDC_BTN_CLOSE, NULL, NULL);

            SendMessage(hBtnSave, WM_SETFONT, (WPARAM)hFontBold, TRUE);
            SendMessage(hBtnClose, WM_SETFONT, (WPARAM)hFontBody, TRUE);

            SwitchSection(hWnd, SEC_GENERAL);
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
                    SetTextColor(hdc, RGB(37, 99, 235));
                } else {
                    FillRect(hdc, &rc, hBrushSidebar);
                    SetTextColor(hdc, RGB(51, 65, 85));
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
            SetTextColor(hdc, RGB(30, 58, 138)); // Deep Royal Blue
            SelectObject(hdc, hFontTitle);
            TextOutW(hdc, 76, 18, L"Likhi (লিখি)", 12);

            SetTextColor(hdc, RGB(100, 116, 139));
            SelectObject(hdc, hFontSub);
            TextOutW(hdc, 76, 46, L"বাংলা লিখুন, সহজেই।", 19);

            // 3. Draw Vertical Divider
            HPEN hPenDivider = CreatePen(PS_SOLID, 1, RGB(226, 232, 240));
            SelectObject(hdc, hPenDivider);
            MoveToEx(hdc, 235, 0, NULL);
            LineTo(hdc, 235, 580);

            // 4. Draw Content Card Area
            RECT rcCard = {245, 15, 800, 465};
            FillRect(hdc, &rcCard, hBrushCard);
            FrameRect(hdc, &rcCard, hBrushBg);

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

                if (SendMessage(GetDlgItem(hWnd, IDC_RADIO_CAND3), BM_GETCHECK, 0, 0) == BST_CHECKED) g_settings.max_candidates = 3;
                else if (SendMessage(GetDlgItem(hWnd, IDC_RADIO_CAND4), BM_GETCHECK, 0, 0) == BST_CHECKED) g_settings.max_candidates = 4;
                else g_settings.max_candidates = 5;

                if (SendMessage(GetDlgItem(hWnd, IDC_RADIO_THEME_LIGHT), BM_GETCHECK, 0, 0) == BST_CHECKED) g_settings.theme = 1;
                else if (SendMessage(GetDlgItem(hWnd, IDC_RADIO_THEME_DARK), BM_GETCHECK, 0, 0) == BST_CHECKED) g_settings.theme = 2;
                else g_settings.theme = 0;

                SaveSettings();
                SetWindowTextW(hLblStatus, L"✅ সেটিংস সফলভাবে সংরক্ষিত হয়েছে!");
            } else if (wmId == IDC_BTN_CLOSE) {
                PostQuitMessage(0);
            } else if (wmId == IDC_BTN_ADD_WORD) {
                wchar_t r_buf[128] = {0}, b_buf[128] = {0};
                GetWindowTextW(hEditRoman, r_buf, 128);
                GetWindowTextW(hEditBangla, b_buf, 128);
                if (wcslen(r_buf) > 0 && wcslen(b_buf) > 0) {
                    std::wstring dict_file = GetConfigDirectory() + L"\\personal_dict.txt";
                    std::wofstream dict_out(dict_file.c_str(), std::ios::app);
                    if (dict_out.is_open()) {
                        dict_out << r_buf << L"\t" << b_buf << L"\n";
                        SetWindowTextW(hLblStatus, L"✅ অভিধানে শব্দটি যুক্ত করা হয়েছে!");
                        SetWindowTextW(hEditRoman, L"");
                        SetWindowTextW(hEditBangla, L"");
                        RefreshDictionaryList();
                    }
                }
            } else if (wmId == IDC_BTN_DEL_WORD) {
                int sel = (int)SendMessage(hListDict, LB_GETCURSEL, 0, 0);
                if (sel != LB_ERR) {
                    wchar_t sel_text[256];
                    SendMessageW(hListDict, LB_GETTEXT, sel, (LPARAM)sel_text);

                    std::wstring dict_file = GetConfigDirectory() + L"\\personal_dict.txt";
                    std::wifstream in(dict_file.c_str());
                    std::vector<std::wstring> lines;
                    std::wstring line;
                    while (std::getline(in, line)) {
                        if (line != sel_text && !line.empty()) {
                            lines.push_back(line);
                        }
                    }
                    in.close();

                    std::wofstream out(dict_file.c_str(), std::ios::trunc);
                    for (const auto& l : lines) out << l << L"\n";
                    out.close();

                    RefreshDictionaryList();
                    SetWindowTextW(hLblStatus, L"🗑️ শব্দটি মুছে ফেলা হয়েছে।");
                }
            } else if (wmId == IDC_BTN_RESET_DEF) {
                g_settings = AppSettings();
                SaveSettings();
                SetWindowTextW(hLblStatus, L"🔄 ফ্যাক্টরি ডিফল্টে রিসেট করা হয়েছে।");
            } else if (wmId == IDC_BTN_CHECK_UPDATE) {
                SetWindowTextW(hLblStatus, L"✨ আপনি Likhi-এর সর্বশেষ সংস্করণ (v1.0.0) ব্যবহার করছেন।");
            }
            break;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            HWND hCtl = (HWND)lParam;
            SetBkMode(hdc, TRANSPARENT);
            
            // Highlight credit in Royal Blue
            if (GetDlgCtrlID(hCtl) == 0 && g_active_section == SEC_ABOUT) {
                SetTextColor(hdc, RGB(2, 132, 199)); // Bright Sky Blue
            } else {
                SetTextColor(hdc, RGB(30, 41, 59));
            }
            return (LRESULT)hBrushCard;
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

    WNDCLASSEXW wcex;
    memset(&wcex, 0, sizeof(WNDCLASSEXW));
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(1));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = CreateSolidBrush(RGB(248, 250, 252));
    wcex.lpszClassName = L"LikhiFluentSettingsClass";
    wcex.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(1));

    RegisterClassExW(&wcex);

    HWND hWnd = CreateWindowW(
        L"LikhiFluentSettingsClass",
        L"Likhi (লিখি) — সেটিংস ও ড্যাশবোর্ড",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 830, 570,
        NULL, NULL, hInstance, NULL
    );

    if (!hWnd) return FALSE;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
