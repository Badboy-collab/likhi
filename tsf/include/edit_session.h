#ifndef BANGLA_EDIT_SESSION_H
#define BANGLA_EDIT_SESSION_H

#include <windows.h>
#include <msctf.h>
#include <string>
#include <functional>

namespace bangla_tsf {

class EditSessionBase : public ITfEditSession {
public:
    EditSessionBase(ITfContext* pContext);
    virtual ~EditSessionBase();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

protected:
    LONG ref_count_;
    ITfContext* context_;
};

// Generic lambda / callback edit session
class ActionEditSession : public EditSessionBase {
public:
    using EditAction = std::function<HRESULT(TfEditCookie ec)>;

    ActionEditSession(ITfContext* pContext, EditAction action);

    // ITfEditSession
    STDMETHODIMP DoEditSession(TfEditCookie ec) override;

private:
    EditAction action_;
};

} // namespace bangla_tsf

#endif // BANGLA_EDIT_SESSION_H
