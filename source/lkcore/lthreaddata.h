#ifndef LTHREADDATA_H
#define LTHREADDATA_H
#include <iostream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <atomic>
#include <memory>
#include <condition_variable>
#include <stack>
#include <list>
#include "lpostevent.h"


LK_NAMESPACE_BEGIN

class LEventLoop;
class LIThread;
class LEventDispatcher;
class LCoreApplication;
class LKCOREDEF LIThreadData {
public:
    LIThreadData()
        : m_thread(nullptr)
        , m_eventDispatcher(nullptr)
    {
    }

    inline bool hasEventDispatcher() const
    {
        return m_eventDispatcher != nullptr;
    }
    std::shared_ptr<LEventDispatcher> createEventDispatcher();
    std::shared_ptr<LEventDispatcher> ensureEventDispatcher()
    {
        if (m_eventDispatcher != nullptr){
            return m_eventDispatcher;
        }
        return createEventDispatcher();
    }

    void pushEvent(std::shared_ptr<LAsyncEvent> event);
    enum State {
        Starting,
        Started,
        Stopping,
        Stopped
    };

    std::atomic<LIThread*> m_thread;
    std::shared_ptr<LEventDispatcher> m_eventDispatcher;

    bool isThreadId(const std::thread::id& id);

    virtual std::thread* stdThread();
protected:
    std::list<std::shared_ptr<LEventLoop> > m_eventLoops;
    mutable std::mutex m_eventMutex;
    std::condition_variable m_cond;
    std::list<std::shared_ptr<LAsyncEvent> >m_events;
    std::atomic<State> m_state = Stopped;
    bool m_requiresCoreApplication = true;

    friend class LEventDispatcher;
    friend class LEventLoop;
    friend class LIThread;
    friend class LMainThread;
    friend class LThread;
    friend class LTimerPool;
    friend class LCoreApplication;
};

class LKCOREDEF LThreadData : public LIThreadData {
public:
    LThreadData(){
    }

    virtual std::thread* stdThread() override;
protected:
    std::thread m_stdThread;
    friend class LThread;
};

LK_NAMESPACE_END
#endif // LTHREADDATA_H
