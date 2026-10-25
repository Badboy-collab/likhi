#ifndef BANGLA_CANDIDATE_WINDOW_H
#define BANGLA_CANDIDATE_WINDOW_H

#include <windows.h>
#include <string>
#include <vector>
#include <functional>
#include <utility>

namespace bangla_tsf {

class CandidateWindow {
public:
    using SelectionCallback = std::function<void(size_t index)>;

    CandidateWindow();
    ~CandidateWindow();

    bool Initialize(HINSTANCE hInst);
    void Destroy();

    void ShowCandidates(const std::vector<std::wstring>& candidates, size_t selected_index, const RECT& caret_rect);
    void Hide();
    bool IsVisible() const { return is_visible_; }
    HWND GetHwnd() const { return hwnd_; }

    void SetSelectionCallback(SelectionCallback cb) { selection_callback_ = cb; }
    size_t GetSelectedIndex() const { return selected_index_; }
    void SelectNext();
    void SelectPrev();

    void SetPredictionMode(bool is_pred) { is_prediction_mode_ = is_pred; }
    bool IsPredictionMode() const { return is_prediction_mode_; }

    // ---- microphone row (voice input) ----------------------------------
    // Off       : no microphone row at all
    // Ready     : idle microphone button (clickable, same as Ctrl+Alt+V)
    // Recording : red microphone + listening label
    // Busy      : recognition in flight
    enum class VoiceState { Off = 0, Ready = 1, Recording = 2, Busy = 3 };
    using VoiceToggleCallback = std::function<void()>;
    using VoiceResultCallback = std::function<void(const std::wstring& text,
                                                   const std::wstring& error)>;

    void SetVoiceState(VoiceState state, const std::wstring& label = std::wstring());
    VoiceState GetVoiceState() const { return voice_state_; }
    void SetVoiceToggleCallback(VoiceToggleCallback cb) { voice_toggle_cb_ = std::move(cb); }
    void SetVoiceResultCallback(VoiceResultCallback cb) { voice_result_cb_ = std::move(cb); }
    // Marshals a recognition result from the worker thread onto the UI thread.
    void PostVoiceResult(const std::wstring& text, const std::wstring& error);
    // (Re)shows the strip anchored at `anchor_rect`. Works with an empty
    // candidate list, so the microphone row is reachable while not composing.
    void ShowVoiceStatus(const RECT& anchor_rect);

    // Cloud suggestions are produced on a worker thread; PostCloudResult
    // marshals them onto this window's (UI) thread through a private window
    // message so the caller can update the dropdown without owning the thread.
    using CloudResultCallback = std::function<void(const std::wstring& word,
                                                   size_t generation,
                                                   const std::vector<std::wstring>& candidates)>;
    void SetCloudResultCallback(CloudResultCallback cb) { cloud_cb_ = std::move(cb); }
    void PostCloudResult(const std::wstring& word, size_t generation,
                         const std::vector<std::wstring>& candidates);

    // Translation result marshaling
    using TranslationResultCallback = std::function<void(const std::wstring& src,
                                                          const std::wstring& trans,
                                                          size_t generation)>;
    void SetTranslationResultCallback(TranslationResultCallback cb) { trans_cb_ = std::move(cb); }
    void PostTranslationResult(const std::wstring& src, const std::wstring& trans, size_t generation);

    // ---- Language Mode Selector (Gboard style: [ বাংলা ] ↔ [ English ]) ----
    enum class ConversionTarget {
        kBangla = 0,
        kEnglish = 1
    };
    using ConversionModeCallback = std::function<void(ConversionTarget mode)>;

    void SetConversionMode(ConversionTarget mode);
    ConversionTarget GetConversionMode() const { return conversion_mode_; }
    void SetConversionModeCallback(ConversionModeCallback cb) { conversion_mode_cb_ = std::move(cb); }
    void ToggleConversionMode();

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    void OnPaint(HWND hWnd);
    void OnLButtonDown(HWND hWnd, int x, int y);
    void OnMouseMove(HWND hWnd, int x, int y);
    void UpdateDimensions();

    HWND hwnd_;
    HINSTANCE hinst_;
    bool is_visible_;
    std::vector<std::wstring> candidates_;
    std::vector<RECT> candidate_item_rects_;
    size_t selected_index_;
    RECT caret_rect_;
    SelectionCallback selection_callback_;
    CloudResultCallback cloud_cb_;
    TranslationResultCallback trans_cb_;

    HFONT hfont_bengali_;
    HFONT hfont_number_;
    HFONT hfont_score_;
    HFONT hfont_mode_;

    int width_;
    int height_;
    int last_width_;
    int last_height_;

    ConversionTarget conversion_mode_;
    ConversionModeCallback conversion_mode_cb_;
    RECT mode_bar_rect_;
    RECT bn_btn_rect_;
    RECT en_btn_rect_;
    bool bn_hovered_;
    bool en_hovered_;
    bool is_prediction_mode_;

    // Voice (microphone) state. Declared last so the constructor's init list
    // order matches the declaration order.
    std::wstring VoiceLabel() const;
    VoiceState voice_state_;
    std::wstring voice_label_;
    RECT mic_rect_;
    VoiceToggleCallback voice_toggle_cb_;
    VoiceResultCallback voice_result_cb_;
};

} // namespace bangla_tsf

#endif // BANGLA_CANDIDATE_WINDOW_H
