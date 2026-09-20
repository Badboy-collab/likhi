#include "../include/bangla_tsf_clsid.h"
#include "../include/tsf_log.h"
#include <windows.h>
#include <olectl.h>

namespace bangla_tsf {
    IClassFactory* CreateClassFactory();
    HRESULT RegisterCOMServer(HINSTANCE hInst);
    HRESULT UnregisterCOMServer();
    HRESULT RegisterTSFProfiles(HINSTANCE hInst);
    HRESULT UnregisterTSFProfiles();

    HINSTANCE g_hInstance = NULL;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    (void)lpvReserved;
    switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
            bangla_tsf::g_hInstance = hinstDLL;
            DisableThreadLibraryCalls(hinstDLL);
            bangla_tsf::TsfLog("DLL_PROCESS_ATTACH hinst=%p bitness=%d", hinstDLL, (int)(sizeof(void*) * 8));
            break;
        case DLL_PROCESS_DETACH:
            bangla_tsf::TsfLog("DLL_PROCESS_DETACH");
            break;
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID* ppv) {
    bangla_tsf::TsfLog("DllGetClassObject");
    if (!ppv) return E_INVALIDARG;
    *ppv = nullptr;

    if (IsEqualCLSID(rclsid, CLSID_BanglaTextService)) {
        IClassFactory* pFactory = bangla_tsf::CreateClassFactory();
        if (!pFactory) return E_OUTOFMEMORY;

        HRESULT hr = pFactory->QueryInterface(riid, ppv);
        pFactory->Release();
        bangla_tsf::TsfLog("DllGetClassObject factory QueryInterface hr=0x%08X", hr);
        return hr;
    }

    return CLASS_E_CLASSNOTAVAILABLE;
}

STDAPI DllCanUnloadNow(void) {
    return S_OK;
}

STDAPI DllRegisterServer(void) {
    HRESULT hr = bangla_tsf::RegisterCOMServer(bangla_tsf::g_hInstance);
    if (FAILED(hr)) return hr;

    hr = bangla_tsf::RegisterTSFProfiles(bangla_tsf::g_hInstance);
    return hr;
}

STDAPI DllUnregisterServer(void) {
    bangla_tsf::UnregisterTSFProfiles();
    bangla_tsf::UnregisterCOMServer();
    return S_OK;
}
