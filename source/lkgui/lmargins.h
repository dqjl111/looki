#ifndef LMARGINS_H
#define LMARGINS_H
#include <cmath>
#include "lkcore/lglobal.h"
#include "lkcore/io/lcbor.h"

LK_NAMESPACE_BEGIN

template<typename T>
class LMargins
{
public:
    LMargins() noexcept{
    }

    LMargins(T m) noexcept
        :m_left(m), m_top(m), m_right(m), m_bottom(m){
    }

    LMargins(T left, T top, T right, T bottom) noexcept
        :m_left(left), m_top(top), m_right(right), m_bottom(bottom){
    }

    LMargins(const LMargins<T>& s) noexcept
        :m_left(s.m_left), m_top(s.m_top), m_right(s.m_right), m_bottom(s.m_bottom){
    }

    bool isNull() const noexcept{
        return lFuzzyIsZero(m_left)
               && lFuzzyIsZero(m_top)
               && lFuzzyIsZero(m_right)
               && lFuzzyIsZero(m_bottom);
    }

    inline LMargins<T> &operator=(const LMargins<T> &s) noexcept{
        m_left =s.m_left;
        m_top =s.m_top;
        m_right =s.m_right;
        m_bottom =s.m_bottom;
        return *this;
    }


    LMargins<T> &operator+=(const LMargins<T> &margins) noexcept;
    LMargins<T> &operator-=(const LMargins<T> &margins) noexcept;
    LMargins<T> &operator|=(const LMargins<T> &margins) noexcept;
    LMargins<T> &operator+=(int) noexcept;
    LMargins<T> &operator-=(int) noexcept;
    LMargins<T> &operator*=(int) noexcept;
    LMargins<T> &operator/=(int);
    LMargins<T> &operator*=(double) noexcept;
    LMargins<T> &operator/=(double);

    void reset(){
        m_left = 0;
        m_top = 0;
        m_right = 0;
        m_bottom = 0;
    }

    LMargins<int> toMargin() const
    {
        return LMargins<int>(std::lround(m_left), std::lround(m_top), std::lround(m_right), std::lround(m_bottom));
    }

    LMargins<double> toMarginF() const
    {
        return LMargins<double>(m_left, m_top, m_right, m_bottom);
    }

    LCbor toCbor(){
        LCbor::map ret;
        ret[0] = m_left;
        ret[1] = m_top;
        ret[2] = m_right;
        ret[3] = m_bottom;
        return ret;
    }

    void fromCbor(const LCbor& v){
        LCbor::map t = v.toMap();
        m_left = t[0];
        m_top = t[1];
        m_right = t[2];
        m_bottom = t[3];
    }


    T m_left = 0;
    T m_top = 0;
    T m_right = 0;
    T m_bottom = 0;
};


template<typename T>
inline bool operator==(const LMargins<T> &m1, const LMargins<T> &m2) noexcept
{
    return lFuzzyIsZero(std::abs(m1.m_left-m2.m_left))
           && lFuzzyIsZero(std::abs(m1.m_top-m2.m_top))
           && lFuzzyIsZero(std::abs(m1.m_right-m2.m_right))
           && lFuzzyIsZero(std::abs(m1.m_bottom-m2.m_bottom));
}

template<typename T>
inline bool operator!=(const LMargins<T> &m1, const LMargins<T> &m2) noexcept
{
    return !lFuzzyIsZero(std::abs(m1.m_left-m2.m_left))|| !lFuzzyIsZero(std::abs(m1.m_top-m2.m_top))
           || !lFuzzyIsZero(std::abs(m1.m_right-m2.m_right)) || !lFuzzyIsZero(std::abs(m1.m_bottom-m2.m_bottom));
}

template<typename T>
inline LMargins<T> operator+(const LMargins<T> &m1, const LMargins<T> &m2) noexcept
{
    return LMargins<T>(m1.m_left + m2.m_left, m1.m_top + m2.m_top,
                    m1.m_right + m2.m_right, m1.m_bottom + m2.m_bottom);
}

template<typename T>
inline LMargins<T> operator-(const LMargins<T> &m1, const LMargins<T> &m2) noexcept
{
    return LMargins<T>(m1.m_left - m2.m_left, m1.m_top - m2.m_top,
                    m1.m_right - m2.m_right, m1.m_bottom - m2.m_bottom);
}

template<typename T>
inline LMargins<T> operator|(const LMargins<T> &m1, const LMargins<T> &m2)
{
    return LMargins<T>(std::max<T>(m1.m_left, m2.m_left), std::max<T>(m1.m_top, m2.m_top),
                       std::max<T>(m1.m_right, m2.m_right), std::max<T>(m1.m_bottom, m2.m_bottom));
}

template<typename T>
inline LMargins<T> operator+(const LMargins<T> &lhs, int rhs) noexcept
{
    return LMargins<T>(lhs.m_left + rhs, lhs.m_top + rhs,
                    lhs.m_right + rhs, lhs.m_bottom + rhs);
}

template<typename T>
inline LMargins<T> operator+(int lhs, const LMargins<T> &rhs) noexcept
{
    return LMargins<T>(rhs.m_left + lhs, rhs.m_top + lhs,
                    rhs.m_right + lhs, rhs.m_bottom + lhs);
}

template<typename T>
inline LMargins<T> operator-(const LMargins<T> &lhs, int rhs) noexcept
{
    return LMargins<T>(lhs.m_left - rhs, lhs.m_top - rhs,
                    lhs.m_right - rhs, lhs.m_bottom - rhs);
}

template<typename T>
inline LMargins<T> operator*(const LMargins<T> &margins, int factor) noexcept
{
    return LMargins<T>(margins.m_left * factor, margins.m_top * factor,
                    margins.m_right * factor, margins.m_bottom * factor);
}

template<typename T>
inline LMargins<T> operator*(int factor, const LMargins<T> &margins) noexcept
{
    return LMargins<T>(margins.m_left * factor, margins.m_top * factor,
                    margins.m_right * factor, margins.m_bottom * factor);
}

inline LMargins<int> operator*(const LMargins<int> &margins, double factor) noexcept
{
    return LMargins<int>(std::lround(margins.m_left * factor), std::lround(margins.m_top * factor),
                    std::lround(margins.m_right * factor), std::lround(margins.m_bottom * factor));
}

inline LMargins<double> operator*(const LMargins<double> &margins, double factor) noexcept
{
    return LMargins<double>(margins.m_left * factor, margins.m_top * factor,
                         margins.m_right * factor, margins.m_bottom * factor);
}

template<typename T>
inline LMargins<int> operator*(double factor, const LMargins<T> &margins) noexcept
{
    return operator*(margins, factor);
}

template<typename T>
inline LMargins<T> operator/(const LMargins<T> &margins, int divisor)
{
    return LMargins<T>(margins.m_left / divisor, margins.m_top / divisor,
                    margins.m_right / divisor, margins.m_bottom / divisor);
}

inline LMargins<int> operator/(const LMargins<int> &margins, double divisor)
{
    return LMargins<int>(std::lround(margins.m_left / divisor), std::lround(margins.m_top / divisor),
                    std::lround(margins.m_right / divisor), std::lround(margins.m_bottom / divisor));
}

template<typename T>
inline LMargins<T> &LMargins<T>::operator+=(const LMargins<T> &margins) noexcept
{
    return *this = *this + margins;
}

template<typename T>
inline LMargins<T> &LMargins<T>::operator-=(const LMargins<T> &margins) noexcept
{
    return *this = *this - margins;
}

template<typename T>
inline LMargins<T> &LMargins<T>::operator|=(const LMargins<T> &margins) noexcept
{
    return *this = *this | margins;
}

template<typename T>
inline LMargins<T> &LMargins<T>::operator+=(int margin) noexcept
{
    m_left += margin;
    m_top += margin;
    m_right += margin;
    m_bottom += margin;
    return *this;
}

template<typename T>
inline LMargins<T> &LMargins<T>::operator-=(int margin) noexcept
{
    m_left -= margin;
    m_top -= margin;
    m_right -= margin;
    m_bottom -= margin;
    return *this;
}

template<typename T>
inline LMargins<T> &LMargins<T>::operator*=(int factor) noexcept
{
    return *this = *this * factor;
}

template<typename T>
inline LMargins<T> &LMargins<T>::operator/=(int divisor)
{
    return *this = *this / divisor;
}

template<typename T>
inline LMargins<T> &LMargins<T>::operator*=(double factor) noexcept
{
    return *this = *this * factor;
}

template<typename T>
inline LMargins<T> &LMargins<T>::operator/=(double divisor)
{
    return *this = *this / divisor;
}

template<typename T>
inline LMargins<T> operator+(const LMargins<T> &margins) noexcept
{
    return margins;
}

template<typename T>
inline LMargins<T> operator-(const LMargins<T> &margins) noexcept
{
    return LMargins(-margins.m_left, -margins.m_top, -margins.m_right, -margins.m_bottom);
}

LK_NAMESPACE_END
#endif // LMARGINS_H
