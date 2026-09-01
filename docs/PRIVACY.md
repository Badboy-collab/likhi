# Privacy Policy & Security Architecture: PC Bangla Typing App

## 1. Privacy Principles
Keystrokes and typed text are among the most sensitive forms of user data, containing passwords, private messages, financial information, and personal thoughts.

**PC Bangla Typing App** adheres to a strict, uncompromising privacy-by-design policy:

1. **Zero External Data Transmission**: The application operates **100% offline**. No typed words, keystrokes, candidate selections, or contextual data are ever sent over a network.
2. **Zero Telemetry & Analytics**: The application contains no tracking SDKs, no analytics probes, no crash telemetry reporting to third parties, and no hidden background beacons.
3. **Local-Only Storage**: User dictionaries, custom vocabulary, and personal frequency counts are stored strictly on the local machine under the user's secure `%APPDATA%` profile folder.
4. **No Password / Secure Field Logging**: When an application marks an input field as a password/secure field (`TS_SS_PASSWORD` / `ES_PASSWORD`), composition and suggestions are automatically bypassed or kept isolated in volatile memory.
5. **Full User Data Control**: The user has full autonomy to inspect, export, edit, or wipe their personal dictionary at any time via the Settings UI or by deleting the local database file.

---

## 2. Data Storage & File Locations

| Data Type | Storage Location | Encryption / Protection |
| :--- | :--- | :--- |
| **Main System Lexicon** | `%ProgramFiles%\PC Bangla Typing App\data\lexicon.bin` | Read-only static binary file (No user data) |
| **User Personal Dictionary** | `%APPDATA%\PC-Bangla-Typing-App\user_dict.sqlite` | Standard Windows User Access Control (ACL protected to current Windows user) |
| **Application Settings** | `%APPDATA%\PC-Bangla-Typing-App\settings.json` | Local plain JSON file |

---

## 3. Network Permissions & Manifest
- The application binary requires **zero network capabilities**.
- The Windows installer will not request firewall inbound/outbound exceptions.
- Any future optional cloud features (e.g. voluntary cloud backup or online dictionary sync) will require **explicit opt-in consent** and will never be enabled by default.
