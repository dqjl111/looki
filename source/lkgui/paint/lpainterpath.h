#ifndef LPAINTERPATH_H
#define LPAINTERPATH_H
#include "lkgui/lrect.h"
#include <vector>
#include <list>
#include "lkcore/livar.h"
#include "lkgui/paint/lpolygon.h"
#include "lkgui/paint/ltransform2d.h"

LK_NAMESPACE_BEGIN
#define LK_PATH_KAPPA 0.5522847498

//class LVectorPathConverter;
class LStrokeHooker;
class LKGUIDEF LPainterPath : public LIVar
{
public:
    LPainterPath(std::shared_ptr<LPainterPath> p = nullptr, bool isOutline= false, bool disableCurve=false, double curveThres=0.25);

    explicit LPainterPath(const LPoint<double> &startPoint, bool isOutline= false, bool disableCurve=false, double curveThres=0.25);
    LPainterPath(const LPainterPath &other, bool isOutline= false, bool disableCurve=false, double curveThres=0.25);
    LPainterPath &operator=(const LPainterPath &other);
    ~LPainterPath();


    enum ElementType {
        MoveToElement,
        LineToElement,
        CurveToElement,
        CurveToDataElement
    };

    class Element {
    public:
        double m_x;
        double m_y;
        ElementType m_type;

        bool isMoveTo() const { return m_type == MoveToElement; }
        bool isLineTo() const { return m_type == LineToElement; }
        bool isCurveTo() const { return m_type == CurveToElement; }

        LPoint<double> toPointF() const { return LPoint<double>(m_x, m_y); }
        bool operator==(const Element &e) const { return lFuzzyIsZero(m_x-e.m_x)
                                                         && lFuzzyIsZero(m_y-e.m_y) && m_type == e.m_type; }
        inline bool operator!=(const Element &e) const { return !operator==(e); }
    };

    bool isEmpty() const;
    void clear();
    int elementCount() const{
        return (int)m_elements.size();
    }

    inline const std::vector<Element>& elements() const{
        return m_elements;
    }

    inline const Element& elementAt(int i) const{
        return m_elements.at(i);
    }

    bool isRect() const;
    static bool isRect(const LPoint<double>* pt, int count);

    void addRect(const LRect<double> &rect);
    inline void addRect(double x, double y, double w, double h)
    {
        addRect(LRect<double>(x, y, w, h, false));
    }

    void addEllipse(const LRect<double> &rect);
    inline void addEllipse(double x, double y, double w, double h){
        addEllipse(LRect<double>(x, y, w, h, false));
    }
    void addEllipse(const LPoint<double> &center, double rx, double ry){
        addEllipse(LRect<double>(center.m_x - rx, center.m_y - ry, 2 * rx, 2 * ry, false));
    }

    void addPolygon(const LPolygon<double> &polygon);
    void addPolygon(const LPoint<double> *points, int pointCount);
    void addLine(const LLine<double> line);
    void addLine(const LLine<double>* lines, int lineCount);

    void addPath(const std::shared_ptr<LPainterPath> path);
    void addRegion(const std::shared_ptr<LRegion> region);

    static bool ajustRounedRectRadius(const LRect<double> &rect, double& xRadius, double& yRadius, LSizeMode mode);
    void addRoundedRect(const LRect<double> &rect,
                        double xRadius,
                        double yRadius,
                        LSizeMode mode = LSizeMode::Absolute);

    static std::vector<LPoint<double> > curvesForArc(const LRect<double> &rect, double startAngle, double sweepLength);
    static void findEllipseCoords(const LRect<double> &r,
                               double angle,
                               double length,
                               LPoint<double>& start,
                               LPoint<double>& end);

    void moveTo(const LPoint<double> &p);
    inline void moveTo(double x, double y)
    {
        moveTo(LPoint<double>(x, y));
    }

    void lineTo(const LPoint<double> &p);
    inline void lineTo(double x, double y)
    {
        lineTo(LPoint<double>(x, y));
    }

    void quadTo(const LPoint<double> &ctrlPt, const LPoint<double> &endPt);
    inline void quadTo(double ctrlPtx, double ctrlPty, double endPtx, double endPty){
        quadTo(LPoint<double>(ctrlPtx, ctrlPty), LPoint<double>(endPtx, endPty));
    }

    void arcMoveTo(const LRect<double> &rect, double angle);
    inline void arcMoveTo(double x, double y, double w, double h, double angle);

    void arcTo(const LRect<double> &rect, double startAngle, double arcLength);
    inline void arcTo(double x, double y, double w, double h, double startAngle, double arcLength);

    void cubicTo(const LPoint<double> &ctrlPt1, const LPoint<double> &ctrlPt2, const LPoint<double> &endPt);

    bool isClosed() const;
    void close();
    void closeSubpath();

    std::list<LPolygon<double> > toSubpathPolygons(const LTransform2D &matrix = LTransform2D()) const;
    std::list<LPolygon<double> > toFillPolygons(const LTransform2D &matrix = LTransform2D()) const;
    LPolygon<double> toFillPolygon(const LTransform2D &matrix = LTransform2D()) const;

    LFillRule fillRule() const;
    void setFillRule(LFillRule fillRule);

    LRect<double> boundingRect() const;
    LRect<double> controlPointRect() const;

    void translate(double dx, double dy);
    inline void translate(const LPoint<double> &offset)
    { translate(offset.m_x, offset.m_y); }

    void setDirty(bool);
    void setElementPositionAt(int i, double x, double y);

    inline bool isOutline() const{
        return m_isOutline;
    }

    inline bool isDisableCurve() const{
        return m_disableCurve;
    }

    static std::vector<Element> toElements(const std::vector<Element>& org, int& subStart, bool isOutline= false, bool disableCurve =false, double curveThreshold=0.25);

    //路径转为轮廓，需要关闭曲线，以保证填充奇偶正确
    std::shared_ptr<LPainterPath> toOutline();

    std::list<std::shared_ptr<LPainterPath>> toSubpathes();

    enum Hint {
        // Shape hints, in 0x000000ff, access using shape()
        AreaShapeMask           = 0x0001,       // shape covers an area
        NonConvexShapeMask      = 0x0002,       // shape is not convex
        CurvedShapeMask         = 0x0004,       // shape contains curves...
        LinesShapeMask          = 0x0008,
        RectangleShapeMask      = 0x0010,
        ShapeMask               = 0x001f,

        // Shape hints merged into basic shapes..
        LinesHint               = LinesShapeMask,
        RectangleHint           = AreaShapeMask | RectangleShapeMask,
        EllipseHint             = AreaShapeMask | CurvedShapeMask,
        ConvexPolygonHint       = AreaShapeMask,
        PolygonHint             = AreaShapeMask | NonConvexShapeMask,
        RoundedRectHint         = AreaShapeMask | CurvedShapeMask,
        ArbitraryShapeHint      = AreaShapeMask | NonConvexShapeMask | CurvedShapeMask,
    };

    const uint32_t& hints() const;
    inline Hint shape() const { return (Hint) (hints() & ShapeMask); }

    bool contains(const LPoint<double> &pt) const;
    bool contains(const LRect<double> &rect) const;
    bool intersects(const LRect<double> &rect) const;

    bool intersects(const std::shared_ptr<LPainterPath> p) const;
    bool contains(const std::shared_ptr<LPainterPath> p) const;

    std::shared_ptr<LPainterPath> simplified() const;
    std::shared_ptr<LPainterPath> united(const std::shared_ptr<LPainterPath> r) const;
    std::shared_ptr<LPainterPath> intersected(const std::shared_ptr<LPainterPath> r) const;
    std::shared_ptr<LPainterPath> subtracted(const std::shared_ptr<LPainterPath> r) const;

    inline double curveThreshold() const{
        return m_curveThreshold;
    }

    inline void setCurveThreshold(double v){
        m_curveThreshold = v;
    }

    double   length() const;
    double   percentAtLength(double t) const;
    LPoint<double> pointAtPercent(double t) const;
    double   angleAtPercent(double t) const;
    double   slopeAtPercent(double t) const;

    static void  findEllipseCoords(const LRect<double> &r, double angle, double length,
                                  LPoint<double>* startPoint, LPoint<double> *endPoint);
    static LPoint<double> findArcPos(const LPoint<double>& center, double xRadius, double yRadius, double angle);

    static double forArcAngleT(double angle);


    static LPolygon<double> united(const LPolygon<double> &p1, const LPolygon<double> &p2);
    static LPolygon<double> intersected(const LPolygon<double> &p1, const LPolygon<double> &p2);
    static LPolygon<double> subtracted(const LPolygon<double> &p1, const LPolygon<double> &p2);


    /*!
    \internal

    Creates a number of curves for a given arc definition. The arc is
    defined an arc along the ellipses that fits into \a rect starting
    at \a startAngle and an arc length of \a sweepLength.

    The function has three out parameters. The return value is the
    starting point of the arc. The \a curves[15] array represents the list
    of cubicTo elements up to a maximum of \a point_count. There are of course
    3 points pr curve.
    */
    static LPoint<double> curvesForArc(const LRect<double> &rect, double startAngle, double sweepLength,
                                  LPoint<double> *curves, int *point_count);
protected:
    void maybeMoveTo();
    void ensureData();
    void resetMark();
    void computeBoundingRect();
    void computeControlPointRect();
    bool checkRectShape();
    void updateHints();

    std::vector<Element> m_elements;

    uint32_t m_hints = 0;
    int m_cStart=0;
    double m_curveThreshold = 0.25;
    LFillRule m_fillRule = LFillRule::OddEven;

    LRect<double> m_bounds;
    LRect<double> m_controlBounds;

    union{
        uint8_t m_flags = 0;
        struct{
            uint8_t m_requireMoveTo : 1;
            uint8_t m_dirtyBounds : 1;
            uint8_t m_dirtyControlBounds : 1;
            uint8_t m_dirtyHints : 1;
            uint8_t m_convex : 1;
            uint8_t m_isRectInit : 1;
            uint8_t m_isOutline : 1;
            uint8_t m_disableCurve : 1;

        };
    };


    static double tForArcAngle(double angle);

//    std::unique_ptr<LVectorPathConverter> m_pathConverter;
    friend class LTransform2D;
    friend class LStrokeHooker;
};

//inline LPainterPath operator *(const LPainterPath &p, const LTransform2D &m)
//{ return m.map(p); }

LK_NAMESPACE_END

#endif // LPAINTERPATH_H
