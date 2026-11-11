#ifndef LWINDOWSMOUSEHANDLER_H
#define LWINDOWSMOUSEHANDLER_H
#include "lwindows.h"
#include "lkgui/lwindow.h"

LK_NAMESPACE_BEGIN

class LWindowsMouseHandler
{
public:
    LWindowsMouseHandler();

    bool translateMouseEvent(std::shared_ptr<LWindow> window,
                             HWND hwnd,
                             LWindows::WindowsEventType t,
                             MSG msg,
                             LRESULT *result);
    bool translateMouseWheelEvent(std::shared_ptr<LWindow> window, MSG msg);

    static LFlags<LMouseButton> queryMouseButtons();
    static LFlags<LMouseButton> keyStateToMouseButtons(WPARAM);
    static LFlags<LKeyModifier> keyStateToModifiers(int);

    std::shared_ptr<LWindow> windowUnderMouse() const { return m_windowUnderMouse.lock(); }
    void clearWindowUnderMouse() { m_windowUnderMouse.reset(); }

    void clearEvents();

private:
    std::weak_ptr<LWindow> m_windowUnderMouse;
    std::weak_ptr<LWindow> m_trackedWindow;
//    std::unorder_map<DWORD, int> m_touchInputIDToTouchPointID;
//    std::unorder_map<int, LPoint<double>> m_lastTouchPositions;
//    LTouchDevice *m_touchDevice = nullptr;
    bool m_leftButtonDown = false;
    std::weak_ptr<LWindow> m_previousCaptureWindow;
    LEvent::Type m_lastEventType = LEvent::None;
    LMouseButton m_lastEventButton = LMouseButton::No;
};

LK_NAMESPACE_END

#endif // LWINDOWSMOUSEHANDLER_H
