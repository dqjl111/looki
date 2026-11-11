#ifndef LTRANSFORM2D_H
#define LTRANSFORM2D_H
#include "lkgui/lrect.h"
#include "lkgui/paint/lline.h"
#include "lkgui/paint/lpolygon.h"
#include "lkgui/paint/lmatrix2d.h"

LK_NAMESPACE_BEGIN
class LRegion;
class LPainterPath;
class LKGUIDEF LTransform2D
{
public:
    LTransform2D();
    LTransform2D(double h11, double h12, double h13,
                 double h21, double h22, double h23,
                 double h31, double h32, double h33 = 1.0);
    LTransform2D(double h11, double h12, double h21,
                 double h22, double dx, double dy);

    enum TransformationType {
        TxNone      = 0x00,
        TxTranslate = 0x01,
        TxScale     = 0x02,
        TxRotate    = 0x04,
        TxShear     = 0x08,
        TxProject   = 0x10
    };

    TransformationType type() const;
    LTransform2D &translate(double dx, double dy);
    LTransform2D inverted(bool *invertible = nullptr) const;

    LTransform2D &rotate(double a, LAxis axis = LAxis::Z);
    LTransform2D &rotateRadians(double a, LAxis axis = LAxis::Z);

    inline double determinant() const
    {
        return m_affine.m_11*(m_33*m_affine.m_22-m_affine.m_dy*m_23)
               -m_affine.m_21*(m_33*m_affine.m_12-m_affine.m_dy*m_13)
               +m_affine.m_dx*(m_23*m_affine.m_12-m_affine.m_22*m_13);
    }

    LTransform2D adjoint() const;


    void map(int x, int y, int *tx, int *ty) const;
    void map(double x, double y, double *tx, double *ty) const;


    template<typename T>
    LPoint<T> map(const LPoint<T> &p) const{
        LPoint<T> ret;
        this->map(p.m_x, p.m_y, &(ret.m_x), &(ret.m_y));
        return ret;
    }

    template<typename T>
    LLine<T> map(const LLine<T> &l) const{
        LLine<T> ret;
        this->map(l.m_pt1.m_x, l.m_pt1.m_y, &(ret.m_pt1.m_x), &(ret.m_pt1.m_y));
        this->map(l.m_pt2.m_x, l.m_pt2.m_y, &(ret.m_pt2.m_x), &(ret.m_pt2.m_y));
        return ret;
    }

    LRect<int> mapRect(const LRect<int> &r) const;
    LRect<double> mapRect(const LRect<double> &r) const;

    std::shared_ptr<LRegion> map(const std::shared_ptr<LRegion> r) const;
    std::shared_ptr<LPainterPath> map(const std::shared_ptr<LPainterPath> p,
                                      bool toOutline = false,
                                      bool disableCurve=false) const;

    LPolygon<double> map(const LPolygon<double> &a) const;
    LPolygon<int> map(const LPolygon<int> &a) const;
    LPolygon<int> mapToPolygon(const LRect<int> &r) const;

    LTransform2D &scale(double sx, double sy);
    LTransform2D &shear(double sh, double sv);


    LTransform2D &operator*=(double num);
    inline bool isIdentity() const{
        return inlineType() == TxNone;
    }

    inline bool isAffine() const
    {
        return inlineType() < TxProject;
    }

    inline bool isTranslating() const
    {
        return inlineType() >= TxTranslate;
    }

    void setMatrix(double m11, double m12, double m13,
                   double m21, double m22, double m23,
                   double m31, double m32, double m33);


    inline LTransform2D &operator/=(double num){
        if (num == 0){
            return *this;
        }
        num = 1/num;
        return operator*=(num);
    }
    LTransform2D &operator+=(double num);
    LTransform2D &operator-=(double num);

    bool operator==(const LTransform2D &) const;
    inline bool operator!=(const LTransform2D &o) const{ return !(operator==(o)); }

    inline double m11() const
    {
        return m_affine.m_11;
    }
    inline double m12() const
    {
        return m_affine.m_12;
    }
    inline double m13() const
    {
        return m_13;
    }
    inline double m21() const
    {
        return m_affine.m_21;
    }
    inline double m22() const
    {
        return m_affine.m_22;
    }
    inline double m23() const
    {
        return m_23;
    }
    inline double m31() const
    {
        return m_affine.m_dx;
    }
    inline double m32() const
    {
        return m_affine.m_dy;
    }
    inline double m33() const
    {
        return m_33;
    }
    inline double dx() const
    {
        return m_affine.m_dx;
    }
    inline double dy() const
    {
        return m_affine.m_dy;
    }

    static LTransform2D fromTranslate(double dx, double dy);
    static LTransform2D fromScale(double sx, double sy);

    LTransform2D &operator*=(const LTransform2D &);
    LTransform2D operator*(const LTransform2D &o) const;

    double scaleForTransform() const;
    void reset();
    static bool scaleForTransform(const LTransform2D &transform, double *scale);

    LCbor toCbor() const;
    void fromCbor(const LCbor&);
protected:
    inline TransformationType inlineType() const{
        if (m_dirty == TxNone){
            return static_cast<TransformationType>(m_type);
        }
        return type();
    }

    LTransform2D & rotate(const double sina, const double cosa, LAxis axis);

    static std::shared_ptr<LPainterPath> mapProjective(const LTransform2D &transform,
                                                       const std::shared_ptr<LPainterPath> path,
                                                       bool isOutline = false,
                                                       bool disableCurve=false,
                                                       double curveThreshold =0.25);

    LMatrix2D m_affine;
    double   m_13 = 0;
    double   m_23 = 0;
    double   m_33 = 1;

    mutable uint8_t m_type = TxNone;
    mutable uint8_t m_dirty = TxNone;
};

inline LTransform2D operator *(const LTransform2D &a, double n)
{ LTransform2D t(a); t *= n; return t; }
inline LTransform2D operator /(const LTransform2D &a, double n)
{ LTransform2D t(a); t /= n; return t; }
inline LTransform2D operator +(const LTransform2D &a, double n)
{ LTransform2D t(a); t += n; return t; }
inline LTransform2D operator -(const LTransform2D &a, double n)
{ LTransform2D t(a); t -= n; return t; }


// mathematical semantics
template<typename T>
inline LPoint<T> operator*(const LPoint<T> &p, const LTransform2D &m)
{ return m.map<T>(p); }

template<typename T>
inline LLine<T> operator*(const LLine<T> &l, const LTransform2D &m)
{ return m.map(l); }


LK_NAMESPACE_END
#endif // LTRANSFORM_H
