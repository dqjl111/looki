#ifndef LSTROKER_H
#define LSTROKER_H
#include "lkgui/paint/lpainterpath.h"
#include "lkcore/livar.h"

LK_NAMESPACE_BEGIN
class LIStrokeHooker{
public:
    LIStrokeHooker();
    virtual ~LIStrokeHooker();
    virtual void moveTo(double x, double y) = 0;
    virtual void lineTo(double x, double y) = 0;
    virtual void cubicTo(double c1x, double c1y,
                             double c2x, double c2y,
                             double ex, double ey)=0;
    virtual void begin(){}
    virtual void end(){}
};

class LStrokeHooker : public LIStrokeHooker{
public:
    LStrokeHooker(bool isFast = true);
    virtual void moveTo(double x, double y);
    inline void moveTo(const LPoint<double> &p){
        moveTo(p.m_x, p.m_y);
    }

    virtual void lineTo(double x, double y);
    inline void lineTo(const LPoint<double> &p){
        lineTo(p.m_x, p.m_y);
    }


    virtual void cubicTo(double c1x, double c1y,
                         double c2x, double c2y,
                         double ex, double ey);
    inline void cubicTo(const LPoint<double> &ctrlPt1,
                         const LPoint<double> &ctrlPt2,
                         const LPoint<double> &endPt){
        cubicTo(ctrlPt1.m_x, ctrlPt1.m_y, ctrlPt2.m_x, ctrlPt2.m_y, endPt.m_x, endPt.m_y);
    }


    virtual void begin(){
        reset();
    }

    virtual void end(){
        if(!m_strokedPath->isEmpty()){
            m_strokedPath->setDirty(true);
        }
    }


    inline std::shared_ptr<LPainterPath> strokedPath() const{
        return m_strokedPath;
    }
    inline void reset(){
        m_strokedPath->clear();
    }

    inline bool isEmpty(){
        return m_strokedPath->isEmpty();
    }
    inline void setHints(uint32_t hints){
        m_strokedPath->m_dirtyHints = false;
        m_strokedPath->m_hints = hints;
    }

    bool m_isFast = true;
protected:
    void addElement(const LPainterPath::Element& ele);
    std::shared_ptr<LPainterPath> m_strokedPath;

};


class LStrokerOps
{
public:
    LStrokerOps();
    virtual ~LStrokerOps();

    virtual void begin(std::shared_ptr<LIStrokeHooker> hooker);
    virtual void end();

    inline void moveTo(double x, double y){
        if (m_elements.size()>1){
            processCurrentSubpath();
        }
        m_elements.clear();
        m_elements.push_back(LPainterPath::Element{x, y, LPainterPath::MoveToElement});
    }
    inline void lineTo(double x, double y){
        m_elements.push_back(LPainterPath::Element{x, y, LPainterPath::LineToElement});
    }
    inline void cubicTo(double c1x, double c1y,
                        double c2x, double c2y,
                        double ex, double ey){

        m_elements.push_back(LPainterPath::Element{c1x, c1y, LPainterPath::CurveToElement});
        m_elements.push_back(LPainterPath::Element{c2x, c2y, LPainterPath::CurveToDataElement});
        m_elements.push_back(LPainterPath::Element{ex, ey, LPainterPath::CurveToDataElement});
    }

    void setCurveThresholdFromTransform(const LTransform2D &transform);
    uint32_t strokePath(const std::shared_ptr<LPainterPath> path,
                    std::shared_ptr<LStrokeHooker> hooker,
                    const LTransform2D &matrix);

    LRect<double> m_clipRect;
    double m_curveThreshold;
protected:
    inline void emitMoveTo(double x, double y){
        m_hooker->moveTo(x, y);
    }
    inline void emitLineTo(double x, double y){
        m_hooker->lineTo(x, y);
    }
    inline void emitCubicTo(double c1x, double c1y, double c2x, double c2y, double ex, double ey){
        m_hooker->cubicTo(c1x, c1y, c2x, c2y, ex, ey);
    }

    virtual void processCurrentSubpath() = 0;
    std::vector<LPainterPath::Element> m_elements;

    double m_dashThreshold;
    std::shared_ptr<LIStrokeHooker> m_hooker;
};

class LPainterPathStroker;
class LStroker : public LStrokerOps
{
public:
    LStroker();

    enum LineJoinMode {
        FlatJoin,
        SquareJoin,
        MiterJoin,
        RoundJoin,
        RoundCap,
        SvgMiterJoin
    };


    double m_strokeWidth;
    double m_miterLimit;

    double m_back1X;
    double m_back1Y;

    double m_back2X;
    double m_back2Y;


    static LPenCapStyle capForJoinMode(LineJoinMode mode);
    static LineJoinMode joinModeForCap(LPenCapStyle);

    static LPenJoinStyle joinForJoinMode(LineJoinMode mode);
    static LineJoinMode joinModeForJoin(LPenJoinStyle joinStyle);

    void setCapStyle(LPenCapStyle capStyle) { m_capStyle = joinModeForCap(capStyle); }
    LPenCapStyle capStyle() const { return capForJoinMode(m_capStyle); }
    inline LineJoinMode capStyleMode() const { return m_capStyle; }

    void setJoinStyle(LPenJoinStyle style) { m_joinStyle = joinModeForJoin(style); }
    LPenJoinStyle joinStyle() const { return joinForJoinMode(m_joinStyle); }
    inline  LineJoinMode joinStyleMode() const { return m_joinStyle; }

    void joinPoints(double x, double y, const LLine<double> &nextLine, LineJoinMode join);
    inline void emitMoveTo(double x, double y){
        m_back2X = m_back1X;
        m_back2Y = m_back1Y;
        m_back1X = x;
        m_back1Y = y;
        LStrokerOps::emitMoveTo(x, y);
    }
    inline void emitLineTo(double x, double y){
        m_back2X = m_back1X;
        m_back2Y = m_back1Y;
        m_back1X = x;
        m_back1Y = y;
        LStrokerOps::emitLineTo(x, y);
    }
    inline void emitCubicTo(double c1x, double c1y, double c2x, double c2y, double ex, double ey){
        if (c2x == ex && c2y == ey) {
            if (c1x == ex && c1y == ey) {
                m_back2X = m_back1X;
                m_back2Y = m_back1Y;
            } else {
                m_back2X = c1x;
                m_back2Y = c1y;
            }
        } else {
            m_back2X = c2x;
            m_back2Y = c2y;
        }
        m_back1X = ex;
        m_back1Y = ey;
        LStrokerOps::emitCubicTo(c1x, c1y, c2x, c2y, ex, ey);
    }
protected:
    virtual void processCurrentSubpath() override;
    LineJoinMode m_capStyle;
    LineJoinMode m_joinStyle;

    friend class LPainterPathStroker;
};

class LDashStroker;
class LDashStrokeHooker : public LIStrokeHooker
{
public:
    LDashStrokeHooker(std::shared_ptr<LStroker> stroker);

    virtual void moveTo(double x, double y);
    virtual void lineTo(double x, double y);
    virtual void cubicTo(double c1x, double c1y,
                         double c2x, double c2y,
                         double ex, double ey);
private:
    std::shared_ptr<LStroker> m_stroker;
    friend class LDashStroker;
};


class LDashStroker : public LStrokerOps
{
public:
    LDashStroker(std::shared_ptr<LStroker> stroker);

    inline std::shared_ptr<LStroker> stroker() const { return hooker()->m_stroker; }
    void begin(std::shared_ptr<LIStrokeHooker> data) override;
    void end() override;

    std::vector<double> m_dashPattern;
    double m_dashOffset;

    double m_strokeWidth;
    double m_miterLimit;


    static std::vector<double> patternForStyle(LPenStyle style);
    static int repetitionLimit() { return 10000; }
protected:
    inline std::shared_ptr<LDashStrokeHooker> hooker() const{
        return std::dynamic_pointer_cast<LDashStrokeHooker>(m_hooker);
    }
    void processCurrentSubpath() override;
};

class LPen;
class LKGUIDEF LPainterPathStroker
{
public:
    LPainterPathStroker();
    explicit LPainterPathStroker(const std::shared_ptr<LPen> pen);

    void setWidth(double width);
    double width() const;

    void setCapStyle(LPenCapStyle style);
    LPenCapStyle capStyle() const;

    void setJoinStyle(LPenJoinStyle style);
    LPenJoinStyle joinStyle() const;

    void setMiterLimit(double length);
    double miterLimit() const;

    void setCurveThreshold(double threshold);
    double curveThreshold() const;

    void setDashPattern(LPenStyle);
    void setDashPattern(const std::vector<double> &dashPattern);
    const std::vector<double>& dashPattern() const;

    void setDashOffset(double offset);
    double dashOffset() const;

    std::shared_ptr<LPainterPath> createStroke(const std::shared_ptr<LPainterPath> path) const;
private:
    std::shared_ptr<LStroker> m_stroker;
    std::shared_ptr<LStrokeHooker> m_hooker;
    std::vector<double> m_dashPattern;
    double m_dashOffset;
};

LK_NAMESPACE_END
#endif // LSTROKER_H
