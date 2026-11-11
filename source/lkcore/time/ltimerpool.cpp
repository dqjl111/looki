#include "ltimerpool.h"
#include <vector>
#include "lkcore/ldebug.h"
#include "lkcore/lcoreapplication.h"

LK_NAMESPACE_BEGIN

LTimerPool::LTimerPool(): m_timers{ }
{
    start();
}

LTimerPool::~LTimerPool()
{
    m_timers.clear();
}

void LTimerPool::registerTimer(TimerHandle timer)
{
    {
        std::lock_guard<std::mutex> lock(m_threadData->m_eventMutex);
        m_timers.remove(timer);
        m_timers.emplace_front(timer);
    }

    m_threadData->m_cond.notify_all();
}

void LTimerPool::unregisterTimer(TimerHandle timer)
{
    {
        std::lock_guard<std::mutex> lock(m_threadData->m_eventMutex);
        m_timers.remove(timer);
    }

    m_threadData->m_cond.notify_all();
}

void LTimerPool::run()
{
    std::vector<TimerHandle> expiredTimers;
    std::vector<TimerHandle> deadTimers;

    while (m_threadData->m_state.load(std::memory_order_relaxed) == LIThreadData::Started)
    {
        std::unique_lock<std::mutex> lock(m_threadData->m_eventMutex);
        if(!deadTimers.empty()){
            for(const TimerHandle& timer : deadTimers){
                m_timers.remove(timer);
            }
            deadTimers.clear();
        }

        const auto nowTime = LClock::now();

        auto wakeTime = nowTime + std::chrono::minutes(1);

        for (const TimerHandle& timer : m_timers)
        {
            if(timer->isAlive()){
                const auto expiryTime = timer->nextExpiry();

                if (expiryTime <= nowTime){
                    expiredTimers.emplace_back(timer);
                }
                else if (expiryTime < wakeTime){
                    wakeTime = expiryTime;
                }
            }
            else{
                deadTimers.push_back(timer);
            }
        }

        if (! expiredTimers.empty())
        {
            // We fire callbacks without the pool modification lock held, so that the timer callbacks can
            // safely manipulate the pool if desired (and so other threads can change the pool while callbacks
            // are in progress). Note that the timer list modification (recursive) mutex remains held, so that
            // we do block if timers are being (de-)registered while the callbacks are run, so that we don't
            // e.g. try to fire a callback to a partially destroyed user-object that owned the timer being unregistered.

            lock.unlock();

            for (const auto& timer : expiredTimers){
                timer->fire(nowTime);
            }

            expiredTimers.clear();
        }
        else
        {
            // No timers have expired yet, we can sleep.
            m_threadData->m_cond.wait_until(lock, wakeTime);
        }
    }
}

void LTimerPoolTimer::setCallback(std::function<void()> callback)
{
    std::lock_guard<decltype(m_mutex)> lock(m_mutex);
    m_callback = std::move(callback);
}

void LTimerPoolTimer::setInterval(std::chrono::milliseconds ms)
{
    std::lock_guard<decltype(m_mutex)> lock(m_mutex);
    m_interval = ms;
}

void LTimerPoolTimer::setRepeated(bool repeated)
{
    std::lock_guard<decltype(m_mutex)> lock(m_mutex);
    m_repeated = repeated;
}


void LTimerPoolTimer::start(StartMode mode)
{
    {
        std::lock_guard<decltype(m_mutex)> lock(m_mutex);

        switch (mode)
        {
            case StartMode::StartOnly:
            {
                // Abort if timer already running, we aren't allowing restarts
                if (m_nextExpiry != maxTimePoint())
                    return;

                break;
            }

            case StartMode::RestartIfRunning:
            {
                // No preconditons, always (re)start
                break;
            }

            case StartMode::RestartOnly:
            {
                // Abort if timer not already running, we are only allowing restarts
                if (m_nextExpiry == maxTimePoint()){
                    return;
                }
                break;
            }
        }

        m_nextExpiry = LClock::now() + m_interval;
    }

    if (const PoolHandle pool = m_pool.lock()){
        pool->wake();
    }
}

void LTimerPoolTimer::stop()
{
    {
        std::lock_guard<decltype(m_mutex)> lock(m_mutex);
        m_nextExpiry = maxTimePoint();
    }

    if (const PoolHandle pool = m_pool.lock()){
        pool->wake();
    }
}

void LTimerPoolTimer::fire(LTimePoint now)
{
    //const TimerHandle selfHandle = shared_from_this();
    uint32_t callbacksRequired = 0;
    std::function<void()> callback;

    {
        std::lock_guard<decltype(m_mutex)> lock(m_mutex);
        callback = m_callback;

        if (m_repeated)
        {
            // We might have to catch up to the current time - it's more efficient
            // to fire as many callbacks as we can be sure we've missed right now while
            // we're making expensive callback object copies, then clean up any extra
            // missed callbacks later when the parent pool re-evaluates the pool timers.
            do
            {
                m_nextExpiry += m_interval;
                callbacksRequired++;
            } while (m_nextExpiry < now);
        }
        else
        {
            m_nextExpiry = maxTimePoint();
            callbacksRequired++;
        }
    }

    if (callback)
    {
        while (callbacksRequired--){
            callback();
        }
    }
}

bool LTimerPoolTimer::isAlive()
{
    return true;
}

bool LTimerPoolTimer::isActive() const noexcept
{
    std::lock_guard<decltype(m_mutex)> lock(m_mutex);
    return m_nextExpiry != maxTimePoint();
}

LTimePoint LTimerPoolTimer::nextExpiry() const noexcept
{
    std::lock_guard<decltype(m_mutex)> lock(m_mutex);
    return m_nextExpiry;
}

LEventTimer::LEventTimer(std::shared_ptr<LObject> reciever, const PoolHandle &ph)
    : LTimerPoolTimer(ph)
    , m_reciever(reciever)
{
    m_id = generateId();
}

bool LEventTimer::isAlive()
{
    return m_reciever.lock() != nullptr;
}

void LEventTimer::initCallBack()
{
    setCallback(
        [this](void) {
            std::shared_ptr<LObject> obj = m_reciever.lock();
            if(obj != nullptr){
                std::shared_ptr<LTimerEvent> event = std::make_shared<LTimerEvent>(m_id);
                LCoreApplication::sendEvent(obj, event);
            }
        });

    {
        PoolHandle p = pool();
        std::shared_ptr<LTimerPoolTimer> tThis = shared_from_this();
        if(p!= nullptr){
            p->registerTimer(tThis);
        }
    }
}

int LEventTimer::generateId()
{
    static int currentEventId = 0;
    return currentEventId++;
}

LK_NAMESPACE_END
