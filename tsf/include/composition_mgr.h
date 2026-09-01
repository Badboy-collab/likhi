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
    bool OnNumberSelection(ITfContext* pContext, int num_1_to_5);
    bool OnPunctuation(ITfContext* pContext, char punct);

    void OnCompositionTerminated(ITfContext* pContext, ITfComposition* pComposition);
    void OnFocusLost(ITfContext* pContext);

    // Commit methods
    bool CommitCurrentComposition(ITfContext* pContext, size_t candidate_idx = 0);
    bool CancelComposition(ITfContext* pContext);

    // Direct Candidate Window selection callback
    void OnCandidateWindowSelection(size_t index);

    // Engine settings config
    void SetAutoCorrectEnabled(bool enabled);
    void SetMaxCandidates(uint32_t max_cands);

    // Query candidates
    const std::vector<std::wstring>& GetCurrentCandidates() const { return current_candidates_w_; }

private:
    void UpdateCompositionAndUI(ITfContext* pContext);
    RECT GetCaretRect(ITfContext* pContext);

    TextService* service_;
    BanglaEngine* engine_;
    CandidateWindow candidate_window_;

    bool is_composing_;
    std::string roman_buffer_;
    std::wstring current_bengali_top_;
    std::vector<std::wstring> current_candidates_w_;
    size_t selected_candidate_idx_;

    ITfComposition* active_composition_;
    ITfContext* current_context_;
};

} // namespace bangla_tsf

#endif // BANGLA_COMPOSITION_MGR_H
