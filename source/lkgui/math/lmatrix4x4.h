#ifndef LMATRIX4X4_H
#define LMATRIX4X4_H

#include "lkgui/math/lvectornd.h"
#include "lkgui/math/lgenericmatrix.h"
#include "lkgui/paint/ltransform2d.h"
LK_NAMESPACE_BEGIN

class LQuaternion;
enum class LMatrix4x4Flag {
    Identity        = 0x0000, // Identity matrix
    Translation     = 0x0001, // Contains a translation
    Scale           = 0x0002, // Contains a scale
    Rotation2D      = 0x0004, // Contains a rotation about the Z axis
    Rotation        = 0x0008, // Contains an arbitrary rotation
    Perspective     = 0x0010, // Last row is different from (0, 0, 0, 1)
    General         = 0x001f  // General matrix, unknown contents
};

L_EN_FLAGS(LMatrix4x4Flag)

class LKGUIDEF LMatrix4x4
{
public:
    inline LMatrix4x4() { setToIdentity(); }
    explicit LMatrix4x4(Initialization) : flagBits(LMatrix4x4Flag::General) {}
    explicit LMatrix4x4(const float *values);
    inline LMatrix4x4(float m11, float m12, float m13, float m14,
                      float m21, float m22, float m23, float m24,
                      float m31, float m32, float m33, float m34,
                      float m41, float m42, float m43, float m44);

    template <int N, int M>
    explicit LMatrix4x4(const LGenericMatrix<N, M, float>& matrix);

    LMatrix4x4(const float *values, int cols, int rows);
    LMatrix4x4(const LTransform2D& transform);

    inline const float& operator()(int row, int column) const;
    inline float& operator()(int row, int column);

    inline LVector4D column(int index) const;
    inline void setColumn(int index, const LVector4D& value);

    inline LVector4D row(int index) const;
    inline void setRow(int index, const LVector4D& value);

    inline bool isAffine() const;

    inline bool isIdentity() const;
    inline void setToIdentity();

    inline void fill(float value);

    double determinant() const;
    LMatrix4x4 inverted(bool *invertible = nullptr) const;
    LMatrix4x4 transposed() const;
    LMatrix3x3 normalMatrix() const;

    inline LMatrix4x4& operator+=(const LMatrix4x4& other);
    inline LMatrix4x4& operator-=(const LMatrix4x4& other);
    inline LMatrix4x4& operator*=(const LMatrix4x4& other);
    inline LMatrix4x4& operator*=(float factor);
    LMatrix4x4& operator/=(float divisor);
    inline bool operator==(const LMatrix4x4& other) const;
    inline bool operator!=(const LMatrix4x4& other) const;

    friend LMatrix4x4 operator+(const LMatrix4x4& m1, const LMatrix4x4& m2);
    friend LMatrix4x4 operator-(const LMatrix4x4& m1, const LMatrix4x4& m2);
    friend LMatrix4x4 operator*(const LMatrix4x4& m1, const LMatrix4x4& m2);


    friend LVector4D operator*(const LVector4D& vector, const LMatrix4x4& matrix);
    friend LVector4D operator*(const LMatrix4x4& matrix, const LVector4D& vector);

    friend LPoint<int> operator*(const LPoint<int>& point, const LMatrix4x4& matrix);
    friend LPoint<double> operator*(const LPoint<double>& point, const LMatrix4x4& matrix);
    friend LMatrix4x4 operator-(const LMatrix4x4& matrix);

    friend LMatrix4x4 operator*(float factor, const LMatrix4x4& matrix);
    friend LMatrix4x4 operator*(const LMatrix4x4& matrix, float factor);
    friend LKGUIDEF LMatrix4x4 operator/(const LMatrix4x4& matrix, float divisor);

    friend LKGUIDEF bool lFuzzyCompare(const LMatrix4x4& m1, const LMatrix4x4& m2) noexcept;

    void scale(const LVector3D& vector);
    void translate(const LVector3D& vector);
    void rotate(float angle, const LVector3D& vector);

    void scale(float x, float y);
    void scale(float x, float y, float z);
    void scale(float factor);
    void translate(float x, float y);
    void translate(float x, float y, float z);
    void rotate(float angle, float x, float y, float z = 0.0f);

    void rotate(const LQuaternion& quaternion);
    void ortho(const LRect<int> & rect);
    void ortho(const LRect<double> & rect);
    void ortho(float left, float right, float bottom, float top, float nearPlane, float farPlane);
    void frustum(float left, float right, float bottom, float top, float nearPlane, float farPlane);
    void perspective(float verticalAngle, float aspectRatio, float nearPlane, float farPlane);
    void lookAt(const LVector3D& eye, const LVector3D& center, const LVector3D& up);
    void viewport(const LRect<double>  &rect);
    void viewport(float left, float bottom, float width, float height, float nearPlane = 0.0f, float farPlane = 1.0f);
    void flipCoordinates();

    void copyDataTo(float *values) const;

    LTransform2D toTransform() const;
    LTransform2D toTransform(float distanceToPlane) const;

    inline LPoint<int> map(const LPoint<int>& point) const;
    inline LPoint<double> map(const LPoint<double>& point) const;
    inline LVector3D map(const LVector3D& point) const;
    inline LVector3D mapVector(const LVector3D& vector) const;
    inline LVector4D map(const LVector4D& point) const;
    LRect<int>  mapRect(const LRect<int> & rect) const;
    LRect<double>  mapRect(const LRect<double> & rect) const;

    template <int N, int M>
    LGenericMatrix<N, M, float> toGenericMatrix() const;

    inline float *data();
    inline const float *data() const { return *m; }
    inline const float *constData() const { return *m; }

    void optimize();
    void projectedRotate(float angle, float x, float y, float z, float distanceToPlane = 1024.0);

    // When matrices are multiplied, the flag bits are or-ed together.
    // Note that the ordering of the bit values matters. (ident < t < s < r2d < r < p)

    using Flags = LFlags<LMatrix4x4Flag>;

    Flags flags() const { return flagBits; }

private:
    float m[4][4];          // Column-major order to match OpenGL.
    Flags flagBits;

    LMatrix4x4 orthonormalInverse() const;
};


inline LMatrix4x4::LMatrix4x4
    (float m11, float m12, float m13, float m14,
     float m21, float m22, float m23, float m24,
     float m31, float m32, float m33, float m34,
     float m41, float m42, float m43, float m44)
{
    m[0][0] = m11; m[0][1] = m21; m[0][2] = m31; m[0][3] = m41;
    m[1][0] = m12; m[1][1] = m22; m[1][2] = m32; m[1][3] = m42;
    m[2][0] = m13; m[2][1] = m23; m[2][2] = m33; m[2][3] = m43;
    m[3][0] = m14; m[3][1] = m24; m[3][2] = m34; m[3][3] = m44;
    flagBits = LMatrix4x4Flag::General;
}

template <int N, int M>
inline LMatrix4x4::LMatrix4x4
    (const LGenericMatrix<N, M, float>& matrix)
{
    const float *values = matrix.constData();
    for (int matrixCol = 0; matrixCol < 4; ++matrixCol) {
        for (int matrixRow = 0; matrixRow < 4; ++matrixRow) {
            if (matrixCol < N && matrixRow < M)
                m[matrixCol][matrixRow] = values[matrixCol * M + matrixRow];
            else if (matrixCol == matrixRow)
                m[matrixCol][matrixRow] = 1.0f;
            else
                m[matrixCol][matrixRow] = 0.0f;
        }
    }
    flagBits = LMatrix4x4Flag::General;
}

template <int N, int M>
LGenericMatrix<N, M, float> LMatrix4x4::toGenericMatrix() const
{
    LGenericMatrix<N, M, float> result;
    float *values = result.data();
    for (int matrixCol = 0; matrixCol < N; ++matrixCol) {
        for (int matrixRow = 0; matrixRow < M; ++matrixRow) {
            if (matrixCol < 4 && matrixRow < 4)
                values[matrixCol * M + matrixRow] = m[matrixCol][matrixRow];
            else if (matrixCol == matrixRow)
                values[matrixCol * M + matrixRow] = 1.0f;
            else
                values[matrixCol * M + matrixRow] = 0.0f;
        }
    }
    return result;
}

inline const float& LMatrix4x4::operator()(int aRow, int aColumn) const
{
    return m[aColumn][aRow];
}

inline float& LMatrix4x4::operator()(int aRow, int aColumn)
{
    flagBits = LMatrix4x4Flag::General;
    return m[aColumn][aRow];
}

inline LVector4D LMatrix4x4::column(int index) const
{
    return LVector4D(m[index][0], m[index][1], m[index][2], m[index][3]);
}

inline void LMatrix4x4::setColumn(int index, const LVector4D& value)
{
    m[index][0] = value.x();
    m[index][1] = value.y();
    m[index][2] = value.z();
    m[index][3] = value.w();
    flagBits = LMatrix4x4Flag::General;
}

inline LVector4D LMatrix4x4::row(int index) const
{
    return LVector4D(m[0][index], m[1][index], m[2][index], m[3][index]);
}

inline void LMatrix4x4::setRow(int index, const LVector4D& value)
{
    m[0][index] = value.x();
    m[1][index] = value.y();
    m[2][index] = value.z();
    m[3][index] = value.w();
    flagBits = LMatrix4x4Flag::General;
}

LKGUIDEF LMatrix4x4 operator/(const LMatrix4x4& matrix, float divisor);

inline bool LMatrix4x4::isAffine() const
{
    return m[0][3] == 0.0f && m[1][3] == 0.0f && m[2][3] == 0.0f && m[3][3] == 1.0f;
}

inline bool LMatrix4x4::isIdentity() const
{
    if (flagBits == LMatrix4x4Flag::Identity)
        return true;
    if (m[0][0] != 1.0f || m[0][1] != 0.0f || m[0][2] != 0.0f)
        return false;
    if (m[0][3] != 0.0f || m[1][0] != 0.0f || m[1][1] != 1.0f)
        return false;
    if (m[1][2] != 0.0f || m[1][3] != 0.0f || m[2][0] != 0.0f)
        return false;
    if (m[2][1] != 0.0f || m[2][2] != 1.0f || m[2][3] != 0.0f)
        return false;
    if (m[3][0] != 0.0f || m[3][1] != 0.0f || m[3][2] != 0.0f)
        return false;
    return (m[3][3] == 1.0f);
}

inline void LMatrix4x4::setToIdentity()
{
    m[0][0] = 1.0f;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[0][3] = 0.0f;
    m[1][0] = 0.0f;
    m[1][1] = 1.0f;
    m[1][2] = 0.0f;
    m[1][3] = 0.0f;
    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = 1.0f;
    m[2][3] = 0.0f;
    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
    m[3][3] = 1.0f;
    flagBits = LMatrix4x4Flag::Identity;
}

inline void LMatrix4x4::fill(float value)
{
    m[0][0] = value;
    m[0][1] = value;
    m[0][2] = value;
    m[0][3] = value;
    m[1][0] = value;
    m[1][1] = value;
    m[1][2] = value;
    m[1][3] = value;
    m[2][0] = value;
    m[2][1] = value;
    m[2][2] = value;
    m[2][3] = value;
    m[3][0] = value;
    m[3][1] = value;
    m[3][2] = value;
    m[3][3] = value;
    flagBits = LMatrix4x4Flag::General;
}

inline LMatrix4x4& LMatrix4x4::operator+=(const LMatrix4x4& other)
{
    m[0][0] += other.m[0][0];
    m[0][1] += other.m[0][1];
    m[0][2] += other.m[0][2];
    m[0][3] += other.m[0][3];
    m[1][0] += other.m[1][0];
    m[1][1] += other.m[1][1];
    m[1][2] += other.m[1][2];
    m[1][3] += other.m[1][3];
    m[2][0] += other.m[2][0];
    m[2][1] += other.m[2][1];
    m[2][2] += other.m[2][2];
    m[2][3] += other.m[2][3];
    m[3][0] += other.m[3][0];
    m[3][1] += other.m[3][1];
    m[3][2] += other.m[3][2];
    m[3][3] += other.m[3][3];
    flagBits = LMatrix4x4Flag::General;
    return *this;
}

inline LMatrix4x4& LMatrix4x4::operator-=(const LMatrix4x4& other)
{
    m[0][0] -= other.m[0][0];
    m[0][1] -= other.m[0][1];
    m[0][2] -= other.m[0][2];
    m[0][3] -= other.m[0][3];
    m[1][0] -= other.m[1][0];
    m[1][1] -= other.m[1][1];
    m[1][2] -= other.m[1][2];
    m[1][3] -= other.m[1][3];
    m[2][0] -= other.m[2][0];
    m[2][1] -= other.m[2][1];
    m[2][2] -= other.m[2][2];
    m[2][3] -= other.m[2][3];
    m[3][0] -= other.m[3][0];
    m[3][1] -= other.m[3][1];
    m[3][2] -= other.m[3][2];
    m[3][3] -= other.m[3][3];
    flagBits = LMatrix4x4Flag::General;
    return *this;
}

inline LMatrix4x4& LMatrix4x4::operator*=(const LMatrix4x4& o)
{
    const LMatrix4x4 other = o; // prevent aliasing when &o == this ### Qt 6: take o by value
    flagBits |= other.flagBits;

    if (uint32_t(flagBits) < (uint32_t)LMatrix4x4Flag::Rotation2D) {
        m[3][0] += m[0][0] * other.m[3][0];
        m[3][1] += m[1][1] * other.m[3][1];
        m[3][2] += m[2][2] * other.m[3][2];

        m[0][0] *= other.m[0][0];
        m[1][1] *= other.m[1][1];
        m[2][2] *= other.m[2][2];
        return *this;
    }

    float m0, m1, m2;
    m0 = m[0][0] * other.m[0][0]
         + m[1][0] * other.m[0][1]
         + m[2][0] * other.m[0][2]
         + m[3][0] * other.m[0][3];
    m1 = m[0][0] * other.m[1][0]
         + m[1][0] * other.m[1][1]
         + m[2][0] * other.m[1][2]
         + m[3][0] * other.m[1][3];
    m2 = m[0][0] * other.m[2][0]
         + m[1][0] * other.m[2][1]
         + m[2][0] * other.m[2][2]
         + m[3][0] * other.m[2][3];
    m[3][0] = m[0][0] * other.m[3][0]
              + m[1][0] * other.m[3][1]
              + m[2][0] * other.m[3][2]
              + m[3][0] * other.m[3][3];
    m[0][0] = m0;
    m[1][0] = m1;
    m[2][0] = m2;

    m0 = m[0][1] * other.m[0][0]
         + m[1][1] * other.m[0][1]
         + m[2][1] * other.m[0][2]
         + m[3][1] * other.m[0][3];
    m1 = m[0][1] * other.m[1][0]
         + m[1][1] * other.m[1][1]
         + m[2][1] * other.m[1][2]
         + m[3][1] * other.m[1][3];
    m2 = m[0][1] * other.m[2][0]
         + m[1][1] * other.m[2][1]
         + m[2][1] * other.m[2][2]
         + m[3][1] * other.m[2][3];
    m[3][1] = m[0][1] * other.m[3][0]
              + m[1][1] * other.m[3][1]
              + m[2][1] * other.m[3][2]
              + m[3][1] * other.m[3][3];
    m[0][1] = m0;
    m[1][1] = m1;
    m[2][1] = m2;

    m0 = m[0][2] * other.m[0][0]
         + m[1][2] * other.m[0][1]
         + m[2][2] * other.m[0][2]
         + m[3][2] * other.m[0][3];
    m1 = m[0][2] * other.m[1][0]
         + m[1][2] * other.m[1][1]
         + m[2][2] * other.m[1][2]
         + m[3][2] * other.m[1][3];
    m2 = m[0][2] * other.m[2][0]
         + m[1][2] * other.m[2][1]
         + m[2][2] * other.m[2][2]
         + m[3][2] * other.m[2][3];
    m[3][2] = m[0][2] * other.m[3][0]
              + m[1][2] * other.m[3][1]
              + m[2][2] * other.m[3][2]
              + m[3][2] * other.m[3][3];
    m[0][2] = m0;
    m[1][2] = m1;
    m[2][2] = m2;

    m0 = m[0][3] * other.m[0][0]
         + m[1][3] * other.m[0][1]
         + m[2][3] * other.m[0][2]
         + m[3][3] * other.m[0][3];
    m1 = m[0][3] * other.m[1][0]
         + m[1][3] * other.m[1][1]
         + m[2][3] * other.m[1][2]
         + m[3][3] * other.m[1][3];
    m2 = m[0][3] * other.m[2][0]
         + m[1][3] * other.m[2][1]
         + m[2][3] * other.m[2][2]
         + m[3][3] * other.m[2][3];
    m[3][3] = m[0][3] * other.m[3][0]
              + m[1][3] * other.m[3][1]
              + m[2][3] * other.m[3][2]
              + m[3][3] * other.m[3][3];
    m[0][3] = m0;
    m[1][3] = m1;
    m[2][3] = m2;
    return *this;
}

inline LMatrix4x4& LMatrix4x4::operator*=(float factor)
{
    m[0][0] *= factor;
    m[0][1] *= factor;
    m[0][2] *= factor;
    m[0][3] *= factor;
    m[1][0] *= factor;
    m[1][1] *= factor;
    m[1][2] *= factor;
    m[1][3] *= factor;
    m[2][0] *= factor;
    m[2][1] *= factor;
    m[2][2] *= factor;
    m[2][3] *= factor;
    m[3][0] *= factor;
    m[3][1] *= factor;
    m[3][2] *= factor;
    m[3][3] *= factor;
    flagBits = LMatrix4x4Flag::General;
    return *this;
}

inline bool LMatrix4x4::operator==(const LMatrix4x4& other) const
{
    return m[0][0] == other.m[0][0] &&
           m[0][1] == other.m[0][1] &&
           m[0][2] == other.m[0][2] &&
           m[0][3] == other.m[0][3] &&
           m[1][0] == other.m[1][0] &&
           m[1][1] == other.m[1][1] &&
           m[1][2] == other.m[1][2] &&
           m[1][3] == other.m[1][3] &&
           m[2][0] == other.m[2][0] &&
           m[2][1] == other.m[2][1] &&
           m[2][2] == other.m[2][2] &&
           m[2][3] == other.m[2][3] &&
           m[3][0] == other.m[3][0] &&
           m[3][1] == other.m[3][1] &&
           m[3][2] == other.m[3][2] &&
           m[3][3] == other.m[3][3];
}

inline bool LMatrix4x4::operator!=(const LMatrix4x4& other) const
{
    return m[0][0] != other.m[0][0] ||
           m[0][1] != other.m[0][1] ||
           m[0][2] != other.m[0][2] ||
           m[0][3] != other.m[0][3] ||
           m[1][0] != other.m[1][0] ||
           m[1][1] != other.m[1][1] ||
           m[1][2] != other.m[1][2] ||
           m[1][3] != other.m[1][3] ||
           m[2][0] != other.m[2][0] ||
           m[2][1] != other.m[2][1] ||
           m[2][2] != other.m[2][2] ||
           m[2][3] != other.m[2][3] ||
           m[3][0] != other.m[3][0] ||
           m[3][1] != other.m[3][1] ||
           m[3][2] != other.m[3][2] ||
           m[3][3] != other.m[3][3];
}

inline LMatrix4x4 operator+(const LMatrix4x4& m1, const LMatrix4x4& m2)
{
    LMatrix4x4 m(Uninitialized);
    m.m[0][0] = m1.m[0][0] + m2.m[0][0];
    m.m[0][1] = m1.m[0][1] + m2.m[0][1];
    m.m[0][2] = m1.m[0][2] + m2.m[0][2];
    m.m[0][3] = m1.m[0][3] + m2.m[0][3];
    m.m[1][0] = m1.m[1][0] + m2.m[1][0];
    m.m[1][1] = m1.m[1][1] + m2.m[1][1];
    m.m[1][2] = m1.m[1][2] + m2.m[1][2];
    m.m[1][3] = m1.m[1][3] + m2.m[1][3];
    m.m[2][0] = m1.m[2][0] + m2.m[2][0];
    m.m[2][1] = m1.m[2][1] + m2.m[2][1];
    m.m[2][2] = m1.m[2][2] + m2.m[2][2];
    m.m[2][3] = m1.m[2][3] + m2.m[2][3];
    m.m[3][0] = m1.m[3][0] + m2.m[3][0];
    m.m[3][1] = m1.m[3][1] + m2.m[3][1];
    m.m[3][2] = m1.m[3][2] + m2.m[3][2];
    m.m[3][3] = m1.m[3][3] + m2.m[3][3];
    return m;
}

inline LMatrix4x4 operator-(const LMatrix4x4& m1, const LMatrix4x4& m2)
{
    LMatrix4x4 m(Uninitialized);
    m.m[0][0] = m1.m[0][0] - m2.m[0][0];
    m.m[0][1] = m1.m[0][1] - m2.m[0][1];
    m.m[0][2] = m1.m[0][2] - m2.m[0][2];
    m.m[0][3] = m1.m[0][3] - m2.m[0][3];
    m.m[1][0] = m1.m[1][0] - m2.m[1][0];
    m.m[1][1] = m1.m[1][1] - m2.m[1][1];
    m.m[1][2] = m1.m[1][2] - m2.m[1][2];
    m.m[1][3] = m1.m[1][3] - m2.m[1][3];
    m.m[2][0] = m1.m[2][0] - m2.m[2][0];
    m.m[2][1] = m1.m[2][1] - m2.m[2][1];
    m.m[2][2] = m1.m[2][2] - m2.m[2][2];
    m.m[2][3] = m1.m[2][3] - m2.m[2][3];
    m.m[3][0] = m1.m[3][0] - m2.m[3][0];
    m.m[3][1] = m1.m[3][1] - m2.m[3][1];
    m.m[3][2] = m1.m[3][2] - m2.m[3][2];
    m.m[3][3] = m1.m[3][3] - m2.m[3][3];
    return m;
}

inline LMatrix4x4 operator*(const LMatrix4x4& m1, const LMatrix4x4& m2)
{
    LMatrix4x4 m(Uninitialized);
    LMatrix4x4::Flags flagBits = m1.flagBits | m2.flagBits;
    if (uint32_t(flagBits) < (uint32_t)LMatrix4x4Flag::Rotation2D) {
        // Scale | Translation
        m.m[0][0] = m1.m[0][0] * m2.m[0][0];
        m.m[0][1] = 0.0f;
        m.m[0][2] = 0.0f;
        m.m[0][3] = 0.0f;

        m.m[1][0] = 0.0f;
        m.m[1][1] = m1.m[1][1] * m2.m[1][1];
        m.m[1][2] = 0.0f;
        m.m[1][3] = 0.0f;

        m.m[2][0] = 0.0f;
        m.m[2][1] = 0.0f;
        m.m[2][2] = m1.m[2][2] * m2.m[2][2];
        m.m[2][3] = 0.0f;

        m.m[3][0] = m1.m[3][0] + m1.m[0][0] * m2.m[3][0];
        m.m[3][1] = m1.m[3][1] + m1.m[1][1] * m2.m[3][1];
        m.m[3][2] = m1.m[3][2] + m1.m[2][2] * m2.m[3][2];
        m.m[3][3] = 1.0f;
        m.flagBits = flagBits;
        return m;
    }

    m.m[0][0] = m1.m[0][0] * m2.m[0][0]
                + m1.m[1][0] * m2.m[0][1]
                + m1.m[2][0] * m2.m[0][2]
                + m1.m[3][0] * m2.m[0][3];
    m.m[0][1] = m1.m[0][1] * m2.m[0][0]
                + m1.m[1][1] * m2.m[0][1]
                + m1.m[2][1] * m2.m[0][2]
                + m1.m[3][1] * m2.m[0][3];
    m.m[0][2] = m1.m[0][2] * m2.m[0][0]
                + m1.m[1][2] * m2.m[0][1]
                + m1.m[2][2] * m2.m[0][2]
                + m1.m[3][2] * m2.m[0][3];
    m.m[0][3] = m1.m[0][3] * m2.m[0][0]
                + m1.m[1][3] * m2.m[0][1]
                + m1.m[2][3] * m2.m[0][2]
                + m1.m[3][3] * m2.m[0][3];

    m.m[1][0] = m1.m[0][0] * m2.m[1][0]
                + m1.m[1][0] * m2.m[1][1]
                + m1.m[2][0] * m2.m[1][2]
                + m1.m[3][0] * m2.m[1][3];
    m.m[1][1] = m1.m[0][1] * m2.m[1][0]
                + m1.m[1][1] * m2.m[1][1]
                + m1.m[2][1] * m2.m[1][2]
                + m1.m[3][1] * m2.m[1][3];
    m.m[1][2] = m1.m[0][2] * m2.m[1][0]
                + m1.m[1][2] * m2.m[1][1]
                + m1.m[2][2] * m2.m[1][2]
                + m1.m[3][2] * m2.m[1][3];
    m.m[1][3] = m1.m[0][3] * m2.m[1][0]
                + m1.m[1][3] * m2.m[1][1]
                + m1.m[2][3] * m2.m[1][2]
                + m1.m[3][3] * m2.m[1][3];

    m.m[2][0] = m1.m[0][0] * m2.m[2][0]
                + m1.m[1][0] * m2.m[2][1]
                + m1.m[2][0] * m2.m[2][2]
                + m1.m[3][0] * m2.m[2][3];
    m.m[2][1] = m1.m[0][1] * m2.m[2][0]
                + m1.m[1][1] * m2.m[2][1]
                + m1.m[2][1] * m2.m[2][2]
                + m1.m[3][1] * m2.m[2][3];
    m.m[2][2] = m1.m[0][2] * m2.m[2][0]
                + m1.m[1][2] * m2.m[2][1]
                + m1.m[2][2] * m2.m[2][2]
                + m1.m[3][2] * m2.m[2][3];
    m.m[2][3] = m1.m[0][3] * m2.m[2][0]
                + m1.m[1][3] * m2.m[2][1]
                + m1.m[2][3] * m2.m[2][2]
                + m1.m[3][3] * m2.m[2][3];

    m.m[3][0] = m1.m[0][0] * m2.m[3][0]
                + m1.m[1][0] * m2.m[3][1]
                + m1.m[2][0] * m2.m[3][2]
                + m1.m[3][0] * m2.m[3][3];
    m.m[3][1] = m1.m[0][1] * m2.m[3][0]
                + m1.m[1][1] * m2.m[3][1]
                + m1.m[2][1] * m2.m[3][2]
                + m1.m[3][1] * m2.m[3][3];
    m.m[3][2] = m1.m[0][2] * m2.m[3][0]
                + m1.m[1][2] * m2.m[3][1]
                + m1.m[2][2] * m2.m[3][2]
                + m1.m[3][2] * m2.m[3][3];
    m.m[3][3] = m1.m[0][3] * m2.m[3][0]
                + m1.m[1][3] * m2.m[3][1]
                + m1.m[2][3] * m2.m[3][2]
                + m1.m[3][3] * m2.m[3][3];
    m.flagBits = flagBits;
    return m;
}

inline LVector4D operator*(const LVector4D& vector, const LMatrix4x4& matrix)
{
    float x, y, z, w;
    x = vector.x() * matrix.m[0][0] +
        vector.y() * matrix.m[0][1] +
        vector.z() * matrix.m[0][2] +
        vector.w() * matrix.m[0][3];
    y = vector.x() * matrix.m[1][0] +
        vector.y() * matrix.m[1][1] +
        vector.z() * matrix.m[1][2] +
        vector.w() * matrix.m[1][3];
    z = vector.x() * matrix.m[2][0] +
        vector.y() * matrix.m[2][1] +
        vector.z() * matrix.m[2][2] +
        vector.w() * matrix.m[2][3];
    w = vector.x() * matrix.m[3][0] +
        vector.y() * matrix.m[3][1] +
        vector.z() * matrix.m[3][2] +
        vector.w() * matrix.m[3][3];
    return LVector4D(x, y, z, w);
}

inline LVector4D operator*(const LMatrix4x4& matrix, const LVector4D& vector)
{
    float x, y, z, w;
    x = vector.x() * matrix.m[0][0] +
        vector.y() * matrix.m[1][0] +
        vector.z() * matrix.m[2][0] +
        vector.w() * matrix.m[3][0];
    y = vector.x() * matrix.m[0][1] +
        vector.y() * matrix.m[1][1] +
        vector.z() * matrix.m[2][1] +
        vector.w() * matrix.m[3][1];
    z = vector.x() * matrix.m[0][2] +
        vector.y() * matrix.m[1][2] +
        vector.z() * matrix.m[2][2] +
        vector.w() * matrix.m[3][2];
    w = vector.x() * matrix.m[0][3] +
        vector.y() * matrix.m[1][3] +
        vector.z() * matrix.m[2][3] +
        vector.w() * matrix.m[3][3];
    return LVector4D(x, y, z, w);
}

inline LPoint<int> operator*(const LPoint<int>& point, const LMatrix4x4& matrix)
{
    float xin, yin;
    float x, y, w;
    xin = point.m_x;
    yin = point.m_y;
    x = xin * matrix.m[0][0] +
        yin * matrix.m[0][1] +
        matrix.m[0][3];
    y = xin * matrix.m[1][0] +
        yin * matrix.m[1][1] +
        matrix.m[1][3];
    w = xin * matrix.m[3][0] +
        yin * matrix.m[3][1] +
        matrix.m[3][3];
    if (w == 1.0f)
        return LPoint<int>(std::lround(x), std::lround(y));
    else
        return LPoint<int>(std::lround(x / w), std::lround(y / w));
}

inline LPoint<double> operator*(const LPoint<double>& point, const LMatrix4x4& matrix)
{
    float xin, yin;
    float x, y, w;
    xin = float(point.m_x);
    yin = float(point.m_y);
    x = xin * matrix.m[0][0] +
        yin * matrix.m[0][1] +
        matrix.m[0][3];
    y = xin * matrix.m[1][0] +
        yin * matrix.m[1][1] +
        matrix.m[1][3];
    w = xin * matrix.m[3][0] +
        yin * matrix.m[3][1] +
        matrix.m[3][3];
    if (w == 1.0f) {
        return LPoint<double>(double(x), double(y));
    } else {
        return LPoint<double>(double(x / w), double(y / w));
    }
}


inline LMatrix4x4 operator-(const LMatrix4x4& matrix)
{
    LMatrix4x4 m(Uninitialized);
    m.m[0][0] = -matrix.m[0][0];
    m.m[0][1] = -matrix.m[0][1];
    m.m[0][2] = -matrix.m[0][2];
    m.m[0][3] = -matrix.m[0][3];
    m.m[1][0] = -matrix.m[1][0];
    m.m[1][1] = -matrix.m[1][1];
    m.m[1][2] = -matrix.m[1][2];
    m.m[1][3] = -matrix.m[1][3];
    m.m[2][0] = -matrix.m[2][0];
    m.m[2][1] = -matrix.m[2][1];
    m.m[2][2] = -matrix.m[2][2];
    m.m[2][3] = -matrix.m[2][3];
    m.m[3][0] = -matrix.m[3][0];
    m.m[3][1] = -matrix.m[3][1];
    m.m[3][2] = -matrix.m[3][2];
    m.m[3][3] = -matrix.m[3][3];
    return m;
}

inline LMatrix4x4 operator*(float factor, const LMatrix4x4& matrix)
{
    LMatrix4x4 m(Uninitialized);
    m.m[0][0] = matrix.m[0][0] * factor;
    m.m[0][1] = matrix.m[0][1] * factor;
    m.m[0][2] = matrix.m[0][2] * factor;
    m.m[0][3] = matrix.m[0][3] * factor;
    m.m[1][0] = matrix.m[1][0] * factor;
    m.m[1][1] = matrix.m[1][1] * factor;
    m.m[1][2] = matrix.m[1][2] * factor;
    m.m[1][3] = matrix.m[1][3] * factor;
    m.m[2][0] = matrix.m[2][0] * factor;
    m.m[2][1] = matrix.m[2][1] * factor;
    m.m[2][2] = matrix.m[2][2] * factor;
    m.m[2][3] = matrix.m[2][3] * factor;
    m.m[3][0] = matrix.m[3][0] * factor;
    m.m[3][1] = matrix.m[3][1] * factor;
    m.m[3][2] = matrix.m[3][2] * factor;
    m.m[3][3] = matrix.m[3][3] * factor;
    return m;
}

inline LMatrix4x4 operator*(const LMatrix4x4& matrix, float factor)
{

        LMatrix4x4 m(Uninitialized);
    m.m[0][0] = matrix.m[0][0] * factor;
    m.m[0][1] = matrix.m[0][1] * factor;
    m.m[0][2] = matrix.m[0][2] * factor;
    m.m[0][3] = matrix.m[0][3] * factor;
    m.m[1][0] = matrix.m[1][0] * factor;
    m.m[1][1] = matrix.m[1][1] * factor;
    m.m[1][2] = matrix.m[1][2] * factor;
    m.m[1][3] = matrix.m[1][3] * factor;
    m.m[2][0] = matrix.m[2][0] * factor;
    m.m[2][1] = matrix.m[2][1] * factor;
    m.m[2][2] = matrix.m[2][2] * factor;
    m.m[2][3] = matrix.m[2][3] * factor;
    m.m[3][0] = matrix.m[3][0] * factor;
    m.m[3][1] = matrix.m[3][1] * factor;
    m.m[3][2] = matrix.m[3][2] * factor;
    m.m[3][3] = matrix.m[3][3] * factor;
    return m;
}

inline LPoint<int> LMatrix4x4::map(const LPoint<int>& point) const
{
    float xin, yin;
    float x, y, w;
    xin = point.m_x;
    yin = point.m_y;
    if (flagBits == LMatrix4x4Flag::Identity) {
        return point;
    } else if (uint32_t(flagBits) < (uint32_t)LMatrix4x4Flag::Rotation2D) {
        // Translation | Scale
        return LPoint<int>(std::lround(xin * m[0][0] + m[3][0]),
                      std::lround(yin * m[1][1] + m[3][1]));
    } else if (uint32_t(flagBits) < (uint32_t)LMatrix4x4Flag::Perspective) {
        return LPoint<int>(std::lround(xin * m[0][0] + yin * m[1][0] + m[3][0]),
                      std::lround(xin * m[0][1] + yin * m[1][1] + m[3][1]));
    } else {
        x = xin * m[0][0] +
            yin * m[1][0] +
            m[3][0];
        y = xin * m[0][1] +
            yin * m[1][1] +
            m[3][1];
        w = xin * m[0][3] +
            yin * m[1][3] +
            m[3][3];
        if (w == 1.0f)
            return LPoint<int>(std::lround(x), std::lround(y));
        else
            return LPoint<int>(std::lround(x / w), std::lround(y / w));
    }
}

inline LPoint<double> LMatrix4x4::map(const LPoint<double>& point) const
{
    double xin, yin;
    double x, y, w;
    xin = point.m_x;
    yin = point.m_y;
    if (flagBits == LMatrix4x4Flag::Identity) {
        return point;
    } else if (uint32_t(flagBits) < (uint32_t)LMatrix4x4Flag::Rotation2D) {
        // Translation | Scale
        return LPoint<double>(xin * double(m[0][0]) + double(m[3][0]),
                       yin * double(m[1][1]) + double(m[3][1]));
    } else if (uint32_t(flagBits) < (uint32_t)LMatrix4x4Flag::Perspective) {
        return LPoint<double>(xin * double(m[0][0]) + yin * double(m[1][0]) +
                           double(m[3][0]),
                       xin * double(m[0][1]) + yin * double(m[1][1]) +
                           double(m[3][1]));
    } else {
        x = xin * double(m[0][0]) +
            yin * double(m[1][0]) +
            double(m[3][0]);
        y = xin * double(m[0][1]) +
            yin * double(m[1][1]) +
            double(m[3][1]);
        w = xin * double(m[0][3]) +
            yin * double(m[1][3]) +
            double(m[3][3]);
        if (w == 1.0) {
            return LPoint<double>(double(x), double(y));
        } else {
            return LPoint<double>(double(x / w), double(y / w));
        }
    }
}


inline LVector3D LMatrix4x4::map(const LVector3D& point) const
{
    float x, y, z, w;
    if (flagBits == LMatrix4x4Flag::Identity) {
        return point;
    } else if (uint32_t(flagBits) < (uint32_t)LMatrix4x4Flag::Rotation2D) {
        // Translation | Scale
        return LVector3D(point.x() * m[0][0] + m[3][0],
                         point.y() * m[1][1] + m[3][1],
                         point.z() * m[2][2] + m[3][2]);
    } else if (uint32_t(flagBits) < (uint32_t)LMatrix4x4Flag::Rotation) {
        // Translation | Scale | Rotation2D
        return LVector3D(point.x() * m[0][0] + point.y() * m[1][0] + m[3][0],
                         point.x() * m[0][1] + point.y() * m[1][1] + m[3][1],
                         point.z() * m[2][2] + m[3][2]);
    } else {
        x = point.x() * m[0][0] +
            point.y() * m[1][0] +
            point.z() * m[2][0] +
            m[3][0];
        y = point.x() * m[0][1] +
            point.y() * m[1][1] +
            point.z() * m[2][1] +
            m[3][1];
        z = point.x() * m[0][2] +
            point.y() * m[1][2] +
            point.z() * m[2][2] +
            m[3][2];
        w = point.x() * m[0][3] +
            point.y() * m[1][3] +
            point.z() * m[2][3] +
            m[3][3];
        if (w == 1.0f)
            return LVector3D(x, y, z);
        else
            return LVector3D(x / w, y / w, z / w);
    }
}

inline LVector3D LMatrix4x4::mapVector(const LVector3D& vector) const
{
    if (uint32_t(flagBits) < (uint32_t)LMatrix4x4Flag::Scale) {
        // Translation
        return vector;
    } else if (uint32_t(flagBits) < (uint32_t)LMatrix4x4Flag::Rotation2D) {
        // Translation | Scale
        return LVector3D(vector.x() * m[0][0],
                         vector.y() * m[1][1],
                         vector.z() * m[2][2]);
    } else {
        return LVector3D(vector.x() * m[0][0] +
                             vector.y() * m[1][0] +
                             vector.z() * m[2][0],
                         vector.x() * m[0][1] +
                             vector.y() * m[1][1] +
                             vector.z() * m[2][1],
                         vector.x() * m[0][2] +
                             vector.y() * m[1][2] +
                             vector.z() * m[2][2]);
    }
}


inline LVector4D LMatrix4x4::map(const LVector4D& point) const
{
    return *this * point;
}


inline float *LMatrix4x4::data()
{
    // We have to assume that the caller will modify the matrix elements,
    // so we flip it over to "General" mode.
    flagBits = LMatrix4x4Flag::General;
    return *m;
}

inline void LMatrix4x4::viewport(const LRect<double>  &rect)
{
    viewport(float(rect.m_x), float(rect.m_y), float(rect.m_width), float(rect.m_height));
}

LK_NAMESPACE_END
#endif // LMATRIX4X4_H
