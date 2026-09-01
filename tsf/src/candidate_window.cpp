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
      hfont_score_(nullptr),
      width_(250),
      height_(42) {
    memset(&caret_rect_, 0, sizeof(RECT));
}

CandidateWindow::~CandidateWindow() {
    Destroy();
}

bool CandidateWindow::Initialize(HINSTANCE hInst) {
    hinst_ = hInst;

    WNDCLASSEXW wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
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
        WS_POPUP | WS_BORDER,
        0, 0, width_, height_,
        NULL, NULL, hInst, this
    );

    if (!hwnd_) return false;

    // Create Anti-Aliased Clean Fonts
    hfont_bengali_ = CreateFontW(
        -16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        L"Nirmala UI"
    );

    hfont_number_ = CreateFontW(
        -12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
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
    if (candidates_.empty() || !hwnd_) return;

    HDC hdc = GetDC(hwnd_);
    HFONT old_font = (HFONT)SelectObject(hdc, hfont_bengali_);

    candidate_item_rects_.clear();
    int current_x = 10;
    int max_item_h = 36;

    for (size_t i = 0; i < candidates_.size(); i++) {
        SIZE num_size = {0}, text_size = {0};

        SelectObject(hdc, hfont_number_);
        std::wstring num_str = std::to_wstring(i + 1) + L". ";
        GetTextExtentPoint32W(hdc, num_str.c_str(), (int)num_str.size(), &num_size);

        SelectObject(hdc, hfont_bengali_);
        GetTextExtentPoint32W(hdc, candidates_[i].c_str(), (int)candidates_[i].size(), &text_size);

        int item_w = num_size.cx + text_size.cx + 16;
        RECT item_r = { current_x, 4, current_x + item_w, 4 + max_item_h };
        candidate_item_rects_.push_back(item_r);

        current_x += item_w + 6;
    }

    SelectObject(hdc, old_font);
    ReleaseDC(hwnd_, hdc);

    width_ = current_x + 10;
    height_ = max_item_h + 8;
}

void CandidateWindow::ShowCandidates(const std::vector<std::wstring>& candidates, size_t selected_index, const RECT& caret_rect) {
    if (candidates.empty()) {
        Hide();
        return;
    }

    candidates_ = candidates;
    selected_index_ = (selected_index < candidates.size()) ? selected_index : 0;
    caret_rect_ = caret_rect;

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

        // Draw index number
        SelectObject(memDC, hfont_number_);
        SetTextColor(memDC, is_selected ? RGB(26, 115, 232) : RGB(128, 134, 139));
        std::wstring num_str = std::to_wstring(i + 1) + L".";
        RECT num_rect = item_rect;
        num_rect.left += 6;
        num_rect.top += 8;
        DrawTextW(memDC, num_str.c_str(), (int)num_str.size(), &num_rect, DT_LEFT | DT_NOCLIP);

        // Draw Bengali Word
        SelectObject(memDC, hfont_bengali_);
        SetTextColor(memDC, is_selected ? RGB(26, 115, 232) : RGB(32, 33, 36));
        RECT text_rect = item_rect;
        text_rect.left += 22;
        text_rect.top += 6;
        DrawTextW(memDC, candidates_[i].c_str(), (int)candidates_[i].size(), &text_rect, DT_LEFT | DT_NOCLIP);
    }

    // Border
    HPEN border_pen = CreatePen(PS_SOLID, 1, RGB(218, 220, 224));
    HGDIOBJ old_pen = SelectObject(memDC, border_pen);
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
