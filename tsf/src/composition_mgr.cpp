#include "../include/composition_mgr.h"
#include "../include/text_service.h"
#include "../include/tsf_utils.h"
#include "../include/edit_session.h"
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
      is_composing_(false),
      selected_candidate_idx_(0),
      active_composition_(nullptr),
      current_context_(nullptr) {
}

CompositionManager::~CompositionManager() {
    Shutdown();
}

bool CompositionManager::Initialize(HINSTANCE hInst) {
    // 1. Initialize Engine Configuration
    EngineConfig config;
    BanglaEngine_GetDefaultConfig(&config);
    config.auto_correct_enabled = false;
    config.max_candidates = 5;

    std::string found_lex = FindLexiconPath(hInst);
    config.lexicon_binary_path = found_lex.c_str();

    engine_ = BanglaEngine_Create(&config);
    if (!engine_) {
        config.lexicon_binary_path = nullptr;
        engine_ = BanglaEngine_Create(&config);
    }

    // 2. Initialize Candidate Window UI (if GUI instance provided)
    if (hInst) {
        candidate_window_.Initialize(hInst);
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
        EngineConfig cfg;
        BanglaEngine_GetDefaultConfig(&cfg);
        cfg.auto_correct_enabled = enabled;
        std::string found_lex = FindLexiconPath(g_hInstance);
        cfg.lexicon_binary_path = found_lex.c_str();
        BanglaEngine_Destroy(engine_);
        engine_ = BanglaEngine_Create(&cfg);
    }
}

void CompositionManager::SetMaxCandidates(uint32_t max_cands) {
    if (engine_) {
        EngineConfig cfg;
        BanglaEngine_GetDefaultConfig(&cfg);
        cfg.max_candidates = max_cands;
        std::string found_lex = FindLexiconPath(g_hInstance);
        cfg.lexicon_binary_path = found_lex.c_str();
        BanglaEngine_Destroy(engine_);
        engine_ = BanglaEngine_Create(&cfg);
    }
}

RECT CompositionManager::GetCaretRect(ITfContext* pContext) {
    RECT caret_rect = { 100, 100, 100, 120 };
    if (!pContext) return caret_rect;

    ITfContextView* pView = nullptr;
    if (SUCCEEDED(pContext->GetActiveView(&pView)) && pView) {
        if (active_composition_) {
            ITfRange* pRange = nullptr;
            if (SUCCEEDED(active_composition_->GetRange(&pRange)) && pRange) {
                BOOL fClipped = FALSE;
                pView->GetTextExt(0, pRange, &caret_rect, &fClipped);
                pRange->Release();
            }
        }
        pView->Release();
    }
    return caret_rect;
}

void CompositionManager::UpdateCompositionAndUI(ITfContext* pContext) {
    if (!engine_) return;

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
    for (uint32_t i = 0; i < list.count; i++) {
        current_candidates_w_.push_back(Utf8ToUtf16(list.candidates[i].bengali_text));
    }

    if (!current_candidates_w_.empty()) {
        current_bengali_top_ = current_candidates_w_[0];
    } else {
        current_bengali_top_ = Utf8ToUtf16(roman_buffer_);
    }

    selected_candidate_idx_ = 0;

    if (!pContext) return;

    // 2. Request TSF Edit Session to update inline composition text
    ITfContextComposition* pContextComp = nullptr;
    if (SUCCEEDED(pContext->QueryInterface(IID_ITfContextComposition, (void**)&pContextComp))) {
        if (!active_composition_) {
            // Start composition
            ITfEditSession* startSession = new ActionEditSession(pContext, [this, pContextComp](TfEditCookie ec) -> HRESULT {
                ITfRange* pInsertRange = nullptr;
                ITfInsertAtSelection* pInsertAtSelection = nullptr;

                if (SUCCEEDED(this->current_context_->QueryInterface(IID_ITfInsertAtSelection, (void**)&pInsertAtSelection))) {
                    pInsertAtSelection->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, NULL, 0, &pInsertRange);
                    pInsertAtSelection->Release();
                }

                if (pInsertRange) {
                    ITfComposition* pComp = nullptr;
                    if (SUCCEEDED(pContextComp->StartComposition(ec, pInsertRange, this->service_, &pComp)) && pComp) {
                        this->active_composition_ = pComp;
                        this->is_composing_ = true;
                    }
                    pInsertRange->Release();
                }
                return S_OK;
            });

            HRESULT hrSession = S_OK;
            pContext->RequestEditSession(service_->GetClientId(), startSession, TF_ES_READWRITE | TF_ES_SYNC, &hrSession);
            startSession->Release();
        }
        pContextComp->Release();
    }

    // Update the composition range with the top Bengali candidate
    if (is_composing_ && active_composition_) {
        ITfEditSession* setTextSession = new ActionEditSession(pContext, [this](TfEditCookie ec) -> HRESULT {
            ITfRange* pRange = nullptr;
            if (SUCCEEDED(this->active_composition_->GetRange(&pRange)) && pRange) {
                pRange->SetText(ec, 0, this->current_bengali_top_.c_str(), (LONG)this->current_bengali_top_.length());
                pRange->Release();
            }
            return S_OK;
        });

        HRESULT hrSession = S_OK;
        pContext->RequestEditSession(service_->GetClientId(), setTextSession, TF_ES_READWRITE | TF_ES_SYNC, &hrSession);
        setTextSession->Release();
    }

    // 3. Update Floating Suggestion Window
    RECT caret_rect = GetCaretRect(pContext);
    candidate_window_.ShowCandidates(current_candidates_w_, selected_candidate_idx_, caret_rect);
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
    return CommitCurrentComposition(pContext, selected_candidate_idx_);
}

bool CompositionManager::OnEnter(ITfContext* pContext) {
    if (!is_composing_) {
        return false;
    }
    return CommitCurrentComposition(pContext, selected_candidate_idx_);
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

bool CompositionManager::OnNumberSelection(ITfContext* pContext, int num_1_to_5) {
    size_t idx = static_cast<size_t>(num_1_to_5 - 1);
    if (!is_composing_ || idx >= current_candidates_w_.size()) {
        return false;
    }
    return CommitCurrentComposition(pContext, idx);
}

bool CompositionManager::OnDigit(ITfContext* pContext, char ascii_digit) {
    if (ascii_digit < '0' || ascii_digit > '9') {
        return false;
    }

    // Candidate selection: If composing and candidate window has selectable candidates and digit is 1..5
    if (is_composing_ && (ascii_digit >= '1' && ascii_digit <= '5')) {
        size_t idx = static_cast<size_t>(ascii_digit - '1');
        if (idx < current_candidates_w_.size()) {
            return OnNumberSelection(pContext, ascii_digit - '0');
        }
    }

    // If composing (e.g. typing 'ami' and then pressed '0'..'9' without selecting a candidate),
    // commit current composition first
    if (is_composing_) {
        CommitCurrentComposition(pContext, selected_candidate_idx_);
    }

    // Convert ASCII digit (0-9) to exact Unicode Bengali digit (০-৯, U+09E6 to U+09EF)
    wchar_t b_digit = static_cast<wchar_t>(0x09E6 + (ascii_digit - '0'));
    std::wstring digit_str(1, b_digit);

    if (pContext && service_) {
        ITfEditSession* digitSession = new ActionEditSession(pContext, [this, digit_str, pContext](TfEditCookie ec) -> HRESULT {
            ITfInsertAtSelection* pInsertAtSelection = nullptr;
            if (SUCCEEDED(pContext->QueryInterface(IID_ITfInsertAtSelection, (void**)&pInsertAtSelection))) {
                ITfRange* pRange = nullptr;
                pInsertAtSelection->InsertTextAtSelection(ec, 0, digit_str.c_str(), (LONG)digit_str.length(), &pRange);
                if (pRange) pRange->Release();
                pInsertAtSelection->Release();
            }
            return S_OK;
        });

        HRESULT hr = S_OK;
        pContext->RequestEditSession(service_->GetClientId(), digitSession, TF_ES_READWRITE | TF_ES_SYNC, &hr);
        digitSession->Release();
    }

    return true;
}

bool CompositionManager::OnPunctuation(ITfContext* pContext, char punct) {
    if (is_composing_) {
        CommitCurrentComposition(pContext, selected_candidate_idx_);
    }

    // Convert standard '.' to Bengali Dāri '।'
    std::wstring punct_str = (punct == '.') ? L"।" : std::wstring(1, (wchar_t)punct);

    if (pContext && service_) {
        ITfEditSession* punctSession = new ActionEditSession(pContext, [this, punct_str, pContext](TfEditCookie ec) -> HRESULT {
            ITfInsertAtSelection* pInsertAtSelection = nullptr;
            if (SUCCEEDED(pContext->QueryInterface(IID_ITfInsertAtSelection, (void**)&pInsertAtSelection))) {
                ITfRange* pRange = nullptr;
                pInsertAtSelection->InsertTextAtSelection(ec, 0, punct_str.c_str(), (LONG)punct_str.length(), &pRange);
                if (pRange) pRange->Release();
                pInsertAtSelection->Release();
            }
            return S_OK;
        });

        HRESULT hr = S_OK;
        pContext->RequestEditSession(service_->GetClientId(), punctSession, TF_ES_READWRITE | TF_ES_SYNC, &hr);
        punctSession->Release();
    }

    return true;
}

bool CompositionManager::CommitCurrentComposition(ITfContext* pContext, size_t candidate_idx) {
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

    if (pContext && service_) {
        // Commit via TSF Edit Session
        ITfEditSession* commitSession = new ActionEditSession(pContext, [this, chosen_w](TfEditCookie ec) -> HRESULT {
            if (this->active_composition_) {
                ITfRange* pRange = nullptr;
                if (SUCCEEDED(this->active_composition_->GetRange(&pRange)) && pRange) {
                    pRange->SetText(ec, 0, chosen_w.c_str(), (LONG)chosen_w.length());
                    // Move insertion point to end of committed word
                    pRange->Collapse(ec, TF_ANCHOR_END);
                    pRange->Release();
                }
                this->active_composition_->EndComposition(ec);
                this->active_composition_->Release();
                this->active_composition_ = nullptr;
            }
            return S_OK;
        });

        HRESULT hr = S_OK;
        pContext->RequestEditSession(service_->GetClientId(), commitSession, TF_ES_READWRITE | TF_ES_SYNC, &hr);
        commitSession->Release();
    }

    // Notify language engine of committed word for N-gram context
    if (engine_) {
        BanglaEngine_CommitWord(engine_, chosen_u8.c_str());
    }

    // Reset local state
    is_composing_ = false;
    roman_buffer_.clear();
    current_candidates_w_.clear();
    current_bengali_top_.clear();
    selected_candidate_idx_ = 0;
    candidate_window_.Hide();

    return true;
}

bool CompositionManager::CancelComposition(ITfContext* pContext) {
    if (!is_composing_) {
        roman_buffer_.clear();
        candidate_window_.Hide();
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
