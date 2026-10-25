// ============================================================================
// LIKHI - UNIVERSAL MODE HOST  (likhi_universal.exe)
// ============================================================================
//
// The second input path. Where TSF cannot reach the host (UWP/Store apps such as
// WhatsApp Desktop, some Java UIs, game engines, remote sessions) this process
// types Bangla through ordinary Unicode keystrokes:
//
//   user types  p o r i b o r t o n      (the letters reach the app normally)
//   boundary    space
//   Likhi       erases those 10 letters with synthetic backspaces and types
//               পরিবর্তন + one space
//
// DESIGN RULES
//   * The decision logic lives in universal/include/universal_typing.h (pure,
//     regression-tested: tests/unit/test_universal_typing.cpp). This file is the
//     thin Win32 host around it.
//   * NOTHING is transformed unless the user has actually selected Bengali input
//     for the focused window (keyboard layout 0x0845). English typing, digits,
//     punctuation, Ctrl/Alt/Win chords, function keys, navigation and the numpad
//     are never touched.
//   * Automatic mode stands down whenever bangla_tsf.dll is loaded in the
//     focused process (TSF is doing the job) or when that cannot be determined.
//   * Transliteration goes through the SAME BanglaEngine as the TSF path, with
//     the same lexicon, fuzzy matcher, personal dictionary and learning file, so
//     there is exactly one language engine and one personalisation model.
//   * No keystroke text is ever logged. The log holds mode changes, target
//     application names and counters only.
//
// USAGE
//   likhi_universal.exe            run the tray host (installed to autostart)
//   likhi_universal.exe --check    print diagnostics and exit (no hook left)
//
// ============================================================================

#include <windows.h>
#include <tlhelp32.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <shlobj.h>
#include <psapi.h>

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cwchar>
#include <cstdarg>

#include "universal_typing.h"
#include "bangla_engine.h"
#include <thread>
#include "likhi_version.h"
#include "update_service.h"
#include "translation_service.h"
#include "likhi_mode_ipc.h"
#include "topbar_window.h"
#include "virtual_keyboard_window.h"
#include "candidate_window.h"

using likhi::universal::ActionKind;
using likhi::universal::ClassifyKey;
using likhi::universal::Decision;
using likhi::universal::InputMode;
using likhi::universal::KeyEvent;
using likhi::universal::UniversalTyping;

namespace {

// --- Bengali (Bangladesh) 0x0845: the language Likhi is registered under ------
const UINT_PTR kBengaliLangId = 0x0845;

// --- tray / window plumbing --------------------------------------------------
const wchar_t kWindowClass[] = L"LikhiUniversalModeWindow";
const UINT kTrayMessage = WM_APP + 1;
const UINT kTrayPinMessage = WM_APP + 3;
const int kHotkeyToggle = 0xA1;
const int kHotkeyToggleConversion = 0xA2;
const int kHotkeyToggleSpace = 0xA3;
const int kHotkeyToggleVoice = 0xA4;
const UINT_PTR kTimerReloadSettings = 1;
const UINT kSettingsPollMs = 3000;
const UINT_PTR kTimerBackgroundUpdateCheck = 2;
const UINT kUpdateCheckDelayMs = 15000;
const UINT kMsgUpdateCheckResult = WM_APP + 2;


// --- state -------------------------------------------------------------------
HINSTANCE g_instance = nullptr;
HWND g_window = nullptr;
HHOOK g_keyboard_hook = nullptr;
HHOOK g_mouse_hook = nullptr;
UniversalTyping g_typing;
BanglaEngine* g_engine = nullptr;

InputMode g_mode = InputMode::kAutomatic;
bool g_host_enabled = true;        // settings.json "universal_mode"
bool g_personal_learning = true;   // settings.json "personal_learning"
bool g_auto_check_update = true;   // settings.json "auto_check_update" (enabled by default)
bool g_english_translation_enabled = true; // settings.json "english_translation"
bool g_next_word_prediction = true;        // settings.json "next_word_prediction" or "word_prediction"
likhi::ConversionTarget g_conversion_target = likhi::ConversionTarget::kBangla;
likhi::TranslationService g_translation_service;
bool g_tray1_added = false;
bool g_tray2_added = false;
HICON g_tray_hotkey_icon = nullptr;
HICON g_tray_pin_icon = nullptr;
likhi::universal::TopBarWindow g_topbar;
likhi::universal::VirtualKeyboardWindow g_osk;
bangla_tsf::CandidateWindow g_candidate_window;
std::vector<std::wstring> g_current_candidates;
bool g_is_predicting_next_word = false;
static POINT g_last_click_pt = {0, 0};
bool g_show_topbar = true;
int g_topbar_x = -1;
int g_topbar_y = 10;

HWND g_last_foreground = nullptr;
std::wstring g_latest_update_version;
std::string g_latest_update_url;

// --- small helpers -----------------------------------------------------------
std::string NarrowUtf8(const std::wstring& text) {
    if (text.empty()) return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(),
                                   nullptr, 0, nullptr, nullptr);
    std::string out(size > 0 ? size : 0, '\0');
    if (size > 0) {
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), out.data(),
                            size, nullptr, nullptr);
    }
    return out;
}

std::wstring WidenUtf8(const std::string& text) {
    if (text.empty()) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(),
                                   nullptr, 0);
    std::wstring out(size > 0 ? size : 0, L'\0');
    if (size > 0) {
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), out.data(), size);
    }
    return out;
}

std::wstring AppDataDir() {
    wchar_t buffer[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, buffer))) {
        std::wstring dir = buffer;
        dir += L"\\PC-Bangla-Typing-App";
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir;
    }
    return std::wstring();
}

std::wstring ExeDir() {
    wchar_t path[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full = path;
    size_t slash = full.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : full.substr(0, slash);
}

// Never logs typed text: only events and application names.
void Log(const std::string& message) {
    std::wstring dir = AppDataDir();
    if (dir.empty()) return;
    std::wstring path = dir + L"\\universal.log";
    WIN32_FILE_ATTRIBUTE_DATA info = {0};
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &info) &&
        info.nFileSizeLow > 256u * 1024u) {
        DeleteFileW(path.c_str());
    }
    SYSTEMTIME now = {0};
    GetLocalTime(&now);
    char stamp[64] = {0};
    sprintf_s(stamp, sizeof(stamp), "%04d-%02d-%02d %02d:%02d:%02d ",
              now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
    std::ofstream out(path.c_str(), std::ios::app);
    if (out) out << stamp << message << "\n";
}

// --- settings.json (same file the TSF path and the Settings app use) ---------
void LoadSettings() {
    std::wstring dir = AppDataDir();
    if (dir.empty()) return;
    std::ifstream in((dir + L"\\settings.json").c_str());
    if (!in) return;
    std::stringstream buffer;
    buffer << in.rdbuf();
    const std::string text = buffer.str();

    if (text.find("\"input_mode\": \"tsf_only\"") != std::string::npos) {
        g_mode = InputMode::kTsfOnly;
    } else if (text.find("\"input_mode\": \"universal\"") != std::string::npos) {
        g_mode = InputMode::kUniversalOnly;
    } else {
        g_mode = InputMode::kAutomatic;
    }
    if (text.find("\"universal_mode\": false") != std::string::npos) g_host_enabled = false;
    if (text.find("\"universal_mode\": true") != std::string::npos) g_host_enabled = true;
    if (text.find("\"personal_learning\": false") != std::string::npos) g_personal_learning = false;
    if (text.find("\"personal_learning\": true") != std::string::npos) g_personal_learning = true;
    if (text.find("\"auto_check_update\": false") != std::string::npos) g_auto_check_update = false;
    if (text.find("\"auto_check_update\": true") != std::string::npos) g_auto_check_update = true;
    if (text.find("\"english_translation\": false") != std::string::npos) g_english_translation_enabled = false;
    if (text.find("\"english_translation\": true") != std::string::npos) g_english_translation_enabled = true;
    if (text.find("\"next_word_prediction\": false") != std::string::npos ||
        text.find("\"word_prediction\": false") != std::string::npos) {
        g_next_word_prediction = false;
    }
    if (text.find("\"next_word_prediction\": true") != std::string::npos ||
        text.find("\"word_prediction\": true") != std::string::npos) {
        g_next_word_prediction = true;
    }
    if (text.find("\"show_topbar\": false") != std::string::npos) g_show_topbar = false;
    if (text.find("\"show_topbar\": true") != std::string::npos) g_show_topbar = true;
    if (text.find("\"topbar_autohide\": true") != std::string::npos) g_topbar.SetAutoHide(true);
    if (text.find("\"topbar_autohide\": false") != std::string::npos) g_topbar.SetAutoHide(false);
    // Dock position kept as customized by user without 3s polling reset
    g_translation_service.SetConsentEnabled(g_english_translation_enabled);

    if (g_engine) BanglaEngine_SetLearningEnabled(g_engine, g_personal_learning);
    g_typing.SetEnabled(g_host_enabled && g_mode != InputMode::kTsfOnly);
}

// --- engine (one language engine shared with TSF) ----------------------------
std::string FindLexicon(const std::wstring& exe_dir) {
    std::vector<std::wstring> candidates;
    if (!exe_dir.empty()) {
        candidates.push_back(exe_dir + L"\\data\\lexicon.bin");
        candidates.push_back(exe_dir + L"\\lexicon.bin");
        candidates.push_back(exe_dir + L"\\..\\release_package\\data\\lexicon.bin");
        candidates.push_back(exe_dir + L"\\..\\..\\release_package\\data\\lexicon.bin");
    }
    std::wstring appdata = AppDataDir();
    if (!appdata.empty()) candidates.push_back(appdata + L"\\lexicon.bin");

    for (const std::wstring& path : candidates) {
        if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return NarrowUtf8(path);
        }
    }
    return std::string();
}

void InitEngine() {
    EngineConfig config;
    BanglaEngine_GetDefaultConfig(&config);
    const std::string lexicon = FindLexicon(ExeDir());
    const std::string user_dict = NarrowUtf8(AppDataDir() + L"\\user_dict.txt");
    if (!lexicon.empty()) config.lexicon_binary_path = lexicon.c_str();
    config.user_dict_path = user_dict.c_str();
    g_engine = BanglaEngine_Create(&config);
    if (g_engine) BanglaEngine_SetLearningEnabled(g_engine, g_personal_learning);
}

// Ask the shared engine for the best Bengali spelling of a roman word. Returns
// an empty string when there is nothing better than what the user typed (for
// example an English word that is preserved on purpose): then Universal Mode
// leaves the typed text completely alone.
std::wstring Transliterate(const std::wstring& roman) {
    if (!g_engine || roman.empty()) return std::wstring();
    const std::string key = NarrowUtf8(roman);
    BanglaEngine_SetComposition(g_engine, key.c_str());
    CandidateList list;
    BanglaEngine_GetCandidates(g_engine, &list);
    if (list.count == 0) return std::wstring();
    const std::wstring best = WidenUtf8(list.candidates[0].bengali_text);
    if (best.empty()) return std::wstring();
    if (best.size() == roman.size()) {
        bool same = true;
        for (size_t i = 0; i < best.size(); ++i) {
            wchar_t a = best[i], b = roman[i];
            if (a >= L'A' && a <= L'Z') a = (wchar_t)(a - L'A' + L'a');
            if (b >= L'A' && b <= L'Z') b = (wchar_t)(b - L'A' + L'a');
            if (a != b) { same = false; break; }
        }
        if (same) return std::wstring();  // English preserved: do not replace
    }
    return best;
}

// --- focus / TSF detection ---------------------------------------------------
HKL ForegroundLayout() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return nullptr;
    DWORD thread = GetWindowThreadProcessId(hwnd, nullptr);
    if (!thread) return nullptr;
    return GetKeyboardLayout(thread);
}

std::wstring ForegroundProcessName() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return std::wstring();
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) return std::wstring();
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return std::wstring();
    wchar_t name[MAX_PATH] = {0};
    DWORD size = MAX_PATH;
    std::wstring result;
    if (QueryFullProcessImageNameW(process, 0, name, &size)) result = name;
    CloseHandle(process);
    return result;
}

// True when the focused process has our TSF service loaded, i.e. the normal
// input path is in charge there. When the process cannot be inspected the
// answer is deliberately "true", so Universal Mode never takes over blindly.
bool TsfLoadedInForegroundProcess() {
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return true;
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) return true;
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!process) return true;
    HMODULE modules[1024];
    DWORD needed = 0;
    bool found = false;
    if (EnumProcessModules(process, modules, sizeof(modules), &needed)) {
        const DWORD count = needed / sizeof(HMODULE);
        for (DWORD i = 0; i < count && i < 1024; ++i) {
            wchar_t name[MAX_PATH] = {0};
            if (GetModuleBaseNameW(process, modules[i], name, MAX_PATH)) {
                if (_wcsicmp(name, L"bangla_tsf.dll") == 0) { found = true; break; }
            }
        }
    }
    CloseHandle(process);
    return found;
}

bool ForegroundIsBengali() {
    HKL layout = ForegroundLayout();
    return layout != nullptr && LOWORD((UINT_PTR)layout) == kBengaliLangId;
}

// The complete activation policy for the universal backend.
bool ShouldHandleNow() {
    if (!g_host_enabled || !g_typing.enabled()) return false;

    likhi::ModeTarget mode = likhi::ModeIpc::Instance().GetMode();
    if (mode == likhi::ModeTarget::kEnglish) {
        return false;
    }

    // When TSF is actively loaded and active in the foreground application,
    // let TSF handle it! Universal mode must NOT double-handle or conflict!
    if (ForegroundIsBengali() && TsfLoadedInForegroundProcess()) {
        return false;
    }

    // In all other cases (e.g. Chrome with ENG layout, Discord, games, non-TSF apps):
    if (mode == likhi::ModeTarget::kBangla || mode == likhi::ModeTarget::kBanglaToEnglish) {
        return true;
    }

    return false;
}

void ResetIfFocusChanged() {
    HWND foreground = GetForegroundWindow();
    if (foreground != g_last_foreground) {
        g_last_foreground = foreground;
        g_typing.Reset();  // the caret belongs to another window now
        if (g_candidate_window.IsVisible()) {
            g_candidate_window.SetPredictionMode(false);
            g_candidate_window.Hide();
            g_current_candidates.clear();
        }
        g_is_predicting_next_word = false;
    }
}


static std::wstring FindSoundFile(const wchar_t* filename) {
    wchar_t appdata[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appdata))) {
        std::wstring p = std::wstring(appdata) + L"\\PC-Bangla-Typing-App\\sound\\" + filename;
        if (GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES) return p;
    }
    wchar_t mod[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, mod, MAX_PATH);
    std::wstring dir = mod;
    size_t sl = dir.find_last_of(L"\\/");
    if (sl != std::wstring::npos) dir = dir.substr(0, sl);
    std::wstring p2 = dir + L"\\sound\\" + filename;
    if (GetFileAttributesW(p2.c_str()) != INVALID_FILE_ATTRIBUTES) return p2;
    std::wstring p3 = dir + L"\\..\\assets\\sound\\" + filename;
    if (GetFileAttributesW(p3.c_str()) != INVALID_FILE_ATTRIBUTES) return p3;
    return L"";
}

void PlaySoundEffect(int sound_type = 0) {
    std::wstring snd_path;
    if (sound_type == 3) {
        snd_path = FindSoundFile(L"voice_start.wav");
    } else if (sound_type == 1) {
        snd_path = FindSoundFile(L"switch_bn.wav");
    } else if (sound_type == 2) {
        snd_path = FindSoundFile(L"switch_en.wav");
    } else {
        bool is_en = (likhi::ModeIpc::Instance().GetMode() == likhi::ModeTarget::kEnglish);
        snd_path = FindSoundFile(is_en ? L"switch_en.wav" : L"switch_bn.wav");
        if (snd_path.empty()) snd_path = FindSoundFile(L"switch_lang.wav");
    }

    if (!snd_path.empty() && PlaySoundW(snd_path.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT)) {
        return;
    }

    if (sound_type == 3) {
        PlaySoundW(L"C:\\Windows\\Media\\Windows Proximity Connection.wav", NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    } else {
        PlaySoundW(L"C:\\Windows\\Media\\Windows Notify Messaging.wav", NULL, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
    }
}

// --- key handling ------------------------------------------------------------
bool ModifierDown(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

wchar_t CharacterFor(unsigned int vk, unsigned int scan) {
    BYTE state[256] = {0};
    state[VK_SHIFT] = ModifierDown(VK_SHIFT) ? 0x80 : 0;
    state[VK_CONTROL] = ModifierDown(VK_CONTROL) ? 0x80 : 0;
    state[VK_MENU] = ModifierDown(VK_MENU) ? 0x80 : 0;
    state[VK_CAPITAL] = (GetKeyState(VK_CAPITAL) & 1) ? 0x01 : 0;
    wchar_t buffer[8] = {0};
    const int produced = ToUnicodeEx(vk, scan, state, buffer, 8, 0, ForegroundLayout());
    if (produced == 1) return buffer[0];
    if (produced < 0) return 0;  // dead key: never ours
    if (vk >= 'A' && vk <= 'Z') {
        const bool upper = ModifierDown(VK_SHIFT) != ((GetKeyState(VK_CAPITAL) & 1) != 0);
        return (wchar_t)(upper ? vk : vk + 32);
    }
    if (vk >= '0' && vk <= '9') return (wchar_t)vk;
    return 0;
}

void AppendUnicode(std::vector<INPUT>& inputs, const std::wstring& text) {
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
}

void AppendBackspaces(std::vector<INPUT>& inputs, int count) {
    for (int i = 0; i < count; ++i) {
        INPUT down = {0};
        down.type = INPUT_KEYBOARD;
        down.ki.wVk = VK_BACK;
        inputs.push_back(down);
        INPUT up = down;
        up.ki.dwFlags = KEYEVENTF_KEYUP;
        inputs.push_back(up);
    }
}

RECT GetCurrentCaretRect() {
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
}

void ShowNextWordPredictions();

void CommitCandidateByIndex(size_t index) {
    if (index >= g_current_candidates.size()) {
        g_candidate_window.SetPredictionMode(false);
        g_candidate_window.Hide();
        g_current_candidates.clear();
        g_typing.Reset();
        g_is_predicting_next_word = false;
        return;
    }

    if (g_is_predicting_next_word) {
        std::wstring replacement = g_current_candidates[index];
        g_candidate_window.SetPredictionMode(false);
        g_candidate_window.Hide();
        g_current_candidates.clear();
        g_is_predicting_next_word = false;

        std::vector<INPUT> inputs;
        AppendUnicode(inputs, replacement);
        AppendUnicode(inputs, L" ");
        if (!inputs.empty()) {
            SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));
        }

        if (g_engine && !replacement.empty()) {
            BanglaEngine_CommitWord(g_engine, NarrowUtf8(replacement).c_str());
        }

        ShowNextWordPredictions();
        return;
    }

    if (!g_typing.has_pending()) {
        g_candidate_window.SetPredictionMode(false);
        g_candidate_window.Hide();
        g_current_candidates.clear();
        g_typing.Reset();
        g_is_predicting_next_word = false;
        return;
    }

    std::wstring replacement = g_current_candidates[index];
    int erase_count = (int)g_typing.roman().size();
    std::wstring roman = g_typing.roman();

    likhi::ModeTarget cur_mode = likhi::ModeIpc::Instance().GetMode();
    bool is_en_mode = (cur_mode == likhi::ModeTarget::kEnglish);
    bool is_trans_mode = (cur_mode == likhi::ModeTarget::kBanglaToEnglish);
    if (is_trans_mode && !replacement.empty()) {
        std::wstring cached_en;
        if (g_translation_service.LookupCache(replacement, cached_en)) {
            replacement = cached_en;
        } else {
            likhi::TranslationResult res = g_translation_service.TranslateDirect(replacement, 1500);
            if (res.success && !res.translated_english.empty()) {
                replacement = res.translated_english;
            }
        }
    }

    std::vector<INPUT> inputs;
    if (erase_count > 0) {
        AppendBackspaces(inputs, erase_count);
    }
    if (!replacement.empty()) {
        AppendUnicode(inputs, replacement);
    }
    AppendUnicode(inputs, L" ");

    if (!inputs.empty()) {
        SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));
    }

    if (!replacement.empty() && g_engine && !is_en_mode) {
        BanglaEngine_LearnWord(g_engine, NarrowUtf8(roman).c_str(),
                               NarrowUtf8(replacement).c_str());
        BanglaEngine_CommitWord(g_engine, NarrowUtf8(replacement).c_str());
    }

    g_candidate_window.SetPredictionMode(false);
    g_candidate_window.Hide();
    g_current_candidates.clear();
    g_typing.Reset();

    if (!is_en_mode && !is_trans_mode && !replacement.empty()) {
        ShowNextWordPredictions();
    }
}

void ShowNextWordPredictions() {
    g_is_predicting_next_word = false;
    if (!g_next_word_prediction || !g_engine || !ShouldHandleNow()) {
        if (g_candidate_window.IsVisible()) {
            g_candidate_window.SetPredictionMode(false);
            g_candidate_window.Hide();
        }
        g_current_candidates.clear();
        return;
    }

    likhi::ModeTarget cur_mode = likhi::ModeIpc::Instance().GetMode();
    if (cur_mode == likhi::ModeTarget::kEnglish) {
        if (g_candidate_window.IsVisible()) {
            g_candidate_window.SetPredictionMode(false);
            g_candidate_window.Hide();
        }
        g_current_candidates.clear();
        return;
    }

    CandidateList preds;
    BanglaEngine_GetNextWordPredictions(g_engine, &preds);
    if (preds.count == 0) {
        if (g_candidate_window.IsVisible()) {
            g_candidate_window.SetPredictionMode(false);
            g_candidate_window.Hide();
        }
        g_current_candidates.clear();
        return;
    }

    g_current_candidates.clear();
    for (size_t i = 0; i < (size_t)preds.count; ++i) {
        std::wstring w = WidenUtf8(preds.candidates[i].bengali_text);
        if (!w.empty() && std::find(g_current_candidates.begin(), g_current_candidates.end(), w) == g_current_candidates.end()) {
            g_current_candidates.push_back(w);
            if (g_current_candidates.size() >= 5) break;
        }
    }

    if (g_current_candidates.empty()) {
        if (g_candidate_window.IsVisible()) {
            g_candidate_window.SetPredictionMode(false);
            g_candidate_window.Hide();
        }
        return;
    }

    g_is_predicting_next_word = true;
    g_candidate_window.SetPredictionMode(true);
    RECT caret_rc = GetCurrentCaretRect();
    g_candidate_window.ShowCandidates(g_current_candidates, 0, caret_rc);
}

void UpdateCandidateWindow() {
    g_is_predicting_next_word = false;
    g_candidate_window.SetPredictionMode(false);
    if (!g_engine || !g_typing.has_pending() || !ShouldHandleNow()) {
        if (g_candidate_window.IsVisible()) {
            g_candidate_window.Hide();
        }
        g_current_candidates.clear();
        return;
    }

    std::wstring roman = g_typing.roman();
    std::string key = NarrowUtf8(roman);
    BanglaEngine_SetComposition(g_engine, key.c_str());
    CandidateList list;
    BanglaEngine_GetCandidates(g_engine, &list);

    g_current_candidates.clear();
    for (size_t i = 0; i < (size_t)list.count; ++i) {
        std::wstring cand = WidenUtf8(list.candidates[i].bengali_text);
        if (!cand.empty() && std::find(g_current_candidates.begin(), g_current_candidates.end(), cand) == g_current_candidates.end()) {
            g_current_candidates.push_back(cand);
            if (g_current_candidates.size() >= 5) break;
        }
    }
    bool has_roman = false;
    for (const auto& c : g_current_candidates) {
        if (c == roman) { has_roman = true; break; }
    }
    if (!has_roman && g_current_candidates.size() < 6) {
        g_current_candidates.push_back(roman);
    }

    if (g_current_candidates.empty()) {
        g_candidate_window.Hide();
        return;
    }

    RECT caret_rc = GetCurrentCaretRect();
    g_candidate_window.ShowCandidates(g_current_candidates, 0, caret_rc);
}

// Performs one word replacement: erase the roman run, type the Bengali word,
// then type the boundary character. When the engine has no Bengali answer the
// erasure is skipped entirely and only the boundary character is sent, so the
// user's own text is never damaged.
// In English conversion mode, converts the transliterated word/phrase into English.
void ApplyReplace(const Decision& decision) {
    std::wstring replacement;
    size_t sel = g_candidate_window.GetSelectedIndex();
    if (g_candidate_window.IsVisible() && sel < g_current_candidates.size()) {
        replacement = g_current_candidates[sel];
    } else {
        replacement = Transliterate(decision.roman);
    }
    
    likhi::ModeTarget cur_mode = likhi::ModeIpc::Instance().GetMode();
    bool is_en_mode = (cur_mode == likhi::ModeTarget::kEnglish);
    bool is_trans_mode = (cur_mode == likhi::ModeTarget::kBanglaToEnglish);
    if (is_trans_mode && !replacement.empty()) {
        std::wstring cached_en;
        if (g_translation_service.LookupCache(replacement, cached_en)) {
            replacement = cached_en;
        } else {
            likhi::TranslationResult res = g_translation_service.TranslateDirect(replacement, 1500);
            if (res.success && !res.translated_english.empty()) {
                replacement = res.translated_english;
            }
        }
    }

    std::vector<INPUT> inputs;
    if (decision.erase_chars > 0) {
        AppendBackspaces(inputs, decision.erase_chars);
    }
    if (!replacement.empty()) {
        AppendUnicode(inputs, replacement);
    }
    AppendUnicode(inputs, decision.trailing);
    if (!inputs.empty()) {
        SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));
    }
    if (!replacement.empty() && g_engine && !is_en_mode) {
        // Same personalisation file as the TSF path: learning in WhatsApp is
        // visible in Word and the other way round.
        BanglaEngine_LearnWord(g_engine, NarrowUtf8(decision.roman).c_str(),
                               NarrowUtf8(replacement).c_str());
        BanglaEngine_CommitWord(g_engine, NarrowUtf8(replacement).c_str());
    }
    if (g_candidate_window.IsVisible()) {
        g_candidate_window.SetPredictionMode(false);
        g_candidate_window.Hide();
        g_current_candidates.clear();
    }
    g_is_predicting_next_word = false;

    if (!is_en_mode && !is_trans_mode && !replacement.empty()) {
        ShowNextWordPredictions();
    }
}

void UpdateTray();
void ShowTransientBadge(bool is_english);

void TriggerModeToggle() {
    static DWORD s_last_toggle = 0;
    DWORD now = GetTickCount();
    if (now - s_last_toggle < 250) return;
    s_last_toggle = now;

    likhi::ModeIpc::Instance().ToggleMode();
    likhi::ModeTarget m = likhi::ModeIpc::Instance().GetMode();
    PlaySoundEffect(m == likhi::ModeTarget::kEnglish ? 2 : 1);
    bool is_en = (m == likhi::ModeTarget::kEnglish);
    Log(is_en ? "hotkey: english mode" : "hotkey: bangla mode");
    UpdateTray();
    // ShowTransientBadge(is_en);
    g_topbar.UpdateMode(m);
    g_typing.Reset();
    if (g_candidate_window.IsVisible()) {
        g_candidate_window.Hide();
        g_current_candidates.clear();
    }
}

void TriggerTranslationToggle() {
    static DWORD s_last_toggle = 0;
    DWORD now = GetTickCount();
    if (now - s_last_toggle < 250) return;
    s_last_toggle = now;

    likhi::ModeIpc::Instance().ToggleBanglaToEnglish();
    likhi::ModeTarget m = likhi::ModeIpc::Instance().GetMode();
    PlaySoundEffect(m == likhi::ModeTarget::kEnglish ? 2 : 1);
    bool is_en = (m == likhi::ModeTarget::kEnglish);
    Log(m == likhi::ModeTarget::kBanglaToEnglish ? "hotkey: english conversion mode" : "hotkey: bangla mode");
    UpdateTray();
    // ShowTransientBadge(is_en);
    g_topbar.UpdateMode(m);
    g_typing.Reset();
    if (g_candidate_window.IsVisible()) {
        g_candidate_window.Hide();
        g_current_candidates.clear();
    }
}

LRESULT CALLBACK KeyboardHookProc(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION && (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN)) {
        const KBDLLHOOKSTRUCT* key = reinterpret_cast<KBDLLHOOKSTRUCT*>(lparam);
        // Never react to our own synthetic keystrokes.
        if (!(key->flags & LLKHF_INJECTED)) {
            bool is_ctrl = ModifierDown(VK_CONTROL);
            bool is_alt = ModifierDown(VK_MENU);
            bool is_shift = ModifierDown(VK_SHIFT);
            bool is_win = ModifierDown(VK_LWIN) || ModifierDown(VK_RWIN);

            // Instant Global Shortcut Toggle:
            // 1. Ctrl + Alt + Space (Primary user hotkey)
            // 2. Ctrl + Space
            // 3. F12
            if ((key->vkCode == VK_SPACE && is_ctrl && is_alt) ||
                (key->vkCode == VK_SPACE && is_ctrl && !is_alt && !is_shift && !is_win) ||
                (key->vkCode == VK_F12 && !is_ctrl && !is_alt && !is_shift && !is_win)) {
                TriggerModeToggle();
                return 1; // Eat the hotkey event completely!
            }

            // Ctrl + Alt + E for Banglish to English translation mode
            if (key->vkCode == 'E' && is_ctrl && is_alt && !is_win) {
                TriggerTranslationToggle();
                return 1;
            }

            ResetIfFocusChanged();
            if (ShouldHandleNow()) {
                if (g_is_predicting_next_word) {
                    if (g_candidate_window.IsVisible()) {
                        if (key->vkCode == VK_ESCAPE && !is_ctrl && !is_alt && !is_win) {
                            g_candidate_window.SetPredictionMode(false);
                            g_candidate_window.Hide();
                            g_current_candidates.clear();
                            g_is_predicting_next_word = false;
                            return 1;
                        }
                        if ((key->vkCode == VK_SPACE || key->vkCode == VK_BACK) && !is_ctrl && !is_alt && !is_win) {
                            g_candidate_window.SetPredictionMode(false);
                            g_candidate_window.Hide();
                            g_current_candidates.clear();
                            g_is_predicting_next_word = false;
                            return CallNextHookEx(nullptr, code, wparam, lparam);
                        }
                        if (key->vkCode == VK_DOWN && !is_ctrl && !is_alt && !is_win) {
                            g_candidate_window.SelectNext();
                            return 1;
                        }
                        if (key->vkCode == VK_UP && !is_ctrl && !is_alt && !is_win) {
                            g_candidate_window.SelectPrev();
                            return 1;
                        }
                        if ((key->vkCode == VK_RETURN || (key->vkCode == VK_TAB && !is_shift)) && !is_ctrl && !is_alt && !is_win) {
                            size_t sel = g_candidate_window.GetSelectedIndex();
                            if (sel < g_current_candidates.size()) {
                                CommitCandidateByIndex(sel);
                                return 1;
                            }
                        }
                        if (!is_ctrl && !is_alt && !is_win && !is_shift) {
                            if (key->vkCode >= '1' && key->vkCode <= '5') {
                                size_t idx = (size_t)(key->vkCode - '1');
                                if (idx < g_current_candidates.size()) {
                                    CommitCandidateByIndex(idx);
                                    return 1;
                                }
                            } else if (key->vkCode >= VK_NUMPAD1 && key->vkCode <= VK_NUMPAD5) {
                                size_t idx = (size_t)(key->vkCode - VK_NUMPAD1);
                                if (idx < g_current_candidates.size()) {
                                    CommitCandidateByIndex(idx);
                                    return 1;
                                }
                            }
                        }
                    }
                    // User started typing a new character or pressed another key:
                    // Immediately dismiss next-word prediction mode without conflicting with present typing!
                    g_is_predicting_next_word = false;
                    g_candidate_window.SetPredictionMode(false);
                    g_candidate_window.Hide();
                    g_current_candidates.clear();
                } else if (g_candidate_window.IsVisible() && g_typing.has_pending()) {
                    if (key->vkCode == VK_DOWN && !is_ctrl && !is_alt && !is_win) {
                        g_candidate_window.SelectNext();
                        return 1;
                    }
                    if (key->vkCode == VK_UP && !is_ctrl && !is_alt && !is_win) {
                        g_candidate_window.SelectPrev();
                        return 1;
                    }
                    if (key->vkCode == VK_TAB && !is_ctrl && !is_alt && !is_win) {
                        g_candidate_window.SelectNext();
                        return 1;
                    }
                    if (key->vkCode == VK_ESCAPE && !is_ctrl && !is_alt && !is_win) {
                        g_candidate_window.Hide();
                        g_current_candidates.clear();
                        g_typing.Reset();
                        return 1;
                    }
                    if (!is_ctrl && !is_alt && !is_win && !is_shift) {
                        if (key->vkCode >= '1' && key->vkCode <= '5') {
                            size_t idx = (size_t)(key->vkCode - '1');
                            if (idx < g_current_candidates.size()) {
                                CommitCandidateByIndex(idx);
                                return 1;
                            }
                        } else if (key->vkCode >= VK_NUMPAD1 && key->vkCode <= VK_NUMPAD5) {
                            size_t idx = (size_t)(key->vkCode - VK_NUMPAD1);
                            if (idx < g_current_candidates.size()) {
                                CommitCandidateByIndex(idx);
                                return 1;
                            }
                        }
                    }
                }

                const wchar_t ch = CharacterFor(key->vkCode, key->scanCode);
                const KeyEvent event = ClassifyKey(
                    (int)key->vkCode, ch, ModifierDown(VK_CONTROL), ModifierDown(VK_MENU),
                    ModifierDown(VK_LWIN) || ModifierDown(VK_RWIN), ModifierDown(VK_SHIFT),
                    false);
                const Decision decision = g_typing.Feed(event);
                if (decision.kind == ActionKind::kReplace) {
                    ApplyReplace(decision);
                    return 1;  // consumed: we type the word and the boundary
                } else if (decision.kind == ActionKind::kBuffer) {
                    UpdateCandidateWindow();
                } else {
                    if (key->vkCode == VK_BACK) {
                        if (g_typing.has_pending()) {
                            UpdateCandidateWindow();
                        } else if (g_candidate_window.IsVisible()) {
                            g_candidate_window.Hide();
                            g_current_candidates.clear();
                        }
                    } else if (!g_typing.has_pending() && g_candidate_window.IsVisible()) {
                        g_candidate_window.Hide();
                        g_current_candidates.clear();
                    }
                }
            } else {
                g_typing.Reset();
                if (g_candidate_window.IsVisible()) {
                    g_candidate_window.SetPredictionMode(false);
                    g_candidate_window.Hide();
                    g_current_candidates.clear();
                }
                g_is_predicting_next_word = false;
            }
        }
    }
    return CallNextHookEx(nullptr, code, wparam, lparam);
}

// A mouse click moves the caret, which invalidates the preview. The hook only
// observes: it never consumes a mouse event.
LRESULT CALLBACK MouseHookProc(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION) {
        const MSLLHOOKSTRUCT* ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lparam);
        switch (wparam) {
            case WM_LBUTTONDOWN:
                if (ms) {
                    g_last_click_pt = ms->pt;
                }
                // fall through
            case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
            case WM_MOUSEWHEEL:  case WM_MOUSEHWHEEL: {
                if (ms && g_candidate_window.IsVisible()) {
                    HWND pt_wnd = WindowFromPoint(ms->pt);
                    if (pt_wnd == g_candidate_window.GetHwnd() || pt_wnd == g_topbar.GetHwnd()) {
                        break;
                    }
                }
                g_typing.Reset();
                if (g_candidate_window.IsVisible()) {
                    g_candidate_window.Hide();
                    g_current_candidates.clear();
                }
                break;
            }
            default:
                break;
        }
    }
    return CallNextHookEx(nullptr, code, wparam, lparam);
}

// // --- tray and floating indicator --------------------------------------------
HICON g_tray_icon = nullptr;
HWND g_badge_window = nullptr;
bool g_badge_is_english = false;
const UINT_PTR kTimerHideBadge = 42;

HICON CreateModeIcon(likhi::ModeTarget mode) {
    int cx = GetSystemMetrics(SM_CXSMICON);
    int cy = GetSystemMetrics(SM_CYSMICON);
    if (cx <= 0) cx = 16;
    if (cy <= 0) cy = 16;

    HDC screen_dc = GetDC(nullptr);
    HDC mem_dc = CreateCompatibleDC(screen_dc);

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = cx;
    bmi.bmiHeader.biHeight = -cy;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP color_bmp = CreateDIBSection(mem_dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!color_bmp || !bits) {
        DeleteDC(mem_dc);
        ReleaseDC(nullptr, screen_dc);
        return nullptr;
    }

    HGDIOBJ old_color = SelectObject(mem_dc, color_bmp);
    memset(bits, 0, cx * cy * 4);

    // Full rounded tile matching Kontho Image 3
    COLORREF bg_color = (mode == likhi::ModeTarget::kEnglish) ? RGB(2, 132, 199)
                      : (mode == likhi::ModeTarget::kBanglaToEnglish) ? RGB(13, 148, 136)
                      : RGB(0, 168, 240); // Cyan for Bangla
    HBRUSH brand_bg = CreateSolidBrush(bg_color);
    HPEN brand_pen = CreatePen(PS_SOLID, 1, bg_color);
    HGDIOBJ ob = SelectObject(mem_dc, brand_bg);
    HGDIOBJ op = SelectObject(mem_dc, brand_pen);
    RoundRect(mem_dc, 0, 0, cx, cy, 5, 5);
    SelectObject(mem_dc, ob);
    SelectObject(mem_dc, op);
    DeleteObject(brand_bg);
    DeleteObject(brand_pen);

    SetBkMode(mem_dc, TRANSPARENT);
    SetTextColor(mem_dc, RGB(255, 255, 255));

    const wchar_t* label = (mode == likhi::ModeTarget::kEnglish) ? L"Eng"
                         : (mode == likhi::ModeTarget::kBanglaToEnglish) ? L"B-E"
                         : L"\u09ac\u09be\u0982";

    int font_h = (mode == likhi::ModeTarget::kEnglish) ? (cy * 65 / 100) : (cy * 75 / 100);
    HFONT font = CreateFontW(
        -font_h, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        (mode == likhi::ModeTarget::kBangla) ? L"Vrinda" : L"Segoe UI"
    );
    HGDIOBJ of = SelectObject(mem_dc, font);
    RECT tr = {0, 0, cx, cy};
    DrawTextW(mem_dc, label, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(mem_dc, of);
    DeleteObject(font);

    GdiFlush();

    // 32-bit Alpha Channel: ensure opaque pixels
    uint32_t* px = static_cast<uint32_t*>(bits);
    std::vector<uint8_t> mask_bits((((cx + 15) / 16) * 2) * cy, 0);
    int mask_stride = ((cx + 15) / 16) * 2;

    for (int y = 0; y < cy; ++y) {
        for (int x = 0; x < cx; ++x) {
            int idx = y * cx + x;
            if ((px[idx] & 0x00FFFFFF) != 0) {
                px[idx] |= 0xFF000000;
            } else {
                mask_bits[y * mask_stride + (x / 8)] |= (0x80 >> (x % 8));
            }
        }
    }

    HBITMAP mask_bmp = CreateBitmap(cx, cy, 1, 1, mask_bits.data());
    ICONINFO ii = {0};
    ii.fIcon = TRUE;
    ii.hbmColor = color_bmp;
    ii.hbmMask = mask_bmp;
    HICON result = CreateIconIndirect(&ii);

    SelectObject(mem_dc, old_color);
    DeleteObject(color_bmp);
    DeleteObject(mask_bmp);
    DeleteDC(mem_dc);
    ReleaseDC(nullptr, screen_dc);
    return result;
}


LRESULT CALLBACK BadgeWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

        HBRUSH bg = CreateSolidBrush(g_badge_is_english ? RGB(26, 115, 232) : RGB(15, 157, 88));
        FillRect(memDC, &rc, bg);
        DeleteObject(bg);

        SetBkMode(memDC, TRANSPARENT);
        SetTextColor(memDC, RGB(255, 255, 255));
        HFONT font = CreateFontW(
            -13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            L"Segoe UI"
        );
        HGDIOBJ of = SelectObject(memDC, font);
        const wchar_t* text = g_badge_is_english ? L"Likhi: English" : L"Likhi: বাংলা";
        DrawTextW(memDC, text, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(memDC, of);
        DeleteObject(font);

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);
        EndPaint(hwnd, &ps);
        return 0;
    } else if (msg == WM_TIMER && wp == kTimerHideBadge) {
        KillTimer(hwnd, kTimerHideBadge);
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    } else if (msg == WM_ERASEBKGND) {
        return 1;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ShowTransientBadge(bool is_english) {
    g_badge_is_english = is_english;
    if (!g_badge_window) {
        WNDCLASSEXW wc = {0};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = BadgeWndProc;
        wc.hInstance = g_instance;
        wc.lpszClassName = L"LikhiFloatingModeBadge";
        RegisterClassExW(&wc);

        g_badge_window = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            L"LikhiFloatingModeBadge", L"", WS_POPUP,
            0, 0, 160, 32,
            nullptr, nullptr, g_instance, nullptr
        );
        if (g_badge_window) {
            HRGN rgn = CreateRoundRectRgn(0, 0, 161, 33, 12, 12);
            SetWindowRgn(g_badge_window, rgn, TRUE);
        }
    }
    if (!g_badge_window) return;

    POINT pt;
    GetCursorPos(&pt);
    int x = pt.x + 12;
    int y = pt.y - 40;
    if (y < 20) y = pt.y + 24;

    SetWindowPos(g_badge_window, HWND_TOPMOST, x, y, 160, 32,
                 SWP_SHOWWINDOW | SWP_NOACTIVATE);
    InvalidateRect(g_badge_window, nullptr, FALSE);
    SetTimer(g_badge_window, kTimerHideBadge, 1200, nullptr);
}

HICON CreateHotkeyTrayIcon() {
    std::wstring exe_dir = ExeDir();
    std::vector<std::wstring> candidates = {
        exe_dir + L"\\icon.ico",
        exe_dir + L"\\assets\\icon.ico",
        exe_dir + L"\\..\\assets\\icon.ico",
        L"C:\\Program Files\\Likhi\\icon.ico"
    };
    for (const auto& path : candidates) {
        HICON h = (HICON)LoadImageW(nullptr, path.c_str(), IMAGE_ICON, 16, 16, LR_LOADFROMFILE);
        if (h) return h;
    }

    int cx = 16, cy = 16;
    HDC screen_dc = GetDC(nullptr);
    HDC mem_dc = CreateCompatibleDC(screen_dc);

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = cx;
    bmi.bmiHeader.biHeight = -cy;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP color_bmp = CreateDIBSection(mem_dc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!color_bmp || !bits) {
        DeleteDC(mem_dc);
        ReleaseDC(nullptr, screen_dc);
        return nullptr;
    }

    HGDIOBJ old_color = SelectObject(mem_dc, color_bmp);
    memset(bits, 0, cx * cy * 4);

    COLORREF bg_color = RGB(15, 23, 42); // Deep Slate
    HBRUSH brand_bg = CreateSolidBrush(bg_color);
    HPEN brand_pen = CreatePen(PS_SOLID, 1, RGB(56, 189, 248)); // Cyan border
    HGDIOBJ ob = SelectObject(mem_dc, brand_bg);
    HGDIOBJ op = SelectObject(mem_dc, brand_pen);
    RoundRect(mem_dc, 0, 0, cx, cy, 4, 4);
    SelectObject(mem_dc, ob);
    SelectObject(mem_dc, op);
    DeleteObject(brand_bg);
    DeleteObject(brand_pen);

    SetBkMode(mem_dc, TRANSPARENT);
    SetTextColor(mem_dc, RGB(255, 255, 255));
    HFONT font = CreateFontW(-12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Nirmala UI");
    HGDIOBJ of = SelectObject(mem_dc, font);
    RECT tr = {0, -1, cx, cy};
    DrawTextW(mem_dc, L"\u09b2", -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(mem_dc, of);
    DeleteObject(font);

    GdiFlush();

    uint32_t* px = static_cast<uint32_t*>(bits);
    std::vector<uint8_t> mask_bits((((cx + 15) / 16) * 2) * cy, 0);
    int mask_stride = ((cx + 15) / 16) * 2;

    for (int y = 0; y < cy; ++y) {
        for (int x = 0; x < cx; ++x) {
            int idx = y * cx + x;
            if ((px[idx] & 0x00FFFFFF) != 0) {
                px[idx] |= 0xFF000000;
            } else {
                mask_bits[y * mask_stride + (x / 8)] |= (0x80 >> (x % 8));
            }
        }
    }

    HBITMAP mask_bmp = CreateBitmap(cx, cy, 1, 1, mask_bits.data());
    ICONINFO ii = {0};
    ii.fIcon = TRUE;
    ii.hbmMask = mask_bmp;
    ii.hbmColor = color_bmp;
    HICON icon = CreateIconIndirect(&ii);

    DeleteObject(mask_bmp);
    SelectObject(mem_dc, old_color);
    DeleteObject(color_bmp);
    DeleteDC(mem_dc);
    ReleaseDC(nullptr, screen_dc);
    return icon;
}

void ShowLanguagePinMenu() {
    HMENU menu = CreatePopupMenu();

    MENUINFO mi = {sizeof(mi)};
    mi.cbSize = sizeof(mi);
    mi.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
    static HBRUSH s_menuBg = CreateSolidBrush(RGB(255, 255, 255));
    mi.hbrBack = s_menuBg;
    SetMenuInfo(menu, &mi);

    likhi::ModeTarget m = likhi::ModeIpc::Instance().GetMode();

    AppendMenuW(menu, MF_STRING | (m == likhi::ModeTarget::kBangla ? MF_CHECKED : 0),
                201, L"\u09ac\u09be\u0982\u09b2\u09be (Bangla Phonetic)\tCtrl+Alt+Space");
    AppendMenuW(menu, MF_STRING | (m == likhi::ModeTarget::kEnglish ? MF_CHECKED : 0),
                202, L"English (Standard US)\tCtrl+Alt+Space");
    AppendMenuW(menu, MF_STRING | (m == likhi::ModeTarget::kBanglaToEnglish ? MF_CHECKED : 0),
                203, L"\u09ac\u09be\u0982\u09b2\u09bf\u09b6 \u2794 \u0987\u0982\u09b0\u09c7\u099c\u09bf \u0985\u09a8\u09c1\u09ac\u09be\u09a6\tCtrl+Alt+E");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(menu, MF_STRING, 204, g_topbar.IsVisible() ? L"\u099f\u09aa\u09ac\u09be\u09b0 \u09b2\u09c1\u0995\u09be\u09a8 (Hide TopBar)" : L"\u099f\u09aa\u09ac\u09be\u09b0 \u09a6\u09c7\u0996\u09be\u09a8 (Restore TopBar)");
    AppendMenuW(menu, MF_STRING, 205, L"\u099f\u09aa\u09ac\u09be\u09b0 \u0995\u09c7\u09a8\u09cd\u09a6্\u09b0\u09c7 \u09b0\u09bf\u09b8\u09c7\u099f (Reset Position)");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(menu, MF_STRING, 206, L"\u09ac\u09be\u0982\u09b2\u09be \u09ad\u09af\u09bc\u09c7\u09b8 \u099f\u09be\u0987\u09aa\u09bf\u0982\tCtrl+Alt+V");
    AppendMenuW(menu, MF_STRING, 207, L"\u0985\u09a8-\u09b8\u09cd\u0995\u09cd\u09b0\u09c0\u09a8 \u0995\u09bf\u09ac\u09cb\u09b0\u09cd\u09a1 (Virtual Keyboard)");
    AppendMenuW(menu, MF_STRING, 208, L"\u09b8\u09c7\u099f\u09bf\u0982\u09b8 \u0993 \u0985\u09aa\u09b6\u09a8...");

    // Set colorful icon badges
    likhi::universal::SetMenuItemIcon(menu, 201, false, likhi::universal::kIconBnLayout);
    likhi::universal::SetMenuItemIcon(menu, 202, false, likhi::universal::kIconEngLayout);
    likhi::universal::SetMenuItemIcon(menu, 203, false, likhi::universal::kIconConverter);
    likhi::universal::SetMenuItemIcon(menu, 204, false, likhi::universal::kIconTray);
    likhi::universal::SetMenuItemIcon(menu, 205, false, likhi::universal::kIconRestart);
    likhi::universal::SetMenuItemIcon(menu, 206, false, likhi::universal::kIconMic);
    likhi::universal::SetMenuItemIcon(menu, 207, false, likhi::universal::kIconKeyboard);
    likhi::universal::SetMenuItemIcon(menu, 208, false, likhi::universal::kIconSettings);

    POINT cursor = {0};
    GetCursorPos(&cursor);
    SetForegroundWindow(g_window);
    const UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                    cursor.x, cursor.y, 0, g_window, nullptr);
    DestroyMenu(menu);
    if (cmd == 0) return;

    if (cmd == 201) {
        likhi::ModeIpc::Instance().SetMode(likhi::ModeTarget::kBangla);
        g_topbar.UpdateMode(likhi::ModeTarget::kBangla);
        UpdateTray();
    } else if (cmd == 202) {
        likhi::ModeIpc::Instance().SetMode(likhi::ModeTarget::kEnglish);
        g_topbar.UpdateMode(likhi::ModeTarget::kEnglish);
        UpdateTray();
    } else if (cmd == 203) {
        likhi::ModeIpc::Instance().SetMode(likhi::ModeTarget::kBanglaToEnglish);
        g_topbar.UpdateMode(likhi::ModeTarget::kBanglaToEnglish);
        UpdateTray();
    } else if (cmd == 204) {
        g_show_topbar = !g_topbar.IsVisible();
        g_topbar.Show(g_show_topbar);
        if (g_show_topbar) g_topbar.ExpandToFull();
        g_topbar.SaveCoordinates();
    } else if (cmd == 205) {
        int screen_w = GetSystemMetrics(SM_CXSCREEN);
        g_topbar.SetPosition((screen_w - 262) / 2, 12);
        g_show_topbar = true;
        g_topbar.Show(true);
        g_topbar.ExpandBar();
        g_topbar.SaveCoordinates();
    } else if (cmd == 206) {
        g_topbar.LaunchVoiceInput();
    } else if (cmd == 207) {
        g_osk.ToggleVisibility();
    } else if (cmd == 208) {
        std::wstring settings = ExeDir() + L"\\bangla_settings.exe";
        if (GetFileAttributesW(settings.c_str()) == INVALID_FILE_ATTRIBUTES) {
            settings = ExeDir() + L"\\..\\release_package\\bangla_settings.exe";
        }
        ShellExecuteW(nullptr, L"open", settings.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void UpdateTray() {
    if (!g_window) return;

    // --- Tray 1: Likhi Hotkeys ---
    if (g_tray1_added) {
        NOTIFYICONDATAW data1 = {0};
        data1.cbSize = sizeof(data1);
        data1.hWnd = g_window;
        data1.uID = 1;
        data1.uFlags = NIF_TIP;
        wcscpy_s(data1.szTip, L"Likhi (\u09b2\u09bf\u0996\u09bf) Hotkeys \u2014 [Ctrl+Alt+Space] [Ctrl+Alt+E]");
        Shell_NotifyIconW(NIM_MODIFY, &data1);
    }

    // --- Tray 2: Likhi Language Pin ---
    if (g_tray2_added) {
        NOTIFYICONDATAW data2 = {0};
        data2.cbSize = sizeof(data2);
        data2.hWnd = g_window;
        data2.uID = 2;
        data2.uFlags = NIF_TIP | NIF_ICON;

        likhi::ModeTarget mode = likhi::ModeIpc::Instance().GetMode();
        HICON new_icon = CreateModeIcon(mode);
        if (new_icon) {
            if (g_tray_pin_icon) DestroyIcon(g_tray_pin_icon);
            g_tray_pin_icon = new_icon;
            data2.hIcon = g_tray_pin_icon;
        }

        const wchar_t* mode_desc = (mode == likhi::ModeTarget::kEnglish) ? L"[EN] English"
                                 : (mode == likhi::ModeTarget::kBanglaToEnglish) ? L"[Tr] Banglish->English \u0985\u09a8\u09c1\u09ac\u09be\u09a6"
                                 : L"[\u09ac\u09be\u0982] \u09ac\u09be\u0982\u09b2\u09be \u09ae\u09cb\u09a1";
        wchar_t tip[128] = {0};
        wsprintfW(tip, L"Likhi Language Pin: %ls\n(\u0995\u09cd\u09b2\u09bf\u0995 \u0995\u09b0\u09c7 \u09ad\u09be\u09b7\u09be \u09aa\u09b0\u09bf\u09ac\u09b0\u09cd\u09a4\u09a8 \u0995\u09b0\u09c1\u09a8)", mode_desc);
        wcsncpy(data2.szTip, tip, 127);
        Shell_NotifyIconW(NIM_MODIFY, &data2);
    }
}

void ShowTrayMenu() {
    HMENU menu = CreatePopupMenu();

    MENUINFO mi = {sizeof(mi)};
    mi.cbSize = sizeof(mi);
    mi.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
    static HBRUSH s_menuBg = CreateSolidBrush(RGB(255, 255, 255));
    mi.hbrBack = s_menuBg;
    SetMenuInfo(menu, &mi);

    // 1. Restore top bar
    AppendMenuW(menu, MF_STRING, 111, L"Restore top bar");

    // 2. Start Voice Typing
    AppendMenuW(menu, MF_STRING, 113, L"Start Voice Typing");

    // 3. Mic Options (Submenu matching circled screenshot)
    HMENU subMic = CreatePopupMenu();
    SetMenuInfo(subMic, &mi);
    AppendMenuW(subMic, MF_STRING, 120, L"Microphone Settings...");
    AppendMenuW(subMic, MF_STRING | MF_CHECKED, 121, L"Default Input Device");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)subMic, L"Mic Options");

    // 4. Typing Mode
    AppendMenuW(menu, MF_STRING | (g_host_enabled ? MF_CHECKED : 0), 115, L"Typing Mode");

    // 5. Typing Options (Submenu)
    HMENU subTyping = CreatePopupMenu();
    SetMenuInfo(subTyping, &mi);
    likhi::ModeTarget curMode = likhi::ModeIpc::Instance().GetMode();
    AppendMenuW(subTyping, MF_STRING | (curMode == likhi::ModeTarget::kBangla ? MF_CHECKED : 0), 107, L"\u09ac\u09be\u0982\u09b2\u09be (Bangla)");
    AppendMenuW(subTyping, MF_STRING | (curMode == likhi::ModeTarget::kEnglish ? MF_CHECKED : 0), 109, L"English");
    AppendMenuW(subTyping, MF_STRING | (curMode == likhi::ModeTarget::kBanglaToEnglish ? MF_CHECKED : 0), 108, L"\u09ac\u09be\u0982\u09b2\u09bf\u09b6 \u2794 \u0987\u0982\u09b0\u09c7\u099c\u09bf \u0985\u09a8\u09c1\u09ac\u09be\u09a6");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)subTyping, L"Typing Options");

    // 6. Word Manager
    AppendMenuW(menu, MF_STRING, 114, L"Word Manager");

    // 7. Options (Submenu)
    HMENU subOptions = CreatePopupMenu();
    SetMenuInfo(subOptions, &mi);
    AppendMenuW(subOptions, MF_STRING, 104, L"Settings...");
    AppendMenuW(subOptions, MF_STRING, 112, L"Reset TopBar Position");
    AppendMenuW(subOptions, MF_STRING, 110, L"On Screen Keyboard");
    if (!g_latest_update_version.empty()) {
        std::wstring upd = L"Check for Update (" + g_latest_update_version + L" available)";
        AppendMenuW(subOptions, MF_STRING, 106, upd.c_str());
    } else {
        AppendMenuW(subOptions, MF_STRING, 106, L"Check for Update");
    }
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)subOptions, L"Options");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    // 8. Restart
    AppendMenuW(menu, MF_STRING, 116, L"Restart");

    // 9. Exit
    AppendMenuW(menu, MF_STRING, 105, L"Exit");

    // Attach colorful icons matching the circled screenshot
    likhi::universal::SetMenuItemIcon(menu, 111, false, likhi::universal::kIconTray);
    likhi::universal::SetMenuItemIcon(menu, 113, false, likhi::universal::kIconMic);
    likhi::universal::SetMenuItemIcon(menu, 116, false, likhi::universal::kIconRestart);
    likhi::universal::SetMenuItemIcon(menu, 105, false, likhi::universal::kIconExit);

    POINT cursor = {0};
    GetCursorPos(&cursor);
    SetForegroundWindow(g_window);
    const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                        cursor.x, cursor.y, 0, g_window, nullptr);
    DestroyMenu(menu);
    if (command == 0) return;

    if (command == 111) {
        g_show_topbar = !g_topbar.IsVisible();
        g_topbar.Show(g_show_topbar);
        if (g_show_topbar) g_topbar.ExpandToFull();
        g_topbar.SaveCoordinates();
    } else if (command == 112) {
        int screen_w = GetSystemMetrics(SM_CXSCREEN);
        g_topbar.SetPosition((screen_w - 262) / 2, 12);
        g_show_topbar = true;
        g_topbar.Show(true);
        g_topbar.ExpandBar();
        g_topbar.SaveCoordinates();
    } else if (command == 110) {
        g_osk.ToggleVisibility();
    } else if (command == 113) {
        g_topbar.LaunchVoiceInput();
    } else if (command == 114) {
        g_topbar.LaunchDictionary();
    } else if (command == 115) {
        g_topbar.ToggleSuggestions();
    } else if (command == 116) {
        wchar_t p[MAX_PATH] = {0};
        GetModuleFileNameW(nullptr, p, MAX_PATH);
        ShellExecuteW(nullptr, L"open", p, nullptr, nullptr, SW_SHOWNORMAL);
        DestroyWindow(g_window);
    } else if (command == 107) {
        likhi::ModeIpc::Instance().SetMode(likhi::ModeTarget::kBangla);
        g_topbar.UpdateMode(likhi::ModeTarget::kBangla);
        UpdateTray();
    } else if (command == 109) {
        likhi::ModeIpc::Instance().SetMode(likhi::ModeTarget::kEnglish);
        g_topbar.UpdateMode(likhi::ModeTarget::kEnglish);
        UpdateTray();
    } else if (command == 108) {
        likhi::ModeIpc::Instance().SetMode(likhi::ModeTarget::kBanglaToEnglish);
        g_topbar.UpdateMode(likhi::ModeTarget::kBanglaToEnglish);
        UpdateTray();
    } else if (command == 104 || command == 120) {
        std::wstring settings = ExeDir() + L"\\bangla_settings.exe";
        if (GetFileAttributesW(settings.c_str()) == INVALID_FILE_ATTRIBUTES) {
            settings = ExeDir() + L"\\..\\release_package\\bangla_settings.exe";
        }
        ShellExecuteW(nullptr, L"open", settings.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    } else if (command == 106) {
        likhi::UpdateService::LaunchOfficialUpdateFlow(g_latest_update_url);
    } else if (command == 105) {
        DestroyWindow(g_window);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == likhi::ModeIpc::GetBroadcastMessage()) {
        bool is_en = (likhi::ModeIpc::Instance().GetMode() == likhi::ModeTarget::kEnglish);
        UpdateTray();
        // ShowTransientBadge(is_en);
        g_topbar.UpdateMode(likhi::ModeIpc::Instance().GetMode());
        return 0;
    }
    if (message == likhi::ModeIpc::GetShowTopBarMessage()) {
        int cur_x = 0, cur_y = 0;
        g_topbar.GetPosition(cur_x, cur_y);
        int screen_w = GetSystemMetrics(SM_CXSCREEN);
        if (cur_x < -50 || cur_x > screen_w - 30) {
            cur_x = (screen_w - 262) / 2;
            cur_y = 12;
            g_topbar.SetPosition(cur_x, cur_y);
        }
        g_show_topbar = true;
        g_topbar.Show(true);
        g_topbar.ExpandBar();
        g_topbar.SetAutoHide(false);
        g_topbar.SaveCoordinates();
        HWND hTopBar = g_topbar.GetHwnd();
        if (hTopBar) {
            SetWindowPos(hTopBar, HWND_TOPMOST, cur_x, cur_y, 262, 24, SWP_SHOWWINDOW);
            ShowWindow(hTopBar, SW_SHOWNORMAL);
            BringWindowToTop(hTopBar);
            SetForegroundWindow(hTopBar);
        }
        return 0;
    }
    if (message == likhi::ModeIpc::GetResetTopBarMessage()) {
        int screen_w = GetSystemMetrics(SM_CXSCREEN);
        g_topbar_x = (screen_w - 262) / 2;
        g_topbar_y = 12;
        g_topbar.SetPosition(g_topbar_x, g_topbar_y);
        g_show_topbar = true;
        g_topbar.Show(true);
        g_topbar.ExpandBar();
        g_topbar.SetAutoHide(false);
        g_topbar.SaveCoordinates();
        HWND hTopBar = g_topbar.GetHwnd();
        if (hTopBar) {
            SetWindowPos(hTopBar, HWND_TOPMOST, g_topbar_x, g_topbar_y, 262, 24, SWP_SHOWWINDOW);
            ShowWindow(hTopBar, SW_SHOWNORMAL);
            BringWindowToTop(hTopBar);
            SetForegroundWindow(hTopBar);
        }
        return 0;
    }

    switch (message) {

        case WM_HOTKEY:
            if (wparam == kHotkeyToggle) {
                g_host_enabled = !g_host_enabled;
                g_typing.SetEnabled(g_host_enabled && g_mode != InputMode::kTsfOnly);
                Log(g_host_enabled ? "hotkey: resumed" : "hotkey: paused");
                UpdateTray();
            } else if (wparam == kHotkeyToggleSpace) {
                TriggerModeToggle();
            } else if (wparam == kHotkeyToggleConversion) {
                TriggerTranslationToggle();
            } else if (wparam == kHotkeyToggleVoice) {
                g_topbar.LaunchVoiceInput();
            }
            return 0;
        case kTrayMessage: // Tray Icon 1: Likhi Hotkeys
            if (LOWORD(lparam) == WM_LBUTTONUP) {
                if (!g_topbar.IsVisible()) {
                    g_show_topbar = true;
                    g_topbar.Show(true);
                    g_topbar.ExpandToFull();
                    g_topbar.SaveCoordinates();
                }
                HWND hTb = g_topbar.GetHwnd();
                if (hTb) {
                    SetWindowPos(hTb, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
                    SetForegroundWindow(hTb);
                }
            } else if (LOWORD(lparam) == WM_LBUTTONDBLCLK) {
                g_show_topbar = !g_topbar.IsVisible();
                g_topbar.Show(g_show_topbar);
                if (g_show_topbar) g_topbar.ExpandToFull();
                g_topbar.SaveCoordinates();
            } else if (LOWORD(lparam) == WM_RBUTTONUP || LOWORD(lparam) == WM_CONTEXTMENU) {
                ShowTrayMenu();
            } else if (LOWORD(lparam) == 0x0405 /* NIN_BALLOONUSERCLICK */) {
                likhi::UpdateService::LaunchOfficialUpdateFlow(g_latest_update_url);
            }
            return 0;
        case kTrayPinMessage: // Tray Icon 2: Language Pin
            if (LOWORD(lparam) == WM_LBUTTONUP) {
                TriggerModeToggle();
            } else if (LOWORD(lparam) == WM_RBUTTONUP || LOWORD(lparam) == WM_CONTEXTMENU) {
                ShowLanguagePinMenu();
            }
            return 0;
        case kMsgUpdateCheckResult: {
            likhi::UpdateCheckResult res = static_cast<likhi::UpdateCheckResult>(wparam);
            likhi::ReleaseInfo* pInfo = reinterpret_cast<likhi::ReleaseInfo*>(lparam);
            if (res == likhi::UpdateCheckResult::kUpdateAvailable && pInfo) {
                g_latest_update_version = std::wstring(pInfo->version.begin(), pInfo->version.end());
                g_latest_update_url = pInfo->update_page_url;
                std::wstring msg = L"Likhi এর একটি নতুন সংস্করণ (" + g_latest_update_version +
                                   L") পাওয়া গেছে!\n\nআপনি কি এখনই নতুন সংস্করণটি ডাউনলোড ও আপডেট করতে চান?";
                int choice = MessageBoxW(g_window, msg.c_str(), L"Likhi Update (লিখি আপডেট)",
                                         MB_YESNO | MB_ICONINFORMATION | MB_TOPMOST);
                if (choice == IDYES) {
                    likhi::UpdateService::LaunchOfficialUpdateFlow(g_latest_update_url);
                }

            }
            if (pInfo) delete pInfo;
            return 0;
        }
        case WM_TIMER:
            if (wparam == kTimerReloadSettings) {
                ResetIfFocusChanged();
                LoadSettings();
                UpdateTray();
            } else if (wparam == kTimerBackgroundUpdateCheck) {
                KillTimer(hwnd, kTimerBackgroundUpdateCheck);
                if (g_auto_check_update && likhi::UpdateService::ShouldCheckOnStartup()) {
                    std::thread([hwnd]() {
                        likhi::ReleaseInfo* pInfo = new likhi::ReleaseInfo();
                        likhi::UpdateCheckResult res = likhi::UpdateService::CheckForUpdate(*pInfo, false);
                        PostMessageW(hwnd, kMsgUpdateCheckResult, static_cast<WPARAM>(res), reinterpret_cast<LPARAM>(pInfo));
                    }).detach();
                }
            }
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

// --- diagnostics -------------------------------------------------------------
// The host is a GUI-subsystem process, so a doubly-safe report path is used:
// everything is collected in memory, written to universal_check.txt and also
// pushed to stdout when a console could be borrowed.
std::string g_report;

void Report(const char* format, ...) {
    char buffer[1024] = {0};
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    g_report += buffer;
}

int RunCheck() {
    Report("LIKHI UNIVERSAL MODE - DIAGNOSTICS\n");
    Report("==================================\n\n");

    Report("[engine]\n");
    const std::string lexicon = FindLexicon(ExeDir());
    Report("  lexicon            : %s\n", lexicon.empty() ? "(not found!)" : lexicon.c_str());
    Report("  user dictionary    : %s\\user_dict.txt (shared with TSF)\n",
           NarrowUtf8(AppDataDir()).c_str());
    Report("  engine created     : %s\n", g_engine ? "yes" : "NO");
    if (g_engine) {
        const char* words[] = {"ami", "poriborton", "somossa", "office", "sakkho"};
        for (const char* word : words) {
            BanglaEngine_SetComposition(g_engine, word);
            CandidateList list;
            BanglaEngine_GetCandidates(g_engine, &list);
            const std::wstring best =
                list.count > 0 ? WidenUtf8(list.candidates[0].bengali_text) : L"";
            Report("  %-12s -> %s\n", word, NarrowUtf8(best).c_str());
        }
    }

    Report("\n[focused window]\n");
    const std::wstring app = ForegroundProcessName();
    const HKL layout = ForegroundLayout();
    Report("  process             : %s\n", app.empty() ? "(none)" : NarrowUtf8(app).c_str());
    Report("  keyboard layout     : 0x%04X %s\n",
           layout ? (unsigned)LOWORD((UINT_PTR)layout) : 0u,
           ForegroundIsBengali() ? "(Bengali - Likhi selected)" : "(not Bengali)");
    Report("  bangla_tsf.dll there: %s\n", TsfLoadedInForegroundProcess() ? "yes (TSF in charge)" : "no (universal can help)");

    Report("\n[policy]\n");
    Report("  host enabled        : %s\n", g_host_enabled ? "yes" : "no");
    Report("  input mode          : %s\n", g_mode == InputMode::kTsfOnly ? "tsf_only"
                                             : g_mode == InputMode::kUniversalOnly ? "universal"
                                                                                   : "automatic");
    Report("  would handle keys   : %s\n", ShouldHandleNow() ? "YES" : "no");

    Report("\n[hook]\n");
    HHOOK probe = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, g_instance, 0);
    Report("  keyboard hook test  : %s\n", probe ? "installed OK" : "FAILED");
    if (probe) UnhookWindowsHookEx(probe);
    Report("\nNo hook is left installed by --check.\n");
    const std::wstring dir = AppDataDir();
    if (!dir.empty()) {
        std::ofstream out((dir + L"\\universal_check.txt").c_str(), std::ios::binary);
        if (out) out << g_report;
    }
    fputs(g_report.c_str(), stdout);
    fflush(stdout);
    return 0;
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR command_line, int) {
    g_instance = instance;

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    // If running from Program Files and a newer user-level build exists in AppData, delegate to it!
    wchar_t exePathPf[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, exePathPf, MAX_PATH);
    if (wcsstr(exePathPf, L"Program Files") != nullptr) {
        wchar_t appdataDir[MAX_PATH] = {0};
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appdataDir))) {
            std::wstring adUni = std::wstring(appdataDir) + L"\\PC-Bangla-Typing-App\\likhi_universal.exe";
            WIN32_FILE_ATTRIBUTE_DATA pfData = {0}, adData = {0};
            if (GetFileAttributesExW(exePathPf, GetFileExInfoStandard, &pfData) &&
                GetFileAttributesExW(adUni.c_str(), GetFileExInfoStandard, &adData)) {
                if (CompareFileTime(&adData.ftLastWriteTime, &pfData.ftLastWriteTime) > 0) {
                    ShellExecuteW(nullptr, L"open", adUni.c_str(), command_line, nullptr, SW_SHOW);
                    return 0;
                }
            }
        }
    }

    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    // One instance only: two hooks would fight over the same word.
    HANDLE single = CreateMutexW(nullptr, FALSE, L"Local\\LikhiUniversalMode");
    const bool already_running = single && GetLastError() == ERROR_ALREADY_EXISTS;

    LoadSettings();
    InitEngine();

    if (command_line && wcsstr(command_line, L"--check")) {
        // A GUI-subsystem process has no console of its own: borrow the parent's
        // so --check can print its report when started from a terminal.
        if (AttachConsole(ATTACH_PARENT_PROCESS)) {
            FILE* dummy = nullptr;
            freopen_s(&dummy, "CONOUT$", "w", stdout);
            SetConsoleOutputCP(CP_UTF8);
        }
        const int result = RunCheck();
        if (g_engine) BanglaEngine_Destroy(g_engine);
        if (single) CloseHandle(single);
    if (gdiplusToken) {
        Gdiplus::GdiplusShutdown(gdiplusToken);
    }
        return result;
    }

    if (already_running) {
        Log("another universal host is already running - requesting topbar show");
        HWND hRunning = FindWindowW(kWindowClass, L"Likhi Universal Mode");
        if (hRunning) {
            PostMessageW(hRunning, likhi::ModeIpc::GetShowTopBarMessage(), 0, 0);
        }
        HWND hTopBar = FindWindowW(L"LikhiFloatingTopBar", nullptr);
        if (hTopBar) {
            int screen_w = GetSystemMetrics(SM_CXSCREEN);
            PostMessageW(hTopBar, likhi::ModeIpc::GetShowTopBarMessage(), 0, 0);
            SetWindowPos(hTopBar, HWND_TOPMOST, (screen_w - 262) / 2, 12, 262, 24, SWP_SHOWWINDOW);
            ShowWindow(hTopBar, SW_SHOWNORMAL);
            BringWindowToTop(hTopBar);
            SetForegroundWindow(hTopBar);
            if (g_engine) BanglaEngine_Destroy(g_engine);
            if (single) CloseHandle(single);
            if (gdiplusToken) Gdiplus::GdiplusShutdown(gdiplusToken);
            return 0;
        }

        // Wait up to 500ms to see if TopBar window appears
        for (int i = 0; i < 5; ++i) {
            Sleep(100);
            hTopBar = FindWindowW(L"LikhiFloatingTopBar", nullptr);
            if (hTopBar) {
                int screen_w = GetSystemMetrics(SM_CXSCREEN);
                PostMessageW(hTopBar, likhi::ModeIpc::GetShowTopBarMessage(), 0, 0);
                SetWindowPos(hTopBar, HWND_TOPMOST, (screen_w - 262) / 2, 12, 262, 24, SWP_SHOWWINDOW);
                ShowWindow(hTopBar, SW_SHOWNORMAL);
                BringWindowToTop(hTopBar);
                SetForegroundWindow(hTopBar);
                if (g_engine) BanglaEngine_Destroy(g_engine);
                if (single) CloseHandle(single);
                if (gdiplusToken) Gdiplus::GdiplusShutdown(gdiplusToken);
                return 0;
            }
        }

        // No topbar exists! The previously running instance is headless or stuck.
        // Terminate any orphaned likhi_universal.exe instance and take over!
        DWORD myPid = GetCurrentProcessId();
        HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hSnap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe = { sizeof(pe) };
            if (Process32FirstW(hSnap, &pe)) {
                do {
                    if (pe.th32ProcessID != myPid && _wcsicmp(pe.szExeFile, L"likhi_universal.exe") == 0) {
                        HANDLE hP = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                        if (hP) {
                            TerminateProcess(hP, 0);
                            CloseHandle(hP);
                        }
                    }
                } while (Process32NextW(hSnap, &pe));
            }
            CloseHandle(hSnap);
        }
        Sleep(100);
        // Continue startup cleanly as active host!
    }

    WNDCLASSEXW window_class = {0};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = instance;
    window_class.lpszClassName = kWindowClass;
    RegisterClassExW(&window_class);

    // Create top-level invisible message window (parent nullptr) so PostMessage(HWND_BROADCAST)
    // is delivered properly. (HWND_MESSAGE windows never receive broadcast messages).
    g_window = CreateWindowExW(0, kWindowClass, L"Likhi Universal Mode", WS_POPUP, 0, 0, 0, 0,
                               nullptr, nullptr, instance, nullptr);
    if (!g_window) {
        if (g_engine) BanglaEngine_Destroy(g_engine);
        return 1;
    }

    // Bypass User Interface Privilege Isolation (UIPI) so non-elevated apps can notify this window
    typedef BOOL (WINAPI *ChangeWindowMessageFilterExFn)(HWND, UINT, DWORD, PCHANGEFILTERSTRUCT);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        auto pChangeFilter = (ChangeWindowMessageFilterExFn)GetProcAddress(user32, "ChangeWindowMessageFilterEx");
        if (pChangeFilter) {
            pChangeFilter(g_window, likhi::ModeIpc::GetBroadcastMessage(), 1 /* MSGFLT_ALLOW */, nullptr);
            pChangeFilter(g_window, likhi::ModeIpc::GetShowTopBarMessage(), 1 /* MSGFLT_ALLOW */, nullptr);
            pChangeFilter(g_window, likhi::ModeIpc::GetResetTopBarMessage(), 1 /* MSGFLT_ALLOW */, nullptr);
            pChangeFilter(g_window, kTrayMessage, 1 /* MSGFLT_ALLOW */, nullptr);
            pChangeFilter(g_window, kTrayPinMessage, 1 /* MSGFLT_ALLOW */, nullptr);
            pChangeFilter(g_window, WM_COMMAND, 1 /* MSGFLT_ALLOW */, nullptr);
        }
    }

    // Tray 1: Likhi Hotkeys
    NOTIFYICONDATAW tray1 = {0};
    tray1.cbSize = sizeof(tray1);
    tray1.hWnd = g_window;
    tray1.uID = 1;
    tray1.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    tray1.uCallbackMessage = kTrayMessage;
    g_tray_hotkey_icon = CreateHotkeyTrayIcon();
    tray1.hIcon = g_tray_hotkey_icon ? g_tray_hotkey_icon : LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(tray1.szTip, L"Likhi (\u09b2\u09bf\u0996\u09bf) Hotkeys \u2014 [Ctrl+Alt+Space] [Ctrl+Alt+E]");
    g_tray1_added = Shell_NotifyIconW(NIM_ADD, &tray1) != FALSE;

    // Tray 2: Likhi Language Pin
    NOTIFYICONDATAW tray2 = {0};
    tray2.cbSize = sizeof(tray2);
    tray2.hWnd = g_window;
    tray2.uID = 2;
    tray2.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    tray2.uCallbackMessage = kTrayPinMessage;
    likhi::ModeTarget initial_mode = likhi::ModeIpc::Instance().GetMode();
    g_tray_pin_icon = CreateModeIcon(initial_mode);
    tray2.hIcon = g_tray_pin_icon ? g_tray_pin_icon : LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(tray2.szTip, L"Likhi Language Pin: [\u09ac\u09be\u0982] \u09ac\u09be\u0982\u09b2\u09be");
    g_tray2_added = Shell_NotifyIconW(NIM_ADD, &tray2) != FALSE;

    UpdateTray();
    RegisterHotKey(g_window, kHotkeyToggle, MOD_CONTROL | MOD_ALT, 'L');
    RegisterHotKey(g_window, kHotkeyToggleConversion, MOD_CONTROL | MOD_ALT, 'E');
    RegisterHotKey(g_window, kHotkeyToggleSpace, MOD_CONTROL | MOD_ALT, VK_SPACE);
    RegisterHotKey(g_window, kHotkeyToggleVoice, MOD_CONTROL | MOD_ALT, 'V');
    g_translation_service.SetConsentEnabled(g_english_translation_enabled);
    g_translation_service.Start();

    // Ensure autostart entry in HKCU Run so Likhi launches on Windows login
    wchar_t current_exe[MAX_PATH] = {0};
    if (GetModuleFileNameW(nullptr, current_exe, MAX_PATH)) {
        HKEY hRunKey = nullptr;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr, 0,
                            KEY_SET_VALUE, nullptr, &hRunKey, nullptr) == ERROR_SUCCESS) {
            std::wstring quoted = L"\"" + std::wstring(current_exe) + L"\"";
            RegSetValueExW(hRunKey, L"LikhiUniversal", 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(quoted.c_str()),
                           (DWORD)((quoted.size() + 1) * sizeof(wchar_t)));
            RegCloseKey(hRunKey);
        }
    }

    SetTimer(g_window, kTimerReloadSettings, kSettingsPollMs, nullptr);
    if (g_auto_check_update) {
        SetTimer(g_window, kTimerBackgroundUpdateCheck, kUpdateCheckDelayMs, nullptr);
    }

    g_osk.Initialize(instance, nullptr);
    g_topbar.SetVirtualKeyboard(&g_osk);
    g_topbar.Initialize(instance, nullptr);
    g_candidate_window.Initialize(instance);
    g_candidate_window.SetSelectionCallback([](size_t index) {
        CommitCandidateByIndex(index);
    });
    g_candidate_window.SetConversionModeCallback([](bangla_tsf::CandidateWindow::ConversionTarget mode) {
        if (mode == bangla_tsf::CandidateWindow::ConversionTarget::kEnglish) {
            likhi::ModeIpc::Instance().SetMode(likhi::ModeTarget::kBanglaToEnglish);
        } else {
            likhi::ModeIpc::Instance().SetMode(likhi::ModeTarget::kBangla);
        }
        likhi::ModeTarget m = likhi::ModeIpc::Instance().GetMode();
        g_topbar.UpdateMode(m);
        UpdateTray();
    });
    int cur_x = 0, cur_y = 0;
    g_topbar.GetPosition(cur_x, cur_y);
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    if (cur_x < -50 || cur_x > screen_w - 30) {
        cur_x = (screen_w - 262) / 2;
        cur_y = 12;
        g_topbar.SetPosition(cur_x, cur_y);
    }
    // If Kontho is active at the screen top and Likhi is at default y, shift below Kontho
    HWND hKontho = FindWindowW(nullptr, L"Nms Voice pad");
    if (!hKontho) hKontho = FindWindowW(nullptr, L"Kontho");
    if (hKontho && cur_y <= 12) {
        RECT rk = {0};
        GetWindowRect(hKontho, &rk);
        if (rk.top <= 10 && rk.bottom > 10) {
            cur_y = rk.bottom + 6;
            g_topbar.SetPosition(cur_x, cur_y);
        }
    }
    g_show_topbar = true;
    g_topbar.Show(true);
    g_topbar.ExpandBar();

    g_keyboard_hook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, instance, 0);
    g_mouse_hook = SetWindowsHookExW(WH_MOUSE_LL, MouseHookProc, instance, 0);
    Log("universal host started (hook=" + std::string(g_keyboard_hook ? "ok" : "FAILED") + ")");
    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (g_keyboard_hook) UnhookWindowsHookEx(g_keyboard_hook);
    if (g_mouse_hook) UnhookWindowsHookEx(g_mouse_hook);
    if (g_tray1_added) {
        NOTIFYICONDATAW d1 = {0}; d1.cbSize = sizeof(d1); d1.hWnd = g_window; d1.uID = 1;
        Shell_NotifyIconW(NIM_DELETE, &d1);
    }
    if (g_tray2_added) {
        NOTIFYICONDATAW d2 = {0}; d2.cbSize = sizeof(d2); d2.hWnd = g_window; d2.uID = 2;
        Shell_NotifyIconW(NIM_DELETE, &d2);
    }
    if (g_tray_hotkey_icon) {
        DestroyIcon(g_tray_hotkey_icon);
        g_tray_hotkey_icon = nullptr;
    }
    if (g_tray_pin_icon) {
        DestroyIcon(g_tray_pin_icon);
        g_tray_pin_icon = nullptr;
    }
    g_candidate_window.Destroy();
    g_osk.Destroy();
    g_topbar.Destroy();
    if (g_badge_window) {
        DestroyWindow(g_badge_window);
        g_badge_window = nullptr;
    }
    UnregisterHotKey(g_window, kHotkeyToggle);
    UnregisterHotKey(g_window, kHotkeyToggleConversion);
    UnregisterHotKey(g_window, kHotkeyToggleSpace);
    UnregisterHotKey(g_window, kHotkeyToggleVoice);
    g_translation_service.Stop();
    KillTimer(g_window, kTimerReloadSettings);
    if (g_engine) BanglaEngine_Destroy(g_engine);
    if (single) CloseHandle(single);
    Log("universal host stopped");
    return 0;
}
