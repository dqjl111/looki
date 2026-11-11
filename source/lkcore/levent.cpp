#include "levent.h"
LK_NAMESPACE_BEGIN

LEvent::LEvent(Type _t) :m_type(_t), m_posted(false), m_spont(false), m_accept(true)
{
}

LEvent::~LEvent()
{

}


LK_NAMESPACE_END
