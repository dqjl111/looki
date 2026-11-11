#ifndef LWINDOWSPOINTERHANDLER_H
#define LWINDOWSPOINTERHANDLER_H
#include "lwindows.h"
#include "lkgui/lwindow.h"

LK_NAMESPACE_BEGIN

class LWindowsPointerHandler
{
public:
    LWindowsPointerHandler();

    bool translatePointerEvent(std::shared_ptr<LWindow> window,
                               HWND hwnd,
                               LWindows::WindowsEventType et,
                               MSG msg,
                               LRESULT *result);
    bool translateMouseEvent(std::shared_ptr<LWindow> window,
                             HWND hwnd,
                             LWindows::WindowsEventType et,
                             MSG msg,
                             LRESULT *result);

    std::shared_ptr<LWindow> windowUnderMouse() const { return m_windowUnderPointer.lock(); }
    void clearWindowUnderMouse() { m_windowUnderPointer.reset(); }

    void clearEvents();

private:
    std::weak_ptr<LWindow> m_windowUnderPointer;
};

LK_NAMESPACE_END

#endif // LWINDOWSPOINTERHANDLER_H
