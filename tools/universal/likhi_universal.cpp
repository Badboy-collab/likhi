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
const int kHotkeyToggle = 0xA1;
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
bool g_tray_added = false;

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
    // Never transform anything unless the user really selected Bengali input for
    // this window: English typing in a chat must stay English.
    if (!ForegroundIsBengali()) return false;
    if (g_mode == InputMode::kTsfOnly) return false;
    if (g_mode == InputMode::kUniversalOnly) return true;
    return !TsfLoadedInForegroundProcess();
}

void ResetIfFocusChanged() {
    HWND foreground = GetForegroundWindow();
    if (foreground != g_last_foreground) {
        g_last_foreground = foreground;
        g_typing.Reset();  // the caret belongs to another window now
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

// Performs one word replacement: erase the roman run, type the Bengali word,
// then type the boundary character. When the engine has no Bengali answer the
// erasure is skipped entirely and only the boundary character is sent, so the
// user's own text is never damaged.
void ApplyReplace(const Decision& decision) {
    const std::wstring bengali = Transliterate(decision.roman);
    std::vector<INPUT> inputs;
    if (!bengali.empty()) {
        AppendBackspaces(inputs, decision.erase_chars);
        AppendUnicode(inputs, bengali);
    }
    AppendUnicode(inputs, decision.trailing);
    if (!inputs.empty()) {
        SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));
    }
    if (!bengali.empty() && g_engine) {
        // Same personalisation file as the TSF path: learning in WhatsApp is
        // visible in Word and the other way round.
        BanglaEngine_LearnWord(g_engine, NarrowUtf8(decision.roman).c_str(),
                               NarrowUtf8(bengali).c_str());
    }
}

LRESULT CALLBACK KeyboardHookProc(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION && (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN)) {
        const KBDLLHOOKSTRUCT* key = reinterpret_cast<KBDLLHOOKSTRUCT*>(lparam);
        // Never react to our own synthetic keystrokes.
        if (!(key->flags & LLKHF_INJECTED)) {
            ResetIfFocusChanged();
            if (ShouldHandleNow()) {
                const wchar_t ch = CharacterFor(key->vkCode, key->scanCode);
                const KeyEvent event = ClassifyKey(
                    (int)key->vkCode, ch, ModifierDown(VK_CONTROL), ModifierDown(VK_MENU),
                    ModifierDown(VK_LWIN) || ModifierDown(VK_RWIN), ModifierDown(VK_SHIFT),
                    false);
                const Decision decision = g_typing.Feed(event);
                if (decision.kind == ActionKind::kReplace) {
                    ApplyReplace(decision);
                    return 1;  // consumed: we type the word and the boundary
                }
            } else {
                g_typing.Reset();
            }
        }
    }
    return CallNextHookEx(nullptr, code, wparam, lparam);
}

// A mouse click moves the caret, which invalidates the preview. The hook only
// observes: it never consumes a mouse event.
LRESULT CALLBACK MouseHookProc(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION) {
        switch (wparam) {
            case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN:
            case WM_MOUSEWHEEL:  case WM_MOUSEHWHEEL:
                g_typing.Reset();
                break;
            default:
                break;
        }
    }
    return CallNextHookEx(nullptr, code, wparam, lparam);
}

// --- tray --------------------------------------------------------------------
void UpdateTray() {
    if (!g_window || !g_tray_added) return;
    NOTIFYICONDATAW data = {0};
    data.cbSize = sizeof(data);
    data.hWnd = g_window;
    data.uID = 1;
    data.uFlags = NIF_TIP;
    const wchar_t* mode_text = g_mode == InputMode::kTsfOnly      ? L"TSF only"
                              : g_mode == InputMode::kUniversalOnly ? L"Universal only"
                                                                    : L"Automatic";
    if (!g_host_enabled) mode_text = L"paused";
    wchar_t tip[128] = {0};
    wsprintfW(tip, L"Likhi Universal Mode - %ls", mode_text);
    wcsncpy(data.szTip, tip, 127);
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

void ShowTrayMenu() {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING | (g_host_enabled ? MF_CHECKED : 0), 100,
                L"Universal Mode active");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (g_mode == InputMode::kAutomatic ? MF_CHECKED : 0),
                101, L"Automatic (TSF first)");
    AppendMenuW(menu, MF_STRING | (g_mode == InputMode::kTsfOnly ? MF_CHECKED : 0),
                102, L"TSF only");
    AppendMenuW(menu, MF_STRING | (g_mode == InputMode::kUniversalOnly ? MF_CHECKED : 0),
                103, L"Universal only");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    if (!g_latest_update_version.empty()) {
        std::wstring upd_text = L"🚀 Likhi " + g_latest_update_version + L" উপলব্ধ (Update Available)...";
        AppendMenuW(menu, MF_STRING, 106, upd_text.c_str());
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    }
    AppendMenuW(menu, MF_STRING, 104, L"Likhi Settings...");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, 105, L"Exit");

    POINT cursor = {0};
    GetCursorPos(&cursor);
    SetForegroundWindow(g_window);
    const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON,
                                        cursor.x, cursor.y, 0, g_window, nullptr);
    DestroyMenu(menu);
    if (command == 0) return;

    if (command == 100) {
        g_host_enabled = !g_host_enabled;
        g_typing.SetEnabled(g_host_enabled && g_mode != InputMode::kTsfOnly);
        Log(g_host_enabled ? "universal mode resumed" : "universal mode paused");
        UpdateTray();
    } else if (command >= 101 && command <= 103) {
        g_mode = command == 101 ? InputMode::kAutomatic
               : command == 102 ? InputMode::kTsfOnly
                                : InputMode::kUniversalOnly;
        g_typing.SetEnabled(g_host_enabled && g_mode != InputMode::kTsfOnly);
        Log(g_mode == InputMode::kAutomatic  ? "mode = automatic"
            : g_mode == InputMode::kTsfOnly  ? "mode = tsf only"
                                             : "mode = universal only");
        UpdateTray();
    } else if (command == 104) {
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
    switch (message) {

        case WM_HOTKEY:
            if (wparam == kHotkeyToggle) {
                g_host_enabled = !g_host_enabled;
                g_typing.SetEnabled(g_host_enabled && g_mode != InputMode::kTsfOnly);
                Log(g_host_enabled ? "hotkey: resumed" : "hotkey: paused");
                UpdateTray();
            }
            return 0;
        case kTrayMessage:
            if (LOWORD(lparam) == WM_RBUTTONUP || LOWORD(lparam) == WM_CONTEXTMENU) {
                ShowTrayMenu();
            } else if (LOWORD(lparam) == 0x0405 /* NIN_BALLOONUSERCLICK */) {
                likhi::UpdateService::LaunchOfficialUpdateFlow(g_latest_update_url);
            }
            return 0;
        case kMsgUpdateCheckResult: {
            likhi::UpdateCheckResult res = static_cast<likhi::UpdateCheckResult>(wparam);
            likhi::ReleaseInfo* pInfo = reinterpret_cast<likhi::ReleaseInfo*>(lparam);
            if (res == likhi::UpdateCheckResult::kUpdateAvailable && pInfo) {
                g_latest_update_version = std::wstring(pInfo->version.begin(), pInfo->version.end());
                g_latest_update_url = pInfo->update_page_url;
                if (g_window && g_tray_added) {
                    NOTIFYICONDATAW nid = {0};
                    nid.cbSize = sizeof(nid);
                    nid.hWnd = g_window;
                    nid.uID = 1;
                    nid.uFlags = NIF_INFO;
                    nid.dwInfoFlags = NIIF_INFO;
                    wcsncpy(nid.szInfoTitle, L"Likhi (লিখি) আপডেট উপলব্ধ", 63);
                    std::wstring balloon = L"Likhi-এর নতুন সংস্করণ (" + g_latest_update_version + L") পাওয়া গেছে। ক্লিক করে ডাউনলোড করুন।";
                    wcsncpy(nid.szInfo, balloon.c_str(), 255);
                    Shell_NotifyIconW(NIM_MODIFY, &nid);
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
                if (likhi::UpdateService::ShouldCheckOnStartup()) {
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
        return result;
    }

    if (already_running) {
        Log("another universal host is already running - exiting");
        if (g_engine) BanglaEngine_Destroy(g_engine);
        if (single) CloseHandle(single);
        return 0;
    }

    WNDCLASSEXW window_class = {0};
    window_class.cbSize = sizeof(window_class);
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = instance;
    window_class.lpszClassName = kWindowClass;
    RegisterClassExW(&window_class);

    g_window = CreateWindowExW(0, kWindowClass, L"Likhi Universal Mode", 0, 0, 0, 0, 0,
                               HWND_MESSAGE, nullptr, instance, nullptr);
    if (!g_window) {
        if (g_engine) BanglaEngine_Destroy(g_engine);
        return 1;
    }

    NOTIFYICONDATAW tray = {0};
    tray.cbSize = sizeof(tray);
    tray.hWnd = g_window;
    tray.uID = 1;
    tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    tray.uCallbackMessage = kTrayMessage;
    tray.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(tray.szTip, L"Likhi Universal Mode");
    g_tray_added = Shell_NotifyIconW(NIM_ADD, &tray) != FALSE;
    RegisterHotKey(g_window, kHotkeyToggle, MOD_CONTROL | MOD_ALT, 'L');
    SetTimer(g_window, kTimerReloadSettings, kSettingsPollMs, nullptr);
    SetTimer(g_window, kTimerBackgroundUpdateCheck, kUpdateCheckDelayMs, nullptr);

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
    if (g_tray_added) Shell_NotifyIconW(NIM_DELETE, &tray);
    KillTimer(g_window, kTimerReloadSettings);
    if (g_engine) BanglaEngine_Destroy(g_engine);
    if (single) CloseHandle(single);
    Log("universal host stopped");
    return 0;
}
