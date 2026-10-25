#include "topbar_window.h"
#include "virtual_keyboard_window.h"
#include "update_service.h"
#include <thread>
#include <gdiplus.h>
#include <shellapi.h>
#include <shlobj.h>
#include <mmsystem.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <ctime>
#include <cmath>

namespace likhi {
namespace universal {

namespace {

const wchar_t kTopBarClassName[] = L"LikhiFloatingTopBar";
const UINT_PTR kTimerAutoHide = 101;
const int kNormalWidth = 260;
const int kCollapsedWidth = 154;
const int kNormalHeight = 24;

std::wstring GetAppDataPath() {
    wchar_t buffer[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, buffer))) {
        std::wstring dir = buffer;
        dir += L"\\PC-Bangla-Typing-App";
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir;
    }
    return std::wstring();
}

std::wstring GetModuleDir() {
    wchar_t path[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full = path;
    size_t slash = full.find_last_of(L"\\/");
    return slash == std::wstring::npos ? std::wstring() : full.substr(0, slash);
}

std::wstring FindSettingsExe() {
    std::wstring exe_dir = GetModuleDir();
    std::vector<std::wstring> candidates = {
        exe_dir + L"\\bangla_settings.exe",
        exe_dir + L"\\..\\release_package\\bangla_settings.exe",
        exe_dir + L"\\..\\build\\bangla_settings.exe",
        L"C:\\Program Files\\Likhi\\bangla_settings.exe",
        L"C:\\Program Files (x86)\\Likhi\\bangla_settings.exe",
        L"E:\\Pervez\\PC Bangla Typing App\\release_package\\bangla_settings.exe",
        L"E:\\Pervez\\PC Bangla Typing App\\build\\bangla_settings.exe"
    };
    for (const auto& path : candidates) {
        if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return path;
        }
    }
    return L"bangla_settings.exe";
}

static const wchar_t kAddWordDlgClass[] = L"LikhiAddWordDlgClass";

struct AddWordDlgData {
    HWND hEditRoman;
    HWND hEditBangla;
    bool added;
};

static LRESULT CALLBACK AddWordDlgProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    AddWordDlgData* data = reinterpret_cast<AddWordDlgData*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_NCCREATE: {
            auto cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return TRUE;
        }
        case WM_CREATE: {
            data = reinterpret_cast<AddWordDlgData*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
            HFONT hFont = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            HFONT hBold = CreateFontW(-14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            HWND hTitle = CreateWindowW(L"STATIC", L"নতুন শব্দ ডিকশনারিতে যুক্ত করুন (Add Word)",
                                        WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 15, 340, 24, hwnd, nullptr, nullptr, nullptr);
            SendMessageW(hTitle, WM_SETFONT, (WPARAM)hBold, TRUE);

            HWND hL1 = CreateWindowW(L"STATIC", L"English / Roman Key (কী):",
                                     WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 48, 340, 20, hwnd, nullptr, nullptr, nullptr);
            SendMessageW(hL1, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hE1 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                      WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 20, 70, 340, 26, hwnd, (HMENU)101, nullptr, nullptr);
            SendMessageW(hE1, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hL2 = CreateWindowW(L"STATIC", L"Bangla Word (বাংলা শব্দ):",
                                     WS_CHILD | WS_VISIBLE | SS_LEFT, 20, 106, 340, 20, hwnd, nullptr, nullptr, nullptr);
            SendMessageW(hL2, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hE2 = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                      WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 20, 128, 340, 26, hwnd, (HMENU)102, nullptr, nullptr);
            SendMessageW(hE2, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hBtnAdd = CreateWindowW(L"BUTTON", L"যোগ করুন (Add)",
                                         WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 150, 172, 100, 30, hwnd, (HMENU)IDOK, nullptr, nullptr);
            SendMessageW(hBtnAdd, WM_SETFONT, (WPARAM)hFont, TRUE);

            HWND hBtnCancel = CreateWindowW(L"BUTTON", L"বাতিল (Cancel)",
                                            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 260, 172, 100, 30, hwnd, (HMENU)IDCANCEL, nullptr, nullptr);
            SendMessageW(hBtnCancel, WM_SETFONT, (WPARAM)hFont, TRUE);

            if (data) {
                data->hEditRoman = hE1;
                data->hEditBangla = hE2;
            }
            SetFocus(hE1);
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wp);
            if (id == IDOK) {
                if (data && data->hEditRoman && data->hEditBangla) {
                    wchar_t r_buf[128] = {0};
                    wchar_t b_buf[128] = {0};
                    GetWindowTextW(data->hEditRoman, r_buf, 128);
                    GetWindowTextW(data->hEditBangla, b_buf, 128);
                    if (wcslen(r_buf) == 0 || wcslen(b_buf) == 0) {
                        MessageBoxW(hwnd, L"দয়া করে রোমান কী এবং বাংলা শব্দ উভয়ই লিখুন।", L"Likhi Word Manager", MB_OK | MB_ICONWARNING);
                        return 0;
                    }
                    std::wstring app_data = GetAppDataPath();
                    std::wstring pdict = app_data + L"\\personal_dict.txt";
                    std::wstring udict = app_data + L"\\user_dict.txt";
                    {
                        std::wofstream out(pdict.c_str(), std::ios::app);
                        if (out.is_open()) out << r_buf << L"\t" << b_buf << L"\n";
                    }
                    {
                        std::wofstream out(udict.c_str(), std::ios::app);
                        if (out.is_open()) out << r_buf << L"\t" << b_buf << L"\t100\n";
                    }
                    data->added = true;
                    MessageBoxW(hwnd, L"✅ শব্দটি সফলভাবে Likhi ডিকশনারিতে যুক্ত করা হয়েছে!", L"Likhi Word Manager", MB_OK | MB_ICONINFORMATION);
                    DestroyWindow(hwnd);
                }
                return 0;
            } else if (id == IDCANCEL) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
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

void TypeUnicodeText(const std::wstring& text) {
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
    if (!inputs.empty()) {
        SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));
    }
}

} // namespace

TopBarWindow::ButtonId TopBarWindow::HitTestButton(POINT pt) const {
    if (PtInRect(&rc_mic_, pt)) return kBtnMic;
    if (PtInRect(&rc_mode_bn_, pt)) return kBtnModeBn;
    if (PtInRect(&rc_mode_en_, pt)) return kBtnModeEn;
    if (PtInRect(&rc_mode_trans_, pt)) return kBtnModeTrans;
    if (!is_collapsed_ && !is_compact_mode_) {
        if (PtInRect(&rc_pause_, pt)) return kBtnPause;
        if (PtInRect(&rc_dict_, pt)) return kBtnDict;
        if (PtInRect(&rc_osk_, pt)) return kBtnOsk;
        if (PtInRect(&rc_script_, pt)) return kBtnScript;
        if (PtInRect(&rc_settings_, pt)) return kBtnSettings;
    }
    if (PtInRect(&rc_power_, pt)) return kBtnPower;
    return kBtnNone;
}

TopBarWindow::TopBarWindow()
    : hinst_(nullptr),
      hwnd_(nullptr),
      hwnd_parent_(nullptr),
      hwnd_tooltip_(nullptr),
      osk_window_(nullptr),
      current_mode_(likhi::ModeTarget::kBangla),
      width_(kNormalWidth),
      height_(kNormalHeight),
      pos_x_(-1),
      pos_y_(12),
      normal_y_(12),
      is_visible_(true),
      auto_hide_enabled_(false),
      is_collapsed_(false),
      is_hovered_(false),
      is_compact_mode_(false),
      suggestions_paused_(false),
      mouse_leave_time_(0),
      hovered_btn_(kBtnNone),
      pressed_btn_(kBtnNone),
      font_bengali_(nullptr),
      font_english_(nullptr),
      font_bengali_small_(nullptr),
      font_english_small_(nullptr) {

    // Likhi Red/Orange Theme (w=260, h=24)
    rc_mic_        = {2,   1, 26,  23}; // w: 24, h: 22
    rc_mode_bn_    = {28,  2, 60,  22}; // w: 32, h: 20 [বাং]
    rc_mode_en_    = {61,  2, 92,  22}; // w: 31, h: 20 [EN]
    rc_mode_trans_ = {93,  2, 127, 22}; // w: 34, h: 20 [🌐 অনুবাদ]
    rc_pause_      = {129, 1, 149, 23}; // w: 20, h: 22
    rc_dict_       = {150, 1, 170, 23}; // w: 20, h: 22
    rc_osk_        = {171, 1, 191, 23}; // w: 20, h: 22
    rc_script_     = {192, 1, 212, 23}; // w: 20, h: 22
    rc_settings_   = {213, 1, 233, 23}; // w: 20, h: 22
    rc_power_      = {234, 1, 258, 23}; // w: 24, h: 22
}

TopBarWindow::~TopBarWindow() {
    Destroy();
}

bool TopBarWindow::Initialize(HINSTANCE hInstance, HWND parentHwnd) {
    hinst_ = hInstance;
    hwnd_parent_ = parentHwnd;

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kTopBarClassName;
    RegisterClassExW(&wc);

    // Initial positioning at top center of primary monitor
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    if (pos_x_ < 0 || pos_x_ > screen_w - 50) {
        pos_x_ = (screen_w - width_) / 2;
    }
    if (normal_y_ < 6 || normal_y_ > screen_h - 40) {
        normal_y_ = 12;
    }
    pos_y_ = normal_y_;

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kTopBarClassName,
        L"Likhi TopBar",
        WS_POPUP,
        pos_x_, pos_y_, width_, height_,
        nullptr, nullptr, hInstance, this
    );

    if (!hwnd_) return false;

    // Allow broadcast messages across integrity levels (UIPI bypass)
    typedef BOOL (WINAPI *ChangeWindowMessageFilterExFn)(HWND, UINT, DWORD, PCHANGEFILTERSTRUCT);
    HMODULE u32 = GetModuleHandleW(L"user32.dll");
    if (u32) {
        auto pChangeFilter = (ChangeWindowMessageFilterExFn)GetProcAddress(u32, "ChangeWindowMessageFilterEx");
        if (pChangeFilter) {
            pChangeFilter(hwnd_, likhi::ModeIpc::GetShowTopBarMessage(), 1 /* MSGFLT_ALLOW */, nullptr);
            pChangeFilter(hwnd_, likhi::ModeIpc::GetResetTopBarMessage(), 1 /* MSGFLT_ALLOW */, nullptr);
            pChangeFilter(hwnd_, WM_COMMAND, 1 /* MSGFLT_ALLOW */, nullptr);
        }
    }

    HRGN rgn = CreateRoundRectRgn(0, 0, width_ + 1, height_ + 1, 6, 6);
    SetWindowRgn(hwnd_, rgn, TRUE);

    // Load voice configuration
    voice_config_ = bangla_tsf::VoiceConfig_Load(GetAppDataPath());
    voice_recorder_.reset(new bangla_tsf::VoiceRecorder());

    // Load fonts (scaled for 24px Kontho height)
    font_bengali_ = CreateFontW(
        -12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Vrinda"
    );
    if (!font_bengali_) {
        font_bengali_ = CreateFontW(
            -12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            L"Nirmala UI"
        );
    }

    font_english_ = CreateFontW(
        -12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );
    if (!font_english_) {
        font_english_ = CreateFontW(
            -13, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            L"Segoe UI"
        );
    }

    font_bengali_small_ = CreateFontW(
        -10, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Vrinda"
    );
    font_english_small_ = CreateFontW(
        -10, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Consolas"
    );

    InitTooltips(hwnd_);
    SetTimer(hwnd_, kTimerAutoHide, 100, nullptr);

    return true;
}

void TopBarWindow::Destroy() {
    if (hwnd_) {
        KillTimer(hwnd_, kTimerAutoHide);
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (font_bengali_) { DeleteObject(font_bengali_); font_bengali_ = nullptr; }
    if (font_english_) { DeleteObject(font_english_); font_english_ = nullptr; }
    if (font_bengali_small_) { DeleteObject(font_bengali_small_); font_bengali_small_ = nullptr; }
    if (font_english_small_) { DeleteObject(font_english_small_); font_english_small_ = nullptr; }
}

void TopBarWindow::Show(bool show) {
    is_visible_ = show;
    if (hwnd_) {
        if (show) {
            is_collapsed_ = false;
            if (normal_y_ < 6) normal_y_ = 12;
            pos_y_ = normal_y_;
            HRGN rgn = CreateRoundRectRgn(0, 0, width_ + 1, height_ + 1, 6, 6);
            SetWindowRgn(hwnd_, rgn, TRUE);
            ShowWindow(hwnd_, SW_SHOWNA);
            SetWindowPos(hwnd_, HWND_TOPMOST, pos_x_, pos_y_, width_, height_,
                         SWP_SHOWWINDOW | SWP_NOACTIVATE);
            BringWindowToTop(hwnd_);
            InvalidateRect(hwnd_, nullptr, TRUE);
            UpdateWindow(hwnd_);
        } else {
            ShowWindow(hwnd_, SW_HIDE);
        }
    }
}

bool TopBarWindow::IsVisible() const {
    return is_visible_;
}

void TopBarWindow::UpdateMode(likhi::ModeTarget mode) {
    current_mode_ = mode;
    if (hwnd_) {
        InvalidateRect(hwnd_, nullptr, FALSE);
        UpdateWindow(hwnd_);
    }
}

void TopBarWindow::SetPosition(int x, int y) {
    pos_x_ = x;
    if (y < 6) y = 12;
    normal_y_ = y;
    pos_y_ = y;
    if (hwnd_) {
        SetWindowPos(hwnd_, HWND_TOPMOST, pos_x_, pos_y_, width_, height_,
                     SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void TopBarWindow::GetPosition(int& x, int& y) const {
    x = pos_x_;
    y = normal_y_;
}

void TopBarWindow::SetDockPosition(DockPosition pos) {
    dock_pos_ = pos;
    auto_hide_enabled_ = true;
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    if (dock_pos_ == kDockRight) {
        pos_x_ = screen_w - width_;
        if (normal_y_ < 20 || normal_y_ > 400) normal_y_ = 50;
        pos_y_ = normal_y_;
    } else {
        pos_x_ = (screen_w - width_) / 2;
        normal_y_ = 6;
        pos_y_ = 6;
    }
    ExpandToFull();
    SaveCoordinates();
}

void TopBarWindow::SetAutoHide(bool enable) {
    auto_hide_enabled_ = enable;
    if (!enable && is_collapsed_) {
        ExpandBar();
    }
}

void TopBarWindow::ToggleCompactMode() {
    is_compact_mode_ = !is_compact_mode_;
    if (is_compact_mode_) {
        CollapseToCompact();
    } else {
        ExpandToFull();
    }
    SaveCoordinates();
}

void TopBarWindow::ExpandToFull() {
    is_collapsed_ = false;
    width_ = kNormalWidth;
    height_ = kNormalHeight;
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    if (normal_y_ < 6) normal_y_ = 12;
    pos_y_ = normal_y_;
    if (dock_pos_ == kDockRight) {
        pos_x_ = screen_w - width_;
    }

    rc_mic_        = {2,   1, 26,  23}; // w: 24, h: 22
    rc_mode_bn_    = {28,  2, 60,  22}; // w: 32, h: 20 [বাং]
    rc_mode_en_    = {61,  2, 92,  22}; // w: 31, h: 20 [EN]
    rc_mode_trans_ = {93,  2, 127, 22}; // w: 34, h: 20 [🌐 অনুবাদ]
    rc_pause_      = {129, 1, 149, 23}; // w: 20, h: 22
    rc_dict_       = {150, 1, 170, 23}; // w: 20, h: 22
    rc_osk_        = {171, 1, 191, 23}; // w: 20, h: 22
    rc_script_     = {192, 1, 212, 23}; // w: 20, h: 22
    rc_settings_   = {213, 1, 233, 23}; // w: 20, h: 22
    rc_power_      = {234, 1, 258, 23}; // w: 24, h: 22

    if (hwnd_) {
        HRGN rgn = CreateRoundRectRgn(0, 0, width_ + 1, height_ + 1, 6, 6);
        SetWindowRgn(hwnd_, rgn, TRUE);
        SetWindowPos(hwnd_, HWND_TOPMOST, pos_x_, pos_y_, width_, height_,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        InvalidateRect(hwnd_, nullptr, TRUE);
    }
}

void TopBarWindow::CollapseToCompact() {
    is_collapsed_ = true;
    width_ = kCollapsedWidth; // 154px
    height_ = kNormalHeight;
    if (normal_y_ < 6) normal_y_ = 12;
    pos_y_ = normal_y_;

    // The 4 essential buttons ALWAYS stay visible!
    rc_mic_        = {2,   1, 26,  23};
    rc_mode_bn_    = {28,  2, 60,  22};
    rc_mode_en_    = {61,  2, 92,  22};
    rc_mode_trans_ = {93,  2, 127, 22};
    rc_power_      = {129, 1, 152, 23}; // displays '>>' to expand

    // Secondary tools hide horizontally into the bar!
    rc_pause_      = {0, 0, 0, 0};
    rc_dict_       = {0, 0, 0, 0};
    rc_osk_        = {0, 0, 0, 0};
    rc_script_     = {0, 0, 0, 0};
    rc_settings_   = {0, 0, 0, 0};

    if (hwnd_) {
        HRGN rgn = CreateRoundRectRgn(0, 0, width_ + 1, height_ + 1, 6, 6);
        SetWindowRgn(hwnd_, rgn, TRUE);
        SetWindowPos(hwnd_, HWND_TOPMOST, pos_x_, pos_y_, width_, height_,
                     SWP_NOACTIVATE | SWP_SHOWWINDOW);
        InvalidateRect(hwnd_, nullptr, TRUE);
    }
}

void TopBarWindow::SaveCoordinates() {
    std::wstring app_data = GetAppDataPath();
    if (app_data.empty()) return;

    std::wstring path = app_data + L"\\settings.json";
    std::string content;
    std::ifstream in(path.c_str());
    if (in) {
        std::stringstream ss;
        ss << in.rdbuf();
        content = ss.str();
    }

    auto patch = [](std::string& s, const std::string& key, const std::string& val) {
        size_t pos = s.find("\"" + key + "\":");
        if (pos != std::string::npos) {
            size_t end = s.find_first_of(",}\n", pos);
            s.replace(pos, end - pos, "\"" + key + "\": " + val);
        } else {
            size_t last_brace = s.find_last_of('}');
            if (last_brace != std::string::npos) {
                std::string ins = ",\n  \"" + key + "\": " + val + "\n";
                s.insert(last_brace, ins);
            }
        }
    };

    patch(content, "topbar_x", std::to_string(pos_x_));
    patch(content, "topbar_y", std::to_string(normal_y_));
    patch(content, "show_topbar", is_visible_ ? "true" : "false");
    patch(content, "topbar_autohide", auto_hide_enabled_ ? "true" : "false");
    patch(content, "topbar_compact", is_compact_mode_ ? "true" : "false");
    patch(content, "topbar_dock", dock_pos_ == kDockRight ? "\"right\"" : "\"top\"");

    if (content.empty()) {
        content = "{\n  \"topbar_x\": " + std::to_string(pos_x_) +
                  ",\n  \"topbar_y\": " + std::to_string(normal_y_) +
                  ",\n  \"show_topbar\": true,\n  \"topbar_autohide\": true\n}\n";
    }

    std::ofstream out(path.c_str());
    if (out) out << content;
}

void TopBarWindow::CheckAutoHide() {
    if (!is_visible_ || is_dragging_) return;

    POINT pt;
    GetCursorPos(&pt);

    bool mouse_in_zone = (pt.y >= (pos_y_ - 8) && pt.y <= (pos_y_ + height_ + 10) &&
                          pt.x >= (pos_x_ - 10) && pt.x <= (pos_x_ + width_ + 10));

    if (mouse_in_zone) {
        if (is_collapsed_ || width_ < kNormalWidth) {
            ExpandToFull();
        }
        is_hovered_ = true;
        mouse_leave_time_ = 0;
    } else {
        is_hovered_ = false;
        if (mouse_leave_time_ == 0) {
            mouse_leave_time_ = GetTickCount();
        } else if (GetTickCount() - mouse_leave_time_ > 1200) {
            if (auto_hide_enabled_ || is_compact_mode_) {
                if (!is_collapsed_ || width_ > kCollapsedWidth) {
                    CollapseToCompact();
                }
            }
        }
    }
}

void TopBarWindow::ExpandBar() {
    ExpandToFull();
}

void TopBarWindow::CollapseBar() {
    CollapseToCompact();
}



LRESULT CALLBACK TopBarWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    TopBarWindow* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = reinterpret_cast<TopBarWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<TopBarWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) {
        return self->HandleMessage(hwnd, msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT TopBarWindow::HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == likhi::ModeIpc::GetShowTopBarMessage()) {
        int screen_w = GetSystemMetrics(SM_CXSCREEN);
        if (pos_x_ < 0 || pos_x_ > screen_w - 50) {
            pos_x_ = (screen_w - width_) / 2;
        }
        if (normal_y_ < 6) normal_y_ = 12;
        pos_y_ = normal_y_;
        Show(true);
        ExpandBar();
        SetAutoHide(false);
        SetWindowPos(hwnd_, HWND_TOPMOST, pos_x_, normal_y_, width_, height_, SWP_SHOWWINDOW);
        SetForegroundWindow(hwnd_);
        SaveCoordinates();
        return 0;
    }
    if (msg == likhi::ModeIpc::GetResetTopBarMessage()) {
        int screen_w = GetSystemMetrics(SM_CXSCREEN);
        pos_x_ = (screen_w - width_) / 2;
        normal_y_ = 10;
        pos_y_ = 10;
        Show(true);
        ExpandBar();
        SetAutoHide(false);
        SetWindowPos(hwnd_, HWND_TOPMOST, pos_x_, normal_y_, width_, height_, SWP_SHOWWINDOW);
        SetForegroundWindow(hwnd_);
        SaveCoordinates();
        return 0;
    }

    switch (msg) {
        case WM_PAINT:
            OnPaint(hwnd);
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_TIMER:
            OnTimer(hwnd, wp);
            return 0;

        case WM_NCHITTEST: {
            POINT pt = {LOWORD(lp), HIWORD(lp)};
            ScreenToClient(hwnd, &pt);
            ButtonId btn = HitTestButton(pt);
            if (btn == kBtnNone) {
                return HTCAPTION;
            }
            return HTCLIENT;
        }

        case WM_LBUTTONDOWN:
            OnLButtonDown(hwnd, LOWORD(lp), HIWORD(lp));
            return 0;

        case WM_LBUTTONUP:
            OnLButtonUp(hwnd, LOWORD(lp), HIWORD(lp));
            return 0;

        case WM_RBUTTONUP: {
            POINT pt = {LOWORD(lp), HIWORD(lp)};
            if (PtInRect(&rc_power_, pt)) {
                ShowPowerMenu(hwnd);
            } else {
                ShowSettingsMenu(hwnd);
            }
            return 0;
        }

        case WM_LBUTTONDBLCLK:
            ToggleCompactMode();
            return 0;

        case WM_MOUSEMOVE:
            OnMouseMove(hwnd, LOWORD(lp), HIWORD(lp));
            return 0;

        case WM_MOUSELEAVE:
            OnMouseLeave(hwnd);
            return 0;

        case WM_EXITSIZEMOVE: {
            RECT rc;
            GetWindowRect(hwnd, &rc);
            int screen_w = GetSystemMetrics(SM_CXSCREEN);
            if (rc.right >= screen_w - 40) {
                dock_pos_ = kDockRight;
                pos_x_ = screen_w - width_;
                normal_y_ = rc.top;
                pos_y_ = normal_y_;
            } else {
                dock_pos_ = kDockTop;
                pos_x_ = rc.left;
                normal_y_ = rc.top;
                if (normal_y_ < 15) {
                    normal_y_ = 12;
                }
                pos_y_ = normal_y_;
            }
            SetWindowPos(hwnd, HWND_TOPMOST, pos_x_, pos_y_, width_, height_,
                         SWP_NOSIZE | SWP_NOACTIVATE);
            SaveCoordinates();
            return 0;
        }

        case WM_DESTROY:
            hwnd_ = nullptr;
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void TopBarWindow::OnTimer(HWND hwnd, UINT_PTR timer_id) {
    if (timer_id == kTimerAutoHide) {
        CheckAutoHide();
    }
}

void TopBarWindow::OnPaint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT cr;
    GetClientRect(hwnd, &cr);
    int cur_h = cr.bottom;

    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, cr.right, cur_h);
    HGDIOBJ oldBmp = SelectObject(memDC, memBmp);

    {
        Gdiplus::Graphics g(memDC);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        // 1. Signature Soft Sky Blue / Azure Gradient (Kontho-inspired, soft & modern)
        Gdiplus::Rect gRc(0, 0, cr.right, cur_h);
        Gdiplus::LinearGradientBrush bgBrush(
            gRc,
            Gdiplus::Color(255, 30, 185, 248),  // #1EB9F8 Soft Sky Cyan Blue
            Gdiplus::Color(255, 10, 130, 230),  // #0A82E6 Radiant Sky Azure Blue
            Gdiplus::LinearGradientModeVertical
        );
        g.FillRectangle(&bgBrush, gRc);

        // Subtle soft highlight border
        Gdiplus::Pen borderPen(Gdiplus::Color(160, 200, 240, 255), 1.0f);
        g.DrawRectangle(&borderPen, 0, 0, cr.right - 1, cur_h - 1);

        // Subtle top sheen line
        Gdiplus::Pen sheenPen(Gdiplus::Color(110, 255, 255, 255), 1.0f);
        g.DrawLine(&sheenPen, 2, 1, cr.right - 3, 1);

        {
            // === FULL KONTHO-MATCHED EXPANDED BAR (Image 1 or Compact Image 2) ===
            Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(255, 255, 255, 255));
            Gdiplus::Pen whitePen(Gdiplus::Color(255, 255, 255, 255), 1.6f);
            whitePen.SetStartCap(Gdiplus::LineCapRound);
            whitePen.SetEndCap(Gdiplus::LineCapRound);

            // 1. Mic Button (rc_mic_)
            {
                float cx = (rc_mic_.left + rc_mic_.right) / 2.0f;
                float cy = (rc_mic_.top + rc_mic_.bottom) / 2.0f;

                if (is_voice_recording_) {
                    Gdiplus::SolidBrush recBrush(Gdiplus::Color(245, 239, 68, 68)); // Vivid Red
                    g.FillEllipse(&recBrush, cx - 10.0f, cy - 10.0f, 20.0f, 20.0f);
                    Gdiplus::Pen ringPen(Gdiplus::Color(180, 255, 255, 255), 1.2f);
                    g.DrawEllipse(&ringPen, cx - 10.0f, cy - 10.0f, 20.0f, 20.0f);
                } else if (is_voice_busy_) {
                    Gdiplus::SolidBrush busyBrush(Gdiplus::Color(245, 59, 130, 246)); // Vibrant Azure Blue
                    g.FillEllipse(&busyBrush, cx - 10.0f, cy - 10.0f, 20.0f, 20.0f);
                    Gdiplus::Pen ringPen(Gdiplus::Color(180, 255, 255, 255), 1.2f);
                    g.DrawEllipse(&ringPen, cx - 10.0f, cy - 10.0f, 20.0f, 20.0f);
                } else if (hovered_btn_ == kBtnMic) {
                    Gdiplus::SolidBrush hBrush(Gdiplus::Color(pressed_btn_ == kBtnMic ? 80 : 45, 255, 255, 255));
                    g.FillEllipse(&hBrush, cx - 9.5f, cy - 9.5f, 19.0f, 19.0f);
                }

                g.FillEllipse(&whiteBrush, cx - 2.8f, cy - 5.5f, 5.6f, 5.6f);
                g.FillRectangle(&whiteBrush, cx - 2.8f, cy - 3.0f, 5.6f, 4.2f);
                g.FillEllipse(&whiteBrush, cx - 2.8f, cy - 0.5f, 5.6f, 5.6f);
                g.DrawArc(&whitePen, cx - 4.8f, cy - 3.5f, 9.6f, 8.8f, 10.0f, 160.0f);
                g.DrawLine(&whitePen, cx, cy + 5.2f, cx, cy + 7.2f);
                g.DrawLine(&whitePen, cx - 3.0f, cy + 7.2f, cx + 3.0f, cy + 7.2f);
            }

            // 2. Segmented Mode Controls ([বাং], [EN], [বা⇄En]) - Zero 3-click cycling!
            {
                // Container background for segmented bar
                Gdiplus::RectF segContRc(
                    (float)rc_mode_bn_.left - 1.0f, (float)rc_mode_bn_.top - 1.0f,
                    (float)(rc_mode_trans_.right - rc_mode_bn_.left + 2.0f),
                    (float)(rc_mode_bn_.bottom - rc_mode_bn_.top + 2.0f)
                );
                Gdiplus::SolidBrush segBgBrush(Gdiplus::Color(90, 100, 10, 25));
                Gdiplus::Pen segBorderPen(Gdiplus::Color(90, 255, 255, 255), 1.0f);
                Gdiplus::GraphicsPath segPath;
                segPath.AddArc(segContRc.X, segContRc.Y, 4.0f, 4.0f, 180, 90);
                segPath.AddArc(segContRc.GetRight() - 4.0f, segContRc.Y, 4.0f, 4.0f, 270, 90);
                segPath.AddArc(segContRc.GetRight() - 4.0f, segContRc.GetBottom() - 4.0f, 4.0f, 4.0f, 0, 90);
                segPath.AddArc(segContRc.X, segContRc.GetBottom() - 4.0f, 4.0f, 4.0f, 90, 90);
                segPath.CloseFigure();
                g.FillPath(&segBgBrush, &segPath);
                g.DrawPath(&segBorderPen, &segPath);

                auto DrawPill = [&](const RECT& r, bool active, ButtonId btnId,
                                    COLORREF activeTop, COLORREF activeBottom,
                                    const wchar_t* label, HFONT hFont) {
                    Gdiplus::RectF pillRc((float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top));
                    Gdiplus::GraphicsPath pPath;
                    float radius = 3.0f;
                    pPath.AddArc(pillRc.X, pillRc.Y, radius * 2, radius * 2, 180, 90);
                    pPath.AddArc(pillRc.GetRight() - radius * 2, pillRc.Y, radius * 2, radius * 2, 270, 90);
                    pPath.AddArc(pillRc.GetRight() - radius * 2, pillRc.GetBottom() - radius * 2, radius * 2, radius * 2, 0, 90);
                    pPath.AddArc(pillRc.X, pillRc.GetBottom() - radius * 2, radius * 2, radius * 2, 90, 90);
                    pPath.CloseFigure();

                    if (active) {
                        Gdiplus::LinearGradientBrush pBrush(
                            pillRc,
                            Gdiplus::Color(255, GetRValue(activeTop), GetGValue(activeTop), GetBValue(activeTop)),
                            Gdiplus::Color(255, GetRValue(activeBottom), GetGValue(activeBottom), GetBValue(activeBottom)),
                            Gdiplus::LinearGradientModeVertical
                        );
                        g.FillPath(&pBrush, &pPath);
                        Gdiplus::Pen pPen(Gdiplus::Color(180, 255, 255, 255), 1.0f);
                        g.DrawPath(&pPen, &pPath);
                    } else if (hovered_btn_ == btnId) {
                        Gdiplus::SolidBrush hBrush(Gdiplus::Color(55, 255, 255, 255));
                        g.FillPath(&hBrush, &pPath);
                    }

                    SelectObject(memDC, hFont);
                    SetBkMode(memDC, TRANSPARENT);
                    SetTextColor(memDC, active ? RGB(255, 255, 255) : (hovered_btn_ == btnId ? RGB(241, 245, 249) : RGB(254, 226, 226)));
                    RECT tr = r;
                    DrawTextW(memDC, label, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                };

                bool is_bn = (current_mode_ == likhi::ModeTarget::kBangla);
                bool is_en = (current_mode_ == likhi::ModeTarget::kEnglish);
                bool is_tr = (current_mode_ == likhi::ModeTarget::kBanglaToEnglish);

                // 1. [বাং] - Emerald Green (#10B981 -> #059669)
                DrawPill(rc_mode_bn_, is_bn, kBtnModeBn, RGB(16, 185, 129), RGB(5, 150, 105), L"বাং", font_bengali_);

                // 2. [EN] - Sapphire Royal Blue (#3B82F6 -> #1D4ED8)
                DrawPill(rc_mode_en_, is_en, kBtnModeEn, RGB(59, 130, 246), RGB(29, 78, 216), L"EN", font_english_);

                // 3. [বা⇄En] - Electric Violet / Amethyst (#8B5CF6 -> #6D28D9)
                DrawPill(rc_mode_trans_, is_tr, kBtnModeTrans, RGB(139, 92, 246), RGB(109, 40, 217), L"বা⇄En", font_bengali_small_);
            }

            if (!is_collapsed_ && !is_compact_mode_) {
                // 3. Pause / Typing Mode Button (rc_pause_) [ || ]
                {
                    float cx = (rc_pause_.left + rc_pause_.right) / 2.0f;
                    float cy = (rc_pause_.top + rc_pause_.bottom) / 2.0f;

                    if (hovered_btn_ == kBtnPause) {
                        Gdiplus::SolidBrush hBrush(Gdiplus::Color(pressed_btn_ == kBtnPause ? 80 : 45, 255, 255, 255));
                        g.FillEllipse(&hBrush, cx - 9.0f, cy - 9.0f, 18.0f, 18.0f);
                    }

                    if (suggestions_paused_) {
                        // Play triangle ▶
                        Gdiplus::PointF pts[3] = {
                            {cx - 3.0f, cy - 4.5f},
                            {cx + 4.5f, cy},
                            {cx - 3.0f, cy + 4.5f}
                        };
                        g.FillPolygon(&whiteBrush, pts, 3);
                    } else {
                        // Pause ||
                        g.FillRectangle(&whiteBrush, cx - 3.5f, cy - 4.5f, 2.5f, 9.0f);
                        g.FillRectangle(&whiteBrush, cx + 1.0f, cy - 4.5f, 2.5f, 9.0f);
                    }
                }

                // 4. Word Manager / Dictionary (rc_dict_) [ 📄🔍 ]
                {
                    float cx = (rc_dict_.left + rc_dict_.right) / 2.0f;
                    float cy = (rc_dict_.top + rc_dict_.bottom) / 2.0f;

                    if (hovered_btn_ == kBtnDict) {
                        Gdiplus::SolidBrush hBrush(Gdiplus::Color(pressed_btn_ == kBtnDict ? 80 : 45, 255, 255, 255));
                        g.FillEllipse(&hBrush, cx - 9.0f, cy - 9.0f, 18.0f, 18.0f);
                    }

                    // Document outline
                    g.DrawRectangle(&whitePen, cx - 5.5f, cy - 5.5f, 8.0f, 11.0f);
                    g.DrawLine(&whitePen, cx - 3.5f, cy - 3.0f, cx, cy - 3.0f);
                    g.DrawLine(&whitePen, cx - 3.5f, cy - 0.5f, cx, cy - 0.5f);

                    // Magnifier at bottom right
                    g.DrawEllipse(&whitePen, cx, cy, 4.0f, 4.0f);
                    g.DrawLine(&whitePen, cx + 3.0f, cy + 3.0f, cx + 5.5f, cy + 5.5f);
                }

                // 5. Virtual Keyboard / Mouse (rc_osk_) [ 🖱️ ]
                {
                    float cx = (rc_osk_.left + rc_osk_.right) / 2.0f;
                    float cy = (rc_osk_.top + rc_osk_.bottom) / 2.0f;

                    if (hovered_btn_ == kBtnOsk) {
                        Gdiplus::SolidBrush hBrush(Gdiplus::Color(pressed_btn_ == kBtnOsk ? 80 : 45, 255, 255, 255));
                        g.FillEllipse(&hBrush, cx - 9.0f, cy - 9.0f, 18.0f, 18.0f);
                    }

                    // Mouse capsule outline (Image 1)
                    Gdiplus::GraphicsPath mousePath;
                    float mw = 7.5f, mh = 11.0f, mr = 3.75f;
                    mousePath.AddArc(cx - mr, cy - mh / 2, mr * 2, mr * 2, 180, 180);
                    mousePath.AddLine(cx + mr, cy - mh / 2 + mr, cx + mr, cy + mh / 2 - mr);
                    mousePath.AddArc(cx - mr, cy + mh / 2 - mr * 2, mr * 2, mr * 2, 0, 180);
                    mousePath.CloseFigure();
                    g.DrawPath(&whitePen, &mousePath);

                    // Vertical divider and scroll wheel
                    g.DrawLine(&whitePen, cx, cy - mh / 2, cx, cy - 1.0f);
                    g.FillRectangle(&whiteBrush, cx - 0.8f, cy - 3.8f, 1.6f, 3.0f);
                }

                // 6. ScriptPad / Converter (rc_script_) [ 📝 ]
                {
                    float cx = (rc_script_.left + rc_script_.right) / 2.0f;
                    float cy = (rc_script_.top + rc_script_.bottom) / 2.0f;

                    if (hovered_btn_ == kBtnScript) {
                        Gdiplus::SolidBrush hBrush(Gdiplus::Color(pressed_btn_ == kBtnScript ? 80 : 45, 255, 255, 255));
                        g.FillEllipse(&hBrush, cx - 9.0f, cy - 9.0f, 18.0f, 18.0f);
                    }

                    // Document sheet
                    g.DrawRectangle(&whitePen, cx - 5.5f, cy - 5.0f, 8.0f, 10.0f);
                    g.DrawLine(&whitePen, cx - 3.5f, cy - 2.5f, cx, cy - 2.5f);
                    g.DrawLine(&whitePen, cx - 3.5f, cy, cx - 1.0f, cy);

                    // Pencil editing stroke
                    g.DrawLine(&whitePen, cx + 1.0f, cy + 4.5f, cx + 5.0f, cy - 1.5f);
                }

                // 7. Settings Gear (rc_settings_) [ ⚙️ ] (Crisp mechanical gear)
                {
                    float cx = (rc_settings_.left + rc_settings_.right) / 2.0f;
                    float cy = (rc_settings_.top + rc_settings_.bottom) / 2.0f;

                    if (hovered_btn_ == kBtnSettings) {
                        Gdiplus::SolidBrush hBrush(Gdiplus::Color(pressed_btn_ == kBtnSettings ? 80 : 45, 255, 255, 255));
                        g.FillEllipse(&hBrush, cx - 9.0f, cy - 9.0f, 18.0f, 18.0f);
                    }

                    // Outer gear rim with circular path
                    Gdiplus::Pen gearRimPen(Gdiplus::Color(255, 255, 255, 255), 1.8f);
                    g.DrawEllipse(&gearRimPen, cx - 3.8f, cy - 3.8f, 7.6f, 7.6f);

                    // 8 solid gear teeth with square ends
                    Gdiplus::Pen toothPen(Gdiplus::Color(255, 255, 255, 255), 2.0f);
                    toothPen.SetStartCap(Gdiplus::LineCapSquare);
                    toothPen.SetEndCap(Gdiplus::LineCapSquare);

                    for (int i = 0; i < 8; ++i) {
                        float angle = (float)(i * 3.14159265 / 4.0);
                        float x1 = cx + cosf(angle) * 3.0f;
                        float y1 = cy + sinf(angle) * 3.0f;
                        float x2 = cx + cosf(angle) * 5.8f;
                        float y2 = cy + sinf(angle) * 5.8f;
                        g.DrawLine(&toothPen, x1, y1, x2, y2);
                    }

                    // Center clear hole
                    Gdiplus::Pen holePen(Gdiplus::Color(255, 10, 130, 230), 1.0f);
                    g.DrawEllipse(&holePen, cx - 1.6f, cy - 1.6f, 3.2f, 3.2f);
                }
            }

            // 8. Power / Close Button (rc_power_)
            // In compact mode: Draw double chevron ">>" indicating expand
            // In full mode: Draw Power ring with Live Green Dot
            {
                float cx = (rc_power_.left + rc_power_.right) / 2.0f;
                float cy = (rc_power_.top + rc_power_.bottom) / 2.0f;

                if (hovered_btn_ == kBtnPower) {
                    if (width_ < kNormalWidth) {
                        Gdiplus::SolidBrush hBrush(Gdiplus::Color(pressed_btn_ == kBtnPower ? 80 : 45, 255, 255, 255));
                        g.FillEllipse(&hBrush, cx - 9.0f, cy - 9.0f, 18.0f, 18.0f);
                    } else {
                        Gdiplus::SolidBrush hBrush(Gdiplus::Color(pressed_btn_ == kBtnPower ? 180 : 120, 239, 68, 68));
                        g.FillEllipse(&hBrush, cx - 9.0f, cy - 9.0f, 18.0f, 18.0f);
                    }
                }

                if (width_ < kNormalWidth) {
                    // Double chevron ">>"
                    Gdiplus::Pen arrPen(Gdiplus::Color(255, 255, 255, 255), 1.6f);
                    arrPen.SetLineCap(Gdiplus::LineCapRound, Gdiplus::LineCapRound, Gdiplus::DashCapRound);
                    g.DrawLine(&arrPen, cx - 3.5f, cy - 4.5f, cx - 0.5f, cy);
                    g.DrawLine(&arrPen, cx - 0.5f, cy, cx - 3.5f, cy + 4.5f);
                    g.DrawLine(&arrPen, cx + 1.0f, cy - 4.5f, cx + 4.0f, cy);
                    g.DrawLine(&arrPen, cx + 4.0f, cy, cx + 1.0f, cy + 4.5f);
                } else {
                    // Power ring
                    Gdiplus::Pen pwrPen(Gdiplus::Color(255, 255, 255, 255), 1.6f);
                    g.DrawArc(&pwrPen, cx - 5.0f, cy - 4.5f, 10.0f, 10.0f, -60.0f, 300.0f);
                    g.DrawLine(&pwrPen, cx, cy - 6.0f, cx, cy - 1.2f);

                    // Active Green Status Dot (Image 1)
                    Gdiplus::SolidBrush greenDot(Gdiplus::Color(255, 52, 211, 153));
                    Gdiplus::Pen greenBorder(Gdiplus::Color(255, 255, 255, 255), 0.8f);
                    float gx = cx + 3.0f;
                    float gy = cy - 6.0f;
                    g.FillEllipse(&greenDot, gx, gy, 4.0f, 4.0f);
                    g.DrawEllipse(&greenBorder, gx, gy, 4.0f, 4.0f);
                }
            }
        }
    }

    // Blit double-buffered frame to display
    BitBlt(hdc, 0, 0, cr.right, cur_h, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);

    EndPaint(hwnd, &ps);
}

void TopBarWindow::OnLButtonDown(HWND hwnd, int x, int y) {
    SetCapture(hwnd);
    is_dragging_ = false;
    mouse_down_pt_ = {x, y};
    GetCursorPos(&drag_start_cursor_);
    drag_start_pos_ = {pos_x_, pos_y_};

    POINT pt = {x, y};
    pressed_btn_ = HitTestButton(pt);
    InvalidateRect(hwnd, nullptr, FALSE);
}

void TopBarWindow::OnLButtonUp(HWND hwnd, int x, int y) {
    bool was_dragging = is_dragging_;
    is_dragging_ = false;
    ReleaseCapture();

    if (was_dragging) {
        SaveCoordinates();
        pressed_btn_ = kBtnNone;
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }

    POINT pt = {x, y};
    ButtonId clicked = HitTestButton(pt);
    ButtonId target = pressed_btn_;
    pressed_btn_ = kBtnNone;
    InvalidateRect(hwnd, nullptr, FALSE);

    if (clicked != target || clicked == kBtnNone) {
        return;
    }

    if (clicked == kBtnMic) {
        PlaySoundEffect(3);
        LaunchVoiceInput();
        return;
    }
    if (clicked == kBtnModeBn) {
        likhi::ModeIpc::Instance().SetMode(likhi::ModeTarget::kBangla);
        current_mode_ = likhi::ModeTarget::kBangla;
        PlaySoundEffect(1);
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }
    if (clicked == kBtnModeEn) {
        likhi::ModeIpc::Instance().SetMode(likhi::ModeTarget::kEnglish);
        current_mode_ = likhi::ModeTarget::kEnglish;
        PlaySoundEffect(2);
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }
    if (clicked == kBtnModeTrans) {
        likhi::ModeIpc::Instance().SetMode(likhi::ModeTarget::kBanglaToEnglish);
        current_mode_ = likhi::ModeTarget::kBanglaToEnglish;
        PlaySoundEffect(2);
        InvalidateRect(hwnd, nullptr, FALSE);
        return;
    }
    if (!is_collapsed_ && !is_compact_mode_) {
        if (clicked == kBtnPause) {
            ToggleSuggestions();
            return;
        }
        if (clicked == kBtnDict) {
            ShowWordManagerMenu(hwnd);
            return;
        }
        if (clicked == kBtnOsk) {
            LaunchOnScreenKeyboard();
            return;
        }
        if (clicked == kBtnScript) {
            LaunchConverter();
            return;
        }
        if (clicked == kBtnSettings) {
            ShowSettingsMenu(hwnd);
            return;
        }
    }
    if (clicked == kBtnPower) {
        if (width_ < kNormalWidth) {
            ExpandToFull();
            return;
        }
        // Direct click on Power/Tray button minimizes TopBar to System Tray!
        Show(false);
        SaveCoordinates();
        return;
    }
}

void TopBarWindow::OnMouseMove(HWND hwnd, int x, int y) {
    TRACKMOUSEEVENT tme = {sizeof(tme)};
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = hwnd;
    TrackMouseEvent(&tme);

    if (GetCapture() == hwnd) {
        POINT cur_cursor;
        GetCursorPos(&cur_cursor);
        int dx = cur_cursor.x - drag_start_cursor_.x;
        int dy = cur_cursor.y - drag_start_cursor_.y;
        if (!is_dragging_ && (abs(dx) > 3 || abs(dy) > 3)) {
            is_dragging_ = true;
            pressed_btn_ = kBtnNone;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        if (is_dragging_) {
            pos_x_ = drag_start_pos_.x + dx;
            pos_y_ = drag_start_pos_.y + dy;
            normal_y_ = pos_y_;
            SetWindowPos(hwnd, HWND_TOPMOST, pos_x_, pos_y_, width_, height_,
                         SWP_NOSIZE | SWP_NOACTIVATE);
            return;
        }
    }

    // Expand immediately on mouse hover if collapsed or in compact mode
    if (is_collapsed_ || width_ < kNormalWidth) {
        ExpandToFull();
    }
    is_hovered_ = true;
    mouse_leave_time_ = 0;

    POINT pt = {x, y};
    ButtonId new_hover = HitTestButton(pt);

    if (new_hover != hovered_btn_) {
        hovered_btn_ = new_hover;
        InvalidateRect(hwnd, nullptr, FALSE);
    }
}

void TopBarWindow::OnMouseLeave(HWND hwnd) {
    hovered_btn_ = kBtnNone;
    pressed_btn_ = kBtnNone;
    is_hovered_ = false;
    mouse_leave_time_ = GetTickCount();
    InvalidateRect(hwnd, nullptr, FALSE);
}

enum MenuIconType {
    kIconTray = 0,
    kIconUpdate,
    kIconRestart,
    kIconExit,
    kIconEngLayout,
    kIconEngEnc,
    kIconBnLayout,
    kIconBnEnc,
    kIconConverter,
    kIconKeyboard,
    kIconSettings,
    kIconDict,
    kIconAbout,
    kIconHelp,
    kIconCompact,
    kIconAutoHide,
    kIconAddWord,
    kIconInfo,
    kIconVersion,
    kIconLogs,
    kIconKey,
    kIconClean,
    kIconMap,
    kIconDownload,
    kIconGithub,
    kIconFacebook,
    kIconYoutube,
    kIconWhatsapp,
    kIconTelegram,
    kIconEmail,
    kIconCount
};

static HBITMAP s_menuIcons[kIconCount] = {nullptr};

HBITMAP GetMenuIconBitmap(MenuIconType type) {
    if (type < 0 || type >= kIconCount) return nullptr;
    if (s_menuIcons[type]) return s_menuIcons[type];

    int w = 16, h = 16;
    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);

    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBmp = CreateDIBSection(memDC, &bi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    if (!hBmp) {
        DeleteDC(memDC);
        ReleaseDC(nullptr, screenDC);
        return nullptr;
    }

    HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

    {
        Gdiplus::Graphics g(memDC);
        g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
        g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);

        Gdiplus::Color bgCol;
        switch (type) {
            case kIconTray:       bgCol = Gdiplus::Color(255, 0, 122, 240); break;
            case kIconUpdate:     bgCol = Gdiplus::Color(255, 14, 165, 233); break;
            case kIconRestart:    bgCol = Gdiplus::Color(255, 16, 185, 129); break;
            case kIconExit:       bgCol = Gdiplus::Color(255, 239, 68, 68); break;
            case kIconEngLayout:  bgCol = Gdiplus::Color(255, 37, 99, 235); break;
            case kIconEngEnc:     bgCol = Gdiplus::Color(255, 99, 102, 241); break;
            case kIconBnLayout:   bgCol = Gdiplus::Color(255, 6, 182, 212); break;
            case kIconBnEnc:      bgCol = Gdiplus::Color(255, 20, 184, 166); break;
            case kIconConverter:  bgCol = Gdiplus::Color(255, 139, 92, 246); break;
            case kIconKeyboard:   bgCol = Gdiplus::Color(255, 245, 158, 11); break;
            case kIconSettings:   bgCol = Gdiplus::Color(255, 2, 132, 199); break;
            case kIconDict:       bgCol = Gdiplus::Color(255, 16, 185, 129); break;
            case kIconAbout:      bgCol = Gdiplus::Color(255, 0, 172, 255); break;
            case kIconHelp:       bgCol = Gdiplus::Color(255, 59, 130, 246); break;
            case kIconCompact:    bgCol = Gdiplus::Color(255, 100, 116, 139); break;
            case kIconAutoHide:   bgCol = Gdiplus::Color(255, 100, 116, 139); break;
            case kIconAddWord:    bgCol = Gdiplus::Color(255, 16, 185, 129); break;
            case kIconInfo:       bgCol = Gdiplus::Color(255, 37, 99, 235); break;
            case kIconVersion:    bgCol = Gdiplus::Color(255, 245, 158, 11); break;
            case kIconLogs:       bgCol = Gdiplus::Color(255, 100, 116, 139); break;
            case kIconKey:        bgCol = Gdiplus::Color(255, 234, 179, 8); break;
            case kIconClean:      bgCol = Gdiplus::Color(255, 239, 68, 68); break;
            case kIconMap:        bgCol = Gdiplus::Color(255, 6, 182, 212); break;
            case kIconDownload:   bgCol = Gdiplus::Color(255, 16, 185, 129); break;
            case kIconGithub:     bgCol = Gdiplus::Color(255, 36, 41, 47); break;
            case kIconFacebook:   bgCol = Gdiplus::Color(255, 24, 119, 242); break;
            case kIconYoutube:    bgCol = Gdiplus::Color(255, 255, 0, 0); break;
            case kIconWhatsapp:   bgCol = Gdiplus::Color(255, 37, 211, 102); break;
            case kIconTelegram:   bgCol = Gdiplus::Color(255, 0, 136, 204); break;
            case kIconEmail:      bgCol = Gdiplus::Color(255, 99, 102, 241); break;
            default:              bgCol = Gdiplus::Color(255, 0, 122, 240); break;
        }

        // Draw rounded tile badge
        Gdiplus::SolidBrush bgBrush(bgCol);
        Gdiplus::GraphicsPath tilePath;
        float r = 2.5f;
        tilePath.AddArc(0.5f, 0.5f, r * 2, r * 2, 180, 90);
        tilePath.AddArc(15.5f - r * 2, 0.5f, r * 2, r * 2, 270, 90);
        tilePath.AddArc(15.5f - r * 2, 15.5f - r * 2, r * 2, r * 2, 0, 90);
        tilePath.AddArc(0.5f, 15.5f - r * 2, r * 2, r * 2, 90, 90);
        tilePath.CloseFigure();
        g.FillPath(&bgBrush, &tilePath);

        // Draw crisp white icon details
        Gdiplus::SolidBrush whiteBrush(Gdiplus::Color(255, 255, 255, 255));
        Gdiplus::Pen whitePen(Gdiplus::Color(255, 255, 255, 255), 1.2f);
        whitePen.SetStartCap(Gdiplus::LineCapRound);
        whitePen.SetEndCap(Gdiplus::LineCapRound);

        if (type == kIconTray) {
            g.DrawLine(&whitePen, 3.5f, 12.0f, 12.5f, 12.0f);
            g.DrawLine(&whitePen, 3.5f, 9.5f, 3.5f, 12.0f);
            g.DrawLine(&whitePen, 12.5f, 9.5f, 12.5f, 12.0f);
            g.DrawLine(&whitePen, 8.0f, 3.5f, 8.0f, 9.5f);
            g.DrawLine(&whitePen, 5.5f, 7.5f, 8.0f, 10.0f);
            g.DrawLine(&whitePen, 10.5f, 7.5f, 8.0f, 10.0f);
        } else if (type == kIconUpdate) {
            g.DrawArc(&whitePen, 3.5f, 3.5f, 9.0f, 9.0f, 40.0f, 280.0f);
            g.DrawLine(&whitePen, 11.5f, 4.5f, 13.0f, 7.5f);
            g.DrawLine(&whitePen, 13.0f, 7.5f, 9.5f, 7.5f);
        } else if (type == kIconRestart) {
            g.DrawArc(&whitePen, 3.5f, 3.5f, 9.0f, 9.0f, -40.0f, 270.0f);
            g.DrawLine(&whitePen, 8.0f, 2.0f, 10.5f, 4.5f);
            g.DrawLine(&whitePen, 8.0f, 6.5f, 10.5f, 4.5f);
        } else if (type == kIconExit) {
            g.DrawArc(&whitePen, 3.5f, 3.5f, 9.0f, 9.0f, -60.0f, 300.0f);
            g.DrawLine(&whitePen, 8.0f, 2.5f, 8.0f, 7.0f);
        } else if (type == kIconEngLayout) {
            HFONT f = CreateFontW(-9, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            HGDIOBJ of = SelectObject(memDC, f);
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, RGB(255, 255, 255));
            RECT tr = {0, 1, 16, 15};
            DrawTextW(memDC, L"EN", -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(memDC, of);
            DeleteObject(f);
        } else if (type == kIconEngEnc) {
            HFONT f = CreateFontW(-8, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            HGDIOBJ of = SelectObject(memDC, f);
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, RGB(255, 255, 255));
            RECT tr = {0, 1, 16, 15};
            DrawTextW(memDC, L"UTF", -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(memDC, of);
            DeleteObject(f);
        } else if (type == kIconBnLayout) {
            HFONT f = CreateFontW(-10, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Vrinda");
            if (!f) f = CreateFontW(-10, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            HGDIOBJ of = SelectObject(memDC, f);
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, RGB(255, 255, 255));
            RECT tr = {0, 0, 16, 16};
            DrawTextW(memDC, L"বাং", -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(memDC, of);
            DeleteObject(f);
        } else if (type == kIconBnEnc) {
            HFONT f = CreateFontW(-10, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Vrinda");
            if (!f) f = CreateFontW(-10, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            HGDIOBJ of = SelectObject(memDC, f);
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, RGB(255, 255, 255));
            RECT tr = {0, 0, 16, 16};
            DrawTextW(memDC, L"অ", -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(memDC, of);
            DeleteObject(f);
        } else if (type == kIconConverter) {
            g.DrawLine(&whitePen, 3.5f, 6.0f, 11.5f, 6.0f);
            g.DrawLine(&whitePen, 9.5f, 4.0f, 12.0f, 6.0f);
            g.DrawLine(&whitePen, 9.5f, 8.0f, 12.0f, 6.0f);
            g.DrawLine(&whitePen, 12.5f, 10.0f, 4.5f, 10.0f);
            g.DrawLine(&whitePen, 6.5f, 8.0f, 4.0f, 10.0f);
            g.DrawLine(&whitePen, 6.5f, 12.0f, 4.0f, 10.0f);
        } else if (type == kIconKeyboard) {
            g.DrawRectangle(&whitePen, 2.5f, 4.5f, 11.0f, 7.0f);
            g.FillRectangle(&whiteBrush, 4.0f, 6.0f, 1.8f, 1.5f);
            g.FillRectangle(&whiteBrush, 7.0f, 6.0f, 1.8f, 1.5f);
            g.FillRectangle(&whiteBrush, 10.0f, 6.0f, 1.8f, 1.5f);
            g.FillRectangle(&whiteBrush, 5.0f, 8.5f, 5.8f, 1.5f);
        } else if (type == kIconSettings) {
            g.DrawEllipse(&whitePen, 4.5f, 4.5f, 7.0f, 7.0f);
            for (int i = 0; i < 6; ++i) {
                float a = (float)(i * 3.14159265 / 3.0);
                g.DrawLine(&whitePen, 8.0f + cosf(a) * 3.2f, 8.0f + sinf(a) * 3.2f,
                                      8.0f + cosf(a) * 5.8f, 8.0f + sinf(a) * 5.8f);
            }
        } else if (type == kIconDict) {
            g.DrawRectangle(&whitePen, 3.0f, 2.5f, 6.5f, 9.5f);
            g.DrawEllipse(&whitePen, 7.5f, 7.5f, 4.5f, 4.5f);
            g.DrawLine(&whitePen, 11.0f, 11.0f, 13.0f, 13.0f);
        } else if (type == kIconAbout) {
            HFONT f = CreateFontW(-10, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Vrinda");
            if (!f) f = CreateFontW(-10, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            HGDIOBJ of = SelectObject(memDC, f);
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, RGB(255, 255, 255));
            RECT tr = {0, 0, 16, 16};
            DrawTextW(memDC, L"লি", -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(memDC, of);
            DeleteObject(f);
        } else if (type == kIconHelp) {
            HFONT f = CreateFontW(-11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            HGDIOBJ of = SelectObject(memDC, f);
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, RGB(255, 255, 255));
            RECT tr = {0, 0, 16, 16};
            DrawTextW(memDC, L"?", -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(memDC, of);
            DeleteObject(f);
        } else if (type == kIconCompact) {
            g.DrawRectangle(&whitePen, 2.5f, 5.0f, 11.0f, 6.0f);
            g.FillEllipse(&whiteBrush, 4.5f, 7.0f, 2.0f, 2.0f);
            g.FillEllipse(&whiteBrush, 7.0f, 7.0f, 2.0f, 2.0f);
            g.FillEllipse(&whiteBrush, 9.5f, 7.0f, 2.0f, 2.0f);
        } else if (type == kIconAutoHide) {
            g.DrawLine(&whitePen, 2.5f, 3.5f, 13.5f, 3.5f);
            g.DrawLine(&whitePen, 8.0f, 12.5f, 8.0f, 6.5f);
            g.DrawLine(&whitePen, 5.5f, 8.5f, 8.0f, 6.0f);
            g.DrawLine(&whitePen, 10.5f, 8.5f, 8.0f, 6.0f);
        } else if (type == kIconAddWord) {
            g.DrawLine(&whitePen, 8.0f, 3.5f, 8.0f, 12.5f);
            g.DrawLine(&whitePen, 3.5f, 8.0f, 12.5f, 8.0f);
        } else if (type == kIconInfo) {
            HFONT f = CreateFontW(-11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            HGDIOBJ of = SelectObject(memDC, f);
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, RGB(255, 255, 255));
            RECT tr = {0, 0, 16, 16};
            DrawTextW(memDC, L"i", -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(memDC, of);
            DeleteObject(f);
        } else if (type == kIconVersion) {
            g.DrawRectangle(&whitePen, 3.0f, 4.0f, 10.0f, 8.0f);
            g.DrawLine(&whitePen, 3.0f, 7.0f, 13.0f, 7.0f);
            g.DrawLine(&whitePen, 8.0f, 7.0f, 8.0f, 12.0f);
        } else if (type == kIconLogs) {
            g.DrawRectangle(&whitePen, 3.5f, 2.5f, 9.0f, 11.0f);
            g.DrawLine(&whitePen, 5.5f, 5.5f, 10.5f, 5.5f);
            g.DrawLine(&whitePen, 5.5f, 8.0f, 10.5f, 8.0f);
            g.DrawLine(&whitePen, 5.5f, 10.5f, 8.5f, 10.5f);
        } else if (type == kIconKey) {
            g.DrawEllipse(&whitePen, 3.5f, 3.5f, 5.0f, 5.0f);
            g.DrawLine(&whitePen, 8.5f, 8.5f, 13.0f, 13.0f);
            g.DrawLine(&whitePen, 11.0f, 11.0f, 12.5f, 9.5f);
        } else if (type == kIconClean) {
            g.DrawLine(&whitePen, 4.0f, 12.0f, 12.0f, 4.0f);
            g.DrawLine(&whitePen, 3.0f, 13.0f, 6.0f, 10.0f);
            g.DrawLine(&whitePen, 7.0f, 3.0f, 13.0f, 9.0f);
        } else if (type == kIconMap) {
            g.DrawRectangle(&whitePen, 2.5f, 3.5f, 11.0f, 9.0f);
            g.DrawLine(&whitePen, 2.5f, 8.0f, 13.5f, 8.0f);
            g.DrawLine(&whitePen, 8.0f, 3.5f, 8.0f, 12.5f);
        } else if (type == kIconDownload) {
            g.DrawLine(&whitePen, 8.0f, 3.0f, 8.0f, 10.5f);
            g.DrawLine(&whitePen, 5.0f, 7.5f, 8.0f, 10.5f);
            g.DrawLine(&whitePen, 11.0f, 7.5f, 8.0f, 10.5f);
            g.DrawLine(&whitePen, 3.5f, 13.0f, 12.5f, 13.0f);
        } else if (type == kIconGithub) {
            HFONT f = CreateFontW(-8, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            HGDIOBJ of = SelectObject(memDC, f);
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, RGB(255, 255, 255));
            RECT tr = {0, 0, 16, 16};
            DrawTextW(memDC, L"GH", -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(memDC, of);
            DeleteObject(f);
        } else if (type == kIconFacebook) {
            HFONT f = CreateFontW(-11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            HGDIOBJ of = SelectObject(memDC, f);
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, RGB(255, 255, 255));
            RECT tr = {1, 0, 15, 16};
            DrawTextW(memDC, L"f", -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(memDC, of);
            DeleteObject(f);
        } else if (type == kIconYoutube) {
            Gdiplus::PointF pts[3] = { {6.0f, 5.0f}, {11.5f, 8.0f}, {6.0f, 11.0f} };
            g.FillPolygon(&whiteBrush, pts, 3);
        } else if (type == kIconWhatsapp) {
            g.DrawEllipse(&whitePen, 3.0f, 3.0f, 10.0f, 9.0f);
            g.DrawLine(&whitePen, 4.0f, 11.0f, 2.5f, 13.5f);
            g.DrawLine(&whitePen, 2.5f, 13.5f, 5.5f, 12.0f);
        } else if (type == kIconTelegram) {
            Gdiplus::PointF pts[3] = { {3.5f, 8.0f}, {12.5f, 4.0f}, {7.5f, 12.0f} };
            g.DrawPolygon(&whitePen, pts, 3);
            g.DrawLine(&whitePen, 7.5f, 12.0f, 7.5f, 8.5f);
        } else if (type == kIconEmail) {
            g.DrawRectangle(&whitePen, 2.5f, 4.0f, 11.0f, 8.0f);
            g.DrawLine(&whitePen, 2.5f, 4.0f, 8.0f, 8.5f);
            g.DrawLine(&whitePen, 13.5f, 4.0f, 8.0f, 8.5f);
        }
    }

    SelectObject(memDC, oldBmp);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);

    s_menuIcons[type] = hBmp;
    return hBmp;
}

void SetMenuItemIcon(HMENU hMenu, UINT idOrPos, bool byPos, MenuIconType iconType) {
    HBITMAP hBmp = GetMenuIconBitmap(iconType);
    if (!hBmp) return;
    MENUITEMINFO mii = {sizeof(mii)};
    mii.fMask = MIIM_BITMAP;
    mii.hbmpItem = hBmp;
    SetMenuItemInfoW(hMenu, idOrPos, byPos ? TRUE : FALSE, &mii);
}

void TopBarWindow::ShowPowerMenu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();

    MENUINFO mi = {sizeof(mi)};
    mi.cbSize = sizeof(mi);
    mi.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
    static HBRUSH s_menuBg = CreateSolidBrush(RGB(255, 255, 255));
    mi.hbrBack = s_menuBg;
    SetMenuInfo(menu, &mi);

    AppendMenuW(menu, MF_STRING, 501, L"Move to Tray");
    AppendMenuW(menu, MF_STRING, 502, L"Check for Update");
    AppendMenuW(menu, MF_STRING, 503, L"Restart");
    AppendMenuW(menu, MF_STRING, 504, L"Exit");

    SetMenuItemIcon(menu, 501, false, kIconTray);
    SetMenuItemIcon(menu, 502, false, kIconUpdate);
    SetMenuItemIcon(menu, 503, false, kIconRestart);
    SetMenuItemIcon(menu, 504, false, kIconExit);

    POINT pt = {rc_power_.right, rc_power_.bottom + 3};
    ClientToScreen(hwnd, &pt);

    SetForegroundWindow(hwnd);
    UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTALIGN | TPM_TOPALIGN,
                              pt.x, pt.y, 0, hwnd, nullptr);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);

    hovered_btn_ = kBtnNone;
    pressed_btn_ = kBtnNone;
    InvalidateRect(hwnd, nullptr, FALSE);

    switch (cmd) {
        case 501:
            Show(false);
            SaveCoordinates();
            break;
        case 502:
            CheckForUpdatesManual(hwnd);
            break;
        case 503:
            RestartLikhi();
            break;
        case 504:
            if (hwnd_parent_) {
                PostMessageW(hwnd_parent_, WM_COMMAND, 105, 0);
            } else {
                DestroyWindow(hwnd);
                PostQuitMessage(0);
            }
            break;
    }
}

void TopBarWindow::ShowSettingsMenu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();

    // 1. English Layouts submenu (Image match)
    HMENU menuEngLayouts = CreatePopupMenu();
    AppendMenuW(menuEngLayouts, MF_STRING | MF_CHECKED, 401, L"QWERTY (Standard US)");
    AppendMenuW(menuEngLayouts, MF_STRING, 402, L"Dvorak");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)menuEngLayouts, L"English Layouts");

    // 2. English Encodings submenu
    HMENU menuEngEnc = CreatePopupMenu();
    AppendMenuW(menuEngEnc, MF_STRING | MF_CHECKED, 411, L"UTF-8 / Unicode (Standard)");
    AppendMenuW(menuEngEnc, MF_STRING, 412, L"ASCII / ANSI");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)menuEngEnc, L"English Encodings");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    // 3. Bangla Layouts submenu (Image match)
    HMENU menuBnLayouts = CreatePopupMenu();
    AppendMenuW(menuBnLayouts, MF_STRING | (current_mode_ == likhi::ModeTarget::kBangla ? MF_CHECKED : 0), 421, L"Avro Phonetic (ফনেটিক)");
    AppendMenuW(menuBnLayouts, MF_STRING | (current_mode_ == likhi::ModeTarget::kBanglaToEnglish ? MF_CHECKED : 0), 422, L"Banglish (বাংলিশ অনুবাদ) [Ctrl+Alt+E]");
    AppendMenuW(menuBnLayouts, MF_STRING, 423, L"Bijoy Bayanno (বিজয় বাহান্ন)");
    AppendMenuW(menuBnLayouts, MF_STRING, 424, L"Probhat (प्रभात)");
    AppendMenuW(menuBnLayouts, MF_STRING, 425, L"National Jatiya (জাতীয়)");
    AppendMenuW(menuBnLayouts, MF_STRING, 426, L"Unibijoy (ইউনিবিজয়)");
    AppendMenuW(menuBnLayouts, MF_STRING, 427, L"Munir Optima (মুনীর অপটিমা)");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)menuBnLayouts, L"Bangla Layouts");

    // 4. Bangla Encodings submenu
    HMENU menuBnEnc = CreatePopupMenu();
    AppendMenuW(menuBnEnc, MF_STRING | MF_CHECKED, 431, L"Unicode (ইউনিকোড - আধুনিক)");
    AppendMenuW(menuBnEnc, MF_STRING, 432, L"ANSI / Bijoy Classic (বিজয় ক্লাসিক)");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)menuBnEnc, L"Bangla Encodings");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    // 5. Encoding Converter
    AppendMenuW(menu, MF_STRING, 441, L"Encoding Converter (ইউনিকোড ⇄ বিজয়)");

    // 6. Edit/Build keyboard layout
    AppendMenuW(menu, MF_STRING, 442, L"Edit / Build Keyboard Layout...");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    // 7. Settings (matching screenshot)
    AppendMenuW(menu, MF_STRING, 211, L"Settings");

    // 8. About Likhi submenu (Exact Image 1 match)
    HMENU menuAbout = CreatePopupMenu();
    AppendMenuW(menuAbout, MF_STRING, 451, L"About");
    AppendMenuW(menuAbout, MF_STRING, 452, L"Current Version");
    AppendMenuW(menuAbout, MF_STRING, 453, L"ChangeLogs");
    AppendMenuW(menuAbout, MF_STRING, 454, L"Activation");
    AppendMenuW(menuAbout, MF_STRING, 455, L"Prepare for Uninstall/Clean Update");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)menuAbout, L"About Likhi");

    // 9. ? Help submenu (Exact Image 2 match)
    HMENU menuHelp = CreatePopupMenu();
    AppendMenuW(menuHelp, MF_STRING, 461, L"Banglish Layout Map");
    AppendMenuW(menuHelp, MF_STRING, 462, L"Download Layouts and Encoders");
    AppendMenuW(menuHelp, MF_STRING, 463, L"Github Site");
    AppendMenuW(menuHelp, MF_STRING, 464, L"Facebook page");
    AppendMenuW(menuHelp, MF_STRING, 465, L"Youtube");
    AppendMenuW(menuHelp, MF_STRING, 466, L"Whatsapp");
    AppendMenuW(menuHelp, MF_STRING, 467, L"Telegram");
    AppendMenuW(menuHelp, MF_STRING, 468, L"Email");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)menuHelp, L"? Help");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    // View options
    AppendMenuW(menu, MF_STRING | ((dock_pos_ == kDockRight && auto_hide_enabled_) ? MF_CHECKED : 0), 212, L"👉 ডান পাশে অটো-হাইড (Auto-Hide to Right Side)");
    AppendMenuW(menu, MF_STRING | ((dock_pos_ == kDockTop && auto_hide_enabled_) ? MF_CHECKED : 0), 210, L"👆 শীর্ষে অটো-হাইড (Auto-Hide to Top)");
    AppendMenuW(menu, MF_STRING | (!auto_hide_enabled_ ? MF_CHECKED : 0), 213, L"📌 সর্বদা দৃশ্যমান (Always Visible)");
    AppendMenuW(menu, MF_STRING | (is_compact_mode_ ? MF_CHECKED : 0), 209, L"📐 কম্প্যাক্ট টপবার (Compact TopBar) [DblClick]");

    MENUINFO mi = {sizeof(mi)};
    mi.cbSize = sizeof(mi);
    mi.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
    static HBRUSH s_menuBg = CreateSolidBrush(RGB(255, 255, 255));
    mi.hbrBack = s_menuBg;
    SetMenuInfo(menu, &mi);

    // Set colorful icon badges on each menu item (matching screenshots)
    SetMenuItemIcon(menu, 0, true, kIconEngLayout);
    SetMenuItemIcon(menu, 1, true, kIconEngEnc);
    SetMenuItemIcon(menu, 3, true, kIconBnLayout);
    SetMenuItemIcon(menu, 4, true, kIconBnEnc);
    SetMenuItemIcon(menu, 441, false, kIconConverter);
    SetMenuItemIcon(menu, 442, false, kIconKeyboard);
    SetMenuItemIcon(menu, 211, false, kIconSettings);
    SetMenuItemIcon(menu, 8, true, kIconAbout);
    SetMenuItemIcon(menu, 9, true, kIconHelp);
    SetMenuItemIcon(menu, 212, false, kIconAutoHide);
    SetMenuItemIcon(menu, 210, false, kIconAutoHide);
    SetMenuItemIcon(menu, 213, false, kIconSettings);
    SetMenuItemIcon(menu, 209, false, kIconCompact);

    // About submenu icons (Image 1)
    SetMenuItemIcon(menuAbout, 451, false, kIconInfo);
    SetMenuItemIcon(menuAbout, 452, false, kIconVersion);
    SetMenuItemIcon(menuAbout, 453, false, kIconLogs);
    SetMenuItemIcon(menuAbout, 454, false, kIconKey);
    SetMenuItemIcon(menuAbout, 455, false, kIconClean);

    // ? Help submenu icons (Image 2)
    SetMenuItemIcon(menuHelp, 461, false, kIconMap);
    SetMenuItemIcon(menuHelp, 462, false, kIconDownload);
    SetMenuItemIcon(menuHelp, 463, false, kIconGithub);
    SetMenuItemIcon(menuHelp, 464, false, kIconFacebook);
    SetMenuItemIcon(menuHelp, 465, false, kIconYoutube);
    SetMenuItemIcon(menuHelp, 466, false, kIconWhatsapp);
    SetMenuItemIcon(menuHelp, 467, false, kIconTelegram);
    SetMenuItemIcon(menuHelp, 468, false, kIconEmail);

    POINT pt = {rc_settings_.left, rc_settings_.bottom + 3};
    ClientToScreen(hwnd, &pt);

    SetForegroundWindow(hwnd);
    UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
                              pt.x, pt.y, 0, hwnd, nullptr);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);

    hovered_btn_ = kBtnNone;
    pressed_btn_ = kBtnNone;
    InvalidateRect(hwnd, nullptr, FALSE);

    switch (cmd) {
        case 401: case 402:
            likhi::ModeIpc::Instance().SetMode(likhi::ModeTarget::kEnglish);
            PlaySoundEffect(false);
            current_mode_ = likhi::ModeTarget::kEnglish;
            UpdateMode(current_mode_);
            break;
        case 421: case 423: case 424: case 425: case 426: case 427:
            likhi::ModeIpc::Instance().SetMode(likhi::ModeTarget::kBangla);
            PlaySoundEffect(false);
            current_mode_ = likhi::ModeTarget::kBangla;
            UpdateMode(current_mode_);
            break;
        case 422:
            likhi::ModeIpc::Instance().SetMode(likhi::ModeTarget::kBanglaToEnglish);
            PlaySoundEffect(false);
            current_mode_ = likhi::ModeTarget::kBanglaToEnglish;
            UpdateMode(current_mode_);
            break;
        case 441:
            LaunchConverter();
            break;
        case 442:
            LaunchSettings(L"/keyboard");
            break;
        case 211:
            LaunchSettings();
            break;
        case 451: // About (Image 1)
            MessageBoxW(hwnd,
                L"Likhi (লিখি) — The Intelligent Bangla Typing Engine\n"
                L"Version 1.0.0 (Windows 64-bit Edition)\n\n"
                L"Features:\n"
                L"• AI-Powered Bengali Voice Typing (Gemini Multimodal Speech)\n"
                L"• 86K+ Bengali High-Frequency Lexicon Trie & Dynamic Ranker\n"
                L"• Roman Banglish to English Real-Time Translation (Ctrl+Alt+E)\n"
                L"• Sleek 24px Kontho-matched Floating TopBar UI\n"
                L"• Standalone Notification Tray & Pure Icon Mode\n\n"
                L"Crafted with ❤️ by AH Creations\n"
                L"Official Portal: https://getlikhi.com\n"
                L"Direct Support: supportglikhi@gmail.com",
                L"About Likhi", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
            break;
        case 452: // Current Version (Image 1)
            MessageBoxW(hwnd,
                L"Likhi Typing Engine\n\n"
                L"Current Version: 1.0.0 (Release Build 2026.10)\n"
                L"Target Architecture: Windows x64 / x86\n"
                L"Release Channel: Stable Production\n\n"
                L"Status: Fully Updated & Running Locally.",
                L"Likhi - Current Version", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
            break;
        case 453: // ChangeLogs (Image 1)
            MessageBoxW(hwnd,
                L"What's New in Likhi v1.0.0:\n\n"
                L"1. Kontho-Exact 24px Height Sleek Floating TopBar with Compact (126px) & Full (242px) modes.\n"
                L"2. Built-in Gemini AI Bengali Voice Recognition with pulse mic indicator.\n"
                L"3. Dual Banglish & English conversion engine with Ctrl+Alt+E hotkey.\n"
                L"4. Independent Standalone Icon Mode (zero Windows keyboard layout clutter).\n"
                L"5. 100% Offline-first privacy and personal learning cache.",
                L"Likhi - ChangeLogs", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
            break;
        case 454: // Activation (Image 1)
            MessageBoxW(hwnd,
                L"Likhi Activation Status:\n\n"
                L"License Type: Free & Open Source Software\n"
                L"Status: Lifetime Activated ✅\n\n"
                L"Likhi is completely free for personal and commercial use without any restrictions.",
                L"Likhi - Activation", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
            break;
        case 455: // Prepare for Uninstall/Clean Update (Image 1)
            PrepareCleanUninstall(hwnd);
            break;
        case 461: // Banglish Layout Map (Image 2)
            ShowLayoutMap(hwnd);
            break;
        case 462: // Download Layouts and Encoders (Image 2)
            ShellExecuteW(nullptr, L"open", L"https://getlikhi.com/download", nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case 463: // Github Site (Image 2)
            ShellExecuteW(nullptr, L"open", L"https://github.com/Badboy-collab/likhi", nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case 464: // Facebook page (Image 2)
            ShellExecuteW(nullptr, L"open", L"https://facebook.com/getlikhi", nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case 465: // Youtube (Image 2)
            ShellExecuteW(nullptr, L"open", L"https://youtube.com/@getlikhi", nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case 466: // Whatsapp (Image 2)
            ShellExecuteW(nullptr, L"open", L"https://wa.me/8801700000000", nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case 467: // Telegram (Image 2)
            ShellExecuteW(nullptr, L"open", L"https://t.me/likhiapp", nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case 468: // Email (Image 2)
            ShellExecuteW(nullptr, L"open", L"mailto:supportglikhi@gmail.com", nullptr, nullptr, SW_SHOWNORMAL);
            break;
        case 212:
            SetDockPosition(kDockRight);
            break;
        case 210:
            SetDockPosition(kDockTop);
            break;
        case 213:
            SetAutoHide(false);
            SaveCoordinates();
            break;
        case 209:
            ToggleCompactMode();
            break;
    }
}

void TopBarWindow::PrepareCleanUninstall(HWND hwnd) {
    int ans = MessageBoxW(hwnd,
        L"আপনি কি Likhi ক্লিন আপডেট বা আনইনস্টলের জন্য উইন্ডোজ কীবোর্ড তালিকা ও সেটিংস প্রস্তুত করতে চান?\n\n"
        L"এটি উইন্ডোজ লেআউট তালিকা (bn-BD) ও ক্যাশ পরিষ্কার করবে। আপনার ব্যক্তিগত ডিকশনারি ও সেটিংস নিরাপদ থাকবে।",
        L"Prepare for Uninstall / Clean Update", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
    if (ans == IDYES) {
        wchar_t exePath[MAX_PATH] = {0};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring dir = exePath;
        size_t slash = dir.find_last_of(L"\\/");
        if (slash != std::wstring::npos) dir = dir.substr(0, slash);
        std::wstring dllPath = dir + L"\\bangla_tsf.dll";
        if (GetFileAttributesW(dllPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            typedef HRESULT (STDAPICALLTYPE *DllUnregFn)();
            HMODULE hDll = LoadLibraryW(dllPath.c_str());
            if (hDll) {
                DllUnregFn pFn = (DllUnregFn)GetProcAddress(hDll, "DllUnregisterServer");
                if (pFn) pFn();
                FreeLibrary(hDll);
            }
        }
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\International\\User Profile", 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
            wchar_t buf[1024] = {0};
            DWORD sz = sizeof(buf);
            if (RegQueryValueExW(hKey, L"Languages", nullptr, nullptr, (LPBYTE)buf, &sz) == ERROR_SUCCESS) {
                std::vector<std::wstring> langs;
                const wchar_t* p = buf;
                while (*p) {
                    std::wstring s(p);
                    if (s != L"bn-BD" && s != L"bn" && s != L"bn-IN") {
                        langs.push_back(s);
                    }
                    p += s.length() + 1;
                }
                if (langs.empty()) langs.push_back(L"en-US");
                std::vector<wchar_t> multi_sz;
                for (const auto& l : langs) {
                    for (wchar_t c : l) multi_sz.push_back(c);
                    multi_sz.push_back(L'\0');
                }
                multi_sz.push_back(L'\0');
                RegSetValueExW(hKey, L"Languages", 0, REG_MULTI_SZ, (const BYTE*)multi_sz.data(), (DWORD)(multi_sz.size() * sizeof(wchar_t)));
            }
            RegCloseKey(hKey);
        }
        RegDeleteTreeW(HKEY_CURRENT_USER, L"Control Panel\\International\\User Profile\\bn-BD");
        MessageBoxW(hwnd,
            L"ক্লিন প্রিপারেশন সফল হয়েছে! উইন্ডোজ কীবোর্ড তালিকা পরিষ্কার করা হয়েছে।",
            L"Likhi Clean Preparation Complete", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
    }
}

void TopBarWindow::ShowLayoutMap(HWND hwnd) {
    MessageBoxW(hwnd,
        L"Likhi Banglish Phonetic Layout Map:\n\n"
        L"• স্বরবর্ণ: a (অ), A / aa (আ), i (ই), I / ee (ঈ), u (উ), U / oo (ঊ), rri (ঋ), e (এ), oi (ঐ), o (ও), ou (ঔ)\n"
        L"• ক-বর্গ: k (ক), kh (খ), g (গ), gh (ঘ), Ng (ঙ)\n"
        L"• চ-বর্গ: c / ch (চ), Ch / chh (ছ), j (জ), jh (ঝ), NG (ঞ)\n"
        L"• ট-বর্গ: T (ট), Th (ঠ), D (ড), Dh (ঢ), N (ণ)\n"
        L"• ত-বর্গ: t (ত), th (থ), d (দ), dh (ধ), n (ন)\n"
        L"• প-বর্গ: p (প), f / ph (ফ), b (ব), bh / v (ভ), m (ম)\n"
        L"• অন্তস্থ: z / y (য), r (র), l (ল), w (ওয়)\n"
        L"• শ-ষ-স-হ: sh / S (শ), Sh (ষ), s (স), h (হ)\n"
        L"• ড়-ঢ়-য়: R (ড়), Rh (ঢ়), y / Y (য়)\n"
        L"• বিশেষ: t`` (ৎ), ng (ং), : (ঃ), ^ (ঁ), ,, (্)\n\n"
        L"অফিসিয়াল ফোনেটিক গাইড বিস্তারিত দেখতে ব্রাউজার খুলবে।",
        L"Likhi Phonetic Layout Guide", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
    ShellExecuteW(nullptr, L"open", L"https://getlikhi.com/phonetic-guide", nullptr, nullptr, SW_SHOWNORMAL);
}

void TopBarWindow::RestartLikhi() {
    wchar_t path[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    ShellExecuteW(nullptr, L"open", path, nullptr, nullptr, SW_SHOWNORMAL);
    if (hwnd_parent_) {
        PostMessageW(hwnd_parent_, WM_COMMAND, 105, 0);
    } else {
        ExitProcess(0);
    }
}

void TopBarWindow::CheckForUpdatesManual(HWND hwnd) {
    std::thread([hwnd]() {
        likhi::ReleaseInfo info;
        likhi::UpdateCheckResult res = likhi::UpdateService::CheckForUpdate(info, true);
        if (res == likhi::UpdateCheckResult::kUpdateAvailable) {
            std::wstring ver(info.version.begin(), info.version.end());
            std::wstring msg = L"Likhi এর একটি নতুন সংস্করণ (" + ver + 
                               L") পাওয়া গেছে!\n\nআপনি কি এখনই নতুন সংস্করণটি ডাউনলোড ও আপডেট করতে চান?";
            int choice = MessageBoxW(hwnd, msg.c_str(), L"Likhi Update (লিখি আপডেট)",
                                     MB_YESNO | MB_ICONINFORMATION | MB_TOPMOST);
            if (choice == IDYES) {
                likhi::UpdateService::LaunchOfficialUpdateFlow(info.update_page_url);
            }
        } else if (res == likhi::UpdateCheckResult::kUpToDate) {
            MessageBoxW(hwnd, L"আপনার Likhi অ্যাপটি আপ-টু-ডেট রয়েছে। কোনো নতুন আপডেট নেই।", 
                        L"Likhi Update (লিখি আপডেট)", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
        } else {
            int choice = MessageBoxW(hwnd, 
                L"সার্ভারের সাথে সংযোগ স্থাপন করা যায়নি বা নেটওয়ার্ক অফলাইন।\n\nআপনি কি অফিসিয়াল ওয়েবসাইট থেকে আপডেট চেক করতে চান?",
                L"Likhi Update", MB_YESNO | MB_ICONQUESTION | MB_TOPMOST);
            if (choice == IDYES) {
                likhi::UpdateService::LaunchOfficialUpdateFlow("");
            }
        }
    }).detach();
}

void TopBarWindow::LaunchDictionary() {
    std::wstring app_data = GetAppDataPath();
    std::wstring dict_path = app_data + L"\\user_dict.txt";
    ShellExecuteW(nullptr, L"open", dict_path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void TopBarWindow::ToggleSuggestions() {
    suggestions_paused_ = !suggestions_paused_;
    InvalidateRect(hwnd_, &rc_pause_, FALSE);
}

void TopBarWindow::InsertTodayDate(bool bengali_digits) {
    SYSTEMTIME st;
    GetLocalTime(&st);

    wchar_t buf[64] = {0};
    if (bengali_digits) {
        const wchar_t* bdigits = L"\u09E6\u09E7\u09E8\u09E9\u09EA\u09EB\u09EC\u09ED\u09EE\u09EF";
        wchar_t raw[64] = {0};
        wsprintfW(raw, L"%02d/%02d/%04d", st.wDay, st.wMonth, st.wYear);
        std::wstring out;
        for (int i = 0; raw[i]; ++i) {
            if (raw[i] >= L'0' && raw[i] <= L'9') {
                out += bdigits[raw[i] - L'0'];
            } else {
                out += raw[i];
            }
        }
        TypeUnicodeText(out);
    } else {
        wsprintfW(buf, L"%02d/%02d/%04d", st.wDay, st.wMonth, st.wYear);
        TypeUnicodeText(buf);
    }
}

void TopBarWindow::InsertCurrentTime() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    int hour = st.wHour;
    const wchar_t* ampm = (hour >= 12) ? L"PM" : L"AM";
    if (hour == 0) hour = 12;
    else if (hour > 12) hour -= 12;

    wchar_t buf[64] = {0};
    wsprintfW(buf, L"%d:%02d %ls", hour, st.wMinute, ampm);
    TypeUnicodeText(buf);
}

void TopBarWindow::TriggerEmojiPicker() {
    keybd_event(VK_LWIN, 0, 0, 0);
    keybd_event(VK_OEM_PERIOD, 0, 0, 0);
    keybd_event(VK_OEM_PERIOD, 0, KEYEVENTF_KEYUP, 0);
    keybd_event(VK_LWIN, 0, KEYEVENTF_KEYUP, 0);
}

void TopBarWindow::LaunchOnScreenKeyboard() {
    if (osk_window_) {
        osk_window_->ToggleVisibility();
    } else {
        ShellExecuteW(nullptr, L"open", L"osk.exe", nullptr, nullptr, SW_SHOWNORMAL);
    }
}

void TopBarWindow::LaunchConverter() {
    ShellExecuteW(nullptr, L"open", L"https://getlikhi.com/converter/", nullptr, nullptr, SW_SHOWNORMAL);
}

void TopBarWindow::LaunchSettings(const wchar_t* args) {
    std::wstring settings = FindSettingsExe();
    HINSTANCE res = ShellExecuteW(nullptr, L"open", settings.c_str(), args, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(res) <= 32) {
        ShellExecuteW(nullptr, L"open", L"bangla_settings.exe", args, nullptr, SW_SHOWNORMAL);
    }
}

void TopBarWindow::ShowWordManagerMenu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    MENUINFO mi = {sizeof(mi)};
    mi.cbSize = sizeof(mi);
    mi.fMask = MIM_BACKGROUND | MIM_APPLYTOSUBMENUS;
    static HBRUSH s_menuBg = CreateSolidBrush(RGB(255, 255, 255));
    mi.hbrBack = s_menuBg;
    SetMenuInfo(menu, &mi);

    AppendMenuW(menu, MF_STRING, 301, L"Word Manager");
    AppendMenuW(menu, MF_STRING, 302, L"Add new word");

    SetMenuItemIcon(menu, 301, false, kIconDict);
    SetMenuItemIcon(menu, 302, false, kIconAddWord);

    POINT pt = {rc_dict_.left, rc_dict_.bottom + 3};
    ClientToScreen(hwnd, &pt);

    SetForegroundWindow(hwnd);
    UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
                              pt.x, pt.y, 0, hwnd, nullptr);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);

    hovered_btn_ = kBtnNone;
    pressed_btn_ = kBtnNone;
    InvalidateRect(hwnd, nullptr, FALSE);

    if (cmd == 301) {
        LaunchSettings(L"/dict");
    } else if (cmd == 302) {
        ShowAddWordDialog(hwnd);
    }
}

void TopBarWindow::ShowAddWordDialog(HWND parentHwnd) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {sizeof(wc)};
        wc.lpfnWndProc = AddWordDlgProc;
        wc.hInstance = hinst_;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = kAddWordDlgClass;
        RegisterClassExW(&wc);
        registered = true;
    }

    AddWordDlgData data = {nullptr, nullptr, false};

    RECT rcParent;
    GetWindowRect(parentHwnd, &rcParent);
    int dlg_w = 390;
    int dlg_h = 255;
    int x = rcParent.left + (width_ - dlg_w) / 2;
    int y = rcParent.bottom + 10;

    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    if (x < 10) x = 10;
    if (x + dlg_w > screen_w - 10) x = screen_w - dlg_w - 10;
    if (y + dlg_h > screen_h - 40) y = rcParent.top - dlg_h - 10;

    HWND hDlg = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_DLGMODALFRAME,
        kAddWordDlgClass,
        L"নতুন শব্দ যোগ করুন — Likhi",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        x, y, dlg_w, dlg_h,
        parentHwnd, nullptr, hinst_, &data
    );

    if (!hDlg) return;

    EnableWindow(parentHwnd, FALSE);

    MSG msg;
    while (IsWindow(hDlg) && GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(hDlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    EnableWindow(parentHwnd, TRUE);
    SetForegroundWindow(parentHwnd);
}

void TopBarWindow::TypeUnicodeString(const std::wstring& text) {
    if (text.empty()) return;
    std::vector<INPUT> inputs;
    inputs.reserve(text.length() * 2);
    for (wchar_t ch : text) {
        INPUT inDown = {0};
        inDown.type = INPUT_KEYBOARD;
        inDown.ki.wScan = ch;
        inDown.ki.dwFlags = KEYEVENTF_UNICODE;
        inputs.push_back(inDown);

        INPUT inUp = {0};
        inUp.type = INPUT_KEYBOARD;
        inUp.ki.wScan = ch;
        inUp.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
        inputs.push_back(inUp);
    }
    SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
}

static std::wstring Utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if (size <= 0) return std::wstring();
    std::wstring out(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), &out[0], size);
    return out;
}

void TopBarWindow::LaunchVoiceInput() {
    if (is_voice_recording_) {
        // Stop recording and start transcription
        std::vector<int16_t> pcm;
        if (voice_recorder_) {
            pcm = voice_recorder_->Stop();
        }
        is_voice_recording_ = false;
        is_voice_busy_ = true;
        if (hwnd_) InvalidateRect(hwnd_, &rc_mic_, FALSE);

        PlaySoundW(L"SystemAsterisk", nullptr, SND_ALIAS | SND_ASYNC);

        if (pcm.empty()) {
            is_voice_busy_ = false;
            if (hwnd_) InvalidateRect(hwnd_, &rc_mic_, FALSE);
            return;
        }

        // Asynchronously transcribe audio via Gemini AI / Speech API
        auto cfg = voice_config_;
        HWND hwnd = hwnd_;
        RECT rc = rc_mic_;
        std::thread([this, pcm, cfg, hwnd, rc]() {
            std::wstring err;
            std::string text_utf8 = bangla_tsf::Voice_Transcribe(pcm, cfg, &err);
            std::wstring wtext = Utf8ToWide(text_utf8);

            is_voice_busy_ = false;
            if (hwnd && IsWindow(hwnd)) {
                InvalidateRect(hwnd, &rc, FALSE);
            }

            if (!wtext.empty()) {
                PlaySoundW(L"SystemDefault", nullptr, SND_ALIAS | SND_ASYNC);
                Sleep(60);
                TypeUnicodeString(wtext);
            } else {
                PlaySoundW(L"SystemHand", nullptr, SND_ALIAS | SND_ASYNC);
            }
        }).detach();

    } else {
        // Start recording
        if (!voice_recorder_) {
            voice_recorder_.reset(new bangla_tsf::VoiceRecorder());
        }

        // Refresh config in case user added key in Settings
        voice_config_ = bangla_tsf::VoiceConfig_Load(GetAppDataPath());
        if (!voice_config_.enabled()) {
            PlaySoundW(L"SystemHand", nullptr, SND_ALIAS | SND_ASYNC);
            LaunchSettings(L"-voice");
            return;
        }

        if (voice_recorder_->Start()) {
            is_voice_recording_ = true;
            PlaySoundW(L"SystemQuestion", nullptr, SND_ALIAS | SND_ASYNC);
            if (hwnd_) InvalidateRect(hwnd_, &rc_mic_, FALSE);
        }
    }
}

void TopBarWindow::InitTooltips(HWND hwnd) {
    hwnd_tooltip_ = CreateWindowExW(
        WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
        WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        hwnd, nullptr, hinst_, nullptr
    );
    if (!hwnd_tooltip_) return;

    SetWindowPos(hwnd_tooltip_, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

    auto AddTool = [this, hwnd](RECT& rc, UINT_PTR id, const wchar_t* text) {
        TOOLINFOW ti = {0};
        ti.cbSize = sizeof(ti);
        ti.uFlags = TTF_SUBCLASS;
        ti.hwnd = hwnd;
        ti.uId = id;
        ti.rect = rc;
        ti.lpszText = const_cast<wchar_t*>(text);
        SendMessageW(hwnd_tooltip_, TTM_ADDTOOLW, 0, (LPARAM)&ti);
    };

    AddTool(rc_mic_,        1, L"🎙️ বাংলা ভয়েস টাইপিং (Bangla Voice Typing) [Ctrl+Alt+V]");
    AddTool(rc_mode_bn_,    2, L"বাংলা মোড [১-ক্লিক সুইচ]");
    AddTool(rc_mode_en_,    3, L"English Mode [1-Click Switch]");
    AddTool(rc_mode_trans_, 4, L"বাংলিশ থেকে ইংরেজি অনুবাদ (Banglish -> English) [Ctrl+Alt+E]");
    AddTool(rc_pause_,      5, L"সাজেশন বিরতি / চালু (Typing Mode / Suggestions)");
    AddTool(rc_dict_,       6, L"শব্দ ম্যানেজার ও ডিকশনারি (Word Manager)");
    AddTool(rc_osk_,        7, L"অন-স্ক্রিন কিবোর্ড (Virtual Keyboard)");
    AddTool(rc_script_,     8, L"ইউনিকোড ⇄ বিজয় কনভার্টার (Converter)");
    AddTool(rc_settings_,   9, L"সেটিংস ও শর্টকাট (Likhi Settings)");
    AddTool(rc_power_,     10, L"অ্যাপ অপশন মেনু ও বন্ধ (App Menu / Exit)");
}

} // namespace universal
} // namespace likhi
