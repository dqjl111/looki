#include <stdlib.h>
#include <stdio.h>
#include <windowsx.h>
#include <comdef.h>
#include <dbt.h>
#include <wtsapi32.h>

#include "lwindowsctx.h"
#include "lkcore/ldebug.h"
#include "lkcore/luuid.h"
#include "lkgui/lmargins.h"
#include "lwindowswin.h"
#include "lkcore/leventdispatcher.h"
#include "lkgui/lwindowsysteminterface.h"
#include "lwindowsintegration.h"
#include "lkcore/lopersysversion.h"
#include "lkgui/osspec/win/lwindowsinputctx.h"
#include "lkcore/ldebug.h"
#include "lkgui/lguiapplication.h"
#include "lkcore/osspec/win/lwinregistry.h"
#include "lkcore/plugin/ldynamiclibrary.h"

LK_NAMESPACE_BEGIN
static inline bool isEmptyRect(const RECT &rect)
{
    return rect.right - rect.left == 0 && rect.bottom - rect.top == 0;
}

static inline LMargins<int> marginsFromRects(const RECT &frame, const RECT &client)
{
    return LMargins<int>(client.left - frame.left, client.top - frame.top,
                    frame.right - client.right, frame.bottom - client.bottom);
}

static RECT rectFromNcCalcSize(UINT message, WPARAM wParam, LPARAM lParam, int n)
{
    RECT result = {0, 0, 0, 0};
    if (message == WM_NCCALCSIZE && wParam){
        result = reinterpret_cast<const NCCALCSIZE_PARAMS *>(lParam)->rgrc[n];
    }
    return result;
}

static inline bool isInputMessage(UINT m)
{
    switch (m) {
    case WM_IME_STARTCOMPOSITION:
    case WM_IME_ENDCOMPOSITION:
    case WM_IME_COMPOSITION:
    case WM_INPUT:
    case WM_TOUCH:
    case WM_MOUSEHOVER:
    case WM_MOUSELEAVE:
    case WM_NCMOUSEHOVER:
    case WM_NCMOUSELEAVE:
    case WM_SIZING:
    case WM_MOVING:
    case WM_SYSCOMMAND:
    case WM_COMMAND:
    case WM_DWMNCRENDERINGCHANGED:
    case WM_PAINT:{
        return true;
    }
    default:{
        break;
    }
    }
    return (m >= WM_MOUSEFIRST && m <= WM_MOUSELAST)
           || (m >= WM_NCMOUSEMOVE && m <= WM_NCXBUTTONDBLCLK)
           || (m >= WM_KEYFIRST && m <= WM_KEYLAST);
}


static inline bool isMinimized(HWND hwnd)
{
    WINDOWPLACEMENT windowPlacement;
    windowPlacement.length = sizeof(WINDOWPLACEMENT);
    return GetWindowPlacement(hwnd, &windowPlacement) && windowPlacement.showCmd == SW_SHOWMINIMIZED;
}

static inline bool isTopLevel(HWND hwnd)
{
    return (GetWindowLongPtr(hwnd, GWL_STYLE) & WS_CHILD) == 0;
}


static inline std::shared_ptr<LWindowsInputCtx> windowsInputContext()
{
    return std::dynamic_pointer_cast<LWindowsInputCtx>(LWindowsIntegration::instance()->inputContext());
}



void LWindowsUser32DLL::init()
{
    std::shared_ptr<LDynamicLibrary> library = LDynamicLibrary::load("user32", LDynamicLibrary::SystemPath);
    if(library != nullptr){
        setProcessDPIAware = (SetProcessDPIAware)library->resolve("SetProcessDPIAware");

        addClipboardFormatListener = (AddClipboardFormatListener)library->resolve("AddClipboardFormatListener");
        removeClipboardFormatListener = (RemoveClipboardFormatListener)library->resolve("RemoveClipboardFormatListener");

        getDisplayAutoRotationPreferences = (GetDisplayAutoRotationPreferences)library->resolve("GetDisplayAutoRotationPreferences");
        setDisplayAutoRotationPreferences = (SetDisplayAutoRotationPreferences)library->resolve("SetDisplayAutoRotationPreferences");

        //Win8 Funcs
        enableMouseInPointer = (EnableMouseInPointer)library->resolve("EnableMouseInPointer");
        getPointerType = (GetPointerType)library->resolve("GetPointerType");
        getPointerInfo = (GetPointerInfo)library->resolve("GetPointerInfo");
        getPointerDeviceRects = (GetPointerDeviceRects)library->resolve("GetPointerDeviceRects");
        getPointerTouchInfo = (GetPointerTouchInfo)library->resolve("GetPointerTouchInfo");
        getPointerFrameTouchInfo = (GetPointerFrameTouchInfo)library->resolve("GetPointerFrameTouchInfo");
        getPointerFrameTouchInfoHistory = (GetPointerFrameTouchInfoHistory)library->resolve("GetPointerFrameTouchInfoHistory");
        getPointerPenInfo = (GetPointerPenInfo)library->resolve("GetPointerPenInfo");
        getPointerPenInfoHistory = (GetPointerPenInfoHistory)library->resolve("GetPointerPenInfoHistory");
        skipPointerFrameMessages = (SkipPointerFrameMessages)library->resolve("SkipPointerFrameMessages");

        //Win10 Funcs
        adjustWindowRectExForDpi = (AdjustWindowRectExForDpi)library->resolve("AdjustWindowRectExForDpi");
        enableNonClientDpiScaling = (EnableNonClientDpiScaling)library->resolve("EnableNonClientDpiScaling");
        getWindowDpiAwarenessContext = (GetWindowDpiAwarenessContext)library->resolve("GetWindowDpiAwarenessContext");
        getAwarenessFromDpiAwarenessContext = (GetAwarenessFromDpiAwarenessContext)library->resolve("GetAwarenessFromDpiAwarenessContext");
        systemParametersInfoForDpi = (SystemParametersInfoForDpi)library->resolve("SystemParametersInfoForDpi");
    }
}

bool LWindowsUser32DLL::supportsPointerApi()
{
    return enableMouseInPointer && getPointerType && getPointerInfo && getPointerDeviceRects
           && getPointerTouchInfo && getPointerFrameTouchInfo && getPointerFrameTouchInfoHistory
           && getPointerPenInfo && getPointerPenInfoHistory && skipPointerFrameMessages;
}

void LWindowsShcoreDLL::init()
{
    //Win8 Funcs
    std::shared_ptr<LDynamicLibrary> library = LDynamicLibrary::load("SHCore", LDynamicLibrary::SystemPath);
    if(library != nullptr){
        getProcessDpiAwareness = (GetProcessDpiAwareness)library->resolve("GetProcessDpiAwareness");
        setProcessDpiAwareness = (SetProcessDpiAwareness)library->resolve("SetProcessDpiAwareness");
        getDpiForMonitor = (GetDpiForMonitor)library->resolve("GetDpiForMonitor");
    }
}

bool LWindowsShcoreDLL::isValid() const
{
    return getProcessDpiAwareness && setProcessDpiAwareness && getDpiForMonitor;
}



LWindowsUser32DLL LWindowsCtx::user32dll;
LWindowsShcoreDLL LWindowsCtx::shcoredll;

LWindowsCtx *LWindowsCtx::m_instance = nullptr;


static inline int windowDpiAwareness(HWND hwnd)
{
    return LWindowsCtx::user32dll.getWindowDpiAwarenessContext && LWindowsCtx::user32dll.getAwarenessFromDpiAwarenessContext
               ? LWindowsCtx::user32dll.getAwarenessFromDpiAwarenessContext(LWindowsCtx::user32dll.getWindowDpiAwarenessContext(hwnd))
               : -1;
}

// Note: This only works within WM_NCCREATE
static bool enableNonClientDpiScaling(HWND hwnd)
{
    bool result = false;
    if ((LWindowsCtx::user32dll.enableNonClientDpiScaling!= nullptr) && windowDpiAwareness(hwnd) == 2) {
        result = LWindowsCtx::user32dll.enableNonClientDpiScaling(hwnd) != FALSE;
        if (!result) {
            const DWORD errorCode = GetLastError();
            lWarn()<<"EnableNonClientDpiScaling() failed:" << errorCode;
        }
    }
    return result;
}

static inline bool resizeOnDpiChanged(const std::shared_ptr<LWindow> w)
{
    bool result = false;
    if (w->isTopLevel()) {
        switch (w->type()) {
        case LWindowType::Window:
        case LWindowType::Dialog:
        case LWindowType::Sheet:
        case LWindowType::Drawer:
        case LWindowType::Tool:{
            result = !w->flags().testFlag(LWindowType::MSWindowsFixedSizeDialogHint);
            break;
        }
        default:{
            break;
        }
        }
    }
    return result;
}


LWindowsCtx::LWindowsCtx()
: m_oleInitializeResult(OleInitialize(nullptr))
{
    LWindowsCtx::user32dll.init();
    LWindowsCtx::shcoredll.init();

    m_displayContext = GetDC(nullptr);
    m_defaultDPI = GetDeviceCaps(m_displayContext, LOGPIXELSY);
    if (FAILED(m_oleInitializeResult)) {
        lWarn() << "LWinCtx: OleInitialize() failed: "
                   << LWindowsCtx::comErrorString(m_oleInitializeResult);
    }
    m_instance = this;
}

LWindowsCtx::~LWindowsCtx()
{
    if (m_powerNotification){
        UnregisterPowerSettingNotification(m_powerNotification);
    }

    if (m_powerDummyWindow){
        DestroyWindow(m_powerDummyWindow);
    }

    unregisterWindowClasses();
    if (m_oleInitializeResult == S_OK || m_oleInitializeResult == S_FALSE){
        OleUninitialize();
    }

    //m_screenManager.clearScreens(); // Order: Potentially calls back to the windows.
    if (m_displayContext){
        ReleaseDC(nullptr, m_displayContext);
    }
    m_instance = nullptr;
}

std::string LWindowsCtx::registerWindowClass(const  std::shared_ptr<LWindow>  w)
{
    const LFlags<LWindowType> flags = w->flags();
    const uint32_t type = (flags & LWindowType::WindowTypeMask).value();
    // Determine style and icon.
    unsigned int style = CS_DBLCLKS;
    bool icon = true;
    // The following will not set CS_OWNDC for any widget window, even if it contains a
    if (flags & LWindowType::MSWindowsOwnDC){
        style |= CS_OWNDC;
    }
    if (!(flags & LWindowType::NoDropShadowWindowHint) && (type == uint32_t(LWindowType::Popup))) {
        style |= CS_DROPSHADOW;
    }
    switch (type) {
    case uint32_t(LWindowType::Tool):
    case uint32_t(LWindowType::ToolTip):
    case uint32_t(LWindowType::Popup):{
        style |= CS_SAVEBITS; // Save/restore background
        icon = false;
        break;
    }
    case uint32_t(LWindowType::Dialog):{
        if (!(flags & LWindowType::WindowSystemMenuHint)){
            icon = false;
        }
        break;
    }
    default:{
        break;
    }
    }
    // Create a unique name for the flag combination
    std::string cname = "Lk_";
    cname += "LWindow";
    switch (type) {
    case uint32_t(LWindowType::Tool):{
        cname += "Tool";
        break;
    }
    case uint32_t(LWindowType::ToolTip):{
        cname += "ToolTip";
        break;
    }
    case uint32_t(LWindowType::Popup):{
        cname += "Popup";
        break;
    }
    default:
        break;
    }

    if (style & CS_DROPSHADOW){
        cname += "DropShadow";
    }
    if (style & CS_SAVEBITS){
        cname += "SaveBits";
    }
    if (style & CS_OWNDC){
        cname += "OwnDC";
    }
    if (icon){
        cname += "Icon";
    }

    return registerWindowClass(cname, lWindowsWndProc, style, GetSysColorBrush(COLOR_WINDOW), icon);

}

std::string LWindowsCtx::registerWindowClass(std::string cname, WNDPROC proc, unsigned int style, HBRUSH brush, bool icon)
{
    const auto appInstance = static_cast<HINSTANCE>(GetModuleHandle(nullptr));
    WNDCLASSA wcinfo;
    const bool classExists = GetClassInfo(appInstance, cname.data(), &wcinfo) != FALSE
                             && wcinfo.lpfnWndProc != proc;

    if (classExists){
        cname += LUuid::createUuid().toString();
    }

    if (m_registeredWindowClassNames.find(cname) != m_registeredWindowClassNames.end()){        // already registered in our list
        return cname;
    }

    WNDCLASSEXA wc;
    wc.cbSize       = sizeof(WNDCLASSEXA);
    wc.style        = style;
    wc.lpfnWndProc  = proc;
    wc.cbClsExtra   = 0;
    wc.cbWndExtra   = 0;
    wc.hInstance    = appInstance;
    wc.hCursor      = nullptr;
    wc.hbrBackground = brush;
    if (icon) {
        wc.hIcon = static_cast<HICON>(LoadImage(appInstance, "IDI_ICON1", IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
        if (wc.hIcon) {
            int sw = GetSystemMetrics(SM_CXSMICON);
            int sh = GetSystemMetrics(SM_CYSMICON);
            wc.hIconSm = static_cast<HICON>(LoadImage(appInstance, "IDI_ICON1", IMAGE_ICON, sw, sh, 0));
        } else {
            wc.hIcon = static_cast<HICON>(LoadImage(nullptr, IDI_APPLICATION, IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
            wc.hIconSm = nullptr;
        }
    } else {
        wc.hIcon    = nullptr;
        wc.hIconSm  = nullptr;
    }

    wc.lpszMenuName  = nullptr;
    wc.lpszClassName = cname.data();
    ATOM atom = RegisterClassExA(&wc);
    if (!atom){
        lWarn() << "LApplication::regClass: Registering window class " << cname << "failed.";
    }

    m_registeredWindowClassNames.insert(cname);
    return cname;
}

void LWindowsCtx::addWindow(HWND hwnd, std::shared_ptr<LWindowsWin> w)
{
    if(w != nullptr && hwnd!=NULL){
        m_windows[hwnd] = w;
    }
}

void LWindowsCtx::removeWindow(HWND hwnd)
{
    const std::unordered_map<HWND, std::weak_ptr<LWindowsWin> >::const_iterator it = m_windows.find(hwnd);
    if (it != m_windows.end()) {
        m_windows.erase(it);
    }
}

LWindowsCtx *LWindowsCtx::instance()
{
    return m_instance;
}

void LWindowsCtx::unregisterWindowClasses()
{
    const auto appInstance = static_cast<HINSTANCE>(GetModuleHandle(nullptr));

    for (const std::string &name : m_registeredWindowClassNames) {
        if (!UnregisterClass(name.data(), appInstance)){
            lWarn() << "UnregisterClass failed for " << name;
        }
    }
    m_registeredWindowClassNames.clear();
}

static inline std::string errorMessageFromComError(const _com_error &comError)
{
    TCHAR *message = nullptr;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
                  nullptr, DWORD(comError.Error()), MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),
                  message, 0, nullptr);
    if (message) {
        const std::string result = message;
        LocalFree(static_cast<HLOCAL>(message));
        return result;
    }
    if (const WORD wCode = comError.WCode()){
        const std::string result = "IDispatch error " + std::to_string(wCode);
        return result;
    }else{
        const std::string result = "Unknown error " + std::to_string(comError.Error());
        return result;
    }
}


std::string LWindowsCtx::comErrorString(HRESULT hr)
{
    std::string result = "COM error " + std::to_string(hr) + ":";
    switch (hr) {
    case S_OK:
        result += "S_OK";
        break;
    case S_FALSE:
        result += "S_FALSE";
        break;
    case E_UNEXPECTED:
        result += "E_UNEXPECTED";
        break;
    case E_ACCESSDENIED:
        result += "E_ACCESSDENIED";
        break;
    case CO_E_ALREADYINITIALIZED:
        result += "CO_E_ALREADYINITIALIZED";
        break;
    case CO_E_NOTINITIALIZED:
        result += "CO_E_NOTINITIALIZED";
        break;
    case RPC_E_CHANGED_MODE:
        result += "RPC_E_CHANGED_MODE";
        break;
    case OLE_E_WRONGCOMPOBJ:
        result += "OLE_E_WRONGCOMPOBJ";
        break;
    case CO_E_NOT_SUPPORTED:
        result += "CO_E_NOT_SUPPORTED";
        break;
    case E_NOTIMPL:
        result += "E_NOTIMPL";
        break;
    case E_INVALIDARG:
        result += "E_INVALIDARG";
        break;
    case E_NOINTERFACE:
        result += "E_NOINTERFACE";
        break;
    case E_POINTER:
        result += "E_POINTER";
        break;
    case E_HANDLE:
        result += "E_HANDLE";
        break;
    case E_ABORT:
        result += "E_ABORT";
        break;
    case E_FAIL:
        result += "E_FAIL";
        break;
    case RPC_E_WRONG_THREAD:
        result += "RPC_E_WRONG_THREAD";
        break;
    case RPC_E_THREAD_NOT_INIT:
        result += "RPC_E_THREAD_NOT_INIT";
        break;
    default:
        break;
    }
    _com_error error(hr);
    result += " (";
    result += errorMessageFromComError(error);
    result += ')';
    return result;
}

bool LWindowsCtx::windowsProc(HWND hwnd, UINT message, LWindows::WindowsEventType et, WPARAM wParam, LPARAM lParam, LRESULT *result, std::shared_ptr<LWindowsWin> platformWindow)
{
    *result = 0;

    MSG msg;
    msg.hwnd = hwnd;         // re-create MSG structure
    msg.message = message;   // time and pt fields ignored
    msg.wParam = wParam;
    msg.lParam = lParam;
    msg.pt.x = msg.pt.y = 0;
    if (et != LWindows::CursorEvent && (et & (LWindows::MouseEventFlag | LWindows::NonClientEventFlag))) {
        msg.pt.x = GET_X_LPARAM(lParam);
        msg.pt.y = GET_Y_LPARAM(lParam);
        // For non-client-area messages, these are screen coordinates (as expected
        // in the MSG structure), otherwise they are client coordinates.
        if (!((uint32_t)et & (uint32_t)LWindows::NonClientEventFlag)) {
            LWindowsWin::clientToScreen(msg.hwnd, &msg.pt);
        }
    } else {
        GetCursorPos(&msg.pt);
    }

    platformWindow = findPlatformWindow(hwnd);

    if (!isInputMessage(msg.message) && filterNativeEvent(&msg, result)){
        return true;
    }

    if ((platformWindow!= nullptr) && filterNativeEvent(platformWindow->window(), &msg, result)){
        return true;
    }

    if ((uint32_t)et & (uint32_t)LWindows::InputMethodEventFlag) {
        std::shared_ptr<LWindowsInputCtx> inputCtx = windowsInputContext();
        // Disable IME assuming this is a special implementation hooking into keyboard input.
        // "Real" IME implementations should use a native event filter intercepting IME events.
        if (inputCtx == nullptr) {
            LWindowsInputCtx::setWindowsImeEnabled(platformWindow, false);
            return false;
        }
        switch (et) {
        case LWindows::InputMethodStartCompositionEvent:{
            return inputCtx->startComposition(hwnd);
        }
        case LWindows::InputMethodCompositionEvent:{
            return inputCtx->composition(hwnd, lParam);
        }
        case LWindows::InputMethodEndCompositionEvent:{
            return inputCtx->endComposition(hwnd);
        }
        case LWindows::InputMethodRequest:{
            return inputCtx->handleIMERequest(wParam, lParam, result);
        }
        default:
            break;
        }
    } // InputMethodEventFlag

    switch (et) {
    case LWindows::InputMethodOpenCandidateWindowEvent:
    case LWindows::InputMethodCloseCandidateWindowEvent:{
        return false;
    }
    case LWindows::DestroyEvent:{
        if ((platformWindow!= nullptr) && !platformWindow->testFlag(LWindowsWin::WithinDestroy)) {
            lWarn() << "External WM_DESTROY received";
        }
        return false;
    }
    case LWindows::ClipboardEvent:{
        return false;
    }
    case LWindows::CursorEvent:{ // Sent to windows that do not have capture.
        if (LOWORD(lParam) == HTCLIENT) {
            if(platformWindow != nullptr){
                platformWindow->applyCursor();
            }else{
                if (LWindowsCursor::hasOverrideCursor()) {
                    LWindowsCursor::enforceOverrideCursor();
                }
            }
            return true;
        }else{
            break;
        }
    }
    case LWindows::UnknownEvent:{
        return false;
    }
    case LWindows::AccessibleObjectFromWindowRequest:{
        // return LWindowsUiaAccessibility::handleWmGetObject(hwnd, wParam, lParam, result);
        return false;
    }
    case LWindows::SettingChangedEvent: {
        // LWindowsWin::settingsChanged();
        // // Only refresh the window theme if the user changes the personalize settings.
        // if ((wParam == 0) && (lParam != 0) // lParam sometimes may be NULL.
        //     && (wcscmp(reinterpret_cast<LPCWSTR>(lParam), L"ImmersiveColorSet") == 0)) {
        //     LWindowsTheme::handleSettingsChanged();
        // }
        return m_screenManager.handleScreenChanges();
    }
    default:
        break;
    }

    // Before CreateWindowEx() returns, some events are sent,
    // for example WM_GETMINMAXINFO asking for size constraints for top levels.
    // Pass on to current creation context
    if ((platformWindow==nullptr) && m_creationContext != nullptr) {
        switch (et) {
        case LWindows::QuerySizeHints:{
            m_creationContext->applyToMinMaxInfo(reinterpret_cast<MINMAXINFO *>(lParam));
            return true;
        }
        case LWindows::ResizeEvent:{
            m_creationContext->obtainedSize = LSize<int>(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return true;
        }
        case LWindows::MoveEvent:{
            m_creationContext->obtainedPos = LPoint<int>(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
            return true;
        }
        case LWindows::NonClientCreate:{
            if (shouldHaveNonClientDpiScaling(m_creationContext->window)){
                enableNonClientDpiScaling(msg.hwnd);
            }
            return false;
        }
        case LWindows::CalculateSize:{
            return LWindowsGeometryHint::handleCalculateSize(m_creationContext->customMargins, msg, result);

        }
        case LWindows::GeometryChangingEvent:{
            return LWindowsWin::handleGeometryChangingMessage(&msg,
                                                              m_creationContext->window,
                                                              m_creationContext->margins + m_creationContext->customMargins);
        }
        default:{
            break;
        }
        }
    }
    if (platformWindow!= nullptr) {
        // Suppress events sent during DestroyWindow() for native children.
        if (platformWindow->testFlag(LWindowsWin::WithinDestroy)){
            return false;
        }
    } else {
        return false;
    }

    switch (et) {
    case LWindows::DeviceChangeEvent:{
//        if (m_systemInfo & LWindowsCtx::SI_SupportsTouch){
//            break;
//        }
//        // See if there are any touch devices added
//        if (wParam == DBT_DEVNODES_CHANGED){
//            initTouch();
//        }
        break;
    }
    case LWindows::InputLanguageChangeEvent:{
        // lWarn() << "InputLanguageChangeEvent";
        std::shared_ptr<LWindowsInputCtx>  wic = windowsInputContext();
        if (wic!= nullptr){
            wic->handleInputLanguageChanged(wParam, lParam);
        }
        return false;
    }
    case LWindows::KeyDownEvent:
    case LWindows::KeyEvent:
    case LWindows::InputMethodKeyEvent:
    case LWindows::InputMethodKeyDownEvent:
    case LWindows::AppCommandEvent:{
        return m_keyMapper.translateKeyEvent(platformWindow->window(), hwnd, msg, result);
    }
    case LWindows::MenuAboutToShowEvent:{
        if (platformWindow == nullptr){
           return false;
        }
        return true;
    }
    case LWindows::MenuCommandEvent:{
        if (platformWindow == nullptr){
           return false;
        }
        return true;
    }
    case LWindows::MoveEvent:{
        platformWindow->handleMoved();
        return true;
    }
    case LWindows::ResizeEvent:{
        platformWindow->handleResized(static_cast<int>(wParam));
        return true;
    }
    case LWindows::QuerySizeHints:{
        platformWindow->getSizeHints(reinterpret_cast<MINMAXINFO *>(lParam));
        return true;// maybe available on some SDKs revisit WM_NCCALCSIZE
    }
    case LWindows::CalculateSize:{
        return LWindowsGeometryHint::handleCalculateSize(platformWindow->customMargins(), msg, result);
    }
    case LWindows::NonClientHitTest:{
        return platformWindow->handleNonClientHitTest(LPoint<int>(msg.pt.x, msg.pt.y), result);
    }
    case LWindows::GeometryChangingEvent:{
        return platformWindow->handleGeometryChanging(&msg);
    }
    case LWindows::ExposeEvent:{
        return platformWindow->handleWmPaint(hwnd, message, wParam, lParam);
    }
    case LWindows::NonClientMouseEvent:{
        if ((m_systemInfo & LWindowsCtx::SI_SupportsPointer) && platformWindow->frameStrutEventsEnabled()){
            return m_pointerHandler.translateMouseEvent(platformWindow->window(), hwnd, et, msg, result);
        }
        else{
            return m_mouseHandler.translateMouseEvent(platformWindow->window(), hwnd, et, msg, result);
        }
    }
    case LWindows::NonClientPointerEvent:{
        if ((m_systemInfo & LWindowsCtx::SI_SupportsPointer) && platformWindow->frameStrutEventsEnabled()){
            return m_pointerHandler.translatePointerEvent(platformWindow->window(), hwnd, et, msg, result);
        }
        break;
    }
    case LWindows::EnterSizeMoveEvent:{
        platformWindow->setFlag(LWindowsWin::ResizeMoveActive);
        return true;
    }
    case LWindows::ExitSizeMoveEvent:{
        platformWindow->clearFlag(LWindowsWin::ResizeMoveActive);
        platformWindow->checkForScreenChanged();
        handleExitSizeMove(platformWindow->window());
        return true;
    }
    case LWindows::ScrollEvent:{
        break;
    }
    case LWindows::MouseWheelEvent:
    case LWindows::MouseEvent:
    case LWindows::LeaveEvent:
    {
        std::shared_ptr<LWindow> window = platformWindow->window();
        while ((window!= nullptr) && (window->flags() & LWindowType::WindowTransparentForInput)){
            window = window->parent();
        }

        if (window == nullptr){
            return false;
        }

        if (m_systemInfo & LWindowsCtx::SI_SupportsPointer){
            return m_pointerHandler.translateMouseEvent(window, hwnd, et, msg, result);
        }else{
            return m_mouseHandler.translateMouseEvent(window, hwnd, et, msg, result);
        }
    }
    case LWindows::TouchEvent:
        break;
    case LWindows::PointerEvent:{
        if (m_systemInfo & LWindowsCtx::SI_SupportsPointer){
            return m_pointerHandler.translatePointerEvent(platformWindow->window(), hwnd, et, msg, result);
        }
        break;
    }
    case LWindows::FocusInEvent: // see LWindowsWin::requestActivateWindow().
    case LWindows::FocusOutEvent:{
        handleFocusEvent(et, platformWindow);
        return true;
    }
    case LWindows::ShowEventOnParentRestoring:{
        if (!platformWindow->window()->visible()) {
            *result = 0;
            return true;
        }
        break;
    }
    case LWindows::HideEvent:{
        platformWindow->handleHidden();
        return false;// Indicate transient children should be hidden by windows (SW_PARENTCLOSING)
    }
    case LWindows::CloseEvent:{
        LWindowSystemInterface::handleCloseEvent(platformWindow->window());
        return true;
    }
   case LWindows::ThemeChanged: {
       // Switch from Aero to Classic changes margins.
       // if (LWindowsTheme *theme = LWindowsTheme::instance())
       //     theme->windowsThemeChanged(platformWindow->window());
       return true;
   }
    case LWindows::CompositionSettingsChanged:{
        //Todo
//        platformWindow->handleCompositionSettingsChanged();
        return true;
    }
    case LWindows::ActivateWindowEvent:{
        if (platformWindow->window()->flags() & LWindowType::WindowDoesNotAcceptFocus) {
            *result = LRESULT(MA_NOACTIVATE);
            return true;
        }
        if (platformWindow->testFlag(LWindowsWin::BlockedByModal))
            if (const std::shared_ptr<LWindow> modalWindow = LGuiApplication::modalWindow()) {
                std::shared_ptr<LWindowsWin> platformWindow = LWindowsWin::windowsWindowOf(modalWindow);
                platformWindow->alertWindow();
            }
        break;
    }
    case LWindows::MouseActivateWindowEvent:
    case LWindows::PointerActivateWindowEvent:{
        if (platformWindow->window()->flags() & LWindowType::WindowDoesNotAcceptFocus) {
            *result = LRESULT(MA_NOACTIVATE);
            return true;
        }
        break;
    }
    case LWindows::ContextMenu:{
        //return handleContextMenuEvent(platformWindow->window(), msg);
        return true;
    }
    case LWindows::WhatsThisEvent: {
        //LWindowSystemInterface::handleEnterWhatsThisEvent();
        return true;
    }
    case LWindows::DpiChangedEvent: {
        const bool doResize = resizeOnDpiChanged(platformWindow->window());
        if (doResize) {
            platformWindow->setFlag(LWindowsWin::WithinDpiChanged);
            platformWindow->updateFullFrameMargins();
            const auto prcNewWindow = reinterpret_cast<RECT *>(lParam);

            SetWindowPos(hwnd, nullptr, prcNewWindow->left, prcNewWindow->top,
                         prcNewWindow->right - prcNewWindow->left,
                         prcNewWindow->bottom - prcNewWindow->top, SWP_NOZORDER | SWP_NOACTIVATE);
            platformWindow->clearFlag(LWindowsWin::WithinDpiChanged);
        }
        platformWindow->checkForScreenChanged(LWindowsWin::FromDpiChange);
        return doResize;
    }
    default:
        break;
    }
    return false;

}

std::shared_ptr<LWindowsWin> LWindowsCtx::findClosestPlatformWindow(HWND hwnd) const
{
    std::shared_ptr<LWindowsWin> window = findPlatformWindow(hwnd);
    // Requested hwnd may also be a child of a platform window in case of embedded native windows.
    // Find the closest parent that has a platform window.
    if (window == nullptr) {
        for (HWND w = hwnd; w; w = GetParent(w)) {
            window = findPlatformWindow(w);
            if (window != nullptr){
                break;
            }
        }
    }
    return window;
}

std::shared_ptr<LWindowsWin> LWindowsCtx::findPlatformWindow(HWND hwnd) const
{
    std::unordered_map<HWND, std::weak_ptr<LWindowsWin> >::const_iterator iter = m_windows.find(hwnd);
    if(iter != m_windows.end()){
        return iter->second.lock();
    }
    return nullptr;
}

bool LWindowsCtx::filterNativeEvent(MSG *msg, LRESULT *result)
{
    std::shared_ptr<LEventDispatcher> dispatcher = LEventDispatcher::instance();
    lintptr filterResult = 0;
    if(dispatcher != nullptr){
        if (dispatcher->filterNativeEvent(LEventDispatcher::nativeEventType(), msg, &filterResult)) {
            *result = LRESULT(filterResult);
            return true;
        }
    }
    return false;
}

bool LWindowsCtx::filterNativeEvent(std::shared_ptr<LWindow> window, MSG *msg, LRESULT *result)
{
    lintptr filterResult = 0;
    if (LWindowSystemInterface::handleNativeEvent(window, LEventDispatcher::nativeEventType(), msg, &filterResult)) {
        *result = LRESULT(filterResult);
        return true;
    }
    return false;
}

std::shared_ptr<LWindowCreationCtx> LWindowsCtx::setWindowCreationContext(const std::shared_ptr<LWindowCreationCtx> &ctx)
{
    const std::shared_ptr<LWindowCreationCtx> old = m_creationContext;
    m_creationContext = ctx;
    return old;
}

bool LWindowsCtx::shouldHaveNonClientDpiScaling(const std::shared_ptr<LWindow> window)
{
    return LOperSysVersion::current() >= LOperSysVersion::Windows10
           && window->isTopLevel();
}

std::shared_ptr<LWindow> LWindowsCtx::windowUnderMouse() const
{
    return (m_systemInfo & LWindowsCtx::SI_SupportsPointer) ?
               m_pointerHandler.windowUnderMouse() : m_mouseHandler.windowUnderMouse();
}

void LWindowsCtx::clearWindowUnderMouse()
{
    if(m_systemInfo & LWindowsCtx::SI_SupportsPointer){
        m_pointerHandler.clearWindowUnderMouse();
    }else{
        m_mouseHandler.clearWindowUnderMouse();
    }
}

bool LWindowsCtx::systemParametersInfo(uint32_t action, uint32_t param, void *out, uint32_t dpi)
{
    const BOOL result = LWindowsCtx::user32dll.systemParametersInfoForDpi != nullptr && dpi != 0
                            ? LWindowsCtx::user32dll.systemParametersInfoForDpi(action, param, out, 0, dpi)
                            : SystemParametersInfo(action, param, out, 0);
    return result == TRUE;

}

bool LWindowsCtx::nonClientMetrics(NONCLIENTMETRICS *ncm, uint32_t dpi)
{
    memset(ncm, 0, sizeof(NONCLIENTMETRICS));
    ncm->cbSize = sizeof(NONCLIENTMETRICS);
    return systemParametersInfo(SPI_GETNONCLIENTMETRICS, ncm->cbSize, ncm, dpi);
}

bool LWindowsCtx::nonClientMetricsForScreen(NONCLIENTMETRICS *ncm, const std::shared_ptr<LPlatformScreen> screen)
{
    const int dpi = screen ? std::lround(screen->logicalDpi().first) : 0;
    return nonClientMetrics(ncm, unsigned(dpi));
}

DWORD LWindowsCtx::readAdvancedExplorerSettings(const std::wstring& subKey, DWORD defaultValue)
{
    LWinRegistry reg;
    return reg.getDword(HKEY_CURRENT_USER,
                        L"Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced",
                        subKey,
                        defaultValue);

}

std::shared_ptr<LWindowsWin> LWindowsCtx::findPlatformWindowAt(HWND parent,
                                                               const LPoint<int> &screenPointIn,
                                                               unsigned int cwexFlags) const
{
    std::shared_ptr<LWindowsWin> result = nullptr;
    const POINT screenPoint = { screenPointIn.m_x, screenPointIn.m_y };

    while(true){
        POINT point = screenPoint;
        LWindowsWin::screenToClient(parent, &point);
        // Returns parent if inside & none matched.
        const HWND child = ChildWindowFromPointEx(parent, point, cwexFlags);
        if (!child || child == parent){
            break;
        }

        std::shared_ptr<LWindowsWin> window = findPlatformWindow(child);
        if (window != nullptr) {
            result = window;
            parent = child;
            continue;
        }

        if (!(cwexFlags & CWP_SKIPTRANSPARENT)
            && (GetWindowLongPtr(child, GWL_EXSTYLE) & WS_EX_TRANSPARENT)) {
            const HWND nonTransparentChild = ChildWindowFromPointEx(parent, point, cwexFlags | CWP_SKIPTRANSPARENT);
            window = findPlatformWindow(nonTransparentChild);
            if (window != nullptr) {
                result = window;
                parent = nonTransparentChild;
                continue;
            }
        }
        parent = child;
    }

    if (result == nullptr) {
        if (const HWND window = WindowFromPoint(screenPoint)){
            result = findPlatformWindow(window);
        }
    }
    return result;
}

void LWindowsCtx::handleFocusEvent(LWindows::WindowsEventType et, std::shared_ptr<LWindowsWin> platformWindow)
{
    std::shared_ptr<LWindow> nextActiveWindow = nullptr;
    if (et == LWindows::FocusInEvent) {
        std::shared_ptr<LWindow> topWindow = LWindowsWin::topLevelOf(platformWindow->window());
        std::shared_ptr<LWindow> modalWindow = LGuiApplication::guiInst()->windowBlocked(topWindow);
        if (modalWindow != nullptr && topWindow != modalWindow) {
            modalWindow->requestActivate();
            return;
        }

        if (platformWindow->testFlag(LWindowsWin::WithinSetParent)) {
            std::shared_ptr<LWindow> currentFocusWindow = LGuiApplication::focusWindow();
            if (currentFocusWindow && currentFocusWindow != platformWindow->window()) {
                currentFocusWindow->requestActivate();
                return;
            }
        }
        nextActiveWindow = platformWindow->window();
    } else {
        // Focus out: Is the next window known and different
        // from the receiving the focus out.
        if (const HWND nextActiveHwnd = GetFocus()){
            if (std::shared_ptr<LWindowsWin> nextActivePlatformWindow = findClosestPlatformWindow(nextActiveHwnd)){
                if (nextActivePlatformWindow != platformWindow){
                    nextActiveWindow = nextActivePlatformWindow->window();
                }
            }
        }
    }
    if (nextActiveWindow != m_lastActiveWindow.lock()) {
        m_lastActiveWindow = nextActiveWindow;
        LWindowSystemInterface::handleWindowActivated(nextActiveWindow);
    }
}

void LWindowsCtx::handleExitSizeMove(std::shared_ptr<LWindow> window)
{
    const LFlags<LMouseButton> currentButtons = LWindowsMouseHandler::queryMouseButtons();
    const LFlags<LMouseButton> appButtons = LGuiApplication::mouseButtons();
    if (currentButtons == appButtons){
        return;
    }
    const LFlags<LKeyModifier> keyboardModifiers = LWindowsKeyMapper::queryKeyboardModifiers();
    const LPoint<int> globalPos = LWindowsCursor::mousePosition();
    const std::shared_ptr<LPlatformWindow> platWin = window->handle();
    const LPoint<int> localPos = platWin->mapFromGlobal(globalPos);
    const LEvent::Type type = platWin->geometry().contains(globalPos)
                                  ? LEvent::MouseButtonRelease : LEvent::NonClientAreaMouseButtonRelease;
    for (LMouseButton button : {LMouseButton::Left, LMouseButton::Right, LMouseButton::Middle}) {
        if (appButtons.testFlag(button) && !currentButtons.testFlag(button)) {
            LWindowSystemInterface::handleMouseEvent(window,
                                                     localPos.toPointF(), globalPos.toPointF(),
                                                     currentButtons, button, type,
                                                     keyboardModifiers);
        }
    }
    if (m_systemInfo & LWindowsCtx::SI_SupportsPointer){
        m_pointerHandler.clearEvents();
    }
    else{
        m_mouseHandler.clearEvents();
    }
}


extern "C" LRESULT L_WIN_CALLBACK lWindowsWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result;
    const LWindows::WindowsEventType et = windowsEventType(message, wParam, lParam);
    std::shared_ptr<LWindowsWin> platformWindow = nullptr;
    const RECT ncCalcSizeFrame = rectFromNcCalcSize(message, wParam, lParam, 0);
    const bool handled = LWindowsCtx::instance()->windowsProc(hwnd, message, et, wParam, lParam, &result, platformWindow);

    if (!handled){
        result = DefWindowProc(hwnd, message, wParam, lParam);
    }

    if (message == WM_NCCALCSIZE && !isEmptyRect(ncCalcSizeFrame) && isTopLevel(hwnd) && !isMinimized(hwnd)) {
        const LMargins<int> margins =
            marginsFromRects(ncCalcSizeFrame, rectFromNcCalcSize(message, wParam, lParam, 0));
        if (margins.m_left >= 0) {
            if (platformWindow) {
                platformWindow->setFullFrameMargins(margins);
            } else {
                std::shared_ptr<LWindowCreationCtx> ctx = LWindowsCtx::instance()->windowCreationContext();
                if (ctx != nullptr){
                    ctx->margins = margins;
                }
            }
        }
    }
    return result;
}
LK_NAMESPACE_END



