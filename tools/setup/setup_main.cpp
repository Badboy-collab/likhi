// ===========================================================================
// Likhi — one-click Windows installer (single self-contained .exe)
//
//   LikhiSetup.exe            install (UAC prompt -> everything automatic)
//   LikhiSetup.exe /silent    install with no dialogs (exit code only)
//   LikhiSetup.exe /dryrun    report what WOULD change, touch nothing
//   LikhiSetup.exe /uninstall remove Likhi (files, registry, TSF profile)
//
// What it does on install:
//   1. Extracts the embedded IME DLL + lexicon + Settings app into
//      %ProgramFiles%\Likhi (data\lexicon.bin lives beside the DLL, which is
//      the first path the typing engine looks at).
//   2. Registers the Text Service (COM in-proc server + TSF keyboard profile)
//      by calling the DLL's own DllRegisterServer — no regsvr32 path quoting
//      pitfalls, and elevation means HKLM/HKCU both succeed.
//   3. Cleans the input-method list: removes OTHER Bengali keyboards
//      (Microsoft Bangla Phonetic, Bengali-India layouts, leftovers of older
//      Likhi builds) so Win+Space shows exactly two things — the PC's normal
//      keyboard and "Likhi (লিখি)". Nothing else is touched.
//   4. Makes Likhi the default profile for the Bengali (Bangladesh) language,
//      adds Start-Menu shortcuts and an Apps-list uninstall entry.
//
// The installer never changes the user's default *language* (English stays the
// PC default) and never touches non-Bengali keyboards.
// ===========================================================================

#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <msctf.h>
#include <objbase.h>
#include <string>
#include <vector>
#include <cstdio>

#include "bangla_tsf_clsid.h"   // CLSID/GUID/langid/name constants (single source)

#define IDR_PAYLOAD_DLL      101
#define IDR_PAYLOAD_LEX      102
#define IDR_PAYLOAD_SETTINGS 103

static const wchar_t* kAppName    = L"Likhi";
static const wchar_t* kAppDisplay = L"Likhi - PC Bangla Typing App";

static std::wstring g_log;
static bool         g_dry = false;

static void Log(const std::wstring& line) {
    g_log += line;
    g_log += L"\r\n";
}

// NOTE: this is built with MinGW, whose ISO-C vswprintf treats %s as a NARROW
// (multibyte) string and %ls as a wide string — the opposite of MSVC. Passing a
// wchar_t* to %s silently truncates it at the first zero byte, so every wide
// argument below must use %ls.
static void LogF(const wchar_t* fmt, ...) {
    wchar_t buf[1024];
    va_list ap;
    va_start(ap, fmt);
    if (vswprintf(buf, sizeof(buf) / sizeof(buf[0]), fmt, ap) < 0) buf[0] = L'\0';
    va_end(ap);
    Log(buf);
}

// ---------------------------------------------------------------------------
// Paths
// ---------------------------------------------------------------------------
static std::wstring ModuleDir() {
    wchar_t path[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring s(path);
    size_t pos = s.find_last_of(L"\\/");
    return (pos == std::wstring::npos) ? L"." : s.substr(0, pos);
}

static std::wstring InstallDir() {
    wchar_t pf[MAX_PATH] = {0};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, 0, pf))) {
        return L"C:\\Program Files\\Likhi";
    }
    return std::wstring(pf) + L"\\" + kAppName;
}

static std::wstring ProgramDataDir() {
    wchar_t pd[MAX_PATH] = {0};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, pd))) {
        return L"C:\\ProgramData\\Likhi";
    }
    return std::wstring(pd) + L"\\" + kAppName;
}

static void WriteLogFile() {
    std::wstring dir = ProgramDataDir();
    CreateDirectoryW(dir.c_str(), nullptr);
    std::wstring file = dir + L"\\setup.log";
    HANDLE h = CreateFileW(file.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    // UTF-16LE so the Bengali lines survive
    const unsigned char bom[2] = {0xFF, 0xFE};
    DWORD written = 0;
    WriteFile(h, bom, 2, &written, nullptr);
    WriteFile(h, g_log.data(), (DWORD)(g_log.size() * sizeof(wchar_t)), &written, nullptr);
    CloseHandle(h);
}

// ---------------------------------------------------------------------------
// Embedded payload extraction
// ---------------------------------------------------------------------------
static bool ExtractResource(int resId, const std::wstring& target, bool& wrote) {
    wrote = false;
    if (g_dry) { Log(L"  [dry] would write " + target); return true; }

    HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(resId), RT_RCDATA);
    if (!res) { LogF(L"  [FAIL] payload resource %d missing", resId); return false; }
    DWORD size = SizeofResource(nullptr, res);
    HGLOBAL mem = LoadResource(nullptr, res);
    if (!mem || size == 0) { LogF(L"  [FAIL] payload resource %d unreadable", resId); return false; }
    const void* data = LockResource(mem);

    // Replace an existing (possibly loaded/in-use) file when possible.
    if (GetFileAttributesW(target.c_str()) != INVALID_FILE_ATTRIBUTES) {
        if (!DeleteFileW(target.c_str())) {
            std::wstring old = target + L".old";
            DeleteFileW(old.c_str());
            MoveFileExW(target.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING);
        }
    }

    HANDLE h = CreateFileW(target.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) {
        LogF(L"  [FAIL] cannot write %ls (error %lu)", target.c_str(), GetLastError());
        return false;
    }
    DWORD written = 0;
    BOOL ok = WriteFile(h, data, size, &written, nullptr);
    CloseHandle(h);
    if (!ok || written != size) {
        LogF(L"  [FAIL] short write for %ls", target.c_str());
        return false;
    }
    LogF(L"  wrote %ls (%lu bytes)", target.c_str(), size);
    wrote = true;
    return true;
}

// ---------------------------------------------------------------------------
// DLL registration (call the DLL's own exports — no regsvr32, no quoting bugs)
// ---------------------------------------------------------------------------
typedef HRESULT (STDAPICALLTYPE *PFN_DllRegisterServer)();
typedef HRESULT (STDAPICALLTYPE *PFN_DllUnregisterServer)();

static bool CallDllExport(const std::wstring& dllPath, const char* exportName, bool& found) {
    found = false;
    if (g_dry) { LogF(L"  [dry] would call %s in %ls", exportName, dllPath.c_str()); return true; }

    HMODULE mod = LoadLibraryW(dllPath.c_str());
    if (!mod) {
        LogF(L"  [FAIL] LoadLibrary %ls failed (%lu)", dllPath.c_str(), GetLastError());
        return false;
    }
    FARPROC pfn = GetProcAddress(mod, exportName);
    if (!pfn) {
        LogF(L"  [FAIL] export %s not found", exportName);
        FreeLibrary(mod);
        return false;
    }
    found = true;
    HRESULT hr = reinterpret_cast<PFN_DllRegisterServer>(pfn)();
    FreeLibrary(mod);
    LogF(L"  %s -> 0x%08lX", exportName, (unsigned long)hr);
    return SUCCEEDED(hr);
}

// ---------------------------------------------------------------------------
// Input-method list cleanup: drop every Bengali keyboard that is not Likhi
// ---------------------------------------------------------------------------
static std::wstring ProfileDescription(ITfInputProcessorProfiles* profiles,
                                       REFCLSID clsid, LANGID langid, REFGUID guidProfile) {
    BSTR desc = nullptr;
    if (profiles && SUCCEEDED(profiles->GetLanguageProfileDescription(
                         clsid, langid, guidProfile, &desc)) && desc) {
        std::wstring s(desc, SysStringLen(desc));
        SysFreeString(desc);
        return s;
    }
    return L"(unknown)";
}

// Enumerates the Bengali input languages and removes every keyboard that is
// not Likhi, so Win+Space shows exactly: the PC's normal keyboard + Likhi.
// (Uses ITfInputProcessorProfiles — the same COM service the DLL registers
// against — so no extra TSF coclass is needed and the two APIs cannot drift.)
static int CleanCompetingKeyboards() {
    ITfInputProcessorProfiles* profiles = nullptr;
    if (FAILED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                IID_ITfInputProcessorProfiles, (void**)&profiles))) {
        Log(L"  [warn] TSF profile service unavailable — skipped keyboard cleanup");
        return 0;
    }

    // removal must happen after enumeration: deleting while iterating would
    // invalidate the enumerator and silently skip entries.
    struct Victim { CLSID clsid; LANGID langid; GUID guid; };
    std::vector<Victim> victims;

    const LANGID langs[2] = { BANGLA_LANGID_BD, BANGLA_LANGID_IN };
    for (int i = 0; i < 2; ++i) {
        IEnumTfLanguageProfiles* en = nullptr;
        if (FAILED(profiles->EnumLanguageProfiles(langs[i], &en)) || !en) continue;

        TF_LANGUAGEPROFILE lp;
        ULONG fetched = 0;
        while (en->Next(1, &lp, &fetched) == S_OK && fetched == 1) {
            if (IsEqualCLSID(lp.clsid, CLSID_BanglaTextService)) continue; // ours -> keep
            std::wstring name = ProfileDescription(profiles, lp.clsid, lp.langid, lp.guidProfile);
            LogF(L"  removing other Bengali keyboard: %ls (langid 0x%04X)", name.c_str(), lp.langid);
            victims.push_back({ lp.clsid, lp.langid, lp.guidProfile });
        }
        en->Release();
    }

    int removed = 0;
    for (const Victim& v : victims) {
        if (g_dry) { ++removed; continue; }
        HRESULT hr = profiles->RemoveLanguageProfile(v.clsid, v.langid, v.guid);
        if (SUCCEEDED(hr)) {
            ++removed;
        } else {
            LogF(L"    [warn] RemoveLanguageProfile -> 0x%08lX (may be protected)", (unsigned long)hr);
        }
    }

    profiles->Release();
    return removed;
}

static bool SetLikhiDefaultBengaliProfile() {
    ITfInputProcessorProfiles* profiles = nullptr;
    if (FAILED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                IID_ITfInputProcessorProfiles, (void**)&profiles))) {
        Log(L"  [warn] cannot open TSF profiles — default profile not set");
        return false;
    }
    if (g_dry) {
        Log(L"  [dry] would set Likhi as default profile for Bengali (Bangladesh)");
        profiles->Release();
        return true;
    }
    HRESULT hr = profiles->SetDefaultLanguageProfile(BANGLA_LANGID_BD, CLSID_BanglaTextService,
                                                     GUID_BanglaProfile);
    profiles->Release();
    LogF(L"  default bn-BD profile -> 0x%08lX", (unsigned long)hr);
    return SUCCEEDED(hr);
}

// ---------------------------------------------------------------------------
// Shell integration
// ---------------------------------------------------------------------------
static void CreateShortcut(const std::wstring& target, const std::wstring& linkPath,
                           const std::wstring& workDir, const std::wstring& desc) {
    if (g_dry) { Log(L"  [dry] shortcut " + linkPath); return; }
    IShellLinkW* link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IShellLinkW, (void**)&link))) return;
    link->SetPath(target.c_str());
    link->SetWorkingDirectory(workDir.c_str());
    link->SetDescription(desc.c_str());
    IPersistFile* file = nullptr;
    if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, (void**)&file))) {
        file->Save(linkPath.c_str(), TRUE);
        file->Release();
    }
    link->Release();
}

static std::wstring StartMenuDir() {
    wchar_t p[MAX_PATH] = {0};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_COMMON_PROGRAMS, nullptr, 0, p))) return L"";
    return std::wstring(p) + L"\\" + kAppName;
}

static bool CreateShortcuts(const std::wstring& installDir) {
    std::wstring dir = StartMenuDir();
    if (dir.empty()) return false;
    if (!g_dry) CreateDirectoryW(dir.c_str(), nullptr);
    CreateShortcut(installDir + L"\\bangla_settings.exe", dir + L"\\Likhi Settings.lnk",
                   installDir, L"Likhi settings");
    CreateShortcut(installDir + L"\\LikhiSetup.exe", dir + L"\\Uninstall Likhi.lnk",
                   installDir, L"Uninstall Likhi");
    Log(L"  start-menu shortcuts created");
    return true;
}

static void WriteUninstallEntry(const std::wstring& installDir, const std::wstring& setupExe) {
    if (g_dry) { Log(L"  [dry] Apps-list uninstall entry"); return; }
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE,
                        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Likhi",
                        0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        Log(L"  [warn] could not write uninstall entry");
        return;
    }
    auto set = [&](const wchar_t* name, const std::wstring& value) {
        RegSetValueExW(key, name, 0, REG_SZ, (const BYTE*)value.c_str(),
                       (DWORD)((value.size() + 1) * sizeof(wchar_t)));
    };
    set(L"DisplayName", kAppDisplay);
    set(L"DisplayVersion", L"1.0.0");
    set(L"Publisher", L"Likhi");
    set(L"InstallLocation", installDir);
    set(L"DisplayIcon", installDir + L"\\bangla_settings.exe");
    set(L"UninstallString", L"\"" + setupExe + L"\" /uninstall");
    set(L"QuietUninstallString", L"\"" + setupExe + L"\" /uninstall /silent");
    DWORD one = 1;
    RegSetValueExW(key, L"NoModify", 0, REG_DWORD, (const BYTE*)&one, sizeof(one));
    RegSetValueExW(key, L"NoRepair", 0, REG_DWORD, (const BYTE*)&one, sizeof(one));
    RegCloseKey(key);
    Log(L"  uninstall entry written (Apps list)");
}

static void RemoveShortcuts() {
    std::wstring dir = StartMenuDir();
    if (dir.empty()) return;
    DeleteFileW((dir + L"\\Likhi Settings.lnk").c_str());
    DeleteFileW((dir + L"\\Uninstall Likhi.lnk").c_str());
    RemoveDirectoryW(dir.c_str());
}

// ---------------------------------------------------------------------------
// Install / uninstall
// ---------------------------------------------------------------------------
static int DoInstall() {
    Log(L"=== Likhi install ===");
    std::wstring dir = InstallDir();
    Log(L"install dir: " + dir);

    if (!g_dry && !CreateDirectoryW(dir.c_str(), nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
        LogF(L"[FAIL] cannot create %ls (error %lu)", dir.c_str(), GetLastError());
        WriteLogFile();
        return 2;
    }
    if (!g_dry) CreateDirectoryW((dir + L"\\data").c_str(), nullptr);

    // 1. payload
    Log(L"[1/5] installing files");
    bool wrote = false;
    if (!ExtractResource(IDR_PAYLOAD_DLL, dir + L"\\bangla_tsf.dll", wrote)) { WriteLogFile(); return 3; }
    if (!ExtractResource(IDR_PAYLOAD_LEX, dir + L"\\data\\lexicon.bin", wrote)) { WriteLogFile(); return 3; }
    if (!ExtractResource(IDR_PAYLOAD_SETTINGS, dir + L"\\bangla_settings.exe", wrote)) { WriteLogFile(); return 3; }

    // The installer itself must live in the install dir so the uninstall entry
    // keeps working after the original download is deleted.
    wchar_t self[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring selfPath(self);
    std::wstring installedSetup = dir + L"\\LikhiSetup.exe";
    if (!g_dry && _wcsicmp(selfPath.c_str(), installedSetup.c_str()) != 0) {
        DeleteFileW((installedSetup + L".old").c_str());
        if (!CopyFileW(selfPath.c_str(), installedSetup.c_str(), FALSE)) {
            LogF(L"  [warn] cannot copy installer into %ls (error %lu)", dir.c_str(), GetLastError());
        } else {
            Log(L"  installer copied for uninstall support");
        }
    }

    // 2. Text Service registration
    Log(L"[2/5] registering Text Service");
    bool found = false;
    if (!CallDllExport(dir + L"\\bangla_tsf.dll", "DllRegisterServer", found)) {
        Log(L"[FAIL] TSF registration failed — the keyboard will not appear");
        WriteLogFile();
        return 4;
    }

    // 3. input-method cleanup + default Bengali profile
    Log(L"[3/5] cleaning input-method list (keep PC keyboard + Likhi only)");
    int removed = CleanCompetingKeyboards();
    LogF(L"  removed %d other Bengali keyboard(s)", removed);
    SetLikhiDefaultBengaliProfile();

    // 4. shell integration
    Log(L"[4/5] shortcuts");
    CreateShortcuts(dir);

    // 5. Apps-list entry
    Log(L"[5/5] uninstall entry");
    WriteUninstallEntry(dir, installedSetup);

    Log(L"=== install finished OK ===");
    WriteLogFile();
    return 0;
}

static int DoUninstall() {
    Log(L"=== Likhi uninstall ===");
    std::wstring dir = InstallDir();
    std::wstring dll = dir + L"\\bangla_tsf.dll";

    Log(L"[1/4] unregistering Text Service");
    bool found = false;
    if (GetFileAttributesW(dll.c_str()) != INVALID_FILE_ATTRIBUTES) {
        CallDllExport(dll, "DllUnregisterServer", found);
    } else {
        Log(L"  DLL not found — nothing to unregister");
    }

    Log(L"[2/4] removing shortcuts");
    if (!g_dry) RemoveShortcuts();

    Log(L"[3/4] removing Apps-list entry");
    if (!g_dry) {
        RegDeleteKeyW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Likhi");
    }

    Log(L"[4/4] deleting files");
    if (!g_dry) {
        std::vector<std::wstring> files = {
            dir + L"\\bangla_tsf.dll", dir + L"\\bangla_settings.exe",
            dir + L"\\data\\lexicon.bin", dir + L"\\LikhiSetup.exe",
            dir + L"\\bangla_tsf.dll.old"
        };
        for (const auto& f : files) {
            if (!DeleteFileW(f.c_str())) {
                MoveFileExW(f.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
            }
        }
        RemoveDirectoryW((dir + L"\\data").c_str());
        if (!RemoveDirectoryW(dir.c_str())) {
            // still locked (IME loaded in a running app) -> finish after reboot
            MoveFileExW((dir + L"\\LikhiSetup.exe").c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
            Log(L"  some files are in use — they will be removed after the next sign-out/restart");
        }
    }

    Log(L"=== uninstall finished ===");
    WriteLogFile();
    return 0;
}

// ---------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool silent = false, uninstall = false;
    for (int i = 1; i < argc; ++i) {
        std::wstring a = argv[i];
        if (_wcsicmp(a.c_str(), L"/silent") == 0 || _wcsicmp(a.c_str(), L"/S") == 0) silent = true;
        else if (_wcsicmp(a.c_str(), L"/uninstall") == 0 || _wcsicmp(a.c_str(), L"/u") == 0) uninstall = true;
        else if (_wcsicmp(a.c_str(), L"/dryrun") == 0) g_dry = true;
    }
    if (argv) LocalFree(argv);

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int rc = uninstall ? DoUninstall() : DoInstall();
    CoUninitialize();

    if (!silent) {
        std::wstring title = uninstall ? L"Likhi — uninstalled" : L"Likhi — installed";
        std::wstring body;
        if (rc == 0 && uninstall) {
            body = L"Likhi সরানো হয়েছে।\r\n\r\n"
                   L"কোনো keyboard মুছে ফেলা হয়নি — শুধু Likhi-এর নিজের registration বাদ গেছে।";
        } else if (rc == 0) {
            body = L"✅ Likhi ইনস্টল হয়েছে!\r\n\r\n"
                   L"Win + Space চেপে দেখুন — শুধু আপনার PC-র সাধারণ keyboard আর "
                   L"\"Likhi (লিখি)\" থাকবে, অন্য বাংলা keyboard গুলো সরিয়ে দেওয়া হয়েছে।\r\n\r\n"
                   L"এখন একবার sign out → sign in করুন (IME নতুন করে load হবে), তারপর "
                   L"Notepad/Word-এ বাংলা লিখুন।\r\n\r\n"
                   L"বিস্তারিত লগ: %ProgramData%\\Likhi\\setup.log";
        } else {
            body = L"⚠️ ইনস্টল সম্পূর্ণ হয়নি (কোড " + std::to_wstring(rc) + L")।\r\n\r\n"
                   L"লগ দেখুন: %ProgramData%\\Likhi\\setup.log";
        }
        MessageBoxW(nullptr, body.c_str(), title.c_str(),
                    MB_OK | (rc == 0 ? MB_ICONINFORMATION : MB_ICONWARNING));
    }
    return rc;
}
