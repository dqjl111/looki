#include "lstroker.h"
#include "lkcore/ldebug.h"
#include "lkgui/paint/lbezier.h"
#include "lkgui/paint/lpen.h"

LK_NAMESPACE_BEGIN

static inline bool lineRectIntersectsRect(LPoint<double> p1, LPoint<double> p2, const LPoint<double> &tl, const LPoint<double> &br)
{
    return ((p1.m_x > tl.m_x || p2.m_x > tl.m_x) && (p1.m_x < br.m_x || p2.m_x < br.m_x)
            && (p1.m_y > tl.m_y || p2.m_y > tl.m_y) && (p1.m_y < br.m_y || p2.m_y < br.m_y));
}

// If the line intersects the rectangle, this function will return true.
static bool lineIntersectsRect(LPoint<double> p1, LPoint<double> p2, const LPoint<double> &tl, const LPoint<double> &br)
{
    if (!lineRectIntersectsRect(p1, p2, tl, br)){
        return false;
    }
    if (p1.m_x == p2.m_x || p1.m_y == p2.m_y){
        return true;
    }

    if (p1.m_y > p2.m_y){
        std::swap(p1, p2); // make p1 above p2
    }
    LPoint<double> u;
    LPoint<double> v;
    LPoint<double> w = {p2.m_x - p1.m_x, p2.m_y - p1.m_y};
    if (p1.m_x < p2.m_x) {
        // backslash
        u.m_x = tl.m_x - p1.m_x; u.m_y = br.m_y - p1.m_y;
        v.m_x = br.m_x - p1.m_x; v.m_y = tl.m_y - p1.m_y;
    } else {
        // slash
        u.m_x = tl.m_x - p1.m_x; u.m_y = tl.m_y - p1.m_y;
        v.m_x = br.m_x - p1.m_x; v.m_y = br.m_y - p1.m_y;
    }

    double val1 = u.m_x * w.m_y - u.m_y * w.m_x;
    double val2 = v.m_x * w.m_y - v.m_y * w.m_x;
    return ((val1 < 0) && (val2 > 0)) || ((val1 > 0) && (val2 < 0));
}



class LSubpathForwardIterator
{
public:
    LSubpathForwardIterator(const std::vector<LPainterPath::Element> *path)
        : m_path(path), m_pos(0) { }
    inline int position() const { return m_pos; }
    inline bool hasNext() const { return m_pos < (int)m_path->size(); }
    inline LPainterPath::Element next() { return m_path->at(m_pos++); }
private:
    const std::vector<LPainterPath::Element> *m_path;
    int m_pos;
};

class LSubpathBackwardIterator
{
public:
    LSubpathBackwardIterator(const std::vector<LPainterPath::Element> *path)
        : m_path(path), m_pos(path->size() - 1) { }

    inline int position() const { return m_pos; }

    inline bool hasNext() const { return m_pos >= 0; }

    inline LPainterPath::Element next()
    {
        LPainterPath::Element ce = m_path->at(m_pos);   // current element

        if (m_pos == m_path->size() - 1) {
            --m_pos;
            ce.m_type = LPainterPath::MoveToElement;
            return ce;
        }

        const LPainterPath::Element &pe = m_path->at(m_pos + 1); // previous element

        switch (pe.m_type) {
        case LPainterPath::LineToElement:{
            ce.m_type = LPainterPath::LineToElement;
            break;
        }
        case LPainterPath::CurveToDataElement:{
            // First control point?
            if (ce.m_type == LPainterPath::CurveToElement) {
                ce.m_type = LPainterPath::CurveToDataElement;
            } else { // Second control point then
                ce.m_type = LPainterPath::CurveToElement;
            }
            break;
        }
        case LPainterPath::CurveToElement:{
            ce.m_type = LPainterPath::CurveToDataElement;
            break;
        }
        default:{
            lWarn() << "LSubpathReverseIterator::next: Case unhandled";
            break;
        }
        }
        --m_pos;

        return ce;
    }

private:
    const std::vector<LPainterPath::Element> *m_path;
    int m_pos;
};



class LSubpathFlatIterator
{
public:
    LSubpathFlatIterator(const std::vector<LPainterPath::Element> *path, double threshold)
        : m_path(path), m_pos(0), m_curveIndex(-1), m_curve_threshold(threshold) { }

    inline bool hasNext() const { return m_curveIndex >= 0 || m_pos < m_path->size(); }

    LPainterPath::Element next()
    {
        if (m_curveIndex >= 0) {
            LPainterPath::Element e;
            e.m_type = LPainterPath::LineToElement;
            e.m_x = m_curve.at(m_curveIndex).m_x;
            e.m_y = m_curve.at(m_curveIndex).m_y;
            ++m_curveIndex;
            if (m_curveIndex >= m_curve.size()){
                m_curveIndex = -1;
            }
            return e;
        }


        LPainterPath::Element e = m_path->at(m_pos);
        if (e.isCurveTo()) {
            if(m_pos >0 && (m_pos +2) < m_path->size() ){
                const LPainterPath::Element& ele2 = m_path->at(m_pos+1);
                const LPainterPath::Element& ele3 = m_path->at(m_pos+2);


                const LPainterPath::Element& ele0 = m_path->at(m_pos-1);
                m_curve = LBezier::fromPoints(LPoint<double>(ele0.m_x, ele0.m_y),
                                                     LPoint<double>(e.m_x, e.m_y),
                                                     LPoint<double>(ele2.m_x, ele2.m_y),
                                                     LPoint<double>(ele3.m_x, ele3.m_y)).toPolygon(m_curve_threshold);

                if(!m_curve.empty()){
                    m_curveIndex = 1;
                    e.m_type = LPainterPath::LineToElement;
                    e.m_x = m_curve.at(0).m_x;
                    e.m_y = m_curve.at(0).m_y;
                }
            }
            m_pos += 2;
        }
        ++m_pos;
        return e;
    }

private:
    const std::vector<LPainterPath::Element> *m_path;
    int m_pos;
    LPolygon<double> m_curve;
    int m_curveIndex;
    double m_curve_threshold;
};


/////////////
LIStrokeHooker::LIStrokeHooker()
{

}

LIStrokeHooker::~LIStrokeHooker()
{

}

LStrokeHooker::LStrokeHooker(bool isFast):m_isFast(isFast)  {
    m_strokedPath = std::make_shared<LPainterPath>();
}

void LStrokeHooker::moveTo(double x, double y)
{
    if(m_isFast){
        m_strokedPath->m_elements.push_back(LPainterPath::Element{ x, y, LPainterPath::MoveToElement });
    }else{
        m_strokedPath->moveTo(x, y);
    }
}

void LStrokeHooker::lineTo(double x, double y)
{
    if(m_isFast){
        m_strokedPath->m_elements.push_back(LPainterPath::Element{ x, y, LPainterPath::LineToElement });
    }else{
        m_strokedPath->lineTo(x, y);
    }
}

void LStrokeHooker::cubicTo(double c1x, double c1y,
                            double c2x, double c2y,
                            double ex, double ey)
{
    if(m_isFast){
        m_strokedPath->m_elements.push_back(LPainterPath::Element{c1x, c1y, LPainterPath::CurveToElement});
        m_strokedPath->m_elements.push_back(LPainterPath::Element{c2x, c2y, LPainterPath::CurveToDataElement});
        m_strokedPath->m_elements.push_back(LPainterPath::Element{ex, ey, LPainterPath::CurveToDataElement});
    }else{
        m_strokedPath->cubicTo(LPoint<double>(c1x, c1y), LPoint<double>(c2x, c2y), LPoint<double>(ex, ey));
    }
}

void LStrokeHooker::addElement(const LPainterPath::Element &ele)
{
    m_strokedPath->m_elements.push_back(ele);
}

////
LStrokerOps::LStrokerOps()
    : m_elements(0)
    , m_curveThreshold(0.25)
    , m_dashThreshold(0.25)
    , m_hooker(nullptr)
{

}

LStrokerOps::~LStrokerOps()
{

}

void LStrokerOps::begin(std::shared_ptr<LIStrokeHooker> hooker)
{
    m_hooker = hooker;
    m_elements.clear();
}

void LStrokerOps::end()
{
    if (m_elements.size() > 1){
        processCurrentSubpath();
    }
}

void LStrokerOps::setCurveThresholdFromTransform(const LTransform2D &transform)
{
    double scale;
    LTransform2D::scaleForTransform(transform, &scale);
    m_dashThreshold = scale == 0 ? double(0.5) : (double(0.5) / scale);
}

uint32_t LStrokerOps::strokePath(const std::shared_ptr<LPainterPath> path,
                             std::shared_ptr<LStrokeHooker> hooker,
                             const LTransform2D &matrix)
{
    if (path->isEmpty()){
        return 0;
    }

    uint32_t flags = path->hints();

    setCurveThresholdFromTransform(LTransform2D());
    begin(hooker);
    const std::vector<LPainterPath::Element>& eles = path->elements();
    if (matrix.isIdentity()) {
        for(uint32_t i=0; i<eles.size(); ) {
            const LPainterPath::Element& ele = eles[i];
            switch (ele.m_type) {
            case LPainterPath::MoveToElement:{
                moveTo(ele.m_x, ele.m_y);
                ++i;
                break;
            }
            case LPainterPath::LineToElement:{
                lineTo(ele.m_x, ele.m_y);
                ++i;
                break;
            }
            case LPainterPath::CurveToElement:{
                if(i+2 <eles.size()){
                    const LPainterPath::Element& ele1 = eles[i+1];
                    const LPainterPath::Element& ele2 = eles[i+2];

                    cubicTo(ele.m_x, ele.m_y, ele1.m_x, ele1.m_y, ele2.m_x, ele2.m_y);
                    flags |= LPainterPath::CurvedShapeMask;
                }
                i+=3;
                break;
            }
            default:{
                lWarn() << "Unexpect paint path element!";
                ++i;
                break;
            }
            }
        }
    } else {

        for(uint32_t i=0; i<eles.size(); ) {
            const LPainterPath::Element& ele = eles[i];
            switch (ele.m_type) {
            case LPainterPath::MoveToElement:{
                double x, y;
                matrix.map(ele.m_x, ele.m_y, &x, &y);
                moveTo(x, y);
                ++i;
                break;
            }
            case LPainterPath::LineToElement:{
                double x, y;
                matrix.map(ele.m_x, ele.m_y, &x, &y);
                lineTo(x, y);
                ++i;
                break;
            }
            case LPainterPath::CurveToElement:
                if(i+2 <eles.size()){
                    double x, y, x1, y1, x2, y2;

                    const LPainterPath::Element& ele1 = eles[i+1];
                    const LPainterPath::Element& ele2 = eles[i+2];

                    matrix.map(ele.m_x, ele.m_y, &x, &y);
                    matrix.map(ele1.m_x, ele1.m_y, &x1, &y1);
                    matrix.map(ele2.m_x, ele2.m_y, &x2, &y2);

                    cubicTo(x, y, x1, y1, x2, y2);
                    flags |= LPainterPath::CurvedShapeMask;
                }
                i+=3;
                break;
            default:{
                lWarn() << "Unexpect paint path element!";
                ++i;
                break;
            }
            }
        }
    }
    end();
    return flags;
}

LStroker::LStroker()
{

}

LPenCapStyle LStroker::capForJoinMode(LineJoinMode mode)
{
    if (mode == FlatJoin){
        return LPenCapStyle::Flat;
    }
    else if (mode == SquareJoin){
        return LPenCapStyle::Square;
    }
    else{
        return LPenCapStyle::Round;
    }
}

LStroker::LineJoinMode LStroker::joinModeForCap(LPenCapStyle style)
{
    if (style == LPenCapStyle::Flat){
        return FlatJoin;
    }
    else if (style == LPenCapStyle::Square){
        return SquareJoin;
    }
    else{
        return RoundCap;
    }
}

LPenJoinStyle LStroker::joinForJoinMode(LineJoinMode mode)
{
    if (mode == FlatJoin){
        return LPenJoinStyle::Bevel;
    }
    else if (mode == MiterJoin){
        return LPenJoinStyle::Miter;
    }
    else if (mode == SvgMiterJoin){
        return LPenJoinStyle::SvgMiter;
    }
    else{
        return LPenJoinStyle::Round;
    }
}

LStroker::LineJoinMode LStroker::joinModeForJoin(LPenJoinStyle joinStyle)
{
    if (joinStyle == LPenJoinStyle::Bevel){
        return FlatJoin;
    }
    else if (joinStyle == LPenJoinStyle::Miter){
        return MiterJoin;
    }
    else if (joinStyle == LPenJoinStyle::SvgMiter){
        return SvgMiterJoin;
    }
    else{
        return RoundJoin;
    }
}

void LStroker::joinPoints(double focalX, double focalY, const LLine<double> &nextLine, LineJoinMode join)
{
    if (lFuzzyCompare(m_back1X, nextLine.m_pt1.m_x) && lFuzzyCompare(m_back1Y, nextLine.m_pt1.m_y)){
        return;
    }

    LLine<double> prevLine(m_back2X, m_back2Y, m_back1X, m_back1Y);
    LPoint<double> isect;
    LLineIntersectType type = prevLine.intersects(nextLine, &isect);

    if (join == FlatJoin) {
        LLine<double> shortCut(prevLine.m_pt2, nextLine.m_pt1);
        double angle = shortCut.angleTo(prevLine);
        if (type == LLineIntersectType::Bounded || (angle > 90 && !lFuzzyCompare(angle, (double)90))) {
            emitLineTo(focalX, focalY);
            emitLineTo(nextLine.m_pt1.m_x, nextLine.m_pt1.m_y);
            return;
        }
        emitLineTo(nextLine.m_pt1.m_x, nextLine.m_pt1.m_y);
    } else {
        if (join == MiterJoin) {
            double appliedMiterLimit = m_strokeWidth * m_miterLimit;

            // If we are on the inside, do the short cut...
            LLine<double> shortCut(prevLine.m_pt2, nextLine.m_pt1);
            double angle = shortCut.angleTo(prevLine);
            if (type == LLineIntersectType::Bounded || (angle > 90 && !lFuzzyCompare(angle, (double)90))) {
                emitLineTo(focalX, focalY);
                emitLineTo(nextLine.m_pt1.m_x, nextLine.m_pt1.m_y);
                return;
            }
            LLine<double> miterLine(LPoint<double>(m_back1X, m_back1Y), isect);
            if (type == LLineIntersectType::No || miterLine.length() > appliedMiterLimit) {
                LLine<double> l1(prevLine);
                l1.setLength(appliedMiterLimit);
                l1.translate(prevLine.dx(), prevLine.dy());

                LLine<double> l2(nextLine);
                l2.setLength(appliedMiterLimit);
                l2.translate(-l2.dx(), -l2.dy());

                emitLineTo(l1.m_pt2.m_x, l1.m_pt2.m_y);
                emitLineTo(l2.m_pt1.m_x, l2.m_pt1.m_y);
                emitLineTo(nextLine.m_pt1.m_x, nextLine.m_pt1.m_y);
            } else {
                emitLineTo(isect.m_x, isect.m_y);
                emitLineTo(nextLine.m_pt1.m_x, nextLine.m_pt1.m_y);
            }

        } else if (join == SquareJoin) {
            double offset = m_strokeWidth / 2;

            LLine<double> l1(prevLine);
            double dp = LPoint<double>::dotProduct(LPoint<double>(prevLine.dx(), prevLine.dy()), LPoint<double>(nextLine.dx(), nextLine.dy()));
            if (dp > 0){  // same direction, means that prevLine is from a bezier that has been "reversed" by shifting
                l1 = LLine<double>(prevLine.m_pt2, prevLine.m_pt1);
            }else{
                l1.translate(l1.dx(), l1.dy());
            }
            l1.setLength((offset));
            LLine<double> l2(nextLine.m_pt2, nextLine.m_pt1);
            l2.translate(l2.dx(), l2.dy());
            l2.setLength((offset));
            emitLineTo(l1.m_pt2.m_x, l1.m_pt2.m_y);
            emitLineTo(l2.m_pt2.m_x, l2.m_pt2.m_y);
            emitLineTo(l2.m_pt1.m_x, l2.m_pt1.m_y);

        } else if (join == RoundJoin) {
            double offset = m_strokeWidth / 2;

            LLine<double> shortCut(prevLine.m_pt2, nextLine.m_pt1);
            double angle = shortCut.angleTo(prevLine);
            if ((type == LLineIntersectType::Bounded || (angle > double(90.01))) && nextLine.length() > offset) {
                emitLineTo(focalX, focalY);
                emitLineTo(nextLine.m_pt1.m_x, nextLine.m_pt1.m_y);
                return;
            }
            double l1OnX = LLine<double>(0, 0, 1, 0).angleTo(prevLine);
            double l2OnX = LLine<double>(0, 0, 1, 0).angleTo(nextLine);

            double sweepLength = std::abs(l2OnX - l1OnX);

            int pointCount;
            LPoint<double> curves[15];

            LPainterPath::curvesForArc(LRect<double>((focalX - offset),
                                                     (focalY - offset),
                                                     (offset * 2),
                                                     (offset * 2),
                                                     false),
                                          l1OnX + 90, -sweepLength,
                                          curves, &pointCount);

            for (int i=0; i<pointCount; i+=3) {
                emitCubicTo(curves[i].m_x,
                            curves[i].m_y,
                            curves[i+1].m_x,
                            curves[i+1].m_y,
                            curves[i+2].m_x,
                            curves[i+2].m_y);
            }

            // line to the end of the arc segment, (should also not be needed).
            emitLineTo(nextLine.m_pt1.m_x, nextLine.m_pt1.m_y);

            // Same as round join except we know its 180 degrees. Can also optimize this
            // later based on the addEllipse logic
        } else if (join == RoundCap) {
            double offset = m_strokeWidth / 2;

            // first control line
            LLine<double> l1 = prevLine;
            double dp = LPoint<double>::dotProduct(LPoint<double>(prevLine.dx(), prevLine.dy()), LPoint<double>(nextLine.dx(), nextLine.dy()));
            if (dp > 0){  // same direction, means that prevLine is from a bezier that has been "reversed" by shifting
                l1 = LLine<double>(prevLine.m_pt2, prevLine.m_pt1);
            }else{
                l1.translate(l1.dx(), l1.dy());
            }
            l1.setLength(LK_PATH_KAPPA * offset);

            // second control line, find through normal between prevLine and focal.
            LLine<double> l2(focalX, focalY, prevLine.m_pt2.m_x, prevLine.m_pt2.m_y);
            l2.translate(-l2.dy(), l2.dx());
            l2.setLength(LK_PATH_KAPPA * offset);

            emitCubicTo(l1.m_pt2.m_x,
                        l1.m_pt2.m_y,
                        l2.m_pt2.m_x,
                        l2.m_pt2.m_y,
                        l2.m_pt1.m_x,
                        l2.m_pt1.m_y);

            // move so that it matches
            l2 = LLine<double>(l2.m_pt1.m_x, l2.m_pt1.m_y, l2.m_pt1.m_x-l2.dx(), l2.m_pt1.m_y-l2.dy());

            // last line is parallel to l1 so just shift it down.
            l1.translate(nextLine.m_pt1.m_x - l1.m_pt1.m_x, nextLine.m_pt1.m_y - l1.m_pt1.m_y);

            emitCubicTo(l2.m_pt2.m_x,
                        l2.m_pt2.m_y,
                        l1.m_pt2.m_x,
                        l1.m_pt2.m_y,
                        l1.m_pt1.m_x,
                        l1.m_pt1.m_y);
        } else if (join == SvgMiterJoin) {
            LLine<double> shortCut(prevLine.m_pt2, nextLine.m_pt1);
            double angle = shortCut.angleTo(prevLine);
            if (type == LLineIntersectType::Bounded || (angle > 90 && !lFuzzyCompare(angle, (double)90))) {
                emitLineTo(focalX, focalY);
                emitLineTo(nextLine.m_pt1.m_x, nextLine.m_pt1.m_y);
                return;
            }
            LLine<double> miterLine(LPoint<double>((focalX),
                                     (focalY)), isect);
            if (type == LLineIntersectType::No || miterLine.length() > (m_strokeWidth * m_miterLimit) / 2) {
                emitLineTo(nextLine.m_pt1.m_x,
                           nextLine.m_pt1.m_y);
            } else {
                emitLineTo(isect.m_x, isect.m_y);
                emitLineTo(nextLine.m_pt1.m_x, nextLine.m_pt1.m_y);
            }
        }
    }
}

/*
   Strokes a subpath side using the \a it as source. Results are put into
   \a stroke. The function returns \c true if the subpath side was closed.
   If \a capFirst is true, we will use capPoints instead of joinPoints to
   connect the first segment, other segments will be joined using joinPoints.
   This is to put capping in order...
*/
template <class Iterator>
bool lkStrokeSide(Iterator *it, LStroker* stroker, bool capFirst, LLine<double> *startTangent)
{
    // Used in CurveToElement section below.
    const int MAX_OFFSET = 16;
    LBezier offsetCurves[MAX_OFFSET];

    LPainterPath::Element firstElement = it->next();

    LPainterPath::Element start = firstElement;
    LPainterPath::Element prev = start;

    bool first = true;

    double offset = stroker->m_strokeWidth / 2;
    while (it->hasNext()) {
        LPainterPath::Element e = it->next();

        // LineToElement
        if (e.isLineTo()) {
            LLine<double> line(prev.m_x, prev.m_y, e.m_x, e.m_y);
            if (line.m_pt1 != line.m_pt2) {
                LLine<double> normal = line.normalVector();
                normal.setLength(offset);
                line.translate(normal.dx(), normal.dy());

                // If we are starting a new subpath, move to correct starting point.
                if (first) {
                    if (capFirst){
                        stroker->joinPoints(prev.m_x, prev.m_y, line, stroker->capStyleMode());
                    }else{
                        stroker->emitMoveTo(line.m_pt1.m_x, line.m_pt1.m_y);
                    }
                    *startTangent = line;
                    first = false;
                } else {
                    stroker->joinPoints(prev.m_x, prev.m_y, line, stroker->joinStyleMode());
                }

                // Add the stroke for this line.
                stroker->emitLineTo(line.m_pt2.m_x, line.m_pt2.m_y);
                prev = e;
            }

            // CurveToElement
        } else if (e.isCurveTo()) {
            LPainterPath::Element cp2 = it->next(); // control point 2
            LPainterPath::Element ep = it->next();  // end point

            LBezier bezier =
                LBezier::fromPoints(LPoint<double>(prev.m_x, prev.m_y),
                                    LPoint<double>(e.m_x, e.m_y),
                                    LPoint<double>(cp2.m_x, cp2.m_y),
                                    LPoint<double>(ep.m_x, ep.m_y));
            int count = bezier.shifted(offsetCurves,
                                       MAX_OFFSET,
                                       offset,
                                       stroker->m_curveThreshold);

            if (count) {
                // If we are starting a new subpath, move to correct starting point
                LLine<double> tangent = bezier.startTangent();
                tangent.translate(offsetCurves[0].pt1() - bezier.pt1());
                if (first) {
                    LPoint<double> pt = offsetCurves[0].pt1();
                    if (capFirst) {
                        stroker->joinPoints(prev.m_x, prev.m_y,
                                            tangent,
                                            stroker->capStyleMode());
                    } else {
                        stroker->emitMoveTo(pt.m_x, pt.m_y);
                    }
                    *startTangent = tangent;
                    first = false;
                } else {
                    stroker->joinPoints(prev.m_x, prev.m_y,
                                        tangent,
                                        stroker->joinStyleMode());
                }

                // Add these beziers
                for (int i=0; i<count; ++i) {
                    LPoint<double> cp1 = offsetCurves[i].pt2();
                    LPoint<double> cp2 = offsetCurves[i].pt3();
                    LPoint<double> ep = offsetCurves[i].pt4();
                    stroker->emitCubicTo(cp1.m_x, cp1.m_y, cp2.m_x, cp2.m_y, ep.m_x, ep.m_y);
                }
            }

            prev = ep;
        }
    }

    //if (lFuzzyCompare(start.m_x, prev.m_x) && lFuzzyCompare(start.m_y, prev.m_y)) {
    //if (lFuzzyIsZero(start.m_x- prev.m_x) && lFuzzyIsZero(start.m_y- prev.m_y)) {
    if ((start.m_x == prev.m_x) && (start.m_y == prev.m_y)) {
        // closed subpath, join first and last point
        // don't join empty subpaths
        if (!first){
            stroker->joinPoints(prev.m_x, prev.m_y, *startTangent, stroker->joinStyleMode());
        }
        return true;
    } else {
        return false;
    }
}


void LStroker::processCurrentSubpath()
{
    LSubpathForwardIterator fwit(&m_elements);
    LSubpathBackwardIterator bwit(&m_elements);

    LLine<double> fwStartTangent, bwStartTangent;

    bool fwclosed = lkStrokeSide<LSubpathForwardIterator>(&fwit, this, false, &fwStartTangent);
    bool bwclosed = lkStrokeSide<LSubpathBackwardIterator>(&bwit, this, !fwclosed, &bwStartTangent);

    if (!bwclosed && !fwStartTangent.isNull()){
        joinPoints(m_elements.at(0).m_x, m_elements.at(0).m_y, fwStartTangent, m_joinStyle);
    }
}

LDashStrokeHooker::LDashStrokeHooker(std::shared_ptr<LStroker> stroker):m_stroker(stroker)
{

}

void LDashStrokeHooker::moveTo(double x, double y)
{
    m_stroker->moveTo(x, y);
}

void LDashStrokeHooker::lineTo(double x, double y)
{
    m_stroker->lineTo(x, y);
}

void LDashStrokeHooker::cubicTo(double c1x, double c1y, double c2x, double c2y, double ex, double ey)
{
    m_stroker->cubicTo(c1x, c1y, c2x, c2y, ex, ey);
}


LDashStroker::LDashStroker(std::shared_ptr<LStroker> stroker)
{
    m_hooker = std::make_shared<LDashStrokeHooker>(stroker);

}

void LDashStroker::begin(std::shared_ptr<LIStrokeHooker> hooker)
{
    stroker()->begin(hooker);
    m_elements.clear();
}

void LDashStroker::end()
{
    if (m_elements.size() > 1){
        processCurrentSubpath();
    }
    stroker()->end();
}

std::vector<double> LDashStroker::patternForStyle(LPenStyle style)
{
    const double space = 2;
    const double dot = 1;
    const double dash = 4;

    std::vector<double> pattern;

    switch (style) {
    case LPenStyle::Dash:{
        pattern.push_back(dash);
        pattern.push_back(space);
        break;
    }
    case LPenStyle::Dot:
        pattern.push_back(dot);
        pattern.push_back(space);
        break;
    case LPenStyle::DashDot:
        pattern.push_back(dash);
        pattern.push_back(space);
        pattern.push_back(dot);
        pattern.push_back(space);
        break;
    case LPenStyle::DashDotDot:
        pattern.push_back(dash);
        pattern.push_back(space);
        pattern.push_back(dot);
        pattern.push_back(space);
        pattern.push_back(dot);
        pattern.push_back(space);
        break;
    default:
        break;
    }

    return pattern;
}

void LDashStroker::processCurrentSubpath()
{
    int dashCount = std::min<int>(m_dashPattern.size(), 32);
    double dashes[32];

    std::shared_ptr<LStroker> tStroker = stroker();
    if (tStroker) {
        // m_customData = m_stroker;
        m_strokeWidth = tStroker->m_strokeWidth;
        m_miterLimit = tStroker->m_miterLimit;
    }

    double longestLength = 0;
    double sumLength = 0;
    for (int i=0; i<dashCount; ++i) {
        dashes[i] = std::max<double>(m_dashPattern.at(i), double(0)) * m_strokeWidth;
        sumLength += dashes[i];
        if (dashes[i] > longestLength)
            longestLength = dashes[i];
    }

    if (lFuzzyIsZero(sumLength)){
        return;
    }

    double invSumLength = double(1) / sumLength;
    dashCount = dashCount & -2; // Round down to even number

    int idash = 0; // Index to current dash
    double pos = 0; // The position on the curve, 0 <= pos <= path.length
    double elen = 0; // element length
    double doffset = m_dashOffset * m_strokeWidth;

    // make sure doffset is in range [0..sumLength)
    doffset = std::fmod(doffset, sumLength);
    if (doffset < 0){
        doffset += sumLength;
    }

    while (doffset >= dashes[idash]) {
        doffset -= dashes[idash];
        if (++idash >= dashCount){
            idash = 0;
        }
    }

    double estart = 0; // The elements starting position
    double estop = 0; // The element stop position

    LLine<double> cline;
    LSubpathFlatIterator it(&m_elements, m_dashThreshold);
    LPainterPath::Element prev = it.next();
    // if (!prev.isFinite()){
    //     return;
    // }

    bool clipping = !m_clipRect.isEmpty();
    LPoint<double> moveToPos = prev.toPointF();
    LPoint<double> lineToPos;

    // Pad to avoid clipping the borders of thick pens.
    double padding = std::max<double>(m_strokeWidth, m_miterLimit) * longestLength;
    LPoint<double> clipTl = { m_clipRect.m_x - padding, m_clipRect.m_y - padding };
    LPoint<double> clipBr = { m_clipRect.x2() + padding , m_clipRect.y2() + padding };

    bool hasMoveTo = false;
    while (it.hasNext()) {
        LPainterPath::Element e = it.next();
        // if (!qfixed2d(e).isFinite()){
        //     continue;
        // }

        assert(e.isLineTo());
        cline.setLine(prev.m_x, prev.m_y, prev.m_x, prev.m_y);
        elen = cline.length();

        estop = estart + elen;

        bool done = pos >= estop;

        // Check if the entire line should be clipped away or simplified
        bool clipIt = clipping && !lineIntersectsRect(prev.toPointF(), e.toPointF(), clipTl, clipBr);
        bool skipDashing = elen * invSumLength > repetitionLimit();
        int maxDashes = dashCount;
        if (skipDashing || clipIt) {
            // Cut away full dash sequences.
            elen -= std::floor(elen * invSumLength) * sumLength;
            // Update dash offset.
            while (!done) {
                double dpos = pos + dashes[idash] - doffset - estart;

                assert(dpos >= 0);

                if (dpos > elen) { // dash extends this line
                    doffset = dashes[idash] - (dpos - elen); // subtract the part already used
                    pos = estop; // move pos to next path element
                    done = true;
                } else { // Dash is on this line
                    pos = --maxDashes > 0 ? dpos + estart : estop;
                    done = pos >= estop;
                    if (++idash >= dashCount)
                        idash = 0;
                    doffset = 0; // full segment so no offset on next.
                }
            }
            if (clipIt) {
                hasMoveTo = false;
            } else {
                // skip costly dashing, just draw solid line
                if (!hasMoveTo) {
                    emitMoveTo(moveToPos.m_x, moveToPos.m_y);
                    hasMoveTo = true;
                }
                emitLineTo(e.m_x, e.m_y);
            }
            moveToPos = e.toPointF();
        }

        // Dash away...
        while (!done) {
            LPoint<double> p2;

            bool has_offset = doffset > 0;
            bool evenDash = (idash & 1) == 0;
            double dpos = pos + dashes[idash] - doffset - estart;

            assert(dpos >= 0);

            if (dpos > elen) { // dash extends this line
                doffset = dashes[idash] - (dpos - elen); // subtract the part already used
                pos = estop; // move pos to next path element
                done = true;
                p2 = cline.m_pt2;
            } else { // Dash is on this line
                p2 = cline.pointAt(dpos/elen);
                pos = dpos + estart;
                done = pos >= estop;
                if (++idash >= dashCount)
                    idash = 0;
                doffset = 0; // full segment so no offset on next.
            }

            if (evenDash) {
                lineToPos = p2;
                if (!clipping
                    || lineRectIntersectsRect(moveToPos, lineToPos, clipTl, clipBr))
                {
                    // If we have an offset, we're continuing a dash
                    // from a previous element and should only
                    // continue the current dash, without starting a
                    // new subpath.
                    if (!has_offset || !hasMoveTo) {
                        emitMoveTo(moveToPos.m_x, moveToPos.m_y);
                        hasMoveTo = true;
                    }

                    emitLineTo(lineToPos.m_x, lineToPos.m_y);
                } else {
                    hasMoveTo = false;
                }
                moveToPos = lineToPos;
            } else {
                moveToPos = p2;
            }
        }

        // Shuffle to the next cycle...
        estart = estop;
        prev = e;
    }
}
/////////////////////////////////////////////////
LPainterPathStroker::LPainterPathStroker()
    : m_dashOffset(0)
{
    m_stroker = std::make_shared<LStroker>();
    m_hooker = std::make_shared<LStrokeHooker>(false);
}

/*!
   Creates a new stroker based on \a pen.

   \since 5.3
 */
LPainterPathStroker::LPainterPathStroker(const std::shared_ptr<LPen> pen):LPainterPathStroker()
{
    setWidth(pen->m_width);
    setCapStyle(pen->m_capStyle);
    setJoinStyle(pen->m_joinStyle);
    setMiterLimit(pen->m_miterLimit);
    setDashOffset(pen->m_dashOffset);

    if (pen->m_style == LPenStyle::CustomDash){
        setDashPattern(pen->customDashPattern());
    }else{
        setDashPattern(pen->m_style);
    }
}


std::shared_ptr<LPainterPath> LPainterPathStroker::createStroke(const std::shared_ptr<LPainterPath> path) const
{
    if (path == nullptr || path->isEmpty()){
        return path;
    }
    m_hooker->begin();

    if (m_dashPattern.empty()) {
       m_stroker->strokePath(path, m_hooker, LTransform2D());
    } else {
        LDashStroker dashStroker(m_stroker);
        dashStroker.m_dashPattern = m_dashPattern;
        dashStroker.m_dashOffset = m_dashOffset;
        dashStroker.m_clipRect = m_stroker->m_clipRect;
        dashStroker.strokePath(path, m_hooker, LTransform2D());
    }
    m_hooker->end();
    std::shared_ptr<LPainterPath> ret = m_hooker->strokedPath();
    ret->setFillRule(LFillRule::Winding);
    return ret;
}

void LPainterPathStroker::setWidth(double width)
{
    if (width <= 0){
        width = 1;
    }
    m_stroker->m_strokeWidth = width;
}

double LPainterPathStroker::width() const
{
    return m_stroker->m_strokeWidth;
}



void LPainterPathStroker::setCapStyle(LPenCapStyle style)
{
    m_stroker->setCapStyle(style);
}


LPenCapStyle LPainterPathStroker::capStyle() const
{
    return m_stroker->capStyle();
}


void LPainterPathStroker::setJoinStyle(LPenJoinStyle style)
{
    m_stroker->setJoinStyle(style);
}


LPenJoinStyle LPainterPathStroker::joinStyle() const
{
    return m_stroker->joinStyle();
}

void LPainterPathStroker::setMiterLimit(double limit)
{
    m_stroker->m_miterLimit = limit;
}

double LPainterPathStroker::miterLimit() const
{
    return m_stroker->m_miterLimit;
}


void LPainterPathStroker::setCurveThreshold(double threshold)
{
    m_stroker->m_curveThreshold = threshold;
}


double LPainterPathStroker::curveThreshold() const
{
    return m_stroker->m_curveThreshold;
}

void LPainterPathStroker::setDashPattern(LPenStyle style)
{
    m_dashPattern = LDashStroker::patternForStyle(style);
}

void LPainterPathStroker::setDashPattern(const std::vector<double>& dashPattern)
{
    m_dashPattern = dashPattern;
}

const std::vector<double>& LPainterPathStroker::dashPattern() const
{
    return m_dashPattern;
}

double LPainterPathStroker::dashOffset() const
{
    return m_dashOffset;
}

void LPainterPathStroker::setDashOffset(double offset)
{
    m_dashOffset = offset;
}

LK_NAMESPACE_END




