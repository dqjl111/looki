#ifndef LPATHCLIPPER_H
#define LPATHCLIPPER_H
#include "lpainterpath.h"

LK_NAMESPACE_BEGIN
class LWingedEdge;
class LPathClipper
{
public:
    LPathClipper(const std::shared_ptr<LPainterPath> subject,
                 const std::shared_ptr<LPainterPath> clip);

    enum Operation {
        BoolAnd,
        BoolOr,
        BoolSub,
        Simplify
    };

    std::shared_ptr<LPainterPath> clip(Operation op = BoolAnd);

    bool intersect();
    bool contains();

    static bool pathToRect(const std::shared_ptr<LPainterPath> path, LRect<double> *rect = nullptr);
    static std::shared_ptr<LPainterPath> intersect(const std::shared_ptr<LPainterPath> path,
                                                   const LRect<double> &rect);

private:
    enum ClipperMode {
        ClipMode, // do the full clip
        CheckMode // for contains/intersects (only interested in whether the result path is non-empty)
    };

    bool handleCrossingEdges(LWingedEdge &list, double y, ClipperMode mode);
    bool doClip(LWingedEdge &list, ClipperMode mode);

    std::shared_ptr<LPainterPath> subjectPath;
    std::shared_ptr<LPainterPath> clipPath;
    Operation op;

    int aMask;
    int bMask;
};

struct LPathVertex
{
public:
    LPathVertex(const LPoint<double> &p = LPoint<double>(), int e = -1);
    LPoint<double> toPointF() const;

    int edge;
    double x;
    double y;
};

class LPathEdge
{
public:
    enum Traversal {
        RightTraversal,
        LeftTraversal
    };

    enum Direction {
        Forward,
        Backward
    };

    enum Type {
        Line,
        Curve
    };

    explicit LPathEdge(int a = -1, int b = -1);

    mutable int flag;

    int windingA;
    int windingB;

    int first;
    int second;

    double angle;
    double invAngle;

    int next(Traversal traversal, Direction direction) const;

    void setNext(Traversal traversal, Direction direction, int next);
    void setNext(Direction direction, int next);

    Direction directionTo(int vertex) const;
    int vertex(Direction direction) const;

private:
    int m_next[2][2];
};

class LPathSegments
{
public:
    struct Intersection {
        double t=0;
        int vertex=0;
        int next=0;

        bool operator<(const Intersection &o) const {
            return t < o.t;
        }
    };

    struct Segment {
        Segment(int pathId=0, int vertexA=0, int vertexB=0)
            : path(pathId)
            , va(vertexA)
            , vb(vertexB)
            , intersection(-1)
        {
        }

        int path=0;
        // vertices
        int va=0;
        int vb=0;
        // intersection index
        int intersection=-1;
        LRect<double> bounds;
    };

    LPathSegments();
    void reserve(int reserve);

    void setPath(const std::shared_ptr<LPainterPath> path);
    void addPath(const std::shared_ptr<LPainterPath> path);

    int intersections() const;
    int segments() const;
    int points() const;

    const Segment &segmentAt(int index) const;
    const LLine<double> lineAt(int index) const;
    const LRect<double> &elementBounds(int index) const;
    int pathId(int index) const;

    const LPoint<double> &pointAt(int vertex) const;
    int addPoint(const LPoint<double> &point);

    const Intersection *intersectionAt(int index) const;
    void addIntersection(int index, const Intersection &intersection);

    void mergePoints();

private:
    std::vector<LPoint<double>> m_points;
    std::vector<Segment> m_segments;
    std::vector<Intersection> m_intersections;

    int m_pathId =0;
};

class LWingedEdge
{
public:
    struct TraversalStatus
    {
        int edge;
        LPathEdge::Traversal traversal;
        LPathEdge::Direction direction;

        void flipDirection();
        void flipTraversal();

        void flip();
    };

    LWingedEdge();
    LWingedEdge(const std::shared_ptr<LPainterPath> subject, const std::shared_ptr<LPainterPath> clip);

    void simplify();
    std::shared_ptr<LPainterPath> toPath() const;

    int edgeCount() const;

    LPathEdge *edge(int edge);
    const LPathEdge *edge(int edge) const;

    int vertexCount() const;

    int addVertex(const LPoint<double> &p);

    LPathVertex *vertex(int vertex);
    const LPathVertex *vertex(int vertex) const;

    TraversalStatus next(const TraversalStatus &status) const;

    int addEdge(const LPoint<double> &a, const LPoint<double> &b);
    int addEdge(int vertexA, int vertexB);

    bool isInside(double x, double y) const;

    static LPathEdge::Traversal flip(LPathEdge::Traversal traversal);
    static LPathEdge::Direction flip(LPathEdge::Direction direction);

private:
    void intersectAndAdd();
    void removeEdge(int ei);

    int insert(const LPathVertex &vertex);
    TraversalStatus findInsertStatus(int vertex, int edge) const;

    double delta(int vertex, int a, int b) const;

    std::vector<LPathEdge> m_edges;
    std::vector<LPathVertex> m_vertices;

    std::vector<double> m_splitPoints;

    LPathSegments m_segments;
};

inline LPathEdge::LPathEdge(int a, int b)
    : flag(0)
    , windingA(0)
    , windingB(0)
    , first(a)
    , second(b)
    , angle(0)
    , invAngle(0)
{
    m_next[0][0] = -1;
    m_next[1][0] = -1;
    m_next[0][0] = -1;
    m_next[1][0] = -1;
}

inline int LPathEdge::next(Traversal traversal, Direction direction) const
{
    return m_next[int(traversal)][int(direction)];
}

inline void LPathEdge::setNext(Traversal traversal, Direction direction, int next)
{
    m_next[int(traversal)][int(direction)] = next;
}

inline void LPathEdge::setNext(Direction direction, int next)
{
    m_next[0][int(direction)] = next;
    m_next[1][int(direction)] = next;
}

inline LPathEdge::Direction LPathEdge::directionTo(int vertex) const
{
    return first == vertex ? Backward : Forward;
}

inline int LPathEdge::vertex(Direction direction) const
{
    return direction == Backward ? first : second;
}

inline LPathVertex::LPathVertex(const LPoint<double> &p, int e)
    : edge(e)
    , x(p.m_x)
    , y(p.m_y)
{
}

inline LPoint<double> LPathVertex::toPointF() const
{
    return LPoint<double>(x, y);
}

inline LPathSegments::LPathSegments() {
}

inline void LPathSegments::reserve(int reserve){
    m_points.reserve(reserve);
    m_segments.reserve(reserve);
    m_intersections.reserve(reserve);
}

inline int LPathSegments::segments() const
{
    return m_segments.size();
}

inline int LPathSegments::points() const
{
    return m_points.size();
}

inline const LPoint<double> &LPathSegments::pointAt(int i) const
{
    return m_points.at(i);
}

inline int LPathSegments::addPoint(const LPoint<double> &point)
{
    m_points.push_back(point);
    return m_points.size() - 1;
}

inline const LPathSegments::Segment &LPathSegments::segmentAt(int index) const
{
    return m_segments.at(index);
}

inline const LLine<double> LPathSegments::lineAt(int index) const
{
    const Segment &segment = m_segments.at(index);
    return LLine<double>(m_points.at(segment.va), m_points.at(segment.vb));
}

inline const LRect<double> &LPathSegments::elementBounds(int index) const
{
    return m_segments.at(index).bounds;
}

inline int LPathSegments::pathId(int index) const
{
    return m_segments.at(index).path;
}

inline const LPathSegments::Intersection *LPathSegments::intersectionAt(int index) const
{
    const int intersection = m_segments.at(index).intersection;
    if (intersection < 0){
        return nullptr;
    }
    else{
        return &m_intersections.at(intersection);
    }
}

inline int LPathSegments::intersections() const
{
    return m_intersections.size();
}

inline void LPathSegments::addIntersection(int index, const Intersection &intersection)
{
    m_intersections.push_back(intersection);

    Segment &segment = m_segments.at(index);
    if (segment.intersection < 0) {
        segment.intersection = m_intersections.size() - 1;
    } else {
        Intersection *isect = &m_intersections.at(segment.intersection);

        while (isect->next != 0){
            isect += isect->next;
        }

        isect->next = (m_intersections.size() - 1) - (isect - m_intersections.data());
    }
}

inline int LWingedEdge::edgeCount() const
{
    return m_edges.size();
}

inline LPathEdge *LWingedEdge::edge(int edge)
{
    return edge < 0 ? nullptr : &m_edges.at(edge);
}

inline const LPathEdge *LWingedEdge::edge(int edge) const
{
    return edge < 0 ? nullptr : &m_edges.at(edge);
}

inline int LWingedEdge::vertexCount() const
{
    return m_vertices.size();
}

inline int LWingedEdge::addVertex(const LPoint<double> &p)
{
    m_vertices.push_back(p);
    return m_vertices.size() - 1;
}

inline LPathVertex *LWingedEdge::vertex(int vertex)
{
    return vertex < 0 ? nullptr : &m_vertices.at(vertex);
}

inline const LPathVertex *LWingedEdge::vertex(int vertex) const
{
    return vertex < 0 ? nullptr : &m_vertices.at(vertex);
}

inline LPathEdge::Traversal LWingedEdge::flip(LPathEdge::Traversal traversal)
{
    return traversal == LPathEdge::RightTraversal ? LPathEdge::LeftTraversal : LPathEdge::RightTraversal;
}

inline void LWingedEdge::TraversalStatus::flipTraversal()
{
    traversal = LWingedEdge::flip(traversal);
}

inline LPathEdge::Direction LWingedEdge::flip(LPathEdge::Direction direction)
{
    return direction == LPathEdge::Forward ? LPathEdge::Backward : LPathEdge::Forward;
}

inline void LWingedEdge::TraversalStatus::flipDirection()
{
    direction = LWingedEdge::flip(direction);
}

inline void LWingedEdge::TraversalStatus::flip()
{
    flipDirection();
    flipTraversal();
}


LK_NAMESPACE_END
#endif // LPATHCLIPPER_H
