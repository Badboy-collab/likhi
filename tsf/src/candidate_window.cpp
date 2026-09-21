#include "../include/candidate_window.h"
#include <algorithm>

namespace bangla_tsf {


static const wchar_t* WINDOW_CLASS_NAME = L"PC_Bangla_Candidate_Window_Class";

CandidateWindow::CandidateWindow()
    : hwnd_(nullptr),
      hinst_(nullptr),
      is_visible_(false),
      selected_index_(0),
      hfont_bengali_(nullptr),
      hfont_number_(nullptr),
      hfont_header_(nullptr),
      hfont_hint_(nullptr),
      width_(140),
      height_(200) {
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

    // Enable Windows 11 Rounded Corners dynamically if supported by OS
    typedef HRESULT(WINAPI* PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
    HMODULE hDwm = LoadLibraryW(L"dwmapi.dll");
    if (hDwm) {
        PFN_DwmSetWindowAttribute pfnDwmSetWindowAttribute =
            (PFN_DwmSetWindowAttribute)GetProcAddress(hDwm, "DwmSetWindowAttribute");
        if (pfnDwmSetWindowAttribute) {
            DWORD corner_preference = 2; // DWMWCP_ROUND (Windows 11 modern rounded corners)
            pfnDwmSetWindowAttribute(hwnd_, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */,
                                     &corner_preference, sizeof(corner_preference));
        }
        FreeLibrary(hDwm);
    }

    // Create Clean Modern Fonts matching Windows 11 Fluent UI
    hfont_header_ = CreateFontW(
        -13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI Variable Text"
    );
    if (!hfont_header_) {
        hfont_header_ = CreateFontW(
            -13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            L"Segoe UI"
        );
    }

    hfont_bengali_ = CreateFontW(
        -17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Nirmala UI"
    );
    if (!hfont_bengali_) {
        hfont_bengali_ = CreateFontW(
            -17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
            L"Vrinda"
        );
    }

    hfont_number_ = CreateFontW(
        -11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI"
    );

    hfont_hint_ = CreateFontW(
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
    if (hfont_header_) {
        DeleteObject(hfont_header_);
        hfont_header_ = nullptr;
    }
    if (hfont_hint_) {
        DeleteObject(hfont_hint_);
        hfont_hint_ = nullptr;
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

    const int HEADER_H = 26;
    const int ITEM_H = 28;
    const int FOOTER_H = 24;

    int max_content_w = 120;

    // Header measurement
    if (!roman_input_.empty()) {
        SelectObject(hdc, hfont_header_);
        SIZE header_sz;
        ZeroMemory(&header_sz, sizeof(header_sz));
        GetTextExtentPoint32W(hdc, roman_input_.c_str(), (int)roman_input_.size(), &header_sz);
        int header_w = header_sz.cx + 60;
        if (header_w > max_content_w) max_content_w = header_w;
    }

    // Candidates measurement
    for (size_t i = 0; i < candidates_.size(); i++) {
        SIZE text_sz;
        ZeroMemory(&text_sz, sizeof(text_sz));

        SelectObject(hdc, hfont_bengali_);
        GetTextExtentPoint32W(hdc, candidates_[i].c_str(), (int)candidates_[i].size(), &text_sz);

        int line_w = 40 + text_sz.cx + 16;
        if (line_w > max_content_w) max_content_w = line_w;
    }

    SelectObject(hdc, old_font);
    ReleaseDC(hwnd_, hdc);

    // Dynamic width with sensible limits (150px to 340px)
    width_ = (std::max)(150, max_content_w);
    if (width_ > 340) width_ = 340;

    candidate_item_rects_.clear();
    int cur_y = HEADER_H + 4;

    for (size_t i = 0; i < candidates_.size(); i++) {
        RECT r = { 4, cur_y, width_ - 4, cur_y + ITEM_H };
        candidate_item_rects_.push_back(r);
        cur_y += ITEM_H;
    }

    // Footer buttons and hint
    int btn_size = 18;
    int footer_y = cur_y + 4;
    up_button_rect_ = { 8, footer_y, 8 + btn_size, footer_y + btn_size };
    down_button_rect_ = { 8 + btn_size + 4, footer_y, 8 + btn_size * 2 + 4, footer_y + btn_size };

    height_ = footer_y + FOOTER_H;
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

    // 1. Fluent background (Pure white card)
    HBRUSH bg_brush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(memDC, &client_rect, bg_brush);
    DeleteObject(bg_brush);

    SetBkMode(memDC, TRANSPARENT);

    const int HEADER_H = 26;

    // 2. Header Area
    RECT header_bg = { 0, 0, client_rect.right, HEADER_H };
    HBRUSH hdr_bg_brush = CreateSolidBrush(RGB(249, 250, 252));
    FillRect(memDC, &header_bg, hdr_bg_brush);
    DeleteObject(hdr_bg_brush);

    // Header divider line
    HPEN div_pen = CreatePen(PS_SOLID, 1, RGB(232, 235, 240));
    HPEN old_pen = (HPEN)SelectObject(memDC, div_pen);
    MoveToEx(memDC, 0, HEADER_H, NULL);
    LineTo(memDC, client_rect.right, HEADER_H);

    // Input buffer with blue cursor
    if (!roman_input_.empty()) {
        SelectObject(memDC, hfont_header_);
        SetTextColor(memDC, RGB(24, 28, 36));
        RECT header_r = { 10, 2, client_rect.right - 50, HEADER_H };
        DrawTextW(memDC, roman_input_.c_str(), (int)roman_input_.size(), &header_r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        // Vibrant blue cursor
        SIZE txt_sz;
        ZeroMemory(&txt_sz, sizeof(txt_sz));
        GetTextExtentPoint32W(memDC, roman_input_.c_str(), (int)roman_input_.size(), &txt_sz);
        int caret_x = 10 + txt_sz.cx + 2;
        HPEN caret_pen = CreatePen(PS_SOLID, 2, RGB(0, 103, 192));
        SelectObject(memDC, caret_pen);
        MoveToEx(memDC, caret_x, 5, NULL);
        LineTo(memDC, caret_x, HEADER_H - 5);
        DeleteObject(caret_pen);
    }

    // App Brand badge on header right: "Likhi"
    SelectObject(memDC, hfont_hint_);
    SetTextColor(memDC, RGB(160, 166, 178));
    RECT brand_r = { client_rect.right - 48, 0, client_rect.right - 8, HEADER_H };
    DrawTextW(memDC, L"Likhi", 5, &brand_r, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    // 3. Vertical Candidate Rows
    for (size_t i = 0; i < candidates_.size() && i < candidate_item_rects_.size(); i++) {
        const RECT& item_rect = candidate_item_rects_[i];
        bool is_selected = (i == selected_index_);

        if (is_selected) {
            // Selected item: Windows 11 Fluent Soft Blue Pill with 6px corner radius
            HBRUSH sel_brush = CreateSolidBrush(RGB(236, 243, 254));
            HPEN sel_pen = CreatePen(PS_SOLID, 1, RGB(196, 220, 252));
            SelectObject(memDC, sel_brush);
            SelectObject(memDC, sel_pen);
            RoundRect(memDC, item_rect.left, item_rect.top, item_rect.right, item_rect.bottom, 6, 6);
            DeleteObject(sel_pen);
            DeleteObject(sel_brush);

            // Left vertical accent bar (Windows 11 Fluent selection indicator)
            HBRUSH bar_brush = CreateSolidBrush(RGB(0, 103, 192));
            RECT bar_r = { item_rect.left + 2, item_rect.top + 5, item_rect.left + 5, item_rect.bottom - 5 };
            FillRect(memDC, &bar_r, bar_brush);
            DeleteObject(bar_brush);
        }

        // Hotkey number badge pill: [ 1 ], [ 2 ], [ 3 ] ...
        RECT badge_r = { item_rect.left + 9, item_rect.top + 4, item_rect.left + 27, item_rect.bottom - 4 };
        HBRUSH badge_bg = CreateSolidBrush(is_selected ? RGB(216, 232, 255) : RGB(242, 244, 247));
        HPEN badge_border = CreatePen(PS_SOLID, 1, is_selected ? RGB(180, 210, 250) : RGB(228, 230, 235));
        SelectObject(memDC, badge_bg);
        SelectObject(memDC, badge_border);
        RoundRect(memDC, badge_r.left, badge_r.top, badge_r.right, badge_r.bottom, 4, 4);
        DeleteObject(badge_border);
        DeleteObject(badge_bg);

        // Hotkey number text
        SelectObject(memDC, hfont_number_);
        SetTextColor(memDC, is_selected ? RGB(0, 95, 184) : RGB(100, 106, 118));
        std::wstring num_str = std::to_wstring(i + 1);
        DrawTextW(memDC, num_str.c_str(), (int)num_str.size(), &badge_r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Bengali candidate word
        SelectObject(memDC, hfont_bengali_);
        SetTextColor(memDC, is_selected ? RGB(10, 20, 35) : RGB(32, 34, 38));
        RECT text_r = item_rect;
        text_r.left += 34;
        text_r.right -= 8;
        DrawTextW(memDC, candidates_[i].c_str(), (int)candidates_[i].size(), &text_r, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // 4. Footer Row
    int footer_top = (up_button_rect_.top > 0) ? up_button_rect_.top - 4 : (client_rect.bottom - 24);
    SelectObject(memDC, div_pen);
    MoveToEx(memDC, 0, footer_top, NULL);
    LineTo(memDC, client_rect.right, footer_top);

    // Footer Pagination Buttons: [ ∧ ] [ ∨ ]
    HBRUSH btn_bg_brush = CreateSolidBrush(RGB(246, 248, 250));
    HPEN btn_border_pen = CreatePen(PS_SOLID, 1, RGB(220, 224, 230));
    SelectObject(memDC, btn_bg_brush);
    SelectObject(memDC, btn_border_pen);
    RoundRect(memDC, up_button_rect_.left, up_button_rect_.top, up_button_rect_.right, up_button_rect_.bottom, 4, 4);
    RoundRect(memDC, down_button_rect_.left, down_button_rect_.top, down_button_rect_.right, down_button_rect_.bottom, 4, 4);
    DeleteObject(btn_border_pen);
    DeleteObject(btn_bg_brush);

    HPEN chevron_pen = CreatePen(PS_SOLID, 1, RGB(90, 95, 105));
    SelectObject(memDC, chevron_pen);

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

    DeleteObject(chevron_pen);

    // Keyboard navigation hint text on footer right
    SelectObject(memDC, hfont_hint_);
    SetTextColor(memDC, RGB(140, 146, 158));
    RECT hint_r = { down_button_rect_.right + 8, footer_top, client_rect.right - 8, client_rect.bottom };
    DrawTextW(memDC, L"Tab \x21B9  Enter \x21B5", 13, &hint_r, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    // 5. Outer 1px Crisp Border (Windows 11 light neutral #D1D5DB)
    HPEN border_pen = CreatePen(PS_SOLID, 1, RGB(210, 215, 222));
    SelectObject(memDC, border_pen);
    SelectObject(memDC, GetStockObject(NULL_BRUSH));
    Rectangle(memDC, client_rect.left, client_rect.top, client_rect.right, client_rect.bottom);
    DeleteObject(border_pen);

    SelectObject(memDC, old_pen);
    DeleteObject(div_pen);

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
