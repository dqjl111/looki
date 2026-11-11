#include "lwindowspointerhandler.h"

LK_NAMESPACE_BEGIN

LWindowsPointerHandler::LWindowsPointerHandler()
{

}

bool LWindowsPointerHandler::translatePointerEvent(std::shared_ptr<LWindow> window,
                                                   HWND hwnd,
                                                   LWindows::WindowsEventType et,
                                                   MSG msg,
                                                   LRESULT *result)
{
    return true;
}

bool LWindowsPointerHandler::translateMouseEvent(std::shared_ptr<LWindow> window,
                                                 HWND hwnd,
                                                 LWindows::WindowsEventType et,
                                                 MSG msg,
                                                 LRESULT *result)
{
    return true;
}

void LWindowsPointerHandler::clearEvents()
{

}


LK_NAMESPACE_END
