#include "../include/text_service.h"
#include "../include/bangla_tsf_clsid.h"
#include <iostream>

namespace bangla_tsf {

extern HINSTANCE g_hInstance;

TextService::TextService()
    : ref_count_(1),
      thread_mgr_(nullptr),
      client_id_(TF_CLIENTID_NULL),
      thread_mgr_sink_cookie_(TF_INVALID_COOKIE),
      composition_mgr_(this) {
}

TextService::~TextService() {
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
    if (!ptim) return E_INVALIDARG;

    thread_mgr_ = ptim;
    thread_mgr_->AddRef();
    client_id_ = tid;

    // 1. Initialize ThreadMgrEventSink
    if (!InitThreadMgrEventSink()) {
        Deactivate();
        return E_FAIL;
    }

    // 2. Initialize KeyEventSink
    if (!InitKeyEventSink()) {
        Deactivate();
        return E_FAIL;
    }

    // 3. Initialize Composition Manager
    if (!composition_mgr_.Initialize(g_hInstance)) {
        Deactivate();
        return E_FAIL;
    }

    return S_OK;
}

STDMETHODIMP TextService::Deactivate() {
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

} // namespace bangla_tsf
