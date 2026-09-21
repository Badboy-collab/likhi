#include "../include/candidate_window.h"
#include <algorithm>

namespace bangla_tsf {

static const wchar_t* WINDOW_CLASS_NAME = L"PC_Bangla_Candidate_Window_Class";

namespace {

// Private message that carries a cloud result from the worker thread onto the
// UI thread (the candidate window is owned by the UI thread).
const UINT kCloudResultMsg = WM_APP + 0x6A;

// Same idea for the voice recognition result (worker thread -> UI thread).
const UINT kVoiceResultMsg = WM_APP + 0x6B;

// Bengali numerals (U+09E6..U+09EF) for the candidate index. The strip is a
// vertical IME list, so numbering uses the user's own script.
const wchar_t* kBengaliDigits = L"\u09E6\u09E7\u09E8\u09E9\u09EA\u09EB\u09EC\u09ED\u09EE\u09EF";

std::wstring IndexLabel(size_t one_based) {
    std::wstring digits;
    size_t n = one_based;
    do {
        digits.insert(digits.begin(), kBengaliDigits[n % 10]);
        n /= 10;
    } while (n > 0);
    digits += L".";
    return digits;
}

// Selection uses number keys 1..9, so the strip never shows more rows.
const size_t kMaxVisible = 9;

// Vertical strip metrics. Compact but airy, matching the reference UI
// (bangla_extracted_engine/index.html .suggestion-item: 8px padding, 12px
// index, 8px radius, soft shadow).
const int kItemH    = 26;
const int kItemGapY = 0;
const int kPadX     = 12;
const int kTopPad   = 4;
const int kNumColW  = 28;
const int kCornerR  = 8;

// Microphone row under the candidate rows.
const int kVoiceRowH    = 24;
const int kVoiceIconBox = 22;
const int kMinWidth     = 132;

struct CloudResultPayload {
    std::wstring word;
    size_t generation;
    std::vector<std::wstring> candidates;
};

struct VoiceResultPayload {
    std::wstring text;
    std::wstring error;
};

// Small GDI microphone glyph (capsule + cradle + stand).
void DrawMicGlyph(HDC dc, int cx, int top, COLORREF color) {
    HBRUSH brush = CreateSolidBrush(color);
    HPEN pen = CreatePen(PS_SOLID, 1, color);
    HGDIOBJ old_brush = SelectObject(dc, brush);
    HGDIOBJ old_pen = SelectObject(dc, pen);

    RoundRect(dc, cx - 3, top + 2, cx + 3, top + 11, 6, 6);       // capsule
    SelectObject(dc, GetStockObject(NULL_BRUSH));
    SelectObject(dc, GetStockObject(NULL_PEN));
    Arc(dc, cx - 6, top + 6, cx + 6, top + 16, cx - 6, top + 11, cx + 6, top + 11);
    SelectObject(dc, pen);
    MoveToEx(dc, cx, top + 15, nullptr);
    LineTo(dc, cx, top + 18);
    MoveToEx(dc, cx - 4, top + 18, nullptr);
    LineTo(dc, cx + 4, top + 18);

    SelectObject(dc, old_brush);
    SelectObject(dc, old_pen);
    DeleteObject(brush);
    DeleteObject(pen);
}

} // namespace

CandidateWindow::CandidateWindow()
    : hwnd_(nullptr),
      hinst_(nullptr),
      is_visible_(false),
      selected_index_(0),
      hfont_bengali_(nullptr),
      hfont_number_(nullptr),
      hfont_score_(nullptr),
      width_(250),
      height_(42),
      voice_state_(VoiceState::Off) {
    memset(&caret_rect_, 0, sizeof(RECT));
}

CandidateWindow::~CandidateWindow() {
    Destroy();
}

bool CandidateWindow::Initialize(HINSTANCE hInst) {
    hinst_ = hInst;

    WNDCLASSEXW wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS | CS_DROPSHADOW;
    wcex.lpfnWndProc = CandidateWindow::WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = sizeof(CandidateWindow*);
    wcex.hInstance = hInst;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = WINDOW_CLASS_NAME;

    RegisterClassExW(&wcex);

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        WINDOW_CLASS_NAME,
        L"Bangla Suggestions",
        WS_POPUP,  // rounded custom border + drop shadow instead of the 3D system border
        0, 0, width_, height_,
        NULL, NULL, hInst, this
    );

    if (!hwnd_) return false;

    // Create Anti-Aliased Clean Fonts
    hfont_bengali_ = CreateFontW(
        -15, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Nirmala UI"
    );

    hfont_number_ = CreateFontW(
        -11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );

    return true;
}

void CandidateWindow::Destroy() {
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    if (hfont_bengali_) {
        DeleteObject(hfont_bengali_);
        hfont_bengali_ = nullptr;
    }
    if (hfont_number_) {
        DeleteObject(hfont_number_);
        hfont_number_ = nullptr;
    }
    if (hinst_) {
        UnregisterClassW(WINDOW_CLASS_NAME, hinst_);
        hinst_ = nullptr;
    }
}

void CandidateWindow::UpdateDimensions() {
    if (!hwnd_) return;

    HDC hdc = GetDC(hwnd_);
    HFONT old_font = (HFONT)SelectObject(hdc, hfont_number_);

    candidate_item_rects_.clear();
    mic_rect_ = RECT{0, 0, 0, 0};

    // VERTICAL strip: one candidate per row, number in a left column so the
    // Bengali words line up. Width = widest row, height = rows + padding.
    const size_t count = (candidates_.size() < kMaxVisible) ? candidates_.size() : kMaxVisible;

    int widest = 0;
    int y = kTopPad;
    for (size_t i = 0; i < count; i++) {
        SIZE num_size = {0}, text_size = {0};

        SelectObject(hdc, hfont_number_);
        const std::wstring num_str = IndexLabel(i + 1);
        GetTextExtentPoint32W(hdc, num_str.c_str(), (int)num_str.size(), &num_size);

        SelectObject(hdc, hfont_bengali_);
        GetTextExtentPoint32W(hdc, candidates_[i].c_str(), (int)candidates_[i].size(), &text_size);

        int item_w = kPadX + num_size.cx + 8 + text_size.cx + kPadX;
        if (item_w > widest) widest = item_w;

        RECT item_r = { 6, y, 6 + item_w, y + kItemH };
        candidate_item_rects_.push_back(item_r);

        y += kItemH + kItemGapY;
    }

    for (RECT& r : candidate_item_rects_) r.right = r.left + widest; // full-width rows

    // Microphone row (voice input) sits under the candidates, separated by a
    // hairline. This is the clickable microphone of the strip.
    if (voice_state_ != VoiceState::Off) {
        SIZE label_size = {0};
        SelectObject(hdc, hfont_bengali_);
        const std::wstring label = VoiceLabel();
        if (!label.empty()) {
            GetTextExtentPoint32W(hdc, label.c_str(), (int)label.size(), &label_size);
        }
        const int voice_w = kPadX + kVoiceIconBox + label_size.cx + kPadX + 24;
        if (voice_w > widest) widest = voice_w;
        mic_rect_ = RECT{6, y, 6 + widest, y + kVoiceRowH};
        y += kVoiceRowH;
    }

    SelectObject(hdc, old_font);
    ReleaseDC(hwnd_, hdc);

    if (widest < kMinWidth) widest = kMinWidth;
    width_ = widest + 12;
    height_ = y + kTopPad;
}

void CandidateWindow::ShowCandidates(const std::vector<std::wstring>& candidates, size_t selected_index, const RECT& caret_rect) {
    // An empty list only hides the strip when the microphone row is inactive:
    // voice typing shows the strip on its own while nothing is composed.
    if (candidates.empty() && voice_state_ == VoiceState::Off) {
        Hide();
        return;
    }

    candidates_ = candidates;
    selected_index_ = (selected_index < candidates.size()) ? selected_index : 0;
    caret_rect_ = caret_rect;

    UpdateDimensions();

    int pos_x = caret_rect.left;
    int pos_y = caret_rect.bottom + 4;

    // Keep on screen bounds — per-monitor aware. Clamping against the primary
    // monitor only (SM_CXSCREEN/SM_CYSCREEN) parks the popup off-screen on
    // multi-monitor / per-monitor DPI setups.
    HMONITOR hMonitor = MonitorFromPoint(POINT{pos_x, pos_y}, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {};
    mi.cbSize = sizeof(mi);
    RECT work = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    if (GetMonitorInfoW(hMonitor, &mi)) {
        work = mi.rcWork;
    }

    if (pos_y + height_ > work.bottom) {
        pos_y = caret_rect.top - height_ - 4;  // flip above the text line
    }
    if (pos_x + width_ > work.right) pos_x = work.right - width_ - 8;
    if (pos_x < work.left)           pos_x = work.left + 8;
    if (pos_y < work.top)            pos_y = work.top + 8;

    // Rounded corners so the popup looks like the reference suggestion box.
    HRGN region = CreateRoundRectRgn(0, 0, width_ + 1, height_ + 1,
                                     kCornerR * 2, kCornerR * 2);
    if (region) SetWindowRgn(hwnd_, region, TRUE);  // window owns the region now

    SetWindowPos(
        hwnd_, HWND_TOPMOST,
        pos_x, pos_y, width_, height_,
        SWP_SHOWWINDOW | SWP_NOACTIVATE
    );

    is_visible_ = true;
    InvalidateRect(hwnd_, NULL, TRUE);
}

void CandidateWindow::Hide() {
    if (is_visible_ && hwnd_) {
        ShowWindow(hwnd_, SW_HIDE);
        is_visible_ = false;
        candidates_.clear();
        candidate_item_rects_.clear();
    }
}

void CandidateWindow::PostCloudResult(const std::wstring& word, size_t generation,
                                      const std::vector<std::wstring>& candidates) {
    if (!hwnd_) return;
    CloudResultPayload* p = new CloudResultPayload{word, generation, candidates};
    if (!PostMessageW(hwnd_, kCloudResultMsg, 0, reinterpret_cast<LPARAM>(p))) {
        delete p; // window gone — nothing to deliver
    }
}

void CandidateWindow::SelectNext() {
    if (candidates_.empty()) return;
    selected_index_ = (selected_index_ + 1) % candidates_.size();
    if (hwnd_) InvalidateRect(hwnd_, NULL, FALSE);
}

void CandidateWindow::SelectPrev() {
    if (candidates_.empty()) return;
    if (selected_index_ == 0) {
        selected_index_ = candidates_.size() - 1;
    } else {
        selected_index_--;
    }
    if (hwnd_) InvalidateRect(hwnd_, NULL, FALSE);
}

void CandidateWindow::OnPaint(HWND hWnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);

    RECT client_rect;
    GetClientRect(hWnd, &client_rect);

    // Double-buffered DC
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBitmap = CreateCompatibleBitmap(hdc, client_rect.right, client_rect.bottom);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

    // Background fill (Modern soft white/slate)
    HBRUSH bg_brush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(memDC, &client_rect, bg_brush);
    DeleteObject(bg_brush);

    // Draw Candidates
    for (size_t i = 0; i < candidates_.size() && i < candidate_item_rects_.size(); i++) {
        const RECT& item_rect = candidate_item_rects_[i];
        bool is_selected = (i == selected_index_);

        if (is_selected) {
            // Highlight Pill
            HBRUSH highlight_brush = CreateSolidBrush(RGB(232, 240, 254)); // Soft Google Blue highlight
            HPEN highlight_pen = CreatePen(PS_SOLID, 1, RGB(168, 199, 250));
            HGDIOBJ old_brush = SelectObject(memDC, highlight_brush);
            HGDIOBJ old_pen = SelectObject(memDC, highlight_pen);

            RoundRect(memDC, item_rect.left, item_rect.top, item_rect.right, item_rect.bottom, 6, 6);

            SelectObject(memDC, old_brush);
            SelectObject(memDC, old_pen);
            DeleteObject(highlight_brush);
            DeleteObject(highlight_pen);
        }

        SetBkMode(memDC, TRANSPARENT);

        // Draw index number (Bengali numerals, vertically centered in its row)
        SelectObject(memDC, hfont_number_);
        SetTextColor(memDC, is_selected ? RGB(26, 115, 232) : RGB(95, 99, 104));
        const std::wstring num_str = IndexLabel(i + 1);
        RECT num_rect = item_rect;
        num_rect.left += 8;
        DrawTextW(memDC, num_str.c_str(), (int)num_str.size(), &num_rect,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);

        // Draw Bengali Word (own column -> all words align)
        SelectObject(memDC, hfont_bengali_);
        SetTextColor(memDC, is_selected ? RGB(26, 115, 232) : RGB(32, 33, 36));
        RECT text_rect = item_rect;
        text_rect.left += kNumColW;
        DrawTextW(memDC, candidates_[i].c_str(), (int)candidates_[i].size(), &text_rect,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP);
    }

    // ---- microphone row (voice input) ----
    if (voice_state_ != VoiceState::Off && mic_rect_.bottom > mic_rect_.top) {
        HPEN sep_pen = CreatePen(PS_SOLID, 1, RGB(232, 234, 237));
        HGDIOBJ old_sep = SelectObject(memDC, sep_pen);
        MoveToEx(memDC, mic_rect_.left + kPadX, mic_rect_.top, nullptr);
        LineTo(memDC, client_rect.right - kPadX, mic_rect_.top);
        SelectObject(memDC, old_sep);
        DeleteObject(sep_pen);

        const COLORREF mic_color = (voice_state_ == VoiceState::Recording) ? RGB(217, 48, 37)
                                 : (voice_state_ == VoiceState::Busy)      ? RGB(26, 115, 232)
                                                                          : RGB(95, 99, 104);
        const int cy = mic_rect_.top + kVoiceRowH / 2;
        DrawMicGlyph(memDC, mic_rect_.left + kPadX - 4 + kVoiceIconBox / 2, cy - 9, mic_color);

        if (voice_state_ == VoiceState::Recording) {  // live red dot
            HBRUSH dot = CreateSolidBrush(RGB(217, 48, 37));
            HGDIOBJ old_dot = SelectObject(memDC, dot);
            HGDIOBJ old_dot_pen = SelectObject(memDC, GetStockObject(NULL_PEN));
            Ellipse(memDC, mic_rect_.right - 24, cy - 4, mic_rect_.right - 16, cy + 4);
            SelectObject(memDC, old_dot);
            SelectObject(memDC, old_dot_pen);
            DeleteObject(dot);
        }

        SetBkMode(memDC, TRANSPARENT);
        SelectObject(memDC, hfont_bengali_);
        SetTextColor(memDC, mic_color);
        const std::wstring label = VoiceLabel();
        RECT label_rect = mic_rect_;
        label_rect.left += kVoiceIconBox;
        label_rect.right -= 24;
        DrawTextW(memDC, label.c_str(), (int)label.size(), &label_rect,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOCLIP | DT_END_ELLIPSIS);
    }

    // Border
    HPEN border_pen = CreatePen(PS_SOLID, 1, RGB(218, 220, 224));
    HGDIOBJ old_pen = SelectObject(memDC, border_pen);
    SelectObject(memDC, GetStockObject(NULL_BRUSH));
    RoundRect(memDC, client_rect.left, client_rect.top, client_rect.right - 1,
              client_rect.bottom - 1, kCornerR * 2, kCornerR * 2);
    SelectObject(memDC, old_pen);
    DeleteObject(border_pen);

    // Blit to screen
    BitBlt(hdc, 0, 0, client_rect.right, client_rect.bottom, memDC, 0, 0, SRCCOPY);

    SelectObject(memDC, oldBitmap);
    DeleteObject(memBitmap);
    DeleteDC(memDC);

    EndPaint(hWnd, &ps);
}

void CandidateWindow::OnLButtonDown(HWND hWnd, int x, int y) {
    // Microphone row first: it is a button, not a candidate.
    if (voice_state_ != VoiceState::Off &&
        PtInRect(&mic_rect_, POINT{x, y}) && voice_toggle_cb_) {
        voice_toggle_cb_();
        return;
    }
    for (size_t i = 0; i < candidate_item_rects_.size(); i++) {
        if (PtInRect(&candidate_item_rects_[i], POINT{x, y})) {
            selected_index_ = i;
            if (selection_callback_) {
                selection_callback_(i);
            }
            break;
        }
    }
}

void CandidateWindow::OnMouseMove(HWND hWnd, int x, int y) {
    for (size_t i = 0; i < candidate_item_rects_.size(); i++) {
        if (PtInRect(&candidate_item_rects_[i], POINT{x, y})) {
            if (selected_index_ != i) {
                selected_index_ = i;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        }
    }
}

std::wstring CandidateWindow::VoiceLabel() const {
    if (!voice_label_.empty()) return voice_label_;
    switch (voice_state_) {
        case VoiceState::Recording: return L"\u09B6\u09C1\u09A8\u099B\u09BF...";
        case VoiceState::Busy:      return L"\u09B2\u09BF\u0996\u099B\u09BF...";
        default:                    return L"\u09AD\u09AF\u09BC\u09C7\u09B8";
    }
}

void CandidateWindow::SetVoiceState(VoiceState state, const std::wstring& label) {
    voice_state_ = state;
    voice_label_ = label;
    if (!hwnd_) return;
    if (state == VoiceState::Off && candidates_.empty()) {
        Hide();
        return;
    }
    if (!is_visible_) return;  // next Show* picks the new state up

    RECT wr = {0, 0, 0, 0};
    GetWindowRect(hwnd_, &wr);
    UpdateDimensions();

    HRGN region = CreateRoundRectRgn(0, 0, width_ + 1, height_ + 1,
                                     kCornerR * 2, kCornerR * 2);
    if (region) SetWindowRgn(hwnd_, region, TRUE);

    SetWindowPos(hwnd_, HWND_TOPMOST, wr.left, wr.top, width_, height_,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(hwnd_, NULL, TRUE);
}

void CandidateWindow::ShowVoiceStatus(const RECT& anchor_rect) {
    if (voice_state_ == VoiceState::Off) return;
    ShowCandidates(candidates_, selected_index_, anchor_rect);
}

void CandidateWindow::PostVoiceResult(const std::wstring& text, const std::wstring& error) {
    if (!hwnd_) return;
    VoiceResultPayload* p = new VoiceResultPayload{text, error};
    if (!PostMessageW(hwnd_, kVoiceResultMsg, 0, reinterpret_cast<LPARAM>(p))) {
        delete p;
    }
}

LRESULT CALLBACK CandidateWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    CandidateWindow* pThis = nullptr;
    if (message == WM_NCCREATE || message == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        if (cs && cs->lpCreateParams) {
            pThis = reinterpret_cast<CandidateWindow*>(cs->lpCreateParams);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        }
    } else {
        pThis = reinterpret_cast<CandidateWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (pThis) {
        switch (message) {
            case WM_PAINT:
                pThis->OnPaint(hWnd);
                return 0;
            case WM_LBUTTONDOWN:
                pThis->OnLButtonDown(hWnd, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
                return 0;
            case WM_MOUSEMOVE:
                pThis->OnMouseMove(hWnd, (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam));
                return 0;
            case kVoiceResultMsg: {
                VoiceResultPayload* p = reinterpret_cast<VoiceResultPayload*>(lParam);
                if (p) {
                    VoiceResultCallback cb = pThis->voice_result_cb_;
                    if (cb) cb(p->text, p->error);
                    delete p;
                }
                return 0;
            }
            case WM_MOUSEACTIVATE:
                return MA_NOACTIVATE;
            case kCloudResultMsg: {
                CloudResultPayload* p = reinterpret_cast<CloudResultPayload*>(lParam);
                if (p) {
                    if (pThis && pThis->cloud_cb_) {
                        pThis->cloud_cb_(p->word, p->generation, p->candidates);
                    }
                    delete p;
                }
                return 0;
            }
        }
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

} // namespace bangla_tsf
