// Self-check: call DllRegisterServer directly and report HRESULT + registry write result.
#include <windows.h>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "usage: register_selfcheck <dll path>\n"; return 2; }
    std::cout << "argv[1] = [" << argv[1] << "]\n";
    HANDLE f = CreateFileA(argv[1], GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (f == INVALID_HANDLE_VALUE) { std::cerr << "file open failed: " << GetLastError() << "\n"; return 5; }
    CloseHandle(f);
    std::cout << "file exists, OK\n";
    SetLastError(0);
    HMODULE h = LoadLibraryA(argv[1]);
    if (!h) {
        DWORD e = GetLastError();
        char msg[512] = {0};
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, e, 0, msg, sizeof(msg), NULL);
        std::cerr << "LoadLibrary failed err=" << e << " : " << msg << "\n";
        return 3;
    }
    std::cout << "LoadLibrary OK, hmodule=" << h << "\n";
    bool do_unregister = (argc >= 3 && std::string(argv[2]) == "unreg");
    typedef HRESULT (STDAPICALLTYPE* RegFn)(void);
    RegFn fn = (RegFn)GetProcAddress(h, do_unregister ? "DllUnregisterServer" : "DllRegisterServer");
    if (!fn) {
        std::cerr << (do_unregister ? "DllUnregisterServer" : "DllRegisterServer") << " export not found\n";
        return 4;
    }
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    HRESULT r = fn();
    std::cout << (do_unregister ? "DllUnregisterServer" : "DllRegisterServer")
              << " returned 0x" << std::hex << r << std::dec << " ("
              << (r == S_OK ? "S_OK" : (r == S_FALSE ? "S_FALSE" : "FAILURE")) << ")\n";
    // Check the CLSID default value
    HKEY k = NULL;
    LONG lr = RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Classes\\CLSID\\{B4F1470A-7C69-4C62-972F-6379532856E1}\\InprocServer32",
        0, KEY_READ, &k);
    if (lr == ERROR_SUCCESS) {
        wchar_t buf[MAX_PATH]; DWORD sz = sizeof(buf); DWORD type = 0;
        LONG vr = RegQueryValueExW(k, NULL, NULL, &type, (LPBYTE)buf, &sz);
        if (vr == ERROR_SUCCESS) std::wcout << L"CLSID registered -> " << buf << L"\n";
        else std::cout << "CLSID value query failed: " << vr << "\n";
        RegCloseKey(k);
    } else {
        std::cout << "CLSID key NOT found (open err " << lr << ")\n";
    }
    if (hr == S_OK || hr == S_FALSE) CoUninitialize();
    FreeLibrary(h);
    return 0;
}
