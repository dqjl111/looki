#include "lcoreapplication.h"
#include "lthread.h"
#include "lobjectdata.h"
#include "lthreaddata.h"
#include "ldebug.h"
#include "leventdispatcher.h"
#include "lkcore/plugin/ldynamiclibrary.h"
#include <filesystem>

LK_NAMESPACE_BEGIN

LCoreApplication* LCoreApplication::m_self = nullptr;
uint32_t LCoreApplication::m_attribs = 0;
bool LCoreApplication::m_isAppRunning = false;
bool LCoreApplication::m_isAppClosing = false;

static bool lkQuitLockRefEnabled = true;

LCoreApplication::LCoreApplication(int &aargc,
                                   char **aargv):m_argc(aargc),m_argv(aargv)
{
    m_self = this;
    m_timepool = std::make_shared<LTimerPool>();
    LThreadDataRegistry::instance().mainThread()->id = std::this_thread::get_id();
}

LCoreApplication::~LCoreApplication()
{
    m_isAppClosing = true;
    m_eventDispatcher = nullptr;
    std::shared_ptr<LIThreadData> data = threadData();
    data->m_eventDispatcher = nullptr;
    m_isAppRunning = false;
}


void LCoreApplication::quit(int exitCode)
{
    std::shared_ptr<LIThreadData> data = threadData();
    for(std::shared_ptr<LEventLoop> eventLoop: data->m_eventLoops){
        eventLoop->quit(exitCode);
    }
}

void LCoreApplication::exit(int exitCode)
{
    if (m_self == nullptr) {
        return ;
    }
    m_self->quit(exitCode);
}

int LCoreApplication::exec()
{
    if (m_self == nullptr){
        return -1;
    }

    std::shared_ptr<LIThreadData>  threadData = m_self->threadData();
    if (threadData != LThreadDataRegistry::instance().currentThreadData()) {
        lWarn() << "LCoreApplication::exec: Must call in main thread.";
        return -1;
    }

    if (!threadData->m_eventLoops.empty()) {
        lWarn() << "LCoreApplication::exec: The event loop is already running";
        return -1;
    }

    threadData->m_state.store(LIThreadData::Started, std::memory_order_release);
    std::shared_ptr<LEventLoop> eventLoop = std::make_shared<LEventLoop>();
    m_self->m_inExec = true;
    int returnCode = eventLoop->exec();
    m_self->execCleanup();
    threadData->m_state.store(LIThreadData::Stopped, std::memory_order_release);
    return returnCode;
}

void LCoreApplication::setAttribute(LAppAttribute attribute, bool on)
{
    if (on){
        LCoreApplication::m_attribs |= 1 << uint32_t(attribute);
    }
    else{
        LCoreApplication::m_attribs &= ~(1 << uint32_t(attribute));
    }
}

std::shared_ptr<LTimerPool> LCoreApplication::timePool()
{
    return m_self->m_timepool;
}

bool LCoreApplication::notify(std::shared_ptr<LObject> receiver, std::shared_ptr<LEvent> event)
{
    if (closingDown()){
        return true;
    }
    return doNotify(receiver, event);
}

bool LCoreApplication::sendEvent(std::shared_ptr<LObject> receiver, std::shared_ptr<LEvent> event)
{
    if (event != nullptr){
        if(!event->posted()){
            event->setSpontaneous(false);
            event->markPosted();
            return notifyInternal(receiver, event);
        }
    }
    return false;
}

void LCoreApplication::postEvent(std::shared_ptr<LObject> receiver, std::shared_ptr<LEvent> event)
{
    if (receiver == nullptr || event == nullptr) {
        return;
    }

    if(!event->posted()){
        std::shared_ptr<LIThreadData> data = receiver->threadData();
        std::shared_ptr<LPostEvent> post = std::make_shared<LPostEvent>();
        post->m_event = event;
        post->m_receiver = receiver;
        data->pushEvent(post);
    }
}

std::shared_ptr<LEventDispatcher> LCoreApplication::createEventDispatcher()
{
    std::shared_ptr<LIThreadData> data = m_data->m_threadData.lock();
    if(data){
        return data->createEventDispatcher();
    }
    return nullptr;
}

void LCoreApplication::eventDispatcherReady()
{
}

bool LCoreApplication::doNotify(std::shared_ptr<LObject> receiver, std::shared_ptr<LEvent> event)
{
    if (receiver == nullptr) {                        // serious error
        lWarn()<< "LCoreApplication::notify: Unexpected null receiver";
        return true;
    }
    return receiver->isWidgetType() ? false : LCoreApplication::notifyHelper(receiver, event);
}

bool LCoreApplication::sendThroughApplicationEventFilters(std::shared_ptr<LObject> receiver, std::shared_ptr<LEvent> event)
{
    if (m_data->m_extraData != nullptr) {
        // application event filters are only called for objects in the GUI thread
        auto iter = m_data->m_extraData->eventFilters.begin();
        for(; iter != m_data->m_extraData->eventFilters.end(); ++iter) {
            std::shared_ptr<LObject> obj = iter->lock();
            if (obj == nullptr){
                continue;
            }
            if (obj->threadData() != threadData()) {
                lWarn()<<"LCoreApplication: Application event filter cannot be in a different thread.";
                continue;
            }
            if (obj->eventFilter(receiver, event)){
                return true;
            }
        }
    }
    return false;
}

bool LCoreApplication::sendThroughObjectEventFilters(std::shared_ptr<LObject> receiver, std::shared_ptr<LEvent> event)
{
    if (receiver.get() != LCoreApplication::instance()) {
        std::shared_ptr<LObjectData> tData = receiver->objData();
        if(tData->m_extraData != nullptr){
            auto iter = tData->m_extraData->eventFilters.begin();
            for(; iter != tData->m_extraData->eventFilters.end(); ++iter) {
                std::shared_ptr<LObject> obj = iter->lock();
                if (obj == nullptr){
                    continue;
                }
                if (obj->threadData() != receiver->threadData()) {
                    lWarn()<<"LCoreApplication: Application event filter cannot be in a different thread.";
                    continue;
                }
                if (obj->eventFilter(receiver, event)){
                    return true;
                }
            }
        }
    }
    return false;
}

bool LCoreApplication::sendSpontaneousEvent(std::shared_ptr<LObject> receiver, std::shared_ptr<LEvent> event)
{
    if (event != nullptr){
        event->setSpontaneous(true);
    }
    return notifyInternal(receiver, event);
}

bool LCoreApplication::forwardEvent(std::shared_ptr<LObject> receiver,
                                    std::shared_ptr<LEvent> event,
                                    std::shared_ptr<LEvent> orgEvent)
{
    if (event!= nullptr && orgEvent != nullptr){
        event->setSpontaneous(orgEvent->spontaneous());
    }
    return notifyInternal(receiver, event);
}

std::filesystem::path LCoreApplication::applicationFilePath()
{
    std::filesystem::path p = lAppFileName();
    return p;
}

void LCoreApplication::init()
{
    std::shared_ptr<LIThreadData> data = threadData();
    if (m_eventDispatcher == nullptr){
        m_eventDispatcher = createEventDispatcher();
    }

    data->m_eventDispatcher = m_eventDispatcher;
    eventDispatcherReady();
    m_isAppRunning = true;
}

void LCoreApplication::processEvents()
{
    std::shared_ptr<LIThreadData> data = LThreadDataRegistry::instance().currentThreadData();
    if (!data->hasEventDispatcher()){
        return;
    }
    data->m_eventDispatcher->processEvents();
}

bool LCoreApplication::isQuitLockEnabled()
{
    return lkQuitLockRefEnabled;
}

void LCoreApplication::setQuitLockEnabled(bool enabled)
{
    lkQuitLockRefEnabled =  enabled;
}

void LCoreApplication::maybeQuit()
{
    if (m_inExec && lkQuitLockRefEnabled && shouldQuit()){
        std::shared_ptr<LEvent> evt = std::make_shared<LEvent>(LEvent::Quit);
        LCoreApplication::postEvent(LCoreApplication::instShare(), evt);
    }
}

void LCoreApplication::execCleanup()
{
    m_inExec = false;
}

bool LCoreApplication::handleEvent(std::shared_ptr<LEvent> e)
{
    if (e->type() == LEvent::Quit) {
        quit();
        return true;
    }
    return LObject::handleEvent(e);
}

bool LCoreApplication::notifyHelper(std::shared_ptr<LObject> receiver, std::shared_ptr<LEvent> event)
{
    bool consumed = false;
    bool filtered = false;

    if (LCoreApplication::m_self
        && receiver->thread() == LThreadDataRegistry::instance().mainThread()
        && LCoreApplication::m_self->sendThroughApplicationEventFilters(receiver, event)) {
        filtered = true;
        return filtered;
    }

    if (sendThroughObjectEventFilters(receiver, event)) {
        filtered = true;
        return filtered;
    }
    // deliver the event
    consumed = receiver->handleEvent(event);
    return consumed;
}

bool LCoreApplication::notifyInternal(std::shared_ptr<LObject> reciever, std::shared_ptr<LEvent> event)
{
    bool selfRequired = LCoreApplication::threadRequiresCoreApplication();
    if (m_self == nullptr && selfRequired){
        return false;
    }

    if (selfRequired){
        return m_self->notify(reciever, event); //一般走抽象类
    }else{
        return doNotify(reciever, event); //守护进程走这里
    }
}

bool LCoreApplication::threadRequiresCoreApplication()
{
    std::shared_ptr<LIThreadData> data = LThreadDataRegistry::instance().currentThreadData();
    if (data == nullptr){
        return true;    // default setting
    }
    return data->m_requiresCoreApplication;
}






LK_NAMESPACE_END
