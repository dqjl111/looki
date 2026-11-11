#include "lthread.h"
#include "lthreaddata.h"
#include "leventloop.h"
#include <thread>
#include <atomic>
#include <memory>
#include "ldebug.h"

LK_NAMESPACE_BEGIN


LThreadDataRegistry::LThreadDataRegistry()
{

}

LThreadDataRegistry &LThreadDataRegistry::instance()
{
    static LThreadDataRegistry lkThreadDataRegistry;
    return lkThreadDataRegistry;
}

void LThreadDataRegistry::registerThread(const std::thread::id &id, std::weak_ptr<LIThreadData> data)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    m_registry.emplace(id,data);
}


LIThread::LIThread()
{
    m_threadData = std::make_shared<LThreadData>();
    m_threadData->m_thread.store(this, std::memory_order_release);
}

LIThread::~LIThread()
{
}

std::shared_ptr<LIThreadData> LIThread::threadData()
{
    return m_threadData;
}


LIThreadData::State LIThread::state()
{
    return m_threadData->m_state.load(std::memory_order_relaxed);
}

bool LIThread::isMain()
{
    return false;
}


LMainThread::LMainThread()
{
    m_threadData->m_state.store(LIThreadData::Stopped, std::memory_order_release);
}

bool LMainThread::isMain()
{
    return true;
}

LThread::LThread()
{
}

LThread::~LThread()
{
    quit();
    std::shared_ptr<LThreadData> d = std::dynamic_pointer_cast<LThreadData>(m_threadData);
    if (d->m_stdThread.joinable()){ //等待原线程结束
        d->m_stdThread.join();
    }
}

void LThread::start()
{
    std::shared_ptr<LThreadData> d = std::dynamic_pointer_cast<LThreadData>(m_threadData);
    if (d->m_state.load(std::memory_order_relaxed) == LIThreadData::Started
        || d->m_state.load(std::memory_order_relaxed) == LIThreadData::Starting) {
        lWarn() << "Thread already started";
        return;
    }

    if (d->m_stdThread.joinable()){ //等待原线程结束
        d->m_stdThread.join();
    }

    d->m_state.store(LIThreadData::Starting, std::memory_order_release);
    d->m_stdThread = std::thread([this] { runThread(); });

}

void LThread::quit(int returnCode)
{
    if (m_threadData->m_state.load(std::memory_order_relaxed) == LIThreadData::Stopping
        || m_threadData->m_state.load(std::memory_order_relaxed) == LIThreadData::Stopped){
        return;
    }

    if(m_isExe){
        std::shared_ptr<LIThreadData> data = threadData();
        for(std::shared_ptr<LEventLoop> eventLoop: data->m_eventLoops){
            eventLoop->quit(returnCode);
        }
    }

    m_threadData->m_state.store(LIThreadData::Stopping, std::memory_order_release);
    m_threadData->m_cond.notify_one();
}

void LThread::wake()
{
    m_threadData->m_cond.notify_all();
}


std::thread::id LThread::threadId()
{
    std::shared_ptr<LThreadData> d = std::dynamic_pointer_cast<LThreadData>(m_threadData);
    return d->m_stdThread.get_id();
}


void LThread::run()
{
    exec();
}

void LThread::exec()
{
    m_isExe = true;
    std::shared_ptr<LEventLoop> loop = std::make_shared<LEventLoop>();
    loop->exec();
}

void LThread::runThread()
{
    //lInfo() << "Start thread";
    std::shared_ptr<LThreadData> d = std::dynamic_pointer_cast<LThreadData>(m_threadData);
    LThreadDataRegistry::instance().registerThread(d->stdThread()->get_id(), m_threadData);
    d->m_state.store(LIThreadData::Started, std::memory_order_release);
    run();
    d->m_state.store(LIThreadData::Stopped, std::memory_order_release);
    LThreadDataRegistry::instance().unregisterThread(d->stdThread()->get_id());
    //lInfo() << "Finish thread";
}


LK_NAMESPACE_END

