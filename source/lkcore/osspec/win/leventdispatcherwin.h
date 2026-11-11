#ifndef LEVENTDISPATCHERWIN_H
#define LEVENTDISPATCHERWIN_H
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "lkcore/leventdispatcher.h"


LK_NAMESPACE_BEGIN

enum {
    WM_LK_SENDPOSTEDEVENTS = WM_USER
};

class LWinMsgContext;
class LKCOREDEF LEventDispatcherWin : public LEventDispatcher
{
public:
    LEventDispatcherWin();
    virtual ~LEventDispatcherWin();

    virtual bool processEvents(LFlags<LProcessEvents> flags= LProcessEvents::All) override;
    virtual void wakeUp() override;
    virtual void init() override;
protected:
    virtual void sendPostedEvents(LFlags<LProcessEvents> flags= LProcessEvents::All);

    static LRESULT L_WIN_CALLBACK loukeWinInternalProc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp);
    static LRESULT L_WIN_CALLBACK loukeWindowMessageHook(int code, WPARAM wp, LPARAM lp);

    friend class LWinMsgContext;
    HWND m_internalHwnd = nullptr;
    std::atomic_bool m_wakeUps = false;
};

LK_NAMESPACE_END

#endif // LEVENTDISPATCHERWIN_H
