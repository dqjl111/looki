#include "lsizepolicy.h"
#include <bit>

LK_NAMESPACE_BEGIN

LSizePolicy::LSizePolicy(uint32_t data): m_data(data)
{

}

LSizePolicy::LSizePolicy(Policy horizontal, Policy vertical, ControlType type)
{
    m_horPolicy = horizontal;
    m_verPolicy = vertical;
    setControlType(type);
}

LFlags<LOrientation> LSizePolicy::expandingDirections() const
{
    return ( (m_verPolicy   & ExpandFlag) ? LOrientation::Vertical   : LFlags<LOrientation>() )
           | ( (m_horPolicy & ExpandFlag) ? LOrientation::Horizontal : LFlags<LOrientation>() ) ;
}

LSizePolicy::ControlType LSizePolicy::controlType() const noexcept
{
    return LSizePolicy::ControlType(1 << m_ctype);
}

void LSizePolicy::setControlType(ControlType type) noexcept
{
    m_ctype = std::countr_zero((uint32_t)type);
}

LSizePolicy LSizePolicy::transposed() const
{
    LSizePolicy ret(m_data);
    ret.m_horStretch = m_verStretch;
    ret.m_verStretch = m_horStretch;
    ret.m_horPolicy = m_verPolicy;
    ret.m_verPolicy = m_horPolicy;
    return ret;
}

LK_NAMESPACE_END
