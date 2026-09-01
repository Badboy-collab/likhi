#include "../include/edit_session.h"

namespace bangla_tsf {

EditSessionBase::EditSessionBase(ITfContext* pContext)
    : ref_count_(1), context_(pContext) {
    if (context_) {
        context_->AddRef();
    }
}

EditSessionBase::~EditSessionBase() {
    if (context_) {
        context_->Release();
        context_ = nullptr;
    }
}

STDMETHODIMP EditSessionBase::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;

    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
        *ppvObj = static_cast<ITfEditSession*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) EditSessionBase::AddRef() {
    return InterlockedIncrement(&ref_count_);
}

STDMETHODIMP_(ULONG) EditSessionBase::Release() {
    LONG count = InterlockedDecrement(&ref_count_);
    if (count == 0) {
        delete this;
    }
    return count;
}

ActionEditSession::ActionEditSession(ITfContext* pContext, EditAction action)
    : EditSessionBase(pContext), action_(action) {
}

STDMETHODIMP ActionEditSession::DoEditSession(TfEditCookie ec) {
    if (action_) {
        return action_(ec);
    }
    return S_OK;
}

} // namespace bangla_tsf
