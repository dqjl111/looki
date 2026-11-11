#ifndef LLINE_H
#define LLINE_H

#include "lkgui/lpoint.h"
LK_NAMESPACE_BEGIN
enum class LLineIntersectType {
    No,
    Bounded,
    Unbounded
};

template<typename T>
class LLine
{
public:
    LLine(){}
    LLine(const LPoint<T> &p1, const LPoint<T> &p2):m_pt1(p1), m_pt2(p2){}
    LLine(T x1, T y1, T x2, T y2):m_pt1(x1, y1), m_pt2(x2, y2){}

    LPoint<T> m_pt1;
    LPoint<T> m_pt2;

    inline bool isNull() const{
        return m_pt1 == m_pt2;
    }

    inline void setPoints(const LPoint<T> &p1, const LPoint<T> &p2){
        m_pt1 = p1;
        m_pt2 = p2;
    }
    inline void setLine(T x1, T y1, T x2, T y2){
        m_pt1.m_x = x1;
        m_pt1.m_y = y1;
        m_pt2.m_x = x2;
        m_pt2.m_y = y2;
    }

    inline bool operator==(const LLine<T> &d) const{
        return m_pt1 == d.m_pt1 && m_pt2 == d.m_pt2;
    }

    inline bool operator!=(const LLine<T> &d) const { return !(*this == d); }

    inline T dx() const
    {
        return m_pt2.m_x - m_pt1.m_x;
    }

    inline T dy() const
    {
        return m_pt2.m_y - m_pt1.m_y;
    }

    inline void translate(const LPoint<T> &point)
    {
        m_pt1 += point;
        m_pt2 += point;
    }

    inline void translate(T adx, T ady)
    {
        translate(LPoint<T>(adx, ady));
    }

    inline LLine<T> translated(const LPoint<T> &p) const
    {
        return LLine<T>(m_pt1 + p, m_pt2 + p);
    }

    inline LLine<T> translated(T adx, T ady) const
    {
        return translated(LPoint<T>(adx, ady));
    }

    inline LPoint<T> center() const
    {
        return LPoint<T>((m_pt1.m_x + m_pt2.m_x) / 2, (m_pt1.m_y + m_pt2.m_y) / 2);
    }

    inline LPoint<T> pointAt(double t) const
    {
        return LPoint<T>(m_pt1.m_x + (m_pt2.m_x - m_pt1.m_x) * t, m_pt1.m_y + (m_pt2.m_y - m_pt1.m_y) * t);
    }

    inline double length() const
    {
        return hypot(m_pt2.m_x - m_pt1.m_x,   m_pt2.m_y - m_pt1.m_y);
    }

    inline LLine<T> normalVector() const
    {
        return LLine<T>(m_pt1, m_pt1 + LPoint<T>(dy(), -dx()));
    }

    LLine<double> unitVector() const
    {
        T x = dx();
        T y = dy();
        double len = length();

        LLine<double> f((double)m_pt1.m_x,
                        (double)m_pt2.m_y,
                        m_pt1.m_x + x/len,
                        m_pt1.m_y + y/len);
        return f;
    }

    inline void setLength(double len);

    LLineIntersectType intersects(const LLine<T> &l, LPoint<T> *intersectionPoint) const{
        // ipmlementation is based on Graphics Gems III's "Faster Line Segment Intersection"
        const LPoint<T> a = m_pt2 - m_pt1;
        const LPoint<T> b = l.m_pt1 - l.m_pt2;
        const LPoint<T> c = m_pt1 - l.m_pt1;

        const T denominator = a.m_y * b.m_x - a.m_x * b.m_y;
        if (denominator == 0 || !std::isfinite(denominator)){
            return LLineIntersectType::No; //平行，叉积为0
        }

        const double reciprocal = 1.0 / denominator;
        const double na = (b.m_y * c.m_x - b.m_x * c.m_y) * reciprocal;
        if (intersectionPoint){
            *intersectionPoint = m_pt1 + a * na;
        }

        if ((na < 0) || (na > 1)){
            return LLineIntersectType::Unbounded;
        }

        const double nb = (a.m_x * c.m_y - a.m_y * c.m_x) * reciprocal;
        if ((nb < 0) || (nb > 1)){
            return LLineIntersectType::Unbounded;
        }
        return LLineIntersectType::Bounded;
    }

    double angle() const
    {
        const double dx = m_pt2.m_x - m_pt1.m_x;
        const double dy = m_pt2.m_y - m_pt1.m_y;

        const double theta = std::atan2(-dy, dx) * 180.0 / std::numbers::pi;

        const double thetaNormalized = theta < 0 ? theta + 360 : theta;

        if (lFuzzyCompare(thetaNormalized, 360)){
            return 0;
        }else{
            return thetaNormalized;
        }
    }


    double angleTo(const LLine<T> &l) const{
        if (isNull() || l.isNull()){
            return 0;
        }

        const double a1 = angle();
        const double a2 = l.angle();

        const double delta = a2 - a1;
        const double deltaNormalized = delta < 0 ? delta + 360 : delta;

        if (lFuzzyCompare(delta, 360)){
            return 0;
        }else{
            return deltaNormalized;
        }
    }
};

template<>
inline void LLine<double>::setLength(double len){
    const LLine<double> v = unitVector();
    len /= v.length(); // In case it's not quite exactly 1.

    m_pt2.m_x = m_pt1.m_x + len * v.dx(),
    m_pt2.m_y = m_pt1.m_y + len * v.dy();
}

template<>
inline void LLine<int>::setLength(double len){
    const LLine<double> v = unitVector();
    len /= v.length(); // In case it's not quite exactly 1.
    m_pt2.m_x = std::lround(m_pt1.m_x + len * v.dx()),
    m_pt2.m_y = std::lround(m_pt1.m_y + len * v.dy());
}


LK_NAMESPACE_END

#endif // LLINE_H
