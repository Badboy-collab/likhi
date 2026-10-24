#pragma once

#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>

namespace likhi {
namespace universal {

enum class OskTab {
    kEnglish = 0,
    kBangla = 1,
    kCharacters = 2,
    kEmoji = 3
};

struct OskKey {
    RECT rc;
    std::wstring label;
    std::wstring shift_label;
    std::wstring type_text;
    WORD vk_code; // If 0, uses type_text
    bool is_special;
    bool is_wide;
};

class VirtualKeyboardWindow {
public:
    VirtualKeyboardWindow();
    ~VirtualKeyboardWindow();

    bool Initialize(HINSTANCE hInstance, HWND parentHwnd);
    void Show(bool show);
    void ToggleVisibility();
    bool IsVisible() const;
    void Destroy();

    void SetTab(OskTab tab);
    OskTab GetTab() const { return current_tab_; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void OnPaint(HWND hwnd);
    void OnLButtonDown(HWND hwnd, int x, int y);
    void OnLButtonUp(HWND hwnd, int x, int y);
    void OnMouseMove(HWND hwnd, int x, int y);
    void OnMouseLeave(HWND hwnd);

    void InitLayouts();
    void BuildEnglishLayout();
    void BuildBanglaLayout();
    void BuildCharactersLayout();
    void BuildEmojiLayout();

    void PressKey(const OskKey& key);
    void TypeText(const std::wstring& text);
    void SendVirtualKey(WORD vk);

    HINSTANCE hinst_;
    HWND hwnd_;
    HWND hwnd_parent_;
    bool is_visible_;

    int width_;
    int height_;
    int pos_x_;
    int pos_y_;

    OskTab current_tab_;
    bool is_shift_active_;
    bool is_caps_active_;

    // Tab buttons header rects
    RECT rc_header_;
    RECT rc_tab_english_;
    RECT rc_tab_bangla_;
    RECT rc_tab_chars_;
    RECT rc_tab_emoji_;

    RECT rc_btn_min_;
    RECT rc_btn_max_;
    RECT rc_btn_close_;

    // Search bar for emoji tab
    RECT rc_search_bar_;
    RECT rc_search_btn_;
    std::wstring search_query_;

    // Keys collections for each tab
    std::vector<OskKey> keys_english_;
    std::vector<OskKey> keys_bangla_;
    std::vector<OskKey> keys_characters_;
    std::vector<OskKey> keys_emoji_;

    // Hover & pressed tracking
    int hovered_key_index_;
    int pressed_key_index_;
    int hovered_tab_;

    // GDI resources
    HFONT font_tab_;
    HFONT font_key_en_;
    HFONT font_key_bn_;
    HFONT font_key_small_;
    HFONT font_emoji_;
};

} // namespace universal
} // namespace likhi
