#include "lwindowsmousehandler.h"
#include "lkgui/lwindowsysteminterface.h"
#include "lwindowswin.h"
#include "lwindowsintegration.h"
#include "lkgui/lguiapplication.h"
#include <windowsx.h>

LK_NAMESPACE_BEGIN
static LPoint<int> lastMouseMovePos;

struct LWindowsMouseEventInfo {
    LEvent::Type type;
    LMouseButton button;
};

static inline LMouseButton extraButton(WPARAM wParam) // for WM_XBUTTON...
{
    return GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? LMouseButton::Back : LMouseButton::Forward;
}

static inline void compressMouseMove(MSG *msg)
{
    // Compress mouse move events
    if (msg->message == WM_MOUSEMOVE) {
        MSG mouseMsg;
        while (PeekMessage(&mouseMsg, msg->hwnd, WM_MOUSEFIRST,
                           WM_MOUSELAST, PM_NOREMOVE)) {
            if (mouseMsg.message == WM_MOUSEMOVE) {
                // Since the Windows PeekMessage() function doesn't
                // correctly return the wParam for WM_MOUSEMOVE events
                // if there is a key release event in the queue
                // _before_ the mouse event, we have to also consider
                // key release events:
                MSG keyMsg;
                bool done = false;
                while (PeekMessage(&keyMsg, nullptr, WM_KEYFIRST, WM_KEYLAST,
                                   PM_NOREMOVE)) {
                    if (keyMsg.time < mouseMsg.time) {
                        if ((keyMsg.lParam & 0xC0000000) == 0x40000000) {
                            PeekMessage(&keyMsg, nullptr, keyMsg.message,
                                        keyMsg.message, PM_REMOVE);
                        } else {
                            done = true;
                            break;
                        }
                    } else {
                        break; // no key event before the WM_MOUSEMOVE event
                    }
                }
                if (done){
                    break;
                }

                msg->lParam = mouseMsg.lParam;
                msg->wParam = mouseMsg.wParam;
                // Extract the x,y coordinates from the lParam as we do in the WndProc
                msg->pt.x = GET_X_LPARAM(mouseMsg.lParam);
                msg->pt.y = GET_Y_LPARAM(mouseMsg.lParam);
                LWindowsWin::clientToScreen(msg->hwnd, &(msg->pt));
                // Remove the mouse move message
                PeekMessage(&mouseMsg, msg->hwnd, WM_MOUSEMOVE,
                            WM_MOUSEMOVE, PM_REMOVE);
            } else {
                break; // there was no more WM_MOUSEMOVE event
            }
        }
    }
}


static inline LWindowsMouseEventInfo eventFromMsg(const MSG &msg)
{
    switch (msg.message) {
    case WM_MOUSEMOVE:{
        return {LEvent::MouseMove, LMouseButton::No};
    }
    case WM_LBUTTONDOWN:{
        return {LEvent::MouseButtonPress, LMouseButton::Left};
    }
    case WM_LBUTTONUP:{
        return {LEvent::MouseButtonRelease, LMouseButton::Left};
    }
    case WM_LBUTTONDBLCLK:{ //Does not handle double clicks, send as press
        return {LEvent::MouseButtonPress, LMouseButton::Left};
    }
    case WM_MBUTTONDOWN:{
        return {LEvent::MouseButtonDblClick, LMouseButton::Middle};
    }
    case WM_MBUTTONUP:{
        return {LEvent::MouseButtonRelease, LMouseButton::Middle};
    }
    case WM_MBUTTONDBLCLK:{
        return {LEvent::MouseButtonDblClick, LMouseButton::Middle};
    }
    case WM_RBUTTONDOWN:{
        return {LEvent::MouseButtonPress, LMouseButton::Right};
    }
    case WM_RBUTTONUP:{
        return {LEvent::MouseButtonRelease, LMouseButton::Right};
    }
    case WM_RBUTTONDBLCLK:{
        return {LEvent::MouseButtonDblClick, LMouseButton::Right};
    }
    case WM_XBUTTONDOWN:{
        return {LEvent::MouseButtonPress, extraButton(msg.wParam)};
    }
    case WM_XBUTTONUP:{
        return {LEvent::MouseButtonRelease, extraButton(msg.wParam)};
    }
    case WM_XBUTTONDBLCLK:{
        return {LEvent::MouseButtonDblClick, extraButton(msg.wParam)};
    }
    case WM_NCMOUSEMOVE:{
        return {LEvent::NonClientAreaMouseMove, LMouseButton::No};
    }
    case WM_NCLBUTTONDOWN:{
        return {LEvent::NonClientAreaMouseButtonPress, LMouseButton::Left};
    }
    case WM_NCLBUTTONUP:{
        return {LEvent::NonClientAreaMouseButtonRelease, LMouseButton::Left};
    }
    case WM_NCLBUTTONDBLCLK:{
        return {LEvent::NonClientAreaMouseButtonDblClick, LMouseButton::Left};
    }
    case WM_NCMBUTTONDOWN:{
        return {LEvent::NonClientAreaMouseButtonPress, LMouseButton::Middle};
    }
    case WM_NCMBUTTONUP:{
        return {LEvent::NonClientAreaMouseButtonRelease, LMouseButton::Middle};
    }
    case WM_NCMBUTTONDBLCLK:{
        return {LEvent::NonClientAreaMouseButtonDblClick, LMouseButton::Middle};
    }
    case WM_NCRBUTTONDOWN:{
        return {LEvent::NonClientAreaMouseButtonPress, LMouseButton::Right};
    }
    case WM_NCRBUTTONUP:{
        return {LEvent::NonClientAreaMouseButtonRelease, LMouseButton::Right};
    }
    case WM_NCRBUTTONDBLCLK:{
        return {LEvent::NonClientAreaMouseButtonDblClick, LMouseButton::Right};
    }
    default: // WM_MOUSELEAVE
        break;
    }
    return {LEvent::None, LMouseButton::No};
}


static bool isValidWheelReceiver(std::shared_ptr<LWindow> candidate)
{
    if (candidate != nullptr) {
        const std::shared_ptr<LWindow> toplevel = LWindowsWin::topLevelOf(candidate);
        if (toplevel->handle() && toplevel->handle()->isForeignWindow()){
            return true;
        }
        const std::shared_ptr<LWindowsWin> ww=  LWindowsWin::windowsWindowOf(toplevel);
        if (ww != nullptr){
            return !ww->testFlag(LWindowsWin::BlockedByModal);
        }
    }
    return false;
}

static void redirectWheelEvent(std::shared_ptr<LWindow> window,
                               const LPoint<int> &globalPos,
                               int delta,
                               LOrientation orientation,
                               LFlags<LKeyModifier> mods)
{
    // Redirect wheel event to one of the following, in order of preference:
    // 1) The window under mouse
    // 2) The window receiving the event
    // If a window is blocked by modality, it can't get the event.

    std::shared_ptr<LWindow> receiver = LWindowsScreen::windowAt(globalPos, CWP_SKIPINVISIBLE);
    while (receiver && receiver->flags().testFlag(LWindowType::WindowTransparentForInput)){
        receiver = receiver->parent();
    }
    bool handleEvent = true;
    if (!isValidWheelReceiver(receiver)) {
        receiver = window;
        if (!isValidWheelReceiver(receiver)){
            handleEvent = false;
        }
    }

    if (handleEvent) {
        const LPoint<int> point = (orientation == LOrientation::Vertical) ? LPoint<int>(0, delta) : LPoint<int>(delta, 0);
        LWindowSystemInterface::handleWheelEvent(receiver,
                                                 LWindowsGeometryHint::mapFromGlobal(receiver, globalPos).toPointF(),
                                                 globalPos.toPointF(),
                                                 LPoint<int>(), point,
                                                 mods);
    }
}



LWindowsMouseHandler::LWindowsMouseHandler()
{

}

bool LWindowsMouseHandler::translateMouseEvent(std::shared_ptr<LWindow> window,
                                               HWND hwnd,
                                               LWindows::WindowsEventType et,
                                               MSG msg,
                                               LRESULT *result)
{
    enum : uint64_t { signatureMask = 0xffffff00, miWpSignature = 0xff515700 };

    if (et == LWindows::MouseWheelEvent){
        return translateMouseWheelEvent(window, msg);
    }

    LPoint<int> winEventPosition(GET_X_LPARAM(msg.lParam), GET_Y_LPARAM(msg.lParam));

    LPoint<int> clientPosition;
    LPoint<int> globalPosition;
    if ((uint32_t)et & (uint32_t)LWindows::NonClientEventFlag) {
        globalPosition = winEventPosition;
        clientPosition = LWindowsGeometryHint::mapFromGlobal(hwnd, globalPosition);
    } else {
        clientPosition = winEventPosition;
        globalPosition = LWindowsGeometryHint::mapToGlobal(hwnd, winEventPosition);
    }

    // Windows sends a mouse move with no buttons pressed to signal "Enter"
    // when a window is shown over the cursor. Discard the event and only use
    // it for generating LEvent::Enter to be consistent with other platforms -
    // X11 and macOS.
    bool discardEvent = false;
    std::shared_ptr<LWindow> winUnderMouse = m_windowUnderMouse.lock();

    if (msg.message == WM_MOUSEMOVE) {
        const bool samePosition = globalPosition == lastMouseMovePos;
        lastMouseMovePos = globalPosition;
        if (msg.wParam == 0 && ((winUnderMouse==nullptr) || samePosition)){
            discardEvent = true;
        }
    }

    LMouseEventSource source = LMouseEventSource::NotSynthesized;
    const LFlags<LKeyModifier> keyModifiers = LWindowsKeyMapper::queryKeyboardModifiers();
    const LWindowsMouseEventInfo mouseEvent = eventFromMsg(msg);
    LFlags<LMouseButton> buttons;

    if (mouseEvent.type >= LEvent::NonClientAreaMouseMove && mouseEvent.type <= LEvent::NonClientAreaMouseButtonDblClick){
        buttons = queryMouseButtons();
    }else{
        buttons = keyStateToMouseButtons(msg.wParam);
    }

    // When the left/right mouse buttons are pressed over the window title bar
    // WM_NCLBUTTONDOWN/WM_NCRBUTTONDOWN messages are received. But no UP
    // messages are received on release, only WM_NCMOUSEMOVE/WM_MOUSEMOVE.
    // We detect it and generate the missing release events here.
    // The last event vars are cleared on LWindowsCtx::handleExitSizeMove()
    // to avoid generating duplicated release events.
    if (m_lastEventType == LEvent::NonClientAreaMouseButtonPress
        && (mouseEvent.type == LEvent::NonClientAreaMouseMove || mouseEvent.type == LEvent::MouseMove)
        && (m_lastEventButton & buttons) == LMouseButton::No) {
        if (mouseEvent.type == LEvent::NonClientAreaMouseMove) {
            LWindowSystemInterface::handleFrameStrutMouseEvent(window,
                                                               clientPosition.toPointF(),
                                                               globalPosition.toPointF(),
                                                               buttons, m_lastEventButton,
                                                               LEvent::NonClientAreaMouseButtonRelease, keyModifiers, source);
        } else {
            LWindowSystemInterface::handleMouseEvent(window,
                                                     clientPosition.toPointF(),
                                                     globalPosition.toPointF(),
                                                     buttons, m_lastEventButton,
                                                     LEvent::MouseButtonRelease, keyModifiers, source);
        }
    }
    m_lastEventType = mouseEvent.type;
    m_lastEventButton = mouseEvent.button;

    if (mouseEvent.type >= LEvent::NonClientAreaMouseMove && mouseEvent.type <= LEvent::NonClientAreaMouseButtonDblClick) {
        LWindowSystemInterface::handleFrameStrutMouseEvent(window,
                                                           clientPosition.toPointF(),
                                                           globalPosition.toPointF(),
                                                           buttons, mouseEvent.button, mouseEvent.type,
                                                           keyModifiers, source);
        return false; // Allow further event processing (dragging of windows).
    }

    *result = 0;
    std::shared_ptr<LWindow> trackedWindow = m_trackedWindow.lock();
    if (msg.message == WM_MOUSELEAVE) {
        // When moving out of a window, WM_MOUSEMOVE within the moved-to window is received first,
        // so if m_trackedWindow is not the window here, it means the cursor has left the
        // application.
        if (window == trackedWindow) {
            std::shared_ptr<LWindow> leaveTarget = winUnderMouse != nullptr ? winUnderMouse : trackedWindow;
            LWindowSystemInterface::handleLeaveEvent(leaveTarget);;
            m_trackedWindow.reset();
            m_windowUnderMouse.reset();
        }
        return true;
    }

    std::shared_ptr<LWindowsWin> platformWindow =
        std::dynamic_pointer_cast<LWindowsWin>(window->handle());

    // If the window was recently resized via mouse doubleclick on the frame or title bar,
    // we don't get WM_LBUTTONDOWN or WM_LBUTTONDBLCLK for the second click,
    // but we will get at least one WM_MOUSEMOVE with left button down and the WM_LBUTTONUP,
    // which will result undesired mouse press and release events.
    // To avoid those, we ignore any events with left button down if we didn't
    // get the original WM_LBUTTONDOWN/WM_LBUTTONDBLCLK.
    if (msg.message == WM_LBUTTONDOWN || msg.message == WM_LBUTTONDBLCLK) {
        m_leftButtonDown = true;
    } else {
        const bool actualLeftDown = buttons & LMouseButton::Left;
        if (!m_leftButtonDown && actualLeftDown) {
            // Autocapture the mouse for current window to and ignore further events until release.
            // Capture is necessary so we don't get WM_MOUSELEAVEs to confuse matters.
            // This autocapture is released normally when button is released.
            if (!platformWindow->hasMouseCapture()) {
                platformWindow->applyCursor();
                platformWindow->setMouseGrabEnabled(true);
                platformWindow->setFlag(LWindowsWin::AutoMouseCapture);
            }
            m_previousCaptureWindow = window;
            return true;
        }
        if (m_leftButtonDown && !actualLeftDown){
            m_leftButtonDown = false;
        }
    }

    // In this context, neither an invisible nor a transparent window (transparent regarding mouse
    // events, "click-through") can be considered as the window under mouse.
    std::shared_ptr<LWindow> currentWindowUnderMouse = platformWindow->hasMouseCapture() ?
                                           LWindowsScreen::windowAt(globalPosition, CWP_SKIPINVISIBLE | CWP_SKIPTRANSPARENT) : window;
    while (currentWindowUnderMouse && currentWindowUnderMouse->flags() & LWindowType::WindowTransparentForInput)
        currentWindowUnderMouse = currentWindowUnderMouse->parent();

    if (!currentWindowUnderMouse) {
        const LRect<int> clientRect(0, 0, window->size());
        if (clientRect.contains(winEventPosition)){
            currentWindowUnderMouse = window;
        }
    }

    compressMouseMove(&msg);
    // Expects the platform plugin to capture the mouse on
    // any button press until release.
    if (!platformWindow->hasMouseCapture()
        && (mouseEvent.type == LEvent::MouseButtonPress || mouseEvent.type == LEvent::MouseButtonDblClick)) {
        platformWindow->setMouseGrabEnabled(true);
        platformWindow->setFlag(LWindowsWin::AutoMouseCapture);

        // Implement "Click to focus" for native child windows (unless it is a native widget window).
        if (!window->isTopLevel() && !window->isWidgetWindow() && LGuiApplication::focusWindow() != window){
            window->requestActivate();
        }
    } else if (platformWindow->hasMouseCapture()
               && platformWindow->testFlag(LWindowsWin::AutoMouseCapture)
               && mouseEvent.type == LEvent::MouseButtonRelease
               && !buttons) {
        platformWindow->setMouseGrabEnabled(false);
    }

    const bool hasCapture = platformWindow->hasMouseCapture();
    const bool currentNotCapturing = hasCapture && currentWindowUnderMouse != window;
    // Enter new window: track to generate leave event.
    // If there is an active capture, only track if the current window is capturing,
    // so we don't get extra leave when cursor leaves the application.
    if (window != trackedWindow && !currentNotCapturing) {
        TRACKMOUSEEVENT tme;
        tme.cbSize = sizeof(TRACKMOUSEEVENT);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        tme.dwHoverTime = HOVER_DEFAULT; //
        m_trackedWindow =  window;
    }

    std::shared_ptr<LWindow> previousCaptureWindow = m_previousCaptureWindow.lock();

    // No enter or leave events are sent as long as there is an autocapturing window.
    if (!hasCapture || !platformWindow->testFlag(LWindowsWin::AutoMouseCapture)) {
        // Leave is needed if:
        // 1) There is no capture and we move from a window to another window.
        //    Note: Leaving the application entirely is handled in WM_MOUSELEAVE case.
        // 2) There is capture and we move out of the capturing window.
        // 3) There is a new capture and we were over another window.
        if (((winUnderMouse!= nullptr) && winUnderMouse != currentWindowUnderMouse
             && (!hasCapture || window == winUnderMouse))
            || (hasCapture && previousCaptureWindow != window && winUnderMouse
                && winUnderMouse != window)) {

            LWindowSystemInterface::handleLeaveEvent(winUnderMouse);
            if (currentNotCapturing) {
                // Clear tracking if capturing and current window is not the capturing window
                // to avoid leave when mouse actually leaves the application.
                m_trackedWindow.reset();
                // We are not officially in any window, but we need to set some cursor to clear
                // whatever cursor the left window had, so apply the cursor of the capture window.
                platformWindow->applyCursor();
            }
        }
        // Enter is needed if:
        // 1) There is no capture and we move to a new window.
        // 2) There is capture and we move into the capturing window.
        // 3) The capture just ended and we are over non-capturing window.
        if ((currentWindowUnderMouse && winUnderMouse != currentWindowUnderMouse
             && (!hasCapture || currentWindowUnderMouse == window))
            || (previousCaptureWindow && window != previousCaptureWindow && currentWindowUnderMouse
                && currentWindowUnderMouse != previousCaptureWindow)) {
            LPoint<int> localPosition;
            if (std::shared_ptr<LWindowsWin> wumPlatformWindow = LWindowsWin::windowsWindowOf(currentWindowUnderMouse)) {
                localPosition = wumPlatformWindow->mapFromGlobal(globalPosition);
                wumPlatformWindow->applyCursor();
            }
            LWindowSystemInterface::handleEnterEvent(currentWindowUnderMouse,
                                                     localPosition.toPointF(),
                                                     globalPosition.toPointF());
        }
        // We need to track m_windowUnderMouse separately from m_trackedWindow, as
        // Windows mouse tracking will not trigger WM_MOUSELEAVE for leaving window when
        // mouse capture is set.
        m_windowUnderMouse = currentWindowUnderMouse;
    }

    if (!discardEvent && mouseEvent.type != LEvent::None) {
        LWindowSystemInterface::handleMouseEvent(window,
                                                 winEventPosition.toPointF(), globalPosition.toPointF(),
                                                 buttons, mouseEvent.button, mouseEvent.type,
                                                 keyModifiers, source);
    }
    if(hasCapture){
        m_previousCaptureWindow = window;
    }else{
        m_previousCaptureWindow.reset();
    }
    // Force synchronous handling for the extra buttons so that WM_APPCOMMAND
    // is sent for unhandled WM_XBUTTONDOWN.
    return (msg.message != WM_XBUTTONUP && msg.message != WM_XBUTTONDOWN && msg.message != WM_XBUTTONDBLCLK)
           || LWindowSystemInterface::flushWinSysEvents();
}

LFlags<LMouseButton> LWindowsMouseHandler::queryMouseButtons()
{
    LFlags<LMouseButton> result;
    const bool mouseSwapped = GetSystemMetrics(SM_SWAPBUTTON);
    if (GetAsyncKeyState(VK_LBUTTON) < 0){
        result |= mouseSwapped ? LMouseButton::Right: LMouseButton::Left;
    }
    if (GetAsyncKeyState(VK_RBUTTON) < 0){
        result |= mouseSwapped ? LMouseButton::Left : LMouseButton::Right;
    }
    if (GetAsyncKeyState(VK_MBUTTON) < 0){
        result |= LMouseButton::Middle;
    }
    if (GetAsyncKeyState(VK_XBUTTON1) < 0){
        result |= LMouseButton::X1;
    }
    if (GetAsyncKeyState(VK_XBUTTON2) < 0){
        result |= LMouseButton::X2;
    }
    return result;
}

LFlags<LMouseButton> LWindowsMouseHandler::keyStateToMouseButtons(WPARAM wParam)
{
    LFlags<LMouseButton> mb(LMouseButton::No);
    if (wParam & MK_LBUTTON){
        mb |= LMouseButton::Left;
    }
    if (wParam & MK_MBUTTON){
        mb |= LMouseButton::Middle;
    }
    if (wParam & MK_RBUTTON){
        mb |= LMouseButton::Right;
    }
    if (wParam & MK_XBUTTON1){
        mb |= LMouseButton::X1;
    }
    if (wParam & MK_XBUTTON2){
        mb |= LMouseButton::X2;
    }
    return mb;
}

LFlags<LKeyModifier> LWindowsMouseHandler::keyStateToModifiers(int wParam)
{
    LFlags<LKeyModifier> mods(LKeyModifier::No);
    if (wParam & MK_CONTROL){
        mods |= LKeyModifier::Ctrl;
    }
    if (wParam & MK_SHIFT){
        mods |= LKeyModifier::Shift;
    }
    if (GetKeyState(VK_MENU) < 0){
        mods |= LKeyModifier::Alt;
    }
    return mods;
}

void LWindowsMouseHandler::clearEvents()
{
    m_lastEventType = LEvent::None;
    m_lastEventButton = LMouseButton::No;
}

bool LWindowsMouseHandler::translateMouseWheelEvent(std::shared_ptr<LWindow> window, MSG msg)
{
    const LFlags<LKeyModifier> mods = keyStateToModifiers(int(msg.wParam));

    int delta;
    if (msg.message == WM_MOUSEWHEEL || msg.message == WM_MOUSEHWHEEL){
        delta = GET_WHEEL_DELTA_WPARAM(msg.wParam);
    }else{
        delta = int(msg.wParam);
    }

    LOrientation orientation = (msg.message == WM_MOUSEHWHEEL
                                   || (mods & LKeyModifier::Alt)) ?
                                      LOrientation::Horizontal : LOrientation::Vertical;

    if (msg.message == WM_MOUSEHWHEEL){
        delta = -delta;
    }

    const LPoint<int> globalPos(GET_X_LPARAM(msg.lParam), GET_Y_LPARAM(msg.lParam));
    redirectWheelEvent(window, globalPos, delta, orientation, mods);

    return true;
}

LK_NAMESPACE_END
