#include "lwindowsclipcbor.h"
#include "lkcore/io/liomemoryman.h"
#include "lkgui/osspec/win/lwindowsclipboard.h"
#include <shlobj.h>

LK_NAMESPACE_BEGIN

LWindowsClipCbor::LWindowsClipCbor(const std::string& typeName):
    m_typeName(typeName),
    m_ref(1)
{
}

LWindowsClipCbor::~LWindowsClipCbor()
{
    releaseCbor();
}

const uint32_t LWindowsClipCbor::formatCF()
{
    static UINT cborCF = RegisterClipboardFormatA("data/Looki Cbor");
    return cborCF;
}

void LWindowsClipCbor::resetCbor(const LCbor& t)
{
    m_data = t;
}

void LWindowsClipCbor::releaseCbor()
{
    m_data = LCbor();
}

const LCbor &LWindowsClipCbor::cbor() const
{
    return m_data;
}

const std::string &LWindowsClipCbor::typeName() const
{
    return m_typeName;
}


DWORD LWindowsClipCbor::reportedPerformedEffect() const
{
    return m_performedEffect;
}

HRESULT LWindowsClipCbor::QueryInterface(const IID &iid, LPVOID *iface)
{
    if (iid == IID_IUnknown){
        *iface = static_cast<IUnknown*>(this);
    }
    else if (iid == IID_IDataObject){
        *iface = static_cast<IDataObject*>(this);
    }

    if (!*iface){
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

ULONG LWindowsClipCbor::AddRef()
{
    return InterlockedIncrement(&m_ref);
}

ULONG LWindowsClipCbor::Release()
{
    LONG refCount = InterlockedDecrement(&m_ref);
    if (!refCount){
        delete this;
    }
    return refCount;
}

STDMETHODIMP LWindowsClipCbor::GetData(FORMATETC *pformatetc, STGMEDIUM *pmedium)
{
    if ( !pformatetc ){
        return DV_E_FORMATETC;
    }

    if ( !pmedium ){
        return STG_E_MEDIUMFULL;
    }


    if ( pformatetc->dwAspect == DVASPECT_THUMBNAIL
        || pformatetc->dwAspect == DVASPECT_ICON
        || pformatetc->dwAspect == DVASPECT_DOCPRINT ){
        return DV_E_DVASPECT;
    }

    HRESULT hr = ResultFromScode(DATA_E_FORMATETC);
    if (pformatetc->cfFormat == formatCF()) {
        if (m_data.isValid()) {
            std::vector<uint8_t> data;
            LVecMan man(data, std::ios_base::out);
            man << m_typeName << m_data;

            HGLOBAL hData = GlobalAlloc(0, SIZE_T(data.size()));
            if (!hData){
                return false;
            }
            void *out = GlobalLock(hData);
            memcpy(out, data.data(), size_t(data.size()));
            GlobalUnlock(hData);
            pmedium->tymed = TYMED_HGLOBAL;
            pmedium->hGlobal = hData;
            pmedium->pUnkForRelease = nullptr;

            hr = true;
        }
    }
    return hr;
}

STDMETHODIMP LWindowsClipCbor::GetDataHere(FORMATETC *pformatetc, STGMEDIUM *pmedium)
{
    if ( !pformatetc ){
        return DV_E_FORMATETC;
    }

    if ( !pmedium ){
        return STG_E_MEDIUMFULL;
    }
    return DV_E_FORMATETC;
}

STDMETHODIMP LWindowsClipCbor::QueryGetData(FORMATETC *pformatetc)
{
    if ( pformatetc )
    {
        if ( pformatetc->dwAspect == DVASPECT_THUMBNAIL
            || pformatetc->dwAspect == DVASPECT_ICON
            || pformatetc->dwAspect == DVASPECT_DOCPRINT ){
            return DV_E_DVASPECT;
        }

        if(pformatetc->cfFormat == formatCF()){
            if ( !( pformatetc->tymed & TYMED_HGLOBAL ) ){
                return DV_E_TYMED;
            }
            return S_OK;
        }
    }
    return DV_E_FORMATETC;
}

STDMETHODIMP LWindowsClipCbor::GetCanonicalFormatEtc(FORMATETC *pformatetc, FORMATETC *pformatetcOut)
{
    if ( !pformatetc || !pformatetcOut ){
        return DV_E_FORMATETC;
    }

    pformatetcOut->ptd = nullptr;
    pformatetcOut->cfFormat = pformatetc->cfFormat;
    pformatetcOut->dwAspect = DVASPECT_CONTENT;

    if(pformatetc->cfFormat == formatCF()){
        pformatetcOut->tymed = TYMED_HGLOBAL;
        return S_OK;
    }

    return DV_E_FORMATETC;
}

STDMETHODIMP LWindowsClipCbor::SetData(FORMATETC* pformatetc, STGMEDIUM* pmedium, BOOL fRelease)
{
    HRESULT hr = ResultFromScode(E_NOTIMPL);

    if (pformatetc->cfFormat == formatCF() && pmedium->tymed == TYMED_HGLOBAL) {
        DWORD * val = (DWORD*)GlobalLock(pmedium->hGlobal);
        m_performedEffect = *val;
        GlobalUnlock(pmedium->hGlobal);
        if (fRelease){
            ReleaseStgMedium(pmedium);
        }
        hr = ResultFromScode(S_OK);
    }
    return hr;
}

STDMETHODIMP LWindowsClipCbor::EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc)
{
    if(!ppenumFormatEtc){
        return E_INVALIDARG;
    }

    if (!m_data.isValid()){
        return ResultFromScode(DATA_E_FORMATETC);
    }

    std::vector<FORMATETC> fmtetcs;
    FORMATETC formatetc;
    formatetc.cfFormat = CLIPFORMAT(formatCF());
    formatetc.dwAspect = DVASPECT_CONTENT;
    formatetc.lindex = -1;
    formatetc.ptd = nullptr;
    formatetc.tymed = TYMED_HGLOBAL;
    fmtetcs.push_back(formatetc);

    if (dwDirection == DATADIR_GET) {
        //Todo add format support, like text,json
    }

    *ppenumFormatEtc = new LWindowsClipEnumFmtEtc(fmtetcs);
    return S_OK;
}

STDMETHODIMP LWindowsClipCbor::DAdvise(FORMATETC FAR*, DWORD, LPADVISESINK, DWORD FAR*)
{
    return ResultFromScode(OLE_E_ADVISENOTSUPPORTED);
}


STDMETHODIMP LWindowsClipCbor::DUnadvise(DWORD)
{
    return ResultFromScode(OLE_E_ADVISENOTSUPPORTED);
}

STDMETHODIMP LWindowsClipCbor::EnumDAdvise(LPENUMSTATDATA FAR*)
{
    return ResultFromScode(OLE_E_ADVISENOTSUPPORTED);
}


LK_NAMESPACE_END
