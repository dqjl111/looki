#include "lbezier.h"
#include <corecrt_math_defines.h>

LK_NAMESPACE_BEGIN

LBezier::LBezier()
{

}

LBezier::LBezier(double x1, double y1, double x2, double y2, double x3, double y3, double x4, double y4)
    :m_x1(x1), m_y1(y1), m_x2(x2), m_y2(y2), m_x3(x3), m_y3(y3), m_x4(x4), m_y4(y4)
{

}

LPolygon<double> LBezier::toPolygon(double bezier_flattening_threshold) const
{
    // flattening is done by splitting the bezier until we can replace the segment by a straight
    // line. We split further until the control points are close enough to the line connecting the
    // boundary points.
    //
    // the Distance of a point p from a line given by the points (a,b) is given by:
    //
    // d = abs( (bx - ax)(ay - py) - (by - ay)(ax - px) ) / line_length
    //
    // We can stop splitting if both control points are close enough to the line.
    // To make the algorithm faster we use the manhattan length of the line.

    LPolygon<double> polygon;
    polygon.push_back(LPoint<double>(m_x1, m_y1));
    addToPolygon(&polygon, bezier_flattening_threshold);
    return polygon;
}

LBezier LBezier::mapBy(const LTransform2D &transform) const
{
    return LBezier::fromPoints(transform.map(pt1()), transform.map(pt2()), transform.map(pt3()), transform.map(pt4()));
}

LBezier LBezier::getSubRange(double t0, double t1) const
{
    LBezier result;
    LBezier temp;

    // cut at t1
    if (lFuzzyIsZero(t1 - double(1.))) {
        result = *this;
    } else {
        temp = *this;
        temp.parameterSplitLeft(t1, &result);
    }

    // cut at t0
    if (!lFuzzyIsZero(t0)){
        result.parameterSplitLeft(t0 / t1, &temp);
    }

    return result;
}

void LBezier::addToPolygon(LPolygon<double> *polygon, double bezierFlatteningThreshold) const
{
    LBezier beziers[10];
    int levels[10];
    beziers[0] = *this;
    levels[0] = 9;
    int top = 0;

    while (top >= 0) {
        LBezier *b = &beziers[top];
        // check if we can pop the top bezier curve from the stack
        double y4y1 = b->m_y4 - b->m_y1;
        double x4x1 = b->m_x4 - b->m_x1;
        double l = std::abs(x4x1) + std::abs(y4y1);
        double d;
        if (l > 1.) {
            d = std::abs( (x4x1)*(b->m_y1 - b->m_y2) - (y4y1)*(b->m_x1 - b->m_x2) )
                + std::abs( (x4x1)*(b->m_y1 - b->m_y3) - (y4y1)*(b->m_x1 - b->m_x3) );
        } else {
            d = std::abs(b->m_x1 - b->m_x2) + std::abs(b->m_y1 - b->m_y2) +
                std::abs(b->m_x1 - b->m_x3) + std::abs(b->m_y1 - b->m_y3);
            l = 1.;
        }
        if (d < bezierFlatteningThreshold * l || levels[top] == 0) {
            // good enough, we pop it off and add the endpoint
            polygon->push_back(LPoint<double>(b->m_x4, b->m_y4));
            --top;
        } else {
            // split, second half of the polygon goes lower into the stack
            std::tie(b[1], b[0]) = b->split();
            levels[top + 1] = --levels[top];
            ++top;
        }
    }
}

LRect<double> LBezier::bounds() const
{
    double xmin = m_x1;
    double xmax = m_x1;
    if (m_x2 < xmin){
        xmin = m_x2;
    }
    else if (m_x2 > xmax){
        xmax = m_x2;
    }
    if (m_x3 < xmin){
        xmin = m_x3;
    }
    else if (m_x3 > xmax){
        xmax = m_x3;
    }
    if (m_x4 < xmin){
        xmin = m_x4;
    }
    else if (m_x4 > xmax){
        xmax = m_x4;
    }

    double ymin = m_y1;
    double ymax = m_y1;
    if (m_y2 < ymin){
        ymin = m_y2;
    }
    else if (m_y2 > ymax){
        ymax = m_y2;
    }
    if (m_y3 < ymin){
        ymin = m_y3;
    }
    else if (m_y3 > ymax){
        ymax = m_y3;
    }
    if (m_y4 < ymin){
        ymin = m_y4;
    }
    else if (m_y4 > ymax){
        ymax = m_y4;
    }
    return LRect<double>(xmin, ymin, xmax, ymax, true);
}


enum ShiftResult {
    Ok,
    Discard,
    Split,
    Circle
};

static ShiftResult good_offset(const LBezier *b1, const LBezier *b2, double offset, double threshold)
{
    const double o2 = offset*offset;
    const double max_dist_line = threshold*offset*offset;
    const double max_dist_normal = threshold*offset;
    const double spacing = double(0.25);
    for (double i = spacing; i < double(0.99); i += spacing) {
        LPoint<double> p1 = b1->pointAt(i);
        LPoint<double> p2 = b2->pointAt(i);
        double d = (p1.m_x - p2.m_x)*(p1.m_x - p2.m_x) + (p1.m_y - p2.m_y)*(p1.m_y - p2.m_y);
        if (std::abs(d - o2) > max_dist_line)
            return Split;

        LPoint<double> normalPoint = b1->normalVector(i);
        double l = std::abs(normalPoint.m_x) + std::abs(normalPoint.m_y);
        if (l != double(0.0)) {
            d = std::abs( normalPoint.m_x*(p1.m_y - p2.m_y) - normalPoint.m_y*(p1.m_x - p2.m_x) ) / l;
            if (d > max_dist_normal){
                return Split;
            }
        }
    }
    return Ok;
}

static ShiftResult shift(const LBezier *orig, LBezier *shifted, double offset, double threshold)
{
    int map[4];
    bool p1_p2_equal = lFuzzyCompare(orig->m_x1, orig->m_x2) && lFuzzyCompare(orig->m_y1, orig->m_y2);
    bool p2_p3_equal = lFuzzyCompare(orig->m_x2, orig->m_x3) && lFuzzyCompare(orig->m_y2, orig->m_y3);
    bool p3_p4_equal = lFuzzyCompare(orig->m_x3, orig->m_x4) && lFuzzyCompare(orig->m_y3, orig->m_y4);

    LPoint<double> points[4];
    int np = 0;
    points[np] = LPoint<double>(orig->m_x1, orig->m_y1);
    map[0] = 0;
    ++np;
    if (!p1_p2_equal) {
        points[np] = LPoint<double>(orig->m_x2, orig->m_y2);
        ++np;
    }
    map[1] = np - 1;
    if (!p2_p3_equal) {
        points[np] = LPoint<double>(orig->m_x3, orig->m_y3);
        ++np;
    }
    map[2] = np - 1;
    if (!p3_p4_equal) {
        points[np] = LPoint<double>(orig->m_x4, orig->m_y4);
        ++np;
    }
    map[3] = np - 1;
    if (np == 1)
        return Discard;

    LRect<double> b = orig->bounds();
    if (np == 4 && b.width() < .1*offset && b.height() < .1*offset) {
        double l = (orig->m_x1 - orig->m_x2)*(orig->m_x1 - orig->m_x2) +
                  (orig->m_y1 - orig->m_y2)*(orig->m_y1 - orig->m_y2) *
                      (orig->m_x3 - orig->m_x4)*(orig->m_x3 - orig->m_x4) +
                  (orig->m_y3 - orig->m_y4)*(orig->m_y3 - orig->m_y4);
        double dot = (orig->m_x1 - orig->m_x2)*(orig->m_x3 - orig->m_x4) +
                    (orig->m_y1 - orig->m_y2)*(orig->m_y3 - orig->m_y4);
        if (dot < 0 && dot*dot < 0.8*l){
            // the points are close and reverse dirction. Approximate the whole
            // thing by a semi circle
            return Circle;
        }
    }

    LPoint<double> points_shifted[4];

    LLine<double> prev = LLine<double>(LPoint<double>(), points[1] - points[0]);
    if (!prev.length()){
        return Discard;
    }
    LPoint<double> prevNormal = prev.normalVector().unitVector().m_pt2;

    points_shifted[0] = points[0] + offset * prevNormal;

    for (int i = 1; i < np - 1; ++i) {
        LLine<double> next = LLine<double>(LPoint<double>(), points[i + 1] - points[i]);
        LPoint<double> next_normal = next.normalVector().unitVector().m_pt2;

        LPoint<double> normal_sum = prevNormal + next_normal;

        double r = double(1.0) + prevNormal.m_x * next_normal.m_x
                  + prevNormal.m_y * next_normal.m_y;

        if (lFuzzyIsZero(r)) {
            points_shifted[i] = points[i] + offset * prevNormal;
        } else {
            double k = offset / r;
            points_shifted[i] = points[i] + k * normal_sum;
        }

        prevNormal = next_normal;
    }

    points_shifted[np - 1] = points[np - 1] + offset * prevNormal;

    *shifted = LBezier::fromPoints(points_shifted[map[0]], points_shifted[map[1]],
                                   points_shifted[map[2]], points_shifted[map[3]]);

    if (np > 2){
        return good_offset(orig, shifted, offset, threshold);
    }
    return Ok;
}

// This value is used to determine the length of control point vectors
// when approximating arc segments as curves. The factor is multiplied
// with the radius of the circle.
#define KAPPA double(0.5522847498)


static bool addCircle(const LBezier *b, double offset, LBezier *o)
{
    LPoint<double> normals[3];

    normals[0] = LPoint<double>(b->m_y2 - b->m_y1, b->m_x1 - b->m_x2);
    double dist = std::sqrt(normals[0].m_x*normals[0].m_x + normals[0].m_y*normals[0].m_y);
    if (lFuzzyIsZero(dist)){
        return false;
    }
    normals[0] /= dist;
    normals[2] = LPoint<double>(b->m_y4 - b->m_y3, b->m_x3 - b->m_x4);
    dist = std::sqrt(normals[2].m_x*normals[2].m_x + normals[2].m_y*normals[2].m_y);
    if (lFuzzyIsZero(dist)){
        return false;
    }
    normals[2] /= dist;

    normals[1] = LPoint<double>(b->m_x1 - b->m_x2 - b->m_x3 + b->m_x4, b->m_y1 - b->m_y2 - b->m_y3 + b->m_y4);
    normals[1] /= -1*std::sqrt(normals[1].m_x*normals[1].m_x + normals[1].m_y*normals[1].m_y);

    double angles[2];
    double sign = 1.;
    for (int i = 0; i < 2; ++i) {
        double cos_a = normals[i].m_x*normals[i+1].m_x + normals[i].m_y*normals[i+1].m_y;
        if (cos_a > 1.){
            cos_a = 1.;
        }
        if (cos_a < -1.){
            cos_a = -1;
        }
        angles[i] = std::acos(cos_a) * double(M_1_PI);
    }

    if (angles[0] + angles[1] > 1.) {
        // more than 180 degrees
        normals[1] = -normals[1];
        angles[0] = 1. - angles[0];
        angles[1] = 1. - angles[1];
        sign = -1.;

    }

    LPoint<double> circle[3];
    circle[0] = LPoint<double>(b->m_x1, b->m_y1) + normals[0]*offset;
    circle[1] = LPoint<double>(double(0.5)*(b->m_x1 + b->m_x4), double(0.5)*(b->m_y1 + b->m_y4)) + normals[1]*offset;
    circle[2] = LPoint<double>(b->m_x4, b->m_y4) + normals[2]*offset;

    for (int i = 0; i < 2; ++i) {
        double kappa = double(2.0) * KAPPA * sign * offset * angles[i];

        o->m_x1 = circle[i].m_x;
        o->m_y1 = circle[i].m_y;
        o->m_x2 = circle[i].m_x - normals[i].m_y*kappa;
        o->m_y2 = circle[i].m_y + normals[i].m_x*kappa;
        o->m_x3 = circle[i+1].m_x + normals[i+1].m_y*kappa;
        o->m_y3 = circle[i+1].m_y - normals[i+1].m_x*kappa;
        o->m_x4 = circle[i+1].m_x;
        o->m_y4 = circle[i+1].m_y;

        ++o;
    }
    return true;
}

int LBezier::shifted(LBezier *curveSegments, int maxSegments, double offset, float threshold) const
{
    if (lFuzzyCompare(m_x1, m_x2) && lFuzzyCompare(m_x1, m_x3) && lFuzzyCompare(m_x1, m_x4) &&
        lFuzzyCompare(m_y1, m_y2) && lFuzzyCompare(m_y1, m_y3) && lFuzzyCompare(m_y1, m_y4)){
        return 0;
    }

    --maxSegments;
    LBezier beziers[10];
redo:
    beziers[0] = *this;
    LBezier *b = beziers;
    LBezier *o = curveSegments;

    while (b >= beziers) {
        int stack_segments = b - beziers + 1;
        if ((stack_segments == 10) || (o - curveSegments == maxSegments - stack_segments)) {
            threshold *= double(1.5);
            if (threshold > double(2.0)){
                goto give_up;
            }
            goto redo;
        }
        ShiftResult res = shift(b, o, offset, threshold);
        if (res == Discard) {
            --b;
        } else if (res == Ok) {
            ++o;
            --b;
        } else if (res == Circle && maxSegments - (o - curveSegments) >= 2) {
            // add semi circle
            if (addCircle(b, offset, o))
                o += 2;
            --b;
        } else {
            std::tie(b[1], b[0]) = b->split();
            ++b;
        }
    }

give_up:
    while (b >= beziers) {
        ShiftResult res = shift(b, o, offset, threshold);

        // if res isn't Ok or Split then *o is undefined
        if (res == Ok || res == Split){
            ++o;
        }

        --b;
    }
    return o - curveSegments;
}

double LBezier::length(double error) const
{
    double len = double(0.0);
    addIfClose(&len, error);
    return len;
}

void LBezier::addIfClose(double *length, double error) const
{
    double len = double(0.0);  /* arc length */
    double chord;             /* chord length */

    len = len + LLine<double>(LPoint<double>(m_x1, m_y1),LPoint<double>(m_x2, m_y2)).length();
    len = len + LLine<double>(LPoint<double>(m_x2, m_y2),LPoint<double>(m_x3, m_y3)).length();
    len = len + LLine<double>(LPoint<double>(m_x3, m_y3),LPoint<double>(m_x4, m_y4)).length();

    chord = LLine<double>(LPoint<double>(m_x1, m_y1),LPoint<double>(m_x4, m_y4)).length();

    if((len-chord) > error) {
        const auto halves = split();                  /* split in two */
        halves.first.addIfClose(length, error);       /* try left side */
        halves.second.addIfClose(length, error);      /* try right side */
        return;
    }

    *length = *length + len;

    return;
}

double LBezier::tForY(double t0, double t1, double y) const
{
    double py0 = pointAt(t0).m_y;
    double py1 = pointAt(t1).m_y;

    if (py0 > py1) {
        std::swap(py0, py1);
        std::swap(t0, t1);
    }

    if (py0 >= y){
        return t0;
    }
    else if (py1 <= y){
        return t1;
    }

    double lt = t0;
    double dt;
    do {
        double t = double(0.5) * (t0 + t1);

        double a, b, c, d;
        LBezier::coefficients(t, a, b, c, d);
        double yt = a * m_y1 + b * m_y2 + c * m_y3 + d * m_y4;

        if (yt < y) {
            t0 = t;
            py0 = yt;
        } else {
            t1 = t;
            py1 = yt;
        }
        dt = lt - t;
        lt = t;
    } while (std::abs(dt) > double(1e-7));

    return t0;
}

int LBezier::stationaryYPoints(double &t0, double &t1) const
{
    // y(t) = (1 - t)^3 * y1 + 3 * (1 - t)^2 * t * y2 + 3 * (1 - t) * t^2 * y3 + t^3 * y4
    // y'(t) = 3 * (-(1-2t+t^2) * y1 + (1 - 4 * t + 3 * t^2) * y2 + (2 * t - 3 * t^2) * y3 + t^2 * y4)
    // y'(t) = 3 * ((-y1 + 3 * y2 - 3 * y3 + y4)t^2 + (2 * y1 - 4 * y2 + 2 * y3)t + (-y1 + y2))

    const double a = -m_y1 + 3 * m_y2 - 3 * m_y3 + m_y4;
    const double b = 2 * m_y1 - 4 * m_y2 + 2 * m_y3;
    const double c = -m_y1 + m_y2;

    if (lFuzzyIsZero(a)) {
        if (lFuzzyIsZero(b)){
            return 0;
        }

        t0 = -c / b;
        return t0 > 0 && t0 < 1;
    }

    double reciprocal = b * b - 4 * a * c;

    if (lFuzzyIsZero(reciprocal)) {
        t0 = -b / (2 * a);
        return t0 > 0 && t0 < 1;
    } else if (reciprocal > 0) {
        double temp = std::sqrt(reciprocal);

        t0 = (-b - temp)/(2*a);
        t1 = (-b + temp)/(2*a);

        if (t1 < t0)
            std::swap(t0, t1);

        int count = 0;
        double t[2] = { 0, 1 };

        if (t0 > 0 && t0 < 1){
            t[count++] = t0;
        }
        if (t1 > 0 && t1 < 1){
            t[count++] = t1;
        }

        t0 = t[0];
        t1 = t[1];

        return count;
    }

    return 0;
}

double LBezier::tAtLength(double l) const
{
    double len = length();
    double t   = double(1.0);
    const double error = double(0.01);
    if (l > len || lFuzzyCompare(l, len)){
        return t;
    }

    t *= double(0.5);

    double lastBigger = double(1.0);
    while (1) {
        LBezier right = *this;
        LBezier left;
        right.parameterSplitLeft(t, &left);
        double lLen = left.length();
        if (std::abs(lLen - l) < error)
            break;

        if (lLen < l) {
            t += (lastBigger - t) * double(0.5);
        } else {
            lastBigger = t;
            t -= t * double(0.5);
        }
        //++iters;
    }
    return t;
}

LBezier LBezier::bezierOnInterval(double t0, double t1) const
{
    if (t0 == 0 && t1 == 1){
        return *this;
    }

    LBezier bezier = *this;

    LBezier result;
    bezier.parameterSplitLeft(t0, &result);
    double trueT = (t1-t0)/(1-t0);
    bezier.parameterSplitLeft(trueT, &result);

    return result;
}


LK_NAMESPACE_END
