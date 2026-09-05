#include <windows.h>
#include <msctf.h>
#include <iostream>
#include <iomanip>
#include <cstdint>
#include "../tsf/include/bangla_tsf_clsid.h"

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::cout << "=========================================================\n";
    std::cout << "  TSF REGISTRATION & COM ACTIVATION VERIFIER\n";
    std::cout << "=========================================================\n\n";

    // 1. Load bangla_tsf.dll directly
    HMODULE hDll = LoadLibraryW(L"release_package\\bangla_tsf.dll");
    if (!hDll) {
        hDll = LoadLibraryW(L"build\\bangla_tsf.dll");
    }

    if (!hDll) {
        std::cerr << "[FAIL] LoadLibrary failed with error code: " << GetLastError() << "\n";
        return 1;
    }
    std::cout << "[PASS] bangla_tsf.dll loaded successfully into process space.\n";

    // 2. Call DllRegisterServer directly to capture HRESULT
    typedef HRESULT (STDAPICALLTYPE *pfnDllRegisterServer)();
    pfnDllRegisterServer DllReg = (pfnDllRegisterServer)GetProcAddress(hDll, "DllRegisterServer");
    if (!DllReg) {
        std::cerr << "[FAIL] GetProcAddress('DllRegisterServer') failed.\n";
        return 1;
    }

    CoInitialize(NULL);

    HRESULT hrReg = DllReg();
    std::cout << "DllRegisterServer() result: HRESULT 0x" << std::hex << (uint32_t)hrReg << std::dec << "\n";
    if (SUCCEEDED(hrReg)) {
        std::cout << "[PASS] DllRegisterServer executed successfully.\n";
    } else {
        std::cout << "[WARN] DllRegisterServer returned non-zero (may require Administrator elevation for HKLM/HKCR).\n";
    }

    // 3. Test COM Class Factory via DllGetClassObject
    typedef HRESULT (STDAPICALLTYPE *pfnDllGetClassObject)(REFCLSID, REFIID, LPVOID*);
    pfnDllGetClassObject DllGetObj = (pfnDllGetClassObject)GetProcAddress(hDll, "DllGetClassObject");
    if (!DllGetObj) {
        std::cerr << "[FAIL] GetProcAddress('DllGetClassObject') failed.\n";
        return 1;
    }

    IClassFactory* pFactory = nullptr;
    HRESULT hrFactory = DllGetObj(CLSID_BanglaTextService, IID_IClassFactory, (void**)&pFactory);
    if (SUCCEEDED(hrFactory) && pFactory) {
        std::cout << "[PASS] DllGetClassObject created IClassFactory for CLSID_BanglaTextService.\n";

        ITfTextInputProcessor* pProcessor = nullptr;
        HRESULT hrCreate = pFactory->CreateInstance(NULL, IID_ITfTextInputProcessor, (void**)&pProcessor);
        if (SUCCEEDED(hrCreate) && pProcessor) {
            std::cout << "[PASS] IClassFactory successfully instantiated ITfTextInputProcessor.\n";
            pProcessor->Release();
        } else {
            std::cerr << "[FAIL] IClassFactory::CreateInstance failed with HRESULT 0x" << std::hex << (uint32_t)hrCreate << std::dec << "\n";
        }
        pFactory->Release();
    } else {
        std::cerr << "[FAIL] DllGetClassObject failed with HRESULT 0x" << std::hex << (uint32_t)hrFactory << std::dec << "\n";
    }

    // 4. Query TSF Profiles from ITfInputProcessorProfiles
    ITfInputProcessorProfiles* pProfiles = nullptr;
    HRESULT hrProfiles = CoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles, (void**)&pProfiles);
    if (SUCCEEDED(hrProfiles) && pProfiles) {
        std::cout << "\n=== Querying Registered TSF Language Profiles ===\n";

        // Check BD LANGID (0x0445) — canonical Bengali (Bangladesh)
        IEnumTfLanguageProfiles* pEnum = nullptr;
        if (SUCCEEDED(pProfiles->EnumLanguageProfiles(BANGLA_LANGID_BD, &pEnum)) && pEnum) {
            TF_LANGUAGEPROFILE profile;
            ULONG fetched = 0;
            bool found_bd = false;
            while (pEnum->Next(1, &profile, &fetched) == S_OK && fetched == 1) {
                if (IsEqualCLSID(profile.clsid, CLSID_BanglaTextService)) {
                    found_bd = true;
                    std::cout << "  [FOUND] Profile for Bengali (Bangladesh) - LANGID 0x0445\n";
                    std::cout << "          CLSID: {B4F1470A-7C69-4C62-972F-6379532856E1}\n";
                    std::cout << "          Active: " << (profile.fActive ? "TRUE" : "FALSE") << "\n";
                }
            }
            pEnum->Release();
            if (!found_bd) {
                std::cout << "  [INFO] Bengali (Bangladesh) 0x0445 profile not in current user's active table (may need admin registration).\n";
            }
        }

        // Check IN LANGID (0x0845) — canonical Bengali (India)
        if (SUCCEEDED(pProfiles->EnumLanguageProfiles(BANGLA_LANGID_IN, &pEnum)) && pEnum) {
            TF_LANGUAGEPROFILE profile;
            ULONG fetched = 0;
            bool found_in = false;
            while (pEnum->Next(1, &profile, &fetched) == S_OK && fetched == 1) {
                if (IsEqualCLSID(profile.clsid, CLSID_BanglaTextService)) {
                    found_in = true;
                    std::cout << "  [FOUND] Profile for Bengali (India) - LANGID 0x0845\n";
                }
            }
            pEnum->Release();
            if (!found_in) {
                std::cout << "  [INFO] Bengali (India) 0x0845 profile not in active table.\n";
            }
        }

        // Check US LANGID (0x0409)
        if (SUCCEEDED(pProfiles->EnumLanguageProfiles(BANGLA_LANGID_US, &pEnum)) && pEnum) {
            TF_LANGUAGEPROFILE profile;
            ULONG fetched = 0;
            bool found_us = false;
            while (pEnum->Next(1, &profile, &fetched) == S_OK && fetched == 1) {
                if (IsEqualCLSID(profile.clsid, CLSID_BanglaTextService)) {
                    found_us = true;
                    std::cout << "  [FOUND] Profile for English (US) - LANGID 0x0409\n";
                }
            }
            pEnum->Release();
            if (!found_us) {
                std::cout << "  [INFO] English (US) 0x0409 profile not in active table.\n";
            }
        }

        pProfiles->Release();
    } else {
        std::cout << "[INFO] ITfInputProcessorProfiles CoCreateInstance: 0x" << std::hex << (uint32_t)hrProfiles << std::dec << "\n";
    }

    CoUninitialize();
    FreeLibrary(hDll);

    std::cout << "\n=========================================================\n";
    std::cout << "  REGISTRATION & IN-PROCESS VERIFICATION COMPLETE\n";
    std::cout << "=========================================================\n";
    return 0;
}
