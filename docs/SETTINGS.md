# Likhi (লিখি) — Settings & Configuration Architecture

> **“বাংলা লিখুন, সহজেই।”**

## 1. Overview

**Likhi (লিখি)** provides a native, lightweight Win32 Settings application (`bangla_settings.exe`) to configure typing preferences and manage personal dictionary entries without third-party dependencies.

---

## 2. Configuration Storage

User settings are saved locally in JSON format at:
`%APPDATA%\PC-Bangla-Typing-App\settings.json`

### Default Configuration Schema:
```json
{
  "auto_correct": false,
  "show_suggestions": true,
  "max_candidates": 5,
  "launch_startup": false
}
```

---

## 3. Configurable Parameters

1. **Auto-Correct Toggle**:
   - `false` (default): No automatic corrections are forced onto committed user text.
   - `true`: Gated auto-correction occurs when confidence score $\ge 0.85$ and margin $\ge 0.03$.
2. **Show Suggestions**:
   - Enables or disables the floating candidate window popup.
3. **Max Candidates**:
   - Allows user to set 3, 4, or 5 candidates in the candidate bar.
4. **Personal Dictionary**:
   - Allows users to add custom Banglish $\to$ Bengali word pairs persisted locally in `%APPDATA%\PC-Bangla-Typing-App\personal_dict.txt`.

---

## 4. Privacy & Offline Guarantee

- **Zero Cloud Communication**: The settings and IME DLL never make any network socket connections.
- **Zero Keystroke Logging**: Keystrokes are processed solely in volatile local memory during composition and discarded upon word commit.
