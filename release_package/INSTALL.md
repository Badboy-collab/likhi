# Likhi (লিখি) — Installation & Setup Guide

> **“বাংলা লিখুন, সহজেই।”**

## 1. One-Click Installation (Recommended)

**Just double-click `LikhiSetup.exe`** and accept the Windows UAC prompt
("Do you want to allow this app to make changes?") — that is the whole install.

The installer is a single self-contained file (the IME DLL, the 52,000+ word
lexicon and the Settings app travel inside it) and it:

- installs everything into `%ProgramFiles%\Likhi`,
- registers the Text Service (COM in-proc server + TSF keyboard profile),
- **cleans the input-method list**: every other Bengali keyboard that was
  installed on the PC (for example *Microsoft Bangla Phonetic* or an older
  Likhi build) is removed, so `Win + Space` shows exactly two things —
  your normal PC keyboard and **“Likhi (লিখি)”**. Nothing else is touched:
  your default language and all non-Bengali keyboards stay as they were,
- makes Likhi the default keyboard for Bengali (Bangladesh),
- adds a **Likhi Settings** Start-Menu shortcut and an Apps-list
  (Add/Remove programs) entry so it can be uninstalled normally.

Then **sign out → sign in once** (or reboot) so Windows loads the new IME, and
type anywhere (Notepad, Word, Chrome, VS Code).

| Command | What it does |
| :--- | :--- |
| `LikhiSetup.exe` | install / update (shows a summary dialog) |
| `LikhiSetup.exe /silent` | install with no dialogs (exit code only) |
| `LikhiSetup.exe /dryrun` | report what *would* change, touch nothing |
| `LikhiSetup.exe /uninstall` | remove Likhi (files, registry, TSF profile) |

A detailed log of every install is written to
`%ProgramData%\Likhi\setup.log`.

---

## 1b. Manual Installation (Administrator Required, legacy)

1. Navigate to the `release_package` folder.
2. **Right-Click** on `install.bat` and select **"Run as administrator"**.
3. The script will:
   - Copy the 52,000+ word dictionary (`lexicon.bin`) into `%APPDATA%\PC-Bangla-Typing-App\`.
   - Register `bangla_tsf.dll` with Windows COM and Text Services Framework (TSF).
4. You will see:
   ```text
   [SUCCESS] Likhi (লিখি) registered successfully!
   ```

Note: this path does **not** clean up competing Bengali keyboards — use
`LikhiSetup.exe` if you want a clean `Win + Space` list.

---

## 2. Enabling & Selecting the IME in Windows

1. Press **`Win + Space`** on your keyboard (or click the language bar icon in the Windows taskbar).
2. Select **"Likhi (লিখি)"**.
3. Open any application (such as **Notepad**, **Microsoft Word**, **Google Chrome**, **Microsoft Edge**, or **VS Code**).
4. Start typing in Roman Banglish (e.g. `ami` $\to$ `আমি`).

---

## 3. Configuring Settings & Personal Dictionary

To configure preferences:
1. Double-click `bangla_settings.exe`.
2. Options available:
   - **Enable Auto-Correct**: Toggle strict auto-correction ($\ge 0.85$ confidence gate).
   - **Show Suggestion Window**: Toggle the floating candidate window.
   - **Number of Candidates**: Select 3, 4, or 5 candidates.
   - **Personal Dictionary**: Add custom Roman $\to$ Bengali word pairs.
3. Click **"Save Settings"**.

---

## 4. Unicode Verification Diagnostic Tool

If you want to verify that typed Bengali text has 100% valid Unicode codepoint sequences:
1. Run `inspect_unicode.exe`.
2. Paste or type any Bengali text (e.g. `বৃষ্টি`, `ব্রহ্মপুত্র`).
3. The tool prints a detailed codepoint analysis confirming zero dangling Hasants (`U+09CD`) and intact conjunct sequences.

---

## 5. Uninstallation

1. **Right-Click** on `uninstall.bat` and select **"Run as administrator"**.
2. The script cleanly unregisters the COM server and removes the TSF input profile.
