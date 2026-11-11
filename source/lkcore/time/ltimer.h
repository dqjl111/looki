#ifndef LTIMER_H
#define LTIMER_H
#include "lkcore/lobject.h"
#include "lkcore/lsignal.h"
#include "lkcore/time/ltimerpool.h"

LK_NAMESPACE_BEGIN

class LKCOREDEF LTimer : public LObject
{
public:
    LTimer(LTimerPool::PoolHandle pool = nullptr);
    virtual ~LTimer();
    LSignal<> sigTimeout;


    void setInterval(std::chrono::milliseconds ms);
    inline void setInterval(int ms){
        setInterval(std::chrono::milliseconds(ms));
    }
    void setRepeated(bool repeated);

    std::chrono::milliseconds interval() const;
    bool isRepeated() const;
    void start(std::chrono::milliseconds ms, bool repeated = true);
    inline void start(int ms, bool repeated = true){
        start(std::chrono::milliseconds(ms), repeated);
    }
    void start();
    void stop();

    bool isActive() const;
protected:
    LTimerPool::TimerHandle m_handle;
};


LK_NAMESPACE_END
#endif // LTIMER_H
