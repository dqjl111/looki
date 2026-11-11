#ifndef LQUATERNION_H
#define LQUATERNION_H
#include "lkgui/math/lvectornd.h"
#include "lkgui/math/lmatrix4x4.h"

LK_NAMESPACE_BEGIN

class LKGUIDEF LQuaternion
{
public:
     LQuaternion() noexcept;
    explicit LQuaternion(Initialization) noexcept {}
     LQuaternion(float scalar, float xpos, float ypos, float zpos) noexcept;
     LQuaternion(float scalar, const LVector3D &vector) noexcept;
     explicit LQuaternion(const LVector4D &vector) noexcept;

     bool isNull() const noexcept;
     bool isIdentity() const noexcept;

     LVector3D vector() const noexcept;
     void setVector(const LVector3D &vector) noexcept;
     void setVector(float x, float y, float z) noexcept;

     float x() const noexcept;
     float y() const noexcept;
     float z() const noexcept;
     float scalar() const noexcept;

     void setX(float x) noexcept;
     void setY(float y) noexcept;
     void setZ(float z) noexcept;
     void setScalar(float scalar) noexcept;

     static float dotProduct(const LQuaternion &q1, const LQuaternion &q2) noexcept;

    float length() const;
    float lengthSquared() const;

    LQuaternion normalized() const;
    void normalize();

     LQuaternion inverted() const noexcept;

    [[nodiscard]]  LQuaternion conjugated() const noexcept;

    LVector3D rotatedVector(const LVector3D &vector) const;

     LQuaternion &operator+=(const LQuaternion &quaternion) noexcept;
     LQuaternion &operator-=(const LQuaternion &quaternion) noexcept;
     LQuaternion &operator*=(float factor) noexcept;
     LQuaternion &operator*=(const LQuaternion &quaternion) noexcept;
     LQuaternion &operator/=(float divisor);

    friend  bool operator==(const LQuaternion &q1, const LQuaternion &q2) noexcept
    {
        return q1.wp == q2.wp && q1.xp == q2.xp && q1.yp == q2.yp && q1.zp == q2.zp;
    }
    friend  bool operator!=(const LQuaternion &q1, const LQuaternion &q2) noexcept
    {
        return !(q1 == q2);
    }

    friend  LQuaternion operator+(const LQuaternion &q1, const LQuaternion &q2) noexcept;
    friend  LQuaternion operator-(const LQuaternion &q1, const LQuaternion &q2) noexcept;
    friend  LQuaternion operator*(float factor, const LQuaternion &quaternion) noexcept;
    friend  LQuaternion operator*(const LQuaternion &quaternion, float factor) noexcept;
    friend  LQuaternion operator*(const LQuaternion &q1, const LQuaternion &q2) noexcept;
    friend  LQuaternion operator-(const LQuaternion &quaternion) noexcept;
    friend  LQuaternion operator/(const LQuaternion &quaternion, float divisor);

    friend  bool lFuzzyCompare(const LQuaternion &q1, const LQuaternion &q2) noexcept;
     LVector4D toVector4D() const noexcept;

    struct Axis {
        float x, y, z;
        friend bool lFuzzyIsZero(Axis axis) noexcept
        { return lFuzzyIsZero(axis.x) && lFuzzyIsZero(axis.y) && lFuzzyIsZero(axis.z); }

        static  Axis fromVector3D(LVector3D v) noexcept
        { return {v.x(), v.y(), v.z()}; }
         LVector3D toVector3D() const noexcept { return {x, y, z}; }

    };

    inline void getAxisAndAngle(LVector3D *axis, float *angle) const;
    static LQuaternion fromAxisAndAngle(const LVector3D &axis, float angle);
    void getAxisAndAngle(float *x, float *y, float *z, float *angle) const;
    static LQuaternion fromAxisAndAngle(float x, float y, float z,
                                        float angle);

    inline LVector3D toEulerAngles() const;

    static LQuaternion fromEulerAngles(const LVector3D &angles)
    { return fromEulerAngles(angles.x(), angles.y(), angles.z()); }

    template <typename T>
    struct EulerAngles
    {
        T pitch, yaw, roll;
    };
    EulerAngles<float> eulerAngles() const;
    static LQuaternion fromEulerAngles(EulerAngles<float> angles)
    { return fromEulerAngles(angles.pitch, angles.yaw, angles.roll); }

    inline void getEulerAngles(float *pitch, float *yaw, float *roll) const;
    static LQuaternion fromEulerAngles(float pitch, float yaw, float roll);

    LMatrix3x3 toRotationMatrix() const;
    static LQuaternion fromRotationMatrix(const LMatrix3x3 &rot3x3);

    struct Axes
    {
        Axis x, y, z;
    };
    Axes toAxes() const;
    static LQuaternion fromAxes(Axes axes); // clazy:exclude=function-args-by-ref

    inline void getAxes(LVector3D *xAxis, LVector3D *yAxis, LVector3D *zAxis) const;
    inline static LQuaternion fromAxes(const LVector3D &xAxis,
                                const LVector3D &yAxis,
                                const LVector3D &zAxis);

    static LQuaternion fromDirection(const LVector3D &direction,
                                     const LVector3D &up);

    static LQuaternion rotationTo(const LVector3D &from,
                                  const LVector3D &to);

    static LQuaternion slerp(const LQuaternion &q1, const LQuaternion &q2,
                             float t);
    static LQuaternion nlerp(const LQuaternion &q1, const LQuaternion &q2,
                             float t);

private:
    float wp, xp, yp, zp;
};


inline LQuaternion::LQuaternion() noexcept : wp(1.0f), xp(0.0f), yp(0.0f), zp(0.0f) {}

inline LQuaternion::LQuaternion(float aScalar, float xpos, float ypos, float zpos) noexcept
    : wp(aScalar), xp(xpos), yp(ypos), zp(zpos) {}



inline bool LQuaternion::isNull() const noexcept
{
    return wp == 0.0f && xp == 0.0f && yp == 0.0f && zp == 0.0f;
}

inline bool LQuaternion::isIdentity() const noexcept
{
    return wp == 1.0f && xp == 0.0f && yp == 0.0f && zp == 0.0f;
}


inline float LQuaternion::x() const noexcept { return xp; }
inline float LQuaternion::y() const noexcept { return yp; }
inline float LQuaternion::z() const noexcept { return zp; }
inline float LQuaternion::scalar() const noexcept { return wp; }

inline void LQuaternion::setX(float aX) noexcept { xp = aX; }
inline void LQuaternion::setY(float aY) noexcept { yp = aY; }
inline void LQuaternion::setZ(float aZ) noexcept { zp = aZ; }
inline void LQuaternion::setScalar(float aScalar) noexcept { wp = aScalar; }

inline float LQuaternion::dotProduct(const LQuaternion &q1, const LQuaternion &q2) noexcept
{
    return q1.wp * q2.wp + q1.xp * q2.xp + q1.yp * q2.yp + q1.zp * q2.zp;
}

inline LQuaternion LQuaternion::inverted() const noexcept
{
    // Need some extra precision if the length is very small.
    double len = double(wp) * double(wp) +
                 double(xp) * double(xp) +
                 double(yp) * double(yp) +
                 double(zp) * double(zp);
    if (!lFuzzyIsZero(len))
        return LQuaternion(float(double(wp) / len), float(double(-xp) / len),
                           float(double(-yp) / len), float(double(-zp) / len));
    return LQuaternion(0.0f, 0.0f, 0.0f, 0.0f);
}

inline LQuaternion LQuaternion::conjugated() const noexcept
{
    return LQuaternion(wp, -xp, -yp, -zp);
}

inline LQuaternion &LQuaternion::operator+=(const LQuaternion &quaternion) noexcept
{
    wp += quaternion.wp;
    xp += quaternion.xp;
    yp += quaternion.yp;
    zp += quaternion.zp;
    return *this;
}

inline LQuaternion &LQuaternion::operator-=(const LQuaternion &quaternion) noexcept
{
    wp -= quaternion.wp;
    xp -= quaternion.xp;
    yp -= quaternion.yp;
    zp -= quaternion.zp;
    return *this;
}

inline LQuaternion &LQuaternion::operator*=(float factor) noexcept
{
    wp *= factor;
    xp *= factor;
    yp *= factor;
    zp *= factor;
    return *this;
}

inline LQuaternion operator*(const LQuaternion &q1, const LQuaternion &q2) noexcept
{
    float yy = (q1.wp - q1.yp) * (q2.wp + q2.zp);
    float zz = (q1.wp + q1.yp) * (q2.wp - q2.zp);
    float ww = (q1.zp + q1.xp) * (q2.xp + q2.yp);
    float xx = ww + yy + zz;
    float qq = 0.5f * (xx + (q1.zp - q1.xp) * (q2.xp - q2.yp));

    float w = qq - ww + (q1.zp - q1.yp) * (q2.yp - q2.zp);
    float x = qq - xx + (q1.xp + q1.wp) * (q2.xp + q2.wp);
    float y = qq - yy + (q1.wp - q1.xp) * (q2.yp + q2.zp);
    float z = qq - zz + (q1.zp + q1.yp) * (q2.wp - q2.xp);

    return LQuaternion(w, x, y, z);
}

inline LQuaternion &LQuaternion::operator*=(const LQuaternion &quaternion) noexcept
{
    *this = *this * quaternion;
    return *this;
}

inline LQuaternion &LQuaternion::operator/=(float divisor)
{
    wp /= divisor;
    xp /= divisor;
    yp /= divisor;
    zp /= divisor;
    return *this;
}

inline LQuaternion operator+(const LQuaternion &q1, const LQuaternion &q2) noexcept
{
    return LQuaternion(q1.wp + q2.wp, q1.xp + q2.xp, q1.yp + q2.yp, q1.zp + q2.zp);
}

inline LQuaternion operator-(const LQuaternion &q1, const LQuaternion &q2) noexcept
{
    return LQuaternion(q1.wp - q2.wp, q1.xp - q2.xp, q1.yp - q2.yp, q1.zp - q2.zp);
}

inline LQuaternion operator*(float factor, const LQuaternion &quaternion) noexcept
{
    return LQuaternion(quaternion.wp * factor, quaternion.xp * factor, quaternion.yp * factor, quaternion.zp * factor);
}

inline LQuaternion operator*(const LQuaternion &quaternion, float factor) noexcept
{
    return LQuaternion(quaternion.wp * factor, quaternion.xp * factor, quaternion.yp * factor, quaternion.zp * factor);
}

inline LQuaternion operator-(const LQuaternion &quaternion) noexcept
{
    return LQuaternion(-quaternion.wp, -quaternion.xp, -quaternion.yp, -quaternion.zp);
}

inline LQuaternion operator/(const LQuaternion &quaternion, float divisor)
{
    return LQuaternion(quaternion.wp / divisor, quaternion.xp / divisor, quaternion.yp / divisor, quaternion.zp / divisor);
}

inline bool lFuzzyCompare(const LQuaternion &q1, const LQuaternion &q2) noexcept
{
    return fuzzyCompare(q1.wp, q2.wp)
    && fuzzyCompare(q1.xp, q2.xp)
        && fuzzyCompare(q1.yp, q2.yp)
        && fuzzyCompare(q1.zp, q2.zp);
}


inline void LQuaternion::getEulerAngles(float *pitch, float *yaw, float *roll) const
{
    const auto angles = eulerAngles();
    *pitch = angles.pitch;
    *yaw   = angles.yaw;
    *roll  = angles.roll;
}

inline LQuaternion::LQuaternion(float aScalar, const LVector3D &aVector) noexcept
    : wp(aScalar), xp(aVector.x()), yp(aVector.y()), zp(aVector.z()) {}

inline void LQuaternion::setVector(const LVector3D &aVector) noexcept
{
    xp = aVector.x();
    yp = aVector.y();
    zp = aVector.z();
}

inline LVector3D LQuaternion::vector() const noexcept
{
    return LVector3D(xp, yp, zp);
}

inline LVector3D operator*(const LQuaternion &quaternion, const LVector3D &vec)
{
    return quaternion.rotatedVector(vec);
}

inline void LQuaternion::getAxisAndAngle(LVector3D *axis, float *angle) const
{
    float aX, aY, aZ;
    getAxisAndAngle(&aX, &aY, &aZ, angle);
    *axis = LVector3D(aX, aY, aZ);
}

inline LVector3D LQuaternion::toEulerAngles() const
{
    const auto angles = eulerAngles();
    return LVector3D{angles.pitch, angles.yaw, angles.roll};
}

inline void LQuaternion::getAxes(LVector3D *xAxis, LVector3D *yAxis, LVector3D *zAxis) const
{
    const Axes axes = toAxes();
    *xAxis = axes.x.toVector3D();
    *yAxis = axes.y.toVector3D();
    *zAxis = axes.z.toVector3D();
}

inline LQuaternion LQuaternion::fromAxes(const LVector3D &xAxis, const LVector3D &yAxis, const LVector3D &zAxis)
{
    return fromAxes(Axes{Axis::fromVector3D(xAxis),
                         Axis::fromVector3D(yAxis),
                         Axis::fromVector3D(zAxis)});
}

inline void LQuaternion::setVector(float aX, float aY, float aZ) noexcept
{
    xp = aX;
    yp = aY;
    zp = aZ;
}

inline LQuaternion::LQuaternion(const LVector4D &aVector) noexcept
    : wp(aVector.w()), xp(aVector.x()), yp(aVector.y()), zp(aVector.z()) {}

inline LVector4D LQuaternion::toVector4D() const noexcept
{
    return LVector4D(xp, yp, zp, wp);
}

LK_NAMESPACE_END

#endif // LQUATERNION_H
