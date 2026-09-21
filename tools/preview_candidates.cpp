// Visual preview for the candidate strip (development aid).
// Shows the real CandidateWindow with sample data so the layout can be checked
// (and screenshotted) without typing in a host application.
#include "../tsf/include/candidate_window.h"

#include <windows.h>
#include <string>
#include <vector>

using namespace bangla_tsf;

int main() {
    CandidateWindow win;
    if (!win.Initialize(GetModuleHandleW(nullptr))) return 1;

    std::vector<std::wstring> candidates = {
        L"\u0986\u09ae\u09be\u0930",           // আমার
        L"\u0986\u09ae\u09b0",                 // আমর
        L"\u0986\u09ae\u09be\u09b0\u09c7",     // আমারে
        L"\u0986\u09ae\u09be\u09a6\u09c7\u09b0",  // আমাদের
        L"\u0986\u09ae\u09be\u09b0\u0987",     // আমারই
        L"amar",                               // the English candidate
    };

    RECT caret = {420, 400, 620, 420};  // pretend the caret sits here
    win.ShowCandidates(candidates, 0, caret);

    DWORD stop_at = GetTickCount() + 6000;
    MSG msg;
    while (GetTickCount() < stop_at) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        Sleep(20);
    }

    win.Hide();
    win.Destroy();
    return 0;
}
