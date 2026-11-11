#include "lthreaddata.h"
#include "lthread.h"
#include "leventdispatcher.h"
#include "ldebug.h"

LK_NAMESPACE_BEGIN

void LIThreadData::pushEvent(std::shared_ptr<LAsyncEvent> event)
{
    //lInfo() << "push event";
    std::unique_lock<std::mutex> lock(m_eventMutex);
    m_events.push_back(event);
    m_cond.notify_one();
}

bool LIThreadData::isThreadId(const std::thread::id &id)
{
    std::thread *th = stdThread();
    if(th == nullptr || id == LThreadDataRegistry::instance().mainThread()->id){
        return m_thread.load(std::memory_order_relaxed) == LThreadDataRegistry::instance().mainThread();
    }else{
        return th->get_id() == id;
    }
}

std::thread *LIThreadData::stdThread()
{
    return nullptr;
}

std::thread *LThreadData::stdThread()
{
    return &m_stdThread;
}


LK_NAMESPACE_END





