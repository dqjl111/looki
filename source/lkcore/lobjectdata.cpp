#include "lobjectdata.h"
#include "lthread.h"

LK_NAMESPACE_BEGIN

LObjectData::LObjectData()
{
    m_threadData = LThreadDataRegistry::instance().currentThreadData();
}

LIThread *LObjectData::thread()
{
    std::shared_ptr<LIThreadData> threadData = m_threadData.lock();
    if(threadData != nullptr){
        return threadData->m_thread.load(std::memory_order_relaxed);
    }
    return LThreadDataRegistry::instance().mainThread();
}

std::shared_ptr<LIThreadData> LObjectData::threadData()
{
    std::shared_ptr<LIThreadData> threadData = m_threadData.lock();
    if(threadData != nullptr){
        return threadData;
    }
    return LThreadDataRegistry::instance().mainThreadData();
}

LK_NAMESPACE_END

