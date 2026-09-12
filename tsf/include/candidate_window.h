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

    void SetSelectionCallback(SelectionCallback cb) { selection_callback_ = cb; }
    size_t GetSelectedIndex() const { return selected_index_; }
    void SelectNext();
    void SelectPrev();

    // Cloud suggestions are produced on a worker thread; PostCloudResult
    // marshals them onto this window's (UI) thread through a private window
    // message so the caller can update the dropdown without owning the thread.
    using CloudResultCallback = std::function<void(const std::wstring& word,
                                                   size_t generation,
                                                   const std::vector<std::wstring>& candidates)>;
    void SetCloudResultCallback(CloudResultCallback cb) { cloud_cb_ = std::move(cb); }
    void PostCloudResult(const std::wstring& word, size_t generation,
                         const std::vector<std::wstring>& candidates);

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

    HFONT hfont_bengali_;
    HFONT hfont_number_;
    HFONT hfont_score_;

    int width_;
    int height_;
};

} // namespace bangla_tsf

#endif // BANGLA_CANDIDATE_WINDOW_H
