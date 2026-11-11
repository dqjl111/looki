#include "lobject.h"
#include "lthreaddata.h"
#include "lthread.h"
#include "lobjectdata.h"
#include "time/ltimerpool.h"
#include "lcoreapplication.h"
#include "lstringutil.h"
#include <atomic>

LK_NAMESPACE_BEGIN

LObject::LObject() : m_data(std::make_shared<LObjectData>())
{
    m_data->m_Obj.store(this);
}

LObject::~LObject()
{
    m_data->m_Obj.store(nullptr);
}


void LObject::moveTothread(LIThread *thread)
{
    bool hasObjectPoll = disconnectObjectPoll();
    m_data->m_threadData = thread->threadData();
    if(hasObjectPoll){
        connectObjectPoll();
    }
}

LIThread *LObject::thread()
{
    return m_data->thread();
}

std::shared_ptr<LIThreadData> LObject::threadData()
{
    return m_data->threadData();
}

bool LObject::handleEvent(std::shared_ptr<LEvent> e)
{
    switch (e->type()) {
    case LEvent::Timer:{
        break;
    }
    case LEvent::ChildAdded:
    case LEvent::ChildPolished:
    case LEvent::ChildRemoved:{
        break;
    }
    case LEvent::DeferredDelete:{
        break;
    }
    case LEvent::MetaCall:{
        break;
    }
    case LEvent::ThreadChange: {
        break;
    }
    default:{
        return false;
    }
    }
    return true;
}

bool LObject::eventFilter(std::shared_ptr<LObject> watched, std::shared_ptr<LEvent> event)
{
    return false;
}

LVariant LObject::inputMethodQuery(LInputMethodQuery, const LVariant &args) const
{
    return LVariant();
}

std::shared_ptr<LEventTimer> LObject::startTimer(std::chrono::milliseconds interval, bool isRepeat)
{
    std::shared_ptr<LObject> tThis = getPtrT<LObject>();
    std::shared_ptr<LTimerPool> pool = LCoreApplication::timePool();
    std::shared_ptr<LEventTimer>  handle = std::make_shared<LEventTimer>(tThis, pool);
    handle->setRepeated(isRepeat);
    handle->setInterval(interval);
    handle->initCallBack();
    pool->registerTimer(handle);
    handle->start();
    return handle;
}

const std::string& LObject::name() const
{
    if(m_data->m_extraData != nullptr){
        return m_data->m_extraData->objectName;
    }
    return LStringUtil::empty;
}

void LObject::setName(const std::string &v)
{
    if(m_data->m_extraData == nullptr){
        m_data->m_extraData = std::make_shared<LObjectData::ExtraData>();
    }
    m_data->m_extraData->objectName = v;
}

const std::wstring LObject::wName() const
{
    return LStringUtil::utf8ToWide(name());
}

void LObject::setWName(const std::wstring &v)
{
    return setName(LStringUtil::wideToUtf8(v));
}


void LObject::addEventFilter(std::shared_ptr<LObject> obj)
{
    if(obj == nullptr || obj.get() == this){
        return;
    }

    if(m_data->m_extraData == nullptr){
        m_data->m_extraData = std::make_shared<LObjectData::ExtraData>();
    }

    m_data->m_extraData->eventFilters.push_back(obj);
}

void LObject::removeEventFilter(std::shared_ptr<LObject> obj)
{
    if(m_data->m_extraData != nullptr){
        auto isObj = [obj](std::weak_ptr<LObject> o) { return o.lock() == obj; };
        m_data->m_extraData->eventFilters.remove_if(isObj);
    }
}

void LObject::clearEventFilter()
{
    if(m_data->m_extraData != nullptr){
        m_data->m_extraData->eventFilters.clear();
    }
}

bool LObject::connectObjectPoll()
{
    std::shared_ptr<LIThreadData> threadData = m_data->m_threadData.lock();
    if(threadData != nullptr){
        std::shared_ptr<LEventDispatcher> dispatcher = threadData->ensureEventDispatcher();
        dispatcher->sigObjectPoll.connect(this, &LObject::objectPoll, ConnectionType::DirectUnique);
        return true;
    }
    return false;
}

bool LObject::disconnectObjectPoll()
{
    std::shared_ptr<LIThreadData> threadData = m_data->m_threadData.lock();
    if(threadData != nullptr){
        if(threadData->m_eventDispatcher != nullptr){
            return threadData->m_eventDispatcher->sigObjectPoll.disconnect(this);
        }
    }
    return false;
}

void LObject::objectPoll()
{
    runObjectPoll();
}

bool LObject::hasObjectPoll()
{
    std::shared_ptr<LIThreadData> threadData = m_data->m_threadData.lock();
    if(threadData != nullptr){
        if(threadData->m_eventDispatcher != nullptr){
            return threadData->m_eventDispatcher->sigObjectPoll.hasConnect(this);
        }
    }
    return false;
}

void LObject::runObjectPoll()
{

}


LK_NAMESPACE_END


