#ifndef LWINDOWSGUIEVENTDISPATCHER_H
#define LWINDOWSGUIEVENTDISPATCHER_H
#include "lkcore/osspec/win/leventdispatcherwin.h"

LK_NAMESPACE_BEGIN

class LWindowsGuiEventDispatcher : public LEventDispatcherWin
{
public:
    LWindowsGuiEventDispatcher();

protected:
    virtual void sendPostedEvents(LFlags<LProcessEvents> flags= LProcessEvents::All);
};

LK_NAMESPACE_END

#endif // LWINDOWSGUIEVENTDISPATCHER_H
