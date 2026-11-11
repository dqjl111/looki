#ifndef LPOINT_H
#define LPOINT_H

#include "lsize.h"

LK_NAMESPACE_BEGIN

template<typename T>
class LPoint : public LSize<T>
{
public:
    using LSize<T>::m_x;
    using LSize<T>::m_y;

    LPoint() noexcept{
    }

    LPoint(T m_xos, T m_yos):LSize<T>(m_xos, m_yos){
    }

    LPoint(const LSize<T>& s):LSize<T>(s){
    }


    LPoint(const LPoint<T>& s):LSize<T>(s.m_x, s.m_y){
    }

    template<typename C=int,
             typename std::enable_if<std::is_integral<C>::value
             && std::is_floating_point<T>::value, bool>::type = true>
    LPoint<C> toPoint() const
    {
        return LPoint<C>((C)std::lround(m_x), (C)std::lround(m_y));
    }


    template<typename C=double,
             typename std::enable_if<std::is_floating_point<C>::value
             && std::is_integral<T>::value, bool>::type = true>
    LPoint<C> toPointF() const
    {
        return LPoint<C>(m_x, m_y);
    }
};


template<typename T>
inline const LPoint<T> operator+(const LPoint<T> & s1, const LPoint<T> & s2) noexcept{
    return LPoint<T>(s1.m_x+s2.m_x, s1.m_y+s2.m_y);
}

template<typename T>
inline const LPoint<T> operator-(const LPoint<T> &s1, const LPoint<T> &s2) noexcept{
    return LPoint<T>(s1.m_x-s2.m_x, s1.m_y-s2.m_y);
}

inline const LPoint<int> operator*(const LPoint<int> &s, double c) noexcept{
    return LPoint<int>(std::lround(s.m_x*c), std::lround(s.m_y*c));
}

inline const LPoint<double> operator*(const LPoint<double> &s, double c) noexcept{
    return LPoint<double>(s.m_x*c, s.m_y*c);
}

template<typename T>
inline const LPoint<T> operator*(double c, const LPoint<T> &s) noexcept{
    return operator*(s, c);
}

inline const LPoint<int>  operator/(const LPoint<int> &p, double c)
{
    return LPoint<int>(std::lround(p.m_x/c), std::lround(p.m_y/c));
}

inline const LPoint<double>  operator/(const LPoint<double> &p, double c)
{
    return LPoint<double>(p.m_x/c, p.m_y/c);
}

template<typename T>
inline const LPoint<T> operator+(const LPoint<T> &p)
{
    return p;
}

template<typename T>
inline const LPoint<T> operator-(const LPoint<T> &p)
{
    return LPoint<T>(-p.m_x, -p.m_y);
}


LK_NAMESPACE_END


#endif // LPOINT_H
