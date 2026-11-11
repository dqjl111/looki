#ifndef LTIMERPOOL_H
#define LTIMERPOOL_H

#include <forward_list>
#include <functional>
#include "lkcore/lthread.h"
#include "lkcore/lglobal.h"

LK_NAMESPACE_BEGIN
class LTimerPoolTimer;
class LKCOREDEF LTimerPool: public LThread
{
public:
    using WeakPoolHandle  = std::weak_ptr<LTimerPool>;
    using PoolHandle      = std::shared_ptr<LTimerPool>;
    using WeakTimerHandle = std::weak_ptr<LTimerPoolTimer>;
    using TimerHandle     = std::shared_ptr<LTimerPoolTimer>;
public:
    explicit LTimerPool();
    virtual ~LTimerPool();

    void registerTimer(TimerHandle timer);
    void unregisterTimer(TimerHandle timer);
protected:
    LTimerPool(const LTimerPool&) = delete;
    LTimerPool& operator=(const LTimerPool&) = delete;
    virtual void run() override;
private:
    std::forward_list<TimerHandle>  m_timers;    
};

class LKCOREDEF LTimerPoolTimer : public std::enable_shared_from_this<LTimerPoolTimer>
{

public:
    using WeakPoolHandle  = LTimerPool::WeakPoolHandle;
    using PoolHandle      = LTimerPool::PoolHandle;
    using WeakTimerHandle = LTimerPool::WeakTimerHandle;
    using TimerHandle     = LTimerPool::TimerHandle;

    static inline LTimePoint maxTimePoint(){
        return LTimePoint(LClock::duration(LLONG_MAX));
    }

    static inline LTimePoint minTimePoint(){
        return LTimePoint(LClock::duration(LLONG_MIN));
    }

private:
    mutable std::mutex m_mutex;
    WeakPoolHandle m_pool;

    LTimePoint m_nextExpiry = maxTimePoint();

    std::function<void()> m_callback = nullptr;
    std::chrono::milliseconds m_interval = std::chrono::milliseconds(0);
    bool m_repeated = false;

public:
    explicit LTimerPoolTimer(const PoolHandle& pool){
        m_pool = pool;
    }
    PoolHandle pool() const {
        return m_pool.lock();
    }

    void setCallback(std::function<void()> callback);
    void setInterval(std::chrono::milliseconds ms);
    void setRepeated(bool repeated);

    std::chrono::milliseconds interval() const{
        return m_interval;
    }
    bool isRepeated() const{
        return m_repeated;
    }

    enum class StartMode
    {
        StartOnly,
        RestartIfRunning,
        RestartOnly,
    };

    void start(StartMode mode = StartMode::RestartIfRunning);
    void stop();

    bool isActive() const noexcept;
    LTimePoint nextExpiry() const noexcept;

    void fire(LTimePoint now = minTimePoint());
    virtual bool isAlive();
protected:
    LTimerPoolTimer(const LTimerPoolTimer&) = delete;
    LTimerPoolTimer& operator=(const LTimerPoolTimer&) = delete;
};

class LKCOREDEF LEventTimer : public LTimerPoolTimer
{
public:
    explicit LEventTimer(std::shared_ptr<LObject> reciever, const PoolHandle& pool);

    virtual bool isAlive() override;
    void initCallBack();
    int id() const noexcept { return m_id; }
private:
    static int generateId();
    std::weak_ptr<LObject> m_reciever;
    int m_id;
};
LK_NAMESPACE_END
#endif
