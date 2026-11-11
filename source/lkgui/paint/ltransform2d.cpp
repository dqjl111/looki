#include "ltransform2d.h"
#include "lkcore/lglobal.h"
#include "lkgui/paint/lregion.h"
#include "lkgui/paint/lbezier.h"

LK_NAMESPACE_BEGIN

#define L_NEAR_CLIP 0.000001

static inline bool lNeedsPerspectiveClipping(const LRect<double> &rect, const LTransform2D &transform)
{
    const double wx = std::min(transform.m13() * rect.m_x, transform.m13() * rect.x2());
    const double wy = std::min(transform.m23() * rect.m_y, transform.m23() * rect.y2());

    return wx + wy + transform.m33() < L_NEAR_CLIP;
}

struct LHomogeneousCoordinate
{
    double x;
    double y;
    double w;

    LHomogeneousCoordinate() {}
    LHomogeneousCoordinate(double x_, double y_, double w_) : x(x_), y(y_), w(w_) {}

    const LPoint<double> toPointF() const {
        double iw = 1. / w;
        return LPoint<double>(x * iw, y * iw);
    }
};

static inline LHomogeneousCoordinate mapHomogeneous(const LTransform2D &transform, const LPoint<double> &p)
{
    LHomogeneousCoordinate c;
    c.x = transform.m11() * p.m_x + transform.m21() * p.m_y + transform.m31();
    c.y = transform.m12() * p.m_x + transform.m22() * p.m_y + transform.m32();
    c.w = transform.m13() * p.m_x + transform.m23() * p.m_y + transform.m33();
    return c;
}

static inline bool lineToClipped(std::shared_ptr<LPainterPath> path,
                                 const LTransform2D &transform,
                                 const LPoint<double> &a,
                                 const LPoint<double> &b,
                                 bool needsMoveTo,
                                 bool needsLineTo = true)
{
    LHomogeneousCoordinate ha = mapHomogeneous(transform, a);
    LHomogeneousCoordinate hb = mapHomogeneous(transform, b);

    if (ha.w < L_NEAR_CLIP && hb.w < L_NEAR_CLIP){
        return false;
    }

    if (hb.w < L_NEAR_CLIP) {
        const double t = (L_NEAR_CLIP - hb.w) / (ha.w - hb.w);

        hb.x += (ha.x - hb.x) * t;
        hb.y += (ha.y - hb.y) * t;
        hb.w = L_NEAR_CLIP;
    } else if (ha.w < L_NEAR_CLIP) {
        const double t = (L_NEAR_CLIP - ha.w) / (hb.w - ha.w);

        ha.x += (hb.x - ha.x) * t;
        ha.y += (hb.y - ha.y) * t;
        ha.w = L_NEAR_CLIP;

        const LPoint<double> p = ha.toPointF();
        if (needsMoveTo) {
            path->moveTo(p);
            needsMoveTo = false;
        } else {
            path->lineTo(p);
        }
    }

    if (needsMoveTo){
        path->moveTo(ha.toPointF());
    }

    if (needsLineTo){
        path->lineTo(hb.toPointF());
    }

    return true;
}

bool LTransform2D::scaleForTransform(const LTransform2D &transform, double *scale)
{
    const LTransform2D::TransformationType type = transform.type();
    if (type <= LTransform2D::TxTranslate) {
        if (scale){
            *scale = 1;
        }
        return true;
    } else if (type == LTransform2D::TxScale) {
        const double xScale = std::abs(transform.m11());
        const double yScale = std::abs(transform.m22());
        if (scale){
            *scale = std::max(xScale, yScale);
        }
        return lFuzzyIsZero(xScale-yScale);
    }

    // rotate then scale: compare columns
    const double xScale1 = transform.m11() * transform.m11()
                          + transform.m21() * transform.m21();
    const double yScale1 = transform.m12() * transform.m12()
                          + transform.m22() * transform.m22();

    // scale then rotate: compare rows
    const double xScale2 = transform.m11() * transform.m11()
                          + transform.m12() * transform.m12();
    const double yScale2 = transform.m21() * transform.m21()
                          + transform.m22() * transform.m22();

    // decide the order of rotate and scale operations
    if (std::abs(xScale1 - yScale1) > std::abs(xScale2 - yScale2)) {
        if (scale){
            *scale = std::sqrt(std::max(xScale1, yScale1));
        }

        return type == LTransform2D::TxRotate && lFuzzyIsZero(xScale1-yScale1);
    } else {
        if (scale){
            *scale = std::sqrt(std::max(xScale2, yScale2));
        }

        return type == LTransform2D::TxRotate && lFuzzyIsZero(xScale2-yScale2);
    }
}

LCbor LTransform2D::toCbor() const
{
    LCbor::array mat;
    mat.push_back(m_affine.m_11);
    mat.push_back(m_affine.m_12);
    mat.push_back(m_13);
    mat.push_back(m_affine.m_21);
    mat.push_back(m_affine.m_22);
    mat.push_back(m_23);
    mat.push_back(m_affine.m_dx);
    mat.push_back(m_affine.m_dy);
    mat.push_back(m_33);

    LCbor::map ret;
    ret[0] = mat;
    ret[1] = m_type;
    ret[2] = m_dirty;
    return ret;
}

void LTransform2D::fromCbor(const LCbor &c)
{
    LCbor::map m = c.toMap();
    LCbor::array mat = m[0].toArray();
    if(mat.size() == 9){
        m_affine.m_11 = mat[0].toDouble();
        m_affine.m_12 = mat[1].toDouble();
        m_13 = mat[2].toDouble();
        m_affine.m_21 = mat[3].toDouble();
        m_affine.m_22 = mat[4].toDouble();
        m_23 = mat[5].toDouble();
        m_affine.m_dx = mat[6].toDouble();
        m_affine.m_dy = mat[7].toDouble();
        m_33 = mat[8].toDouble();

        m_type = m[1].toUInt64();
        m_dirty = m[1].toUInt64();
    }
}


static inline bool cubicToClipped(std::shared_ptr<LPainterPath> path,
                                  const LTransform2D &transform,
                                  const LPoint<double> &a,
                                  const LPoint<double> &b,
                                  const LPoint<double> &c,
                                  const LPoint<double> &d,
                                  bool needsMoveTo,
                                  double curveThreshold)
{
    LPolygon<double> segment = LBezier::fromPoints(a, b, c, d).toPolygon(curveThreshold);

    for (int i = 0; i < segment.size() - 1; ++i)
        if (lineToClipped(path, transform, segment.at(i), segment.at(i+1), needsMoveTo)){
            needsMoveTo = false;
        }

    return !needsMoveTo;
}



LTransform2D::LTransform2D()
{

}

LTransform2D::LTransform2D(double h11, double h12, double h13,
                           double h21, double h22, double h23,
                           double h31, double h32, double h33)
    : m_affine(h11, h12, h21, h22, h31, h32)
    , m_13(h13), m_23(h23), m_33(h33)
    , m_type(TxNone)
    , m_dirty(TxProject)
{

}

LTransform2D::LTransform2D(double h11, double h12, double h21,
             double h22, double dx, double dy)
    : m_affine(h11, h12, h21, h22, dx, dy)
    , m_13(0), m_23(0), m_33(1)
    , m_type(TxNone)
    , m_dirty(TxShear)
{

}


LTransform2D::TransformationType LTransform2D::type() const
{
    if(m_dirty == TxNone || m_dirty < m_type){
        return static_cast<TransformationType>(m_type);
    }

    switch (static_cast<TransformationType>(m_dirty)) {
    case TxProject:
        if (!lFuzzyIsZero(m_13) || !lFuzzyIsZero(m_23) || !lFuzzyIsZero(m_33 - 1)) {
            m_type = TxProject;
            break;
        }
    case TxShear:
    case TxRotate:
        if (!lFuzzyIsZero(m_affine.m_12) || !lFuzzyIsZero(m_affine.m_21)) {
            const double dot = m_affine.m_11 * m_affine.m_12 + m_affine.m_21 * m_affine.m_22;
            if (lFuzzyIsZero(dot)){
                m_type = TxRotate;
            }
            else{
                m_type = TxShear;
            }
            break;
        }
    case TxScale:
        if (!lFuzzyIsZero(m_affine.m_11 - 1) || !lFuzzyIsZero(m_affine.m_22 - 1)) {
            m_type = TxScale;
            break;
        }
    case TxTranslate:
        if (!lFuzzyIsZero(m_affine.m_dx) || !lFuzzyIsZero(m_affine.m_dy)) {
            m_type = TxTranslate;
            break;
        }
    case TxNone:
        m_type = TxNone;
        break;
    }

    m_dirty = TxNone;
    return static_cast<TransformationType>(m_type);
}

LTransform2D &LTransform2D::translate(double dx, double dy)
{
    if (lFuzzyIsZero(dx) && lFuzzyIsZero(dy)){
        return *this;
    }

    switch(inlineType()) {
    case TxNone:{
        m_affine.m_dx = dx;
        m_affine.m_dy = dy;
        break;
    }
    case TxTranslate:
        m_affine.m_dx += dx;
        m_affine.m_dy += dy;
        break;
    case TxScale:{
        m_affine.m_dx += dx*m_affine.m_11;
        m_affine.m_dy += dy*m_affine.m_22;
        break;
    }
    case TxProject:{
        m_33 += dx*m_13 + dy*m_23;
    }
    case TxShear:
    case TxRotate:{
        m_affine.m_dx += dx*m_affine.m_11 + dy*m_affine.m_21;
        m_affine.m_dy += dy*m_affine.m_22 + dx*m_affine.m_12;
        break;
    }
    }
    if (m_dirty < TxTranslate){
        m_dirty = TxTranslate;
    }
    return *this;

}

LTransform2D LTransform2D::inverted(bool *invertible) const
{
    LTransform2D invert;
    bool inv = true;

    switch(inlineType()) {
    case TxNone:{
        break;
    }
    case TxTranslate:{
        invert.m_affine.m_dx = -m_affine.m_dx;
        invert.m_affine.m_dy = -m_affine.m_dy;
        break;
    }
    case TxScale:{
        inv = !lFuzzyIsZero(m_affine.m_11);
        inv &= !lFuzzyIsZero(m_affine.m_22);
        if (inv) {
            invert.m_affine.m_11 = 1. / m_affine.m_11;
            invert.m_affine.m_22 = 1. / m_affine.m_22;
            invert.m_affine.m_dx = -m_affine.m_dx * invert.m_affine.m_11;
            invert.m_affine.m_dy = -m_affine.m_dy * invert.m_affine.m_22;
        }
        break;
    }
    case TxRotate:
    case TxShear:{
        invert.m_affine = m_affine.inverted(&inv);
        break;
    }
    default:{
        // general case
        double det = determinant();
        inv = !lFuzzyIsZero(det);
        if (inv){
            invert = adjoint() / det;
        }
        break;
    }
    }

    if (invertible){
        *invertible = inv;
    }

    if (inv) {
        // inverting doesn't change the type
        invert.m_type = m_type;
        invert.m_dirty = m_dirty;
    }

    return invert;
}

LTransform2D& LTransform2D::rotate(const double sina, const double cosa, LAxis axis)
{
    if (axis == LAxis::Z) {
        switch(inlineType()) {
        case TxNone:
        case TxTranslate:
            m_affine.m_11 = cosa;
            m_affine.m_12 = sina;
            m_affine.m_21 = -sina;
            m_affine.m_22 = cosa;
            break;
        case TxScale: {
            double tm11 = cosa*m_affine.m_11;
            double tm12 = sina*m_affine.m_22;
            double tm21 = -sina*m_affine.m_11;
            double tm22 = cosa*m_affine.m_22;
            m_affine.m_11 = tm11; m_affine.m_12 = tm12;
            m_affine.m_21 = tm21; m_affine.m_22 = tm22;
            break;
        }
        case TxProject: {
            double tm13 = cosa*m_13 + sina*m_23;
            double tm23 = -sina*m_13 + cosa*m_23;
            m_13 = tm13;
            m_23 = tm23;
            //No break;
        }
        case TxRotate:
        case TxShear: {
            double tm11 = cosa*m_affine.m_11 + sina*m_affine.m_21;
            double tm12 = cosa*m_affine.m_12 + sina*m_affine.m_22;
            double tm21 = -sina*m_affine.m_11 + cosa*m_affine.m_21;
            double tm22 = -sina*m_affine.m_12 + cosa*m_affine.m_22;
            m_affine.m_11 = tm11; m_affine.m_12 = tm12;
            m_affine.m_21 = tm21; m_affine.m_22 = tm22;
            break;
        }
        }
        if (m_dirty < TxRotate)
            m_dirty = TxRotate;
    } else {
        LTransform2D result;
        if (axis == LAxis::Y) {
            result.m_affine.m_11 = cosa;
            result.m_13 = -sina /1024.;
        } else {
            result.m_affine.m_22 = cosa;
            result.m_23 = -sina /1024.;
        }
        result.m_type = TxProject;
        *this = result * (*this);
    }
    return *this;
}



LTransform2D &LTransform2D::rotate(double a, LAxis axis)
{
    if (a == 0){
        return *this;
    }

    double sina = 0;
    double cosa = 0;
    if (a == 90. || a == -270.)
        sina = 1.;
    else if (a == 270. || a == -90.)
        sina = -1.;
    else if (a == 180.)
        cosa = -1.;
    else{
        double b = lDegreesToRadians(a);
        sina = std::sin(b);
        cosa = std::cos(b);
    }
    return rotate(sina, cosa, axis);
}

LTransform2D &LTransform2D::rotateRadians(double a, LAxis axis)
{
    double sina = std::sin(a);
    double cosa = std::cos(a);
    rotate(sina, cosa, axis);
    return *this;

}

LTransform2D LTransform2D::adjoint() const
{
    double h11, h12, h13,
        h21, h22, h23,
        h31, h32, h33;
    h11 = m_affine.m_22*m_33 - m_23*m_affine.m_dy;
    h21 = m_23*m_affine.m_dx - m_affine.m_21*m_33;
    h31 = m_affine.m_21*m_affine.m_dy - m_affine.m_22*m_affine.m_dx;
    h12 = m_13*m_affine.m_dy - m_affine.m_12*m_33;
    h22 = m_affine.m_11*m_33 - m_13*m_affine.m_dx;
    h32 = m_affine.m_12*m_affine.m_dx - m_affine.m_11*m_affine.m_dy;
    h13 = m_affine.m_12*m_23 - m_13*m_affine.m_22;
    h23 = m_13*m_affine.m_21 - m_affine.m_11*m_23;
    h33 = m_affine.m_11*m_affine.m_22 - m_affine.m_12*m_affine.m_21;

    return LTransform2D(h11, h12, h13,
                        h21, h22, h23,
                        h31, h32, h33);
}

void LTransform2D::map(int x, int y, int *tx, int *ty) const
{
    double ox, oy;
    map(double(x), double(y), &ox, &oy);
    *tx = std::lround(ox);
    *ty = std::lround(oy);
}



void LTransform2D::map(double x, double y, double *tx, double *ty) const
{
    TransformationType t = inlineType();
    switch(t) {
    case TxNone:{
        *tx = x;
        *ty = y;
        break;
    }
    case TxTranslate:{
        *tx = x + m_affine.m_dx;
        *ty = y + m_affine.m_dy;
        break;
    }
    case TxScale:{
        *tx = m_affine.m_11 * x + m_affine.m_dx;
        *ty = m_affine.m_22 * y + m_affine.m_dy;
        break;
    }
    case TxRotate:
    case TxShear:
    case TxProject:{
        *tx = m_affine.m_11 * x + m_affine.m_21 * y + m_affine.m_dx;
        *ty = m_affine.m_12 * x + m_affine.m_22 * y + m_affine.m_dy;
        if (t == TxProject) {
            double w = (m_13 * x + m_23 * y + m_33);
            if (w < double(L_NEAR_CLIP)){
                w = double(L_NEAR_CLIP);
            }
            w = 1./w;
            *tx *= w;
            *ty *= w;
        }
        break;
    }
    }
}

LRect<int> LTransform2D::mapRect(const LRect<int> &rect) const
{
    TransformationType t = inlineType();
    if (t <= TxTranslate){
        return rect.translated(std::lround(m_affine.m_dx), std::lround(m_affine.m_dy));
    }

    if (t <= TxScale) {
        int x = std::lround(m_affine.m_11*rect.m_x + m_affine.m_dx);
        int y = std::lround(m_affine.m_22*rect.m_y + m_affine.m_dy);
        int w = std::lround(m_affine.m_11*rect.width());
        int h = std::lround(m_affine.m_22*rect.height());
        if (w < 0) {
            w = -w;
            x -= w;
        }
        if (h < 0) {
            h = -h;
            y -= h;
        }
        return LRect<int>(x, y, w, h, false);
    } else if (t < TxProject || !lNeedsPerspectiveClipping(rect.toRectF(), *this)) {
        // see mapToPolygon for explanations of the algorithm.
        double x = 0, y = 0;
        map(rect.m_x, rect.m_y, &x, &y);
        double xmin = x;
        double ymin = y;
        double xmax = x;
        double ymax = y;
        map(rect.x2(), rect.m_y, &x, &y);
        xmin = std::min(xmin, x);
        ymin = std::min(ymin, y);
        xmax = std::max(xmax, x);
        ymax = std::max(ymax, y);
        map(rect.x2(), rect.y2(), &x, &y);
        xmin = std::min(xmin, x);
        ymin = std::min(ymin, y);
        xmax = std::max(xmax, x);
        ymax = std::max(ymax, y);
        map(rect.m_x, rect.y2(), &x, &y);
        xmin = std::min(xmin, x);
        ymin = std::min(ymin, y);
        xmax = std::max(xmax, x);
        ymax = std::max(ymax, y);
        return LRect<int>(std::lround(xmin),
                          std::lround(ymin),
                          (std::lround(xmax)-std::lround(xmin)),
                          (std::lround(ymax)-std::lround(ymin)), false);
    } else {
        std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
        path->addRect(rect.toRectF());
        path = map(path);
        return map(path)->boundingRect().toRect();
    }
}

LRect<double> LTransform2D::mapRect(const LRect<double> &rect) const
{
    TransformationType t = inlineType();
    if (t <= TxTranslate){
        return rect.translated(m_affine.m_dx, m_affine.m_dy);
    }

    if (t <= TxScale) {
        double x = m_affine.m_11*rect.m_x + m_affine.m_dx;
        double y = m_affine.m_22*rect.m_y + m_affine.m_dy;
        double w = m_affine.m_11*rect.width();
        double h = m_affine.m_22*rect.height();
        if (w < 0) {
            w = -w;
            x -= w;
        }
        if (h < 0) {
            h = -h;
            y -= h;
        }
        return LRect<double>(x, y, w, h, false);
    } else if (t < TxProject || !lNeedsPerspectiveClipping(rect, *this)) {
        double x = 0, y = 0;
        map(rect.m_x, rect.m_y, &x, &y);
        double xmin = x;
        double ymin = y;
        double xmax = x;
        double ymax = y;
        map(rect.m_x + rect.width(), rect.m_y, &x, &y);
        xmin = std::min(xmin, x);
        ymin = std::min(ymin, y);
        xmax = std::max(xmax, x);
        ymax = std::max(ymax, y);
        map(rect.m_x + rect.width(), rect.m_y + rect.height(), &x, &y);
        xmin = std::min(xmin, x);
        ymin = std::min(ymin, y);
        xmax = std::max(xmax, x);
        ymax = std::max(ymax, y);
        map(rect.m_x, rect.m_y + rect.height(), &x, &y);
        xmin = std::min(xmin, x);
        ymin = std::min(ymin, y);
        xmax = std::max(xmax, x);
        ymax = std::max(ymax, y);
        return LRect<double>(xmin, ymin, xmax-xmin, ymax-ymin, false);
    } else {
        std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
        path->addRect(rect);
        path = map(path);
        return path->boundingRect();
    }
}

std::shared_ptr<LRegion> LTransform2D::map(const std::shared_ptr<LRegion> r) const
{
    if(r == nullptr){
        return std::make_shared<LRegion>();
    }

    if(r->isEmpty()){
        return r;
    }

    TransformationType t = inlineType();
    if (t == TxNone){
        return r;
    }

    if (t == TxTranslate) {
        std::shared_ptr<LRegion> copy = std::make_shared<LRegion>(*r);
        copy->translate(std::lround(m_affine.m_dx), std::lround(m_affine.m_dy));
        return copy;
    }

    if (t == TxScale) {
        std::shared_ptr<LRegion> res = std::make_shared<LRegion>();
        if (m11() < 0 || m22() < 0) {
            for (const LRect<int> &rect : *r){
                res->unite(mapRect(rect.toRectF()).toRect());
            }
        } else {
            std::vector<LRect<int> > rects;
            rects.reserve(r->rectCount());
            for (const LRect<int> &rect : *r) {
                LRect<int> nr = mapRect(rect.toRectF()).toRect();
                if (!nr.isEmpty()){
                    rects.push_back(nr);
                }
            }
            res->setRects(rects.data(), rects.size());
        }
        return res;
    }
    std::shared_ptr<LPainterPath> p = LRegion::regionToPath(r.get());
    p = map(p);
    std::shared_ptr<LRegion> ret
        = std::make_shared<LRegion>(p->toFillPolygon(LTransform2D()).toPolygon());
    return ret;
}

std::shared_ptr<LPainterPath> LTransform2D::map(std::shared_ptr<LPainterPath> path, bool toOutline, bool disableCurve) const
{
    double scale = scaleForTransform();
    double curveThreshold = scale == 0 ? 0.25 : 0.25 / scale;
    std::shared_ptr<LPainterPath> ret = nullptr;
    if(path == nullptr){
        ret = std::make_shared<LPainterPath>(path, toOutline, disableCurve, curveThreshold);
        return ret;
    }

    if(path->isEmpty()){
        if((path->isOutline() != toOutline) || (path->isDisableCurve() != disableCurve)){
            ret = std::make_shared<LPainterPath>(path, toOutline, disableCurve, curveThreshold);
            return ret;
        }else{
            return path;
        }
    }

    TransformationType t = inlineType();
    if (t == TxNone){
        if((path->isOutline() != toOutline) || (path->isDisableCurve() != disableCurve)){
            ret = std::make_shared<LPainterPath>(path, toOutline, disableCurve, curveThreshold);
            return ret;
        }else{
            return path;
        }
    }

    if (t >= TxProject){
        return mapProjective(*this, path, toOutline, disableCurve, curveThreshold);
    }

    ret = std::make_shared<LPainterPath>(path, toOutline, disableCurve, curveThreshold);
    if (t == TxTranslate) {
        ret->translate(m_affine.m_dx, m_affine.m_dy);
    } else {
        ret->resetMark();
        // Full xform
        for (int i=0; i<ret->elementCount(); ++i) {
            LPainterPath::Element &e = ret->m_elements[i];
            map(e.m_x, e.m_y, &e.m_x, &e.m_y);
        }
    }
    return ret;
}

LPolygon<double> LTransform2D::map(const LPolygon<double> &a) const
{
    TransformationType t = inlineType();
    if (t <= TxTranslate){
        return a.translated(m_affine.m_dx, m_affine.m_dy);
    }

    int size = a.size();
    int i;
    LPolygon<double> p(size);
    const LPoint<double> *da = a.data();
    LPoint<double> *dp = p.data();

    for(i = 0; i < size; ++i) {
        map(da[i].m_x, da[i].m_y, &(dp[i].m_x), &dp[i].m_y);
    }
    return p;
}

LPolygon<int> LTransform2D::map(const LPolygon<int> &a) const
{
    TransformationType t = inlineType();
    if (t <= TxTranslate){
        return a.translated(m_affine.m_dx, m_affine.m_dy);
    }

    int size = a.size();
    int i;
    LPolygon<int> p(size);
    const LPoint<int> *da = a.data();
    LPoint<int> *dp = p.data();

    for(i = 0; i < size; ++i) {
        double nx = 0, ny = 0;
        map(da[i].m_x, da[i].m_y, &nx, &ny);
        dp[i].m_x = std::round(nx);
        dp[i].m_y = std::round(ny);
    }
    return p;
}

LPolygon<int> LTransform2D::mapToPolygon(const LRect<int> &rect) const
{
    TransformationType t = inlineType();

    LPolygon<int> a(4);
    double x[4] = { 0, 0, 0, 0 }, y[4] = { 0, 0, 0, 0 };
    if (t <= TxScale) {
        x[0] = m_affine.m_11*rect.m_x + m_affine.m_dx;
        y[0] = m_affine.m_22*rect.m_y + m_affine.m_dy;
        double w = m_affine.m_11*rect.m_width;
        double h = m_affine.m_22*rect.m_width;
        if (w < 0) {
            w = -w;
            x[0] -= w;
        }
        if (h < 0) {
            h = -h;
            y[0] -= h;
        }
        x[1] = x[0]+w;
        x[2] = x[1];
        x[3] = x[0];
        y[1] = y[0];
        y[2] = y[0]+h;
        y[3] = y[2];
    } else {
        auto right = rect.m_x + rect.m_width;
        auto bottom = rect.m_y + rect.m_width;
        map(rect.m_x, rect.m_y, &x[0], &y[0]);
        map(right, rect.m_y, &x[1], &y[1]);
        map(right, bottom, &x[2], &y[2]);
        map(rect.m_x, bottom, &x[3], &y[3]);
    }

    a[0].m_x = std::lround(x[0]);
    a[0].m_y = std::lround(y[0]);
    a[1].m_x = std::lround(x[1]);
    a[1].m_y = std::lround(y[1]);
    a[2].m_x = std::lround(x[2]);
    a[2].m_y = std::lround(y[2]);
    a[3].m_x = std::lround(x[3]);
    a[3].m_y = std::lround(y[3]);
    return a;
}

LTransform2D &LTransform2D::scale(double sx, double sy)
{
    if (sx == 1 && sy == 1){
        return *this;
    }

    switch(inlineType()) {
    case TxNone:
    case TxTranslate:{
        m_affine.m_11 = sx;
        m_affine.m_22 = sy;
        break;
    }
    case TxProject:{
        m_13 *= sx;
        m_23 *= sy;
        //No break;
    }
    case TxRotate:
    case TxShear:{
        m_affine.m_12 *= sx;
        m_affine.m_21 *= sy;
        //No break;
    }
    case TxScale:{
        m_affine.m_11 *= sx;
        m_affine.m_22 *= sy;
        break;
    }
    }
    if (m_dirty < TxScale){
        m_dirty = TxScale;
    }
    return *this;
}

LTransform2D &LTransform2D::shear(double sh, double sv)
{
    if (sh == 0 && sv == 0){
        return *this;
    }


    switch(inlineType()) {
    case TxNone:
    case TxTranslate:{
        m_affine.m_12 = sv;
        m_affine.m_21 = sh;
        break;
    }
    case TxScale:{
        m_affine.m_12 = sv*m_affine.m_22;
        m_affine.m_21 = sh*m_affine.m_11;
        break;
    }
    case TxProject: {
        double tm13 = sv * m_23;
        double tm23 = sh * m_13;
        m_13 += tm13;
        m_23 += tm23;
        //No break;
    }
    case TxRotate:
    case TxShear: {
        double tm11 = sv * m_affine.m_21;
        double tm22 = sh * m_affine.m_12;
        double tm12 = sv * m_affine.m_22;
        double tm21 = sh * m_affine.m_11;
        m_affine.m_11 += tm11;
        m_affine.m_12 += tm12;
        m_affine.m_21 += tm21;
        m_affine.m_22 += tm22;
        break;
    }
    }
    if (m_dirty < TxShear){
        m_dirty = TxShear;
    }
    return *this;
}

LTransform2D &LTransform2D::operator*=(double num)
{
    if (num == 1.){
        return *this;
    }
    m_affine.m_11 *= num;
    m_affine.m_12 *= num;
    m_13        *= num;
    m_affine.m_21 *= num;
    m_affine.m_22 *= num;
    m_23        *= num;
    m_affine.m_dx  *= num;
    m_affine.m_dy  *= num;
    m_33        *= num;
    if (m_dirty < TxScale){
        m_dirty = TxScale;
    }
    return *this;
}

void LTransform2D::setMatrix(double m11, double m12, double m13,
                             double m21, double m22, double m23,
                             double m31, double m32, double m33)
{
    m_affine.m_11 = m11; m_affine.m_12 = m12; m_13 = m13;
    m_affine.m_21 = m21; m_affine.m_22 = m22; m_23 = m23;
    m_affine.m_dx = m31; m_affine.m_dy = m32; m_33 = m33;
    m_type = TxNone;
    m_dirty = TxProject;

}

LTransform2D &LTransform2D::operator+=(double num)
{
    if (num == 0){
        return *this;
    }
    m_affine.m_11 += num;
    m_affine.m_12 += num;
    m_13        += num;
    m_affine.m_21 += num;
    m_affine.m_22 += num;
    m_23        += num;
    m_affine.m_dx  += num;
    m_affine.m_dy  += num;
    m_33        += num;
    m_dirty     = TxProject;
    return *this;
}

LTransform2D &LTransform2D::operator-=(double num)
{
    if (num == 0){
        return *this;
    }
    m_affine.m_11 -= num;
    m_affine.m_12 -= num;
    m_13        -= num;
    m_affine.m_21 -= num;
    m_affine.m_22 -= num;
    m_23        -= num;
    m_affine.m_dx  -= num;
    m_affine.m_dy  -= num;
    m_33        -= num;
    m_dirty     = TxProject;
    return *this;
}

bool LTransform2D::operator==(const LTransform2D & o) const
{
    return m_affine.m_11 == o.m_affine.m_11 &&
           m_affine.m_12 == o.m_affine.m_12 &&
           m_affine.m_21 == o.m_affine.m_21 &&
           m_affine.m_22 == o.m_affine.m_22 &&
           m_affine.m_dx == o.m_affine.m_dx &&
           m_affine.m_dy == o.m_affine.m_dy &&
           m_13 == o.m_13 &&
           m_23 == o.m_23 &&
           m_33 == o.m_33;
}

std::shared_ptr<LPainterPath> LTransform2D::mapProjective(const LTransform2D &transform,
                                                          const std::shared_ptr<LPainterPath> path,
                                                          bool isOutline,
                                                          bool disableCurve,
                                                          double curveThreshold)
{
    std::shared_ptr<LPainterPath> result = std::make_shared<LPainterPath>(nullptr, isOutline, disableCurve, curveThreshold);

    LPoint<double> last;
    LPoint<double> lastMoveTo;
    bool needsMoveTo = true;
    for (int i = 0; i < path->m_elements.size(); ++i) {
        switch (path->m_elements.at(i).m_type) {
        case LPainterPath::MoveToElement:{
            if (i > 0 && lastMoveTo != last){
                lineToClipped(result, transform, last, lastMoveTo, needsMoveTo);
            }

            lastMoveTo = path->m_elements.at(i).toPointF();
            last = path->m_elements.at(i).toPointF();
            needsMoveTo = true;
            break;
        }
        case LPainterPath::LineToElement:{
            if (lineToClipped(result, transform, last, path->m_elements.at(i).toPointF(), needsMoveTo)){
                needsMoveTo = false;
            }
            last = path->m_elements.at(i).toPointF();
            break;
        }
        case LPainterPath::CurveToElement:{
            if (cubicToClipped(result,
                               transform,
                               last,
                               path->m_elements.at(i).toPointF(),
                               path->m_elements.at(i+1).toPointF(),
                               path->m_elements.at(i+2).toPointF(),
                               needsMoveTo,
                               curveThreshold)){
                needsMoveTo = false;
            }
            i += 2;
            last = path->m_elements.at(i).toPointF();
            break;
        }
        default:{
            break;
        }
        }
    }

    if (path->elementCount() > 0 && lastMoveTo != last){
        lineToClipped(result, transform, last, lastMoveTo, needsMoveTo, false);
    }

    result->setFillRule(path->fillRule());
    return result;
}

LTransform2D LTransform2D::fromTranslate(double dx, double dy)
{
    if (std::isnan(dx) | std::isnan(dy)) {
        return LTransform2D();
    }

    LTransform2D transform(1, 0, 0, 0, 1, 0, dx, dy, 1);
    if (dx == 0 && dy == 0){
        transform.m_type = TxNone;
    }else{
        transform.m_type = TxTranslate;
    }
    transform.m_dirty = TxNone;
    return transform;
}

LTransform2D LTransform2D::fromScale(double sx, double sy)
{
    if (std::isnan(sx) || std::isnan(sy)) {
        return LTransform2D();
    }
    LTransform2D transform(sx, 0, 0, 0, sy, 0, 0, 0, 1);
    if (sx == 1. && sy == 1.){
        transform.m_type = TxNone;
    }
    else{
        transform.m_type = TxScale;
    }
    transform.m_dirty = TxNone;
    return transform;
}


LTransform2D& LTransform2D::operator*=(const LTransform2D &o){
    const TransformationType otherType = o.inlineType();
    if (otherType == TxNone){
        return *this;
    }

    const TransformationType thisType = inlineType();
    if (thisType == TxNone){
        return operator=(o);
    }

    TransformationType t = std::max(thisType, otherType);
    switch(t) {
    case TxNone:
        break;
    case TxTranslate:{
        m_affine.m_dx += o.m_affine.m_dx;
        m_affine.m_dy += o.m_affine.m_dy;
        break;
    }
    case TxScale:
    {
        double m11 = m_affine.m_11*o.m_affine.m_11;
        double m22 = m_affine.m_22*o.m_affine.m_22;

        double m31 = m_affine.m_dx*o.m_affine.m_11 + o.m_affine.m_dx;
        double m32 = m_affine.m_dy*o.m_affine.m_22 + o.m_affine.m_dy;

        m_affine.m_11 = m11;
        m_affine.m_22 = m22;
        m_affine.m_dx = m31;
        m_affine.m_dy = m32;
        break;
    }
    case TxRotate:
    case TxShear:
    {
        double m11 = m_affine.m_11*o.m_affine.m_11 + m_affine.m_12*o.m_affine.m_21;
        double m12 = m_affine.m_11*o.m_affine.m_12 + m_affine.m_12*o.m_affine.m_22;

        double m21 = m_affine.m_21*o.m_affine.m_11 + m_affine.m_22*o.m_affine.m_21;
        double m22 = m_affine.m_21*o.m_affine.m_12 + m_affine.m_22*o.m_affine.m_22;

        double m31 = m_affine.m_dx*o.m_affine.m_11 + m_affine.m_dy*o.m_affine.m_21 + o.m_affine.m_dx;
        double m32 = m_affine.m_dx*o.m_affine.m_12 + m_affine.m_dy*o.m_affine.m_22 + o.m_affine.m_dy;

        m_affine.m_11 = m11; m_affine.m_12 = m12;
        m_affine.m_21 = m21; m_affine.m_22 = m22;
        m_affine.m_dx = m31; m_affine.m_dy = m32;
        break;
    }
    case TxProject:
    {
        double m11 = m_affine.m_11*o.m_affine.m_11 + m_affine.m_12*o.m_affine.m_21 + m_13*o.m_affine.m_dx;
        double m12 = m_affine.m_11*o.m_affine.m_12 + m_affine.m_12*o.m_affine.m_22 + m_13*o.m_affine.m_dy;
        double m13 = m_affine.m_11*o.m_13 + m_affine.m_12*o.m_23 + m_13*o.m_33;

        double m21 = m_affine.m_21*o.m_affine.m_11 + m_affine.m_22*o.m_affine.m_21 + m_23*o.m_affine.m_dx;
        double m22 = m_affine.m_21*o.m_affine.m_12 + m_affine.m_22*o.m_affine.m_22 + m_23*o.m_affine.m_dy;
        double m23 = m_affine.m_21*o.m_13 + m_affine.m_22*o.m_23 + m_23*o.m_33;

        double m31 = m_affine.m_dx*o.m_affine.m_11 + m_affine.m_dy*o.m_affine.m_21 + m_33*o.m_affine.m_dx;
        double m32 = m_affine.m_dx*o.m_affine.m_12 + m_affine.m_dy*o.m_affine.m_22 + m_33*o.m_affine.m_dy;
        double m33 = m_affine.m_dx*o.m_13 + m_affine.m_dy*o.m_23 + m_33*o.m_33;

        m_affine.m_11 = m11; m_affine.m_12 = m12; m_13 = m13;
        m_affine.m_21 = m21; m_affine.m_22 = m22; m_23 = m23;
        m_affine.m_dx = m31; m_affine.m_dy = m32; m_33 = m33;
    }
    }

    m_dirty = t;
    m_type = t;

    return *this;
}

LTransform2D LTransform2D::operator*(const LTransform2D &o) const{
    LTransform2D t(*this);
    t*=o;
    return t;
}

double LTransform2D::scaleForTransform() const
{
    double ret;
    scaleForTransform(*this, &ret);
    return ret;
}

void LTransform2D::reset()
{
    m_affine.reset();
    m_13 = 0;
    m_23 = 0;
    m_33 = 1;

    m_type = TxNone;
    m_dirty = TxNone;
}

LK_NAMESPACE_END
