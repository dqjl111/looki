#include "leventdispatcher.h"
#include "lthreaddata.h"
#include "lobjectdata.h"
#include "lpostevent.h"
#include "ldebug.h"
#include "lthread.h"
#include "lcoreapplication.h"

LK_NAMESPACE_BEGIN


LEventDispatcher::LEventDispatcher()
{

}

std::shared_ptr<LEventDispatcher> LEventDispatcher::instance(LIThread *thread)
{
    if(thread != nullptr){
        std::shared_ptr<LIThreadData> threadData = thread->threadData();
        if(threadData != nullptr){
            return threadData->m_eventDispatcher;
        }
    }

    std::shared_ptr<LIThreadData>  threadData = LThreadDataRegistry::instance().currentThreadData();
    if(threadData != nullptr){
        return threadData->m_eventDispatcher;
    }
    return nullptr;
}

bool LEventDispatcher::processEvents(LFlags<LProcessEvents>  flags)
{
    std::shared_ptr<LIThreadData> threadData = m_data->m_threadData.lock();
    if(threadData != nullptr){
        if(!(flags & LProcessEvents::ExcludeObjectPoll)){
            sigObjectPoll.emt();
        }

        {
            std::unique_lock<std::mutex> lock(threadData->m_eventMutex);
            std::copy(threadData->m_events.begin(), threadData->m_events.end(), std::back_inserter(m_events));
            threadData->m_events.clear();
        }

        while (!m_events.empty()) {
            if (threadData->m_state.load(std::memory_order_relaxed) !=  LIThreadData::Started){
                break;
            }

            std::shared_ptr<LAsyncEvent> event = m_events.front();
            m_events.pop_front();

            switch(event->asyncType()){
            case LAsyncEvent::Slot:{
                std::shared_ptr<LSlotEvent> tEvent
                    = std::dynamic_pointer_cast<LSlotEvent>(event);
                tEvent->invoke();
                break;
            }
            case LAsyncEvent::BlockSlot:{
                std::shared_ptr<LBlockSlotEvent> tEvent
                    = std::dynamic_pointer_cast<LBlockSlotEvent>(event);
                tEvent->invoke();
                break;
            }
            default:{
                std::shared_ptr<LPostEvent> tEvent
                    = std::dynamic_pointer_cast<LPostEvent>(event);
                std::shared_ptr<LObject> m_reciver = tEvent->m_receiver.lock();
                if(m_reciver != nullptr){
                    LCoreApplication::sendEvent(m_reciver,tEvent->m_event);
                }
                break;
            }
            }
        }

        return true;
    }
    return false;
}

bool LEventDispatcher::filterNativeEvent(const std::string &eventType, void *message, lintptr *result)
{
    for(LNativeEventFilter *filter : m_eventFilters){
        if (filter->nativeEventFilter(eventType, message, result)){
            return true;
        }
    }
    return false;
}

void LEventDispatcher::installNativeEventFilter(LNativeEventFilter *filterObj)
{
    if(filterObj != nullptr){
        m_eventFilters.push_front(filterObj);
    }
}

void LEventDispatcher::removeNativeEventFilter(LNativeEventFilter *filterObj)
{
    m_eventFilters.remove(filterObj);
}

void LEventDispatcher::wakeUp()
{
    std::shared_ptr<LIThreadData> threadData = m_data->m_threadData.lock();
    if(threadData != nullptr){
        threadData->m_cond.notify_all();
    }
}


LK_NAMESPACE_END

