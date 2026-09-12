#include "../include/composition_mgr.h"
#include "../include/text_service.h"
#include "../include/tsf_utils.h"
#include "../include/edit_session.h"
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
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

// Personal learning file lives next to the lexicon in %APPDATA%: user_dict.txt
// (roman<TAB>bengali<TAB>count<TAB>timestamp, tab-separated TSV). One file per
// Windows user — that is the machine-local "memory" of what the user picks.
static std::string FindUserDictPath() {
    wchar_t appdata[MAX_PATH] = {0};
    std::wstring dir;
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appdata))) {
        dir = std::wstring(appdata) + L"\\PC-Bangla-Typing-App";
    } else {
        dir = L"PC-Bangla-Typing-App";
    }
    CreateDirectoryW(dir.c_str(), NULL); // fine if it already exists
    return bangla_tsf::Utf16ToUtf8(dir + L"\\user_dict.txt");
}

// True when the UTF-8 string contains at least one Bengali code point
// (U+0980..U+09FF). Pure-ASCII commits (the user chose the English fallback
// candidate) must NOT be learned as a "Bengali spelling".
static bool ContainsBengali(const std::string& u8) {
    for (size_t i = 0; i + 2 < u8.size(); ++i) {
        unsigned char b1 = static_cast<unsigned char>(u8[i + 1]);
        if (static_cast<unsigned char>(u8[i]) == 0xE0 && b1 >= 0xA6 && b1 <= 0xA7) {
            return true;
        }
    }
    return false;
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
    // 1. Engine: lexicon + per-user personal learning file.
    lexicon_path_ = FindLexiconPath(hInst);
    user_dict_path_ = FindUserDictPath();
    auto_correct_enabled_ = false;
    max_candidates_ = 5;

    EngineConfig cfg_lex = BuildConfig(true);
    engine_ = BanglaEngine_Create(&cfg_lex);
    if (!engine_) {
        EngineConfig cfg_nolex = BuildConfig(false); // no lexicon fallback
        engine_ = BanglaEngine_Create(&cfg_nolex);
    }
    if (engine_) BanglaEngine_SetLearningEnabled(engine_, personal_learning_enabled_);

    // 2. Initialize Candidate Window UI (if GUI instance provided)
    if (hInst) {
        candidate_window_.Initialize(hInst);
        candidate_window_.SetSelectionCallback([this](size_t index) {
            OnCandidateWindowSelection(index);
        });
        // Cloud replies arrive on the CloudTranslit worker thread; marshal them
        // onto the UI thread via the candidate window before touching state.
        cloud_.SetResultCallback([this](const std::wstring& word, uint32_t generation,
                                        const std::vector<std::wstring>& candidates) {
            candidate_window_.PostCloudResult(word, generation, candidates);
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
    DropCloudState();
    cloud_.Stop();
    candidate_window_.Destroy();
}

void CompositionManager::SetAutoCorrectEnabled(bool enabled) {
    if (enabled == auto_correct_enabled_) return;
    auto_correct_enabled_ = enabled;
    RebuildEngine();
}

void CompositionManager::SetMaxCandidates(uint32_t max_cands) {
    if (max_cands == max_candidates_) return;
    max_candidates_ = max_cands;
    RebuildEngine();
}

void CompositionManager::SetPersonalLearningEnabled(bool enabled) {
    personal_learning_enabled_ = enabled;
    if (engine_) BanglaEngine_SetLearningEnabled(engine_, enabled);
}

size_t CompositionManager::ClearLearnedData() {
    return engine_ ? BanglaEngine_ClearLearnedData(engine_) : 0;
}

size_t CompositionManager::ClearExplicitUserWords() {
    return engine_ ? BanglaEngine_ClearUserWords(engine_) : 0;
}

EngineConfig CompositionManager::BuildConfig(bool with_lexicon) {
    EngineConfig cfg;
    BanglaEngine_GetDefaultConfig(&cfg);
    cfg.auto_correct_enabled = auto_correct_enabled_;
    cfg.max_candidates = max_candidates_;
    if (with_lexicon && !lexicon_path_.empty()) cfg.lexicon_binary_path = lexicon_path_.c_str();
    if (!user_dict_path_.empty()) cfg.user_dict_path = user_dict_path_.c_str();
    return cfg;
}

std::string CompositionManager::LowerRomanKey() const {
    std::string key;
    key.reserve(roman_buffer_.size());
    for (char c : roman_buffer_) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        key.push_back(c);
    }
    return key;
}

void CompositionManager::RebuildEngine() {
    EngineConfig cfg_lex = BuildConfig(true);
    BanglaEngine* fresh = BanglaEngine_Create(&cfg_lex);
    if (!fresh) {
        EngineConfig cfg_nolex = BuildConfig(false);
        fresh = BanglaEngine_Create(&cfg_nolex);
    }
    if (!fresh) return; // keep the old instance rather than lose typing
    BanglaEngine_SetLearningEnabled(fresh, personal_learning_enabled_);
    if (engine_) BanglaEngine_Destroy(engine_);
    engine_ = fresh;
}

void CompositionManager::SetCloudTranslitEnabled(bool enabled) {
    if (enabled == cloud_enabled_) return;
    cloud_enabled_ = enabled;
    if (enabled) {
        cloud_.Start();
    } else {
        DropCloudState();
        cloud_.Stop();
    }
}

void CompositionManager::RequestCloudLookup() {
    if (!cloud_enabled_ || cloud_applying_ || !is_composing_ || roman_buffer_.empty()) {
        return;
    }
    // roman_buffer_ is pure ASCII (lowercased letters); widen in place.
    std::wstring word(roman_buffer_.begin(), roman_buffer_.end());
    ++cloud_gen_;
    cloud_.Request(word, cloud_gen_);
}

void CompositionManager::DropCloudState() {
    cloud_.Cancel();
    ++cloud_gen_; // any in-flight reply now fails the generation check
}

void CompositionManager::OnCloudResults(const std::wstring& word, uint32_t generation,
                                        const std::vector<std::wstring>& candidates) {
    // Runs on the UI thread (marshalled through the candidate window).
    if (!cloud_enabled_ || cloud_applying_) return;
    if (!is_composing_ || generation != cloud_gen_) return; // stale result
    std::wstring current(roman_buffer_.begin(), roman_buffer_.end());
    if (current != word) return;                            // user moved on
    if (candidates.empty()) return;

    // Google list (max 5) + the exact typed English word appended as the final
    // selectable candidate, so the roman spelling is always recoverable.
    std::vector<std::wstring> merged = candidates;
    if (merged.size() > 5) merged.resize(5);
    bool has_english = false;
    for (const auto& c : merged) {
        if (c == word) { has_english = true; break; }
    }
    if (!has_english) merged.push_back(word);

    // Personal habit beats Google's default order (Gboard-style personalization)
    // — but ONLY with enough evidence: a spelling committed just once must not
    // reorder the cloud list (one accidental pick is not a preference).
    if (engine_ && personal_learning_enabled_ && !word.empty()) {
        std::string roman_key;
        roman_key.reserve(word.size());
        for (wchar_t wc : word) {
            if (wc >= L'A' && wc <= L'Z') wc = static_cast<wchar_t>(wc - L'A' + L'a');
            if (wc <= 0x7F) roman_key.push_back(static_cast<char>(wc));
        }
        UserWordRef learned[16];
        int n = BanglaEngine_GetUserWords(engine_, roman_key.c_str(), learned, 16);
        if (n > 0 && learned[0].frequency >= 2) {
            std::wstring pref = Utf8ToUtf16(learned[0].bengali_text);
            auto it = std::find(merged.begin(), merged.end(), pref);
            if (it != merged.end() && it != merged.begin()) {
                std::rotate(merged.begin(), it, it + 1);
            }
        }
    }

    if (merged == current_candidates_w_) return; // identical → no churn

    cloud_applying_ = true;
    current_candidates_w_ = merged;
    current_bengali_top_ = merged[0];
    selected_candidate_idx_ = 0;

    // Push the Google top candidate inline so what the user sees (composition
    // text) matches what Space would commit. Read-write session with a live
    // cookie — never touches the cloud path again (no key event involved).
    if (current_context_ && active_composition_) {
        ITfEditSession* textSession = new ActionEditSession(current_context_,
            [this](TfEditCookie ec) -> HRESULT {
                ITfRange* pRange = nullptr;
                if (this->active_composition_ &&
                    SUCCEEDED(this->active_composition_->GetRange(&pRange)) && pRange) {
                    pRange->SetText(ec, 0, this->current_bengali_top_.c_str(),
                                    (LONG)this->current_bengali_top_.length());
                    pRange->Release();
                }
                return S_OK;
            });
        HRESULT hrSession = S_OK;
        HRESULT hr = current_context_->RequestEditSession(
            service_->GetClientId(), textSession, TF_ES_READWRITE | TF_ES_SYNC, &hrSession);
        if (hr == TF_E_SYNCHRONOUS) {
            current_context_->RequestEditSession(
                service_->GetClientId(), textSession, TF_ES_READWRITE, &hrSession);
        }
        textSession->Release();
    }

    if (caret_rect_valid_) {
        candidate_window_.ShowCandidates(current_candidates_w_, selected_candidate_idx_,
                                         caret_rect_);
    }
    cloud_applying_ = false;
}

bool CompositionManager::ReadTextExtInSession(TfEditCookie ec, ITfContext* pContext, RECT& out) {
    if (!pContext) return false;

    ITfContextView* pView = nullptr;
    if (FAILED(pContext->GetActiveView(&pView)) || !pView) return false;

    bool ok = false;
    ITfRange* pRange = nullptr;
    if (active_composition_ && SUCCEEDED(active_composition_->GetRange(&pRange)) && pRange) {
        // Anchor at the live typing caret = the collapsed END of the composition
        // (Google/Gboard put the suggestion right under the caret).
        ITfRange* pCaret = nullptr;
        if (SUCCEEDED(pRange->Clone(&pCaret)) && pCaret) {
            if (SUCCEEDED(pCaret->Collapse(ec, TF_ANCHOR_END))) {
                BOOL fClipped = FALSE;
                RECT rc = {};
                if (SUCCEEDED(pView->GetTextExt(ec, pCaret, &rc, &fClipped)) &&
                    (rc.right != rc.left || rc.bottom != rc.top)) {
                    out = rc;
                    ok = true;
                }
            }
            pCaret->Release();
        }
        // Fallback: full composition extent.
        if (!ok) {
            BOOL fClipped = FALSE;
            RECT rc = {};
            if (SUCCEEDED(pView->GetTextExt(ec, pRange, &rc, &fClipped)) &&
                (rc.right != rc.left || rc.bottom != rc.top)) {
                out = rc;
                ok = true;
            }
        }
        pRange->Release();
    } else {
        // No composition yet — anchor at the insertion caret.
        ITfInsertAtSelection* pIns = nullptr;
        if (SUCCEEDED(pContext->QueryInterface(IID_ITfInsertAtSelection, (void**)&pIns)) && pIns) {
            ITfRange* pInsRange = nullptr;
            if (SUCCEEDED(pIns->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, NULL, 0, &pInsRange)) && pInsRange) {
                BOOL fClipped = FALSE;
                RECT rc = {};
                if (SUCCEEDED(pView->GetTextExt(ec, pInsRange, &rc, &fClipped)) &&
                    (rc.right != rc.left || rc.bottom != rc.top)) {
                    out = rc;
                    ok = true;
                }
                pInsRange->Release();
            }
            pIns->Release();
        }
    }
    pView->Release();
    return ok;
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

    // Refresh the popup content immediately at the last known anchor (async-only
    // hosts like Chromium/Electron may not run the edit session until the next
    // message-loop tick, so this keeps the suggestion list feeling instant).
    if (caret_rect_valid_ && !current_candidates_w_.empty()) {
        candidate_window_.ShowCandidates(current_candidates_w_, selected_candidate_idx_, caret_rect_);
    } else if (current_candidates_w_.empty()) {
        candidate_window_.Hide();
    }

    // ONE read-write edit session: (re)start the composition if needed, push the
    // current top candidate as the inline composition text, then resolve the caret
    // anchor WITH THE LIVE EDIT COOKIE and re-anchor the popup. Outside an active
    // edit session ITfContextView::GetTextExt returns TF_E_NOLAYOUT in most hosts
    // (notably Chromium/Electron), which previously left the popup parked at the
    // top-left corner of the screen instead of following the caret.
    ITfEditSession* updateSession = new ActionEditSession(pContext, [this, pContext](TfEditCookie ec) -> HRESULT {
        if (!this->active_composition_) {
            ITfContextComposition* pCtxComp = nullptr;
            if (SUCCEEDED(pContext->QueryInterface(IID_ITfContextComposition, (void**)&pCtxComp)) && pCtxComp) {
                ITfInsertAtSelection* pIns = nullptr;
                if (SUCCEEDED(pContext->QueryInterface(IID_ITfInsertAtSelection, (void**)&pIns)) && pIns) {
                    ITfRange* pStartRange = nullptr;
                    if (SUCCEEDED(pIns->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, NULL, 0, &pStartRange)) && pStartRange) {
                        ITfComposition* pComp = nullptr;
                        if (SUCCEEDED(pCtxComp->StartComposition(ec, pStartRange, this->service_, &pComp)) && pComp) {
                            this->active_composition_ = pComp;
                            this->is_composing_ = true;
                        }
                        pStartRange->Release();
                    }
                    pIns->Release();
                }
                pCtxComp->Release();
            }
        }

        if (this->active_composition_) {
            ITfRange* pRange = nullptr;
            if (SUCCEEDED(this->active_composition_->GetRange(&pRange)) && pRange) {
                pRange->SetText(ec, 0, this->current_bengali_top_.c_str(),
                                (LONG)this->current_bengali_top_.length());
                pRange->Release();
            }
        }

        RECT anchor = {};
        if (this->ReadTextExtInSession(ec, pContext, anchor)) {
            bool moved = !this->caret_rect_valid_ ||
                         anchor.left   != this->caret_rect_.left ||
                         anchor.top    != this->caret_rect_.top ||
                         anchor.right  != this->caret_rect_.right ||
                         anchor.bottom != this->caret_rect_.bottom;
            this->caret_rect_ = anchor;
            this->caret_rect_valid_ = true;
            if (moved && !this->current_candidates_w_.empty()) {
                this->candidate_window_.ShowCandidates(this->current_candidates_w_,
                                                       this->selected_candidate_idx_,
                                                       this->caret_rect_);
            }
        }
        return S_OK;
    });

    HRESULT hrSession = S_OK;
    HRESULT hr = pContext->RequestEditSession(service_->GetClientId(), updateSession,
                                              TF_ES_READWRITE | TF_ES_SYNC, &hrSession);
    if (hr == TF_E_SYNCHRONOUS) {
        pContext->RequestEditSession(service_->GetClientId(), updateSession,
                                     TF_ES_READWRITE, &hrSession);
    }
    updateSession->Release();

    // Schedule the online lookup for the current word (debounced internally).
    RequestCloudLookup();
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
    // OnTestKeyDown returned TRUE for Space when composing, so the host will
    // NOT insert a space. Commit "word + one U+0020" as a SINGLE SetText inside
    // the composition range (see CommitCurrentComposition). Exactly one space.
    return CommitCurrentComposition(pContext, selected_candidate_idx_, L" ");
}

bool CompositionManager::OnEnter(ITfContext* pContext) {
    if (!is_composing_) {
        return false;
    }
    return CommitCurrentComposition(pContext, selected_candidate_idx_, L"");
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
    if (num_1_to_9 < 1 || num_1_to_9 > 9) return false;
    size_t idx = static_cast<size_t>(num_1_to_9 - 1);
    if (!is_composing_ || idx >= current_candidates_w_.size()) {
        return false;
    }
    return CommitCurrentComposition(pContext, idx, L"");
}

bool CompositionManager::CanSelectCandidate(char digit) {
    if (!is_composing_ || current_candidates_w_.empty()) return false;
    // Any visible candidate is selectable by its shown number: the strip labels
    // items 1..N (N up to 9). Older builds capped this at 5, so the 6th entry
    // (the exact-English cloud fallback) could not be picked by keyboard and
    // the digit fell through as a plain number.
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
    // Only reached while composing (policy step 15 eats '.' only then).
    // Commit the word with the Bengali Dāri (।) attached as the trailing
    // text of the SAME composition-range SetText — never via a separate
    // post-EndComposition insert (which could delete the word in some hosts).
    if (!is_composing_) {
        return false;
    }
    std::wstring trailing = (punct == '.') ? L"।" : std::wstring(1, (wchar_t)punct);
    return CommitCurrentComposition(pContext, selected_candidate_idx_, trailing);
}

bool CompositionManager::OnBengaliDigit(ITfContext* pContext, wchar_t bengali_digit) {
    // Numpad digit (NumLock ON) -> insert the Bengali numeral ০-৯. This runs
    // in OnKeyDown for an EATEN key, so the host never produced an ASCII digit.
    // Any active composition was already committed in OnTestKeyDown, so we are
    // inserting at a plain caret (no composition involved) — safe to use the
    // current app selection here.
    if (!pContext || !service_ || bengali_digit == L'\0') {
        return false;
    }
    std::wstring digit_str(1, bengali_digit);
    ITfEditSession* digitSession = new ActionEditSession(pContext,
        [this, digit_str, pContext](TfEditCookie ec) -> HRESULT {
            ITfInsertAtSelection* pInsertAtSelection = nullptr;
            if (SUCCEEDED(pContext->QueryInterface(IID_ITfInsertAtSelection, (void**)&pInsertAtSelection))) {
                ITfRange* pRange = nullptr;
                pInsertAtSelection->InsertTextAtSelection(ec, 0, digit_str.c_str(),
                                                          (LONG)digit_str.length(), &pRange);
                if (pRange) pRange->Release();
                pInsertAtSelection->Release();
            }
            return S_OK;
        });

    HRESULT hr = S_OK;
    HRESULT hrSession = S_OK;
    hr = pContext->RequestEditSession(service_->GetClientId(), digitSession,
                                      TF_ES_READWRITE | TF_ES_SYNC, &hrSession);
    if (hr == TF_E_SYNCHRONOUS) {
        pContext->RequestEditSession(service_->GetClientId(), digitSession,
                                     TF_ES_READWRITE, &hrSession);
    }
    digitSession->Release();
    return true;
}

bool CompositionManager::CommitCurrentComposition(ITfContext* pContext, size_t candidate_idx,
                                                  const std::wstring& trailing) {
    if (!is_composing_) {
        return false;
    }

    // Drop any pending/in-flight cloud lookup for this word.
    DropCloudState();

    std::wstring chosen_w;
    if (candidate_idx < current_candidates_w_.size()) {
        chosen_w = current_candidates_w_[candidate_idx];
    } else if (!current_bengali_top_.empty()) {
        chosen_w = current_bengali_top_;
    } else {
        chosen_w = Utf8ToUtf16(roman_buffer_);
    }

    std::string chosen_u8 = Utf16ToUtf8(chosen_w);
    // Final text committed in ONE SetText: word [+ trailing] (see header note).
    std::wstring commit_text = chosen_w + trailing;

    if (pContext && service_) {
        ITfEditSession* commitSession = new ActionEditSession(pContext,
            [this, commit_text](TfEditCookie ec) -> HRESULT {

            if (this->active_composition_) {
                ITfRange* pRange = nullptr;
                if (SUCCEEDED(this->active_composition_->GetRange(&pRange)) && pRange) {
                    pRange->SetText(ec, 0, commit_text.c_str(), (LONG)commit_text.length());
                    pRange->Release();
                }
                this->active_composition_->EndComposition(ec);
                this->active_composition_->Release();
                this->active_composition_ = nullptr;
            }
            return S_OK;
        });

        HRESULT hr = S_OK;
        HRESULT hrSession = S_OK;
        hr = pContext->RequestEditSession(service_->GetClientId(), commitSession,
                                          TF_ES_READWRITE | TF_ES_SYNC, &hrSession);
        if (hr == TF_E_SYNCHRONOUS) {
            pContext->RequestEditSession(service_->GetClientId(), commitSession,
                                          TF_ES_READWRITE, &hrSession);
        }
        commitSession->Release();
    }

    // Personal learning: remember (roman → chosen Bengali) with a usage count so
    // the app "knows" the user's favourite spelling and shows it first next time
    // — online (reorders Google's list, see OnCloudResults) and offline (engine
    // ranks it top via the personal-dictionary boost). Pure-English commits are
    // not learned. Also feeds the N-gram context history.
    if (engine_ && personal_learning_enabled_ && ContainsBengali(chosen_u8)) {
        BanglaEngine_LearnWord(engine_, LowerRomanKey().c_str(), chosen_u8.c_str());
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
        DropCloudState();
        candidate_window_.Hide();
        return false;
    }

    DropCloudState();

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
    DropCloudState();
    candidate_window_.Hide();
}

void CompositionManager::OnFocusLost(ITfContext* pContext) {
    if (is_composing_) {
        DropCloudState();
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


