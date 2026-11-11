#include "lwindowsguieventdispatcher.h"
#include "lkgui/lwindowsysteminterface.h"

LK_NAMESPACE_BEGIN


LWindowsGuiEventDispatcher::LWindowsGuiEventDispatcher()
{
}

void LWindowsGuiEventDispatcher::sendPostedEvents(LFlags<LProcessEvents> flags)
{
    LEventDispatcherWin::sendPostedEvents(flags);
    LWindowSystemInterface::sendWinSysEvents(flags);
}


LK_NAMESPACE_END
