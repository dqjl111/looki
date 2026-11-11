#include "lkcore/lthreaddata.h"
#include "lkcore/lthread.h"
#include "lkcore/ldebug.h"
#include "leventdispatcherwin.h"

LK_NAMESPACE_BEGIN

std::shared_ptr<LEventDispatcher> LIThreadData::createEventDispatcher()
{
    m_eventDispatcher = makeInit<LEventDispatcherWin>();
    return m_eventDispatcher;
}

LK_NAMESPACE_END





