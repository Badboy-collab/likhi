#pragma once

#include <windows.h>
#include <gdiplus.h>
#include <commctrl.h>
#include <string>
#include <memory>
#include "likhi_mode_ipc.h"
#include "voice_input.h"

namespace likhi {
namespace universal {

class VirtualKeyboardWindow;

class TopBarWindow {
public:
    TopBarWindow();
    ~TopBarWindow();

    bool Initialize(HINSTANCE hInstance, HWND parentHwnd);
    void Show(bool show);
    bool IsVisible() const;
    HWND GetHwnd() const { return hwnd_; }
    void UpdateMode(likhi::ModeTarget mode);
    void Destroy();

    void SetPosition(int x, int y);
    void GetPosition(int& x, int& y) const;
    void SaveCoordinates();

    enum DockPosition {
        kDockTop = 0,
        kDockRight = 1
    };

    enum ButtonId {
        kBtnNone = 0,
        kBtnMic,
        kBtnModeBn,      // 1-Click Bangla [বাং]
        kBtnModeEn,      // 1-Click English [EN]
        kBtnModeTrans,   // 1-Click Banglish -> English Translation [বা⇄En]
        kBtnPause,
        kBtnDict,
        kBtnOsk,
        kBtnScript,
        kBtnSettings,
        kBtnPower
    };

    void SetVirtualKeyboard(VirtualKeyboardWindow* osk) { osk_window_ = osk; }
    void SetAutoHide(bool enable);
    bool IsAutoHide() const { return auto_hide_enabled_; }
    void SetDockPosition(DockPosition pos);
    DockPosition GetDockPosition() const { return dock_pos_; }
    void ToggleCompactMode();
    bool IsCompactMode() const { return is_compact_mode_; }
    void ExpandBar();
    void CollapseBar();
    void ExpandToFull();
    void CollapseToCompact();
    void LaunchVoiceInput();
    void TypeUnicodeString(const std::wstring& text);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void OnPaint(HWND hwnd);
    void OnLButtonDown(HWND hwnd, int x, int y);
    void OnLButtonUp(HWND hwnd, int x, int y);
    void OnMouseMove(HWND hwnd, int x, int y);
    void OnMouseLeave(HWND hwnd);
    void OnTimer(HWND hwnd, UINT_PTR timer_id);
    ButtonId HitTestButton(POINT pt) const;
    void ShowPowerMenu(HWND hwnd);
    void ShowSettingsMenu(HWND hwnd);
    void ShowWordManagerMenu(HWND hwnd);
    void ShowAddWordDialog(HWND hwnd);
    void RestartLikhi();
    void CheckForUpdatesManual(HWND hwnd);
    void PrepareCleanUninstall(HWND hwnd);
    void ShowLayoutMap(HWND hwnd);

    void CheckAutoHide();

    void InitTooltips(HWND hwnd);

    // Actions
    void InsertTodayDate(bool bengali_digits);
    void InsertCurrentTime();
    void TriggerEmojiPicker();
    void LaunchOnScreenKeyboard();
    void LaunchConverter();
    void LaunchSettings(const wchar_t* args = nullptr);
    void LaunchDictionary();
    void ToggleSuggestions();

    HINSTANCE hinst_;
    HWND hwnd_;
    HWND hwnd_parent_;
    HWND hwnd_tooltip_;
    VirtualKeyboardWindow* osk_window_;

    likhi::ModeTarget current_mode_;
    int width_;
    int height_;
    int pos_x_;
    int pos_y_;
    int normal_y_;
    bool is_visible_;

    // Auto-hide and compact states
    DockPosition dock_pos_ = kDockTop;
    bool auto_hide_enabled_;
    bool is_collapsed_;
    bool is_hovered_;
    bool is_compact_mode_;
    bool suggestions_paused_;
    DWORD mouse_leave_time_;

    // Button tracking (Kontho Image 1 Suite: Mic, Mode, Pause, Dict, Mouse/OSK, Script, Settings, Power)

    ButtonId hovered_btn_;
    ButtonId pressed_btn_;

    // Drag tracking for smooth dragging anywhere on the bar
    bool is_dragging_ = false;
    POINT drag_start_cursor_ = {0, 0};
    POINT drag_start_pos_ = {0, 0};
    POINT mouse_down_pt_ = {0, 0};

    // Layout rectangles
    RECT rc_mic_;
    RECT rc_mode_bn_;
    RECT rc_mode_en_;
    RECT rc_mode_trans_;
    RECT rc_pause_;
    RECT rc_dict_;
    RECT rc_osk_;
    RECT rc_script_;
    RECT rc_settings_;
    RECT rc_power_;

    // Fonts
    HFONT font_bengali_;
    HFONT font_english_;
    HFONT font_bengali_small_;
    HFONT font_english_small_;

    // Voice recognition
    bool is_voice_recording_ = false;
    bool is_voice_busy_ = false;
    std::unique_ptr<bangla_tsf::VoiceRecorder> voice_recorder_;
    bangla_tsf::VoiceConfig voice_config_;
};

} // namespace universal
} // namespace likhi
