#ifndef LWINDOWSCLIPCBOR_H
#define LWINDOWSCLIPCBOR_H

#include "lwindows.h"
#include "lkcore/io/lcbor.h"
#include <map>
#include <objidl.h>

LK_NAMESPACE_BEGIN

class LWindowsClipCbor : public IDataObject
{
public:
    LWindowsClipCbor(const std::string& typeName);
    virtual ~LWindowsClipCbor();

    static const uint32_t formatCF();

    void resetCbor(const LCbor& data);
    void releaseCbor();
    const LCbor& cbor() const;
    const std::string& typeName() const;
    DWORD reportedPerformedEffect() const;
    // IUnknown
    HRESULT WINAPI QueryInterface(REFIID iid, LPVOID *iface);
    ULONG WINAPI AddRef();
    ULONG WINAPI Release();

    // IDataObject
    STDMETHOD(GetData)(FORMATETC *pformatetc, STGMEDIUM *pmedium);
    STDMETHOD(GetDataHere)(FORMATETC* pformatetc, STGMEDIUM* pmedium);
    STDMETHOD(QueryGetData)(FORMATETC* pformatetc);
    STDMETHOD(GetCanonicalFormatEtc)(FORMATETC* pformatetc,FORMATETC* pformatetcOut);
    STDMETHOD(SetData)(FORMATETC* pformatetc, STGMEDIUM* pmedium, BOOL fRelease);
    STDMETHOD(EnumFormatEtc)(DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc);
    STDMETHOD(DAdvise)(FORMATETC *pformatetc, DWORD advf, IAdviseSink *pAdvSink, DWORD *pdwConnection);
    STDMETHOD(DUnadvise)(DWORD dwConnection);
    STDMETHOD(EnumDAdvise)(IEnumSTATDATA **ppenumAdvise);

private:
    LCbor m_data;
    const std::string m_typeName;
    DWORD m_performedEffect = DROPEFFECT_NONE;
    LONG m_ref=1;
};




LK_NAMESPACE_END

#endif // LWINDOWSCLIPCBOR_H
