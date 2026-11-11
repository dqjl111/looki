#ifndef LSIZE_H
#define LSIZE_H
#include "lmargins.h"
#include "lguienums.h"
#include "lkcore/io/lcbor.h"

LK_NAMESPACE_BEGIN
template<typename T>
class LSize
{
public:
    LSize() noexcept{
    }

    LSize(T w, T h) noexcept
        :m_x(w),m_y(h){
    }

    LSize(const LSize<T>& s) noexcept
        :m_x(s.m_x),m_y(s.m_y){
    }

    LSize(const LCbor& v){
        fromCbor(v);
    }

    inline bool isNull() const noexcept{
        return lIsZero(m_x) && lIsZero(m_y);
    }
    inline bool isEmpty() const noexcept{
        return m_x <= T(0) || m_y <= T(0);
    }
    inline bool isValid() const noexcept{
        return m_x >= T(0) && m_y >= T(0);
    }

    inline bool isValidF() const noexcept{
        return m_x > T(0) && m_y > T(0);
    }

    void transpose() noexcept{
        std::swap(m_x, m_y);
    }
    inline LSize<T> transposed() const noexcept{
        return LSize<T>(m_y, m_x);
    }

    inline void scale(T w, T h, LAspectRatioMode mode) noexcept{
        scale(LSize<T>(w, h), mode);
    }

    inline void scale(const LSize<T> &s, LAspectRatioMode mode) noexcept{
         *this = scaled(s, mode);
    }

    LSize<T> scaled(T w, T h, LAspectRatioMode mode) const noexcept{
        scaled(LSize<T>(w, h), mode);
    }

    LSize<T> scaled(const LSize<T> &s, LAspectRatioMode mode) const noexcept{
        if (mode == LAspectRatioMode::IgnoreAspectRatio || lIsZero(m_x) || lIsZero(m_y)) {
            return s;
        } else {
            bool useHeight;
            T rw = s.m_y * m_x / m_y;

            if (mode == LAspectRatioMode::KeepAspectRatio) {
                useHeight = (rw <= s.m_x);
            } else { // mode == LAspectRatioMode::KeepAspectRatioByExpanding
                useHeight = (rw >= s.m_x);
            }

            if (useHeight) {
                return LSize<T>(rw, s.m_y);
            } else {
                return LSize<T>(s.m_x, s.m_x * m_y / m_x);
            }
        }
    }

    inline LSize<T> expandedTo(const LSize<T> &otherSize) const noexcept{
        return LSize<T>(std::max<T>(m_x,otherSize.m_x), std::max<T>(m_y,otherSize.m_y));
    }

    inline LSize<T> boundedTo(const LSize<T> &otherSize) const noexcept{
        return LSize<T>(std::min<T>(m_x,otherSize.m_x), std::min<T>(m_y,otherSize.m_y));
    }

    LSize grownBy(LMargins<T> m) const noexcept{
        return {m_x + m.m_left + m.m_right, m_y + m.m_top + m.m_bottom};
    }
    LSize shrunkBy(LMargins<T> m) const noexcept{
        return {m_x - m.m_left - m.m_right, m_y - m.m_top - m.m_bottom};
    }

    inline LSize<T> &operator=(const LSize<T> &s) noexcept{
        m_x =s.m_x;
        m_y =s.m_y;
        return *this;
    }

    inline LSize<T> &operator+=(const LSize<T> &s) noexcept{
        m_x +=s.m_x;
        m_y+=s.m_y;
        return *this;
    }

    inline LSize<T> &operator-=(const LSize<T> &s) noexcept{
        m_x -= s.m_x;
        m_y-=s.m_y;
        return *this;
    }

    inline LSize<T> &operator*=(double c) noexcept;
    inline LSize<T> &operator/=(double c);

    inline T width() const noexcept
    { return m_x; }

    inline T height() const noexcept
    { return m_y; }

    inline void setWidth(T w) noexcept
    { m_x = w; }

    inline void setHeight(T h) noexcept
    { m_y = h; }

    inline int &rwidth() noexcept
    { return m_x; }

    inline int &rheight() noexcept
    { return m_y; }


    static inline T dotProduct(const LSize<T> &s1, const LSize<T> &s2)
    { return s1.m_x * s2.m_x + s1.m_y * s2.m_y; }

    inline int manhattanLength() const{
        return std::abs(m_x)+std::abs(m_y);
    }

    LSize<int> toSize() const
    {
        return LSize<int>(std::lround(m_x), std::lround(m_y));
    }

    LSize<double> toSizeF() const
    {
        return LSize<double>(m_x, m_y);
    }

    void reset(){
        m_x = 0;
        m_y = 0;
    }

    LCbor toCbor(){
        LCbor::map ret;
        ret[0] = m_x;
        ret[1] = m_y;
        return ret;
    }

    void fromCbor(const LCbor& v){
        LCbor::map t = v.toMap();
        m_x = t[0];
        m_y = t[1];
    }


    T m_x = T(0);
    T m_y = T(0);
};

template<typename T>
inline bool operator==(const LSize<T>& s1, const LSize<T> &s2) noexcept{
    return lFuzzyIsZero(s1.m_x-s2.m_x) && lFuzzyIsZero(s1.m_y-s2.m_y);
}

template<typename T>
inline bool operator!=(const LSize<T> &s1, const LSize<T> &s2) noexcept{
    return !operator==(s1, s2);
}

template<typename T>
inline const LSize<T> operator+(const LSize<T> & s1, const LSize<T> & s2) noexcept{
    return LSize<T>(s1.m_x+s2.m_x, s1.m_y+s2.m_y);
}

template<typename T>
inline const LSize<T> operator-(const LSize<T> &s1, const LSize<T> &s2) noexcept{
    return LSize<T>(s1.m_x-s2.m_x, s1.m_y-s2.m_y);
}

inline const LSize<int> operator*(const LSize<int> &s, double c) noexcept{
    return LSize<int>(std::lround(s.m_x*c), std::lround(s.m_y*c));
}

inline const LSize<double> operator*(const LSize<double> &s, double c) noexcept{
    return LSize<double>(s.m_x*c, s.m_y*c);
}

template<typename T>
inline const LSize<T> operator*(double c, const LSize<T> &s) noexcept{
    return operator*(s, c);
}

template<typename T>
inline LSize<T> &LSize<T>::operator*=(double factor) noexcept
{
    return *this = *this * factor;
}

inline const LSize<int>  operator/(const LSize<int> &p, double c)
{
    return LSize<int>(std::lround(p.m_x/c), std::lround(p.m_y/c));
}

inline const LSize<double>  operator/(const LSize<double> &p, double c)
{
    return LSize<double>(p.m_x/c, p.m_y/c);
}

template<typename T>
inline LSize<T> &LSize<T>::operator/=(double divisor)
{
    return *this = *this / divisor;
}




LK_NAMESPACE_END
#endif // LSIZE_H
