#include "lwindowsclipboard.h"
#include "lwindows.h"
#include "lwindowsclipcbor.h"
#include "lkcore/io/liomemoryman.h"
#include "lkgui/image/limagewriter.h"
#include "lkgui/image/limagereader.h"

LK_NAMESPACE_BEGIN
LWindowsClipEnumFmtEtc::LWindowsClipEnumFmtEtc(const std::vector<FORMATETC> &fmtetcs):
    m_nIndex(0),
    m_fmtetcs(fmtetcs),
    m_ref(1)
{
}

LWindowsClipEnumFmtEtc::~LWindowsClipEnumFmtEtc()
{
    LPMALLOC pmalloc;

    if (CoGetMalloc(MEMCTX_TASK, &pmalloc) == NOERROR) {
        for (int idx = 0; idx < m_fmtetcs.size(); ++idx) {
            FORMATETC tmpetc = m_fmtetcs.at(idx);
            if (tmpetc.ptd){
                pmalloc->Free(tmpetc.ptd);
            }
        }
        pmalloc->Release();
    }
    m_fmtetcs.clear();
}


HRESULT LWindowsClipEnumFmtEtc::QueryInterface(const IID &iid, LPVOID *iface)
{
    if (iid == IID_IUnknown){
        *iface = static_cast<IUnknown*>(this);
    }
    else if (iid == IID_IEnumFORMATETC){
        *iface = static_cast<IEnumFORMATETC*>(this);
    }

    if (!*iface){
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

ULONG LWindowsClipEnumFmtEtc::AddRef()
{
    return InterlockedIncrement(&m_ref);
}

ULONG LWindowsClipEnumFmtEtc::Release()
{
    LONG refCount = InterlockedDecrement(&m_ref);
    if (!refCount){
        delete this;
    }
    return refCount;
}


STDMETHODIMP LWindowsClipEnumFmtEtc::Next(ULONG celt, LPFORMATETC rgelt, ULONG *pceltFetched)
{
    ULONG i=0;
    ULONG nOffset;

    if (rgelt == nullptr){
        return ResultFromScode(E_INVALIDARG);
    }

    while (i < celt) {
        nOffset = m_nIndex + i;

        if (nOffset < ULONG(m_fmtetcs.size())) {
            copyFormatEtc(rgelt + i, &(m_fmtetcs.at(int(nOffset))));
            i++;
        } else {
            break;
        }
    }

    m_nIndex += i;

    if (pceltFetched != nullptr){
        *pceltFetched = i;
    }

    if (i != celt){
        return ResultFromScode(S_FALSE);
    }

    return NOERROR;
}

STDMETHODIMP LWindowsClipEnumFmtEtc::Skip(ULONG celt)
{
    ULONG i=0;
    ULONG nOffset;

    while (i < celt) {
        nOffset = m_nIndex + i;

        if (nOffset < ULONG(m_fmtetcs.size())) {
            i++;
        } else {
            break;
        }
    }

    m_nIndex += i;

    if (i != celt){
        return ResultFromScode(S_FALSE);
    }
    return NOERROR;
}

STDMETHODIMP LWindowsClipEnumFmtEtc::Reset()
{
    m_nIndex = 0;
    return NOERROR;
}

STDMETHODIMP LWindowsClipEnumFmtEtc::Clone(LPENUMFORMATETC *newEnum)
{
    if (newEnum == nullptr){
        return ResultFromScode(E_INVALIDARG);
    }

    LWindowsClipEnumFmtEtc *result = new LWindowsClipEnumFmtEtc(m_fmtetcs);
    result->m_nIndex = m_nIndex;
    *newEnum = result;
    return NOERROR;
}

bool LWindowsClipEnumFmtEtc::copyFormatEtc(LPFORMATETC dest, const FORMATETC *src) const
{
    if (dest == nullptr || src == nullptr){
        return false;
    }

    *dest = *src;

    if (src->ptd) {
        LPMALLOC pmalloc;

        if (CoGetMalloc(MEMCTX_TASK, &pmalloc) != NOERROR){
            return false;
        }

        pmalloc->Alloc(src->ptd->tdSize);
        memcpy(dest->ptd, src->ptd, size_t(src->ptd->tdSize));

        pmalloc->Release();
    }

    return true;
}


///////////////////////////

LWindowsClipboard::LWindowsClipboard() {}

LWindowsClipboard::~LWindowsClipboard()
{
    if (m_transData != nullptr) {
        if(OleIsCurrentClipboard(m_transData) == S_OK){
            OleFlushClipboard();
        }

        m_transData->Release();
        m_transData = nullptr;
    }
}

std::wstring LWindowsClipboard::text() const
{
    std::wstring ret;
    if (OpenClipboard(NULL)) {
        HANDLE hClipboardData = GetClipboardData(CF_UNICODETEXT);
        if (hClipboardData) {
            WCHAR* pchData = (WCHAR*)GlobalLock(hClipboardData);
            if (pchData) {
                ret = pchData;
                GlobalUnlock(hClipboardData);
            }
        }
        CloseClipboard();
    }
    return ret;
}

void LWindowsClipboard::setText(const std::wstring &s)
{
    releaseTransData();

    if (OpenClipboard(NULL)) {
        // Clear existing clipboard contents
        EmptyClipboard();

        // Allocate global memory for the text
        size_t len = (s.length() + 1)*sizeof(wchar_t);
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
        if(hMem != NULL){
            memcpy(GlobalLock(hMem), s.data(), len);
            GlobalUnlock(hMem);

            // Set the clipboard data
            SetClipboardData(CF_UNICODETEXT, hMem);
            // Close the clipboard
        }
        CloseClipboard();
    }
}

LCbor LWindowsClipboard::cbor(const std::string &typeName) const
{
    if(m_transData != nullptr &&  OleIsCurrentClipboard(m_transData) == S_OK){
        if(m_transData->typeName() == typeName){
            return m_transData->cbor();
        }else{
            return LCbor();
        }
    }

    LCbor ret;


    IDataObject * pDataObj = nullptr;
    if (SUCCEEDED(OleGetClipboard(&pDataObj))) {

        FORMATETC formatetc;
        formatetc.cfFormat = CLIPFORMAT(LWindowsClipCbor::formatCF());
        formatetc.dwAspect = DVASPECT_CONTENT;
        formatetc.lindex = -1;
        formatetc.ptd = nullptr;
        formatetc.tymed = TYMED_HGLOBAL;

        if (pDataObj->QueryGetData(&formatetc) == S_OK){
            STGMEDIUM s;
            if (pDataObj->GetData(&formatetc, &s) == S_OK) {
                uint8_t *val = (uint8_t*)GlobalLock(s.hGlobal);
                std::vector<uint8_t> data;
                data = std::vector<uint8_t>();

                LIOMemoryMan man(val, int(GlobalSize(s.hGlobal)), std::ios_base::in);
                std::string tName;
                man >> tName;
                if(typeName.empty() || tName == typeName){
                    man >> ret;
                }
                GlobalUnlock(s.hGlobal);
                ReleaseStgMedium(&s);
            }
        }
        pDataObj->Release();
    }
    return ret;
}

void LWindowsClipboard::setCbor(const LCbor &data, const std::string &typeName)
{
    if (OpenClipboard(NULL)) {
        // Clear existing clipboard contents
        EmptyClipboard();

        if(m_transData == nullptr){
            m_transData = createCborObject(typeName);
        }else{
            if(m_transData->typeName() != typeName){
                m_transData = createCborObject(typeName);
            }
        }

        m_transData->resetCbor(data);
        HRESULT hr = OleSetClipboard(m_transData);
        if (hr != S_OK) {
            releaseTransData();
        }

        // Close the clipboard
        CloseClipboard();
    }
}

std::shared_ptr<LImage> LWindowsClipboard::image() const
{
    std::shared_ptr<LImage> ret = nullptr;
    if (OpenClipboard(NULL)) {
        if (IsClipboardFormatAvailable(CF_DIBV5)) {
            HGLOBAL hGlobal =GetClipboardData(CF_DIBV5);
            if (hGlobal != nullptr) {
                uint8_t* pchData = (uint8_t*)GlobalLock(hGlobal);
                if (pchData != nullptr) {
                    const size_t totalSize = GlobalSize(hGlobal);
                    if(totalSize >0){
                        LIOMemoryMan io(pchData, totalSize, std::ios_base::in);
                        ret = LImageReader::loadFromDib5(io);
                    }
                    GlobalUnlock(hGlobal);
                    CloseClipboard();
                    return ret;
                }
            }
        }

        if (IsClipboardFormatAvailable(CF_DIB)) {
            HGLOBAL hGlobal =GetClipboardData(CF_DIB);
            if (hGlobal != nullptr) {
                uint8_t* pchData = (uint8_t*)GlobalLock(hGlobal);
                if (pchData != nullptr) {
                    const size_t totalSize = GlobalSize(hGlobal);
                    if(totalSize >0){
                        LIOMemoryMan io(pchData, totalSize, std::ios_base::in);
                        ret = LImageReader::loadFromDib5(io);
                    }
                    GlobalUnlock(hGlobal);
                    CloseClipboard();
                    return ret;
                }
            }
        }
        CloseClipboard();
    }
    return ret;
}

void LWindowsClipboard::setImage(const std::shared_ptr<LImage> p)
{
    releaseTransData();
    if(p != nullptr){
        if (OpenClipboard(NULL)) {
            std::vector<uint8_t> data;
            LVecMan man(data, std::ios_base::out);
            if(LImageWriter::writeToDib5(man, p)){
                // Allocate global memory for the data
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, data.size());
                if(hMem != NULL){
                    memcpy(GlobalLock(hMem), data.data(), data.size());
                    GlobalUnlock(hMem);
                    // Set the clipboard data
                    SetClipboardData(CF_DIBV5, hMem);
                    // Close the clipboard
                }
            }
            CloseClipboard();
        }
    }
}

void LWindowsClipboard::clear()
{
    releaseTransData();
}

LWindowsClipCbor *LWindowsClipboard::createCborObject(const std::string &typeName)
{
    return new LWindowsClipCbor(typeName);
}

void LWindowsClipboard::releaseTransData()
{
    if (m_transData != nullptr) {
        if(OleIsCurrentClipboard(m_transData) == S_OK){
            OleFlushClipboard();
        }

        m_transData->Release();
        m_transData = nullptr;
    }
}


LK_NAMESPACE_END
