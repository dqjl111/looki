#include "ltimer.h"
#include "lkcore/lcoreapplication.h"
#include "lkcore/ldebug.h"

LK_NAMESPACE_BEGIN


LTimer::LTimer(LTimerPool::PoolHandle pool)
{
    if(pool == nullptr){
        pool = LCoreApplication::timePool();
    }
    m_handle = std::make_shared<LTimerPoolTimer>(pool);
    m_handle->setCallback(
        [this](void) {
            sigTimeout.emt();
        });

    pool->registerTimer(m_handle);
}

LTimer::~LTimer()
{
    m_handle->stop();
    if (const auto& pool = m_handle->pool()){
        pool->unregisterTimer(m_handle);
    }
}

void LTimer::setInterval(std::chrono::milliseconds ms)
{
    m_handle->setInterval(ms);
}

void LTimer::setRepeated(bool repeated)
{
    m_handle->setRepeated(repeated);
}

std::chrono::milliseconds LTimer::interval() const
{
    return m_handle->interval();
}

bool LTimer::isRepeated() const
{
    return m_handle->isRepeated();
}

void LTimer::start(std::chrono::milliseconds ms, bool repeated)
{
    m_handle->setRepeated(repeated);
    m_handle->setInterval(ms);
    m_handle->start();
}

void LTimer::start()
{
    m_handle->start();
}

void LTimer::stop()
{
    m_handle->stop();
}

bool LTimer::isActive() const
{
    return m_handle->isActive();
}




LK_NAMESPACE_END
