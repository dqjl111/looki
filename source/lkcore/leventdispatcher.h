#ifndef LEVENTDISPATCHER_H
#define LEVENTDISPATCHER_H
#include "lobject.h"
#include <string>
#include <queue>
#include "leventloop.h"
#include "lsignal.h"
#include "lnativeeventfilter.h"
#include "lcoreenums.h"
LK_NAMESPACE_BEGIN

class LKCOREDEF LEventDispatcher : public LObject
{
public:
    LEventDispatcher();

    static std::shared_ptr<LEventDispatcher> instance(LIThread *thread = nullptr);
    virtual bool processEvents(LFlags<LProcessEvents> flags= LProcessEvents::All);
    bool filterNativeEvent(const std::string &eventType, void *message, lintptr *result);

    void installNativeEventFilter(LNativeEventFilter *filterObj);
    void removeNativeEventFilter(LNativeEventFilter *filterObj);

    virtual void wakeUp();

    LSignal<> sigObjectPoll;

    const static inline std::string& nativeEventType() {
        static std::string s = "windows_generic_MSG";
        return s;
    }
protected:
    std::list<LNativeEventFilter *> m_eventFilters;
    std::list<std::shared_ptr<LAsyncEvent> > m_events;

};


LK_NAMESPACE_END
#endif // LEVENTDISPATCHER_H
