#include "lpathclipper.h"
#include "lbezier.h"

LK_NAMESPACE_BEGIN

static void normalize(double &x, double &y)
{
    double reciprocal = 1 / std::sqrt(x * x + y * y);
    x *= reciprocal;
    y *= reciprocal;
}


static bool isLine(const LBezier &bezier)
{
    const bool equal_1_2 = bezier.pt1()== bezier.pt2();
    const bool equal_2_3 = bezier.pt2() == bezier.pt3();
    const bool equal_3_4 = bezier.pt3() == bezier.pt4();

    // point?
    if (equal_1_2 && equal_2_3 && equal_3_4){
        return true;
    }

    if (bezier.pt1()== bezier.pt4()){
        return equal_1_2 || equal_3_4;
    }

    return (equal_1_2 && equal_3_4) || (equal_1_2 && equal_2_3) || (equal_2_3 && equal_3_4);
}

struct LIntersection
{
    double alphaA;
    double alphaB;
    LPoint<double> pos;
};

class LIntersectionFinder
{
public:
    void produceIntersections(LPathSegments &segments);
    bool hasIntersections(const LPathSegments &a, const LPathSegments &b) const;

private:
    bool linesIntersect(const LLine<double> &a, const LLine<double> &b) const;
};

bool LIntersectionFinder::linesIntersect(const LLine<double> &a, const LLine<double> &b) const
{
    const LPoint<double> p1 = a.m_pt1;
    const LPoint<double> p2 = a.m_pt2;

    const LPoint<double> q1 = b.m_pt1;
    const LPoint<double> q2 = b.m_pt2;

    if ((p1 == p2) || (q1 == q2)){
        return false;
    }

    const bool p1EqualsQ1 = p1 == q1;
    const bool p2EqualsQ2 = p2 == q2;

    if (p1EqualsQ1 && p2EqualsQ2){
        return true;
    }

    const bool p1EqualsQ2 = p1 == q2;
    const bool p2EqualsQ1 = p2 == q1;

    if (p1EqualsQ2 && p2EqualsQ1){
        return true;
    }

    const LPoint<double> pDelta = p2 - p1;
    const LPoint<double> qDelta = q2 - q1;

    const double par = pDelta.m_x * qDelta.m_y - pDelta.m_y * qDelta.m_x;

    if (lFuzzyIsZero(par)) {
        const LPoint<double> normal(-pDelta.m_y, pDelta.m_x);

        // coinciding?
        if (lFuzzyIsZero(LSize<double>::dotProduct(normal, q1 - p1))) {
            const double dp = LSize<double>::dotProduct(pDelta, pDelta);

            const double tq1 = LSize<double>::dotProduct(pDelta, q1 - p1);
            const double tq2 = LSize<double>::dotProduct(pDelta, q2 - p1);

            if ((tq1 > 0 && tq1 < dp) || (tq2 > 0 && tq2 < dp)){
                return true;
            }

            const double dq = LSize<double>::dotProduct(qDelta, qDelta);

            const double tp1 = LSize<double>::dotProduct(qDelta, p1 - q1);
            const double tp2 = LSize<double>::dotProduct(qDelta, p2 - q1);

            if ((tp1 > 0 && tp1 < dq) || (tp2 > 0 && tp2 < dq)){
                return true;
            }
        }

        return false;
    }

    const double invPar = 1 / par;
    const double tp = (qDelta.m_y * (q1.m_x - p1.m_x) - qDelta.m_x * (q1.m_y - p1.m_y)) * invPar;
    if ((tp < 0) || (tp > 1)){
        return false;
    }

    const double tq = (pDelta.m_y * (q1.m_x - p1.m_x) - pDelta.m_x * (q1.m_y - p1.m_y)) * invPar;
    return tq >= 0 && tq <= 1;
}

bool LIntersectionFinder::hasIntersections(const LPathSegments &a, const LPathSegments &b) const
{
    if (a.segments() == 0 || b.segments() == 0){
        return false;
    }

    const LRect<double> &rb0 = b.elementBounds(0);

    double minX = rb0.m_x;
    double minY = rb0.m_y;
    double maxX = rb0.x2();
    double maxY = rb0.y2();

    for (int i = 1; i < b.segments(); ++i) {
        const LRect<double> &r = b.elementBounds(i);
        minX = std::min<double>(minX, r.m_x);
        minY = std::min<double>(minY, r.m_y);
        maxX = std::max<double>(maxX, r.x2());
        maxY = std::max<double>(maxY, r.y2());
    }

    LRect<double> rb(minX, minY, maxX - minX, maxY - minY, false);

    for (int i = 0; i < a.segments(); ++i) {
        const LRect<double> &r1 = a.elementBounds(i);

        if (r1.m_x > rb.x2() || rb.m_x > r1.x2()){
            continue;
        }
        if (r1.m_y > rb.y2() || rb.m_y > r1.y2()){
            continue;
        }

        for (int j = 0; j < b.segments(); ++j) {
            const LRect<double> &r2 = b.elementBounds(j);

            if (r1.m_x > r2.x2() || r2.m_x > r1.x2()){
                continue;
            }
            if (r1.m_y > r2.y2() || r2.m_y > r1.y2()){
                continue;
            }

            if (linesIntersect(a.lineAt(i), b.lineAt(j))){
                return true;
            }
        }
    }
    return false;
}



LPathClipper::LPathClipper(const std::shared_ptr<LPainterPath> subject,
                           const std::shared_ptr<LPainterPath> clip)
    : subjectPath(subject)
    , clipPath(clip)
{
    aMask = subjectPath->fillRule() == LFillRule::Winding ? ~0x0 : 0x1;
    bMask = clipPath->fillRule() == LFillRule::Winding ? ~0x0 : 0x1;
}

static void clear(LWingedEdge& list, int edge, LPathEdge::Traversal traversal)
{
    LWingedEdge::TraversalStatus status;
    status.edge = edge;
    status.traversal = traversal;
    status.direction = LPathEdge::Forward;

    do {
        if (status.traversal == LPathEdge::LeftTraversal){
            list.edge(status.edge)->flag |= 1;
        }
        else{
            list.edge(status.edge)->flag |= 2;
        }

        status = list.next(status);
    } while (status.edge != edge);
}

template <typename InputIterator>
InputIterator lFuzzyFind(InputIterator first, InputIterator last, double val)
{
    while (first != last && !lFuzzyCompare(*first, val)){
        ++first;
    }
    return first;
}


bool LPathClipper::pathToRect(const std::shared_ptr<LPainterPath> path, LRect<double> *rect)
{
    if(path == nullptr){
        return false;
    }

    if (path->elementCount() != 5){
        return false;
    }

    const bool mightBeRect = path->elementAt(0).isMoveTo()
                             && path->elementAt(1).isLineTo()
                             && path->elementAt(2).isLineTo()
                             && path->elementAt(3).isLineTo()
                             && path->elementAt(4).isLineTo();

    if (!mightBeRect){
        return false;
    }

    const double x1 = path->elementAt(0).m_x;
    const double y1 = path->elementAt(0).m_y;

    const double x2 = path->elementAt(1).m_x;
    const double y2 = path->elementAt(2).m_y;

    if (path->elementAt(1).m_y != y1){
        return false;
    }

    if (path->elementAt(2).m_x != x2){
        return false;
    }

    if (path->elementAt(3).m_x != x1 || path->elementAt(3).m_y != y2){
        return false;
    }

    if (path->elementAt(4).m_x != x1 || path->elementAt(4).m_y != y1){
        return false;
    }

    if (rect){
        rect->setCoords(x1, y1, x2, y2);
    }
    return true;
}


std::shared_ptr<LPainterPath> LPathClipper::clip(Operation operation)
{
    op = operation;

    if (op != Simplify) {
        if (subjectPath == clipPath){
            return op == BoolSub ? std::make_shared<LPainterPath>() : subjectPath;
        }

        bool subjectIsRect = pathToRect(subjectPath, nullptr);
        bool clipIsRect = pathToRect(clipPath, nullptr);

        const LRect<double> clipBounds = clipPath->boundingRect();
        const LRect<double> subjectBounds = subjectPath->boundingRect();

        if (!clipBounds.isIntersects(subjectBounds)) {
            switch (op) {
            case BoolSub:{
                return subjectPath;
            }
            case BoolAnd:{
                return std::make_shared<LPainterPath>();
            }
            case BoolOr: {
                std::shared_ptr<LPainterPath> result = std::make_shared<LPainterPath>(subjectPath);
                if (result->fillRule() == clipPath->fillRule()) {
                    result->addPath(clipPath);
                } else if (result->fillRule() == LFillRule::Winding) {
                    result = result->simplified();
                    result->addPath(clipPath);
                } else {
                    result->addPath(clipPath->simplified());
                }
                return result;
            }
            default:
                break;
            }
        }

        if (clipBounds.contains(subjectBounds)) {
            if (clipIsRect) {
                switch (op) {
                case BoolSub:{
                    return std::make_shared<LPainterPath>();
                }
                case BoolAnd:{
                    return subjectPath;
                }
                case BoolOr:{
                    return clipPath;
                }
                default:
                    break;
                }
            }
        } else if (subjectBounds.contains(clipBounds)) {
            if (subjectIsRect) {
                switch (op) {
                case BoolSub:
                    if (clipPath->fillRule() == LFillRule::OddEven) {
                        std::shared_ptr<LPainterPath> result = clipPath;
                        result->addRect(subjectBounds);
                        return result;
                    } else {
                        std::shared_ptr<LPainterPath> result = clipPath->simplified();
                        result->addRect(subjectBounds);
                        return result;
                    }
                case BoolAnd:{
                    return clipPath;
                }
                case BoolOr:{
                    return subjectPath;
                }
                default:
                    break;
                }
            }
        }

        if (op == BoolAnd) {
            if (subjectIsRect){
                return intersect(clipPath, subjectBounds);
            }
            else if (clipIsRect){
                return intersect(subjectPath, clipBounds);
            }
        }
    }

    LWingedEdge list(subjectPath, clipPath);

    doClip(list, ClipMode);
    std::shared_ptr<LPainterPath> path = list.toPath();
    return path;
}

bool LPathClipper::intersect()
{
    if (subjectPath == clipPath){
        return true;
    }

    LRect<double> r1 = subjectPath->controlPointRect();
    LRect<double> r2 = clipPath->controlPointRect();
    if (std::max<double>(r1.m_x, r2.m_x) > std::min<double>(r1.x2(), r2.x2()) ||
        std::max<double>(r1.m_y, r2.m_y) > std::min<double>(r1.y2(), r2.y2())) {
        // no way we could intersect
        return false;
    }

    bool subjectIsRect = pathToRect(subjectPath);
    bool clipIsRect = pathToRect(clipPath);

    if (subjectIsRect && clipIsRect){
        return true;
    }
    else if (subjectIsRect){
        return clipPath->intersects(r1);
    }
    else if (clipIsRect){
        return subjectPath->intersects(r2);
    }

    LPathSegments a;
    a.reserve(subjectPath->elementCount());
    a.setPath(subjectPath);
    LPathSegments b;
    b.reserve(clipPath->elementCount());
    b.setPath(clipPath);

    LIntersectionFinder finder;
    if (finder.hasIntersections(a, b)){
        return true;
    }

    for (int i = 0; i < clipPath->elementCount(); ++i) {
        if (clipPath->elementAt(i).m_type == LPainterPath::MoveToElement) {
            const LPoint<double> point = clipPath->elementAt(i).toPointF();
            if (r1.contains(point) && subjectPath->contains(point)){
                return true;
            }
        }
    }

    for (int i = 0; i < subjectPath->elementCount(); ++i) {
        if (subjectPath->elementAt(i).m_type == LPainterPath::MoveToElement) {
            const LPoint<double> point = subjectPath->elementAt(i).toPointF();
            if (r2.contains(point) && clipPath->contains(point)){
                return true;
            }
        }
    }

    return false;
}

bool LPathClipper::contains()
{
    if (subjectPath == clipPath){
        return false;
    }

    LRect<double> r1 = subjectPath->controlPointRect();
    LRect<double> r2 = clipPath->controlPointRect();
    if (std::max<double>(r1.m_x, r2.m_x) > std::min<double>(r1.m_x + r1.width(), r2.m_x + r2.width()) ||
        std::max<double>(r1.m_y, r2.m_y) > std::min<double>(r1.m_y + r1.height(), r2.m_y + r2.height())) {
        // no intersection -> not contained
        return false;
    }

    bool clipIsRect = pathToRect(clipPath);
    if (clipIsRect){
        return subjectPath->contains(r2);
    }

    LPathSegments a;
    a.reserve(subjectPath->elementCount());
    a.setPath(subjectPath);
    LPathSegments b;
    b.reserve(clipPath->elementCount());
    b.setPath(clipPath);

    LIntersectionFinder finder;
    if (finder.hasIntersections(a, b)){
        return false;
    }

    for (int i = 0; i < clipPath->elementCount(); ++i) {
        if (clipPath->elementAt(i).m_type == LPainterPath::MoveToElement) {
            const LPoint<double> point = clipPath->elementAt(i).toPointF();
            if (!r1.contains(point) || !subjectPath->contains(point)){
                return false;
            }
        }
    }
    return true;
}

static bool fuzzyCompare(double a, double b)
{
    return lFuzzyCompare(a, b);
}

bool LPathClipper::doClip(LWingedEdge &list, ClipperMode mode)
{
    std::vector<double> yCoords;
    yCoords.reserve(list.vertexCount());
    for (int i = 0; i < list.vertexCount(); ++i){
        yCoords.push_back(list.vertex(i)->y);
    }

    sort(yCoords.begin(), yCoords.end());
    yCoords.erase(std::unique(yCoords.begin(), yCoords.end(), fuzzyCompare), yCoords.end());


    bool found;
    do {
        found = false;
        int index = 0;
        double maxHeight = 0;
        for (int i = 0; i < list.edgeCount(); ++i) {
            LPathEdge *edge = list.edge(i);

            // have both sides of this edge already been handled?
            if ((edge->flag & 0x3) == 0x3){
                continue;
            }

            LPathVertex *a = list.vertex(edge->first);
            LPathVertex *b = list.vertex(edge->second);

            if (lFuzzyCompare(a->y, b->y)){
                continue;
            }

            found = true;

            double height = std::abs(a->y - b->y);
            if (height > maxHeight) {
                index = i;
                maxHeight = height;
            }
        }

        if (found) {
            LPathEdge *edge = list.edge(index);

            LPathVertex *a = list.vertex(edge->first);
            LPathVertex *b = list.vertex(edge->second);

            // FIXME: this can be optimized by using binary search
            const int first = lFuzzyFind(yCoords.cbegin(), yCoords.cend(), std::min<double>(a->y, b->y)) - yCoords.cbegin();
            const int last = lFuzzyFind(yCoords.cbegin() + first, yCoords.cend(), std::max<double>(a->y, b->y)) - yCoords.cbegin();

//            std::assert(first < yCoords.size() - 1);
//            std::assert(last < yCoords.size());

            double biggestGap = yCoords.at(first + 1) - yCoords.at(first);
            int bestIdx = first;
            for (int i = first + 1; i < last; ++i) {
                double gap = yCoords.at(i + 1) - yCoords.at(i);

                if (gap > biggestGap) {
                    bestIdx = i;
                    biggestGap = gap;
                }
            }
            const double bestY = 0.5 * (yCoords.at(bestIdx) + yCoords.at(bestIdx + 1));

            if (handleCrossingEdges(list, bestY, mode) && mode == CheckMode){
                return true;
            }

            edge->flag |= 0x3;
        }
    } while (found);

    if (mode == ClipMode){
        list.simplify();
    }
    return false;
}

static void traverse(LWingedEdge &list, int edge, LPathEdge::Traversal traversal)
{
    LWingedEdge::TraversalStatus status;
    status.edge = edge;
    status.traversal = traversal;
    status.direction = LPathEdge::Forward;

    do {
        int flag = status.traversal == LPathEdge::LeftTraversal ? 1 : 2;
        LPathEdge *ep = list.edge(status.edge);

        ep->flag |= (flag | (flag << 4));
        status = list.next(status);
    } while (status.edge != edge);
}

struct LCrossingEdge
{
    int edge;
    double x;

    bool operator<(const LCrossingEdge &edge) const
    {
        return x < edge.x;
    }
};

static bool boolOp(bool a, bool b, LPathClipper::Operation op)
{
    switch (op) {
    case LPathClipper::BoolAnd:{
        return a && b;
    }
    case LPathClipper::BoolOr:
    case LPathClipper::Simplify:{
        return a || b;
    }
    case LPathClipper::BoolSub:{
        return a && !b;
    }
    default:{
        return false;
    }
    }
}

LWingedEdge::LWingedEdge(){
}

LWingedEdge::LWingedEdge(const std::shared_ptr<LPainterPath> subject, const std::shared_ptr<LPainterPath> clip)
{
    int reserveCount = subject->elementCount();
    m_edges.reserve(reserveCount);
    m_vertices.reserve(reserveCount);
    m_segments.reserve(reserveCount);

    m_segments.setPath(subject);
    m_segments.addPath(clip);
    intersectAndAdd();
}

void LWingedEdge::simplify()
{
    for (int i = 0; i < edgeCount(); ++i) {
        const LPathEdge *ep = edge(i);

        // if both sides are part of the inside then we can collapse the edge
        int flag = 0x3 << 4;
        if ((ep->flag & flag) == flag) {
            removeEdge(i);
            ep->flag &= ~flag;
        }
    }
}

static void addLineTo(std::shared_ptr<LPainterPath> path, const LPoint<double> &point)
{
    const int elementCount = path->elementCount();
    if (elementCount >= 2) {
        const LPainterPath::Element &middle = path->elementAt(elementCount - 1);
        if (middle.m_type == LPainterPath::LineToElement) {
            const LPoint<double> first = path->elementAt(elementCount - 2).toPointF();
            const LPoint<double> d1 = point - first;
            const LPoint<double> d2 = middle.toPointF() - first;

            const LPoint<double> p(-d1.m_y, d1.m_x);

            if (lFuzzyIsZero(LSize<double>::dotProduct(p, d2))) {
                path->setElementPositionAt(elementCount - 1, point.m_x, point.m_y);
                return;
            }
        }
    }
    path->lineTo(point);
}

static void add(std::shared_ptr<LPainterPath> path, const LWingedEdge &list, int edge, LPathEdge::Traversal traversal)
{
    LWingedEdge::TraversalStatus status;
    status.edge = edge;
    status.traversal = traversal;
    status.direction = LPathEdge::Forward;

    path->moveTo(list.vertex(list.edge(edge)->first)->toPointF());
    do {
        const LPathEdge *ep = list.edge(status.edge);
        addLineTo(path, list.vertex(ep->vertex(status.direction))->toPointF());
        if (status.traversal == LPathEdge::LeftTraversal){
            ep->flag &= ~16;
        }
        else{
            ep->flag &= ~32;
        }
        status = list.next(status);
    } while (status.edge != edge);
}


std::shared_ptr<LPainterPath> LWingedEdge::toPath() const
{
    std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();

    for (int i = 0; i < edgeCount(); ++i) {
        const LPathEdge *ep = edge(i);
        if (ep->flag & 16) {
            add(path, *this, i, LPathEdge::LeftTraversal);
        }

        if (ep->flag & 32){
            add(path, *this, i, LPathEdge::RightTraversal);
        }
    }
    return path;
}

LWingedEdge::TraversalStatus LWingedEdge::next(const TraversalStatus &status) const
{
    const LPathEdge *sp = edge(status.edge);

    TraversalStatus result;
    result.edge = sp->next(status.traversal, status.direction);
    result.traversal = status.traversal;
    result.direction = status.direction;

    const LPathEdge *rp = edge(result.edge);

    if (sp->vertex(status.direction) == rp->vertex(status.direction)){
        result.flip();
    }
    return result;
}

int LWingedEdge::addEdge(const LPoint<double> &a, const LPoint<double> &b)
{
    int fi = insert(a);
    int si = insert(b);

    return addEdge(fi, si);
}

static int commonEdge(const LWingedEdge &list, int a, int b)
{
    const LPathVertex *ap = list.vertex(a);
    const LPathVertex *bp = list.vertex(b);

    if (ap->edge < 0 || bp->edge < 0){
        return -1;
    }

    LWingedEdge::TraversalStatus status;
    status.edge = ap->edge;
    status.direction = list.edge(status.edge)->directionTo(a);
    status.traversal = LPathEdge::RightTraversal;

    do {
        const LPathEdge *ep = list.edge(status.edge);

        if ((ep->first == a && ep->second == b)
            || (ep->first == b && ep->second == a)){
            return status.edge;
        }
        status = list.next(status);
        status.flip();
    } while (status.edge != ap->edge);

    return -1;
}


static double computeAngle(const LPoint<double> &v)
{
    if (v.m_x == 0) {
        return v.m_y <= 0 ? 0 : 64.;
    } else if (v.m_y == 0) {
        return v.m_x <= 0 ? 32. : 96.;
    }

    double vx = v.m_x;
    double vy = v.m_y;
    normalize(vx, vy);
    if (vy < 0) {
        if (vx < 0) { // 0 - 32
            return -32. * vx;
        } else { // 96 - 128
            return 128. - 32. * vx;
        }
    } else { // 32 - 96
        return 64. + 32. * vx;
    }
}

int LWingedEdge::addEdge(int fi, int si)
{
    if (fi == si){
        return -1;
    }

    int common = commonEdge(*this, fi, si);
    if (common >= 0){
        return common;
    }

    m_edges.push_back(LPathEdge(fi, si));
    int ei = m_edges.size() - 1;

    LPathVertex *fp = vertex(fi);
    LPathVertex *sp = vertex(si);

    LPathEdge *ep = edge(ei);
    const LPoint<double> tangent = sp->toPointF() - fp->toPointF();
    ep->angle = computeAngle(tangent);
    ep->invAngle = ep->angle + 64;
    if (ep->invAngle >= 128){
        ep->invAngle -= 128;
    }

    LPathVertex *vertices[2] = { fp, sp };
    LPathEdge::Direction dirs[2] = { LPathEdge::Backward, LPathEdge::Forward };

    for (int i = 0; i < 2; ++i) {
        LPathVertex *vp = vertices[i];
        if (vp->edge < 0) {
            vp->edge = ei;
            ep->setNext(dirs[i], ei);
        } else {
            int vi = ep->vertex(dirs[i]);
            TraversalStatus os = findInsertStatus(vi, ei);
            LPathEdge *op = edge(os.edge);
            TraversalStatus ns = next(os);
            ns.flipDirection();
            LPathEdge *np = edge(ns.edge);

            op->setNext(os.traversal, os.direction, ei);
            np->setNext(ns.traversal, ns.direction, ei);

            int oe = os.edge;
            int ne = ns.edge;

            os = next(os);
            ns = next(ns);

            os.flipDirection();
            ns.flipDirection();

            ep->setNext(os.traversal, os.direction, oe);
            ep->setNext(ns.traversal, ns.direction, ne);
        }
    }

    return ei;
}

bool LWingedEdge::isInside(double x, double y) const
{
    int winding = 0;
    for (int i = 0; i < edgeCount(); ++i) {
        const LPathEdge *ep = edge(i);

        // left xor right
        int w = ((ep->flag >> 4) ^ (ep->flag >> 5)) & 1;

        if (!w)
            continue;

        LPoint<double> a = vertex(ep->first)->toPointF();
        LPoint<double> b = vertex(ep->second)->toPointF();

        if ((a.m_y < y && b.m_y > y) || (a.m_y > y && b.m_y < y)) {
            double intersectionX = a.m_x + (b.m_x - a.m_x) * (y - a.m_y) / (b.m_y - a.m_y);

            if (intersectionX > x){
                winding += w;
            }
        }
    }

    return winding & 1;
}

void LWingedEdge::intersectAndAdd()
{
    LIntersectionFinder finder;
    finder.produceIntersections(m_segments);

    m_segments.mergePoints();

    for (int i = 0; i < m_segments.points(); ++i){
        addVertex(m_segments.pointAt(i));
    }

    std::vector<LPathSegments::Intersection> intersections;
    intersections.reserve(m_segments.segments());
    for (int i = 0; i < m_segments.segments(); ++i) {
        intersections.clear();

        int pathId = m_segments.pathId(i);

        const LPathSegments::Intersection *isect = m_segments.intersectionAt(i);
        while (isect) {
            intersections.push_back(*isect);

            if (isect->next) {
                isect += isect->next;
            } else {
                isect = nullptr;
            }
        }

        std::sort(intersections.begin(), intersections.end());

        int first = m_segments.segmentAt(i).va;
        int second = m_segments.segmentAt(i).vb;

        int last = first;
        for (int j = 0; j < (int)intersections.size(); ++j) {
            const LPathSegments::Intersection &isect = intersections.at(j);

            LPathEdge *ep = edge(addEdge(last, isect.vertex));

            if (ep) {
                const int dir = m_segments.pointAt(last).m_y < m_segments.pointAt(isect.vertex).m_y ? 1 : -1;
                if (pathId == 0){
                    ep->windingA += dir;
                }
                else{
                    ep->windingB += dir;
                }
            }

            last = isect.vertex;
        }

        LPathEdge *ep = edge(addEdge(last, second));

        if (ep) {
            const int dir = m_segments.pointAt(last).m_y < m_segments.pointAt(second).m_y ? 1 : -1;
            if (pathId == 0){
                ep->windingA += dir;
            }
            else{
                ep->windingB += dir;
            }
        }
    }
}

void LWingedEdge::removeEdge(int ei)
{
    LPathEdge *ep = edge(ei);

    TraversalStatus status;
    status.direction = LPathEdge::Forward;
    status.traversal = LPathEdge::RightTraversal;
    status.edge = ei;

    TraversalStatus forwardRight = next(status);
    forwardRight.flipDirection();

    status.traversal = LPathEdge::LeftTraversal;
    TraversalStatus forwardLeft = next(status);
    forwardLeft.flipDirection();

    status.direction = LPathEdge::Backward;
    TraversalStatus backwardLeft = next(status);
    backwardLeft.flipDirection();

    status.traversal = LPathEdge::RightTraversal;
    TraversalStatus backwardRight = next(status);
    backwardRight.flipDirection();

    edge(forwardRight.edge)->setNext(forwardRight.traversal, forwardRight.direction, forwardLeft.edge);
    edge(forwardLeft.edge)->setNext(forwardLeft.traversal, forwardLeft.direction, forwardRight.edge);

    edge(backwardRight.edge)->setNext(backwardRight.traversal, backwardRight.direction, backwardLeft.edge);
    edge(backwardLeft.edge)->setNext(backwardLeft.traversal, backwardLeft.direction, backwardRight.edge);

    ep->setNext(LPathEdge::Forward, ei);
    ep->setNext(LPathEdge::Backward, ei);

    LPathVertex *a = vertex(ep->first);
    LPathVertex *b = vertex(ep->second);

    a->edge = backwardRight.edge;
    b->edge = forwardRight.edge;
}

int LWingedEdge::insert(const LPathVertex &vertex)
{
    if (!m_vertices.empty()) {
        const LPathVertex &last = m_vertices.back();
        if (vertex.x == last.x && vertex.y == last.y)
            return m_vertices.size() - 1;

        for (int i = 0; i < (int)m_vertices.size(); ++i) {
            const LPathVertex &v = m_vertices.at(i);
            if (lFuzzyCompare(v.x, vertex.x) && lFuzzyCompare(v.y, vertex.y)) {
                return i;
            }
        }
    }

    m_vertices.push_back(vertex);
    return m_vertices.size() - 1;
}

LWingedEdge::TraversalStatus LWingedEdge::findInsertStatus(int vi, int ei) const
{
    const LPathVertex *vp = vertex(vi);

    int position = vp->edge;
    double d = 128.;

    TraversalStatus status;
    status.direction = edge(vp->edge)->directionTo(vi);
    status.traversal = LPathEdge::RightTraversal;
    status.edge = vp->edge;

    do {
        status = next(status);
        status.flip();

        double d2 = delta(vi, ei, status.edge);

        if (d2 < d) {
            position = status.edge;
            d = d2;
        }
    } while (status.edge != vp->edge);

    status.traversal = LPathEdge::LeftTraversal;
    status.direction = LPathEdge::Forward;
    status.edge = position;

    if (edge(status.edge)->vertex(status.direction) != vi){
        status.flip();
    }
    return status;
}

double LWingedEdge::delta(int vertex, int a, int b) const
{
    const LPathEdge *ap = edge(a);
    const LPathEdge *bp = edge(b);

    double a_angle = ap->angle;
    double b_angle = bp->angle;

    if (vertex == ap->second){
        a_angle = ap->invAngle;
    }

    if (vertex == bp->second){
        b_angle = bp->invAngle;
    }

    double result = b_angle - a_angle;

    if (result >= 128.){
        return result - 128.;
    }
    else if (result < 0){
        return result + 128.;
    }
    else{
        return result;
    }
}

static std::vector<LCrossingEdge> findCrossings(const LWingedEdge &list, double y)
{
    std::vector<LCrossingEdge> crossings;
    for (int i = 0; i < (int)list.edgeCount(); ++i) {
        const LPathEdge *edge = list.edge(i);
        LPoint<double> a = list.vertex(edge->first)->toPointF();
        LPoint<double> b = list.vertex(edge->second)->toPointF();

        if ((a.m_y < y && b.m_y > y) || (a.m_y > y && b.m_y < y)) {
            const double intersection = a.m_x + (b.m_x - a.m_x) * (y - a.m_y) / (b.m_y - a.m_y);
            const LCrossingEdge edge = { i, intersection };
            crossings.push_back(edge);
        }
    }
    return crossings;
}

bool LPathClipper::handleCrossingEdges(LWingedEdge &list, double y, ClipperMode mode)
{
    std::vector<LCrossingEdge> crossings = findCrossings(list, y);

    sort(crossings.begin(), crossings.end());

    int windingA = 0;
    int windingB = 0;

    int windingD = 0;

    for (int i = 0; i < (int)crossings.size() - 1; ++i) {
        int ei = crossings.at(i).edge;
        const LPathEdge *edge = list.edge(ei);

        windingA += edge->windingA;
        windingB += edge->windingB;

        const bool hasLeft = (edge->flag >> 4) & 1;
        const bool hasRight = (edge->flag >> 4) & 2;

        windingD += hasLeft ^ hasRight;

        const bool inA = (windingA & aMask) != 0;
        const bool inB = (windingB & bMask) != 0;
        const bool inD = (windingD & 0x1) != 0;

        const bool inside = boolOp(inA, inB, op);
        const bool add = inD ^ inside;

        if (add) {
            if (mode == CheckMode)
                return true;

            double y0 = list.vertex(edge->first)->y;
            double y1 = list.vertex(edge->second)->y;

            if (y0 < y1) {
                if (!(edge->flag & 1)){
                    traverse(list, ei, LPathEdge::LeftTraversal);
                }

                if (!(edge->flag & 2)){
                    clear(list, ei, LPathEdge::RightTraversal);
                }
            } else {
                if (!(edge->flag & 1)){
                    clear(list, ei, LPathEdge::LeftTraversal);
                }

                if (!(edge->flag & 2)){
                    traverse(list, ei, LPathEdge::RightTraversal);
                }
            }

            ++windingD;
        } else {
            if (!(edge->flag & 1)){
                clear(list, ei, LPathEdge::LeftTraversal);
            }

            if (!(edge->flag & 2)){
                clear(list, ei, LPathEdge::RightTraversal);
            }
        }
    }

    return false;
}

namespace lClip{
std::vector<std::shared_ptr<LPainterPath> > toSubpaths(const std::shared_ptr<LPainterPath> path)
{

    std::vector<std::shared_ptr<LPainterPath> > subpaths;
    if (path->isEmpty()){
        return subpaths;
    }

    std::shared_ptr<LPainterPath> current = std::make_shared<LPainterPath>();
    for (int i = 0; i < path->elementCount(); ++i) {
        const LPainterPath::Element &e = path->elementAt(i);
        switch (e.m_type) {
        case LPainterPath::MoveToElement:{
            if (current->elementCount() > 1){
                subpaths.push_back(current);
            }
            current = std::make_shared<LPainterPath>();
            current->moveTo(e.toPointF());
            break;
        }
        case LPainterPath::LineToElement:{
            current->lineTo(e.toPointF());
            break;
        }
        case LPainterPath::CurveToElement: {
            current->cubicTo(e.toPointF(),
                             path->elementAt(i + 1).toPointF(),
                             path->elementAt(i + 2).toPointF());
            i+=2;
            break;
        }
        case LPainterPath::CurveToDataElement:{
            break;
        }
        }
    }

    if (current->elementCount() > 1){
        subpaths.push_back(current);
    }
    return subpaths;
}

enum Edge
{
    Left, Top, Right, Bottom
};

static bool isVertical(Edge edge)
{
    return edge == Left || edge == Right;
}

template <Edge edge>
bool compare(const LPoint<double> &p, double t)
{
    switch (edge)
    {
    case Left:{
        return p.m_x < t;
    }
    case Right:{
        return p.m_x > t;
    }
    case Top:{
        return p.m_y < t;
    }
    default:{
        return p.m_y > t;
    }
    }
}

template <Edge edge>
LPoint<double> intersectLine(const LPoint<double> &a, const LPoint<double> &b, double t)
{
    LLine<double> line(a, b);
    switch (edge) {
    case Left:
    case Right:{
        return line.pointAt((t - a.m_x) / (b.m_x - a.m_x));
    }
    default:{
        return line.pointAt((t - a.m_y) / (b.m_y - a.m_y));
    }
    }
}

void addLine(std::shared_ptr<LPainterPath> path, const LLine<double> &line)
{
    if (path->elementCount() > 0){
        path->lineTo(line.m_pt1);
    }
    else{
        path->moveTo(line.m_pt2);
    }
    path->lineTo(line.m_pt2);
}

template <Edge edge>
void clipLine(const LPoint<double> &a, const LPoint<double> &b, double t, std::shared_ptr<LPainterPath> result)
{
    bool outA = compare<edge>(a, t);
    bool outB = compare<edge>(b, t);
    if (outA && outB){
        return;
    }

    if (outA){
        addLine(result, LLine<double>(intersectLine<edge>(a, b, t), b));
    }
    else if(outB){
        addLine(result, LLine<double>(a, intersectLine<edge>(a, b, t)));
    }
    else{
        addLine(result, LLine<double>(a, b));
    }
}

void addBezier(std::shared_ptr<LPainterPath> path, const LBezier &bezier)
{
    if (path->elementCount() > 0){
        path->lineTo(bezier.pt1());
    }
    else{
        path->moveTo(bezier.pt1());
    }
    path->cubicTo(bezier.pt2(), bezier.pt3(), bezier.pt4());
}

template <Edge edge>
void clipBezier(const LPoint<double> &a,
                const LPoint<double> &b,
                const LPoint<double> &c,
                const LPoint<double> &d,
                double t,
                std::shared_ptr<LPainterPath> result)
{
    LBezier bezier = LBezier::fromPoints(a, b, c, d);

    bool outA = compare<edge>(a, t);
    bool outB = compare<edge>(b, t);
    bool outC = compare<edge>(c, t);
    bool outD = compare<edge>(d, t);

    int outCount = int(outA) + int(outB) + int(outC) + int(outD);

    if (outCount == 4){
        return;
    }

    if (outCount == 0) {
        addBezier(result, bezier);
        return;
    }

    LTransform2D flip = isVertical(edge) ? LTransform2D(0, 1, 1, 0, 0, 0) : LTransform2D();
    LBezier unflipped = bezier;
    LBezier flipped = bezier.mapBy(flip);

    double t0 = 0, t1 = 1;
    int stationary = flipped.stationaryYPoints(t0, t1);

    double segments[4];
    LPoint<double> points[4];
    points[0] = unflipped.pt1();
    segments[0] = 0;

    int segmentCount = 0;
    if (stationary > 0) {
        ++segmentCount;
        segments[segmentCount] = t0;
        points[segmentCount] = unflipped.pointAt(t0);
    }
    if (stationary > 1) {
        ++segmentCount;
        segments[segmentCount] = t1;
        points[segmentCount] = unflipped.pointAt(t1);
    }
    ++segmentCount;
    segments[segmentCount] = 1;
    points[segmentCount] = unflipped.pt4();

    double lastIntersection = 0;
    for (int i = 0; i < segmentCount; ++i) {
        outA = compare<edge>(points[i], t);
        outB = compare<edge>(points[i+1], t);

        if (outA != outB) {
            double intersection = flipped.tForY(segments[i], segments[i+1], t);

            if (outB){
                addBezier(result, unflipped.getSubRange(lastIntersection, intersection));
            }
            lastIntersection = intersection;
        }
    }

    if (!outB){
        addBezier(result, unflipped.getSubRange(lastIntersection, 1));
    }
}

// clips a single subpath against a single edge
template <Edge edge>
std::shared_ptr<LPainterPath> clip(const std::shared_ptr<LPainterPath> path, double t)
{
    std::shared_ptr<LPainterPath> result = std::make_shared<LPainterPath>();
    for (int i = 1; i < path->elementCount(); ++i) {
        const LPainterPath::Element &element = path->elementAt(i);
        if (element.isLineTo()) {
            clipLine<edge>(path->elementAt(i-1).toPointF(),
                           path->elementAt(i).toPointF(),
                           t,
                           result);
        } else {
            clipBezier<edge>(path->elementAt(i-1).toPointF(),
                             path->elementAt(i).toPointF(),
                             path->elementAt(i+1).toPointF(),
                             path->elementAt(i+2).toPointF(),
                             t,
                             result);
            i += 2;
        }
    }

    int last = path->elementCount() - 1;
    if (path->elementAt(last).toPointF() != path->elementAt(0).toPointF()){
        clipLine<edge>(path->elementAt(last).toPointF(),
                       path->elementAt(0).toPointF(),
                       t,
                       result);
    }
    return result;
}

struct TreeNode
{
    double splitLeft;
    double splitRight;
    bool leaf;

    int lowestLeftIndex;
    int lowestRightIndex;

    union {
        struct {
            int first;
            int last;
        } interval;
        struct {
            int left;
            int right;
        } children;
    } index;
};

struct RectF
{
    double x1;
    double y1;
    double x2;
    double y2;
};


class SegmentTree
{
public:
    SegmentTree(LPathSegments &segments);

    void produceIntersections(int segment);

private:
    TreeNode buildTree(int first, int last, int depth, const RectF &bounds);

    void produceIntersectionsLeaf(const TreeNode &node, int segment);
    void produceIntersections(const TreeNode &node, int segment, const RectF &segmentBounds, const RectF &nodeBounds, int axis);
    void intersectLines(const LLine<double> &a,
                        const LLine<double> &b,
                        std::vector<LIntersection> &intersections);

    LPathSegments &m_segments;
    std::vector<int> m_index;

    RectF m_bounds;

    std::vector<TreeNode> m_tree;
    std::vector<LIntersection> m_intersections;
};

SegmentTree::SegmentTree(LPathSegments &segments)
    : m_segments(segments),
    m_intersections(0)
{
    m_bounds.x1 = std::numeric_limits<double>::infinity();
    m_bounds.y1 = std::numeric_limits<double>::infinity();
    m_bounds.x2 = -std::numeric_limits<double>::infinity();
    m_bounds.y2 = -std::numeric_limits<double>::infinity();

    m_index.resize(m_segments.segments());

    for (int i = 0; i < (int)m_index.size(); ++i) {
        m_index[i] = i;

        const LRect<double> &segmentBounds = m_segments.elementBounds(i);

        if (segmentBounds.m_x < m_bounds.x1){
            m_bounds.x1 = segmentBounds.m_x;
        }
        if (segmentBounds.m_y < m_bounds.y1){
            m_bounds.y1 = segmentBounds.m_y;
        }
        if (segmentBounds.x2() > m_bounds.x2){
            m_bounds.x2 = segmentBounds.x2();
        }
        if (segmentBounds.y2() > m_bounds.y2){
            m_bounds.y2 = segmentBounds.y2();
        }
    }

    m_tree.resize(1);

    TreeNode root = buildTree(0, m_index.size(), 0, m_bounds);
    m_tree[0] = root;
}

static inline double coordinate(const LPoint<double> &pos, int axis)
{
    return axis == 0 ? pos.m_x : pos.m_y;
}

TreeNode SegmentTree::buildTree(int first, int last, int depth, const RectF &bounds)
{
    if (depth >= 24 || (last - first) <= 10) {
        TreeNode node = {};
        node.leaf = true;
        node.index.interval.first = first;
        node.index.interval.last = last;

        return node;
    }

    int splitAxis = (depth & 1);

    TreeNode node;
    node.leaf = false;

    double split = 0.5f * ((&bounds.x1)[splitAxis] + (&bounds.x2)[splitAxis]);

    node.splitLeft = (&bounds.x1)[splitAxis];
    node.splitRight = (&bounds.x2)[splitAxis];

    node.lowestLeftIndex = INT_MAX;
    node.lowestRightIndex = INT_MAX;

    const int treeSize = m_tree.size();

    node.index.children.left = treeSize;
    node.index.children.right = treeSize + 1;

    m_tree.resize(treeSize + 2);

    int l = first;
    int r = last - 1;

    // partition into left and right sets
    while (l <= r) {
        const int index = m_index.at(l);
        const LRect<double> &segmentBounds = m_segments.elementBounds(index);

        double lowCoordinate = coordinate(segmentBounds.topLeft(), splitAxis);

        if (coordinate(segmentBounds.center(), splitAxis) < split) {
            double highCoordinate = coordinate(segmentBounds.bottomRight(), splitAxis);
            if (highCoordinate > node.splitLeft){
                node.splitLeft = highCoordinate;
            }
            if (index < node.lowestLeftIndex){
                node.lowestLeftIndex = index;
            }
            ++l;
        } else {
            if (lowCoordinate < node.splitRight){
                node.splitRight = lowCoordinate;
            }
            if (index < node.lowestRightIndex){
                node.lowestRightIndex = index;
            }
            std::swap(m_index[l], m_index[r]);
            --r;
        }
    }

    RectF lbounds = bounds;
    (&lbounds.x2)[splitAxis] = node.splitLeft;

    RectF rbounds = bounds;
    (&rbounds.x1)[splitAxis] = node.splitRight;

    TreeNode left = buildTree(first, l, depth + 1, lbounds);
    m_tree[node.index.children.left] = left;

    TreeNode right = buildTree(l, last, depth + 1, rbounds);
    m_tree[node.index.children.right] = right;

    return node;
}

void SegmentTree::intersectLines(const LLine<double> &a,
                                 const LLine<double> &b,
                                 std::vector<LIntersection> &intersections)
{
    const LPoint<double> p1 = a.m_pt1;
    const LPoint<double> p2 = a.m_pt2;

    const LPoint<double> q1 = b.m_pt1;
    const LPoint<double> q2 = b.m_pt2;

    if ((p1 == p2) || (q1 == q2)){
        return;
    }

    const bool p1EqualsQ1 = p1 == q1;
    const bool p2EqualsQ2 = p2 == q2;

    if (p1EqualsQ1 && p2EqualsQ2){
        return;
    }

    const bool p1EqualsQ2 = p1 == q2;
    const bool p2EqualsQ1 = p2 == q1;

    if (p1EqualsQ2 && p2EqualsQ1){
        return;
    }

    const LPoint<double> pDelta = p2 - p1;
    const LPoint<double> qDelta = q2 - q1;

    const double par = pDelta.m_x * qDelta.m_y - pDelta.m_y * qDelta.m_x;

    if (lFuzzyIsZero(par)) {
        const LPoint<double> normal(-pDelta.m_y, pDelta.m_x);

        // coinciding?
        if (lFuzzyIsZero(LSize<double>::dotProduct(normal, q1 - p1))) {
            const double invDp = 1 / LSize<double>::dotProduct(pDelta, pDelta);

            const double tq1 = LSize<double>::dotProduct(pDelta, q1 - p1) * invDp;
            const double tq2 = LSize<double>::dotProduct(pDelta, q2 - p1) * invDp;

            if (tq1 > 0 && tq1 < 1) {
                LIntersection intersection;
                intersection.alphaA = tq1;
                intersection.alphaB = 0;
                intersection.pos = q1;
                intersections.push_back(intersection);
            }

            if (tq2 > 0 && tq2 < 1) {
                LIntersection intersection;
                intersection.alphaA = tq2;
                intersection.alphaB = 1;
                intersection.pos = q2;
                intersections.push_back(intersection);
            }

            const double invDq = 1 / LSize<double>::dotProduct(qDelta, qDelta);

            const double tp1 = LSize<double>::dotProduct(qDelta, p1 - q1) * invDq;
            const double tp2 = LSize<double>::dotProduct(qDelta, p2 - q1) * invDq;

            if (tp1 > 0 && tp1 < 1) {
                LIntersection intersection;
                intersection.alphaA = 0;
                intersection.alphaB = tp1;
                intersection.pos = p1;
                intersections.push_back(intersection);
            }

            if (tp2 > 0 && tp2 < 1) {
                LIntersection intersection;
                intersection.alphaA = 1;
                intersection.alphaB = tp2;
                intersection.pos = p2;
                intersections.push_back(intersection);
            }
        }
        return;
    }

    // if the lines are not parallel and share a common end point, then they
    // don't intersect
    if (p1EqualsQ1 || p1EqualsQ2 || p2EqualsQ1 || p2EqualsQ2){
        return;
    }


    const double tp = (qDelta.m_y * (q1.m_x - p1.m_x) -
                      qDelta.m_x * (q1.m_y - p1.m_y)) / par;
    const double tq = (pDelta.m_y * (q1.m_x - p1.m_x) -
                      pDelta.m_x * (q1.m_y - p1.m_y)) / par;

    if ((tp<0) || (tp>1) || (tq<0) || (tq>1)){
        return;
    }

    const bool pZero = lFuzzyIsZero(tp);
    const bool pOne = lFuzzyIsZero(tp - 1);

    const bool qZero = lFuzzyIsZero(tq);
    const bool qOne = lFuzzyIsZero(tq - 1);

    if ((qZero || qOne) && (pZero || pOne)){
        return;
    }

    LPoint<double> pt;
    if (pZero) {
        pt = p1;
    } else if (pOne) {
        pt = p2;
    } else if (qZero) {
        pt = q1;
    } else if (qOne) {
        pt = q2;
    } else {
        pt = q1 + (q2 - q1) * tq;
    }

    LIntersection intersection;
    intersection.alphaA = tp;
    intersection.alphaB = tq;
    intersection.pos = pt;
    intersections.push_back(intersection);
}

void SegmentTree::produceIntersections(int segment)
{
    const LRect<double> &segmentBounds = m_segments.elementBounds(segment);

    RectF sbounds;
    sbounds.x1 = segmentBounds.m_x;
    sbounds.y1 = segmentBounds.m_y;
    sbounds.x2 = segmentBounds.x2();
    sbounds.y2 = segmentBounds.y2();

    produceIntersections(m_tree.at(0), segment, sbounds, m_bounds, 0);
}

void SegmentTree::produceIntersectionsLeaf(const TreeNode &node, int segment)
{
    const LRect<double> &r1 = m_segments.elementBounds(segment);
    const LLine<double> lineA = m_segments.lineAt(segment);

    for (int i = node.index.interval.first; i < node.index.interval.last; ++i) {
        const int other = m_index.at(i);
        if (other >= segment){
            continue;
        }

        const LRect<double> &r2 = m_segments.elementBounds(other);

        if (r1.m_x > r2.x2() || r2.m_x > r1.x2()){
            continue;
        }
        if (r1.m_y > r2.y2() || r2.m_y > r1.y2()){
            continue;
        }

        m_intersections.clear();

        const LLine<double> lineB = m_segments.lineAt(other);

        intersectLines(lineA, lineB, m_intersections);

        for (int k = 0; k < (int)m_intersections.size(); ++k) {
            LPathSegments::Intersection i_isect, j_isect;
            i_isect.t = m_intersections.at(k).alphaA;
            j_isect.t = m_intersections.at(k).alphaB;

            i_isect.vertex = j_isect.vertex = m_segments.addPoint(m_intersections.at(k).pos);

            i_isect.next = 0;
            j_isect.next = 0;

            m_segments.addIntersection(segment, i_isect);
            m_segments.addIntersection(other, j_isect);
        }
    }
}

void SegmentTree::produceIntersections(const TreeNode &node, int segment, const RectF &segmentBounds, const RectF &nodeBounds, int axis)
{
    if (node.leaf) {
        produceIntersectionsLeaf(node, segment);
        return;
    }

    RectF lbounds = nodeBounds;
    (&lbounds.x2)[axis] = node.splitLeft;

    RectF rbounds = nodeBounds;
    (&rbounds.x1)[axis] = node.splitRight;

    if (segment > node.lowestLeftIndex && (&segmentBounds.x1)[axis] <= node.splitLeft){
        produceIntersections(m_tree.at(node.index.children.left), segment, segmentBounds, lbounds, !axis);
    }

    if (segment > node.lowestRightIndex && (&segmentBounds.x2)[axis] >= node.splitRight){
        produceIntersections(m_tree.at(node.index.children.right), segment, segmentBounds, rbounds, !axis);
    }
}

} //end namespace lClip


void LIntersectionFinder::produceIntersections(LPathSegments &segments)
{
    lClip::SegmentTree tree(segments);

    for (int i = 0; i < segments.segments(); ++i){
        tree.produceIntersections(i);
    }
}

std::shared_ptr<LPainterPath> LPathClipper::intersect(const std::shared_ptr<LPainterPath> path, const LRect<double> &rect)
{
    std::vector<std::shared_ptr<LPainterPath> > subpaths = lClip::toSubpaths(path);

    std::shared_ptr<LPainterPath> result = std::make_shared<LPainterPath>();
    result->setFillRule(path->fillRule());
    for (int i = 0; i < (int)subpaths.size(); ++i) {
        std::shared_ptr<LPainterPath> subPath = subpaths.at(i);
        LRect<double> bounds = subPath->boundingRect();
        if (bounds.isIntersects(rect)) {
            if (bounds.m_x < rect.m_x){
                subPath = lClip::clip<lClip::Left>(subPath, rect.m_x);
            }
            if (bounds.x2() > rect.x2()){
                subPath = lClip::clip<lClip::Right>(subPath, rect.x2());
            }

            bounds = subPath->boundingRect();

            if (bounds.m_y < rect.m_y){
                subPath = lClip::clip<lClip::Top>(subPath, rect.m_y);
            }
            if (bounds.y2() > rect.y2()){
                subPath = lClip::clip<lClip::Bottom>(subPath, rect.y2());
            }

            if (subPath->elementCount() > 1){
                result->addPath(subPath);
            }
        }
    }
    // The algorithm above might return one side of \a rect if there was no intersection,
    // so only return intersections that are not empty rectangles.
    if (result->boundingRect().isEmpty()){
        return std::make_shared<LPainterPath>();
    }
    else{
        return result;
    }
}

void LPathSegments::setPath(const std::shared_ptr<LPainterPath> path)
{
    m_points.clear();
    m_intersections.clear();
    m_segments.clear();

    m_pathId = 0;

    addPath(path);
}

void LPathSegments::addPath(const std::shared_ptr<LPainterPath> path)
{
    if(path == nullptr){
        return;
    }

    int firstSegment = m_segments.size();

    bool hasMoveTo = false;
    int lastMoveTo = 0;
    int last = 0;
    for (int i = 0; i < path->elementCount(); ++i) {
        int current = m_points.size();

        LPoint<double> currentPoint;
        if (path->elementAt(i).m_type == LPainterPath::CurveToElement){
            currentPoint = path->elementAt(i+2).toPointF();
        }
        else{
            currentPoint = path->elementAt(i).toPointF();
        }

        if (i > 0 && (m_points.at(lastMoveTo) == currentPoint)){
            current = lastMoveTo;
        }
        else{
            m_points.push_back(currentPoint);
        }

        switch (path->elementAt(i).m_type) {
        case LPainterPath::MoveToElement:{
            if (hasMoveTo && last != lastMoveTo && (m_points.at(last) != m_points.at(lastMoveTo))){
                m_segments.push_back(Segment(m_pathId, last, lastMoveTo));
            }
            hasMoveTo = true;
            last = lastMoveTo = current;
            break;
        }
        case LPainterPath::LineToElement:{
            m_segments.push_back(Segment(m_pathId, last, current));
            last = current;
            break;
        }
        case LPainterPath::CurveToElement:
        {
            LBezier bezier = LBezier::fromPoints(m_points.at(last),
                                                 path->elementAt(i).toPointF(),
                                                 path->elementAt(i+1).toPointF(),
                                                 path->elementAt(i+2).toPointF());
            if (isLine(bezier)) {
                m_segments.push_back(Segment(m_pathId, last, current));
            } else {
                LRect<double> bounds = bezier.bounds();
                double threshold = std::floor(std::min<double>(64, std::max<double>(bounds.width(), bounds.height()) * (std::numbers::pi/3)));

                if (threshold < 3){
                    threshold = 3;
                }
                double oneOverThresholdMinus_1 = double(1) / (threshold - 1);

                for (int t = 1; t < threshold - 1; ++t) {
                    currentPoint = bezier.pointAt(t * oneOverThresholdMinus_1);

                    int index = m_points.size();
                    m_segments.push_back(Segment(m_pathId, last, index));
                    last = index;

                    m_points.push_back(currentPoint);
                }

                m_segments.push_back(Segment(m_pathId, last, current));
            }

            last = current;
            i += 2;
            break;
        }
        default:{
            break;
        }
        }
    }

    if (hasMoveTo && last != lastMoveTo && (m_points.at(last) != m_points.at(lastMoveTo))){
        m_segments.push_back(Segment(m_pathId, last, lastMoveTo));
    }

    for (int i = firstSegment; i < (int)m_segments.size(); ++i) {
        const LLine<double> line = lineAt(i);

        double x1 = line.m_pt1.m_x;
        double y1 = line.m_pt1.m_y;
        double x2 = line.m_pt2.m_x;
        double y2 = line.m_pt2.m_y;

        if (x2 < x1){
            std::swap(x1, x2);
        }
        if (y2 < y1){
            std::swap(y1, y2);
        }
        m_segments.at(i).bounds = LRect<double>(x1, y1, x2 - x1, y2 - y1, false);
    }
    ++m_pathId;
}


class LKdPointTree
{
public:
    enum Traversal {
        TraverseBoth,
        TraverseLeft,
        TraverseRight,
        TraverseNone
    };

    struct Node {
        int point;
        int id;

        Node *left;
        Node *right;
    };

    LKdPointTree(const LPathSegments &segments)
        : m_segments(&segments)
        , m_nodes(m_segments->points())
        , m_id(0)
    {
        m_nodes.resize(m_segments->points());

        for (int i = 0; i < (int)m_nodes.size(); ++i) {
            m_nodes.at(i).point = i;
            m_nodes.at(i).id = -1;
        }

        m_rootNode = build(0, m_nodes.size());
    }

    int build(int begin, int end, int depth = 0);

    Node *rootNode()
    {
        return &m_nodes.at(m_rootNode);
    }

    inline int nextId()
    {
        return m_id++;
    }

private:
    const LPathSegments *m_segments;
    std::vector<Node> m_nodes;

    int m_rootNode;
    int m_id;
};

template <typename T>
void lTraverseKdPointTree(LKdPointTree::Node &node, T &t, int depth = 0)
{
    LKdPointTree::Traversal status = t(node, depth);

    const bool traverseRight = (status == LKdPointTree::TraverseBoth || status == LKdPointTree::TraverseRight);
    const bool traverseLeft = (status == LKdPointTree::TraverseBoth || status == LKdPointTree::TraverseLeft);

    if (traverseLeft && node.left){
        lTraverseKdPointTree<T>(*node.left, t, depth + 1);
    }

    if (traverseRight && node.right){
        lTraverseKdPointTree<T>(*node.right, t, depth + 1);
    }
}

static inline double component(const LPoint<double> &point, unsigned int i)
{
    const double components[] = { point.m_x, point.m_y };
    return components[i];
}

int LKdPointTree::build(int begin, int end, int depth)
{
    const double pivot = component(m_segments->pointAt(m_nodes.at(begin).point), depth & 1);

    int first = begin + 1;
    int last = end - 1;

    while (first <= last) {
        const double value = component(m_segments->pointAt(m_nodes.at(first).point), depth & 1);

        if (value < pivot){
            ++first;
        }
        else {
            std::swap(m_nodes.at(first), m_nodes.at(last));
            --last;
        }
    }

    std::swap(m_nodes.at(last), m_nodes.at(begin));

    if (last > begin){
        m_nodes.at(last).left = &m_nodes.at(build(begin, last, depth + 1));
    }
    else{
        m_nodes.at(last).left = nullptr;
    }

    if (last + 1 < end){
        m_nodes.at(last).right = &m_nodes.at(build(last + 1, end, depth + 1));
    }
    else{
        m_nodes.at(last).right = nullptr;
    }
    return last;
}

class LKdPointFinder
{
public:
    LKdPointFinder(int point, const LPathSegments &segments, LKdPointTree &tree)
        : m_result(-1)
        , m_segments(&segments)
        , m_tree(&tree)
    {
        pointComponents[0] = segments.pointAt(point).m_x;
        pointComponents[1] = segments.pointAt(point).m_y;
    }

    inline LKdPointTree::Traversal operator()(LKdPointTree::Node &node, int depth)
    {
        if (m_result != -1){
            return LKdPointTree::TraverseNone;
        }

        const LPoint<double> &nodePoint = m_segments->pointAt(node.point);
        const double pivotComponents[] = { nodePoint.m_x, nodePoint.m_y };

        const double pivot = pivotComponents[depth & 1];
        const double value = pointComponents[depth & 1];

        if (lFuzzyIsZero(pivot - value)) {
            const double pivot2 = pivotComponents[(depth + 1) & 1];
            const double value2 = pointComponents[(depth + 1) & 1];

            if (lFuzzyIsZero(pivot2 - value2)) {
                if (node.id < 0){
                    node.id = m_tree->nextId();
                }
                m_result = node.id;
                return LKdPointTree::TraverseNone;
            } else
                return LKdPointTree::TraverseBoth;
        } else if (value < pivot) {
            return LKdPointTree::TraverseLeft;
        } else {
            return LKdPointTree::TraverseRight;
        }
    }

    int result() const{
        return m_result;
    }
private:
    double pointComponents[2];
    int m_result;
    const LPathSegments *m_segments;
    LKdPointTree *m_tree;
};

void LPathSegments::mergePoints()
{
    LKdPointTree tree(*this);

    if (tree.rootNode()) {
        std::vector<LPoint<double> > mergedPoints;
        mergedPoints.reserve(points());
        std::vector<int> pointIndices;
        pointIndices.reserve(points());

        for (int i = 0; i < points(); ++i) {
            LKdPointFinder finder(i, *this, tree);
            lTraverseKdPointTree<LKdPointFinder>(*tree.rootNode(), finder);

            if (finder.result() >= (int)mergedPoints.size()){
                mergedPoints.push_back(m_points.at(i));
            }
            pointIndices.push_back(finder.result());
        }

        for (int i = 0; i < (int)m_segments.size(); ++i) {
            m_segments.at(i).va = pointIndices.at(m_segments.at(i).va);
            m_segments.at(i).vb = pointIndices.at(m_segments.at(i).vb);
        }

        for (int i = 0; i < (int)m_intersections.size(); ++i){
            m_intersections.at(i).vertex = pointIndices.at(m_intersections.at(i).vertex);
        }
        m_points.swap(mergedPoints);
    }
}


LK_NAMESPACE_END
