#ifndef LELAPSEDTIMER_H
#define LELAPSEDTIMER_H
#include "lkcore/lglobal.h"

LK_NAMESPACE_BEGIN

class LKCOREDEF LElapsedTimer
{
public:
    LElapsedTimer(bool isRun=false)
    {
        if(isRun){
            start();
        }
    }


    void start() noexcept;
    LTimePoint restart() noexcept;
    void invalidate() noexcept;
    bool isValid() const noexcept;

    int64_t nsecsElapsed() const noexcept;
    int64_t msecsElapsed() const noexcept;
    int64_t secsElapsed() const noexcept;

    bool hasExpiredNsecs(int64_t timeout) const noexcept;
    bool hasExpiredMsecs(int64_t timeout) const noexcept;
    bool hasExpiredSecs(int64_t timeout) const noexcept;

    const LDuration duration() const noexcept;
    const LTimePoint& startTimePoint() const noexcept;

    int64_t nsecsTo(const LElapsedTimer &other) const noexcept;
    int64_t msecsTo(const LElapsedTimer &other) const noexcept;
    int64_t secsTo(const LElapsedTimer &other) const noexcept;

    bool operator==(const LElapsedTimer &other) const noexcept;
    bool operator!=(const LElapsedTimer &other) const noexcept
    { return !(*this == other); }

    friend bool operator<(const LElapsedTimer &v1, const LElapsedTimer &v2) noexcept;

private:
    LTimePoint m_startTimePoint;
    bool m_isValid = false;
};


LK_NAMESPACE_END
#endif // LELAPSEDTIMER_H
