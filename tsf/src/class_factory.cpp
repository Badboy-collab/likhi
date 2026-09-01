#include "../include/text_service.h"
#include "../include/bangla_tsf_clsid.h"
#include <unknwn.h>

namespace bangla_tsf {

class BanglaClassFactory : public IClassFactory {
public:
    BanglaClassFactory() : ref_count_(1) {}
    virtual ~BanglaClassFactory() {}

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override {
        if (!ppvObj) return E_INVALIDARG;
        *ppvObj = nullptr;

        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
            *ppvObj = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override {
        return InterlockedIncrement(&ref_count_);
    }

    STDMETHODIMP_(ULONG) Release() override {
        LONG count = InterlockedDecrement(&ref_count_);
        if (count == 0) {
            delete this;
        }
        return count;
    }

    // IClassFactory
    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObj) override {
        if (!ppvObj) return E_INVALIDARG;
        *ppvObj = nullptr;

        if (pUnkOuter != nullptr) {
            return CLASS_E_NOAGGREGATION;
        }

        TextService* pService = new TextService();
        if (!pService) return E_OUTOFMEMORY;

        HRESULT hr = pService->QueryInterface(riid, ppvObj);
        pService->Release();
        return hr;
    }

    STDMETHODIMP LockServer(BOOL fLock) override {
        // Handled by DLL reference counting if needed
        return S_OK;
    }

private:
    LONG ref_count_;
};

IClassFactory* CreateClassFactory() {
    return new BanglaClassFactory();
}

} // namespace bangla_tsf
