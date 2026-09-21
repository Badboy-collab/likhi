#include "../include/bangla_tsf_clsid.h"
#include <msctf.h>
#include <olectl.h>
#include <string>

namespace bangla_tsf {

static const wchar_t* CLSID_STRING = L"{B4F1470A-7C69-4C62-972F-6379532856E1}";

HRESULT RegisterCOMServer(HINSTANCE hInst) {
    wchar_t dll_path[MAX_PATH];
    if (GetModuleFileNameW(hInst, dll_path, MAX_PATH) == 0) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Try system-wide HKCR first (writes to HKLM\SOFTWARE\Classes\CLSID if elevated)
    HKEY rootKeys[] = { HKEY_CLASSES_ROOT, HKEY_CURRENT_USER };
    std::wstring subBases[] = {
        L"CLSID\\" + std::wstring(CLSID_STRING),
        L"Software\\Classes\\CLSID\\" + std::wstring(CLSID_STRING)
    };

    bool registered = false;
    for (int i = 0; i < 2; i++) {
        HKEY hKey = NULL;
        if (RegCreateKeyExW(rootKeys[i], subBases[i].c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
            RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)BANGLA_IME_NAME_W, (DWORD)((wcslen(BANGLA_IME_NAME_W) + 1) * sizeof(wchar_t)));
            RegCloseKey(hKey);

            std::wstring inproc_path = subBases[i] + L"\\InprocServer32";
            if (RegCreateKeyExW(rootKeys[i], inproc_path.c_str(), 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
                RegSetValueExW(hKey, NULL, 0, REG_SZ, (const BYTE*)dll_path, (DWORD)((wcslen(dll_path) + 1) * sizeof(wchar_t)));
                const wchar_t* threading_model = L"Apartment";
                RegSetValueExW(hKey, L"ThreadingModel", 0, REG_SZ, (const BYTE*)threading_model, (DWORD)((wcslen(threading_model) + 1) * sizeof(wchar_t)));
                RegCloseKey(hKey);
                registered = true;
            }
        }
    }

    return registered ? S_OK : E_FAIL;
}

HRESULT UnregisterCOMServer() {
    std::wstring clsid_str(CLSID_STRING);
    // Remove from HKCR
    RegDeleteKeyW(HKEY_CLASSES_ROOT, (L"CLSID\\" + clsid_str + L"\\InprocServer32").c_str());
    RegDeleteKeyW(HKEY_CLASSES_ROOT, (L"CLSID\\" + clsid_str).c_str());

    // Remove from HKCU
    RegDeleteKeyW(HKEY_CURRENT_USER, (L"Software\\Classes\\CLSID\\" + clsid_str + L"\\InprocServer32").c_str());
    RegDeleteKeyW(HKEY_CURRENT_USER, (L"Software\\Classes\\CLSID\\" + clsid_str).c_str());
    return S_OK;
}

HRESULT RegisterTSFProfiles(HINSTANCE hInst) {
    wchar_t dll_path[MAX_PATH];
    GetModuleFileNameW(hInst, dll_path, MAX_PATH);

    ITfInputProcessorProfiles* pProfiles = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles, (void**)&pProfiles);
    if (FAILED(hr) || !pProfiles) return hr;

    hr = pProfiles->Register(CLSID_BanglaTextService);
    if (SUCCEEDED(hr)) {
        LANGID langids[] = { BANGLA_LANGID_BD, BANGLA_LANGID_IN, BANGLA_LANGID_US };
        for (LANGID lid : langids) {
            pProfiles->AddLanguageProfile(
                CLSID_BanglaTextService,
                lid,
                GUID_BanglaProfile,
                BANGLA_IME_NAME_W,
                (ULONG)wcslen(BANGLA_IME_NAME_W),
                dll_path,
                (ULONG)wcslen(dll_path),
                0
            );
            pProfiles->EnableLanguageProfile(CLSID_BanglaTextService, lid, GUID_BanglaProfile, TRUE);
        }
    }
    pProfiles->Release();

    // Register Categories: Text Service is a standard keyboard TIP with Immersive/UWP app support
    ITfCategoryMgr* pCategoryMgr = nullptr;
    hr = CoCreateInstance(CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr, (void**)&pCategoryMgr);
    if (SUCCEEDED(hr) && pCategoryMgr) {
        pCategoryMgr->RegisterCategory(CLSID_BanglaTextService, GUID_TFCAT_TIP_KEYBOARD, CLSID_BanglaTextService);
        pCategoryMgr->RegisterCategory(CLSID_BanglaTextService, GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT, CLSID_BanglaTextService);
        pCategoryMgr->RegisterCategory(CLSID_BanglaTextService, GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT, CLSID_BanglaTextService);
        // Cleanly remove any stale GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER from older builds
        pCategoryMgr->UnregisterCategory(CLSID_BanglaTextService, GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER, CLSID_BanglaTextService);
        pCategoryMgr->Release();
    }

    return S_OK;
}

HRESULT UnregisterTSFProfiles() {
    ITfInputProcessorProfiles* pProfiles = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles, (void**)&pProfiles))) {
        pProfiles->Unregister(CLSID_BanglaTextService);
        pProfiles->Release();
    }

    ITfCategoryMgr* pCategoryMgr = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, NULL, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr, (void**)&pCategoryMgr))) {
        pCategoryMgr->UnregisterCategory(CLSID_BanglaTextService, GUID_TFCAT_TIP_KEYBOARD, CLSID_BanglaTextService);
        pCategoryMgr->UnregisterCategory(CLSID_BanglaTextService, GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT, CLSID_BanglaTextService);
        pCategoryMgr->UnregisterCategory(CLSID_BanglaTextService, GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT, CLSID_BanglaTextService);
        pCategoryMgr->UnregisterCategory(CLSID_BanglaTextService, GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER, CLSID_BanglaTextService);
        pCategoryMgr->Release();
    }

    return S_OK;
}

} // namespace bangla_tsf
