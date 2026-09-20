#include "../include/candidate_window.h"
#include <algorithm>

namespace bangla_tsf {

static const wchar_t* WINDOW_CLASS_NAME = L"PC_Bangla_Candidate_Window_Class";

static std::wstring ToBengaliDigits(size_t num) {
    std::wstring s = std::to_wstring(num);
    std::wstring res;
    for (wchar_t ch : s) {
        if (ch >= L'0' && ch <= L'9') {
            res.push_back((wchar_t)(0x09E6 + (ch - L'0')));
        } else {
            res.push_back(ch);
        }
    }
    return res + L".";
}

CandidateWindow::CandidateWindow()
    : hwnd_(nullptr),
      hinst_(nullptr),
      is_visible_(false),
      selected_index_(0),
      hfont_bengali_(nullptr),
      hfont_number_(nullptr),
      hfont_header_(nullptr),
      width_(115),
      height_(190) {
    memset(&caret_rect_, 0, sizeof(RECT));
    memset(&up_button_rect_, 0, sizeof(RECT));
    memset(&down_button_rect_, 0, sizeof(RECT));
}

CandidateWindow::~CandidateWindow() {
    Destroy();
}

bool CandidateWindow::Initialize(HINSTANCE hInst) {
    hinst_ = hInst;

    WNDCLASSEXW wcex;
    ZeroMemory(&wcex, sizeof(wcex));
    wcex.cbSize = sizeof(WNDCLASSEXW);
    // CS_DROPSHADOW provides the soft elevation drop shadow seen in Google Translate
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS | CS_DROPSHADOW;
    wcex.lpfnWndProc = CandidateWindow::WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = sizeof(CandidateWindow*);
    wcex.hInstance = hInst;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wcex.lpszClassName = WINDOW_CLASS_NAME;

    RegisterClassExW(&wcex);

    hwnd_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        WINDOW_CLASS_NAME,
        L"Bangla Suggestions",
        WS_POPUP,
        0, 0, width_, height_,
        NULL, NULL, hInst, this
    );

    if (!hwnd_) return false;

    // Create Clean Modern Fonts matching Google Translate / Input Tools
    hfont_header_ = CreateFontW(
        -14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );

    hfont_bengali_ = CreateFontW(
        -15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Vrinda"
    );
    if (!hfont_bengali_) {
        hfont_bengali_ = CreateFontW(
            -15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            L"Nirmala UI"
        );
    }

    hfont_number_ = CreateFontW(
        -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Nirmala UI"
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
    if (hfont_header_) {
        DeleteObject(hfont_header_);
        hfont_header_ = nullptr;
    }
    if (hinst_) {
        UnregisterClassW(WINDOW_CLASS_NAME, hinst_);
        hinst_ = nullptr;
    }
}

void CandidateWindow::UpdateDimensions() {
    if (!hwnd_) return;

    HDC hdc = GetDC(hwnd_);
    HFONT old_font = (HFONT)SelectObject(hdc, hfont_bengali_);

    const int HEADER_H = 22;
    const int ITEM_H = 23;

    int max_content_w = 60;

    // Header measurement
    if (!roman_input_.empty()) {
        SelectObject(hdc, hfont_header_);
        SIZE header_sz;
        ZeroMemory(&header_sz, sizeof(header_sz));
        GetTextExtentPoint32W(hdc, roman_input_.c_str(), (int)roman_input_.size(), &header_sz);
        if (header_sz.cx > max_content_w) max_content_w = header_sz.cx;
    }

    // Candidates measurement
    for (size_t i = 0; i < candidates_.size(); i++) {
        SIZE num_sz, text_sz;
        ZeroMemory(&num_sz, sizeof(num_sz));
        ZeroMemory(&text_sz, sizeof(text_sz));

        SelectObject(hdc, hfont_number_);
        std::wstring num_str = ToBengaliDigits(i + 1);
        GetTextExtentPoint32W(hdc, num_str.c_str(), (int)num_str.size(), &num_sz);

        SelectObject(hdc, hfont_bengali_);
        GetTextExtentPoint32W(hdc, candidates_[i].c_str(), (int)candidates_[i].size(), &text_sz);

        int line_w = num_sz.cx + text_sz.cx + 20;
        if (line_w > max_content_w) max_content_w = line_w;
    }

    SelectObject(hdc, old_font);
    ReleaseDC(hwnd_, hdc);

    // Dynamic width with sensible limits (105px to 280px)
    width_ = (std::max)(105, max_content_w + 24);
    if (width_ > 280) width_ = 280;

    candidate_item_rects_.clear();
    int cur_y = HEADER_H + 4;

    for (size_t i = 0; i < candidates_.size(); i++) {
        RECT r = { 3, cur_y, width_ - 3, cur_y + ITEM_H };
        candidate_item_rects_.push_back(r);
        cur_y += ITEM_H;
    }

    // Small square pagination buttons at the bottom: [ ^ ] [ v ]
    int btn_size = 17;
    int footer_y = cur_y + 4;
    up_button_rect_ = { 8, footer_y, 8 + btn_size, footer_y + btn_size };
    down_button_rect_ = { 8 + btn_size + 4, footer_y, 8 + btn_size * 2 + 4, footer_y + btn_size };

    height_ = footer_y + btn_size + 7;
}

void CandidateWindow::ShowCandidates(const std::vector<std::wstring>& candidates, size_t selected_index, const RECT& caret_rect, const std::wstring& roman_input) {
    if (candidates.empty()) {
        Hide();
        return;
    }

    candidates_ = candidates;
    selected_index_ = (selected_index < candidates.size()) ? selected_index : 0;
    caret_rect_ = caret_rect;
    roman_input_ = roman_input;

    UpdateDimensions();

    int pos_x = caret_rect.left;
    int pos_y = caret_rect.bottom + 4;

    // Keep on screen bounds
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);

    if (pos_x + width_ > screen_w) {
        pos_x = screen_w - width_ - 10;
    }
    if (pos_x < 10) pos_x = 10;

    if (pos_y + height_ > screen_h) {
        pos_y = caret_rect.top - height_ - 4;
    }
    if (pos_y < 10) pos_y = 10;

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
        roman_input_.clear();
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

    // 1. Pure White Background
    HBRUSH bg_brush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(memDC, &client_rect, bg_brush);
    DeleteObject(bg_brush);

    SetBkMode(memDC, TRANSPARENT);

    // 2. Header Row (Input buffer with blue caret)
    if (!roman_input_.empty()) {
        SelectObject(memDC, hfont_header_);
        SetTextColor(memDC, RGB(32, 33, 36));
        RECT header_r = { 8, 4, client_rect.right - 8, 22 };
        DrawTextW(memDC, roman_input_.c_str(), (int)roman_input_.size(), &header_r, DT_LEFT | DT_TOP | DT_SINGLELINE);

        // Blue cursor line right after roman input
        SIZE txt_sz;
        ZeroMemory(&txt_sz, sizeof(txt_sz));
        GetTextExtentPoint32W(memDC, roman_input_.c_str(), (int)roman_input_.size(), &txt_sz);
        int caret_x = 8 + txt_sz.cx + 1;
        HPEN caret_pen = CreatePen(PS_SOLID, 2, RGB(26, 115, 232));
        HPEN old_pen = (HPEN)SelectObject(memDC, caret_pen);
        MoveToEx(memDC, caret_x, 4, NULL);
        LineTo(memDC, caret_x, 19);
        SelectObject(memDC, old_pen);
        DeleteObject(caret_pen);
    }

    // 3. Vertical Candidate Rows (Matching screenshot)
    for (size_t i = 0; i < candidates_.size() && i < candidate_item_rects_.size(); i++) {
        const RECT& item_rect = candidate_item_rects_[i];
        bool is_selected = (i == selected_index_);

        if (is_selected) {
            // Selected candidate background highlight: Google soft gray (#F1F3F4)
            HBRUSH sel_brush = CreateSolidBrush(RGB(241, 243, 244));
            FillRect(memDC, &item_rect, sel_brush);
            DeleteObject(sel_brush);
        }

        // Bengali number prefix: ১., ২., ৩., etc.
        SelectObject(memDC, hfont_number_);
        SetTextColor(memDC, RGB(95, 99, 104));
        std::wstring num_str = ToBengaliDigits(i + 1);
        RECT num_r = item_rect;
        num_r.left += 6;
        num_r.right = num_r.left + 16;
        DrawTextW(memDC, num_str.c_str(), (int)num_str.size(), &num_r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Bengali candidate word
        SelectObject(memDC, hfont_bengali_);
        SetTextColor(memDC, RGB(32, 33, 36));
        RECT text_r = item_rect;
        text_r.left += 23;
        DrawTextW(memDC, candidates_[i].c_str(), (int)candidates_[i].size(), &text_r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // 4. Footer Pagination Buttons: [ ^ ] [ v ]
    HBRUSH btn_border_brush = CreateSolidBrush(RGB(218, 220, 224));
    FrameRect(memDC, &up_button_rect_, btn_border_brush);
    FrameRect(memDC, &down_button_rect_, btn_border_brush);
    DeleteObject(btn_border_brush);

    HPEN chevron_pen = CreatePen(PS_SOLID, 1, RGB(95, 99, 104));
    HPEN old_pen = (HPEN)SelectObject(memDC, chevron_pen);

    // Up chevron ^
    int up_cx = (up_button_rect_.left + up_button_rect_.right) / 2;
    int up_cy = (up_button_rect_.top + up_button_rect_.bottom) / 2;
    MoveToEx(memDC, up_cx - 4, up_cy + 2, NULL);
    LineTo(memDC, up_cx, up_cy - 2);
    LineTo(memDC, up_cx + 4, up_cy + 2);

    // Down chevron v
    int dn_cx = (down_button_rect_.left + down_button_rect_.right) / 2;
    int dn_cy = (down_button_rect_.top + down_button_rect_.bottom) / 2;
    MoveToEx(memDC, dn_cx - 4, dn_cy - 2, NULL);
    LineTo(memDC, dn_cx, dn_cy + 2);
    LineTo(memDC, dn_cx + 4, dn_cy - 2);

    SelectObject(memDC, old_pen);
    DeleteObject(chevron_pen);

    // 5. Outer 1px Border (Clean light gray #DADCE0)
    HPEN border_pen = CreatePen(PS_SOLID, 1, RGB(218, 220, 224));
    old_pen = (HPEN)SelectObject(memDC, border_pen);
    SelectObject(memDC, GetStockObject(NULL_BRUSH));
    Rectangle(memDC, client_rect.left, client_rect.top, client_rect.right, client_rect.bottom);
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
    (void)hWnd;
    POINT pt = { x, y };

    if (PtInRect(&up_button_rect_, pt)) {
        SelectPrev();
        return;
    }
    if (PtInRect(&down_button_rect_, pt)) {
        SelectNext();
        return;
    }

    for (size_t i = 0; i < candidate_item_rects_.size(); i++) {
        if (PtInRect(&candidate_item_rects_[i], pt)) {
            selected_index_ = i;
            if (selection_callback_) {
                selection_callback_(i);
            }
            break;
        }
    }
}

void CandidateWindow::OnMouseMove(HWND hWnd, int x, int y) {
    POINT pt = { x, y };
    for (size_t i = 0; i < candidate_item_rects_.size(); i++) {
        if (PtInRect(&candidate_item_rects_[i], pt)) {
            if (selected_index_ != i) {
                selected_index_ = i;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            break;
        }
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
            case WM_MOUSEACTIVATE:
                return MA_NOACTIVATE;
        }
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

} // namespace bangla_tsf
