#ifndef BANGLA_COMPOSITION_MGR_H
#define BANGLA_COMPOSITION_MGR_H

#include <windows.h>
#include <msctf.h>
#include <string>
#include <vector>
#include "../../engine/include/bangla_engine.h"
#include "candidate_window.h"

namespace bangla_tsf {

class TextService;

class CompositionManager {
public:
    CompositionManager(TextService* service);
    ~CompositionManager();

    bool Initialize(HINSTANCE hInst);
    void Shutdown();

    bool IsComposing() const { return is_composing_; }
    const std::string& GetRomanBuffer() const { return roman_buffer_; }

    // Core Event Handlers from TSF KeyEventSink
    bool OnCharacter(ITfContext* pContext, char ch);
    bool OnBackspace(ITfContext* pContext);
    bool OnSpace(ITfContext* pContext);
    bool OnEnter(ITfContext* pContext);
    bool OnEscape(ITfContext* pContext);
    bool OnArrow(ITfContext* pContext, bool down_next);
    bool OnNumberSelection(ITfContext* pContext, int num_1_to_9);
    bool OnDigit(ITfContext* pContext, char ascii_digit);
    bool CanSelectCandidate(char digit);
    bool OnPunctuation(ITfContext* pContext, char punct);
    bool HasVisibleCandidates() const { return is_composing_ && !current_candidates_w_.empty(); }

    void OnCompositionTerminated(ITfContext* pContext, ITfComposition* pComposition);
    void OnFocusLost(ITfContext* pContext);

    // Commit methods
    bool CommitCurrentComposition(ITfContext* pContext, size_t candidate_idx = 0, bool append_space = false, const std::wstring& append_punct = L"");
    bool CancelComposition(ITfContext* pContext);

    // Direct Candidate Window selection callback
    void OnCandidateWindowSelection(size_t index);

    // Engine settings config
    void SetAutoCorrectEnabled(bool enabled);
    void SetMaxCandidates(uint32_t max_cands);

    // Query candidates
    const std::vector<std::wstring>& GetCurrentCandidates() const { return current_candidates_w_; }
    void SetCandidatesForTesting(const std::vector<std::wstring>& cands) { is_composing_ = true; current_candidates_w_ = cands; }

private:
    void UpdateCompositionAndUI(ITfContext* pContext);
    RECT GetCaretRect(ITfContext* pContext);
    void CheckAndReloadUserDict();

    TextService* service_;
    BanglaEngine* engine_;
    CandidateWindow candidate_window_;

    std::string lexicon_path_;
    std::string user_dict_path_;
    FILETIME last_dict_file_time_;
    std::wstring settings_file_path_;
    FILETIME last_settings_file_time_;

    bool is_composing_;
    bool is_top_exact_match_;
    std::string roman_buffer_;
    std::wstring current_bengali_top_;
    std::vector<std::wstring> current_candidates_w_;
    size_t selected_candidate_idx_;

    ITfComposition* active_composition_;
    ITfContext* current_context_;
    RECT cached_caret_rect_;
    bool has_cached_caret_rect_;
};

} // namespace bangla_tsf

#endif // BANGLA_COMPOSITION_MGR_H


