#ifndef LWINDOWSCLIPBOARD_H
#define LWINDOWSCLIPBOARD_H

#include "lkgui/lplatformclipboard.h"
#include "lkgui/osspec/win/lwindows.h"
#include <objidl.h>

LK_NAMESPACE_BEGIN

class LWindowsClipEnumFmtEtc : public IEnumFORMATETC
{
public:
    explicit LWindowsClipEnumFmtEtc(const std::vector<FORMATETC> &fmtetcs);
    virtual ~LWindowsClipEnumFmtEtc();

    // IUnknown
    HRESULT WINAPI QueryInterface(REFIID iid, LPVOID *iface);
    ULONG WINAPI AddRef();
    ULONG WINAPI Release();

    // IEnumFORMATETC methods
    STDMETHOD(Next)(ULONG celt, LPFORMATETC rgelt, ULONG FAR* pceltFetched);
    STDMETHOD(Skip)(ULONG celt);
    STDMETHOD(Reset)(void);
    STDMETHOD(Clone)(LPENUMFORMATETC FAR* newEnum);

private:
    bool copyFormatEtc(LPFORMATETC dest, const FORMATETC *src) const;

    ULONG m_nIndex = 0;
    std::vector<FORMATETC> m_fmtetcs;
    LONG m_ref=1;
};


class LWindowsClipCbor;
class LWindowsClipboard  : public LPlatformClipboard
{
public:
    LWindowsClipboard();
    virtual ~LWindowsClipboard();

    virtual std::wstring text() const override;
    virtual void setText(const std::wstring &) override;

    virtual LCbor cbor(const std::string &typeName = "") const override;
    virtual void setCbor(const LCbor &c, const std::string &typeName = "") override;

    virtual std::shared_ptr<LImage> image() const override;
    virtual void setImage(const std::shared_ptr<LImage>) override;
    virtual void clear() override;
    virtual LWindowsClipCbor* createCborObject(const std::string &typeName); //继承修改，如果想创建自己的Ole Clip类型
protected:
    LWindowsClipCbor *m_transData = nullptr;
    void releaseTransData();
};

LK_NAMESPACE_END

#endif // LWINDOWSCLIPBOARD_H
