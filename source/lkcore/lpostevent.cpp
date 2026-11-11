#include "lpostevent.h"

LK_NAMESPACE_BEGIN

LTimerEvent::LTimerEvent(int timerId) :LEvent(LEvent::Timer), m_id(timerId)
{

}


LChildEvent::LChildEvent(Type type, std::shared_ptr<LObject> child)
    :LEvent(type), c(child)
{

}

LChildEvent::~LChildEvent()
{

}




LK_NAMESPACE_END
