#include "lpainterpath.h"
#include "lkcore/ldebug.h"
#include "lkgui/paint/lbezier.h"
#include "lkgui/paint/lregion.h"
#include "lpathclipper.h"

#include <corecrt_math_defines.h>

#define LK_PATH_KAPPA 0.5522847498

LK_NAMESPACE_BEGIN

static void doClose(std::vector<LPainterPath::Element>& eles, int subStart){
    const LPainterPath::Element &first = eles.at(subStart);
    LPainterPath::Element &last = eles.back();
    if(last.m_type != LPainterPath::MoveToElement){
        if (first.m_x != last.m_x || first.m_y != last.m_y) {
            if (last.m_type ==  LPainterPath::LineToElement && lFuzzyCompare(first.m_x, last.m_x) && lFuzzyCompare(first.m_y, last.m_y)) {
                last.m_x = first.m_x;
                last.m_y = first.m_y;
            } else {
                LPainterPath::Element e = { first.m_x, first.m_y, LPainterPath::LineToElement };
                eles.push_back(e);
            }
        }
    }
}

static inline bool isValidCoord(double c)
{
    return std::isfinite(c) && std::fabs(c) < 1e128;
}

static bool hasValidCoords(LPoint<double> p)
{
    return isValidCoord(p.m_x) && isValidCoord(p.m_y);
}

static bool hasValidCoords(LRect<double> r)
{
    return isValidCoord(r.m_x) && isValidCoord(r.m_y) && isValidCoord(r.width()) && isValidCoord(r.height());
}


inline double lbezierXa(const LBezier& bezier){
    return 3*(-bezier.m_x1 + 3*bezier.m_x2 - 3*bezier.m_x3 + bezier.m_x4);
}

inline double lbezierYa(const LBezier& bezier){
    return 3*(-bezier.m_y1 + 3*bezier.m_y2 - 3*bezier.m_y3 + bezier.m_y4);
}

inline double lbezierXb(const LBezier& bezier){
    return 6*(bezier.m_x1 - 2*bezier.m_x2 + bezier.m_x3);
}

inline double lbezierYb(const LBezier& bezier){
    return 6*(bezier.m_y1 - 2*bezier.m_y2 + bezier.m_y3);
}

inline double lbezierXc(const LBezier& bezier){
    return 3*(-bezier.m_x1 + bezier.m_x2);
}

inline double lbezierYc(const LBezier& bezier){
    return 3*(-bezier.m_y1 + bezier.m_y2);
}


inline void lBezierCheckT(const LBezier& bezier,
                          const double& t,
                          double& minx,
                          double& miny,
                          double& maxx,
                          double& maxy){
    if (t >= 0 && t <= 1) {
        LPoint<double> p(bezier.pointAt(t));
        if (p.m_x < minx){
            minx = p.m_x;
        }
        else if (p.m_x > maxx){
            maxx = p.m_x;
        }
        if (p.m_y < miny){
            miny = p.m_y;
        }
        else if (p.m_y > maxy){
            maxy = p.m_y;
        }
    }
}

static inline LBezier lBezierAtT(const LPainterPath &path, double t, double *startingLength, double *bezierLength)
{
    *startingLength = 0;
    if (t > 1){
        return LBezier();
    }

    double curLen = 0;
    double totalLength = path.length();

    const int lastElement = path.elementCount() - 1;
    for (int i=0; i <= lastElement; ++i) {
        const LPainterPath::Element &e = path.elementAt(i);

        switch (e.m_type) {
        case LPainterPath::MoveToElement:{
            break;
        }
        case LPainterPath::LineToElement:
        {
            const LPainterPath::Element &last = path.elementAt(i-1);
            double llen = hypot(last.m_x-e.m_x, last.m_y-e.m_y);
            curLen += llen;
            if (i == lastElement || curLen/totalLength >= t) {
                *bezierLength = llen;
                LPoint<double> a = path.elementAt(i - 1).toPointF();
                LPoint<double> ep = e.toPointF();
                LPoint<double> delta = ep - a;
                return LBezier::fromPoints(a, a + delta / 3, a + 2 * delta / 3, ep);
            }
            break;
        }
        case LPainterPath::CurveToElement:
        {
            if((i+2) < path.elementCount()){
                LBezier b = LBezier::fromPoints(path.elementAt(i-1).toPointF(),
                                                e.toPointF(),
                                                path.elementAt(i+1).toPointF(),
                                                path.elementAt(i+2).toPointF());
                double blen = b.length();
                curLen += blen;

                if (i + 2 == lastElement || curLen/totalLength >= t) {
                    *bezierLength = blen;
                    return b;
                }
            }
            i += 2;
            break;
        }
        default:
            break;
        }
        *startingLength = curLen;
    }
    return LBezier();
}


//derivative of the equation
static inline double lSlopeAt(double t, double a, double b, double c, double d)
{
    return 3*t*t*(d - 3*c + 3*b - a) + 6*t*(c - 2*b + a) + 3*(b - a);
}

static LRect<double> painterPathBezierExtrema(const LBezier &b)
{
    double minx, miny, maxx, maxy;

    // initialize with end points
    if (b.m_x1 < b.m_x4) {
        minx = b.m_x1;
        maxx = b.m_x4;
    } else {
        minx = b.m_x4;
        maxx = b.m_x1;
    }
    if (b.m_y1 < b.m_y4) {
        miny = b.m_y1;
        maxy = b.m_y4;
    } else {
        miny = b.m_y4;
        maxy = b.m_y1;
    }

    // Update for the X extrema
    {
        double ax = lbezierXa(b);
        double bx = lbezierXb(b);
        double cx = lbezierXc(b);
        // specialcase quadratic curves to avoid div by zero
        if (lFuzzyIsZero(ax)) {

            // linear curves are covered by initialization.
            if (!lFuzzyIsZero(bx)) {
                double t = -cx / bx;
                lBezierCheckT(b, t, minx, miny, maxx, maxy);
            }

        } else {
            const double tx = bx * bx - 4 * ax * cx;

            if (tx >= 0) {
                double temp = std::sqrt(tx);
                double rcp = 1 / (2 * ax);
                double t1 = (-bx + temp) * rcp;
                lBezierCheckT(b, t1, minx, miny, maxx, maxy);

                double t2 = (-bx - temp) * rcp;
                lBezierCheckT(b, t2, minx, miny, maxx, maxy);
            }
        }
    }

    // Update for the Y extrema
    {
        double ay = lbezierYa(b);
        double by = lbezierYb(b);
        double cy = lbezierYc(b);

        // specialcase quadratic curves to avoid div by zero
        if (lFuzzyIsZero(ay)) {

            // linear curves are covered by initialization.
            if (!lFuzzyIsZero(by)) {
                double t = -cy / by;
                lBezierCheckT(b, t, minx, miny, maxx, maxy);
            }

        } else {
            const double ty = by * by - 4 * ay * cy;

            if (ty > 0) {
                double temp = std::sqrt(ty);
                double rcp = 1 / (2 * ay);
                double t1 = (-by + temp) * rcp;
                lBezierCheckT(b, t1, minx, miny, maxx, maxy);

                double t2 = (-by - temp) * rcp;
                lBezierCheckT(b, t2, minx, miny, maxx, maxy);
            }
        }
    }
    return LRect<double>(minx, miny, maxx - minx, maxy - miny, false);
}

enum PainterDirections { Left, Right, Top, Bottom };

static bool lPainterPathIsectLineRect(double x1, double y1, double x2, double y2, const LRect<double> &rect)
{
    double left = rect.m_x;
    double right = rect.x2();
    double top = rect.m_y;
    double bottom = rect.y2();

    // clip the lines, after cohen-sutherland, see e.g. http://www.nondot.org/~sabre/graphpro/line6.html
    int p1 = ((x1 < left) << Left)
             | ((x1 > right) << Right)
             | ((y1 < top) << Top)
             | ((y1 > bottom) << Bottom);
    int p2 = ((x2 < left) << Left)
             | ((x2 > right) << Right)
             | ((y2 < top) << Top)
             | ((y2 > bottom) << Bottom);

    if (p1 & p2){
        // completely inside
        return false;
    }

    if (p1 | p2) {
        double dx = x2 - x1;
        double dy = y2 - y1;

        // clip x coordinates
        if (x1 < left) {
            y1 += dy/dx * (left - x1);
            x1 = left;
        } else if (x1 > right) {
            y1 -= dy/dx * (x1 - right);
            x1 = right;
        }
        if (x2 < left) {
            y2 += dy/dx * (left - x2);
            x2 = left;
        } else if (x2 > right) {
            y2 -= dy/dx * (x2 - right);
            x2 = right;
        }

        p1 = ((y1 < top) << Top)
             | ((y1 > bottom) << Bottom);
        p2 = ((y2 < top) << Top)
             | ((y2 > bottom) << Bottom);

        if (p1 & p2)
            return false;

        // clip y coordinates
        if (y1 < top) {
            x1 += dx/dy * (top - y1);
            // y1 = top;
        } else if (y1 > bottom) {
            x1 -= dx/dy * (y1 - bottom);
            // y1 = bottom;
        }
        if (y2 < top) {
            x2 += dx/dy * (top - y2);
            // y2 = top;
        } else if (y2 > bottom) {
            x2 -= dx/dy * (y2 - bottom);
            // y2 = bottom;
        }

        p1 = ((x1 < left) << Left)
             | ((x1 > right) << Right);
        p2 = ((x2 < left) << Left)
             | ((x2 > right) << Right);

        if (p1 & p2){
            return false;
        }
        return true;
    }
    return false;
}


static void lPainterPathIsectCurve(const LBezier &bezier, const LPoint<double> &pt,
                                       int *winding, int depth = 0)
{
    double y = pt.m_y;
    double x = pt.m_x;
    LRect<double> bounds = bezier.bounds();

    // potential intersection, divide and try again...
    // Please note that a sideeffect of the bottom exclusion is that
    // horizontal lines are dropped, but this is correct according to
    // scan conversion rules.
    if (y >= bounds.m_y && y < bounds.m_y + bounds.height()) {

        // hit lower limit... This is a rough threshold, but its a
        // tradeoff between speed and precision.
        const double lowerBound = 0.001;
        if (depth == 32 || (bounds.width() < lowerBound && bounds.height() < lowerBound)) {
            // We make the assumption here that the curve starts to
            // approximate a line after while (i.e. that it doesn't
            // change direction drastically during its slope)
            if (bezier.pt1().m_x <= x) {
                (*winding) += (bezier.pt4().m_y > bezier.pt1().m_y ? 1 : -1);
            }
            return;
        }

        // split curve and try again...
        const auto halves = bezier.split();
        lPainterPathIsectCurve(halves.first,  pt, winding, depth + 1);
        lPainterPathIsectCurve(halves.second, pt, winding, depth + 1);
    }
}

static bool lIsectCurveHorizontal(const LBezier &bezier, double y, double x1, double x2, int depth = 0)
{
    LRect<double> bounds = bezier.bounds();

    if (y >= bounds.m_y && y < bounds.y2()
        && bounds.x2() >= x1 && bounds.m_x < x2) {
        const double lower_bound = double(.01);
        if (depth == 32 || (bounds.width() < lower_bound && bounds.height() < lower_bound)){
            return true;
        }

        const auto halves = bezier.split();
        if (lIsectCurveHorizontal(halves.first, y, x1, x2, depth + 1)
            || lIsectCurveHorizontal(halves.second, y, x1, x2, depth + 1)){
            return true;
        }
    }
    return false;
}

static bool lIsectCurveVertical(const LBezier &bezier, double x, double y1, double y2, int depth = 0)
{
    LRect<double> bounds = bezier.bounds();

    if (x >= bounds.m_x && x < bounds.x2()
        && bounds.y2() >= y1 && bounds.m_y < y2) {
        const double lower_bound = double(.01);
        if (depth == 32 || (bounds.width() < lower_bound && bounds.height() < lower_bound)){
            return true;
        }

        const auto halves = bezier.split();
        if (lIsectCurveVertical(halves.first, x, y1, y2, depth + 1)
            || lIsectCurveVertical(halves.second, x, y1, y2, depth + 1)){
            return true;
        }
    }
    return false;
}

static bool pointOnEdge(const LRect<double> &rect, const LPoint<double> &point)
{
    if ((point.m_x == rect.m_x || point.m_x == rect.x2()) &&
        (point.m_y >= rect.m_y && point.m_y <= rect.y2())){
        return true;
    }
    if ((point.m_y == rect.m_y || point.m_y == rect.y2()) &&
        (point.m_x >= rect.m_x && point.m_x <= rect.x2())){
        return true;
    }
    return false;
}

/*
    Returns \c true if any lines or curves cross the four edges in of rect
*/
static bool lPainterPathCheckCrossing(const std::shared_ptr<LPainterPath> path, const LRect<double> &rect)
{
    LPoint<double> lastPt;
    LPoint<double> lastStart;
    enum { OnRect, InsideRect, OutsideRect} edgeStatus = OnRect;
    for (int i=0; i<path->elementCount(); ++i) {
        const LPainterPath::Element &e = path->elementAt(i);

        switch (e.m_type) {
        case LPainterPath::MoveToElement:{
            if (i > 0
                && lFuzzyCompare(lastPt.m_x, lastStart.m_x)
                && lFuzzyCompare(lastPt.m_y, lastStart.m_y)
                && lPainterPathIsectLineRect(lastPt.m_x, lastPt.m_y, lastStart.m_x, lastStart.m_y, rect)){
                return true;
            }
            lastStart = lastPt = e.toPointF();
            break;
        }
        case LPainterPath::LineToElement:{
            if (lPainterPathIsectLineRect(lastPt.m_x, lastPt.m_y, e.m_x, e.m_y, rect))
                return true;
            lastPt = e.toPointF();
            break;
        }
        case LPainterPath::CurveToElement:{
            LPoint<double> cp2 = path->elementAt(++i).toPointF();
            LPoint<double> ep = path->elementAt(++i).toPointF();
            LBezier bezier = LBezier::fromPoints(lastPt, e.toPointF(), cp2, ep);
            if (lIsectCurveHorizontal(bezier, rect.m_y, rect.m_x, rect.x2())
                || lIsectCurveHorizontal(bezier, rect.y2(), rect.m_x, rect.x2())
                || lIsectCurveVertical(bezier, rect.m_x, rect.m_y, rect.y2())
                || lIsectCurveVertical(bezier, rect.x2(), rect.m_y, rect.y2())){
                return true;
            }
            lastPt = ep;
            break;
        }
        default:{
            break;
        }
        }
        // Handle crossing the edges of the rect at the end-points of individual sub-paths.
        // A point on on the edge itself is considered neither inside nor outside for this purpose.
        if (!pointOnEdge(rect, lastPt)) {
            bool contained = rect.contains(lastPt);
            switch (edgeStatus) {
            case OutsideRect:{
                if (contained){
                    return true;
                }
                break;
            }
            case InsideRect:{
                if (!contained){
                    return true;
                }
                break;
            }
            case OnRect:{
                edgeStatus = contained ? InsideRect : OutsideRect;
                break;
            }
            default:{
                break;
            }
            }
        } else {
            if (lastPt == lastStart){
                edgeStatus = OnRect;
            }
        }
    }

    // implicitly close last subpath
    if (lastPt != lastStart
        && lPainterPathIsectLineRect(lastPt.m_x, lastPt.m_y, lastStart.m_x, lastStart.m_y, rect)){
        return true;
    }
    return false;
}



LPainterPath::LPainterPath(std::shared_ptr<LPainterPath> p, bool isOutline, bool disableCurve, double curveThres)
{
    if(p != nullptr){
        if(((p->isOutline() != isOutline) && isOutline)|| p->isDisableCurve() != disableCurve){
            m_flags = p->m_flags;
            m_isOutline = isOutline;
            m_disableCurve = disableCurve;
            m_curveThreshold = curveThres;
            m_elements = toElements(p->m_elements, m_cStart,m_isOutline, m_disableCurve, m_curveThreshold);
            m_hints = p->m_hints;
            m_bounds = p->m_bounds;
            m_controlBounds = p->m_controlBounds;
        }else{
            *this = *p;
            m_isOutline = isOutline;
            m_disableCurve = disableCurve;
            m_curveThreshold = curveThres;
        }
    }else{
        m_isOutline = isOutline;
        m_disableCurve = disableCurve;
        m_curveThreshold = curveThres;
    }
}

LPainterPath::LPainterPath(const LPoint<double> &startPoint, bool isOutline, bool disableCurve, double curveThres)
{
    m_curveThreshold = curveThres;
    m_isOutline = isOutline;
    m_disableCurve = disableCurve;
    Element e = { startPoint.m_x, startPoint.m_y, MoveToElement };
    m_elements.push_back(e);
}

LPainterPath::LPainterPath(const LPainterPath &other, bool isOutline, bool disableCurve,  double curveThres)
{
    if(((other.isOutline() != isOutline) && isOutline)|| other.isDisableCurve() != disableCurve){
        m_flags = other.m_flags;
        m_isOutline = isOutline;
        m_disableCurve = disableCurve;
        m_curveThreshold = curveThres;
        m_elements = toElements(other.m_elements, m_cStart, m_isOutline, m_disableCurve, m_curveThreshold);
        m_hints = other.m_hints;
        m_bounds = other.m_bounds;
        m_controlBounds = other.m_controlBounds;
    }else{
        *this = other;
        m_isOutline = isOutline;
        m_disableCurve = disableCurve;
        m_curveThreshold = curveThres;
    }

}

bool LPainterPath::isClosed() const{
    if(m_cStart >=0 && m_cStart < (int)m_elements.size()){
        const Element &first = m_elements.at(m_cStart);
        const Element &last = m_elements.back();
        return (first.m_x == last.m_x) && (first.m_y == last.m_y);
    }
    return false;
}



void LPainterPath::close()
{
    if(m_cStart >=0 && m_cStart < (int)m_elements.size()){
        m_requireMoveTo = true;
        doClose(m_elements, m_cStart);
    }
}

void LPainterPath::closeSubpath()
{
    if (isEmpty()){
        return;
    }
    resetMark();
    close();
}

std::list<LPolygon<double> > LPainterPath::toSubpathPolygons(const LTransform2D &matrix) const
{
    std::list<LPolygon<double> > flatCurves;
    if (isEmpty()){
        return flatCurves;
    }

    LPolygon<double> current;
    for (int i=0; i<(int)m_elements.size(); ++i) {
        const Element &e = m_elements.at(i);
        switch (e.m_type) {
        case MoveToElement:{
            if (current.size() > 1){
                flatCurves.push_back(current);
            }
            current.clear();
            current.reserve(16);
            current.push_back(LPoint<double>(e.m_x, e.m_y) * matrix);
            break;
        }
        case LineToElement:
            current.push_back(LPoint<double>(e.m_x, e.m_y) * matrix);
            break;
        case CurveToElement: {
            if(i>=1 && (i+2) < m_elements.size()){
                LBezier bezier = LBezier::fromPoints(LPoint<double>(m_elements.at(i-1).m_x, m_elements.at(i-1).m_y) * matrix,
                                                     LPoint<double>(e.m_x, e.m_y) * matrix,
                                                     LPoint<double>(m_elements.at(i+1).m_x, m_elements.at(i+1).m_y) * matrix,
                                                     LPoint<double>(m_elements.at(i+2).m_x, m_elements.at(i+2).m_y) * matrix);
                bezier.addToPolygon(&current, m_curveThreshold);
            }
            i+=2;
            break;
        }
        default:{
            break;
        }
        }
    }

    if (current.size()>1){
        flatCurves.push_back(current);
    }
    return flatCurves;
}

std::list<LPolygon<double> > LPainterPath::toFillPolygons(const LTransform2D &matrix) const
{
    std::list<LPolygon<double> > polys;

    std::list<LPolygon<double> > subpaths = toSubpathPolygons(matrix);
    int count = subpaths.size();

    if (count == 0){
        return polys;
    }

    std::vector<LRect<double>> bounds;
    bounds.reserve(count);
    for (const LPolygon<double> & p : subpaths){
        bounds.push_back(p.boundingRect());
    }


    std::vector< std::vector<int> > isects;
    isects.resize(count);

    // find all intersections
    for (int j=0; j<count; ++j) {
        for (const LPolygon<double> & p : subpaths){
            if(p.size() <= 2){
                continue;
            }
        }

        const LRect<double>& cbounds = bounds[j];
        for (int i=0; i<count; ++i) {
            if (cbounds.isIntersects(bounds[i])) {
                isects[j].push_back(i);
            }
        }
    }


    // flatten the sets of intersections
    for (int i=0; i<count; ++i) {
        const std::vector<int> &currentIsects = isects[i];
        for (int j=0; j<currentIsects.size(); ++j) {
            int isectJ = currentIsects[j];
            if (isectJ == i){
                continue;
            }
            const std::vector<int> &isectsJ = isects[isectJ];
            for (int k = 0, size = isectsJ.size(); k < size; ++k) {
                int isectK = isectsJ.at(k);
                if (isectK != i ) {
                    auto it = std::find(currentIsects.cbegin(), currentIsects.cend(), isectK);
                    if(it != currentIsects.cend()){
                        isects[i].push_back(isectK);
                    }
                }
            }
            isects[isectJ].clear();
        }
    }


    // Join the intersected subpaths as rewinded polygons
    for (int i=0; i<count; ++i) {
        const std::vector<int> &subpathVec = isects[i];
        if (!subpathVec.empty()) {
            LPolygon<double> buildUp;
            for (int j=0; j<subpathVec.size(); ++j) {
                auto it = subpaths.cbegin();
                std::advance(it, subpathVec[j]);

                buildUp.insert(buildUp.end(), it->cbegin(), it->cend());
                if (!it->isClosed()){
                    buildUp.push_back(it->front());
                }
                if (!buildUp.isClosed()){
                    buildUp.push_back(buildUp.front());
                }
            }
            polys.push_back(buildUp);
        }
    }

    return polys;
}

LPolygon<double> LPainterPath::toFillPolygon(const LTransform2D &matrix) const
{
    const std::list<LPolygon<double> >  flats = toSubpathPolygons(matrix);
    LPolygon<double> polygon;
    if (flats.empty()){
        return polygon;
    }
    std::list<LPolygon<double> > ::const_iterator iter = flats.cbegin();
    LPoint<double> first = iter->front();
    for (; iter != flats.cend(); ++iter) {
        polygon.insert(polygon.end(),iter->cbegin(), iter->cend());
        if (!iter->isClosed()){
            polygon.push_back(iter->front());
        }
        if (iter != flats.cbegin()){
            polygon.push_back(first);
        }
    }
    return polygon;
}

LFillRule LPainterPath::fillRule() const
{
    return isEmpty() ? LFillRule::OddEven : m_fillRule;
}

void LPainterPath::setFillRule(LFillRule fillRule)
{
    ensureData();
    if (m_fillRule == fillRule){
        return;
    }
    resetMark();

    m_fillRule = fillRule;
}

LRect<double> LPainterPath::boundingRect() const
{
    if (m_dirtyBounds){
        const_cast<LPainterPath*>(this)->computeBoundingRect();
    }
    return m_bounds;
}

LRect<double> LPainterPath::controlPointRect() const
{
    if (m_dirtyControlBounds){
        const_cast<LPainterPath*>(this)->computeControlPointRect();
    }
    return m_controlBounds;
}

void LPainterPath::translate(double dx, double dy)
{
    if (isEmpty() || (dx == 0 && dy == 0)){
        return;
    }

    resetMark();
    for(LPainterPath::Element& element : m_elements){
        element.m_x += dx;
        element.m_y += dy;
    }
}

void LPainterPath::maybeMoveTo()
{
    if (m_requireMoveTo) {
        Element e = m_elements.back();
        if(e.m_type != MoveToElement){
            e.m_type = MoveToElement;
            m_elements.push_back(e);
        }
        m_requireMoveTo = false;
    }
}

void LPainterPath::clear()
{
    m_elements.clear();

    m_hints = 0;
    m_cStart = 0;
    m_fillRule = LFillRule::OddEven;
    m_bounds.reset();
    m_controlBounds.reset();

    m_requireMoveTo = false;
    m_dirtyBounds = false;
    m_dirtyControlBounds = false;
    m_dirtyHints = false;
    m_convex = false;
    m_isRectInit = false;

}

bool LPainterPath::isRect() const
{
    return shape() == RectangleHint;
}

bool LPainterPath::isRect(const LPoint<double> *pt, int count)
{
    if(count != 5 && count != 4 ){
        return false;
    }else{
        if(count == 5){
            return  pt[0].m_x == pt[4].m_x
                   && pt[0].m_y == pt[4].m_y // last point == first point
                   && pt[0].m_x == pt[3].m_x
                   && pt[1].m_x == pt[2].m_x // x values equal
                   && pt[0].m_y == pt[1].m_y
                   && pt[2].m_y == pt[3].m_y // y values equal...
                   && pt[0].m_x < pt[1].m_x && pt[0].m_y < pt[2].m_y;
        }else{
            return pt[0].m_x == pt[3].m_x
                   && pt[1].m_x == pt[2].m_x // x values equal
                   && pt[0].m_y == pt[1].m_y
                   && pt[2].m_y == pt[3].m_y // y values equal...
                   && pt[0].m_x < pt[1].m_x && pt[0].m_y < pt[2].m_y;
        }
    }

}

void LPainterPath::addRect(const LRect<double> &r)
{
    if (!hasValidCoords(r)) {
        return;
    }

    if (r.isNull()){
        return;
    }

    ensureData();
    resetMark();

    bool isOrgEmpty = isEmpty();
    bool first = m_elements.size() < 2;

    moveTo(r.m_x, r.m_y);

    Element l1 = { r.m_x + r.width(), r.m_y, LineToElement };
    Element l2 = { r.m_x + r.width(), r.m_y + r.height(), LineToElement };
    Element l3 = { r.m_x, r.m_y + r.height(), LineToElement };
    Element l4 = { r.m_x, r.m_y, LineToElement };

    m_elements.push_back(l1);
    m_elements.push_back(l2);
    m_elements.push_back(l3);
    m_elements.push_back(l4);

    m_requireMoveTo = true;
    m_convex = first;
    m_isRectInit = isOrgEmpty;
}

void LPainterPath::addEllipse(const LRect<double> &boundingRect)
{
    if (!hasValidCoords(boundingRect)) {
        return;
    }

    if (boundingRect.isNull()){
        return;
    }

    ensureData();
    resetMark();

    bool first = m_elements.size() < 2;


    std::vector<LPoint<double> > pts = curvesForArc(boundingRect, 0, -360);
    if(pts.size() >= 13){
        moveTo(pts[0]);
        cubicTo(pts[1], pts[2], pts[3]);           // 0 -> 270
        cubicTo(pts[4], pts[5], pts[6]);           // 270 -> 180
        cubicTo(pts[7], pts[8], pts[9]);           // 180 -> 90
        cubicTo(pts[10], pts[11], pts[12]);         // 90 - >0
    }
    m_requireMoveTo = true;
    m_convex = first;
    m_isRectInit = false;
}

void LPainterPath::addPolygon(const LPolygon<double> &polygon)
{
    if (polygon.empty()){
        return;
    }

    ensureData();
    resetMark();

    moveTo(polygon.front());
    for (uint32_t i=1; i<polygon.size(); ++i) {
        Element elm = { polygon.at(i).m_x, polygon.at(i).m_y, LineToElement };
        m_elements.push_back(elm);
    }
    m_isRectInit = false;
}

void LPainterPath::addPolygon(const LPoint<double> *points, int pointCount)
{
    if(pointCount>0){
        ensureData();
        resetMark();

        moveTo(points[0]);
        for (int i=1; i<pointCount; ++i) {
            Element elm = { points[i].m_x, points[i].m_y, LineToElement };
            m_elements.push_back(elm);
        }
        m_isRectInit = false;
    }
}

void LPainterPath::addLine(const LLine<double> line)
{
    ensureData();
    resetMark();

    moveTo(line.m_pt1);
    Element elm = { line.m_pt2.m_x, line.m_pt2.m_y, LineToElement };
    m_elements.push_back(elm);
    m_isRectInit = false;
}

void LPainterPath::addLine(const LLine<double> *lines, int lineCount)
{
    if(lineCount>0){
        ensureData();
        resetMark();

        moveTo(lines[0].m_pt1);
        Element elm = { lines[0].m_pt2.m_x, lines[0].m_pt2.m_y, LineToElement };
        m_elements.push_back(elm);
        for (int i=1; i<lineCount; ++i) {
            Element elm1 = { lines[i].m_pt1.m_x, lines[i].m_pt1.m_y, MoveToElement };
            m_elements.push_back(elm1);
            Element elm2 = { lines[i].m_pt2.m_x, lines[i].m_pt2.m_y, LineToElement };
            m_elements.push_back(elm2);
        }
        m_isRectInit = false;
    }
}

void LPainterPath::addPath(const std::shared_ptr<LPainterPath> other)
{
    if (other == nullptr || other->isEmpty()){
        return;
    }

    ensureData();
    resetMark();

    if(!m_elements.empty()){
        if (m_elements.back().m_type == MoveToElement){
            m_elements.pop_back();
        }
    }
    // Locate where our own current subpath will start after the other path is added.
    int cStart = m_elements.size() + other->m_cStart;
    m_elements.insert(m_elements.end(), other->m_elements.cbegin(), other->m_elements.cend());
    m_cStart = cStart;

    m_requireMoveTo = other->isClosed();
}

void LPainterPath::addRegion(const std::shared_ptr<LRegion> region)
{
    if(region != nullptr){
        ensureData();
        resetMark();

        for (const LRect<int> &rect : *region){
            addRect(rect.toRectF());
        }
    }
}

bool LPainterPath::ajustRounedRectRadius(const LRect<double> &r, double &xRadius, double &yRadius, LSizeMode mode)
{
    if (!r.isValid()){
        return false;
    }

    if (mode == LSizeMode::Absolute) {
        double w = r.m_width / 2;
        double h = r.m_height / 2;

        if (w == 0) {
            xRadius = 0;
        } else {
            xRadius = 100 * std::min<double>(xRadius, w) / w;
        }
        if (h == 0) {
            yRadius = 0;
        } else {
            yRadius = 100 * std::min<double>(yRadius, h) / h;
        }
    } else {
        if (xRadius > 100){                          // fix ranges
            xRadius = 100;
        }

        if (yRadius > 100){
            yRadius = 100;
        }
    }
    return true;
}

void LPainterPath::addRoundedRect(const LRect<double> &r, double xRadius, double yRadius, LSizeMode mode)
{
    if(ajustRounedRectRadius(r, xRadius, yRadius, mode)){
        if (xRadius <= 0 || yRadius <= 0) {             // add normal rectangle
            addRect(r);
            return;
        }

        const double& x = r.m_x;
        const double& y = r.m_y;
        const double& w = r.m_width;
        const double& h = r.m_height;
        double rxx2 = w*xRadius/100;
        double ryy2 = h*yRadius/100;

        ensureData();
        resetMark();

        bool first = m_elements.size() < 2;

        arcMoveTo(x, y, rxx2, ryy2, 180);
        arcTo(x, y, rxx2, ryy2, 180, -90);
        arcTo(x+w-rxx2, y, rxx2, ryy2, 90, -90);
        arcTo(x+w-rxx2, y+h-ryy2, rxx2, ryy2, 0, -90);
        arcTo(x, y+h-ryy2, rxx2, ryy2, 270, -90);
        closeSubpath();

        m_requireMoveTo = true;
        m_convex = first;
    }
}

std::vector<LPoint<double> > LPainterPath::curvesForArc(const LRect<double> &rect,
                                                        double startAngle,
                                                        double sweepLength)
{
    if (std::isnan(rect.m_x) || std::isnan(rect.m_y) || std::isnan(rect.width()) || std::isnan(rect.height())
        || std::isnan(startAngle) || std::isnan(sweepLength)) {
        lWarn()<< "LPainterPath::arcTo: Adding arc where a parameter is NaN, results are undefined";
        return std::vector<LPoint<double> >();
    }

    if (rect.isNull()) {
        return std::vector<LPoint<double> >();
    }

    double x = rect.m_x;
    double y = rect.m_y;

    double w = rect.width();
    double w2 = rect.width() / 2;
    double w2k = w2 * LK_PATH_KAPPA;

    double h = rect.height();
    double h2 = rect.height() / 2;
    double h2k = h2 * LK_PATH_KAPPA;

    LPoint<double> points[16];
    points[0].m_x = x+w;
    points[0].m_y = y+h2;

    //0 -> 270 degrees
    points[1].m_x = x+w;
    points[1].m_y = y+h2+h2k;

    points[2].m_x = x+w2+w2k;
    points[2].m_y = y+h;

    points[3].m_x = x+w2;
    points[3].m_y = y+h;

    // 270 -> 180 degrees
    points[4].m_x = x+w2-w2k;
    points[4].m_y = y+h;

    points[5].m_x = x;
    points[5].m_y = y+h2+h2k;

    points[6].m_x = x;
    points[6].m_y = y+h2;

    // 180 -> 90 degrees
    points[7].m_x = x;
    points[7].m_y = y+h2-h2k;

    points[8].m_x = x+w2-w2k;
    points[8].m_y = y;

    points[9].m_x = x+w2;
    points[9].m_y = y;

    // 90 -> 0 degrees
    points[10].m_x = x+w2+w2k;
    points[10].m_y = y;

    points[11].m_x = x+w;
    points[11].m_y = y+h2-h2k;

    points[12].m_x = x+w;
    points[12].m_y = y+h2;


    if (sweepLength > 360){
        sweepLength = 360;
    }
    else if (sweepLength < -360){
        sweepLength = -360;
    }

    std::vector<LPoint<double> > curves;
    // Special case fast paths
    if (startAngle == 0.0) {
        if (sweepLength == 360.0) {
            for (int i = 12; i >= 0; --i){
                curves.push_back(points[i]);
            }
            return curves;
        } else if (sweepLength == -360.0) {
            for (int i = 0; i <= 12; ++i){
                curves.push_back(points[i]);
            }
            return curves;
        }
    }

    int startSegment = int(std::floor(startAngle / 90));
    int endSegment = int(std::floor((startAngle + sweepLength) / 90));

    double startT = (startAngle - startSegment * 90) / 90;
    double endT = (startAngle + sweepLength - endSegment * 90) / 90;

    int delta = sweepLength > 0 ? 1 : -1;
    if (delta < 0) {
        startT = 1 - startT;
        endT = 1 - endT;
    }

    // avoid empty start segment
    if (lFuzzyIsZero(startT - double(1))) {
        startT = 0;
        startSegment += delta;
    }

    // avoid empty end segment
    if (lFuzzyIsZero(endT)) {
        endT = 1;
        endSegment -= delta;
    }

    startT = tForArcAngle(startT * 90);
    endT = tForArcAngle(endT * 90);

    const bool splitAtStart = !lFuzzyIsZero(startT);
    const bool splitAtEnd = !lFuzzyIsZero(endT - double(1));

    const int end = endSegment + delta;

    // empty arc?
    if (startSegment == end) {
        const int quadrant = 3 - ((startSegment % 4) + 4) % 4;
        const int j = 3 * quadrant;

        if(delta > 0){
            curves.push_back(points[j + 3]);
        }else{
            curves.push_back(points[j]);
        }
        return curves;
    }

    LPoint<double> startPoint, endPoint;
    findEllipseCoords(rect, startAngle, sweepLength, startPoint, endPoint);
    curves.push_back(startPoint);

    for (int i = startSegment; i != end; i += delta) {
        const int quadrant = 3 - ((i % 4) + 4) % 4;
        const int j = 3 * quadrant;

        LBezier b;
        if (delta > 0){
            b = LBezier::fromPoints(points[j + 3], points[j + 2], points[j + 1], points[j]);
        }else{
            b = LBezier::fromPoints(points[j], points[j + 1], points[j + 2], points[j + 3]);
        }

        // empty arc?
        if (startSegment == endSegment && lFuzzyCompare(startT, endT)){
            return curves;
        }

        if (i == startSegment) {
            if (i == endSegment && splitAtEnd){
                b = b.bezierOnInterval(startT, endT);
            }
            else if (splitAtStart){
                b = b.bezierOnInterval(startT, 1);
            }
        } else if (i == endSegment && splitAtEnd) {
            b = b.bezierOnInterval(0, endT);
        }

        // push control points
        curves.push_back(b.pt2());
        curves.push_back(b.pt3());
        curves.push_back(b.pt4());
    }

    curves.push_back(endPoint);
    return curves;

}

void LPainterPath::findEllipseCoords(const LRect<double> &r,
                                     double angle,
                                     double length,
                                     LPoint<double> &startPoint,
                                     LPoint<double> &endPoint)
{
    if (r.isNull()) {
        startPoint.reset();
        endPoint.reset();
        return;
    }

    double w2 = r.width() / 2;
    double h2 = r.height() / 2;

    double angles[2] = { angle, angle + length };
    LPoint<double>* points[2] = { &startPoint, &endPoint };

    for (int i = 0; i < 2; ++i) {
        double theta = angles[i] - 360 * std::floor(angles[i] / 360);
        double t = theta / 90;
        // truncate
        int quadrant = int(t);
        t -= quadrant;

        t = tForArcAngle(90 * t);

        // swap x and y?
        if (quadrant & 1){
            t = 1 - t;
        }

        double a, b, c, d;
        LBezier::coefficients(t, a, b, c, d);
        LPoint<double> p(a + b + c*LK_PATH_KAPPA, d + c + b*LK_PATH_KAPPA);

        // left quadrants
        if (quadrant == 1 || quadrant == 2){
            p.m_x = -p.m_x;
        }

        // top quadrants
        if (quadrant == 0 || quadrant == 1){
            p.m_y = -p.m_y;
        }

        *points[i] = r.center() + LPoint<double>(w2 * p.m_x, h2 * p.m_y);
    }
}

void LPainterPath::moveTo(const LPoint<double> &p)
{
    if (!hasValidCoords(p)) {
        return;
    }

    ensureData();
    resetMark();

    m_requireMoveTo = false;

    if(m_isOutline){
        close();
    }

    //m_elements.back()
    Element& lastELe = m_elements.back();
    if (lastELe.m_type == MoveToElement) {
        lastELe.m_x = p.m_x;
        lastELe.m_y = p.m_y;
    } else {
        Element elm = { p.m_x, p.m_y, MoveToElement };
        m_elements.push_back(elm);
    }
    m_cStart = m_elements.size() - 1;

}

void LPainterPath::lineTo(const LPoint<double> &p)
{
    if (!hasValidCoords(p)) {
        return;
    }

    ensureData();
    resetMark();

    maybeMoveTo();
    if (p == m_elements.back().toPointF()){
        return;
    }
    Element elm = { p.m_x, p.m_y, LineToElement };
    m_elements.push_back(elm);
    m_convex = m_elements.size() == 3 || (m_elements.size() == 4 && isClosed());
    m_isRectInit = false;
}

void LPainterPath::quadTo(const LPoint<double> &c, const LPoint<double> &e)
{
    if (!hasValidCoords(c) || !hasValidCoords(e)) {
        return;
    }

    ensureData();
    resetMark();

    const LPainterPath::Element &elm = m_elements.at(elementCount()-1);
    LPoint<double> prev(elm.m_x, elm.m_y);

    // Abort on empty curve as a stroker cannot handle this and the
    // curve is irrelevant anyway.
    if (prev == c && c == e){
        return;
    }
    LPoint<double> c1((prev.m_x + 2*c.m_x) / 3, (prev.m_y + 2*c.m_y) / 3);
    LPoint<double> c2((e.m_x + 2*c.m_x) / 3, (e.m_y + 2*c.m_y) / 3);
    cubicTo(c1, c2, e);
}

void LPainterPath::arcMoveTo(const LRect<double> &rect, double angle)
{
    if (rect.isNull()){
        return;
    }

    LPoint<double> pt;
    findEllipseCoords(rect, angle, 0, &pt, nullptr);
    moveTo(pt);
}

void LPainterPath::arcMoveTo(double x, double y, double w, double h, double angle)
{
    arcMoveTo(LRect<double>(x, y, w, h, false), angle);
}

void LPainterPath::arcTo(const LRect<double> &rect, double startAngle, double sweepLength)
{

    if (!hasValidCoords(rect) || !isValidCoord(startAngle) || !isValidCoord(sweepLength)) {
        return;
    }

    if (rect.isNull()){
        return;
    }

    ensureData();
    resetMark();

    int pointCount;
    LPoint<double> pts[15];
    LPoint<double> curveStart = curvesForArc(rect, startAngle, sweepLength, pts, &pointCount);

    lineTo(curveStart);
    for (int i=0; i<pointCount; i+=3) {
        cubicTo(pts[i], pts[i+1], pts[i+2]);
    }
}

void LPainterPath::arcTo(double x, double y, double w, double h, double startAngle, double arcLength)
{
    arcTo(LRect<double>(x, y, w, h, false), startAngle, arcLength);
}

void LPainterPath::cubicTo(const LPoint<double> &c1, const LPoint<double> &c2, const LPoint<double> &e)
{
    if (!hasValidCoords(c1) || !hasValidCoords(c2) || !hasValidCoords(e)) {
        return;
    }

    ensureData();
    resetMark();

    // Abort on empty curve as a stroker cannot handle this and the
    // curve is irrelevant anyway.
    if (m_elements.back().toPointF() == c1 && c1 == c2 && c2 == e){
        return;
    }

    maybeMoveTo();
    if(m_disableCurve){
        const Element& ele = m_elements.back();
        LBezier bezier = LBezier::fromPoints(LPoint<double>(ele.m_x, ele.m_y), c1, c2, e);
        LPolygon<double> temp;
        bezier.addToPolygon(&temp, m_curveThreshold);
        for(const LPoint<double>& p : temp){
            Element t = { p.m_x, p.m_y, LineToElement };
            m_elements.push_back(t);
        }
    }else{
        Element ce1 = { c1.m_x, c1.m_y, CurveToElement };
        Element ce2 = { c2.m_x, c2.m_y, CurveToDataElement };
        Element ee = { e.m_x, e.m_y, CurveToDataElement };
        m_elements.push_back(ce1);
        m_elements.push_back(ce2);
        m_elements.push_back(ee);
    }
    m_isRectInit = false;
}

double LPainterPath::tForArcAngle(double angle)
{
    if (lFuzzyIsZero(angle)){
        return 0;
    }

    if (lFuzzyCompare(angle, double(90))){
        return 1;
    }

    double radians = angle * (M_PI / 180);
    double cosAngle = std::cos(radians);
    double sinAngle = std::sin(radians);

    // initial guess
    double tc = angle / 90;
    // do some iterations of newton's method to approximate cosAngle
    // finds the zero of the function b.pointAt(tc).x() - cosAngle
    tc -= ((((2-3*LK_PATH_KAPPA) * tc + 3*(LK_PATH_KAPPA-1)) * tc) * tc + 1 - cosAngle) // value
          / (((6-9*LK_PATH_KAPPA) * tc + 6*(LK_PATH_KAPPA-1)) * tc); // derivative
    tc -= ((((2-3*LK_PATH_KAPPA) * tc + 3*(LK_PATH_KAPPA-1)) * tc) * tc + 1 - cosAngle) // value
          / (((6-9*LK_PATH_KAPPA) * tc + 6*(LK_PATH_KAPPA-1)) * tc); // derivative

    // initial guess
    double ts = tc;
    // do some iterations of newton's method to approximate sinAngle
    // finds the zero of the function b.pointAt(tc).y() - sinAngle
    ts -= ((((3*LK_PATH_KAPPA-2) * ts -  6*LK_PATH_KAPPA + 3) * ts + 3*LK_PATH_KAPPA) * ts - sinAngle)
          / (((9*LK_PATH_KAPPA-6) * ts + 12*LK_PATH_KAPPA - 6) * ts + 3*LK_PATH_KAPPA);
    ts -= ((((3*LK_PATH_KAPPA-2) * ts -  6*LK_PATH_KAPPA + 3) * ts + 3*LK_PATH_KAPPA) * ts - sinAngle)
          / (((9*LK_PATH_KAPPA-6) * ts + 12*LK_PATH_KAPPA - 6) * ts + 3*LK_PATH_KAPPA);

    // use the average of the t that best approximates cosAngle
    // and the t that best approximates sinAngle
    double t = 0.5 * (tc + ts);

    return t;

}

void LPainterPath::ensureData()
{
    if(m_elements.empty()){
        m_elements.reserve(16);
        Element e = { 0, 0, MoveToElement };
        m_elements.push_back(e);
    }
}

void LPainterPath::resetMark()
{
    setDirty(true);
}


void LPainterPath::setDirty(bool dirty)
{
    m_dirtyBounds        = dirty;
    m_dirtyControlBounds = dirty;
    m_dirtyHints = dirty;
    m_convex = false;
    m_isRectInit = false;
}

void LPainterPath::setElementPositionAt(int i, double x, double y)
{
    resetMark();
    LPainterPath::Element &e = m_elements[i];
    e.m_x = x;
    e.m_y = y;
}

std::vector<LPainterPath::Element> LPainterPath::toElements(const std::vector<Element> &org, int& subStart, bool isOutline, bool disableCurve, double curveThreshold)
{
    std::vector<LPainterPath::Element> ret;
    subStart = 0;
    for(int index =0; index <org.size(); ++index){
        const Element& ele = org[index];
        if(ele.m_type == MoveToElement){
            if(isOutline && (!ret.empty())){
                doClose(ret, subStart);
            }

            subStart = (int)ret.size();
            ret.push_back(ele);
        }else if(ele.m_type == CurveToElement){
            if(index >0 && (index +2) < org.size() ){
                const Element& ele2 = org[index+1];
                const Element& ele3 = org[index+2];

                if(disableCurve){
                    const Element& ele0 = org[index-1];
                    LBezier bezier = LBezier::fromPoints(LPoint<double>(ele0.m_x, ele0.m_y),
                                                         LPoint<double>(ele.m_x, ele.m_y),
                                                         LPoint<double>(ele2.m_x, ele2.m_y),
                                                         LPoint<double>(ele3.m_x, ele3.m_y));
                    LPolygon<double> temp;
                    bezier.addToPolygon(&temp, curveThreshold);
                    for(const LPoint<double>& p : temp){
                        Element t = { p.m_x, p.m_y, LineToElement };
                        ret.push_back(t);
                    }
                }else{
                    ret.push_back(ele);
                    ret.push_back(ele2);
                    ret.push_back(ele3);
                }
            }
            index += 2;
        }else if(ele.m_type == LineToElement){
            ret.push_back(ele);
        }
    }

    if(isOutline && (!ret.empty())){
        doClose(ret, subStart);
    }
    return ret;
}

std::shared_ptr<LPainterPath> LPainterPath::toOutline()
{
    if(m_isOutline){
        return getPtrT<LPainterPath>();
    }
    return std::make_shared<LPainterPath>(*this, true, isDisableCurve(), m_curveThreshold);
}

std::list<std::shared_ptr<LPainterPath> > LPainterPath::toSubpathes()
{
    std::list<std::shared_ptr<LPainterPath> > subPathes;
    if (isEmpty()){
        return subPathes;
    }

    std::shared_ptr<LPainterPath> current;
    for (int i=0; i<(int)m_elements.size(); ++i) {
        const Element &e = m_elements.at(i);
        switch (e.m_type) {
        case MoveToElement:{
            if(current != nullptr){
                if(!current->isEmpty()){
                    subPathes.push_back(current);
                }
            }
            current = std::make_shared<LPainterPath>(nullptr, (bool)m_isOutline, isDisableCurve(), m_curveThreshold);
            current->moveTo( LPoint<double>(e.m_x, e.m_y));
            break;
        }
        case LineToElement:
            if(current != nullptr){
                current->lineTo(LPoint<double>(e.m_x, e.m_y));
            }
            break;
        case CurveToElement: {
            if(current != nullptr){
                if((i+2) < m_elements.size()){
                    current->cubicTo(LPoint<double>(e.m_x, e.m_y),
                                     LPoint<double>(m_elements.at(i+1).m_x, m_elements.at(i+1).m_y),
                                     LPoint<double>(m_elements.at(i+2).m_x, m_elements.at(i+2).m_y));
                }
            }
            i+=2;
            break;
        }
        case CurveToDataElement:
            break;
        }
    }

    if(current != nullptr){
        if (!current->isEmpty()){
            subPathes.push_back(current);
        }
    }
    return subPathes;
}

const uint32_t& LPainterPath::hints() const
{
    if(m_dirtyHints){
        const_cast<LPainterPath*>(this)->updateHints();
    }
    return m_hints;
}

bool LPainterPath::contains(const LPoint<double> &pt) const
{
    if (isEmpty() || !controlPointRect().contains(pt)){
        return false;
    }

    int windingNumber = 0;

    LPoint<double> lastPt;
    LPoint<double> lastStart;
    for (uint32_t i=0; i< m_elements.size(); ++i) {
        const Element &e = m_elements.at(i);

        switch (e.m_type) {            
        case MoveToElement:{
            if (i > 0){ // implicitly close all paths.
                LPolygon<double>::polygonIsectLine(lastPt, lastStart, pt, &windingNumber);
            }
            lastStart = lastPt = e.toPointF();
            break;
        }
        case LineToElement:{
            LPolygon<double>::polygonIsectLine(lastPt, e.toPointF(), pt, &windingNumber);
            lastPt = e.toPointF();
            break;
        }
        case CurveToElement:{
            const Element &cp2 = m_elements.at(++i);
            const Element &ep = m_elements.at(++i);
            lPainterPathIsectCurve(LBezier::fromPoints(lastPt, e.toPointF(), cp2.toPointF(), ep.toPointF()), pt, &windingNumber);
            lastPt = ep.toPointF();
            break;
        }
        default:{
            break;
        }
        }
    }

    // implicitly close last subpath
    if (lastPt != lastStart){
        LPolygon<double>::polygonIsectLine(lastPt, lastStart, pt, &windingNumber);
    }

    return (m_fillRule == LFillRule::Winding
                ? (windingNumber != 0)
                : ((windingNumber % 2) != 0));
}

bool LPainterPath::contains(const LRect<double> &rect) const
{
    // the path is empty or the control point rect doesn't completely
    // cover the rectangle we abort stratight away.
    if (isEmpty() || !controlPointRect().contains(rect)){
        return false;
    }

    // if there are intersections, chances are that the rect is not
    // contained, except if we have winding rule, in which case it
    // still might.
    std::shared_ptr<LPainterPath> tThis = getPtrT<LPainterPath>();
    if (lPainterPathCheckCrossing(tThis, rect)) {
        if (fillRule() == LFillRule::OddEven) {
            return false;
        } else {
            // Do some wague sampling in the winding case. This is not
            // precise but it should mostly be good enough.
            if (!contains(rect.topLeft()) ||
                !contains(rect.topRight()) ||
                !contains(rect.bottomRight()) ||
                !contains(rect.bottomLeft())){
                return false;
            }
        }
    }

    // If there exists a point inside that is not part of the path its
    // because: rectangle lies completely outside path or a subpath
    // excludes parts of the rectangle. Both cases mean that the rect
    // is not contained
    if (!contains(rect.center())){
        return false;
    }

    // If there are any subpaths inside this rectangle we need to
    // check if they are still contained as a result of the fill
    // rule. This can only be the case for WindingFill though. For
    // OddEvenFill the rect will never be contained if it surrounds a
    // subpath. (the case where two subpaths are completely identical
    // can be argued but we choose to neglect it).
    for (int i=0; i< (int)m_elements.size(); ++i) {
        const Element &e = m_elements.at(i);
        if (e.m_type == LPainterPath::MoveToElement && rect.contains(e.toPointF())) {
            if (fillRule() == LFillRule::OddEven)
                return false;

            bool stop = false;
            for (; !stop && i< (int)m_elements.size(); ++i) {
                const Element &el = m_elements.at(i);
                switch (el.m_type) {
                case MoveToElement:{
                    stop = true;
                    break;
                }
                case LineToElement:{
                    if (!contains(el.toPointF())){
                        return false;
                    }
                    break;
                }
                case CurveToElement:{
                    if (!contains(m_elements.at(i+2).toPointF())){
                        return false;
                    }
                    i += 2;
                    break;
                }
                default:
                    break;
                }
            }
            // compensate for the last ++i in the inner for
            --i;
        }
    }
    return true;
}

bool LPainterPath::intersects(const LRect<double> &rect) const
{
    if (elementCount() == 1 && rect.contains(elementAt(0).toPointF())){
        return true;
    }

    if (isEmpty()){
        return false;
    }

    LRect<double> cp = controlPointRect();
    LRect<double> rn = rect.normalized();

    // QRectF::intersects returns false if one of the rects is a null rect
    // which would happen for a painter path consisting of a vertical or
    // horizontal line
    if (std::max<double>(rn.m_x, cp.m_x) > std::min<double>(rn.x2(), cp.x2())
        || std::max<double>(rn.m_y, cp.m_y) > std::min<double>(rn.y2(), cp.y2())){
        return false;
    }

    // If any path element cross the rect its bound to be an intersection
    std::shared_ptr<LPainterPath> tThis = getPtrT<LPainterPath>();
    if (lPainterPathCheckCrossing(tThis, rect)){
        return true;
    }

    if (contains(rect.center())){
        return true;
    }

    // Check if the rectangle surounds any subpath...
    for (uint32_t i=0; i<m_elements.size(); ++i) {
        const Element &e = m_elements.at(i);
        if (e.m_type == LPainterPath::MoveToElement && rect.contains(e.toPointF())){
            return true;
        }
    }

    return false;
}

bool LPainterPath::intersects(const std::shared_ptr<LPainterPath> p) const
{
    if(p == nullptr){
        return false;
    }

    if (p->elementCount() == 1){
        return contains(p->elementAt(0).toPointF());
    }
    if (isEmpty() || p->isEmpty()){
        return false;
    }
    std::shared_ptr<LPainterPath> tThis = getPtrT<LPainterPath>();
    LPathClipper clipper(tThis, p);
    return clipper.intersect();
}

bool LPainterPath::contains(const std::shared_ptr<LPainterPath> p) const
{
    if(p == nullptr){
        return false;
    }

    if (p->elementCount() == 1){
        return contains(p->elementAt(0).toPointF());
    }
    if (isEmpty() || p->isEmpty()){
        return false;
    }
    std::shared_ptr<LPainterPath> tThis = getPtrT<LPainterPath>();
    LPathClipper clipper(tThis, p);
    return clipper.contains();
}

std::shared_ptr<LPainterPath> LPainterPath::simplified() const
{
    if(isEmpty()){
        return getPtrT<LPainterPath>();
    }
    std::shared_ptr<LPainterPath> tThis = getPtrT<LPainterPath>();
    std::shared_ptr<LPainterPath> t = std::make_shared<LPainterPath>();
    LPathClipper clipper(tThis, t);
    return clipper.clip(LPathClipper::Simplify);
}

std::shared_ptr<LPainterPath> LPainterPath::united(const std::shared_ptr<LPainterPath> p) const
{
    std::shared_ptr<LPainterPath> tThis = getPtrT<LPainterPath>();
    if(p == nullptr || p->isEmpty()){
        return tThis;
    }
    if(isEmpty()){
        return p;
    }
    LPathClipper clipper(tThis, p);
    return clipper.clip(LPathClipper::BoolOr);
}

std::shared_ptr<LPainterPath> LPainterPath::intersected(const std::shared_ptr<LPainterPath> p) const
{
    if (p== nullptr || isEmpty() || p->isEmpty()){
        return std::make_shared<LPainterPath>();
    }
    std::shared_ptr<LPainterPath> tThis = getPtrT<LPainterPath>();
    LPathClipper clipper(tThis, p);
    return clipper.clip(LPathClipper::BoolAnd);
}

std::shared_ptr<LPainterPath> LPainterPath::subtracted(const std::shared_ptr<LPainterPath> p) const
{
    std::shared_ptr<LPainterPath> tThis = getPtrT<LPainterPath>();
    if (p== nullptr || isEmpty() || p->isEmpty()){
        return tThis;
    }
    LPathClipper clipper(tThis, p);
    return clipper.clip(LPathClipper::BoolSub);
}

double LPainterPath::length() const
{
    if (isEmpty()){
        return 0;
    }

    double len = 0;
    for (int i=1; i<m_elements.size(); ++i) {
        const Element &e = m_elements[i];
        switch (e.m_type) {
        case MoveToElement:{
            break;
        }
        case LineToElement:
        {
            const Element &last = m_elements[i-1];
            len += hypot(last.m_x-e.m_x, last.m_y-e.m_y);
            break;
        }
        case CurveToElement:
        {
            if((i+2) < m_elements.size()){
                LBezier b = LBezier::fromPoints(m_elements[i-1].toPointF(),
                                                e.toPointF(),
                                                m_elements[i+1].toPointF(),
                                                m_elements[i+2].toPointF());
                len += b.length();
            }

            i += 2;
            break;
        }
        default:
            break;
        }
    }
    return len;
}

double LPainterPath::percentAtLength(double len) const
{
    if (isEmpty() || len <= 0){
        return 0;
    }

    double totalLength = length();
    if (len > totalLength){
        return 1;
    }

    double curLen = 0;
    for (int i=1; i<m_elements.size(); ++i) {
        const Element &e = m_elements[i];

        switch (e.m_type) {
        case MoveToElement:{
            break;
        }
        case LineToElement:
        {
            const Element &last = m_elements[i-1];
            curLen += hypot(last.m_x-e.m_x, last.m_y-e.m_y);;
            if (curLen >= len) {
                return len/totalLength ;
            }

            break;
        }
        case CurveToElement:
        {

            if((i+2) < m_elements.size()){
                LBezier b = LBezier::fromPoints(m_elements[i-1].toPointF(),
                                                e.toPointF(),
                                                m_elements[i+1].toPointF(),
                                                m_elements[i+2].toPointF());
                double blen = b.length();
                double prevLen = curLen;
                curLen += blen;

                if (curLen >= len) {
                    double res = b.tAtLength(len - prevLen);
                    return (res * blen + prevLen)/totalLength;
                }
            }
            i += 2;
            break;
        }
        default:
            break;
        }
    }

    return 0;
}

LPoint<double> LPainterPath::pointAtPercent(double t) const
{
    if ((t < 0) ||( t > 1)) {
        lWarn() << "LPainterPath::pointAtPercent accepts only values between 0 and 1";
        return LPoint<double>();
    }

    if ( m_elements.empty()){
        return LPoint<double>();
    }

    if (m_elements.size() == 1){
        return m_elements[0].toPointF();
    }

    double totalLength = length();
    double curLen = 0;
    double bezierLen = 0;
    LBezier b = lBezierAtT(*this, t, &curLen, &bezierLen);
    double realT = (totalLength * t - curLen) / bezierLen;

    return b.pointAt(lBound(0.0, realT, 1.0));
}

double LPainterPath::angleAtPercent(double t) const
{
    if ((t < 0) ||( t > 1)) {
        lWarn() << "LPainterPath::angleAtPercent accepts only values between 0 and 1";
        return 0;
    }

    if ( isEmpty()){
        return 0;
    }

    double totalLength = length();
    double curLen = 0;
    double bezierLen = 0;
    LBezier bez = lBezierAtT(*this, t, &curLen, &bezierLen);
    double realT = (totalLength * t - curLen) / bezierLen;

    double m1 = lSlopeAt(realT, bez.m_x1, bez.m_x2, bez.m_x3, bez.m_x4);
    double m2 = lSlopeAt(realT, bez.m_y1, bez.m_y2, bez.m_y3, bez.m_y4);

    return LLine<double>(0, 0, m1, m2).angle();
}

double LPainterPath::slopeAtPercent(double t) const
{
    if ((t < 0) ||( t > 1)) {
        lWarn() << "LPainterPath::slopeAtPercent accepts only values between 0 and 1";
        return 0;
    }

    double totalLength = length();
    double curLen = 0;
    double bezierLen = 0;
    LBezier bez = lBezierAtT(*this, t, &curLen, &bezierLen);
    double realT = (totalLength * t - curLen) / bezierLen;

    double m1 = lSlopeAt(realT, bez.m_x1, bez.m_x2, bez.m_x3, bez.m_x4);
    double m2 = lSlopeAt(realT, bez.m_y1, bez.m_y2, bez.m_y3, bez.m_y4);
    //tangent line
    double slope = 0;

    if (m1)
        slope = m2/m1;
    else {
        if (std::numeric_limits<double>::has_infinity) {
            slope = (m2  < 0) ? -std::numeric_limits<double>::infinity()
                              : std::numeric_limits<double>::infinity();
        } else {
            if (sizeof(double) == sizeof(double)) {
                return 1.79769313486231570e+308;
            } else {
                return ((double)3.40282346638528860e+38);
            }
        }
    }

    return slope;
}

void LPainterPath::findEllipseCoords(const LRect<double> &r, double angle, double length, LPoint<double> *startPoint, LPoint<double> *endPoint)
{
    if (r.isNull()) {
        if (startPoint){
            *startPoint = LPoint<double>();
        }
        if (endPoint){
            *endPoint = LPoint<double>();
        }
        return;
    }

    double w2 = r.width() / 2;
    double h2 = r.height() / 2;
    LPoint<double> c = r.center();

    double angles[2] = { angle, angle + length };
    LPoint<double> *points[2] = { startPoint, endPoint };

    for (int i = 0; i < 2; ++i) {
        if (!points[i]){
            continue;
        }
        *points[i] = findArcPos(c, w2, h2, angles[i]);
    }

}

LPoint<double> LPainterPath::findArcPos(const LPoint<double> &center, double xRadius, double yRadius, double angle)
{
    double theta = angle - 360 * std::floor(angle / 360);
    double t = theta / 90;
    // truncate
    int quadrant = int(t);
    t -= quadrant;

    t = forArcAngleT(90 * t);

    // swap x and y?
    if (quadrant & 1){
        t = 1 - t;
    }

    double a, b, c, d;
    LBezier::coefficients(t, a, b, c, d);
    LPoint<double> p(a + b + c*LK_PATH_KAPPA, d + c + b*LK_PATH_KAPPA);

    // left quadrants
    if (quadrant == 1 || quadrant == 2){
        p.m_x = -p.m_x;
    }

    // top quadrants
    if (quadrant == 0 || quadrant == 1){
        p.m_y = -p.m_y;
    }

    return center + LPoint<double>(xRadius * p.m_x, yRadius * p.m_y);
}

double LPainterPath::forArcAngleT(double angle)
{
    if (lFuzzyIsZero(angle)){
        return 0;
    }

    if (lFuzzyCompare(angle, double(90))){
        return 1;
    }

    double radians = lDegreesToRadians(angle);
    double cosAngle = std::cos(radians);
    double sinAngle = std::sin(radians);

    // initial guess
    double tc = angle / 90;
    // do some iterations of newton's method to approximate cosAngle
    // finds the zero of the function b.pointAt(tc).x() - cosAngle
    tc -= ((((2-3*LK_PATH_KAPPA) * tc + 3*(LK_PATH_KAPPA-1)) * tc) * tc + 1 - cosAngle) // value
          / (((6-9*LK_PATH_KAPPA) * tc + 6*(LK_PATH_KAPPA-1)) * tc); // derivative
    tc -= ((((2-3*LK_PATH_KAPPA) * tc + 3*(LK_PATH_KAPPA-1)) * tc) * tc + 1 - cosAngle) // value
          / (((6-9*LK_PATH_KAPPA) * tc + 6*(LK_PATH_KAPPA-1)) * tc); // derivative

    // initial guess
    double ts = tc;
    // do some iterations of newton's method to approximate sinAngle
    // finds the zero of the function b.pointAt(tc).y() - sinAngle
    ts -= ((((3*LK_PATH_KAPPA-2) * ts -  6*LK_PATH_KAPPA + 3) * ts + 3*LK_PATH_KAPPA) * ts - sinAngle)
          / (((9*LK_PATH_KAPPA-6) * ts + 12*LK_PATH_KAPPA - 6) * ts + 3*LK_PATH_KAPPA);
    ts -= ((((3*LK_PATH_KAPPA-2) * ts -  6*LK_PATH_KAPPA + 3) * ts + 3*LK_PATH_KAPPA) * ts - sinAngle)
          / (((9*LK_PATH_KAPPA-6) * ts + 12*LK_PATH_KAPPA - 6) * ts + 3*LK_PATH_KAPPA);

    // use the average of the t that best approximates cosAngle
    // and the t that best approximates sinAngle
    double t = 0.5 * (tc + ts);
    return t;
}

LPolygon<double> LPainterPath::united(const LPolygon<double> &p1, const LPolygon<double> &p2)
{
    std::shared_ptr<LPainterPath> subject = std::make_shared<LPainterPath>();
    subject->addPolygon(p1);
    std::shared_ptr<LPainterPath> clip = std::make_shared<LPainterPath>();
    clip->addPolygon(p2);

    return subject->united(clip)->toFillPolygon();
}

LPolygon<double> LPainterPath::intersected(const LPolygon<double> &p1, const LPolygon<double> &p2)
{
    std::shared_ptr<LPainterPath> subject = std::make_shared<LPainterPath>();
    subject->addPolygon(p1);
    std::shared_ptr<LPainterPath> clip = std::make_shared<LPainterPath>();
    clip->addPolygon(p2);

    return subject->intersected(clip)->toFillPolygon();
}

LPolygon<double> LPainterPath::subtracted(const LPolygon<double> &p1, const LPolygon<double> &p2)
{
    std::shared_ptr<LPainterPath> subject = std::make_shared<LPainterPath>();
    subject->addPolygon(p1);
    std::shared_ptr<LPainterPath> clip = std::make_shared<LPainterPath>();
    clip->addPolygon(p2);

    return subject->subtracted(clip)->toFillPolygon();
}

LPoint<double> LPainterPath::curvesForArc(const LRect<double> &rect, double startAngle, double sweepLength, LPoint<double> *curves, int *point_count)
{
    *point_count = 0;
    if (std::isnan(rect.m_x) || std::isnan(rect.m_y) || std::isnan(rect.width()) || std::isnan(rect.height())
        || std::isnan(startAngle) || std::isnan(sweepLength)) {
        lWarn()<< "LPainterPath::arcTo: Adding arc where a parameter is NaN, results are undefined";
        return LPoint<double>();
    }

    if (rect.isNull()) {
        return LPoint<double>();
    }

    double x = rect.m_x;
    double y = rect.m_y;

    double w = rect.width();
    double w2 = rect.width() / 2;
    double w2k = w2 * LK_PATH_KAPPA;

    double h = rect.height();
    double h2 = rect.height() / 2;
    double h2k = h2 * LK_PATH_KAPPA;

    LPoint<double> points[16] =
        {
            // start point
            LPoint<double>(x + w, y + h2),

            // 0 -> 270 degrees
            LPoint<double>(x + w, y + h2 + h2k),
            LPoint<double>(x + w2 + w2k, y + h),
            LPoint<double>(x + w2, y + h),

            // 270 -> 180 degrees
            LPoint<double>(x + w2 - w2k, y + h),
            LPoint<double>(x, y + h2 + h2k),
            LPoint<double>(x, y + h2),

            // 180 -> 90 degrees
            LPoint<double>(x, y + h2 - h2k),
            LPoint<double>(x + w2 - w2k, y),
            LPoint<double>(x + w2, y),

            // 90 -> 0 degrees
            LPoint<double>(x + w2 + w2k, y),
            LPoint<double>(x + w, y + h2 - h2k),
            LPoint<double>(x + w, y + h2)
        };

    if (sweepLength > 360) sweepLength = 360;
    else if (sweepLength < -360) sweepLength = -360;

    // Special case fast paths
    if (startAngle == 0.0) {
        if (sweepLength == 360.0) {
            for (int i = 11; i >= 0; --i){
                curves[(*point_count)++] = points[i];
            }
            return points[12];
        } else if (sweepLength == -360.0) {
            for (int i = 1; i <= 12; ++i){
                curves[(*point_count)++] = points[i];
            }
            return points[0];
        }
    }

    int startSegment = int(std::floor(startAngle / 90));
    int endSegment = int(std::floor((startAngle + sweepLength) / 90));

    double startT = (startAngle - startSegment * 90) / 90;
    double endT = (startAngle + sweepLength - endSegment * 90) / 90;

    int delta = sweepLength > 0 ? 1 : -1;
    if (delta < 0) {
        startT = 1 - startT;
        endT = 1 - endT;
    }

    // avoid empty start segment
    if (lFuzzyIsZero(startT - double(1))) {
        startT = 0;
        startSegment += delta;
    }

    // avoid empty end segment
    if (lFuzzyIsZero(endT)) {
        endT = 1;
        endSegment -= delta;
    }

    startT = forArcAngleT(startT * 90);
    endT = forArcAngleT(endT * 90);

    const bool splitAtStart = !lFuzzyIsZero(startT);
    const bool splitAtEnd = !lFuzzyIsZero(endT - double(1));

    const int end = endSegment + delta;

    // empty arc?
    if (startSegment == end) {
        const int quadrant = 3 - ((startSegment % 4) + 4) % 4;
        const int j = 3 * quadrant;
        return delta > 0 ? points[j + 3] : points[j];
    }

    LPoint<double> startPoint, endPoint;
    findEllipseCoords(rect, startAngle, sweepLength, &startPoint, &endPoint);

    for (int i = startSegment; i != end; i += delta) {
        const int quadrant = 3 - ((i % 4) + 4) % 4;
        const int j = 3 * quadrant;

        LBezier b;
        if (delta > 0){
            b = LBezier::fromPoints(points[j + 3], points[j + 2], points[j + 1], points[j]);
        }else{
            b = LBezier::fromPoints(points[j], points[j + 1], points[j + 2], points[j + 3]);
        }

        // empty arc?
        if (startSegment == endSegment && lFuzzyCompare(startT, endT)){
            return startPoint;
        }

        if (i == startSegment) {
            if (i == endSegment && splitAtEnd){
                b = b.bezierOnInterval(startT, endT);
            }
            else if (splitAtStart){
                b = b.bezierOnInterval(startT, 1);
            }
        } else if (i == endSegment && splitAtEnd) {
            b = b.bezierOnInterval(0, endT);
        }

        // push control points
        curves[(*point_count)++] = b.pt2();
        curves[(*point_count)++] = b.pt3();
        curves[(*point_count)++] = b.pt4();
    }

    curves[*(point_count)-1] = endPoint;
    return startPoint;
}

void LPainterPath::computeBoundingRect()
{
    m_dirtyBounds = false;
    if (isEmpty()) {
        m_bounds = LRect<double>();
        return;
    }

    double minx, maxx, miny, maxy;
    minx = maxx = m_elements.at(0).m_x;
    miny = maxy = m_elements.at(0).m_y;
    for (uint32_t i=1; i<m_elements.size(); ++i) {
        const Element &e = m_elements.at(i);

        switch (e.m_type) {
        case MoveToElement:
        case LineToElement:{
            if (e.m_x > maxx){
                maxx = e.m_x;
            }
            else if (e.m_x < minx){
                minx = e.m_x;
            }

            if (e.m_y > maxy){
                maxy = e.m_y;
            }
            else if (e.m_y < miny){
                miny = e.m_y;
            }
            break;
        }
        case CurveToElement:{
            LBezier b = LBezier::fromPoints(m_elements.at(i-1).toPointF(),
                                            e.toPointF(),
                                            m_elements.at(i+1).toPointF(),
                                            m_elements.at(i+2).toPointF());
            LRect<double> r = painterPathBezierExtrema(b);
            double right = r.x2();
            double bottom = r.y2();
            if (r.m_x < minx){
                minx = r.m_x;
            }
            if (right > maxx){
                maxx = right;
            }
            if (r.m_y < miny){
                miny = r.m_y;
            }
            if (bottom > maxy){
                maxy = bottom;
            }
            i += 2;
            break;
        }
        default:{
            break;
        }
        }
    }
    m_bounds = LRect<double>(minx, miny, maxx, maxy, true);
}

void LPainterPath::computeControlPointRect()
{
    m_dirtyControlBounds = false;
    if (isEmpty()) {
        m_controlBounds = LRect<double>();
        return;
    }

    double minx, maxx, miny, maxy;
    minx = maxx = m_elements.at(0).m_x;
    miny = maxy = m_elements.at(0).m_y;
    for (uint32_t i=1; i<m_elements.size(); ++i) {
        const Element &e = m_elements.at(i);
        if (e.m_x > maxx){
            maxx = e.m_x;
        }
        else if (e.m_x < minx){
            minx = e.m_x;
        }
        if (e.m_y > maxy){
            maxy = e.m_y;
        }
        else if (e.m_y < miny){
            miny = e.m_y;
        }
    }
    m_controlBounds = LRect<double>(minx, miny, maxx, maxy, true);
}

bool LPainterPath::checkRectShape()
{
    if(m_elements.size() != 5 && m_elements.size() != 4 ){
        return false;
    }else{
        if(m_isRectInit){
            return true;
        }

        if(m_elements[0].m_type != MoveToElement){
            return false;
        }

        if(m_elements[1].m_type != LineToElement){
            return false;
        }

        if(m_elements[2].m_type != LineToElement){
            return false;
        }

        if(m_elements[3].m_type != LineToElement){
            return false;
        }

        if(m_elements.size() == 5){
            return  m_elements[0].m_x == m_elements[4].m_x
                   && m_elements[0].m_y == m_elements[4].m_y // last point == first point
                   && m_elements[0].m_x == m_elements[3].m_x
                   && m_elements[1].m_x == m_elements[2].m_x // x values equal
                   && m_elements[0].m_y == m_elements[1].m_y
                   && m_elements[2].m_y == m_elements[3].m_y // y values equal...
                   && m_elements[0].m_x < m_elements[1].m_x && m_elements[0].m_y < m_elements[2].m_y;
        }else{
            return m_elements[0].m_x == m_elements[3].m_x
                   && m_elements[1].m_x == m_elements[2].m_x // x values equal
                   && m_elements[0].m_y == m_elements[1].m_y
                   && m_elements[2].m_y == m_elements[3].m_y // y values equal...
                   && m_elements[0].m_x < m_elements[1].m_x && m_elements[0].m_y < m_elements[2].m_y;
        }
    }
}

void LPainterPath::updateHints()
{
    m_hints = 0;

    bool isLines = true;
    for (uint32_t i=0; i<m_elements.size(); ++i) {
        const LPainterPath::Element &e = m_elements.at(i);
        if (e.m_type == LPainterPath::CurveToElement){
            m_hints |= LPainterPath::CurvedShapeMask;
        }

        // This is to check if the path contains only alternating lineTo/moveTo,
        // in which case we can set the LinesHint in the path. MoveTo is 0 and
        // LineTo is 1 so the i%2 gets us what we want cheaply.
        isLines = isLines && e.m_type == (LPainterPath::ElementType) (i%2);
    }

    if (isLines){
        m_hints |= LPainterPath::LinesShapeMask;
    }else {
        m_hints |= LPainterPath::AreaShapeMask;
        if (!m_convex){
            m_hints |= LPainterPath::NonConvexShapeMask;
        }
    }

    if(checkRectShape()){
        m_hints |= LPainterPath::RectangleShapeMask;
    }
    m_dirtyHints = false;

}


LPainterPath &LPainterPath::operator=(const LPainterPath &other){
    m_elements = other.m_elements;
    m_hints = other.m_hints;
    m_cStart = other.m_cStart;
    m_curveThreshold = other.m_curveThreshold;
    m_bounds = other.m_bounds;
    m_controlBounds = other.m_controlBounds;
    m_flags = other.m_flags;
    m_fillRule = other.m_fillRule;
    return *this;
}

LPainterPath::~LPainterPath()
{

}

bool LPainterPath::isEmpty() const
{
    if(m_elements.empty()){
        return true;
    }
    return (m_elements.size() == 1 && m_elements.front().m_type == MoveToElement);
}

LK_NAMESPACE_END
