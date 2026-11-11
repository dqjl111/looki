#include "leventloop.h"
#include "lthreaddata.h"
#include <shared_mutex>
#include <condition_variable>
#include "lobjectdata.h"
#include "ldebug.h"
#include "leventdispatcher.h"

LK_NAMESPACE_BEGIN

LEventLoop::LEventLoop(){
    std::shared_ptr<LIThreadData> threadData = m_data->threadData();
    threadData->ensureEventDispatcher();
}

bool LEventLoop::isRunning() const
{
    return m_isRunning;
}

void LEventLoop::quit(int exitCode)
{
    m_exitCode = exitCode;
    m_exit.store(true);
    std::shared_ptr<LIThreadData> threadData = m_data->threadData();
    if(threadData != nullptr){
        threadData->m_cond.notify_all();
    }
}

int LEventLoop::exec()
{
    //lInfo() << "Start exec";

    std::shared_ptr<LIThreadData> threadData = m_data->threadData();
    if(threadData == nullptr){
        return -1;
    }

    if (threadData->m_state.load(std::memory_order_relaxed) !=  LIThreadData::Started){
        return -1;
    }

    if (m_isRunning) {
        lWarn() << "LEventLoop::exec has already called";
        return -1;
    }

    {
        m_isRunning = true;
        m_exit.store(false);
        threadData->m_eventLoops.push_front(getPtrT<LEventLoop>());
    }

    int waitTime = m_maxWaitDuration;

    while (!m_exit.load(std::memory_order_relaxed)){
        if (!threadData->hasEventDispatcher()){
            lWarn() << "Exec lack of event dispatcher!";
            break;
        }

        if(threadData->m_events.empty()){
            if(waitTime >0){
                std::unique_lock<std::mutex> lock(threadData->m_eventMutex);
                threadData->m_cond.wait_for(lock, std::chrono::milliseconds(waitTime));
            }
            //动态增加等待时间
            waitTime = (waitTime < m_maxWaitDuration) ? waitTime+1 : m_maxWaitDuration;

            if(m_exit.load(std::memory_order_relaxed)){
                break;
            }

            if (threadData->m_state.load(std::memory_order_relaxed) !=  LIThreadData::Started){
                break;
            }
        }else{
            waitTime = (waitTime >0) ? waitTime-1 : 0;//动态减小等待时间
        }

        threadData->m_eventDispatcher->processEvents();
    }

    {
        m_isRunning = false;
        threadData->m_eventLoops.pop_front();
    }

    //lInfo() << "Exit exec";
    return m_exitCode;
}


LK_NAMESPACE_END
