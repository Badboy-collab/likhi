# 🚀 Likhi (লিখি) — cPanel & System Update Status Log
**Last Updated**: 2026-10-25 20:45 (BST)  
**Release Version**: `v1.1.0-stable`  
**Target Environment**: Windows 10 & 11 (x64 / x86) | Web: [getlikhi.com](https://getlikhi.com)

---

## 📌 Executive Summary of Today's Completed Milestones

### 1. 🧠 Next-Word Prediction Engine (পরবর্তী শব্দ সাজেশন)
- **Local Contextual Bigrams**: শব্দ টাইপ শেষ করে স্পেস দিলেই প্রাসঙ্গিক পরবর্তী শব্দগুলো সাজেশনে চলে আসে (যেমন: "আমি" -> "আজকে", "অফিসে", "বাসায়")।
- **Non-Intrusive Dismissal**: পরবর্তী কোনো সাজেশন সিলেক্ট না করে ব্যবহারকারী নতুন টাইপ শুরু করা মাত্রই পেছনের সাজেশনটি তৎক্ষণাৎ সরে গিয়ে বর্তমান লেখার সাজেশন চলে আসবে (কনফ্লিক্ট ফিক্সড)।
- **Language Mode Isolation**: ইংরেজি বা বাংলিশ মোডে থাকাকালে ভুলভাবে সাজেশন উইন্ডো আটকে থাকা রোধ করা হয়েছে।
- **Privacy Gated**: ১০০% অফলাইন ও লোকাল প্রসেসিং — কোনো ডেটা ক্লাউডে পাঠানো হয় না।

### 2. 📖 Dictionary Curation & Typo Purification
- অভিধানের ৮৬,০০০+ শব্দভাণ্ডার নিখুঁতভাবে অডিট করা হয়েছে।
- হাবিজাবি বানান, ভুল যুক্তবর্ণ এবং অবাঞ্ছিত শব্দ ফিল্টার করে রিমুভ করা হয়েছে।
- রোমান ফোনেটিক ওভাররাইড ম্যাপিং উন্নত করা হয়েছে যাতে বাস্তব জীবনের বাংলা টাইপিং আরো বেশি দ্রুত ও নির্ভুল হয়।

### 3. 🖥️ TopBar Auto-Hide & Persistence
- **Auto-Hide Behaviour**: মাউস টপবার থেকে সরে গেলে ১২০০ms পর স্বয়ংক্রিয়ভাবে সংকুচিত হয়ে ১৬০px কমপ্যাক্ট মোডে চলে আসে।
- **Instant Expand**: মাউস টপবারের যেকোনো অংশে হোভার করলেই ইনস্ট্যান্ট ২৬২px ফুল বারে এক্সপ্যান্ড হয়।
- **Persistent Settings**: `%APPDATA%\PC-Bangla-Typing-App\settings.json`-এ `"topbar_autohide": true` এবং `"show_topbar": true` স্থায়ীভাবে সংরক্ষিত।
- **PC Restart Guarantee**: পিসি রিস্টার্টের পর টপবার যাতে ১০০% নিশ্চিতভাবে চলে আসে সেজন্য ডুয়েল-লেয়ার ব্যাকআপ ব্যবস্থা করা হয়েছে:
  1. `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` (`LikhiUniversal`)
  2. `%APPDATA%\Microsoft\Windows\Start Menu\Programs\Startup\Likhi.lnk`

### 4. 🎴 Kontho-Style Dedicated Hover Tooltip Cards (`LikhiTooltipCard`)
উইন্ডোজের সাধারণ বা বাগি ডিফল্ট টুলটিপ বাদ দিয়ে ডেডিকেটেড হোয়াইট কার্ড, ১px বর্ডার (#D1D5DB) এবং সফট শ্যাডো (`CS_DROPSHADOW`) সম্বলিত উইন্ডো যুক্ত করা হয়েছে। প্রতিটি আইকনে মাউস নিলেই সাথে সাথে কার্ডটি ভেসে ওঠে:
- **[⌨️] On Screen Keyboard**: *"Use the mouse to click and type in Bangla, English, special characters, or emojis."*
- **[⏸️] Typing Mode**: *"Enable (▶) to display the word suggestion window while typing, and pause (⏸) when typing sensitive content to avoid displaying text."*
- **[📖] Word Manager**: *"Add custom words to your personal dictionary and manage learned words."*
- **[🌐] Translation Mode**: *"Convert Banglish phonetic typing directly into English sentences [Ctrl+Alt+E]."*
- **[🎙️] Voice Typing**: *"Click to start voice typing in Bangla, or press [Ctrl+Alt+V]."*
- **[⚙️] Likhi Settings**: *"Configure keyboard shortcuts, layout, voice typing options, and appearance."*
- **[⏻] Likhi Menu**: *"Access options, move to system tray, restart, or exit Likhi."*

### 5. 🛠️ Settings Dashboard (`bangla_settings.exe`) Fixes
- **Save & Close Buttons**: Save এবং Close বাটনের হ্যান্ডলার পুনরুদ্ধার করা হয়েছে; সেটিংস পরিবর্তন সাথে সাথে রান-টাইমে কার্যকর হয়।

---

## 📦 Binary & Deployment Artifacts

| Binary / Asset | Path | Size | SHA-256 Digest | Status |
| :--- | :--- | :--- | :--- | :--- |
| **LikhiSetup.exe** | `release_package\LikhiSetup.exe` | 20,420,568 B (~19.47 MB) | `38036f02392bcf668b448d26f1c64dba854445556ca63702b79d05194650924b` | ✅ Built & Ready |
| **likhi_universal.exe** | `release_package\likhi_universal.exe` | 1,811,968 B | — | ✅ Verified & Live |
| **bangla_settings.exe** | `release_package\bangla_settings.exe` | 1,224,704 B | — | ✅ Verified |
| **bangla_tsf.dll** | `release_package\bangla_tsf.dll` | 2,418,688 B | — | ✅ Verified |
| **lexicon.bin** | `release_package\data\lexicon.bin` | 6,416,281 B | — | ✅ Verified (86k+ words) |
| **version.json** | `release_package\version.json` | Updated | — | ✅ Synced |

---

## 🧪 Automated Regression Verification Results
- **Update Service Tests**: 80 / 80 Passed (100%)
- **Key Policy Regression Suite**: 196 / 196 Passed (100%)
- **Core Engine Unit Tests (Suite 1-12)**: 260 / 260 Passed (100%)
- **Unicode & Conjunct Integrity Tests**: 46 / 46 Passed (100%)
- **Total Test Gate**: **582 / 582 Assertions (100% Success, 0 Failures)**

---

## 🌐 Next Steps for Tomorrow
1. Live feedback iteration on user typing sessions.
2. Web portal analytics & download metrics review.
3. Continued linguistic polish based on user personal dictionaries.
