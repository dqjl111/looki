#ifndef LEVENTLOOP_H
#define LEVENTLOOP_H
#include "lkcore/lookiconfig.h"
#include "lobject.h"


LK_NAMESPACE_BEGIN
class LKCOREDEF LEventLoop : public LObject
{
public:
    LEventLoop();

    bool isRunning() const;
    void quit(int exitCode = 0);
    int exec();
    inline void setMaxWaitDuration(int duration){
        m_maxWaitDuration = duration;
    }
    inline int maxWaitDuration() const{
        return m_maxWaitDuration;
    }
protected:
    int m_exitCode = 0;
    std::atomic<bool> m_exit = false;
    int m_maxWaitDuration = 18;
    volatile bool m_isRunning = false;
};


LK_NAMESPACE_END
#endif // LEVENTLOOP_H
