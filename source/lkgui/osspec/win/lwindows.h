#ifndef LWINDOWS_H
#define LWINDOWS_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "lkcore/lookiconfig.h"
#include <lkcore/ldebug.h>

LK_NAMESPACE_BEGIN

namespace LWindows
{
enum EventFlag
{
    WindowEventFlag = 0x10000,
    MouseEventFlag = 0x20000,
    NonClientEventFlag = 0x40000,
    InputMethodEventFlag = 0x80000,
    KeyEventFlag = 0x100000,
    KeyDownEventFlag = 0x200000,
    TouchEventFlag = 0x400000,
    ClipboardEventFlag = 0x800000,
    ApplicationEventFlag = 0x1000000,
    ThemingEventFlag = 0x2000000,
    GenericEventFlag = 0x4000000, // Misc
    PointerEventFlag = 0x8000000,
};

enum WindowsEventType // Simplify event types
{
    ExposeEvent = WindowEventFlag + 1,
    ActivateWindowEvent = WindowEventFlag + 2,
    DeactivateWindowEvent = WindowEventFlag + 3,
    MouseActivateWindowEvent = WindowEventFlag + 4,
    LeaveEvent = WindowEventFlag + 5,
    CloseEvent = WindowEventFlag + 6,
    ShowEvent = WindowEventFlag + 7,
    ShowEventOnParentRestoring = WindowEventFlag + 20,
    HideEvent = WindowEventFlag + 8,
    DestroyEvent = WindowEventFlag + 9,
    GeometryChangingEvent = WindowEventFlag + 10,
    MoveEvent = WindowEventFlag + 11,
    ResizeEvent = WindowEventFlag + 12,
    QuerySizeHints = WindowEventFlag + 15,
    CalculateSize = WindowEventFlag + 16,
    FocusInEvent = WindowEventFlag + 17,
    FocusOutEvent = WindowEventFlag + 18,
    WhatsThisEvent = WindowEventFlag + 19,
    DpiChangedEvent = WindowEventFlag + 21,
    EnterSizeMoveEvent = WindowEventFlag + 22,
    ExitSizeMoveEvent = WindowEventFlag + 23,
    PointerActivateWindowEvent = WindowEventFlag + 24,
    DpiScaledSizeEvent = WindowEventFlag + 25,
    DpiChangedAfterParentEvent = WindowEventFlag + 27,
    TaskbarButtonCreated = WindowEventFlag + 28,
    MouseEvent = MouseEventFlag + 1,
    MouseWheelEvent = MouseEventFlag + 2,
    CursorEvent = MouseEventFlag + 3,
    TouchEvent = TouchEventFlag + 1,
    PointerEvent = PointerEventFlag + 1,
    NonClientMouseEvent = NonClientEventFlag + MouseEventFlag + 1,
    NonClientHitTest = NonClientEventFlag + 2,
    NonClientCreate = NonClientEventFlag + 3,
    NonClientPointerEvent = NonClientEventFlag + PointerEventFlag + 4,
    KeyEvent = KeyEventFlag + 1,
    KeyDownEvent = KeyEventFlag + KeyDownEventFlag + 1,
    InputLanguageChangeEvent = KeyEventFlag + 2,
    InputMethodKeyEvent = InputMethodEventFlag + KeyEventFlag + 1,
    InputMethodKeyDownEvent = InputMethodEventFlag + KeyEventFlag + KeyDownEventFlag + 1,
    ClipboardEvent = ClipboardEventFlag + 1,
    ActivateApplicationEvent = ApplicationEventFlag + 1,
    DeactivateApplicationEvent = ApplicationEventFlag + 2,
    AccessibleObjectFromWindowRequest = ApplicationEventFlag + 3,
    QueryEndSessionApplicationEvent = ApplicationEventFlag + 4,
    EndSessionApplicationEvent = ApplicationEventFlag + 5,
    AppCommandEvent = ApplicationEventFlag + 6,
    DeviceChangeEvent = ApplicationEventFlag + 7,
    MenuAboutToShowEvent = ApplicationEventFlag + 8,
    AcceleratorCommandEvent = ApplicationEventFlag + 9,
    MenuCommandEvent = ApplicationEventFlag + 10,
    InputMethodStartCompositionEvent = InputMethodEventFlag + 1,
    InputMethodCompositionEvent = InputMethodEventFlag + 2,
    InputMethodEndCompositionEvent = InputMethodEventFlag + 3,
    InputMethodOpenCandidateWindowEvent = InputMethodEventFlag + 4,
    InputMethodCloseCandidateWindowEvent = InputMethodEventFlag + 5,
    InputMethodRequest = InputMethodEventFlag + 6,
    ThemeChanged = ThemingEventFlag + 1,
    CompositionSettingsChanged = ThemingEventFlag + 2,
    SettingChangedEvent = 438,
    ScrollEvent = GenericEventFlag + 1,
    ContextMenu = 123,
    GestureEvent = 124,
    UnknownEvent = 542
};

// Matches Process_DPI_Awareness (Windows 8.1 onwards), used for SetProcessDpiAwareness()
enum ProcessDpiAwareness
{
    ProcessDpiUnaware,
    ProcessSystemDpiAware,
    ProcessPerMonitorDpiAware
};
}

static inline std::string winMessageName(UINT message){

    switch (message) {
    case WM_PAINT:{
        return "WM_PAINT";
    }
    case WM_ERASEBKGND:{
        return "WM_ERASEBKGND";
    }
    case WM_CLOSE:{
        return "WM_CLOSE";
    }
    case WM_DESTROY:{
        return "WM_DESTROY";
    }
    case WM_ACTIVATEAPP:{
        return "WM_ACTIVATEAPP";
    }
    case WM_MOUSEACTIVATE:{
        return "WM_MOUSEACTIVATE";
    }
    case WM_ACTIVATE:{
        return "WM_ACTIVATE";
    }
    case WM_SETCURSOR:{
        return "WM_SETCURSOR";
    }
    case WM_MOUSELEAVE:{
        return "WM_MOUSELEAVE";
    }
    case WM_HSCROLL:{
        return "WM_HSCROLL";
    }
    case WM_MOUSEWHEEL:{
        return "WM_MOUSEWHEEL";
    }
    case WM_WINDOWPOSCHANGING:{
        return "WM_WINDOWPOSCHANGING";
    }
    case WM_MOVE:{
        return "WM_MOVE";
    }
    case WM_SHOWWINDOW:{
        return "WM_SHOWWINDOW";
    }
    case WM_SIZE:{
        return "WM_SIZE";
    }
    case WM_NCCREATE:{
        return "WM_NCCREATE";
    }
    case WM_NCCALCSIZE:{
        return "WM_NCCALCSIZE";
    }
    case WM_NCHITTEST:{
        return "WM_NCHITTEST";
    }
    case WM_GETMINMAXINFO:{
        return "WM_GETMINMAXINFO";
    }
    case WM_KEYDOWN:{
        return "WM_KEYDOWN";
    }
    case WM_SYSKEYDOWN:{
        return "WM_SYSKEYDOWN";
    }
    case WM_KEYUP:{
        return "WM_KEYUP";
    }
    case WM_SYSKEYUP:{
        return "WM_SYSKEYUP";
    }
    case WM_CHAR:{
        return "WM_CHAR";
    }
    case WM_IME_CHAR:{
        return "WM_IME_CHAR";
    }
    case WM_IME_KEYDOWN:{
        return "WM_IME_KEYDOWN";
    }
    case WM_INPUTLANGCHANGE:{
        return "WM_INPUTLANGCHANGE";
    }
    case WM_CHANGECBCHAIN:{
        return "WM_CHANGECBCHAIN";
    }
    case WM_DRAWCLIPBOARD:{
        return "WM_DRAWCLIPBOARD";
    }
    case WM_RENDERFORMAT:{
        return "WM_RENDERFORMAT";
    }
    case WM_RENDERALLFORMATS:{
        return "WM_RENDERALLFORMATS";
    }
    case WM_DESTROYCLIPBOARD:{
        return "WM_DESTROYCLIPBOARD";
    }
    case WM_IME_STARTCOMPOSITION:{
        return "WM_IME_STARTCOMPOSITION";
    }
    case WM_IME_ENDCOMPOSITION:{
        return "WM_IME_ENDCOMPOSITION";
    }
    case WM_IME_COMPOSITION:{
        return "WM_IME_COMPOSITION";
    }
    case WM_IME_REQUEST:{
        return "WM_IME_REQUEST";
    }
    case WM_IME_NOTIFY:{
        return "WM_IME_NOTIFY";
    }
    case WM_GETOBJECT:{
        return "WM_GETOBJECT";
    }
    case WM_SETFOCUS:{
        return "WM_SETFOCUS";
    }
    case WM_KILLFOCUS:{
        return "WM_SETFOCUS";
    }
    case WM_SETTINGCHANGE:{
        return "WM_SETTINGCHANGE";
    }
    case WM_DISPLAYCHANGE:{
        return "WM_DISPLAYCHANGE";
    }
    case WM_THEMECHANGED:{
        return "WM_THEMECHANGED";
    }
    case WM_SYSCOLORCHANGE:{
        return "WM_SYSCOLORCHANGE";
    }
    case WM_CONTEXTMENU:{
        return "WM_CONTEXTMENU";
    }
    case WM_SYSCOMMAND:{
        return "WM_SYSCOMMAND";
    }
    case WM_QUERYENDSESSION:{
        return "WM_QUERYENDSESSION";
    }
    case WM_ENDSESSION:{
        return "WM_ENDSESSION";
    }
    case WM_APPCOMMAND:{
        return "WM_APPCOMMAND";
    }
    case WM_DEVICECHANGE:{
        return "WM_DEVICECHANGE";
    }
    case WM_INITMENU:{
        return "WM_INITMENU";
    }
    case WM_INITMENUPOPUP:{
        return "WM_INITMENUPOPUP";
    }
    case WM_COMMAND:{
        return "WM_COMMAND";
    }
    case WM_ENTERSIZEMOVE:{
        return "WM_ENTERSIZEMOVE";
    }
    case WM_EXITSIZEMOVE:{
        return "WM_EXITSIZEMOVE";
    }
    default:{
        return std::to_string(message);
    }
    }
}

inline LWindows::WindowsEventType windowsEventType(UINT message, WPARAM wParamIn, LPARAM lParamIn)
{
    //lWarn() << "win message:" << winMessageName(message) << ","  << uint64_t(wParamIn) << "," << uint64_t(lParamIn);

    // static const UINT WM_TASKBAR_BUTTON_CREATED = []{
    //     UINT message = RegisterWindowMessageW(L"TaskbarButtonCreated");
    //     // In case the application is run elevated, allow the
    //     // TaskbarButtonCreated message through.
    //     ChangeWindowMessageFilter(message, MSGFLT_ADD);
    //     return message;
    // }();

    switch (message) {
    case WM_PAINT:
    case WM_ERASEBKGND:{
        return LWindows::ExposeEvent;
    }
    case WM_CLOSE:{
        return LWindows::CloseEvent;
    }
    case WM_DESTROY:{
        return LWindows::DestroyEvent;
    }
    case WM_ACTIVATEAPP:{
        return (int)wParamIn ?
                   LWindows::ActivateApplicationEvent : LWindows::DeactivateApplicationEvent;
    }
    case WM_MOUSEACTIVATE:{
        return LWindows::MouseActivateWindowEvent;
    }
    case WM_POINTERACTIVATE:{
        return LWindows::PointerActivateWindowEvent;
    }
    case WM_ACTIVATE:{
        return  LOWORD(wParamIn) == WA_INACTIVE ?
                   LWindows::DeactivateWindowEvent : LWindows::ActivateWindowEvent;
    }
    case WM_SETCURSOR:{
        return LWindows::CursorEvent;
    }
    case WM_MOUSELEAVE:{
        return LWindows::MouseEvent;
    }
    case WM_HSCROLL:{
        return LWindows::ScrollEvent;
    }
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:{
        return LWindows::MouseWheelEvent;
    }
    case WM_WINDOWPOSCHANGING:{
        return LWindows::GeometryChangingEvent;
    }
    case WM_MOVE:{
        return LWindows::MoveEvent;
    }
    case WM_SHOWWINDOW:{
        if (wParamIn){
            return lParamIn == SW_PARENTOPENING ? LWindows::ShowEventOnParentRestoring : LWindows::ShowEvent;
        }
        return LWindows::HideEvent;
    }
    case WM_SIZE:{
        return LWindows::ResizeEvent;
    }
    case WM_NCCREATE:{
        return LWindows::NonClientCreate;
    }
    case WM_NCCALCSIZE:{
        return LWindows::CalculateSize;
    }
    case WM_NCHITTEST:{
        return LWindows::NonClientHitTest;
    }
    case WM_GETMINMAXINFO:{
        return LWindows::QuerySizeHints;
    }
    case WM_KEYDOWN:                        // keyboard event
    case WM_SYSKEYDOWN:{
        return LWindows::KeyDownEvent;
    }
    case WM_KEYUP:
    case WM_SYSKEYUP:
    case WM_CHAR:{
        return LWindows::KeyEvent;
    }
    case WM_IME_CHAR:{
        return LWindows::InputMethodKeyEvent;
    }
    case WM_IME_KEYDOWN:{
        return LWindows::InputMethodKeyDownEvent;
    }
#ifdef WM_INPUTLANGCHANGE
    case WM_INPUTLANGCHANGE:{
        return LWindows::InputLanguageChangeEvent;
    }
#endif // WM_INPUTLANGCHANGE
    case WM_TOUCH:{
        return LWindows::TouchEvent;
    }
    case WM_CHANGECBCHAIN:
    case WM_DRAWCLIPBOARD:
    case WM_RENDERFORMAT:
    case WM_RENDERALLFORMATS:
    case WM_DESTROYCLIPBOARD:{
        return LWindows::ClipboardEvent;
    }
    case WM_IME_STARTCOMPOSITION:{
        return LWindows::InputMethodStartCompositionEvent;
    }
    case WM_IME_ENDCOMPOSITION:{
        return LWindows::InputMethodEndCompositionEvent;
    }
    case WM_IME_COMPOSITION:{
        return LWindows::InputMethodCompositionEvent;
    }
    case WM_IME_REQUEST:{
        return LWindows::InputMethodRequest;
    }
    case WM_IME_NOTIFY:
        switch (int(wParamIn)) {
        case IMN_OPENCANDIDATE:{
            return LWindows::InputMethodOpenCandidateWindowEvent;
        }
        case IMN_CLOSECANDIDATE:{
            return LWindows::InputMethodCloseCandidateWindowEvent;
        }
        default:{
            break;
        }
        }
        break;
    case WM_GETOBJECT:{
        return LWindows::AccessibleObjectFromWindowRequest;
    }
    case WM_SETFOCUS:{
        return LWindows::FocusInEvent;
    }
    case WM_KILLFOCUS:{
        return LWindows::FocusOutEvent;
    }
    // Among other things, WM_SETTINGCHANGE happens when the taskbar is moved
    // and therefore the "working area" changes.
    // http://msdn.microsoft.com/en-us/library/ms695534(v=vs.85).aspx
    case WM_SETTINGCHANGE:{
        return LWindows::SettingChangedEvent;
    }
    case WM_THEMECHANGED:
    case WM_SYSCOLORCHANGE: // Handle color change as theme change (QTBUG-34170).
    case WM_DWMCOLORIZATIONCOLORCHANGED:{
        return LWindows::ThemeChanged;
    }
    case WM_DWMCOMPOSITIONCHANGED:{
        return LWindows::CompositionSettingsChanged;
    }
    case WM_CONTEXTMENU:{
        return LWindows::ContextMenu;
    }
    case WM_SYSCOMMAND:{
        if ((wParamIn & 0xfff0) == SC_CONTEXTHELP){
            return LWindows::WhatsThisEvent;
        }
        break;
    }
    case WM_QUERYENDSESSION:{
        return LWindows::QueryEndSessionApplicationEvent;
    }
    case WM_ENDSESSION:{
        return LWindows::EndSessionApplicationEvent;
    }
#if defined(WM_APPCOMMAND)
    case WM_APPCOMMAND:{
        return LWindows::AppCommandEvent;
    }
#endif
    case WM_GESTURE:{
        return LWindows::GestureEvent;
    }
    case WM_DEVICECHANGE:{
        return LWindows::DeviceChangeEvent;
    }
    case WM_INITMENU:
    case WM_INITMENUPOPUP:{
        return LWindows::MenuAboutToShowEvent;
    }
    case WM_COMMAND:{
        return HIWORD(wParamIn) ? LWindows::AcceleratorCommandEvent : LWindows::MenuCommandEvent;
    }
    case WM_DPICHANGED:{
        return LWindows::DpiChangedEvent;
    }
    case WM_DPICHANGED_AFTERPARENT:{
        return LWindows::DpiChangedAfterParentEvent;
    }
    case WM_GETDPISCALEDSIZE:{
        return LWindows::DpiScaledSizeEvent;
    }
    case WM_ENTERSIZEMOVE:{
        return LWindows::EnterSizeMoveEvent;
    }
    case WM_EXITSIZEMOVE:{
        return LWindows::ExitSizeMoveEvent;
    }
    default:{
        break;
    }
    }

    if (message >= WM_NCMOUSEMOVE && message <= WM_NCMBUTTONDBLCLK){
        return LWindows::NonClientMouseEvent; //
    }
    if ((message >= WM_MOUSEFIRST && message <= WM_MOUSELAST)
        || (message >= WM_XBUTTONDOWN && message <= WM_XBUTTONDBLCLK)){
        return LWindows::MouseEvent;
    }
    if (message >= WM_NCPOINTERUPDATE && message <= WM_NCPOINTERUP){
        return LWindows::NonClientPointerEvent;
    }
    if (message >= WM_POINTERUPDATE && message <= WM_POINTERHWHEEL){
        return LWindows::PointerEvent;
    }
    // if (message == WM_TASKBAR_BUTTON_CREATED){
    //     return LWindows::TaskbarButtonCreated;
    // }
    return LWindows::UnknownEvent;
}

LK_NAMESPACE_END
#endif // LWINDOWS_H
