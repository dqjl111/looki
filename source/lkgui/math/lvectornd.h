#ifndef LVECTORND_H
#define LVECTORND_H

#include "lkgui/lpoint.h"
#include "lkgui/lrect.h"
#include "lkcore/lcoreenums.h"
#include "lkgui/math/lmath.h"

LK_NAMESPACE_BEGIN

class LVector3D;
class LVector4D;
class LMatrix4x4;

class LKGUIDEF LVector2D
{
public:
    LVector2D() noexcept;
    explicit LVector2D(Initialization) noexcept {}
    LVector2D(float xpos, float ypos) noexcept;
    explicit LVector2D(LPoint<int> point) noexcept;
    explicit LVector2D(LPoint<double> point) noexcept;
    explicit LVector2D(LVector3D vector) noexcept;
    explicit LVector2D(LVector4D vector) noexcept;


    bool isNull() const noexcept;

    float x() const noexcept;
    float y() const noexcept;

    void setX(float x) noexcept;
    void setY(float y) noexcept;

    float &operator[](int i);
    float operator[](int i) const;

    [[nodiscard]] float length() const noexcept;
    [[nodiscard]] float lengthSquared() const noexcept;

    [[nodiscard]] LVector2D normalized() const noexcept;
    void normalize() noexcept;

    [[nodiscard]] float distanceToPoint(LVector2D point) const noexcept;
    [[nodiscard]] float distanceToLine(LVector2D point, LVector2D direction) const noexcept;

    LVector2D &operator+=(LVector2D vector) noexcept;
    LVector2D &operator-=(LVector2D vector) noexcept;
    LVector2D &operator*=(float factor) noexcept;
    LVector2D &operator*=(LVector2D vector) noexcept;
    LVector2D &operator/=(float divisor);
    LVector2D &operator/=(LVector2D vector);

    [[nodiscard]] static float dotProduct(LVector2D v1, LVector2D v2) noexcept;

    friend inline bool operator==(LVector2D v1, LVector2D v2) noexcept
    {
        return v1.v[0] == v2.v[0] && v1.v[1] == v2.v[1];
    }

    friend inline bool operator!=(LVector2D v1, LVector2D v2) noexcept
    {
        return v1.v[0] != v2.v[0] || v1.v[1] != v2.v[1];
    }

    friend inline LVector2D operator+(LVector2D v1, LVector2D v2) noexcept
    {
        return LVector2D(v1.v[0] + v2.v[0], v1.v[1] + v2.v[1]);
    }

    friend inline LVector2D operator-(LVector2D v1, LVector2D v2) noexcept
    {
        return LVector2D(v1.v[0] - v2.v[0], v1.v[1] - v2.v[1]);
    }

    friend inline LVector2D operator*(float factor, LVector2D vector) noexcept
    {
        return LVector2D(vector.v[0] * factor, vector.v[1] * factor);
    }

    friend inline LVector2D operator*(LVector2D vector, float factor) noexcept
    {
        return LVector2D(vector.v[0] * factor, vector.v[1] * factor);
    }

    friend inline LVector2D operator*(LVector2D v1, LVector2D v2) noexcept
    {
        return LVector2D(v1.v[0] * v2.v[0], v1.v[1] * v2.v[1]);
    }

    friend inline LVector2D operator-(LVector2D vector) noexcept
    {
        return LVector2D(-vector.v[0], -vector.v[1]);
    }

    friend inline LVector2D operator/(LVector2D vector, float divisor)
    {
        return LVector2D(vector.v[0] / divisor, vector.v[1] / divisor);
    }

    friend inline LVector2D operator/(LVector2D vector, LVector2D divisor)
    {
        return LVector2D(vector.v[0] / divisor.v[0], vector.v[1] / divisor.v[1]);
    }

    friend LKGUIDEF bool lFuzzyCompare(const LVector2D& v1, const LVector2D& v2) noexcept;

    LVector3D toVector3D() const noexcept;
    LVector4D toVector4D() const noexcept;
    LPoint<int> toPoint() const noexcept;
    LPoint<double> toPointF() const noexcept;

private:
    float v[2];

    friend class LVector3D;
    friend class LVector4D;

    template <std::size_t I,
             typename V,
             std::enable_if_t<(I < 2), bool> = true,
             std::enable_if_t<std::is_same_v<std::remove_cv_t<std::remove_reference_t<V>>, LVector2D>, bool> = true>
    friend decltype(auto) get(V &&vec) noexcept
    {
        return std::forward_like<V>(vec.v[I]);
    }
};


class LKGUIDEF LVector3D
{
public:
    LVector3D() noexcept;
    explicit LVector3D(Initialization) noexcept {}
    LVector3D(float xpos, float ypos, float zpos) noexcept : v{xpos, ypos, zpos} {}

    explicit LVector3D(LPoint<int> point) noexcept;
    explicit LVector3D(LPoint<double> point) noexcept;
    explicit LVector3D(LVector2D vector) noexcept;
    LVector3D(LVector2D vector, float zpos) noexcept;
    explicit LVector3D(LVector4D vector) noexcept;
    bool isNull() const noexcept;

    float x() const noexcept;
    float y() const noexcept;
    float z() const noexcept;

    void setX(float x) noexcept;
    void setY(float y) noexcept;
    void setZ(float z) noexcept;

    float &operator[](int i);
    float operator[](int i) const;

    [[nodiscard]] float length() const noexcept;
    [[nodiscard]] float lengthSquared() const noexcept;

    [[nodiscard]] LVector3D normalized() const noexcept;
    void normalize() noexcept;

    LVector3D &operator+=(LVector3D vector) noexcept;
    LVector3D &operator-=(LVector3D vector) noexcept;
    LVector3D &operator*=(float factor) noexcept;
    LVector3D &operator*=(LVector3D vector) noexcept;
    LVector3D &operator/=(float divisor);
    LVector3D &operator/=(LVector3D vector);

    [[nodiscard]] static float dotProduct(LVector3D v1, LVector3D v2) noexcept;
    [[nodiscard]] static LVector3D crossProduct(LVector3D v1, LVector3D v2) noexcept;

    [[nodiscard]] static LVector3D normal(LVector3D v1, LVector3D v2) noexcept;
    [[nodiscard]] static LVector3D normal(LVector3D v1, LVector3D v2, LVector3D v3) noexcept;

    LVector3D project(const LMatrix4x4 &modelView, const LMatrix4x4 &projection, const LRect<int> &viewport) const;
    LVector3D unproject(const LMatrix4x4 &modelView, const LMatrix4x4 &projection, const LRect<int> &viewport) const;


    friend inline bool operator==(LVector3D v1, LVector3D v2) noexcept
    {
        return v1.v[0] == v2.v[0] && v1.v[1] == v2.v[1] && v1.v[2] == v2.v[2];
    }

    friend inline bool operator!=(LVector3D v1, LVector3D v2) noexcept
    {
        return v1.v[0] != v2.v[0] || v1.v[1] != v2.v[1] || v1.v[2] != v2.v[2];
    }

    float distanceToPoint(LVector3D point) const noexcept;
    float distanceToPlane(LVector3D plane, LVector3D normal) const noexcept;
    float distanceToPlane(LVector3D plane1, LVector3D plane2, LVector3D plane3) const noexcept;
    float distanceToLine(LVector3D point, LVector3D direction) const noexcept;


    friend inline LVector3D operator+(LVector3D v1, LVector3D v2) noexcept
    {
        return LVector3D(v1.v[0] + v2.v[0], v1.v[1] + v2.v[1], v1.v[2] + v2.v[2]);
    }

    friend inline LVector3D operator-(LVector3D v1, LVector3D v2) noexcept
    {
        return LVector3D(v1.v[0] - v2.v[0], v1.v[1] - v2.v[1], v1.v[2] - v2.v[2]);
    }

    friend inline LVector3D operator*(float factor, LVector3D vector) noexcept
    {
        return LVector3D(vector.v[0] * factor, vector.v[1] * factor, vector.v[2] * factor);
    }

    friend inline LVector3D operator*(LVector3D vector, float factor) noexcept
    {
        return LVector3D(vector.v[0] * factor, vector.v[1] * factor, vector.v[2] * factor);
    }

    friend inline LVector3D operator*(LVector3D v1, LVector3D v2) noexcept
    {
        return LVector3D(v1.v[0] * v2.v[0], v1.v[1] * v2.v[1], v1.v[2] * v2.v[2]);
    }

    friend inline LVector3D operator-(LVector3D vector) noexcept
    {
        return LVector3D(-vector.v[0], -vector.v[1], -vector.v[2]);
    }

    friend inline LVector3D operator/(LVector3D vector, float divisor)
    {
        return LVector3D(vector.v[0] / divisor, vector.v[1] / divisor, vector.v[2] / divisor);
    }

    friend inline LVector3D operator/(LVector3D vector, LVector3D divisor)
    {
        return LVector3D(vector.v[0] / divisor.v[0], vector.v[1] / divisor.v[1],
                         vector.v[2] / divisor.v[2]);
    }

    friend LKGUIDEF bool lFuzzyCompare(const LVector3D& v1, const LVector3D& v2) noexcept;

    LVector2D toVector2D() const noexcept;
    LVector4D toVector4D() const noexcept;

    LPoint<int> toPoint() const noexcept;
    LPoint<double> toPointF() const noexcept;
private:
    float v[3];

    friend class LVector2D;
    friend class LVector4D;

    friend LVector3D operator*(const LVector3D& vector, const LMatrix4x4& matrix);
    friend LVector3D operator*(const LMatrix4x4& matrix, const LVector3D& vector);

    template <std::size_t I,
             typename V,
             std::enable_if_t<(I < 3), bool> = true,
             std::enable_if_t<std::is_same_v<std::remove_cv_t<std::remove_reference_t<V>>, LVector3D>, bool> = true>
    friend decltype(auto) get(V &&vec) noexcept
    {
        return std::forward_like<V>(vec.v[I]);
    }
};



class LVector4D
{
public:
    LVector4D() noexcept;
    explicit LVector4D(Initialization) noexcept {}
    LVector4D(float xpos, float ypos, float zpos, float wpos) noexcept;
    explicit LVector4D(LPoint<int> point) noexcept;
    explicit LVector4D(LPoint<double> point) noexcept;

    explicit LVector4D(LVector2D vector) noexcept;
    LVector4D(LVector2D vector, float zpos, float wpos) noexcept;
    explicit LVector4D(LVector3D vector) noexcept;
    LVector4D(LVector3D vector, float wpos) noexcept;


    bool isNull() const noexcept;

    float x() const noexcept;
    float y() const noexcept;
    float z() const noexcept;
    float w() const noexcept;

    void setX(float x) noexcept;
    void setY(float y) noexcept;
    void setZ(float z) noexcept;
    void setW(float w) noexcept;

    float &operator[](int i);
    float operator[](int i) const;

    [[nodiscard]] float length() const noexcept;
    [[nodiscard]] float lengthSquared() const noexcept;

    [[nodiscard]] LVector4D normalized() const noexcept;
    void normalize() noexcept;

    LVector4D &operator+=(LVector4D vector) noexcept;
    LVector4D &operator-=(LVector4D vector) noexcept;
    LVector4D &operator*=(float factor) noexcept;
    LVector4D &operator*=(LVector4D vector) noexcept;
    LVector4D &operator/=(float divisor);
    inline LVector4D &operator/=(LVector4D vector);

    [[nodiscard]] static float dotProduct(LVector4D v1, LVector4D v2) noexcept;

    friend inline bool operator==(LVector4D v1, LVector4D v2) noexcept
    {
        return v1.v[0] == v2.v[0] && v1.v[1] == v2.v[1] && v1.v[2] == v2.v[2] && v1.v[3] == v2.v[3];
    }

    friend inline bool operator!=(LVector4D v1, LVector4D v2) noexcept
    {
        return v1.v[0] != v2.v[0] || v1.v[1] != v2.v[1] || v1.v[2] != v2.v[2] || v1.v[3] != v2.v[3];
    }

    friend inline LVector4D operator+(LVector4D v1, LVector4D v2) noexcept
    {
        return LVector4D(v1.v[0] + v2.v[0], v1.v[1] + v2.v[1], v1.v[2] + v2.v[2], v1.v[3] + v2.v[3]);
    }

    friend inline LVector4D operator-(LVector4D v1, LVector4D v2) noexcept
    {
        return LVector4D(v1.v[0] - v2.v[0], v1.v[1] - v2.v[1], v1.v[2] - v2.v[2], v1.v[3] - v2.v[3]);
    }

    friend inline LVector4D operator*(float factor, LVector4D vector) noexcept
    {
        return LVector4D(vector.v[0] * factor, vector.v[1] * factor, vector.v[2] * factor, vector.v[3] * factor);
    }

    friend inline LVector4D operator*(LVector4D vector, float factor) noexcept
    {
        return LVector4D(vector.v[0] * factor, vector.v[1] * factor, vector.v[2] * factor, vector.v[3] * factor);
    }

    friend inline LVector4D operator*(LVector4D v1, LVector4D v2) noexcept
    {
        return LVector4D(v1.v[0] * v2.v[0], v1.v[1] * v2.v[1], v1.v[2] * v2.v[2], v1.v[3] * v2.v[3]);
    }

    friend inline LVector4D operator-(LVector4D vector) noexcept
    {
        return LVector4D(-vector.v[0], -vector.v[1], -vector.v[2], -vector.v[3]);
    }

    friend inline LVector4D operator/(LVector4D vector, float divisor)
    {
        return LVector4D(vector.v[0] / divisor, vector.v[1] / divisor, vector.v[2] / divisor, vector.v[3] / divisor);
    }

    friend inline LVector4D operator/(LVector4D vector, LVector4D divisor)
    {
        return LVector4D(vector.v[0] / divisor.v[0], vector.v[1] / divisor.v[1],
                         vector.v[2] / divisor.v[2], vector.v[3] / divisor.v[3]);
    }

    friend LKGUIDEF bool lFuzzyCompare(const LVector4D& v1, const LVector4D& v2) noexcept;

    LVector2D toVector2D() const noexcept;
    LVector2D toVector2DAffine() const noexcept;

    LVector3D toVector3D() const noexcept;
    LVector3D toVector3DAffine() const noexcept;


    LPoint<int> toPoint() const noexcept;
    LPoint<double> toPointF() const noexcept;
private:
    float v[4];

    friend class LVector2D;
    friend class LVector3D;
    friend class LMatrix4x4;

    friend LVector4D operator*(const LVector4D& vector, const LMatrix4x4& matrix);
    friend LVector4D operator*(const LMatrix4x4& matrix, const LVector4D& vector);

    template <std::size_t I,
             typename V,
             std::enable_if_t<(I < 4), bool> = true,
             std::enable_if_t<std::is_same_v<std::remove_cv_t<std::remove_reference_t<V>>, LVector4D>, bool> = true>
    friend decltype(auto) get(V &&vec) noexcept
    {
        return std::forward_like<V>(vec.v[I]);
    }
};


inline LVector2D::LVector2D() noexcept : v{0.0f, 0.0f} {}
inline LVector2D::LVector2D(float xpos, float ypos) noexcept : v{xpos, ypos} {}
inline LVector2D::LVector2D(LPoint<int> point) noexcept : v{float(point.m_x), float(point.m_y)} {}
inline LVector2D::LVector2D(LPoint<double> point) noexcept : v{float(point.m_x), float(point.m_y)} {}
inline LVector2D::LVector2D(LVector3D vector) noexcept : v{vector[0], vector[1]} {}
inline LVector2D::LVector2D(LVector4D vector) noexcept : v{vector[0], vector[1]} {}


inline bool LVector2D::isNull() const noexcept
{
    return lIsZero(v[0]) && lIsZero(v[1]);
}

inline float LVector2D::x() const noexcept { return v[0]; }
inline float LVector2D::y() const noexcept { return v[1]; }

inline void LVector2D::setX(float aX) noexcept { v[0] = aX; }
inline void LVector2D::setY(float aY) noexcept { v[1] = aY; }

inline float &LVector2D::operator[](int i)
{
    return v[i];
}

inline float LVector2D::operator[](int i) const
{
    return v[i];
}

inline float LVector2D::length() const noexcept
{
    return std::hypot(v[0], v[1]);
}

inline float LVector2D::lengthSquared() const noexcept
{
    return v[0] * v[0] + v[1] * v[1];
}

inline LVector2D LVector2D::normalized() const noexcept
{
    const float len = length();
    return lFuzzyIsZero(len - 1.0f) ? *this : lFuzzyIsZero(len) ? LVector2D()
                                                                : LVector2D(v[0] / len, v[1] / len);
}

inline void LVector2D::normalize() noexcept
{
    const float len = length();
    if (lFuzzyIsZero(len - 1.0f) || lFuzzyIsZero(len))
        return;

    v[0] /= len;
    v[1] /= len;
}

inline float LVector2D::distanceToPoint(LVector2D point) const noexcept
{
    return (*this - point).length();
}

inline float LVector2D::distanceToLine(LVector2D point, LVector2D direction) const noexcept
{
    if (direction.isNull())
        return (*this - point).length();
    LVector2D p = point + dotProduct(*this - point, direction) * direction;
    return (*this - p).length();
}

inline LVector2D &LVector2D::operator+=(LVector2D vector) noexcept
{
    v[0] += vector.v[0];
    v[1] += vector.v[1];
    return *this;
}

inline LVector2D &LVector2D::operator-=(LVector2D vector) noexcept
{
    v[0] -= vector.v[0];
    v[1] -= vector.v[1];
    return *this;
}

inline LVector2D &LVector2D::operator*=(float factor) noexcept
{
    v[0] *= factor;
    v[1] *= factor;
    return *this;
}

inline LVector2D &LVector2D::operator*=(LVector2D vector) noexcept
{
    v[0] *= vector.v[0];
    v[1] *= vector.v[1];
    return *this;
}

inline LVector2D &LVector2D::operator/=(float divisor)
{
    v[0] /= divisor;
    v[1] /= divisor;
    return *this;
}

inline LVector2D &LVector2D::operator/=(LVector2D vector)
{
    v[0] /= vector.v[0];
    v[1] /= vector.v[1];
    return *this;
}

inline float LVector2D::dotProduct(LVector2D v1, LVector2D v2) noexcept
{
    return v1.v[0] * v2.v[0] + v1.v[1] * v2.v[1];
}

inline LVector3D LVector2D::toVector3D() const noexcept
{
    return LVector3D(v[0], v[1], 0.0f);
}

inline LVector4D LVector2D::toVector4D() const noexcept
{
    return LVector4D(v[0], v[1], 0.0f, 0.0f);
}



inline LPoint<int> LVector2D::toPoint() const noexcept
{
    return LPoint<int>(std::round(v[0]), std::round(v[1]));
}

inline LPoint<double> LVector2D::toPointF() const noexcept
{
    return LPoint<double>(double(v[0]), double(v[1]));
}


inline LVector3D::LVector3D() noexcept : v{0.0f, 0.0f, 0.0f} {}

inline LVector3D::LVector3D(LPoint<int> point) noexcept : v{float(point.m_x), float(point.m_y), 0.0f} {}

inline LVector3D::LVector3D(LPoint<double> point) noexcept : v{float(point.m_x), float(point.m_y), 0.0f} {}


inline LVector3D::LVector3D(LVector2D vector) noexcept : v{vector[0], vector[1], 0.0f} {}
inline LVector3D::LVector3D(LVector2D vector, float zpos) noexcept : v{vector[0], vector[1], zpos} {}

inline LVector3D::LVector3D(LVector4D vector) noexcept : v{vector[0], vector[1], vector[2]} {}


inline bool LVector3D::isNull() const noexcept
{
    return lIsZero(v[0]) && lIsZero(v[1]) && lIsZero(v[2]);
}

inline float LVector3D::x() const noexcept { return v[0]; }
inline float LVector3D::y() const noexcept { return v[1]; }
inline float LVector3D::z() const noexcept { return v[2]; }

inline void LVector3D::setX(float aX) noexcept { v[0] = aX; }
inline void LVector3D::setY(float aY) noexcept { v[1] = aY; }
inline void LVector3D::setZ(float aZ) noexcept { v[2] = aZ; }

inline float &LVector3D::operator[](int i)
{
    return v[i];
}

inline float LVector3D::operator[](int i) const
{
    return v[i];
}

inline float LVector3D::length() const noexcept
{
    return std::hypot(v[0], v[1], v[2]);
}

inline LVector3D LVector3D::normalized() const noexcept
{
    const float len = length();
    return lFuzzyIsZero(len - 1.0f) ? *this : lFuzzyIsZero(len) ? LVector3D()
                                                                : LVector3D(v[0] / len, v[1] / len, v[2] / len);
}

inline void LVector3D::normalize() noexcept
{
    const float len = length();
    if (lFuzzyIsZero(len - 1.0f) || lFuzzyIsZero(len))
        return;

    v[0] /= len;
    v[1] /= len;
    v[2] /= len;
}

inline float LVector3D::lengthSquared() const noexcept
{
    return v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
}

inline LVector3D &LVector3D::operator+=(LVector3D vector) noexcept
{
    v[0] += vector.v[0];
    v[1] += vector.v[1];
    v[2] += vector.v[2];
    return *this;
}

inline LVector3D &LVector3D::operator-=(LVector3D vector) noexcept
{
    v[0] -= vector.v[0];
    v[1] -= vector.v[1];
    v[2] -= vector.v[2];
    return *this;
}

inline LVector3D &LVector3D::operator*=(float factor) noexcept
{
    v[0] *= factor;
    v[1] *= factor;
    v[2] *= factor;
    return *this;
}

inline LVector3D &LVector3D::operator*=(LVector3D vector) noexcept
{
    v[0] *= vector.v[0];
    v[1] *= vector.v[1];
    v[2] *= vector.v[2];
    return *this;
}

inline LVector3D &LVector3D::operator/=(float divisor)
{
    v[0] /= divisor;
    v[1] /= divisor;
    v[2] /= divisor;
    return *this;
}

inline LVector3D &LVector3D::operator/=(LVector3D vector)
{
    v[0] /= vector.v[0];
    v[1] /= vector.v[1];
    v[2] /= vector.v[2];
    return *this;
}

inline float LVector3D::dotProduct(LVector3D v1, LVector3D v2) noexcept
{
    return v1.v[0] * v2.v[0] + v1.v[1] * v2.v[1] + v1.v[2] * v2.v[2];
}

inline LVector3D LVector3D::crossProduct(LVector3D v1, LVector3D v2) noexcept
{
    return LVector3D(v1.v[1] * v2.v[2] - v1.v[2] * v2.v[1],
                     v1.v[2] * v2.v[0] - v1.v[0] * v2.v[2],
                     v1.v[0] * v2.v[1] - v1.v[1] * v2.v[0]);
}

inline LVector3D LVector3D::normal(LVector3D v1, LVector3D v2) noexcept
{
    return crossProduct(v1, v2).normalized();
}

inline LVector3D LVector3D::normal(LVector3D v1, LVector3D v2, LVector3D v3) noexcept
{
    return crossProduct((v2 - v1), (v3 - v1)).normalized();
}

inline float LVector3D::distanceToPoint(LVector3D point) const noexcept
{
    return (*this - point).length();
}

inline float LVector3D::distanceToPlane(LVector3D plane, LVector3D normal) const noexcept
{
    return dotProduct(*this - plane, normal);
}

inline float LVector3D::distanceToPlane(LVector3D plane1, LVector3D plane2, LVector3D plane3) const noexcept
{
    LVector3D n = normal(plane2 - plane1, plane3 - plane1);
    return dotProduct(*this - plane1, n);
}

inline float LVector3D::distanceToLine(LVector3D point, LVector3D direction) const noexcept
{
    if (direction.isNull())
        return (*this - point).length();
    LVector3D p = point + dotProduct(*this - point, direction) * direction;
    return (*this - p).length();
}


inline LVector2D LVector3D::toVector2D() const noexcept
{
    return LVector2D(v[0], v[1]);
}

inline LVector4D LVector3D::toVector4D() const noexcept
{
    return LVector4D(v[0], v[1], v[2], 0.0f);
}


inline LPoint<int> LVector3D::toPoint() const noexcept
{
    return LPoint<int>(std::round(v[0]), std::round(v[1]));
}

inline LPoint<double> LVector3D::toPointF() const noexcept
{
    return LPoint<double>(double(v[0]), double(v[1]));
}


inline LVector4D::LVector4D() noexcept : v{0.0f, 0.0f, 0.0f, 0.0f} {}

inline LVector4D::LVector4D(float xpos, float ypos, float zpos, float wpos) noexcept : v{xpos, ypos, zpos, wpos} {}

inline LVector4D::LVector4D(LPoint<int> point) noexcept : v{float(point.m_x), float(point.m_y), 0.0f, 0.0f} {}

inline LVector4D::LVector4D(LPoint<double> point) noexcept : v{float(point.m_x), float(point.m_y), 0.0f, 0.0f} {}

inline LVector4D::LVector4D(LVector2D vector) noexcept : v{vector[0], vector[1], 0.0f, 0.0f} {}
inline LVector4D::LVector4D(LVector2D vector, float zpos, float wpos) noexcept : v{vector[0], vector[1], zpos, wpos} {}
inline LVector4D::LVector4D(LVector3D vector) noexcept : v{vector[0], vector[1], vector[2], 0.0f} {}
inline LVector4D::LVector4D(LVector3D vector, float wpos) noexcept : v{vector[0], vector[1], vector[2], wpos} {}
inline bool LVector4D::isNull() const noexcept
{
    return lIsZero(v[0]) && lIsZero(v[1]) && lIsZero(v[2]) && lIsZero(v[3]);
}

inline float LVector4D::x() const noexcept { return v[0]; }
inline float LVector4D::y() const noexcept { return v[1]; }
inline float LVector4D::z() const noexcept { return v[2]; }
inline float LVector4D::w() const noexcept { return v[3]; }

inline void LVector4D::setX(float aX) noexcept { v[0] = aX; }
inline void LVector4D::setY(float aY) noexcept { v[1] = aY; }
inline void LVector4D::setZ(float aZ) noexcept { v[2] = aZ; }
inline void LVector4D::setW(float aW) noexcept { v[3] = aW; }

inline float &LVector4D::operator[](int i)
{
    return v[i];
}

inline float LVector4D::operator[](int i) const
{
    return v[i];
}

inline float LVector4D::length() const noexcept
{
    return lHypot(v[0], v[1], v[2], v[3]);
}

inline float LVector4D::lengthSquared() const noexcept
{
    return v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3];
}

inline LVector4D LVector4D::normalized() const noexcept
{
    const float len = length();
    return lFuzzyIsZero(len - 1.0f) ? *this : lFuzzyIsZero(len) ? LVector4D()
                                                                : LVector4D(v[0] / len, v[1] / len, v[2] / len, v[3] / len);
}

inline void LVector4D::normalize() noexcept
{
    const float len = length();
    if (lFuzzyIsZero(len - 1.0f) || lFuzzyIsZero(len))
        return;

    v[0] /= len;
    v[1] /= len;
    v[2] /= len;
    v[3] /= len;
}

inline LVector4D &LVector4D::operator+=(LVector4D vector) noexcept
{
    v[0] += vector.v[0];
    v[1] += vector.v[1];
    v[2] += vector.v[2];
    v[3] += vector.v[3];
    return *this;
}

inline LVector4D &LVector4D::operator-=(LVector4D vector) noexcept
{
    v[0] -= vector.v[0];
    v[1] -= vector.v[1];
    v[2] -= vector.v[2];
    v[3] -= vector.v[3];
    return *this;
}

inline LVector4D &LVector4D::operator*=(float factor) noexcept
{
    v[0] *= factor;
    v[1] *= factor;
    v[2] *= factor;
    v[3] *= factor;
    return *this;
}

inline LVector4D &LVector4D::operator*=(LVector4D vector) noexcept
{
    v[0] *= vector.v[0];
    v[1] *= vector.v[1];
    v[2] *= vector.v[2];
    v[3] *= vector.v[3];
    return *this;
}

inline LVector4D &LVector4D::operator/=(float divisor)
{
    v[0] /= divisor;
    v[1] /= divisor;
    v[2] /= divisor;
    v[3] /= divisor;
    return *this;
}

inline LVector4D &LVector4D::operator/=(LVector4D vector)
{
    v[0] /= vector.v[0];
    v[1] /= vector.v[1];
    v[2] /= vector.v[2];
    v[3] /= vector.v[3];
    return *this;
}

inline float LVector4D::dotProduct(LVector4D v1, LVector4D v2) noexcept
{
    return v1.v[0] * v2.v[0] + v1.v[1] * v2.v[1] + v1.v[2] * v2.v[2] + v1.v[3] * v2.v[3];
}

inline LVector2D LVector4D::toVector2D() const noexcept
{
    return LVector2D(v[0], v[1]);
}

inline LVector2D LVector4D::toVector2DAffine() const noexcept
{
    if (lIsZero(v[3]))
        return LVector2D();
    return LVector2D(v[0] / v[3], v[1] / v[3]);
}

inline LVector3D LVector4D::toVector3D() const noexcept
{
    return LVector3D(v[0], v[1], v[2]);
}

inline LVector3D LVector4D::toVector3DAffine() const noexcept
{
    if (lIsZero(v[3]))
        return LVector3D();
    return LVector3D(v[0] / v[3], v[1] / v[3], v[2] / v[3]);
}

inline LPoint<int> LVector4D::toPoint() const noexcept
{
    return LPoint<int>(std::lround(v[0]), std::lround(v[1]));
}

inline LPoint<double> LVector4D::toPointF() const noexcept
{
    return LPoint<double>(double(v[0]), double(v[1]));
}


LK_NAMESPACE_END
#endif // LVECTORND_H
