#include "lelapsedtimer.h"

LK_NAMESPACE_BEGIN

void LElapsedTimer::start() noexcept
{
    restart();
}

LTimePoint LElapsedTimer::restart() noexcept
{
    m_startTimePoint = LClock::now();
    m_isValid = true;
    return m_startTimePoint;
}

void LElapsedTimer::invalidate() noexcept
{
    m_isValid = false;
}

bool LElapsedTimer::isValid() const noexcept
{
    return m_isValid;
}


int64_t LElapsedTimer::nsecsElapsed() const noexcept
{
    return lDurationToNs(LClock::now() - m_startTimePoint);
}

int64_t LElapsedTimer::msecsElapsed() const noexcept
{
    return lDurationToMs(LClock::now() - m_startTimePoint);
}

int64_t LElapsedTimer::secsElapsed() const noexcept
{
    return lDurationToSecond(LClock::now() - m_startTimePoint);
}

bool LElapsedTimer::hasExpiredNsecs(int64_t timeout) const noexcept
{
    return uint64_t(nsecsElapsed()) > uint64_t(timeout);
}

bool LElapsedTimer::hasExpiredMsecs(int64_t timeout) const noexcept
{
    return uint64_t(msecsElapsed()) > uint64_t(timeout);
}

bool LElapsedTimer::hasExpiredSecs(int64_t timeout) const noexcept
{
    return uint64_t(secsElapsed()) > uint64_t(timeout);
}


const LDuration LElapsedTimer::duration() const noexcept
{
    return LClock::now() - m_startTimePoint;
}

const LTimePoint &LElapsedTimer::startTimePoint() const noexcept
{
    return m_startTimePoint;
}

int64_t LElapsedTimer::nsecsTo(const LElapsedTimer &other) const noexcept
{
    return lDurationToNs(other.m_startTimePoint - m_startTimePoint);
}

int64_t LElapsedTimer::msecsTo(const LElapsedTimer &other) const noexcept
{
    return lDurationToMs(other.m_startTimePoint - m_startTimePoint);
}

int64_t LElapsedTimer::secsTo(const LElapsedTimer &other) const noexcept
{
    return lDurationToSecond(other.m_startTimePoint - m_startTimePoint);
}

bool LElapsedTimer::operator==(const LElapsedTimer &other) const noexcept
{
    if(m_isValid){
        return other.m_startTimePoint == m_startTimePoint;
    }else{
        return other.m_isValid == false;
    }
}

bool operator<(const LElapsedTimer &v1, const LElapsedTimer &v2) noexcept
{
    if(v1.m_isValid){
        if(v2.m_isValid){
            return v1.m_startTimePoint < v2.m_startTimePoint;
        }else{
            return true;
        }
    }else{
        return false;
    }

}


LK_NAMESPACE_END
