#include "../include/text_service.h"
#include "../include/bangla_tsf_clsid.h"
#include "../include/tsf_log.h"
#include <iostream>

namespace bangla_tsf {

extern HINSTANCE g_hInstance;

TextService::TextService()
    : ref_count_(1),
      thread_mgr_(nullptr),
      client_id_(TF_CLIENTID_NULL),
      thread_mgr_sink_cookie_(TF_INVALID_COOKIE),
      composition_mgr_(this) {
    TsfLog("TextService::TextService constructed");
}

TextService::~TextService() {
    TsfLog("TextService::~TextService destroyed");
    Deactivate();
}

STDMETHODIMP TextService::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_ITfTextInputProcessor)) {
        *ppvObj = static_cast<ITfTextInputProcessor*>(this);
    } else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink)) {
        *ppvObj = static_cast<ITfThreadMgrEventSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfKeyEventSink)) {
        *ppvObj = static_cast<ITfKeyEventSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfCompositionSink)) {
        *ppvObj = static_cast<ITfCompositionSink*>(this);
    } else if (IsEqualIID(riid, IID_ITfDisplayAttributeProvider)) {
        *ppvObj = static_cast<ITfDisplayAttributeProvider*>(this);
    }

    if (*ppvObj) {
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) TextService::AddRef() {
    return InterlockedIncrement(&ref_count_);
}

STDMETHODIMP_(ULONG) TextService::Release() {
    LONG count = InterlockedDecrement(&ref_count_);
    if (count == 0) {
        delete this;
    }
    return count;
}

STDMETHODIMP TextService::Activate(ITfThreadMgr* ptim, TfClientId tid) {
    TsfLog("TextService::Activate ptim=%p tid=%lu", ptim, tid);
    if (!ptim) return E_INVALIDARG;

    thread_mgr_ = ptim;
    thread_mgr_->AddRef();
    client_id_ = tid;

    // 1. Initialize ThreadMgrEventSink
    if (!InitThreadMgrEventSink()) {
        TsfLog("TextService::Activate FAIL: InitThreadMgrEventSink failed");
        Deactivate();
        return E_FAIL;
    }

    // 2. Initialize KeyEventSink
    if (!InitKeyEventSink()) {
        TsfLog("TextService::Activate FAIL: InitKeyEventSink failed");
        Deactivate();
        return E_FAIL;
    }

    // 3. Initialize Composition Manager
    if (!composition_mgr_.Initialize(g_hInstance)) {
        TsfLog("TextService::Activate FAIL: composition_mgr_.Initialize failed");
        Deactivate();
        return E_FAIL;
    }

    TsfLog("TextService::Activate SUCCESS");
    return S_OK;
}

STDMETHODIMP TextService::Deactivate() {
    TsfLog("TextService::Deactivate");
    UninitKeyEventSink();
    UninitThreadMgrEventSink();

    composition_mgr_.Shutdown();

    if (thread_mgr_) {
        thread_mgr_->Release();
        thread_mgr_ = nullptr;
    }
    client_id_ = TF_CLIENTID_NULL;

    return S_OK;
}

bool TextService::InitThreadMgrEventSink() {
    if (!thread_mgr_) return false;

    ITfSource* pSource = nullptr;
    if (FAILED(thread_mgr_->QueryInterface(IID_ITfSource, (void**)&pSource))) {
        return false;
    }

    HRESULT hr = pSource->AdviseSink(IID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink*>(this), &thread_mgr_sink_cookie_);
    pSource->Release();

    return SUCCEEDED(hr);
}

void TextService::UninitThreadMgrEventSink() {
    if (!thread_mgr_ || thread_mgr_sink_cookie_ == TF_INVALID_COOKIE) return;

    ITfSource* pSource = nullptr;
    if (SUCCEEDED(thread_mgr_->QueryInterface(IID_ITfSource, (void**)&pSource))) {
        pSource->UnadviseSink(thread_mgr_sink_cookie_);
        pSource->Release();
    }
    thread_mgr_sink_cookie_ = TF_INVALID_COOKIE;
}

bool TextService::InitKeyEventSink() {
    if (!thread_mgr_) return false;

    ITfKeystrokeMgr* pKeystrokeMgr = nullptr;
    if (FAILED(thread_mgr_->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr))) {
        return false;
    }

    HRESULT hr = pKeystrokeMgr->AdviseKeyEventSink(client_id_, static_cast<ITfKeyEventSink*>(this), TRUE);
    pKeystrokeMgr->Release();

    return SUCCEEDED(hr);
}

void TextService::UninitKeyEventSink() {
    if (!thread_mgr_) return;

    ITfKeystrokeMgr* pKeystrokeMgr = nullptr;
    if (SUCCEEDED(thread_mgr_->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr))) {
        pKeystrokeMgr->UnadviseKeyEventSink(client_id_);
        pKeystrokeMgr->Release();
    }
}

STDMETHODIMP TextService::OnInitDocumentMgr(ITfDocumentMgr* pdim) {
    return S_OK;
}

STDMETHODIMP TextService::OnUninitDocumentMgr(ITfDocumentMgr* pdim) {
    return S_OK;
}

STDMETHODIMP TextService::OnSetFocus(ITfDocumentMgr* pdimFocus, ITfDocumentMgr* pdimPrevFocus) {
    if (pdimPrevFocus) {
        composition_mgr_.OnFocusLost(nullptr);
    }
    return S_OK;
}

STDMETHODIMP TextService::OnPushContext(ITfContext* pic) {
    return S_OK;
}

STDMETHODIMP TextService::OnPopContext(ITfContext* pic) {
    return S_OK;
}

STDMETHODIMP TextService::OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition* pComposition) {
    composition_mgr_.OnCompositionTerminated(nullptr, pComposition);
    return S_OK;
}

STDMETHODIMP TextService::EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** ppEnum) {
    if (!ppEnum) return E_INVALIDARG;
    *ppEnum = nullptr;
    return S_FALSE; // No custom display attributes
}

STDMETHODIMP TextService::GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** ppInfo) {
    (void)guid;
    if (!ppInfo) return E_INVALIDARG;
    *ppInfo = nullptr;
    return TF_E_NOPROVIDER;
}

} // namespace bangla_tsf
