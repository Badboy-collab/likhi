#ifndef BANGLA_COMPOSITION_MGR_H
#define BANGLA_COMPOSITION_MGR_H

#include <windows.h>
#include <msctf.h>
#include <string>
#include <vector>
#include "../../engine/include/bangla_engine.h"
#include "candidate_window.h"
#include "cloud_translit.h"

namespace bangla_tsf {

class TextService;

// Google-Input-Tools-style online suggestions: results stream in on a
// background thread (CloudTranslit) and are merged on the UI thread. Local
// engine output always shows first (0 ms); cloud results refine the dropdown.
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
    bool OnBengaliDigit(ITfContext* pContext, wchar_t bengali_digit);
    bool HasVisibleCandidates() const { return is_composing_ && !current_candidates_w_.empty(); }

    void OnCompositionTerminated(ITfContext* pContext, ITfComposition* pComposition);
    void OnFocusLost(ITfContext* pContext);

    // Commit methods. The whole final text (word [+ trailing suffix such as a
    // space or Dāri]) is written with ONE ITfRange::SetText inside the active
    // composition, which is then ended. We never insert "after the fact" at the
    // current app selection — some hosts keep the selection over the just-ended
    // composition, so a later insert would REPLACE (delete) the committed word.
    bool CommitCurrentComposition(ITfContext* pContext, size_t candidate_idx = 0,
                                  const std::wstring& trailing = L"");
    bool CancelComposition(ITfContext* pContext);

    // Direct Candidate Window selection callback
    void OnCandidateWindowSelection(size_t index);

    // Engine settings config
    void SetAutoCorrectEnabled(bool enabled);
    void SetMaxCandidates(uint32_t max_cands);

    // Personal learning controls (LOCAL-first). Disabling pauses recording of
    // new preferences but keeps using everything already learned; clearing
    // wipes learned data (never the user's explicit words).
    void SetPersonalLearningEnabled(bool enabled);
    bool IsPersonalLearningEnabled() const { return personal_learning_enabled_; }
    size_t ClearLearnedData();
    size_t ClearExplicitUserWords();

    // Cloud transliteration (Google Input Tools online suggestions). Enabled by
    // the real TSF TextService; unit/integration builds keep it OFF so no
    // worker thread or network traffic ever happens under test.
    void SetCloudTranslitEnabled(bool enabled);
    bool IsCloudTranslitEnabled() const { return cloud_enabled_; }

    // Query candidates
    const std::vector<std::wstring>& GetCurrentCandidates() const { return current_candidates_w_; }

private:
    void UpdateCompositionAndUI(ITfContext* pContext);

    // Reads the caret's screen bounding box (preferring the collapsed END of the
    // composition = the live typing caret, falling back to the full composition
    // extent / insertion caret). MUST be called with a live edit cookie (ec):
    // hosts like Chromium/Electron return TF_E_NOLAYOUT when queried outside an
    // active edit session, which is what previously parked the suggestion popup
    // at the top-left corner (default rect 100,100).
    bool ReadTextExtInSession(TfEditCookie ec, ITfContext* pContext, RECT& out);

    // Cloud suggestion plumbing (runs on the UI thread).
    void RequestCloudLookup();
    void DropCloudState();
    void OnCloudResults(const std::wstring& word, uint32_t generation,
                        const std::vector<std::wstring>& candidates);

    // Builds an EngineConfig whose path pointers target THIS manager's stable
    // string members (the engine copies them at Create time). `with_lexicon`
    // false is the no-lexicon fallback for broken installs.
    EngineConfig BuildConfig(bool with_lexicon);
    std::string LowerRomanKey() const;   // lowercased roman_buffer_ (ASCII widen)
    void RebuildEngine();

    TextService* service_;
    BanglaEngine* engine_;
    CandidateWindow candidate_window_;

    bool is_composing_;
    std::string roman_buffer_;
    std::wstring current_bengali_top_;
    std::vector<std::wstring> current_candidates_w_;
    size_t selected_candidate_idx_;

    // Stable storage backing EngineConfig path pointers (never dangle).
    std::string lexicon_path_;
    std::string user_dict_path_;
    bool auto_correct_enabled_ = false;
    uint32_t max_candidates_ = 5;
    bool personal_learning_enabled_ = true;

    // Last successfully resolved anchor for the suggestion popup. Persists across
    // updates so a transient TF_E_NOLAYOUT never makes the window jump elsewhere.
    RECT caret_rect_ = {};
    bool caret_rect_valid_ = false;

    // Online suggestion state (see cloud_translit.h).
    CloudTranslit cloud_;
    bool cloud_enabled_ = false;
    bool cloud_applying_ = false;
    uint32_t cloud_gen_ = 0;

    ITfComposition* active_composition_;
    ITfContext* current_context_;
};

} // namespace bangla_tsf

#endif // BANGLA_COMPOSITION_MGR_H


