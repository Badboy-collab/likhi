#include <windows.h>
#include <msctf.h>
#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include "../../tsf/include/text_service.h"
#include "../../tsf/include/tsf_utils.h"
#include "../../engine/include/bangla_engine.h"

namespace bangla_tsf {
    HINSTANCE g_hInstance = NULL;
}

// Mock ITfContext implementation for testing key sink
class MockTfContext : public ITfContext {
    LONG ref_count_ = 1;
public:
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_INVALIDARG;
        if (riid == IID_IUnknown || riid == IID_ITfContext) {
            *ppv = static_cast<ITfContext*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&ref_count_); }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG r = InterlockedDecrement(&ref_count_);
        if (r == 0) delete this;
        return r;
    }
    STDMETHODIMP RequestEditSession(TfClientId tid, ITfEditSession *pes, DWORD dwFlags, HRESULT *phrSession) override {
        (void)tid; (void)pes; (void)dwFlags;
        if (phrSession) *phrSession = S_OK;
        return S_OK;
    }
    STDMETHODIMP InWriteSession(TfClientId tid, BOOL *pfWriteSession) override { (void)tid; if (pfWriteSession) *pfWriteSession = FALSE; return S_OK; }
    STDMETHODIMP GetSelection(TfEditCookie ec, ULONG ulIndex, ULONG ulCount, TF_SELECTION *pSelection, ULONG *pcFetched) override { (void)ec; (void)ulIndex; (void)ulCount; (void)pSelection; (void)pcFetched; return E_NOTIMPL; }
    STDMETHODIMP SetSelection(TfEditCookie ec, ULONG ulCount, const TF_SELECTION *pSelection) override { (void)ec; (void)ulCount; (void)pSelection; return E_NOTIMPL; }
    STDMETHODIMP GetStart(TfEditCookie ec, ITfRange **ppStart) override { (void)ec; (void)ppStart; return E_NOTIMPL; }
    STDMETHODIMP GetEnd(TfEditCookie ec, ITfRange **ppEnd) override { (void)ec; (void)ppEnd; return E_NOTIMPL; }
    STDMETHODIMP GetActiveView(ITfContextView **ppViewOut) override { (void)ppViewOut; return E_NOTIMPL; }
    STDMETHODIMP EnumViews(IEnumTfContextViews **ppEnum) override { (void)ppEnum; return E_NOTIMPL; }
    STDMETHODIMP GetStatus(TF_STATUS *pdcs) override { (void)pdcs; return E_NOTIMPL; }
    STDMETHODIMP GetProperty(REFGUID rguidProp, ITfProperty **ppProp) override { (void)rguidProp; (void)ppProp; return E_NOTIMPL; }
    STDMETHODIMP GetAppProperty(REFGUID rguidProp, ITfReadOnlyProperty **ppProp) override { (void)rguidProp; (void)ppProp; return E_NOTIMPL; }
    STDMETHODIMP TrackProperties(const GUID **prgProp, ULONG cProp, const GUID **prgAppProp, ULONG cAppProp, ITfReadOnlyProperty **ppProperty) override { (void)prgProp; (void)cProp; (void)prgAppProp; (void)cAppProp; (void)ppProperty; return E_NOTIMPL; }
    STDMETHODIMP EnumProperties(IEnumTfProperties **ppEnum) override { (void)ppEnum; return E_NOTIMPL; }
    STDMETHODIMP GetDocumentMgr(ITfDocumentMgr **ppDm) override { (void)ppDm; return E_NOTIMPL; }
    STDMETHODIMP CreateRangeBackup(TfEditCookie ec, ITfRange *pRange, ITfRangeBackup **ppBackup) override { (void)ec; (void)pRange; (void)ppBackup; return E_NOTIMPL; }
};

int main() {
    SetConsoleOutputCP(CP_UTF8);
    std::cout << "=========================================================\n";
    std::cout << "  LIKHI - P0 CRITICAL KEYBOARD EVENT TEST GATE\n";
    std::cout << "=========================================================\n\n";

    bangla_tsf::TextService service;
    service.Activate(nullptr, 1);
    MockTfContext* mock_ctx = new MockTfContext();

    int total_passed = 0;
    int total_failed = 0;

    auto RunCheck = [&](bool result, const std::string& name) {
        if (result) {
            std::cout << "  [PASS] " << name << "\n";
            total_passed++;
        } else {
            std::cout << "  [FAIL] " << name << "\n";
            total_failed++;
        }
    };

    std::cout << "=== [P0.1] Modifier Keys Themselves Always Pass Through ===\n";
    {
        // Ctrl/Alt/Shift/Win/Apps key-downs themselves must never be eaten.
        // (The full Ctrl+C/V/X/Z/A chord contract lives in test_key_policy,
        //  because GetKeyState/GetAsyncKeyState cannot be injected here.)
        std::vector<std::pair<WPARAM, std::string>> modifier_keys = {
            {VK_CONTROL, "Ctrl"},
            {VK_LCONTROL, "Left Ctrl"},
            {VK_RCONTROL, "Right Ctrl"},
            {VK_MENU, "Alt"},
            {VK_LMENU, "Left Alt"},
            {VK_RMENU, "Right Alt"},
            {VK_SHIFT, "Shift"},
            {VK_LSHIFT, "Left Shift"},
            {VK_RSHIFT, "Right Shift"},
            {VK_LWIN, "Left Win"},
            {VK_RWIN, "Right Win"},
            {VK_APPS, "Apps/Menu"}
        };

        for (const auto& mk : modifier_keys) {
            BOOL pfEaten = TRUE;
            HRESULT hr = service.OnTestKeyDown(mock_ctx, mk.first, 0, &pfEaten);
            bool ok = SUCCEEDED(hr) && (pfEaten == FALSE);
            RunCheck(ok, mk.second + " key itself passes through (never eaten)");
        }
    }

    std::cout << "\n=== [P0.2] Function Keys (F1-F12) Pass-Through ===\n";
    {
        for (WPARAM fkey = VK_F1; fkey <= VK_F12; fkey++) {
            BOOL pfEaten = TRUE;
            HRESULT hr = service.OnTestKeyDown(mock_ctx, fkey, 0, &pfEaten);
            bool ok = SUCCEEDED(hr) && (pfEaten == FALSE);
            RunCheck(ok, "F" + std::to_string(fkey - VK_F1 + 1) + " key passed through without eating");
        }
    }

    std::cout << "\n=== [P0.3] Top-Row Numeric Keys (0-9) Pass Through Natively ===\n";
    {
        // Master-prompt contract: with NO active candidate selection, digits
        // are native numbers — Likhi must NOT swallow them.
        for (char c = '0'; c <= '9'; c++) {
            BOOL pfEaten = TRUE;
            HRESULT hr = service.OnTestKeyDown(mock_ctx, static_cast<WPARAM>(c), 0, &pfEaten);
            bool ok = SUCCEEDED(hr) && (pfEaten == FALSE);
            RunCheck(ok, std::string("Top Row '") + c + "' passes through natively (not eaten)");
        }
    }

    std::cout << "\n=== [P0.4] Physical Numeric Keypad (Numpad Digits & Operators) ===\n";
    {
        // Test Numpad Digits — with no active candidate selection they are
        // native digits/navigation and must NEVER be eaten (regardless of
        // the NumLock toggle state).
        for (int i = 0; i <= 9; i++) {
            WPARAM np_key = VK_NUMPAD0 + i;
            BOOL pfEaten = TRUE;
            HRESULT hr = service.OnTestKeyDown(mock_ctx, np_key, 0, &pfEaten);
            bool ok = SUCCEEDED(hr) && (pfEaten == FALSE);
            RunCheck(ok, "Numpad " + std::to_string(i) + " not eaten when idle");
        }

        // Test Numpad Operators (MUST PASS THROUGH)
        std::vector<std::pair<WPARAM, std::string>> numpad_ops = {
            {VK_MULTIPLY, "Numpad * (Multiply)"},
            {VK_ADD, "Numpad + (Add)"},
            {VK_SUBTRACT, "Numpad - (Subtract)"},
            {VK_DECIMAL, "Numpad . (Decimal)"},
            {VK_DIVIDE, "Numpad / (Divide)"},
            {VK_NUMLOCK, "Num Lock key"}
        };

        for (const auto& no : numpad_ops) {
            BOOL pfEaten = TRUE;
            HRESULT hr = service.OnTestKeyDown(mock_ctx, no.first, 0, &pfEaten);
            bool ok = SUCCEEDED(hr) && (pfEaten == FALSE);
            RunCheck(ok, no.second + " passed through natively");
        }
    }

    std::cout << "\n=== [P0.5] System & Navigation Keys Pass-Through ===\n";
    {
        std::vector<std::pair<WPARAM, std::string>> sys_keys = {
            {VK_SNAPSHOT, "Print Screen"},
            {VK_PAUSE, "Pause / Break"},
            {VK_INSERT, "Insert"},
            {VK_CAPITAL, "Caps Lock"},
            {VK_SCROLL, "Scroll Lock"},
            {VK_LEFT, "Left Arrow (idle state)"},
            {VK_RIGHT, "Right Arrow (idle state)"},
            {VK_UP, "Up Arrow (idle state)"},
            {VK_DOWN, "Down Arrow (idle state)"},
            {VK_HOME, "Home (idle state)"},
            {VK_END, "End (idle state)"},
            {VK_PRIOR, "Page Up (idle state)"},
            {VK_NEXT, "Page Down (idle state)"},
            {VK_DELETE, "Delete (idle state)"},
            {VK_TAB, "Tab (idle state)"},
            {VK_ESCAPE, "Escape (idle state)"},
            {VK_SPACE, "Space (idle state)"}
        };

        for (const auto& sk : sys_keys) {
            BOOL pfEaten = TRUE;
            HRESULT hr = service.OnTestKeyDown(mock_ctx, sk.first, 0, &pfEaten);
            bool ok = SUCCEEDED(hr) && (pfEaten == FALSE);
            RunCheck(ok, sk.second + " not eaten");
        }
    }

    std::cout << "\n=== [P1] Dynamic Composition & Incremental Buffer Evaluation ===\n";
    {
        EngineConfig cfg;
        BanglaEngine_GetDefaultConfig(&cfg);
        cfg.auto_correct_enabled = false;
        BanglaEngine* eng = BanglaEngine_Create(&cfg);

        // Test incremental typing: ANO -> ANOY -> ANOYA -> ANOYAR -> আনোয়ার
        const char* step1 = "ano";
        BanglaEngine_SetComposition(eng, step1);
        CandidateList c1;
        BanglaEngine_GetCandidates(eng, &c1);
        RunCheck(c1.count > 0 && std::string(c1.candidates[0].bengali_text) == "আনো", "Step 1: 'ano' -> 'আনো'");

        const char* step2 = "anoy";
        BanglaEngine_SetComposition(eng, step2);
        CandidateList c2;
        BanglaEngine_GetCandidates(eng, &c2);
        RunCheck(c2.count > 0, "Step 2: 'anoy' re-evaluates active buffer (not committed)");

        const char* step3 = "anoya";
        BanglaEngine_SetComposition(eng, step3);
        CandidateList c3;
        BanglaEngine_GetCandidates(eng, &c3);
        RunCheck(c3.count > 0, "Step 3: 'anoya' continues active composition");

        const char* step4 = "anoyar";
        BanglaEngine_SetComposition(eng, step4);
        CandidateList c4;
        BanglaEngine_GetCandidates(eng, &c4);
        RunCheck(c4.count > 0 && std::string(c4.candidates[0].bengali_text) == "আনোয়ার", "Step 4: 'anoyar' -> 'আনোয়ার'");

        BanglaEngine_Destroy(eng);
    }

    std::cout << "\n=== [P2/P3] Banglish & English Loanwords Recognition ===\n";
    {
        EngineConfig cfg;
        BanglaEngine_GetDefaultConfig(&cfg);
        BanglaEngine* eng = BanglaEngine_Create(&cfg);

        std::vector<std::pair<std::string, std::string>> loanwords = {
            {"fan", "ফ্যান"},
            {"table", "টেবিল"},
            {"chair", "চেয়ার"},
            {"cher", "চেয়ার"},
            {"chear", "চেয়ার"},
            {"computer", "কম্পিউটার"},
            {"mouse", "মাউস"},
            {"control", "কন্ট্রোল"},
            {"mobile", "মোবাইল"},
            {"office", "অফিস"},
            {"anwar", "আনোয়ার"},
            {"hosen", "হোসেন"},
            {"rahman", "রহমান"},
            {"sumaiya", "সুমাইয়া"},
            {"imran", "ইমরান"}
        };

        for (const auto& lw : loanwords) {
            BanglaEngine_SetComposition(eng, lw.first.c_str());
            CandidateList list;
            BanglaEngine_GetCandidates(eng, &list);
            bool matched = false;
            for (uint32_t i = 0; i < list.count; i++) {
                if (std::string(list.candidates[i].bengali_text) == lw.second) {
                    matched = true;
                    break;
                }
            }
            RunCheck(matched, "Loanword/Name '" + lw.first + "' -> '" + lw.second + "' found in candidates");
        }

        BanglaEngine_Destroy(eng);
    }

    mock_ctx->Release();
    service.Deactivate();

    std::cout << "\n=========================================================\n";
    std::cout << "  P0 CRITICAL TEST RESULTS SUMMARY\n";
    std::cout << "  Total Passed: " << total_passed << "\n";
    std::cout << "  Total Failed: " << total_failed << "\n";
    std::cout << "  Status: " << ((total_failed == 0) ? "100% SUCCESS (P0 GATE PASSED)" : "FAILURE") << "\n";
    std::cout << "=========================================================\n";

    return (total_failed == 0) ? 0 : 1;
}
