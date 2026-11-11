#ifndef LBEZIER_H
#define LBEZIER_H
#include "lkgui/paint/lpolygon.h"
#include "lkgui/paint/lline.h"
#include "lkgui/paint/ltransform2d.h"

LK_NAMESPACE_BEGIN
class LKGUIDEF LBezier
{
public:
    LBezier();
    LBezier(double x1,
            double y1,
            double x2,
            double y2,
            double x3,
            double y3,
            double x4,
            double y4);

    static LBezier fromPoints(const LPoint<double> &p1, const LPoint<double> &p2,
                              const LPoint<double> &p3, const LPoint<double> &p4)
    { return {p1.m_x, p1.m_y, p2.m_x, p2.m_y, p3.m_x, p3.m_y, p4.m_x, p4.m_y}; }

    static void coefficients(double t, double &a, double &b, double &c, double &d);

    inline LPoint<double> pointAt(double t) const;
    inline LPoint<double> normalVector(double t) const;

    inline LPoint<double> derivedAt(double t) const;
    inline LPoint<double> secondDerivedAt(double t) const;

    LPolygon<double> toPolygon(double bezierFlatteningThreshold = 0.5) const;
    void addToPolygon(LPolygon<double> *p, double bezierFlatteningThreshold = 0.5) const;
//    void addToPolygon(QDataBuffer<LPoint<double>> &polygon, double bezier_flattening_threshold) const;

    LRect<double> bounds() const;
    double length(double error = 0.01) const;
    void addIfClose(double *length, double error) const;

    double tAtLength(double len) const;

    int stationaryYPoints(double &t0, double &t1) const;
    double tForY(double t0, double t1, double y) const;

    LPoint<double> pt1() const { return LPoint<double>(m_x1, m_y1); }
    LPoint<double> pt2() const { return LPoint<double>(m_x2, m_y2); }
    LPoint<double> pt3() const { return LPoint<double>(m_x3, m_y3); }
    LPoint<double> pt4() const { return LPoint<double>(m_x4, m_y4); }

    LBezier mapBy(const LTransform2D &transform) const;

    inline LPoint<double> midPoint() const;
    inline LLine<double> midTangent() const;

    inline LLine<double> startTangent() const;
    inline LLine<double> endTangent() const;

    inline void parameterSplitLeft(double t, LBezier *left);
    inline std::pair<LBezier, LBezier> split() const;

    int shifted(LBezier *curveSegments, int maxSegmets,
                double offset, float threshold) const;

    LBezier bezierOnInterval(double t0, double t1) const;
    LBezier getSubRange(double t0, double t1) const;

    double m_x1=0, m_y1=0, m_x2=0, m_y2=0, m_x3=0, m_y3=0, m_x4=0, m_y4=0;
};

inline LPoint<double> LBezier::midPoint() const
{
    return LPoint<double>((m_x1 + m_x4 + 3*(m_x2 + m_x3))/8., (m_y1 + m_y4 + 3*(m_y2 + m_y3))/8.);
}

inline LLine<double> LBezier::midTangent() const
{
    LPoint<double> mid = midPoint();
    LLine<double> dir(LLine<double>(m_x1, m_y1, m_x2, m_y2).center(), LLine<double>(m_x3, m_y3, m_x4, m_y4).center());
    return LLine<double>(mid.m_x - dir.dx(), mid.m_y - dir.dy(),
                  mid.m_x + dir.dx(), mid.m_y + dir.dy());
}

inline LLine<double> LBezier::startTangent() const
{
    LLine<double> tangent(pt1(), pt2());
    if (tangent.isNull()){
        tangent = LLine<double>(pt1(), pt3());
    }
    if (tangent.isNull()){
        tangent = LLine<double>(pt1(), pt4());
    }
    return tangent;
}

inline LLine<double> LBezier::endTangent() const
{
    LLine<double> tangent(pt4(), pt3());
    if (tangent.isNull()){
        tangent = LLine<double>(pt4(), pt2());
    }
    if (tangent.isNull()){
        tangent = LLine<double>(pt4(), pt1());
    }
    return tangent;
}

inline void LBezier::coefficients(double t, double &a, double &b, double &c, double &d)
{
    double m_t = 1. - t;
    b = m_t * m_t;
    c = t * t;
    d = c * t;
    a = b * m_t;
    b *= 3. * t;
    c *= 3. * m_t;
}

inline LPoint<double> LBezier::pointAt(double t) const
{
    // numerically more stable:
    double x, y;

    double m_t = 1. - t;
    {
        double a = m_x1*m_t + m_x2*t;
        double b = m_x2*m_t + m_x3*t;
        double c = m_x3*m_t + m_x4*t;
        a = a*m_t + b*t;
        b = b*m_t + c*t;
        x = a*m_t + b*t;
    }
    {
        double a = m_y1*m_t + m_y2*t;
        double b = m_y2*m_t + m_y3*t;
        double c = m_y3*m_t + m_y4*t;
        a = a*m_t + b*t;
        b = b*m_t + c*t;
        y = a*m_t + b*t;
    }
    return LPoint<double>(x, y);
}

inline LPoint<double> LBezier::normalVector(double t) const
{
    double m_t = 1. - t;
    double a = m_t * m_t;
    double b = t * m_t;
    double c = t * t;

    return LPoint<double>((m_y2-m_y1) * a + (m_y3-m_y2) * b + (m_y4-m_y3) * c,
                          -(m_x2-m_x1) * a - (m_x3-m_x2) * b - (m_x4-m_x3) * c);
}

inline LPoint<double> LBezier::derivedAt(double t) const
{
    // p'(t) = 3 * (-(1-2t+t^2) * p0 + (1 - 4 * t + 3 * t^2) * p1 + (2 * t - 3 * t^2) * p2 + t^2 * p3)

    double m_t = 1. - t;

    double d = t * t;
    double a = -m_t * m_t;
    double b = 1 - 4 * t + 3 * d;
    double c = 2 * t - 3 * d;

    return LPoint<double>(a * m_x1 + b * m_x2 + c * m_x3 + d * m_x4,
                       a * m_y1 + b * m_y2 + c * m_y3 + d * m_y4)*3;
}

inline LPoint<double> LBezier::secondDerivedAt(double t) const
{
    double a = 2. - 2. * t;
    double b = -4 + 6 * t;
    double c = 2 - 6 * t;
    double d = 2 * t;

    return LPoint<double>(a * m_x1 + b * m_x2 + c * m_x3 + d * m_x4,
                       a * m_y1 + b * m_y2 + c * m_y3 + d * m_y4)*3;
}

std::pair<LBezier, LBezier> LBezier::split() const
{
    const auto mid = [](LPoint<double> lhs, LPoint<double> rhs) { return (lhs + rhs) * .5; };

    const LPoint<double> mid_12 = mid(pt1(), pt2());
    const LPoint<double> mid_23 = mid(pt2(), pt3());
    const LPoint<double> mid_34 = mid(pt3(), pt4());
    const LPoint<double> mid_12_23 = mid(mid_12, mid_23);
    const LPoint<double> mid_23_34 = mid(mid_23, mid_34);
    const LPoint<double> mid_12_23__23_34 = mid(mid_12_23, mid_23_34);

    return {
        fromPoints(pt1(), mid_12, mid_12_23, mid_12_23__23_34),
        fromPoints(mid_12_23__23_34, mid_23_34, mid_34, pt4()),
    };
}

inline void LBezier::parameterSplitLeft(double t, LBezier *left)
{
    left->m_x1 = m_x1;
    left->m_y1 = m_y1;

    left->m_x2 = m_x1 + t * ( m_x2 - m_x1 );
    left->m_y2 = m_y1 + t * ( m_y2 - m_y1 );

    left->m_x3 = m_x2 + t * ( m_x3 - m_x2 ); // temporary holding spot
    left->m_y3 = m_y2 + t * ( m_y3 - m_y2 ); // temporary holding spot

    m_x3 = m_x3 + t * ( m_x4 - m_x3 );
    m_y3 = m_y3 + t * ( m_y4 - m_y3 );

    m_x2 = left->m_x3 + t * ( m_x3 - left->m_x3);
    m_y2 = left->m_y3 + t * ( m_y3 - left->m_y3);

    left->m_x3 = left->m_x2 + t * ( left->m_x3 - left->m_x2 );
    left->m_y3 = left->m_y2 + t * ( left->m_y3 - left->m_y2 );

    left->m_x4 = m_x1 = left->m_x3 + t * (m_x2 - left->m_x3);
    left->m_y4 = m_y1 = left->m_y3 + t * (m_y2 - left->m_y3);
}


LK_NAMESPACE_END

#endif // LBEZIER_H
