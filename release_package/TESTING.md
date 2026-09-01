# Likhi (লিখি) — Manual Testing Checklist & Verification Protocol

> **“বাংলা লিখুন, সহজেই।”**

Please use this checklist to manually test and verify **Likhi (লিখি)** in real Windows applications.

---

## Current Verification Status

- **Automated Tests**: `352 / 352 PASS` (100%)
- **Manual Real-World Testing**: `NOT YET VERIFIED`
- **Production Readiness**: `NOT YET DECLARED`

---

## 1. Basic Bengali Typing Test

Open **Notepad** (or any text editor) and type the following words:

- [ ] `ami` $\longrightarrow$ Expected: **আমি** (Status: [ ] PASS / [ ] FAIL)
- [ ] `tumi` $\longrightarrow$ Expected: **তুমি** (Status: [ ] PASS / [ ] FAIL)
- [ ] `bhalo` $\longrightarrow$ Expected: **ভালো** (Status: [ ] PASS / [ ] FAIL)
- [ ] `valo` $\longrightarrow$ Expected: **ভালো** (Status: [ ] PASS / [ ] FAIL)
- [ ] `shundor` $\longrightarrow$ Expected: **সুন্দর** (Status: [ ] PASS / [ ] FAIL)
- [ ] `bangla` $\longrightarrow$ Expected: **বাংলা** (Status: [ ] PASS / [ ] FAIL)
- [ ] `desh` $\longrightarrow$ Expected: **দেশ** (Status: [ ] PASS / [ ] FAIL)
- [ ] `office` $\longrightarrow$ Expected: **অফিস** (Status: [ ] PASS / [ ] FAIL)
- [ ] `computer` $\longrightarrow$ Expected: **কম্পিউটার** (Status: [ ] PASS / [ ] FAIL)
- [ ] `internet` $\longrightarrow$ Expected: **ইন্টারনেট** (Status: [ ] PASS / [ ] FAIL)
- [ ] `mobile` $\longrightarrow$ Expected: **মোবাইল** (Status: [ ] PASS / [ ] FAIL)

---

## 2. Complex Bengali / Jukto Akkhor Test

Type the following words containing complex conjuncts, reph, and folas:

- [ ] `brohmoputro` $\longrightarrow$ Expected: **ব্রহ্মপুত্র** (Status: [ ] PASS / [ ] FAIL)
- [ ] `antorjatik` $\longrightarrow$ Expected: **আন্তর্জাতিক** (Status: [ ] PASS / [ ] FAIL)
- [ ] `biggopti` $\longrightarrow$ Expected: **বিজ্ঞপ্তি** (Status: [ ] PASS / [ ] FAIL)
- [ ] `attiyo` $\longrightarrow$ Expected: **আত্মীয়** (Status: [ ] PASS / [ ] FAIL)
- [ ] `utkrishto` $\longrightarrow$ Expected: **উৎকৃষ্ট** (Status: [ ] PASS / [ ] FAIL)
- [ ] `brishti` $\longrightarrow$ Expected: **বৃষ্টি** (Status: [ ] PASS / [ ] FAIL)
- [ ] `smriti` $\longrightarrow$ Expected: **স্মৃতি** (Status: [ ] PASS / [ ] FAIL)
- [ ] `akangkha` $\longrightarrow$ Expected: **আকাঙ্ক্ষা** (Status: [ ] PASS / [ ] FAIL)
- [ ] `dondwo` $\longrightarrow$ Expected: **দ্বন্দ্ব** (Status: [ ] PASS / [ ] FAIL)
- [ ] `tottwo` $\longrightarrow$ Expected: **তত্ত্ব** (Status: [ ] PASS / [ ] FAIL)
- [ ] `shringkhola` $\longrightarrow$ Expected: **শৃঙ্খলা** (Status: [ ] PASS / [ ] FAIL)
- [ ] `ujjwol` $\longrightarrow$ Expected: **উজ্জ্বল** (Status: [ ] PASS / [ ] FAIL)
- [ ] `chottogram` $\longrightarrow$ Expected: **চট্টগ্রাম** (Status: [ ] PASS / [ ] FAIL)
- [ ] `shasthyo` $\longrightarrow$ Expected: **স্বাস্থ্য** (Status: [ ] PASS / [ ] FAIL)

> **Diagnostic Inspection**: Copy the resulting text and run `inspect_unicode.exe "<pasted_text>"` to verify the Unicode codepoint stream.

---

## 3. Real-Time Suggestion & Candidate Selection Test

1. Type `ami` $\longrightarrow$ Floating suggestion window appears with ranked candidates (`1. আমি`, etc.).
2. Type `ami ajke` $\longrightarrow$ Next-word predictions and contextual candidates appear.
3. Verify selection methods:
   - [ ] Candidate selection via **Number Keys** (`1` - `5`).
   - [ ] Candidate selection via **Mouse Click** on candidate bar.
   - [ ] Candidate navigation via **Up / Down Arrow Keys**.
   - [ ] Candidate commit via **Space / Enter**.
   - [ ] Composition cancellation via **Escape**.

---

## 4. Auto-Correct Policy Test

### Test A: Auto-Correct OFF (Default)
1. Ensure Auto-Correct is OFF in `bangla_settings.exe`.
2. Type ambiguous or imperfect Banglish words.
3. Verify:
   - [ ] Candidate bar offers suggestions, but **NEVER silently alters** the user's selected/committed text.

### Test B: Auto-Correct ON
1. Turn Auto-Correct ON in `bangla_settings.exe`.
2. Type common misspelled words.
3. Verify:
   - [ ] Auto-correction occurs ONLY when confidence $\ge 0.85$ and score margin $\ge 0.03$.
   - [ ] Sub-threshold candidates are NOT automatically forced.

---

## 5. Backspace & Grapheme Rollback Test

- [ ] Type `ka` (**কা**) $\to$ Press `Backspace` once $\to$ Expected: Clean **ক** (no dangling hasant).
- [ ] Type `kkh` (**ক্ষ**) $\to$ Press `Backspace` once $\to$ Expected: Clean **ক** (no dangling hasant).
- [ ] Type `brishti` (**বৃষ্টি**) $\to$ Press `Backspace` sequentially $\to$ Expected: Graphemes roll back cleanly without orphan combining marks or detached kar/fola.

---

## 6. Punctuation Test

- [ ] Type `ami.` $\longrightarrow$ Expected: **আমি।** (Period converts to Bengali Dāri).
- [ ] Type `ami, tumi?` $\longrightarrow$ Expected: **আমি, তুমি?** (Punctuation commits active word safely).

---

## 7. Focus Switch & Recovery Test

1. Start typing a composition in Notepad (e.g. `ekh`).
2. Without pressing Space or Enter, switch focus to another application (e.g. Chrome / Desktop).
3. Return focus to Notepad.
4. Verify:
   - [ ] No crash or unhandled exception.
   - [ ] No stuck composition or orphan text buffer.
   - [ ] Candidate window hides cleanly on focus loss.
   - [ ] Keyboard input remains fully responsive.

---

## 8. Candidate Window Visual Quality

- [ ] **Positioning**: Window appears directly beneath the active text cursor/caret.
- [ ] **High-DPI**: Clear text rendering without blurry scaling on 100%, 125%, 150% DPI.
- [ ] **Flickering**: Zero flickering during rapid keystrokes.
- [ ] **Focus Stealing**: Candidate window does NOT steal focus (`WS_EX_NOACTIVATE`).

---

## 9. Real Windows Application Matrix

| Application | Architecture | Activation | Typing | Suggestions | Backspace | Commit | Focus | Result |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Windows Notepad** | Win32 / UWP | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | **NOT MANUALLY VERIFIED** |
| **Google Chrome** | Chromium x64 | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | **NOT MANUALLY VERIFIED** |
| **Microsoft Edge** | Chromium x64 | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | **NOT MANUALLY VERIFIED** |
| **VS Code** | Electron x64 | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | **NOT MANUALLY VERIFIED** |
| **Microsoft Word** | Win32 / Office | [ ] | [ ] | [ ] | [ ] | [ ] | [ ] | **NOT MANUALLY VERIFIED** |

---

## 10. Install / Uninstall Safety Verification

- [ ] `install.bat` registers COM server and TSF profile without errors.
- [ ] `uninstall.bat` completely removes TSF profile and unregisters COM DLL without damaging unrelated Windows registry keys or input settings.

---

## Manual Test Outcome Summary

- **Basic Typing**: `[ ] PASS / [ ] FAIL`
- **Jukto Akkhor Integrity**: `[ ] PASS / [ ] FAIL`
- **Suggestions & Selection**: `[ ] PASS / [ ] FAIL`
- **Auto-Correct Gating**: `[ ] PASS / [ ] FAIL`
- **Grapheme Backspace**: `[ ] PASS / [ ] FAIL`
- **Punctuation**: `[ ] PASS / [ ] FAIL`
- **Focus Switching**: `[ ] PASS / [ ] FAIL`
- **App Compatibility**: `[ ] PASS / [ ] FAIL`
- **Overall Verdict**: `[ ] READY FOR PHASE 5 / [ ] FIXES NEEDED`
