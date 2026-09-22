#include "../include/composition_mgr.h"
#include "../include/text_service.h"
#include "../include/tsf_utils.h"
#include "../include/edit_session.h"
#include "../include/tsf_log.h"
#include <iostream>
#include <string>
#include <vector>
#include <shlobj.h>
#include <fstream>

namespace bangla_tsf {

extern HINSTANCE g_hInstance;

static std::string FindLexiconPath(HINSTANCE hInst) {
    // 1. Try directory containing the DLL
    if (hInst) {
        wchar_t dll_path[MAX_PATH] = {0};
        if (GetModuleFileNameW(hInst, dll_path, MAX_PATH)) {
            std::wstring ws(dll_path);
            size_t pos = ws.find_last_of(L"\\/");
            if (pos != std::wstring::npos) {
                std::wstring dir = ws.substr(0, pos);
                
                // Check dir\data\lexicon.bin
                std::wstring p1 = dir + L"\\data\\lexicon.bin";
                if (GetFileAttributesW(p1.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    return bangla_tsf::Utf16ToUtf8(p1);
                }
                
                // Check dir\lexicon.bin
                std::wstring p2 = dir + L"\\lexicon.bin";
                if (GetFileAttributesW(p2.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    return bangla_tsf::Utf16ToUtf8(p2);
                }

                // Check dir\..\engine\data\lexicon.bin
                std::wstring p3 = dir + L"\\..\\engine\\data\\lexicon.bin";
                if (GetFileAttributesW(p3.c_str()) != INVALID_FILE_ATTRIBUTES) {
                    return bangla_tsf::Utf16ToUtf8(p3);
                }
            }
        }
    }

    // 2. Try %APPDATA%\PC-Bangla-Typing-App\lexicon.bin
    wchar_t appdata[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appdata))) {
        std::wstring p_app = std::wstring(appdata) + L"\\PC-Bangla-Typing-App\\lexicon.bin";
        if (GetFileAttributesW(p_app.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return bangla_tsf::Utf16ToUtf8(p_app);
        }
    }

    // 3. Fallbacks
    if (GetFileAttributesW(L"engine\\data\\lexicon.bin") != INVALID_FILE_ATTRIBUTES) {
        return "engine/data/lexicon.bin";
    }
    if (GetFileAttributesW(L"lexicon.bin") != INVALID_FILE_ATTRIBUTES) {
        return "lexicon.bin";
    }

    return "engine/data/lexicon.bin";
}

CompositionManager::CompositionManager(TextService* service)
    : service_(service),
      engine_(nullptr),
      last_dict_file_time_({0, 0}),
      last_settings_file_time_({0, 0}),
      is_composing_(false),
      is_top_exact_match_(false),
      selected_candidate_idx_(0),
      active_composition_(nullptr),
      current_context_(nullptr),
      cached_caret_rect_({0, 0, 0, 0}),
      has_cached_caret_rect_(false) {
}

CompositionManager::~CompositionManager() {
    Shutdown();
}

bool CompositionManager::Initialize(HINSTANCE hInst) {
    // 1. Initialize Engine Configuration
    EngineConfig config;
    BanglaEngine_GetDefaultConfig(&config);
    config.auto_correct_enabled = false;
    config.max_candidates = 6;

    lexicon_path_ = FindLexiconPath(hInst);
    config.lexicon_binary_path = lexicon_path_.c_str();

    wchar_t appdata_path[MAX_PATH];
    int initial_theme_mode = 0;
    if (GetEnvironmentVariableW(L"APPDATA", appdata_path, MAX_PATH) > 0) {
        std::wstring dir = std::wstring(appdata_path) + L"\\PC-Bangla-Typing-App";
        CreateDirectoryW(dir.c_str(), NULL);
        std::wstring dict_w = dir + L"\\personal_dict.txt";
        user_dict_path_ = bangla_tsf::Utf16ToUtf8(dict_w);
        config.user_dict_path = user_dict_path_.c_str();

        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (GetFileAttributesExW(dict_w.c_str(), GetFileExInfoStandard, &fad)) {
            last_dict_file_time_ = fad.ftLastWriteTime;
        } else {
            last_dict_file_time_ = {0, 0};
        }

        settings_file_path_ = dir + L"\\settings.json";
        WIN32_FILE_ATTRIBUTE_DATA sfad;
        if (GetFileAttributesExW(settings_file_path_.c_str(), GetFileExInfoStandard, &sfad)) {
            last_settings_file_time_ = sfad.ftLastWriteTime;
            std::ifstream sin(bangla_tsf::Utf16ToUtf8(settings_file_path_));
            if (sin.is_open()) {
                std::string line;
                while (std::getline(sin, line)) {
                    if (line.find("\"auto_correct\": true") != std::string::npos) config.auto_correct_enabled = true;
                    if (line.find("\"auto_correct\": false") != std::string::npos) config.auto_correct_enabled = false;
                    if (line.find("\"max_candidates\": 3") != std::string::npos) config.max_candidates = 3;
                    if (line.find("\"max_candidates\": 4") != std::string::npos) config.max_candidates = 4;
                    if (line.find("\"max_candidates\": 5") != std::string::npos) config.max_candidates = 5;
                    if (line.find("\"theme\": 1") != std::string::npos) initial_theme_mode = 1;
                    if (line.find("\"theme\": 2") != std::string::npos) initial_theme_mode = 2;
                    if (line.find("\"theme\": 0") != std::string::npos) initial_theme_mode = 0;
                }
            }
        } else {
            last_settings_file_time_ = {0, 0};
        }
    }

    engine_ = BanglaEngine_Create(&config);
    if (!engine_) {
        config.lexicon_binary_path = nullptr;
        engine_ = BanglaEngine_Create(&config);
    }

    // 2. Initialize Candidate Window UI (if GUI instance provided)
    if (hInst) {
        candidate_window_.Initialize(hInst);
        candidate_window_.SetThemeMode(initial_theme_mode);
        candidate_window_.SetSelectionCallback([this](size_t index) {
            OnCandidateWindowSelection(index);
        });
    }

    return true;
}

void CompositionManager::Shutdown() {
    if (active_composition_) {
        active_composition_->Release();
        active_composition_ = nullptr;
    }
    if (engine_) {
        BanglaEngine_Destroy(engine_);
        engine_ = nullptr;
    }
    candidate_window_.Destroy();
}

void CompositionManager::SetAutoCorrectEnabled(bool enabled) {
    if (engine_) {
        BanglaEngine_SetAutoCorrectEnabled(engine_, enabled);
    }
}

void CompositionManager::SetMaxCandidates(uint32_t max_cands) {
    if (engine_) {
        BanglaEngine_SetMaxCandidates(engine_, max_cands);
    }
}

RECT CompositionManager::GetCaretRect(ITfContext* pContext) {
    (void)pContext;
    // 1. TSF text extension cached during active EditCookie session
    if (has_cached_caret_rect_) {
        if ((cached_caret_rect_.right > cached_caret_rect_.left || cached_caret_rect_.bottom > cached_caret_rect_.top)
            && !(cached_caret_rect_.left <= 0 && cached_caret_rect_.top <= 0)) {
            return cached_caret_rect_;
        }
    }

    // 2. Win32 GUITHREADINFO caret detection (Chrome, Edge, Word, Notepad, Electron, etc.)
    GUITHREADINFO gti;
    ZeroMemory(&gti, sizeof(gti));
    gti.cbSize = sizeof(GUITHREADINFO);
    if (GetGUIThreadInfo(0, &gti)) {
        if (gti.hwndCaret && IsWindow(gti.hwndCaret)) {
            RECT rc = gti.rcCaret;
            POINT ptTL = { rc.left, rc.top };
            POINT ptBR = { rc.right, rc.bottom };
            ClientToScreen(gti.hwndCaret, &ptTL);
            ClientToScreen(gti.hwndCaret, &ptBR);
            rc.left = ptTL.x;
            rc.top = ptTL.y;
            rc.right = ptBR.x;
            rc.bottom = ptBR.y;
            if (rc.bottom > rc.top || rc.right > rc.left) {
                if (rc.bottom == rc.top) rc.bottom = rc.top + 20;
                return rc;
            }
        }
    }

    // 3. Fallback: GetCaretPos with hwndFocus or ForegroundWindow
    HWND hFocus = gti.hwndFocus ? gti.hwndFocus : GetFocus();
    if (!hFocus) {
        hFocus = GetForegroundWindow();
    }
    if (hFocus && IsWindow(hFocus)) {
        POINT pt = { 0, 0 };
        if (GetCaretPos(&pt) && (pt.x != 0 || pt.y != 0)) {
            ClientToScreen(hFocus, &pt);
            RECT rc = { pt.x, pt.y, pt.x + 2, pt.y + 20 };
            return rc;
        }
    }

    // 4. Cursor position fallback (where user clicked or mouse is)
    POINT ptCursor = { 0, 0 };
    if (GetCursorPos(&ptCursor) && (ptCursor.x != 0 || ptCursor.y != 0)) {
        RECT rc = { ptCursor.x, ptCursor.y, ptCursor.x + 2, ptCursor.y + 20 };
        return rc;
    }

    // 5. Default fallback
    RECT fallback = { 100, 100, 100, 120 };
    return fallback;
}

void CompositionManager::CheckAndReloadUserDict() {
    if (!engine_) return;

    if (!user_dict_path_.empty()) {
        std::wstring dict_w = bangla_tsf::Utf8ToUtf16(user_dict_path_);
        WIN32_FILE_ATTRIBUTE_DATA fad;
        if (GetFileAttributesExW(dict_w.c_str(), GetFileExInfoStandard, &fad)) {
            if (CompareFileTime(&fad.ftLastWriteTime, &last_dict_file_time_) != 0) {
                last_dict_file_time_ = fad.ftLastWriteTime;
                BanglaEngine_ReloadUserDict(engine_);
                TsfLog("CheckAndReloadUserDict: reloaded %s", user_dict_path_.c_str());
            }
        }
    }

    if (!settings_file_path_.empty()) {
        WIN32_FILE_ATTRIBUTE_DATA sfad;
        if (GetFileAttributesExW(settings_file_path_.c_str(), GetFileExInfoStandard, &sfad)) {
            if (CompareFileTime(&sfad.ftLastWriteTime, &last_settings_file_time_) != 0) {
                last_settings_file_time_ = sfad.ftLastWriteTime;
                std::ifstream sin(bangla_tsf::Utf16ToUtf8(settings_file_path_));
                if (sin.is_open()) {
                    std::string line;
                    bool auto_correct = false;
                    uint32_t max_cands = 5;
                    int theme_mode = candidate_window_.GetThemeMode();
                    while (std::getline(sin, line)) {
                        if (line.find("\"auto_correct\": true") != std::string::npos) auto_correct = true;
                        if (line.find("\"auto_correct\": false") != std::string::npos) auto_correct = false;
                        if (line.find("\"max_candidates\": 3") != std::string::npos) max_cands = 3;
                        if (line.find("\"max_candidates\": 4") != std::string::npos) max_cands = 4;
                        if (line.find("\"max_candidates\": 5") != std::string::npos) max_cands = 5;
                        if (line.find("\"theme\": 1") != std::string::npos) theme_mode = 1;
                        if (line.find("\"theme\": 2") != std::string::npos) theme_mode = 2;
                        if (line.find("\"theme\": 0") != std::string::npos) theme_mode = 0;
                    }
                    BanglaEngine_SetAutoCorrectEnabled(engine_, auto_correct);
                    BanglaEngine_SetMaxCandidates(engine_, max_cands);
                    candidate_window_.SetThemeMode(theme_mode);
                    TsfLog("CheckAndReloadUserDict: updated auto_correct to %d, max_candidates to %u, theme to %d", auto_correct ? 1 : 0, max_cands, theme_mode);
                }
            }
        }
    }
}

void CompositionManager::UpdateCompositionAndUI(ITfContext* pContext) {
    if (!engine_) return;

    CheckAndReloadUserDict();

    current_context_ = pContext;

    if (roman_buffer_.empty()) {
        CancelComposition(pContext);
        return;
    }

    is_composing_ = true;

    // 1. Query candidates from native engine
    BanglaEngine_ResetComposition(engine_);
    BanglaEngine_SetComposition(engine_, roman_buffer_.c_str());

    CandidateList list;
    BanglaEngine_GetCandidates(engine_, &list);

    current_candidates_w_.clear();
    is_top_exact_match_ = false;
    if (list.count > 0) {
        if (list.candidates[0].category_flags & (CANDIDATE_FLAG_EXACT_MATCH | CANDIDATE_FLAG_PERSONAL)) {
            is_top_exact_match_ = true;
        }
    }
    for (uint32_t i = 0; i < list.count; i++) {
        current_candidates_w_.push_back(Utf8ToUtf16(list.candidates[i].bengali_text));
    }

    // Append raw English input as candidate 6 (matching Google Input Tools: e.g. "৬. ami")
    std::wstring raw_w = Utf8ToUtf16(roman_buffer_);
    bool already_has_raw = false;
    for (const auto& cw : current_candidates_w_) {
        if (cw == raw_w) { already_has_raw = true; break; }
    }
    if (!already_has_raw && !raw_w.empty()) {
        current_candidates_w_.push_back(raw_w);
    }

    if (!current_candidates_w_.empty()) {
        current_bengali_top_ = current_candidates_w_[0];
    } else {
        current_bengali_top_ = raw_w;
    }

    selected_candidate_idx_ = 0;

    if (!pContext) return;

    TsfLog("UpdateCompositionAndUI: pContext=%p buffer='%s' top_len=%zu",
           pContext, roman_buffer_.c_str(), current_bengali_top_.length());

    // 2. Request single atomic TSF Edit Session to start/update inline composition text
    ITfEditSession* updateSession = new ActionEditSession(pContext, [this, pContext](TfEditCookie ec) -> HRESULT {
        TsfLog("updateSession DoEditSession: ec=0x%X active_comp=%p", ec, this->active_composition_);
        // Step A: If composition is not started, start it
        if (!this->active_composition_) {
            ITfContextComposition* pContextComp = nullptr;
            HRESULT hrCC = pContext->QueryInterface(IID_ITfContextComposition, (void**)&pContextComp);
            TsfLog("QueryInterface ITfContextComposition hr=0x%08X pContextComp=%p", hrCC, pContextComp);
            if (SUCCEEDED(hrCC) && pContextComp) {
                ITfRange* pInsertRange = nullptr;
                ITfInsertAtSelection* pInsertAtSelection = nullptr;

                HRESULT hrIAS = pContext->QueryInterface(IID_ITfInsertAtSelection, (void**)&pInsertAtSelection);
                TsfLog("QueryInterface ITfInsertAtSelection hr=0x%08X", hrIAS);
                if (SUCCEEDED(hrIAS) && pInsertAtSelection) {
                    HRESULT hrIns = pInsertAtSelection->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, NULL, 0, &pInsertRange);
                    TsfLog("InsertTextAtSelection hr=0x%08X pInsertRange=%p", hrIns, pInsertRange);
                    pInsertAtSelection->Release();
                }

                if (!pInsertRange) {
                    TF_SELECTION sel;
                    ULONG cFetched = 0;
                    HRESULT hrSel = pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &cFetched);
                    TsfLog("GetSelection hr=0x%08X cFetched=%lu range=%p", hrSel, cFetched, sel.range);
                    if (SUCCEEDED(hrSel) && cFetched > 0 && sel.range) {
                        pInsertRange = sel.range;
                    }
                }

                if (pInsertRange) {
                    ITfComposition* pComp = nullptr;
                    HRESULT hrStart = pContextComp->StartComposition(ec, pInsertRange, this->service_, &pComp);
                    TsfLog("StartComposition hr=0x%08X pComp=%p", hrStart, pComp);
                    if (SUCCEEDED(hrStart) && pComp) {
                        this->active_composition_ = pComp;
                        this->is_composing_ = true;
                    }
                    pInsertRange->Release();
                }
                pContextComp->Release();
            }
        }

        // Step B: Set composition text and query caret location in the same edit cookie
        if (this->active_composition_) {
            ITfRange* pRange = nullptr;
            HRESULT hrRange = this->active_composition_->GetRange(&pRange);
            TsfLog("active_composition_->GetRange hr=0x%08X pRange=%p", hrRange, pRange);
            if (SUCCEEDED(hrRange) && pRange) {
                HRESULT hrSetText = pRange->SetText(ec, 0, this->current_bengali_top_.c_str(), (LONG)this->current_bengali_top_.length());
                TsfLog("pRange->SetText hr=0x%08X", hrSetText);

                // Update selection to the end of the active composition so host apps
                // (such as WhatsApp/Chromium/Electron) maintain accurate cursor state.
                ITfRange* pSelectionRange = nullptr;
                if (SUCCEEDED(pRange->Clone(&pSelectionRange)) && pSelectionRange) {
                    pSelectionRange->Collapse(ec, TF_ANCHOR_END);
                    TF_SELECTION sel;
                    sel.range = pSelectionRange;
                    sel.style.ase = TF_AE_NONE;
                    sel.style.fInterimChar = FALSE;
                    pContext->SetSelection(ec, 1, &sel);
                    pSelectionRange->Release();
                }

                ITfContextView* pView = nullptr;
                if (SUCCEEDED(pContext->GetActiveView(&pView)) && pView) {
                    RECT rc = {0, 0, 0, 0};
                    BOOL fClipped = FALSE;
                    if (SUCCEEDED(pView->GetTextExt(ec, pRange, &rc, &fClipped))) {
                        if (rc.right > rc.left || rc.bottom > rc.top) {
                            this->cached_caret_rect_ = rc;
                            this->has_cached_caret_rect_ = true;
                        }
                    }
                    pView->Release();
                }

                pRange->Release();
            }
        }
        return S_OK;
    });

    HRESULT hrSession = S_OK;
    HRESULT hr = pContext->RequestEditSession(service_->GetClientId(), updateSession, TF_ES_READWRITE | TF_ES_SYNC, &hrSession);
    TsfLog("RequestEditSession (SYNC) hr=0x%08X hrSession=0x%08X", hr, hrSession);
    if (hr == TF_E_SYNCHRONOUS) {
        hr = pContext->RequestEditSession(service_->GetClientId(), updateSession, TF_ES_READWRITE, &hrSession);
        TsfLog("RequestEditSession (ASYNC fallback) hr=0x%08X hrSession=0x%08X", hr, hrSession);
    }
    updateSession->Release();

    // 3. Update Floating Suggestion Window
    RECT caret_rect = GetCaretRect(pContext);
    candidate_window_.ShowCandidates(current_candidates_w_, selected_candidate_idx_, caret_rect, raw_w);
}

bool CompositionManager::OnCharacter(ITfContext* pContext, char ch) {
    roman_buffer_.push_back(ch);
    UpdateCompositionAndUI(pContext);
    return true;
}

bool CompositionManager::OnBackspace(ITfContext* pContext) {
    if (!is_composing_ || roman_buffer_.empty()) {
        return false;
    }

    roman_buffer_.pop_back();

    if (roman_buffer_.empty()) {
        CancelComposition(pContext);
    } else {
        UpdateCompositionAndUI(pContext);
    }
    return true;
}

bool CompositionManager::OnSpace(ITfContext* pContext) {
    if (!is_composing_) {
        return false;
    }
    // OnTestKeyDown returned TRUE for Space when composing,
    // so the host will NOT insert a space. We must do it ourselves via TSF.
    // Strategy: commit the Bengali word, then insert exactly one U+0020 space.
    return CommitCurrentComposition(pContext, selected_candidate_idx_, /*append_space=*/true);
}

bool CompositionManager::OnEnter(ITfContext* pContext) {
    if (!is_composing_) {
        return false;
    }
    return CommitCurrentComposition(pContext, selected_candidate_idx_, /*append_space=*/false);
}

bool CompositionManager::OnEscape(ITfContext* pContext) {
    if (!is_composing_) {
        return false;
    }
    return CancelComposition(pContext);
}

bool CompositionManager::OnArrow(ITfContext* pContext, bool down_next) {
    (void)pContext;
    if (!is_composing_ || current_candidates_w_.empty()) {
        return false;
    }

    if (down_next) {
        selected_candidate_idx_ = (selected_candidate_idx_ + 1) % current_candidates_w_.size();
        candidate_window_.SelectNext();
    } else {
        if (selected_candidate_idx_ == 0) {
            selected_candidate_idx_ = current_candidates_w_.size() - 1;
        } else {
            selected_candidate_idx_--;
        }
        candidate_window_.SelectPrev();
    }
    return true;
}

bool CompositionManager::OnNumberSelection(ITfContext* pContext, int num_1_to_9) {
    if (num_1_to_9 < 1) return false;
    size_t idx = static_cast<size_t>(num_1_to_9 - 1);
    if (!is_composing_ || idx >= current_candidates_w_.size()) {
        return false;
    }
    return CommitCurrentComposition(pContext, idx, /*append_space=*/false);
}

bool CompositionManager::CanSelectCandidate(char digit) {
    if (!is_composing_ || current_candidates_w_.empty()) return false;
    if (digit < '1' || digit > '9') return false;
    size_t idx = static_cast<size_t>(digit - '1');
    return idx < current_candidates_w_.size();
}

bool CompositionManager::OnDigit(ITfContext* pContext, char ascii_digit) {
    if (ascii_digit < '0' || ascii_digit > '9') {
        return false;
    }

    if (CanSelectCandidate(ascii_digit)) {
        return OnNumberSelection(pContext, ascii_digit - '0');
    }
    
    return false; // Not a valid candidate selection. Host natively handles numbers.
}

bool CompositionManager::OnPunctuation(ITfContext* pContext, char punct) {
    // Convert standard '.' to Bengali Dāri '।'
    std::wstring punct_str = (punct == '.') ? L"।" : std::wstring(1, (wchar_t)punct);

    if (is_composing_) {
        return CommitCurrentComposition(pContext, selected_candidate_idx_, /*append_space=*/false, punct_str);
    }

    if (pContext && service_) {
        ITfEditSession* punctSession = new ActionEditSession(pContext, [this, punct_str, pContext](TfEditCookie ec) -> HRESULT {
            bool inserted = false;

            // Tier 1: Try ephemeral ITfComposition (Universal in Chromium, Edge, Web inputs)
            ITfContextComposition* pContextComp = nullptr;
            if (SUCCEEDED(pContext->QueryInterface(IID_ITfContextComposition, (void**)&pContextComp)) && pContextComp) {
                TF_SELECTION sel;
                ULONG cFetched = 0;
                if (SUCCEEDED(pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &cFetched)) && cFetched > 0 && sel.range) {
                    ITfComposition* pEphemeralComp = nullptr;
                    if (SUCCEEDED(pContextComp->StartComposition(ec, sel.range, this->service_, &pEphemeralComp)) && pEphemeralComp) {
                        sel.range->SetText(ec, 0, punct_str.c_str(), (LONG)punct_str.length());
                        sel.range->Collapse(ec, TF_ANCHOR_END);
                        pEphemeralComp->EndComposition(ec);
                        pEphemeralComp->Release();
                        inserted = true;
                    }
                    sel.range->Release();
                }
                pContextComp->Release();
            }

            // Tier 2: Try ITfInsertAtSelection (Standard Word, Notepad, RichEdit)
            if (!inserted) {
                ITfInsertAtSelection* pInsertAtSelection = nullptr;
                if (SUCCEEDED(pContext->QueryInterface(IID_ITfInsertAtSelection, (void**)&pInsertAtSelection)) && pInsertAtSelection) {
                    ITfRange* pRange = nullptr;
                    if (SUCCEEDED(pInsertAtSelection->InsertTextAtSelection(ec, 0, punct_str.c_str(), (LONG)punct_str.length(), &pRange)) && pRange) {
                        pRange->Collapse(ec, TF_ANCHOR_END);
                        TF_SELECTION sel;
                        sel.range = pRange;
                        sel.style.ase = TF_AE_NONE;
                        sel.style.fInterimChar = FALSE;
                        pContext->SetSelection(ec, 1, &sel);
                        pRange->Release();
                        inserted = true;
                    }
                    pInsertAtSelection->Release();
                }
            }

            // Tier 3: Direct selection range replace
            if (!inserted) {
                TF_SELECTION sel;
                ULONG cFetched = 0;
                if (SUCCEEDED(pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &cFetched)) && cFetched > 0 && sel.range) {
                    sel.range->SetText(ec, 0, punct_str.c_str(), (LONG)punct_str.length());
                    sel.range->Collapse(ec, TF_ANCHOR_END);
                    pContext->SetSelection(ec, 1, &sel);
                    sel.range->Release();
                    inserted = true;
                }
            }
            return S_OK;
        });

        HRESULT hr = S_OK;
        HRESULT hrSession = S_OK;
        hr = pContext->RequestEditSession(service_->GetClientId(), punctSession, TF_ES_READWRITE | TF_ES_SYNC, &hrSession);
        if (hr == TF_E_SYNCHRONOUS) {
            pContext->RequestEditSession(service_->GetClientId(), punctSession, TF_ES_READWRITE, &hrSession);
        }
        punctSession->Release();
    }

    return true;
}

bool CompositionManager::CommitCurrentComposition(ITfContext* pContext, size_t candidate_idx, bool append_space, const std::wstring& append_punct) {
    if (!is_composing_) {
        return false;
    }

    std::wstring chosen_w;
    if (candidate_idx < current_candidates_w_.size()) {
        chosen_w = current_candidates_w_[candidate_idx];
    } else if (!current_bengali_top_.empty()) {
        chosen_w = current_bengali_top_;
    } else {
        chosen_w = Utf8ToUtf16(roman_buffer_);
    }

    std::string chosen_u8 = Utf16ToUtf8(chosen_w);

    TsfLog("CommitCurrentComposition: text='%s' space=%d punct='%ls' active_comp=%p",
           chosen_u8.c_str(), append_space, append_punct.c_str(), this->active_composition_);

    if (pContext && service_) {
        // Build full text to commit: Bengali word + optional punctuation + optional space
        std::wstring text_to_commit = chosen_w;
        if (!append_punct.empty()) {
            text_to_commit += append_punct;
        }
        if (append_space) {
            text_to_commit.push_back(L' ');
        }

        ITfEditSession* commitSession = new ActionEditSession(pContext,
            [this, text_to_commit, pContext](TfEditCookie ec) -> HRESULT {
            TsfLog("commitSession DoEditSession: ec=0x%X active_comp=%p", ec, this->active_composition_);

            // Set the composition range text to the complete word (+ space) atomically
            if (this->active_composition_) {
                ITfRange* pRange = nullptr;
                HRESULT hrRange = this->active_composition_->GetRange(&pRange);
                TsfLog("commitSession GetRange hr=0x%08X pRange=%p", hrRange, pRange);
                if (SUCCEEDED(hrRange) && pRange) {
                    HRESULT hrSetText = pRange->SetText(ec, 0, text_to_commit.c_str(), (LONG)text_to_commit.length());
                    TsfLog("commitSession SetText hr=0x%08X", hrSetText);

                    // 4. Advance caret using a cloned range so the active composition's range
                    //    remains covering the full committed word when EndComposition is called.
                    ITfRange* pSelectionRange = nullptr;
                    if (SUCCEEDED(pRange->Clone(&pSelectionRange)) && pSelectionRange) {
                        pSelectionRange->Collapse(ec, TF_ANCHOR_END);
                        TF_SELECTION sel;
                        sel.range = pSelectionRange;
                        sel.style.ase = TF_AE_NONE;
                        sel.style.fInterimChar = FALSE;
                        pContext->SetSelection(ec, 1, &sel);
                        pSelectionRange->Release();
                    }

                    pRange->Release();
                }
                HRESULT hrEnd = this->active_composition_->EndComposition(ec);
                TsfLog("commitSession EndComposition hr=0x%08X", hrEnd);
                this->active_composition_->Release();
                this->active_composition_ = nullptr;
            } else {
                // Fallback: If active_composition_ was null or closed prematurely by host, insert text directly at selection
                TsfLog("commitSession Fallback (active_comp is null)");
                ITfInsertAtSelection* pInsertAtSelection = nullptr;
                bool inserted = false;
                if (SUCCEEDED(pContext->QueryInterface(IID_ITfInsertAtSelection, (void**)&pInsertAtSelection)) && pInsertAtSelection) {
                    ITfRange* pRange = nullptr;
                    if (SUCCEEDED(pInsertAtSelection->InsertTextAtSelection(ec, 0, text_to_commit.c_str(), (LONG)text_to_commit.length(), &pRange)) && pRange) {
                        pRange->Collapse(ec, TF_ANCHOR_END);
                        TF_SELECTION sel;
                        sel.range = pRange;
                        sel.style.ase = TF_AE_NONE;
                        sel.style.fInterimChar = FALSE;
                        pContext->SetSelection(ec, 1, &sel);
                        pRange->Release();
                        inserted = true;
                        TsfLog("commitSession Fallback inserted via InsertTextAtSelection");
                    }
                    pInsertAtSelection->Release();
                }
                if (!inserted) {
                    TF_SELECTION sel;
                    ULONG cFetched = 0;
                    if (SUCCEEDED(pContext->GetSelection(ec, TF_DEFAULT_SELECTION, 1, &sel, &cFetched)) && cFetched > 0 && sel.range) {
                        sel.range->SetText(ec, 0, text_to_commit.c_str(), (LONG)text_to_commit.length());
                        sel.range->Collapse(ec, TF_ANCHOR_END);
                        pContext->SetSelection(ec, 1, &sel);
                        sel.range->Release();
                        inserted = true;
                        TsfLog("commitSession Fallback inserted via GetSelection");
                    }
                }
            }
            return S_OK;
        });

        HRESULT hr = S_OK;
        HRESULT hrSession = S_OK;
        hr = pContext->RequestEditSession(service_->GetClientId(), commitSession,
                                          TF_ES_READWRITE | TF_ES_SYNC, &hrSession);
        TsfLog("commitSession RequestEditSession (SYNC) hr=0x%08X hrSession=0x%08X", hr, hrSession);
        if (hr == TF_E_SYNCHRONOUS) {
            hr = pContext->RequestEditSession(service_->GetClientId(), commitSession,
                                          TF_ES_READWRITE, &hrSession);
            TsfLog("commitSession RequestEditSession (ASYNC) hr=0x%08X hrSession=0x%08X", hr, hrSession);
        }
        commitSession->Release();
    }

    // Notify language engine of committed word for N-gram context & user learning
    if (engine_) {
        BanglaEngine_CommitWordWithOrigin(engine_, roman_buffer_.c_str(), chosen_u8.c_str());
    }

    // Reset local state
    is_composing_ = false;
    roman_buffer_.clear();
    current_candidates_w_.clear();
    current_bengali_top_.clear();
    selected_candidate_idx_ = 0;
    has_cached_caret_rect_ = false;
    candidate_window_.Hide();

    return true;
}

bool CompositionManager::CancelComposition(ITfContext* pContext) {
    if (!is_composing_) {
        roman_buffer_.clear();
        candidate_window_.Hide();
        has_cached_caret_rect_ = false;
        return false;
    }

    if (pContext && service_) {
        ITfEditSession* cancelSession = new ActionEditSession(pContext, [this](TfEditCookie ec) -> HRESULT {
            if (this->active_composition_) {
                ITfRange* pRange = nullptr;
                if (SUCCEEDED(this->active_composition_->GetRange(&pRange)) && pRange) {
                    pRange->SetText(ec, 0, L"", 0);
                    pRange->Release();
                }
                this->active_composition_->EndComposition(ec);
                this->active_composition_->Release();
                this->active_composition_ = nullptr;
            }
            return S_OK;
        });

        HRESULT hr = S_OK;
        pContext->RequestEditSession(service_->GetClientId(), cancelSession, TF_ES_READWRITE | TF_ES_SYNC, &hr);
        cancelSession->Release();
    }

    is_composing_ = false;
    roman_buffer_.clear();
    current_candidates_w_.clear();
    current_bengali_top_.clear();
    selected_candidate_idx_ = 0;
    has_cached_caret_rect_ = false;
    candidate_window_.Hide();

    return true;
}

void CompositionManager::OnCompositionTerminated(ITfContext* pContext, ITfComposition* pComposition) {
    (void)pContext;
    if (active_composition_ == pComposition) {
        active_composition_->Release();
        active_composition_ = nullptr;
    }
    is_composing_ = false;
    roman_buffer_.clear();
    current_candidates_w_.clear();
    has_cached_caret_rect_ = false;
    candidate_window_.Hide();
}

void CompositionManager::OnFocusLost(ITfContext* pContext) {
    if (is_composing_) {
        CommitCurrentComposition(pContext, selected_candidate_idx_);
    }
    candidate_window_.Hide();
}

void CompositionManager::OnCandidateWindowSelection(size_t index) {
    if (current_context_ && is_composing_) {
        CommitCurrentComposition(current_context_, index);
    }
}

} // namespace bangla_tsf


