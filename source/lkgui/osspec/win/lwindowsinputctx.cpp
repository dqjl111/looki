#include "lwindowsinputctx.h"
#include "lkgui/lguiapplication.h"
#include "lwindowswin.h"
#include "lkgui/lhighdpiscaling.h"
#include "lkgui/linputmethod.h"
#include "lkcore/lstringutil.h"
#include "lkcore/ldebug.h"
#include "lkcore/text/ltextboundaryfinder.h"
LK_NAMESPACE_BEGIN

// Cancel current IME composition.
static inline void imeNotifyCancelComposition(HWND hwnd)
{
    if (!hwnd) {
        lWarn() << __FUNCTION__ << "called with" << hwnd;
        return;
    }
    const HIMC himc = ImmGetContext(hwnd);
    ImmNotifyIME(himc, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
    ImmReleaseContext(hwnd, himc);
}

static inline LCID languageIdFromLocaleId(LCID localeId)
{
    return localeId & 0xFFFF;
}


static inline LCID currentInputLanguageId()
{
    return languageIdFromLocaleId(reinterpret_cast<luintptr>(GetKeyboardLayout(0)));
}


LWindowsInputCtx::LWindowsInputCtx():
    m_WM_MSIME_MOUSE(RegisterWindowMessageA("MSIMEMouseOperation"))
{
    m_languageId = currentInputLanguageId();
    std::string language = LStringUtil::wideToUtf8(lcidToLocaleName(m_languageId));
    std::vector<std::string> langInfos = LStringUtil::splitToVec(language, "_-.@");
    if(langInfos.size() >1){
        m_script = lchar::Util::shortNameToScript(LStringUtil::trim(langInfos[1]));
    }else{
        m_script = lchar::Script_Unknown;
    }
}

void LWindowsInputCtx::setWindowsImeEnabled(std::shared_ptr<LWindowsWin> platformWindow, bool enabled)
{
    if (platformWindow == nullptr){
        return;
    }
    if (enabled) {
        // Re-enable Windows IME by associating default context.
        ImmAssociateContextEx(platformWindow->handle(), nullptr, IACE_DEFAULT);
    } else {
        // Disable Windows IME by associating 0 context.
        ImmAssociateContext(platformWindow->handle(), nullptr);
    }
}

bool LWindowsInputCtx::startComposition(HWND hwnd)
{
    std::shared_ptr<LObject> fo = LGuiApplication::focusObject();
    if (fo == nullptr){
        return false;
    }
    // This should always match the object.
    std::shared_ptr<LWindow> window = LGuiApplication::focusWindow();
    if (window == nullptr){
        return false;
    }

    if (!fo || LWindowsWin::handleOf(window) != hwnd){
        return false;
    }
    initContext(hwnd, fo);
    startContextComposition();
    return true;

}

static inline std::wstring getCompositionString(HIMC himc, DWORD dwIndex)
{
    std::wstring ret;
    auto size = ImmGetCompositionStringW(himc, dwIndex, nullptr, 0);
    if (size > 0)
    {
        std::vector<wchar_t> buffer(size / sizeof(wchar_t)+1);
        size = ImmGetCompositionStringW(himc, dwIndex, buffer.data(), size + sizeof(wchar_t));
        buffer[size / sizeof(wchar_t)] = '\0';
        ret = buffer.data();
    }
    return ret;
}

// Determine the converted string range as pair of start/length to be selected.
static inline void getCompositionStringConvertedRange(HIMC himc, int *selStart, int *selLength)
{
    // Find the range of bytes with ATTR_TARGET_CONVERTED set.
    *selStart = *selLength = 0;
    auto attrLength = ImmGetCompositionStringW(himc, GCS_COMPATTR, nullptr, 0);
    if (attrLength > 0){
        std::vector<char> attrBuffer(attrLength);
        attrLength = ImmGetCompositionStringW(himc, GCS_COMPATTR, attrBuffer.data(), attrLength);
        int start = 0;
        while (start < attrLength && !(attrBuffer[start] & ATTR_TARGET_CONVERTED)){
            start++;
        }
        if (start < attrLength) {
            int end = start + 1;
            while (end < attrLength && (attrBuffer[end] & ATTR_TARGET_CONVERTED)){
                end++;
            }
            *selStart = start;
            *selLength = end - start;
        }
    }
}


enum StandardFormat {
    PreeditFormat,
    SelectionFormat
};

//static inline LTextCharFormat standardFormat(StandardFormat format)
static inline LTextFormat standardFormat(StandardFormat format)
{
   LTextCharFormat result;
   switch (format) {
   case PreeditFormat:
       result.setUnderlineStyle(LTextCharFormat::DashUnderline);
       break;
   case SelectionFormat: {
       const std::shared_ptr<LPalette> palette = LGuiApplication::palette();
       result.setBackground(palette->text());
       result.setForeground(palette->window());
       break;
   }
   }
   return result;
}

bool LWindowsInputCtx::composition(HWND hwnd, LPARAM lParamIn)
{
    const int lParam = int(lParamIn);

    if (m_compositionContext.focusObject.expired() || m_compositionContext.hwnd != hwnd || !lParam){
        return false;
    }
    const HIMC himc = ImmGetContext(m_compositionContext.hwnd);
    if (!himc){
        return false;
    }

    std::shared_ptr<LInputMethodEvent> event = nullptr;
    if (lParam & (GCS_COMPSTR| GCS_CURSORPOS)) {
        if (!m_compositionContext.isComposing){
            startContextComposition();
        }

        m_compositionContext.composition = getCompositionString(himc, GCS_COMPSTR);
        m_compositionContext.position = ImmGetCompositionStringW(himc, GCS_CURSORPOS, nullptr, 0);
        event = std::make_shared<LInputMethodEvent>(m_compositionContext.composition);
    }
    if (event == nullptr){
        event = std::make_shared<LInputMethodEvent>();
    }

    if (lParam & GCS_RESULTSTR) {
        // A fixed result, return the converted string
        event->setCommitString(getCompositionString(himc, GCS_RESULTSTR));
        if (!(lParam & GCS_DELTASTART)){
            endContextComposition();
        }
    }

    std::shared_ptr<LObject> fo = m_compositionContext.focusObject.lock();
    bool result = LCoreApplication::sendEvent(fo, event);
    update(LInputMethodQuery::ImQueryAll);
    ImmReleaseContext(m_compositionContext.hwnd, himc);
    return result;
}

bool LWindowsInputCtx::endComposition(HWND hwnd)
{
    if (m_endCompositionRecursionGuard || m_compositionContext.hwnd != hwnd){
        return false;
    }

    std::shared_ptr<LObject> focusObj = m_compositionContext.focusObject.lock();
    if (focusObj == nullptr){
        return false;
    }

    m_endCompositionRecursionGuard = true;

    imeNotifyCancelComposition(m_compositionContext.hwnd);
    if (m_compositionContext.isComposing) {
        std::shared_ptr<LInputMethodEvent> event = std::make_shared<LInputMethodEvent>();
        LCoreApplication::sendEvent(focusObj, event);
    }
    doneContext();

    m_endCompositionRecursionGuard = false;
    return true;
}

void LWindowsInputCtx::update(LFlags<LInputMethodQuery> queries)
{
    if (queries & LInputMethodQuery::ImEnabled){
        updateEnabled();
    }
}

void LWindowsInputCtx::cursorRectChanged()
{
    std::shared_ptr<LWindow> window = LGuiApplication::focusWindow();
    if (window == nullptr){
        return;
    }

    double factor = LHighDpiScaling::factor(window.get());

    const std::shared_ptr<LInputMethod> inputMethod = LGuiApplication::inputMethod();
    const LRect<double> cursorRectangleF = inputMethod->cursorRectangle()*factor;
    if (!cursorRectangleF.isValid()){
        return;
    }

    const LRect<int> cursorRectangle(std::lround(cursorRectangleF.m_x),
                                     std::lround(cursorRectangleF.m_y),
                                     LSize<int>(std::lround(cursorRectangleF.m_width),
                                                std::lround(cursorRectangleF.m_height)));

    if (m_caretCreated){
        SetCaretPos(cursorRectangle.m_x, cursorRectangle.m_y);
    }

    if (!m_compositionContext.hwnd){
        return;
    }

    const HIMC himc = ImmGetContext(m_compositionContext.hwnd);
    if (!himc){
        return;
    }
    // Move candidate list window to the microfocus position.
    COMPOSITIONFORM cf;
    // ### need X-like inputStyle config settings
    cf.dwStyle = CFS_FORCE_POSITION;
    cf.ptCurrentPos.x = cursorRectangle.m_x;
    cf.ptCurrentPos.y = cursorRectangle.m_y;

    CANDIDATEFORM candf;
    candf.dwIndex = 0;
    candf.dwStyle = CFS_EXCLUDE;
    candf.ptCurrentPos.x = cursorRectangle.m_x;
    candf.ptCurrentPos.y = cursorRectangle.y2();
    candf.rcArea.left = cursorRectangle.m_x;
    candf.rcArea.top = cursorRectangle.m_y;
    candf.rcArea.right = cursorRectangle.x2();
    candf.rcArea.bottom = cursorRectangle.y2();

    ImmSetCompositionWindow(himc, &cf);
    ImmSetCandidateWindow(himc, &candf);
    ImmReleaseContext(m_compositionContext.hwnd, himc);

}

int LWindowsInputCtx::reconvertString(RECONVERTSTRING *reconv)
{
    std::shared_ptr<LObject>  fo = LGuiApplication::focusObject();
    if (fo==nullptr){
        return false;
    }

    const LVariant surroundingTextV = LInputMethod::queryFocusObject(LInputMethodQuery::ImSurroundingText, LVariant());
    if (surroundingTextV.isNull()){
        return -1;
    }
    const std::wstring surroundingText = surroundingTextV.toWString();
    const int memSize = int(sizeof(RECONVERTSTRING))
                        + (surroundingText.length() + 1) * int(sizeof(uint16_t));

    // If memory is not allocated, return the required size.
    if (reconv == nullptr){ //第一次请求，根据字符串长度，返回系统分配内存
        return surroundingText.empty() ? -1 : memSize;
    }

    const LVariant posV = LInputMethod::queryFocusObject(LInputMethodQuery::ImCursorPosition, LVariant());
    const int pos = posV.getInt32();
    // Find the word in the surrounding text.
    LTextBoundaryFinder bounds(LTextBoundaryFinder::Word, surroundingText);
    bounds.setPosition(pos);
    if (bounds.position() > 0 && !(bounds.boundaryReasons() & LTextBoundaryReason::StartOfItem)){
        bounds.toPreviousBoundary();
    }
    const int startPos = bounds.position();
    bounds.toNextBoundary();
    const int endPos = bounds.position();

    reconv->dwSize = DWORD(memSize);
    reconv->dwVersion = 0;

    reconv->dwStrLen = DWORD(surroundingText.size());
    reconv->dwStrOffset = sizeof(RECONVERTSTRING);
    reconv->dwCompStrLen = DWORD(endPos - startPos); // TCHAR count.
    reconv->dwCompStrOffset = DWORD(startPos) * sizeof(uint16_t); // byte count.
    reconv->dwTargetStrLen = reconv->dwCompStrLen;
    reconv->dwTargetStrOffset = reconv->dwCompStrOffset;

    uint16_t *pastReconv = reinterpret_cast<uint16_t *>(reconv + 1);
    std::copy(surroundingText.data(), surroundingText.data() + surroundingText.size(), pastReconv);
    return memSize;
}

bool LWindowsInputCtx::handleIMERequest(WPARAM wparam, LPARAM lparam, LRESULT *result)
{
    switch (int(wparam)) {
    case IMR_RECONVERTSTRING: {
        const int size = reconvertString(reinterpret_cast<RECONVERTSTRING *>(lparam));
        if (size < 0){
            return false;
        }
        *result = size;
        return true;
    }
    case IMR_CONFIRMRECONVERTSTRING:{
        return true;
    }
    default:{
        break;
    }
    }
    return false;

}

void LWindowsInputCtx::handleInputLanguageChanged(WPARAM wparam, LPARAM lparam)
{
    const LCID newLanguageId = languageIdFromLocaleId(WORD(lparam));
    if (newLanguageId == m_languageId){
        return;
    }
    m_languageId = newLanguageId;
    handleLocaleChanged();

}

void LWindowsInputCtx::setFocusObject(std::shared_ptr<LObject> object)
{
    if (m_compositionContext.isComposing){
        reset();
    }
    updateEnabled();
}

void LWindowsInputCtx::reset()
{
    if (!m_compositionContext.hwnd){
        return;
    }

    if (m_compositionContext.isComposing) {
        std::shared_ptr<LObject> focusObject = m_compositionContext.focusObject.lock();
        if(focusObject != nullptr){
            std::shared_ptr<LInputMethodEvent> event = std::make_shared<LInputMethodEvent>();

            if (!m_compositionContext.composition.empty()){
                event->setCommitString(m_compositionContext.composition);
            }
            LCoreApplication::sendEvent(focusObject, event);
            endContextComposition();
        }
    }
    imeNotifyCancelComposition(m_compositionContext.hwnd);
    doneContext();

}

void LWindowsInputCtx::showInputPanel()
{
    lWarn() << "showInputPanel";
    if (!inputMethodAccepted()){
        return;
    }

    std::shared_ptr<LWindow> window = LGuiApplication::focusWindow();
    if (window == nullptr){
        return;
    }

    std::shared_ptr<LWindowsWin> platformWindow = LWindowsWin::windowsWindowOf(window);
    if (platformWindow == nullptr){
        return;
    }

    // Create an invisible 2x2 caret, which will be kept at the microfocus position.
    // It is important for triggering the on-screen keyboard in touch-screen devices,
    // for some Chinese input methods, and for Magnifier's "follow keyboard" feature.
    if (!m_caretCreated && m_transparentBitmap){
        m_caretCreated = CreateCaret(platformWindow->handle(), m_transparentBitmap, 0, 0);
    }

    if (m_caretCreated) {
        cursorRectChanged();
        ShowCaret(platformWindow->handle());
    }
}

void LWindowsInputCtx::hideInputPanel()
{
    if (m_caretCreated) {
        DestroyCaret();
        m_caretCreated = false;
    }
}

std::string LWindowsInputCtx::localeLanguage() const
{
    return LStringUtil::wideToUtf8(lcidToLocaleName(m_languageId));
}

void LWindowsInputCtx::invokeAction(LInputMethod::Action action, int cursorPosition)
{
    if (action != LInputMethod::Click || !m_compositionContext.hwnd) {
        LPlatformInputCtx::invokeAction(action, cursorPosition);
        return;
    }

    if (cursorPosition < 0 || cursorPosition > m_compositionContext.composition.size()){
        reset();
    }

    // Magic code that notifies IME about the cursor
    // position.
    const HIMC himc = ImmGetContext(m_compositionContext.hwnd);
    const HWND imeWindow = ImmGetDefaultIMEWnd(m_compositionContext.hwnd);
    const WPARAM mouseOperationCode =  MAKELONG(MAKEWORD(MK_LBUTTON, cursorPosition == 0 ? 2 : 1), cursorPosition);
    SendMessage(imeWindow, m_WM_MSIME_MOUSE, mouseOperationCode, LPARAM(himc));
    ImmReleaseContext(m_compositionContext.hwnd, himc);
}

std::wstring LWindowsInputCtx::lcidToLocaleName(LCID id)
{
    WCHAR strNameBuffer[LOCALE_NAME_MAX_LENGTH];
    DWORD error = ERROR_SUCCESS;
    if (LCIDToLocaleName(id, strNameBuffer, LOCALE_NAME_MAX_LENGTH, 0) == 0)
    {
        lWarn() << "LCID "<< uint32_t(id) << " to locale name error: " << GetLastError();
        return L"";
    }
    return strNameBuffer;
}

LCID LWindowsInputCtx::localeNameToLcid(const std::wstring &name)
{
    // Get the LCID for the locale
    LCID lcid = LocaleNameToLCID(name.data(), 0);
    if (lcid == 0)
    {
        lWarn() << "Local name "<< name << " to lcid error: " << GetLastError();
    }
    return lcid;
}


void LWindowsInputCtx::initContext(HWND hwnd, std::shared_ptr<LObject> focusObject)
{
    if (m_compositionContext.hwnd){
        doneContext();
    }
    m_compositionContext.hwnd = hwnd;
    m_compositionContext.focusObject = focusObject;

    update(LInputMethodQuery::ImQueryAll);
    m_compositionContext.isComposing = false;
    m_compositionContext.position = 0;
}

void LWindowsInputCtx::doneContext()
{
    if (!m_compositionContext.hwnd){
        return;
    }
    m_compositionContext.hwnd = nullptr;
    m_compositionContext.composition.clear();
    m_compositionContext.position = 0;
    m_compositionContext.isComposing = false;
    m_compositionContext.focusObject.reset();
}

void LWindowsInputCtx::startContextComposition()
{
    if (m_compositionContext.isComposing) {
        return;
    }
    m_compositionContext.isComposing = true;
    m_compositionContext.composition.clear();
    m_compositionContext.position = 0;
    cursorRectChanged(); // position cursor initially.
    update(LInputMethodQuery::ImQueryAll);
}

void LWindowsInputCtx::endContextComposition()
{
    if (!m_compositionContext.isComposing) {
        return;
    }
    m_compositionContext.composition.clear();
    m_compositionContext.position = 0;
    m_compositionContext.isComposing = false;
}

void LWindowsInputCtx::updateEnabled()
{
    std::shared_ptr<LWindow> focusWin = LGuiApplication::focusWindow();
    if(focusWin != nullptr){
        std::shared_ptr<LObject> focusObj = focusWin->focusObject();
        if(focusObj != nullptr){
            std::shared_ptr<LWindowsWin> platformWindow= LWindowsWin::windowsWindowOf(focusWin);
            if (platformWindow!= nullptr) {
                LWindowsInputCtx::setWindowsImeEnabled(platformWindow, m_inputMethodAccepted);
            }
        }
    }
}

LK_NAMESPACE_END
