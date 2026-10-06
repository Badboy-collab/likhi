#pragma once
#include <windows.h>
#include <windowsx.h>
#include <string>
#include <vector>
#include <algorithm>

// Expects SectionID and IDC_* constants from settings_main.cpp

namespace likhi_why {

static const int TOTAL_HEIGHT = 3420;
static int s_scrollY = 0;
static int s_hoverBtn = -1;
static int s_pressedBtn = -1;

struct CtaButton {
    int x, y, w, h;
    const wchar_t* text;
    bool isPrimary;
};

static const CtaButton s_ctaButtons[4] = {
    { 18, 3340, 122, 38, L"⌨️ টাইপিং সেটিংস", false },
    { 148, 3340, 150, 38, L"📖 Personal Dictionary", false },
    { 306, 3340, 130, 38, L"💡 Personal Learning", false },
    { 444, 3340, 96, 38, L"🔄 Check Updates", true }
};

static void DrawBadge(HDC hdc, int x, int y, const wchar_t* text, COLORREF bg, COLORREF fg, COLORREF border, HFONT hFont) {
    HFONT hOld = (HFONT)SelectObject(hdc, hFont);
    SIZE sz;
    GetTextExtentPoint32W(hdc, text, (int)wcslen(text), &sz);
    int px = 9, py = 3;
    RECT rc = { x, y, x + sz.cx + px * 2, y + sz.cy + py * 2 };

    HBRUSH hBr = CreateSolidBrush(bg);
    HPEN hPen = CreatePen(PS_SOLID, 1, border);
    HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, hBr);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
    SelectObject(hdc, hOldBr);
    SelectObject(hdc, hOldPen);
    DeleteObject(hBr);
    DeleteObject(hPen);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, fg);
    RECT rcTxt = { rc.left + px, rc.top + py, rc.right - px, rc.bottom - py };
    DrawTextW(hdc, text, -1, &rcTxt, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, hOld);
}

static void DrawCard(HDC hdc, int x, int y, int w, int h, COLORREF bg, COLORREF border, int rad = 8) {
    HBRUSH hBr = CreateSolidBrush(bg);
    HPEN hPen = CreatePen(PS_SOLID, 1, border);
    HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, hBr);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    RoundRect(hdc, x, y, x + w, y + h, rad, rad);
    SelectObject(hdc, hOldBr);
    SelectObject(hdc, hOldPen);
    DeleteObject(hBr);
    DeleteObject(hPen);
}

static int DrawTextWrap(HDC hdc, int x, int y, int w, const wchar_t* text, HFONT hFont, COLORREF color, UINT flags = DT_LEFT | DT_WORDBREAK) {
    HFONT hOld = (HFONT)SelectObject(hdc, hFont);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    RECT rc = { x, y, x + w, y + 2000 };
    int h = DrawTextW(hdc, text, -1, &rc, flags);
    SelectObject(hdc, hOld);
    return h;
}

static void DrawDivider(HDC hdc, int x, int y, int w) {
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(226, 232, 240));
    HPEN hOld = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, x, y, NULL);
    LineTo(hdc, x + w, y);
    SelectObject(hdc, hOld);
    DeleteObject(hPen);
}

LRESULT CALLBACK WhyLikhiWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            RECT rc;
            GetClientRect(hWnd, &rc);
            SCROLLINFO si;
            memset(&si, 0, sizeof(si));
            si.cbSize = sizeof(si);
            si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE;
            si.nMin = 0;
            si.nMax = TOTAL_HEIGHT;
            si.nPage = rc.bottom;
            si.nPos = s_scrollY;
            SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
            return 0;
        }

        case WM_SIZE: {
            int clientH = HIWORD(lParam);
            int maxScroll = std::max<int>(0, TOTAL_HEIGHT - clientH);
            if (s_scrollY > maxScroll) s_scrollY = maxScroll;
            SCROLLINFO si;
            memset(&si, 0, sizeof(si));
            si.cbSize = sizeof(si);
            si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE;
            si.nMin = 0;
            si.nMax = TOTAL_HEIGHT;
            si.nPage = clientH;
            si.nPos = s_scrollY;
            SetScrollInfo(hWnd, SB_VERT, &si, TRUE);
            InvalidateRect(hWnd, NULL, FALSE);
            return 0;
        }

        case WM_VSCROLL: {
            RECT rc;
            GetClientRect(hWnd, &rc);
            int clientH = (int)rc.bottom;
            int maxScroll = std::max<int>(0, TOTAL_HEIGHT - clientH);
            int oldY = s_scrollY;
            switch (LOWORD(wParam)) {
                case SB_TOP: s_scrollY = 0; break;
                case SB_BOTTOM: s_scrollY = maxScroll; break;
                case SB_LINEUP: s_scrollY = std::max<int>(0, s_scrollY - 35); break;
                case SB_LINEDOWN: s_scrollY = std::min<int>(maxScroll, s_scrollY + 35); break;
                case SB_PAGEUP: s_scrollY = std::max<int>(0, s_scrollY - clientH); break;
                case SB_PAGEDOWN: s_scrollY = std::min<int>(maxScroll, s_scrollY + clientH); break;
                case SB_THUMBTRACK:
                case SB_THUMBPOSITION:
                    s_scrollY = HIWORD(wParam);
                    break;
            }
            if (s_scrollY < 0) s_scrollY = 0;
            if (s_scrollY > maxScroll) s_scrollY = maxScroll;
            if (s_scrollY != oldY) {
                SetScrollPos(hWnd, SB_VERT, s_scrollY, TRUE);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);
            RECT rc;
            GetClientRect(hWnd, &rc);
            int maxScroll = std::max<int>(0, TOTAL_HEIGHT - (int)rc.bottom);
            int oldY = s_scrollY;
            s_scrollY = std::min<int>(maxScroll, std::max<int>(0, s_scrollY - (zDelta / WHEEL_DELTA) * 65));
            if (s_scrollY != oldY) {
                SetScrollPos(hWnd, SB_VERT, s_scrollY, TRUE);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_MOUSEMOVE: {
            int mx = GET_X_LPARAM(lParam);
            int my = GET_Y_LPARAM(lParam) + s_scrollY;
            int hovered = -1;
            for (int i = 0; i < 4; i++) {
                const auto& btn = s_ctaButtons[i];
                if (mx >= btn.x && mx <= btn.x + btn.w && my >= btn.y && my <= btn.y + btn.h) {
                    hovered = i;
                    break;
                }
            }
            if (hovered != s_hoverBtn) {
                s_hoverBtn = hovered;
                InvalidateRect(hWnd, NULL, FALSE);
                TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, 0 };
                TrackMouseEvent(&tme);
            }
            if (hovered >= 0) {
                SetCursor(LoadCursor(NULL, IDC_HAND));
            } else {
                SetCursor(LoadCursor(NULL, IDC_ARROW));
            }
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (s_hoverBtn != -1) {
                s_hoverBtn = -1;
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            if (s_hoverBtn >= 0) {
                s_pressedBtn = s_hoverBtn;
                SetCapture(hWnd);
                InvalidateRect(hWnd, NULL, FALSE);
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            if (GetCapture() == hWnd) {
                ReleaseCapture();
                int clicked = (s_pressedBtn == s_hoverBtn) ? s_pressedBtn : -1;
                s_pressedBtn = -1;
                InvalidateRect(hWnd, NULL, FALSE);
                if (clicked >= 0) {
                    HWND hParent = GetParent(hWnd);
                    if (clicked == 0) {
                        PostMessageW(hParent, WM_COMMAND, MAKEWPARAM(IDC_NAV_BASE + SEC_TYPING, 0), 0);
                    } else if (clicked == 1) {
                        PostMessageW(hParent, WM_COMMAND, MAKEWPARAM(IDC_NAV_BASE + SEC_DICTIONARY, 0), 0);
                    } else if (clicked == 2) {
                        PostMessageW(hParent, WM_COMMAND, MAKEWPARAM(IDC_NAV_BASE + SEC_SUGGESTIONS, 0), 0);
                    } else if (clicked == 3) {
                        PostMessageW(hParent, WM_COMMAND, MAKEWPARAM(IDC_NAV_BASE + SEC_ABOUT, 0), 0);
                        PostMessageW(hParent, WM_COMMAND, MAKEWPARAM(IDC_BTN_CHECK_UPDATE, 0), 0);
                    }
                }
            }
            return 0;
        }

        case WM_ERASEBKGND:
            return 1; // Double-buffered in WM_PAINT

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            RECT rcClient;
            GetClientRect(hWnd, &rcClient);
            int width = rcClient.right;
            int height = rcClient.bottom;

            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
            HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

            // 1. Fill clean background
            HBRUSH hBrWhite = CreateSolidBrush(RGB(255, 255, 255));
            FillRect(memDC, &rcClient, hBrWhite);
            DeleteObject(hBrWhite);

            // Setup fonts
            HFONT hHero = CreateFontW(-22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Nirmala UI");
            HFONT hHead = CreateFontW(-17, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Nirmala UI");
            HFONT hCardT = CreateFontW(-15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Nirmala UI");
            HFONT hBold = CreateFontW(-14, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Nirmala UI");
            HFONT hBody = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Nirmala UI");
            HFONT hTag = CreateFontW(-12, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            HFONT hSub = CreateFontW(-12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            HFONT hMono = CreateFontW(-13, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");

            int offY = -s_scrollY;
            int cw = 514;
            int cx = 18;

            // ==============================================================
            // 1. HERO SECTION
            // ==============================================================
            DrawBadge(memDC, cx, offY + 16, L"🌟  পণ্য পরিচিতি • Product Philosophy", RGB(239, 246, 255), RGB(29, 78, 216), RGB(191, 219, 254), hTag);
            DrawTextWrap(memDC, cx, offY + 48, cw, L"কেন লিখি? (Why Likhi)", hHero, RGB(30, 58, 138), DT_LEFT | DT_SINGLELINE);
            DrawTextWrap(memDC, cx, offY + 84, cw, L"কেন Likhi তৈরি করা হয়েছে এবং এটি কীভাবে বাংলা টাইপিংকে সহজ করে", hBold, RGB(71, 85, 105), DT_LEFT | DT_SINGLELINE);

            // Quote Box
            DrawCard(memDC, cx, offY + 114, cw, 80, RGB(239, 246, 255), RGB(191, 219, 254), 8);
            DrawTextWrap(memDC, cx + 14, offY + 124, cw - 28, L"“বাংলা টাইপিংকে ব্যবহারকারীর জন্য সহজ করতে লিখি।”", hHead, RGB(29, 78, 216), DT_LEFT | DT_SINGLELINE);
            DrawTextWrap(memDC, cx + 14, offY + 154, cw - 28, L"“প্রযুক্তির সঙ্গে মানিয়ে নেওয়ার দায়িত্ব ব্যবহারকারীর নয়; প্রযুক্তিকেই ব্যবহারকারীর সঙ্গে মানিয়ে নিতে হবে।”", hBold, RGB(30, 64, 175), DT_LEFT | DT_SINGLELINE);

            // Intro text
            DrawTextWrap(memDC, cx, offY + 206, cw,
                L"Likhi তৈরি হয়েছে এমন একটি বাংলা typing experience দেওয়ার জন্য, যেখানে ব্যবহারকারীকে প্রতিটি শব্দের spelling নিয়ে অতিরিক্ত চিন্তা করতে না হয়। আপনি স্বাভাবিকভাবে Banglish-এ লিখবেন, Likhi আপনার input থেকে বাংলা শব্দ তৈরি ও সাজেস্ট করার চেষ্টা করবে।",
                hBody, RGB(51, 65, 85));

            DrawDivider(memDC, cx, offY + 276, cw);

            // ==============================================================
            // 2. WHY LIKHI WAS CREATED
            // ==============================================================
            DrawTextWrap(memDC, cx, offY + 292, cw, L"কেন লিখি তৈরি করা হয়েছে?", hHead, RGB(15, 23, 42), DT_LEFT | DT_SINGLELINE);
            DrawTextWrap(memDC, cx, offY + 322, cw,
                L"বাংলায় লেখার সময় অনেক Windows ব্যবহারকারীকে keyboard layout, spelling pattern, যুক্তাক্ষর, phonetic conversion এবং typing correction নিয়ে অতিরিক্ত চিন্তা করতে হয়। অনেকে Banglish-এ লিখতে স্বাচ্ছন্দ্যবোধ করেন, কিন্তু একই বাংলা শব্দের জন্য একাধিক spelling ব্যবহার করেন।",
                hBody, RGB(51, 65, 85));

            // Variations card
            DrawCard(memDC, cx, offY + 392, cw, 46, RGB(248, 250, 252), RGB(226, 232, 240), 8);
            DrawTextWrap(memDC, cx + 14, offY + 404, cw - 28,
                L"উদাহরণ:  somossa  /  shomossa  /  somossha  ➔  সমস্যা  (স্বাভাবিক বৈচিত্র্য)",
                hBold, RGB(30, 58, 138), DT_LEFT | DT_SINGLELINE);

            // Loanwords card
            DrawCard(memDC, cx, offY + 448, cw, 46, RGB(248, 250, 252), RGB(226, 232, 240), 8);
            DrawTextWrap(memDC, cx + 14, offY + 460, cw - 28,
                L"দৈনন্দিন শব্দ:  office ➔ অফিস  •  computer ➔ কম্পিউটার  •  mouse ➔ মাউস  •  battery ➔ ব্যাটারি",
                hBold, RGB(15, 23, 42), DT_LEFT | DT_SINGLELINE);

            DrawTextWrap(memDC, cx, offY + 504, cw,
                L"💡 দ্রষ্টব্য: Likhi অবাস্তব AI দাবি করে না; বরং ফোনেটিক অ্যালগরিদম ও ইন্টেলিজেন্ট র‍্যাঙ্কিংয়ের মাধ্যমে সম্ভাব্য সেরা পরামর্শ দেওয়ার চেষ্টা করে।",
                hSub, RGB(100, 116, 139));

            DrawDivider(memDC, cx, offY + 538, cw);

            // ==============================================================
            // 3. PROBLEMS LIKHI SOLVES (4 Cards)
            // ==============================================================
            DrawTextWrap(memDC, cx, offY + 554, cw, L"বাংলা টাইপ করার সময় সাধারণ সমস্যাগুলো (সমস্যা ও সমাধান)", hHead, RGB(15, 23, 42), DT_LEFT | DT_SINGLELINE);

            // Card 1
            DrawCard(memDC, cx, offY + 586, cw, 78, RGB(248, 250, 252), RGB(226, 232, 240), 8);
            DrawTextWrap(memDC, cx + 14, offY + 596, cw - 28, L"১. Spelling নিয়ে দ্বিধা (Spelling Variation Dilemma)", hCardT, RGB(29, 78, 216), DT_LEFT | DT_SINGLELINE);
            DrawTextWrap(memDC, cx + 14, offY + 622, cw - 28,
                L"একই শব্দের একাধিক স্বাভাবিক উচ্চারণ (somossa / shomossa / somossha)। Likhi কোনো একটি বানানে সীমাবদ্ধ না রেখে স্বাভাবিক বৈচিত্র্য বুঝে সঠিক শব্দের দিকে নিয়ে যায়।",
                hBody, RGB(51, 65, 85));

            // Card 2
            DrawCard(memDC, cx, offY + 674, cw, 78, RGB(248, 250, 252), RGB(226, 232, 240), 8);
            DrawTextWrap(memDC, cx + 14, offY + 684, cw - 28, L"২. যুক্তাক্ষর ও জটিল শব্দ (Complex Conjuncts & Ligatures)", hCardT, RGB(29, 78, 216), DT_LEFT | DT_SINGLELINE);
            DrawTextWrap(memDC, cx + 14, offY + 710, cw - 28,
                L"যুক্তাক্ষরের জটিল নিয়ম মুখস্থ ছাড়াই লিখুন: brohmoputro → ব্রহ্মপুত্র, antorjatik → আন্তর্জাতিক, shasthyo → স্বাস্থ্য, akangkha → আকাঙ্ক্ষা।",
                hBody, RGB(51, 65, 85));

            // Card 3
            DrawCard(memDC, cx, offY + 762, cw, 78, RGB(248, 250, 252), RGB(226, 232, 240), 8);
            DrawTextWrap(memDC, cx + 14, offY + 772, cw - 28, L"৩. বাংলা ও English মিশিয়ে লেখা (Mixed Language Flow)", hCardT, RGB(29, 78, 216), DT_LEFT | DT_SINGLELINE);
            DrawTextWrap(memDC, cx + 14, offY + 798, cw - 28,
                L"বাক্যের মধ্যে বাংলা ও ইংরেজি নির্বিঘ্নে লিখুন: “ami ajke office e jabo” ➔ “আমি আজকে অফিসে যাব”। ইংরেজি শব্দ জোর করে অবাস্তব বাংলায় পরিবর্তিত হয় না।",
                hBody, RGB(51, 65, 85));

            // Card 4
            DrawCard(memDC, cx, offY + 850, cw, 78, RGB(248, 250, 252), RGB(226, 232, 240), 8);
            DrawTextWrap(memDC, cx + 14, offY + 860, cw - 28, L"৪. একই শব্দের একাধিক সম্ভাবনা (Intelligent Disambiguation)", hCardT, RGB(29, 78, 216), DT_LEFT | DT_SINGLELINE);
            DrawTextWrap(memDC, cx + 14, offY + 886, cw - 28,
                L"একই ইনপুটের বহু অর্থ থাকতে পারে (t → ত / ট, ta → তা / টা, taka → টাকা / তাকা)। Likhi অন্ধভাবে একটি চাপিয়ে না দিয়ে প্রাসঙ্গিক সাজেশনের সুযোগ দেয়।",
                hBody, RGB(51, 65, 85));

            DrawDivider(memDC, cx, offY + 940, cw);

            // ==============================================================
            // 4. HOW LIKHI WORKS (6 Steps)
            // ==============================================================
            DrawTextWrap(memDC, cx, offY + 956, cw, L"Likhi কীভাবে কাজ করে? (How Likhi Works)", hHead, RGB(15, 23, 42), DT_LEFT | DT_SINGLELINE);

            const wchar_t* steps[6][2] = {
                { L"ধাপ ১: স্বাভাবিক টাইপিং", L"ব্যবহারকারী নিজস্ব সাবলীল ধরনে Banglish-এ টাইপ করেন।" },
                { L"ধাপ ২: ফোনেটিক বিশ্লেষণ", L"Likhi ইনপুট প্যাটার্ন ও বানানের স্বাভাবিক বৈচিত্র্য বিশ্লেষণ করে।" },
                { L"ধাপ ৩: ক্যান্ডিডেট তৈরি", L"৮০,০০০+ শব্দকোষের ভিত্তিতে সম্ভাব্য সঠিক বাংলা রূপান্তর (Candidates) তৈরি করে।" },
                { L"ধাপ ৪: ইন্টেলিজেন্ট র‍্যাঙ্কিং", L"শব্দের ফ্রিকোয়েন্সি, উচ্চারণ সাদৃশ্য, প্রসঙ্গ এবং ব্যক্তিগত পছন্দের ভিত্তিতে র‍্যাঙ্কিং করা হয়।" },
                { L"ধাপ ৫: সাজেশন প্রদর্শন", L"Likhi সাজেশন বারে শীর্ষ সম্ভাব্য ফলাফলগুলো সহজে দৃশ্যমান করে।" },
                { L"ধাপ ৬: স্বাধীন নির্বাচন", L"ব্যবহারকারী পছন্দের শব্দটি বেছে নেন অথবা Space চেপে শীর্ষ পরামর্শ গ্রহণ করেন।" }
            };

            for (int i = 0; i < 6; i++) {
                int sy = offY + 990 + (i * 38);
                wchar_t snum[4];
                wsprintfW(snum, L"%d", i + 1);
                DrawBadge(memDC, cx, sy, snum, RGB(239, 246, 255), RGB(29, 78, 216), RGB(191, 219, 254), hTag);
                DrawTextWrap(memDC, cx + 32, sy + 1, 140, steps[i][0], hBold, RGB(30, 58, 138), DT_LEFT | DT_SINGLELINE);
                DrawTextWrap(memDC, cx + 180, sy + 1, cw - 180, steps[i][1], hBody, RGB(71, 85, 105), DT_LEFT | DT_SINGLELINE);
            }

            // Visual Workflow Box
            DrawCard(memDC, cx, offY + 1230, cw, 68, RGB(241, 245, 249), RGB(203, 213, 225), 8);
            DrawTextWrap(memDC, cx + 14, offY + 1240, cw - 28,
                L"আপনি লিখবেন  ➔  Banglish Input  ➔  Likhi বিশ্লেষণ  ➔  সম্ভাব্য Candidates",
                hBold, RGB(30, 58, 138), DT_LEFT | DT_SINGLELINE);
            DrawTextWrap(memDC, cx + 14, offY + 1264, cw - 28,
                L"                          ➔  Intelligent Ranking  ➔  আপনি নির্বাচন করবেন  ➔  বাংলা লেখা",
                hBold, RGB(22, 101, 52), DT_LEFT | DT_SINGLELINE);

            DrawDivider(memDC, cx, offY + 1312, cw);

            // ==============================================================
            // 5. NATURAL BANGLISH TABLE
            // ==============================================================
            DrawTextWrap(memDC, cx, offY + 1328, cw, L"আপনি যেমন লিখতে চান, তেমনই লিখুন (Natural Banglish)", hHead, RGB(15, 23, 42), DT_LEFT | DT_SINGLELINE);
            DrawTextWrap(memDC, cx, offY + 1356, cw, L"প্রতিনিধিত্বমূলক কিছু বাস্তবসম্মত উদাহরণ:", hBody, RGB(100, 116, 139), DT_LEFT | DT_SINGLELINE);

            DrawCard(memDC, cx, offY + 1380, cw, 220, RGB(255, 255, 255), RGB(226, 232, 240), 8);

            const wchar_t* col1[7][2] = {
                { L"ami", L"আমি" }, { L"tumi", L"তুমি" }, { L"bhalo", L"ভালো" },
                { L"valo", L"ভালো" }, { L"somossa", L"সমস্যা" }, { L"porishkar", L"পরিষ্কার" },
                { L"office", L"অফিস" }
            };
            const wchar_t* col2[7][2] = {
                { L"computer", L"কম্পিউটার" }, { L"mouse", L"মাউস" }, { L"control", L"কন্ট্রোল" },
                { L"battery", L"ব্যাটারি" }, { L"screenshot", L"স্ক্রিনশট" }, { L"output", L"আউটপুট" },
                { L"better", L"বেটার" }
            };

            for (int r = 0; r < 7; r++) {
                int ry = offY + 1388 + (r * 29);
                COLORREF rowBg = (r % 2 == 1) ? RGB(248, 250, 252) : RGB(255, 255, 255);
                HBRUSH hRBr = CreateSolidBrush(rowBg);
                RECT rcR = { cx + 4, ry, cx + cw - 4, ry + 27 };
                FillRect(memDC, &rcR, hRBr);
                DeleteObject(hRBr);

                // Col 1
                DrawTextWrap(memDC, cx + 18, ry + 3, 110, col1[r][0], hMono, RGB(30, 58, 138), DT_LEFT | DT_SINGLELINE);
                DrawTextWrap(memDC, cx + 135, ry + 2, 25, L"➔", hBody, RGB(148, 163, 184), DT_LEFT | DT_SINGLELINE);
                DrawTextWrap(memDC, cx + 165, ry + 2, 85, col1[r][1], hBold, RGB(15, 23, 42), DT_LEFT | DT_SINGLELINE);

                // Col 2
                DrawTextWrap(memDC, cx + 275, ry + 3, 110, col2[r][0], hMono, RGB(30, 58, 138), DT_LEFT | DT_SINGLELINE);
                DrawTextWrap(memDC, cx + 395, ry + 2, 25, L"➔", hBody, RGB(148, 163, 184), DT_LEFT | DT_SINGLELINE);
                DrawTextWrap(memDC, cx + 425, ry + 2, 85, col2[r][1], hBold, RGB(15, 23, 42), DT_LEFT | DT_SINGLELINE);
            }

            // Sentence Example Card
            DrawCard(memDC, cx, offY + 1612, cw, 42, RGB(240, 253, 244), RGB(187, 247, 208), 8);
            DrawTextWrap(memDC, cx + 14, offY + 1622, cw - 28,
                L"বাক্য প্রবাহ:  ami ajke office e jabo   ➔   আমি আজকে অফিসে যাব",
                hBold, RGB(22, 101, 52), DT_LEFT | DT_SINGLELINE);

            DrawDivider(memDC, cx, offY + 1668, cw);

            // ==============================================================
            // 6. INTELLIGENT SUGGESTIONS
            // ==============================================================
            DrawTextWrap(memDC, cx, offY + 1684, cw, L"শুধু টাইপ নয় — সম্ভাব্য শব্দও দেখুন (Intelligent Suggestions)", hHead, RGB(15, 23, 42), DT_LEFT | DT_SINGLELINE);
            DrawTextWrap(memDC, cx, offY + 1712, cw,
                L"Likhi সাজেশন বার ব্যবহার করে একাধিক সম্ভাব্য প্রার্থী উপস্থাপন করে, যাতে আপনার অভিপ্রায় সঠিকভাবে প্রকাশ পায়:",
                hBody, RGB(51, 65, 85));

            DrawCard(memDC, cx, offY + 1746, cw, 68, RGB(248, 250, 252), RGB(226, 232, 240), 8);
            DrawTextWrap(memDC, cx + 14, offY + 1756, cw - 28,
                L"Input: taka   ➔   সাজেশন:  টাকা  |  তাকা          Input: ta   ➔   সাজেশন:  তা  |  টা",
                hBold, RGB(30, 58, 138), DT_LEFT | DT_SINGLELINE);
            DrawTextWrap(memDC, cx + 14, offY + 1782, cw - 28,
                L"Input: office ➔   সাজেশন:  অফিস  |  office  |  অফিসে",
                hBold, RGB(30, 58, 138), DT_LEFT | DT_SINGLELINE);

            DrawTextWrap(memDC, cx, offY + 1826, cw,
                L"🎯 “চূড়ান্ত সিদ্ধান্ত ব্যবহারকারীর হাতে।” — Likhi কখনোই জোর করে অযাচিত শব্দ প্রতিস্থাপন করে না।",
                hBold, RGB(22, 101, 52), DT_LEFT | DT_SINGLELINE);

            DrawDivider(memDC, cx, offY + 1860, cw);

            // ==============================================================
            // 7. PERSONAL DICTIONARY & TEACH MODE
            // ==============================================================
            DrawTextWrap(memDC, cx, offY + 1876, cw, L"আপনার নিজের শব্দ, আপনার নিজের Dictionary & Teach Mode", hHead, RGB(15, 23, 42), DT_LEFT | DT_SINGLELINE);
            DrawTextWrap(memDC, cx, offY + 1904, cw,
                L"Personal Dictionary ব্যবহারকারীদের নিজস্ব গুরুত্বপূর্ণ শব্দ যোগ করার পূর্ণ স্বাধীনতা দেয় (ব্যক্তিগত নাম, প্রতিষ্ঠানের নাম, লোকেশন, পণ্যের নাম, কাস্টম বাংলা শব্দ)। এটি লোকাল, নিরাপদ এবং সম্পূর্ণ ব্যবহারকারীর নিয়ন্ত্রণে থাকে।",
                hBody, RGB(51, 65, 85));

            // Teach Mode Card
            DrawCard(memDC, cx, offY + 1970, cw, 66, RGB(250, 245, 255), RGB(233, 213, 255), 8);
            DrawTextWrap(memDC, cx + 14, offY + 1978, cw - 28, L"Teach Mode — Likhi-কে নিজের মতো শেখান", hCardT, RGB(107, 33, 168), DT_LEFT | DT_SINGLELINE);
            DrawTextWrap(memDC, cx + 14, offY + 2004, cw - 28,
                L"ব্যবহারকারী কোনো নির্দিষ্ট শব্দের জন্য নিজস্ব ম্যাপিং সংজ্ঞায়িত করতে পারেন (যেমন: khuddh → শুদ্ধ)। একবার সেভ করলে এটি স্থায়ীভাবে কার্যকর থাকে।",
                hBody, RGB(88, 28, 135));

            DrawDivider(memDC, cx, offY + 2050, cw);

            // ==============================================================
            // 8. PERSONAL LEARNING (PRIVACY-FIRST)
            // ==============================================================
            DrawTextWrap(memDC, cx, offY + 2066, cw, L"Likhi আপনার typing preference শিখতে পারে — আপনার অনুমতিতে", hHead, RGB(15, 23, 42), DT_LEFT | DT_SINGLELINE);

            DrawCard(memDC, cx, offY + 2096, cw, 96, RGB(248, 250, 252), RGB(203, 213, 225), 8);
            DrawTextWrap(memDC, cx + 14, offY + 2106, cw - 28,
                L"🔒 “Personal Learning একটি user-controlled feature। এটি defaultভাবে গোপনে আপনার typing data সংগ্রহ করার জন্য তৈরি নয়।”",
                hBold, RGB(30, 58, 138));
            DrawTextWrap(memDC, cx + 14, offY + 2140, cw - 28,
                L"ব্যবহারকারী অনুমতি দিলে Likhi নির্বাচিত typing patterns ও vocabulary থেকে ranking উন্নত করতে পারে। সমস্ত লার্নিং ডেটা শুধুমাত্র আপনার নিজস্ব কম্পিউটারে সংরক্ষিত থাকে এবং যেকোনো সময় এক ক্লিকে সম্পূর্ণ রিসেট করা যায়।",
                hBody, RGB(51, 65, 85));

            DrawDivider(memDC, cx, offY + 2206, cw);

            // ==============================================================
            // 9. BANGLA + ENGLISH MIXED TYPING
            // ==============================================================
            DrawTextWrap(memDC, cx, offY + 2222, cw, L"বাংলা ও English — একই লেখায় (Mixed Language)", hHead, RGB(15, 23, 42), DT_LEFT | DT_SINGLELINE);
            DrawTextWrap(memDC, cx, offY + 2250, cw,
                L"বাস্তব জীবনে আমরা বাংলা ও ইংরেজি শব্দ মিলিয়ে লিখি। যেমন: “আজকে office থেকে computer নিয়ে বাসায় যাব।” Likhi মিশ্র লেখার প্রবাহ বজায় রাখে; প্রতিটি ইংরেজি শব্দকে জোরপূর্বক বাংলায় রূপান্তরের চেষ্টা করে লেখার গতি ব্যাহত করে না।",
                hBody, RGB(51, 65, 85));

            DrawDivider(memDC, cx, offY + 2314, cw);

            // ==============================================================
            // 10. OFFLINE-FIRST & PRIVATE
            // ==============================================================
            DrawTextWrap(memDC, cx, offY + 2330, cw, L"Offline-First & Private (গোপনীয়তা ও স্বাধীনতা)", hHead, RGB(15, 23, 42), DT_LEFT | DT_SINGLELINE);

            DrawCard(memDC, cx, offY + 2360, cw, 80, RGB(240, 253, 244), RGB(187, 247, 208), 8);
            DrawTextWrap(memDC, cx + 14, offY + 2370, cw - 28, L"🛡️ “Core Typing-এর জন্য কোনো Cloud Required নয়।”", hHead, RGB(22, 101, 52), DT_LEFT | DT_SINGLELINE);
            DrawTextWrap(memDC, cx + 14, offY + 2400, cw - 28,
                L"কোর টাইপিং সম্পূর্ণ অফলাইনে কাজ করে। টাইপ করা টেক্সট কোনো ক্লাউডে যায় না। ০% কি-লগিং বা ব্যাকগ্রাউন্ড নজরদারি। আপডেট পরীক্ষা সম্পূর্ণ পৃথক ও ঐচ্ছিক।",
                hBody, RGB(20, 83, 45));

            DrawDivider(memDC, cx, offY + 2454, cw);

            // ==============================================================
            // 11. CONTROL IN YOUR HANDS
            // ==============================================================
            DrawTextWrap(memDC, cx, offY + 2470, cw, L"Control আপনার হাতে (User Control)", hHead, RGB(15, 23, 42), DT_LEFT | DT_SINGLELINE);
            DrawTextWrap(memDC, cx, offY + 2498, cw,
                L"Auto Correct, Suggestion Bar, Personal Dictionary, Personal Learning, Teach Mode, কিংবা Update Checking—সবকিছুর চাবিকাঠি আপনার হাতে। “Likhi ব্যবহারকারীকে control করার জন্য নয়; typing experience-কে ব্যবহারকারীর জন্য সহজ করার জন্য।”",
                hBody, RGB(51, 65, 85));

            DrawDivider(memDC, cx, offY + 2568, cw);

            // ==============================================================
            // 12. WHO IS LIKHI FOR?
            // ==============================================================
            DrawTextWrap(memDC, cx, offY + 2584, cw, L"কার জন্য Likhi? (Who is Likhi For?)", hHead, RGB(15, 23, 42), DT_LEFT | DT_SINGLELINE);

            const wchar_t* personas[10] = {
                L"🎓 শিক্ষার্থী (Students)", L"💼 অফিস পেশাজীবী", L"🎨 কনটেন্ট ক্রিয়েটর",
                L"✍️ ব্লগার ও লেখক", L"📱 সোশ্যাল মিডিয়া ব্যবহারকারী", L"💻 প্রোগ্রামার ও ডেভেলপার",
                L"📚 শিক্ষক ও গবেষক", L"🏢 ব্যবসায়ী", L"📰 অনুবাদক ও সাংবাদিক",
                L"⚡ যাঁরা সহজ বাংলিশ চান"
            };

            for (int i = 0; i < 5; i++) {
                int px = cx + (i * 103);
                DrawBadge(memDC, px, offY + 2614, personas[i], RGB(241, 245, 249), RGB(51, 65, 85), RGB(203, 213, 225), hTag);
            }
            for (int i = 5; i < 10; i++) {
                int px = cx + ((i - 5) * 103);
                DrawBadge(memDC, px, offY + 2646, personas[i], RGB(241, 245, 249), RGB(51, 65, 85), RGB(203, 213, 225), hTag);
            }

            DrawDivider(memDC, cx, offY + 2686, cw);

            // ==============================================================
            // 13. LIKHI VS WORKFLOW & VISION
            // ==============================================================
            DrawTextWrap(memDC, cx, offY + 2702, cw, L"আমাদের স্বপ্ন ও দর্শন (The Likhi Vision)", hHead, RGB(15, 23, 42), DT_LEFT | DT_SINGLELINE);
            DrawTextWrap(memDC, cx, offY + 2730, cw,
                L"“বাংলা লেখা হোক সবার জন্য সহজ।”\nআমরা এমন একটি typing experience তৈরি করতে চাই যেখানে ব্যবহারকারীকে software-এর নিয়ম মুখস্থ করার পরিবর্তে software ব্যবহারকারীর স্বাভাবিক লেখার ধরন বুঝতে চেষ্টা করবে।",
                hBold, RGB(30, 58, 138));
            DrawTextWrap(memDC, cx, offY + 2790, cw,
                L"Likhi প্রচলিত কিবোর্ড বা টুলের বিরোধী নয়। “Likhi-এর লক্ষ্য হলো phonetic/Banglish-based Windows typing experience-কে আরও natural, suggestion-driven এবং user-controlled করা।”",
                hBody, RGB(71, 85, 105));

            DrawDivider(memDC, cx, offY + 2846, cw);

            // ==============================================================
            // 14. LIKHI BRAIN (ROADMAP)
            // ==============================================================
            DrawTextWrap(memDC, cx, offY + 2862, cw, L"Likhi Brain", hHead, RGB(15, 23, 42), DT_LEFT | DT_SINGLELINE);
            DrawBadge(memDC, cx + 115, offY + 2862, L"Roadmap / পরবর্তী পরিকল্পনা", RGB(254, 243, 199), RGB(146, 64, 14), RGB(253, 230, 138), hTag);

            DrawTextWrap(memDC, cx, offY + 2894, cw,
                L"“Likhi Brain হলো Likhi-এর intelligent language-processing direction, যার লক্ষ্য context, phonetic similarity, fuzzy spelling এবং user-controlled learning ব্যবহার করে suggestion quality উন্নত করা।” এটি সম্পূর্ণ ক্লাউডমুক্ত বা স্থানীয়ভাবে কার্যকর করার রূপরেখায় প্রণীত।",
                hBody, RGB(51, 65, 85));

            DrawDivider(memDC, cx, offY + 2962, cw);

            // ==============================================================
            // 15. TRUST & TRANSPARENCY (WHAT LIKHI DOES NOT DO)
            // ==============================================================
            DrawTextWrap(memDC, cx, offY + 2978, cw, L"Likhi কী করে না (স্বচ্ছতা ও প্রতিশ্রুতি)", hHead, RGB(15, 23, 42), DT_LEFT | DT_SINGLELINE);

            DrawCard(memDC, cx, offY + 3008, cw, 96, RGB(254, 242, 242), RGB(254, 202, 202), 8);
            DrawTextWrap(memDC, cx + 14, offY + 3016, cw - 28,
                L"• আপনার typed text-এর ওপর unnecessary cloud dependency নেই।\n• Core typing-এর জন্য internet বাধ্যতামূলক নয়।\n• Personal Learning permission ছাড়া silently enabled হয় না।\n• Update না চাইলে টাইপিং কখনোই বন্ধ হয় না।",
                hBold, RGB(153, 27, 27));

            DrawDivider(memDC, cx, offY + 3120, cw);

            // ==============================================================
            // 16. UPDATE CONNECTION
            // ==============================================================
            DrawTextWrap(memDC, cx, offY + 3136, cw, L"Likhi সবসময় উন্নত হচ্ছে (Safe Updates)", hHead, RGB(15, 23, 42), DT_LEFT | DT_SINGLELINE);
            DrawTextWrap(memDC, cx, offY + 3164, cw,
                L"Likhi-তে রয়েছে অফিসিয়াল ও নিরাপদ আপডেট ব্যবস্থা। ব্যবহারকারী চাইলে নতুন ভার্সন পরীক্ষা করতে পারেন এবং ক্রিপ্টোগ্রাফিক SHA-256 যাচাইকরণ সহ এক ক্লিকে আপডেট নিতে পারেন। আপডেটের সময় ব্যবহারকারীর কোনো সেটিংস বা ব্যক্তিগত অভিধানের ক্ষতি হয় না।",
                hBody, RGB(51, 65, 85));

            DrawDivider(memDC, cx, offY + 3240, cw);

            // ==============================================================
            // 17. CALL TO ACTION (INTERACTIVE BUTTONS)
            // ==============================================================
            DrawTextWrap(memDC, cx, offY + 3256, cw, L"বাংলা টাইপ করুন নিজের মতো করে।", hHero, RGB(30, 58, 138), DT_LEFT | DT_SINGLELINE);
            DrawTextWrap(memDC, cx, offY + 3290, cw,
                L"Banglish-এ লিখুন। Suggestion দেখুন। নিজের শব্দ যোগ করুন। নিজের typing preference নিজের নিয়ন্ত্রণে রাখুন।",
                hBody, RGB(71, 85, 105), DT_LEFT | DT_SINGLELINE);

            // Draw CTA Buttons
            for (int i = 0; i < 4; i++) {
                const auto& btn = s_ctaButtons[i];
                int by = btn.y + offY;
                bool isHover = (s_hoverBtn == i);
                bool isPressed = (s_pressedBtn == i && isHover);

                COLORREF bg, border, tc;
                if (btn.isPrimary) {
                    if (isPressed) bg = RGB(29, 78, 216);
                    else if (isHover) bg = RGB(37, 99, 235);
                    else bg = RGB(30, 58, 138);
                    border = bg;
                    tc = RGB(255, 255, 255);
                } else {
                    if (isPressed) bg = RGB(226, 232, 240);
                    else if (isHover) bg = RGB(241, 245, 249);
                    else bg = RGB(255, 255, 255);
                    border = isHover ? RGB(37, 99, 235) : RGB(203, 213, 225);
                    tc = isHover ? RGB(37, 99, 235) : RGB(30, 41, 59);
                }

                HBRUSH hBBr = CreateSolidBrush(bg);
                HPEN hBPen = CreatePen(PS_SOLID, 1, border);
                HBRUSH hOldBBr = (HBRUSH)SelectObject(memDC, hBBr);
                HPEN hOldBPen = (HPEN)SelectObject(memDC, hBPen);
                RoundRect(memDC, btn.x, by, btn.x + btn.w, by + btn.h, 8, 8);
                SelectObject(memDC, hOldBBr);
                SelectObject(memDC, hOldBPen);
                DeleteObject(hBBr);
                DeleteObject(hBPen);

                SetBkMode(memDC, TRANSPARENT);
                SetTextColor(memDC, tc);
                SelectObject(memDC, hBold);
                RECT rcBtn = { btn.x, by, btn.x + btn.w, by + btn.h };
                DrawTextW(memDC, btn.text, -1, &rcBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }

            // Blit to screen
            BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

            // Cleanup
            SelectObject(memDC, oldBmp);
            DeleteObject(memBmp);
            DeleteDC(memDC);

            DeleteObject(hHero);
            DeleteObject(hHead);
            DeleteObject(hCardT);
            DeleteObject(hBold);
            DeleteObject(hBody);
            DeleteObject(hTag);
            DeleteObject(hSub);
            DeleteObject(hMono);

            EndPaint(hWnd, &ps);
            return 0;
        }

        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

static void RegisterWhyLikhiPageClass(HINSTANCE hInstance) {
    WNDCLASSEXW wcex;
    memset(&wcex, 0, sizeof(WNDCLASSEXW));
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WhyLikhiWndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wcex.lpszClassName = L"LikhiWhyLikhiPageClass";
    RegisterClassExW(&wcex);
}

static HWND CreateWhyLikhiPage(HWND hParent, HINSTANCE hInstance, int x, int y, int w, int h) {
    return CreateWindowExW(
        0, L"LikhiWhyLikhiPageClass", L"",
        WS_CHILD | WS_VSCROLL | WS_CLIPCHILDREN,
        x, y, w, h,
        hParent, NULL, hInstance, NULL
    );
}

} // namespace likhi_why
