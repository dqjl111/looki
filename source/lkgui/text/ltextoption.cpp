#include "ltextoption.h"

LK_NAMESPACE_BEGIN

LTextOption::LTextOption()
    : m_align((uint32_t)LAlignment::Left),
    m_wordWrap(LTextOption::WordWrap),
    m_design(false),
    m_flags(0),
    m_tabStopDistance(-1)
{
    m_direction = (uint32_t)LLayoutDirection::Auto;
}

LTextOption::LTextOption(const LTextOption &o)
    : m_align(o.m_align),
    m_wordWrap(o.m_wordWrap),
    m_design(o.m_design),
    m_direction(o.m_direction),
    m_flags(o.m_flags),
    m_tabStopDistance(o.m_tabStopDistance)
{
    m_tabStops = o.m_tabStops;
}

LTextOption &LTextOption::operator=(const LTextOption &o)
{
    if (this == &o){
        return *this;
    }

    m_align = o.m_align;
    m_wordWrap = o.m_wordWrap;
    m_design = o.m_design;
    m_direction = o.m_direction;
    m_flags = o.m_flags;
    m_tabStopDistance = o.m_tabStopDistance;
    m_tabStops = o.m_tabStops;
    return *this;
}

void LTextOption::setTabArray(const std::list<double> &tabStops)
{
    //Todo
}

LK_NAMESPACE_END
