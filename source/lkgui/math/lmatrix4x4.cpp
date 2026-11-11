#include "lmatrix4x4.h"
#include "lquaternion.h"
#include <array>
LK_NAMESPACE_BEGIN

static const float inv_dist_to_plane = 1.0f / 1024.0f;

LMatrix4x4::LMatrix4x4(const float *values)
{
    for (int row = 0; row < 4; ++row){
        for (int col = 0; col < 4; ++col){
            m[col][row] = values[row * 4 + col];
        }
    }
    flagBits = LMatrix4x4Flag::General;
}

LMatrix4x4::LMatrix4x4(const float *values, int cols, int rows)
{
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            if (col < cols && row < rows)
                m[col][row] = values[col * rows + row];
            else if (col == row)
                m[col][row] = 1.0f;
            else
                m[col][row] = 0.0f;
        }
    }
    flagBits = LMatrix4x4Flag::General;
}


LMatrix4x4::LMatrix4x4(const LTransform2D& transform)
{
    m[0][0] = transform.m11();
    m[0][1] = transform.m12();
    m[0][2] = 0.0f;
    m[0][3] = transform.m13();
    m[1][0] = transform.m21();
    m[1][1] = transform.m22();
    m[1][2] = 0.0f;
    m[1][3] = transform.m23();
    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = 1.0f;
    m[2][3] = 0.0f;
    m[3][0] = transform.dx();
    m[3][1] = transform.dy();
    m[3][2] = 0.0f;
    m[3][3] = transform.m33();
    flagBits = LMatrix4x4Flag::General;
}


using Double4x4 = std::array<std::array<double, 4>, 4>;

static double matrixDet2(const Double4x4 &m, int col0, int col1, int row0, int row1)
{
    return m[col0][row0] * m[col1][row1] - m[col0][row1] * m[col1][row0];
}


// The 4x4 matrix inverse algorithm is based on that described at:
// http://www.j3d.org/matrix_faq/matrfaq_latest.html#Q24
// Some optimization has been done to avoid making copies of 3x3
// sub-matrices and to unroll the loops.

// Calculate the determinant of a 3x3 sub-matrix.
//     | A B C |
// M = | D E F |   det(M) = A * (EI - HF) - B * (DI - GF) + C * (DH - GE)
//     | G H I |
static double matrixDet3(const Double4x4 &m,
                         int col0, int col1, int col2,
                         int row0, int row1, int row2)
{
    return m[col0][row0] * matrixDet2(m, col1, col2, row1, row2)
    - m[col1][row0] * matrixDet2(m, col0, col2, row1, row2)
        + m[col2][row0] * matrixDet2(m, col0, col1, row1, row2);
}

// Calculate the determinant of a 4x4 matrix.
static double matrixDet4(const Double4x4 &m)
{
    double det;
    det  = m[0][0] * matrixDet3(m, 1, 2, 3, 1, 2, 3);
    det -= m[1][0] * matrixDet3(m, 0, 2, 3, 1, 2, 3);
    det += m[2][0] * matrixDet3(m, 0, 1, 3, 1, 2, 3);
    det -= m[3][0] * matrixDet3(m, 0, 1, 2, 1, 2, 3);
    return det;
}

static Double4x4 copyToDoubles(const float m[4][4])
{
    Double4x4 mm;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            mm[i][j] = double(m[i][j]);
    return mm;
}

double LMatrix4x4::determinant() const
{
    if ((flagBits & ~(LMatrix4x4Flag::Translation | LMatrix4x4Flag::Rotation2D | LMatrix4x4Flag::Rotation)) == LMatrix4x4Flag::Identity)
        return 1.0;
    if (uint32_t(flagBits) < (uint32_t)LMatrix4x4Flag::Rotation2D)
        return 1.0 * m[0][0] * m[1][1] * m[2][2]; // Translation | Scale

    const Double4x4 mm = copyToDoubles(m);
    if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Perspective)
        return matrixDet3(mm, 0, 1, 2, 0, 1, 2);
    return matrixDet4(mm);
}

LMatrix4x4 LMatrix4x4::inverted(bool *invertible) const
{
    // Handle some of the easy cases first.
    if (flagBits == LMatrix4x4Flag::Identity) {
        if (invertible)
            *invertible = true;
        return LMatrix4x4();
    } else if (flagBits == LMatrix4x4Flag::Translation) {
        LMatrix4x4 inv;
        inv.m[3][0] = -m[3][0];
        inv.m[3][1] = -m[3][1];
        inv.m[3][2] = -m[3][2];
        inv.flagBits = LMatrix4x4Flag::Translation;
        if (invertible)
            *invertible = true;
        return inv;
    } else if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Rotation2D) {
        // Translation | Scale
        if (m[0][0] == 0 || m[1][1] == 0 || m[2][2] == 0) {
            if (invertible)
                *invertible = false;
            return LMatrix4x4();
        }
        LMatrix4x4 inv;
        inv.m[0][0] = 1.0f / m[0][0];
        inv.m[1][1] = 1.0f / m[1][1];
        inv.m[2][2] = 1.0f / m[2][2];
        inv.m[3][0] = -m[3][0] * inv.m[0][0];
        inv.m[3][1] = -m[3][1] * inv.m[1][1];
        inv.m[3][2] = -m[3][2] * inv.m[2][2];
        inv.flagBits = flagBits;

        if (invertible)
            *invertible = true;
        return inv;
    } else if ((flagBits & ~(LMatrix4x4Flag::Translation | LMatrix4x4Flag::Rotation2D | LMatrix4x4Flag::Rotation)) == LMatrix4x4Flag::Identity) {
        if (invertible)
            *invertible = true;
        return orthonormalInverse();
    } else if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Perspective) {
        LMatrix4x4 inv(Uninitialized);
        const Double4x4 mm = copyToDoubles(m);

        double det = matrixDet3(mm, 0, 1, 2, 0, 1, 2);
        if (det == 0.0f) {
            if (invertible)
                *invertible = false;
            return LMatrix4x4();
        }
        det = 1.0f / det;

        inv.m[0][0] =  matrixDet2(mm, 1, 2, 1, 2) * det;
        inv.m[0][1] = -matrixDet2(mm, 0, 2, 1, 2) * det;
        inv.m[0][2] =  matrixDet2(mm, 0, 1, 1, 2) * det;
        inv.m[0][3] = 0;
        inv.m[1][0] = -matrixDet2(mm, 1, 2, 0, 2) * det;
        inv.m[1][1] =  matrixDet2(mm, 0, 2, 0, 2) * det;
        inv.m[1][2] = -matrixDet2(mm, 0, 1, 0, 2) * det;
        inv.m[1][3] = 0;
        inv.m[2][0] =  matrixDet2(mm, 1, 2, 0, 1) * det;
        inv.m[2][1] = -matrixDet2(mm, 0, 2, 0, 1) * det;
        inv.m[2][2] =  matrixDet2(mm, 0, 1, 0, 1) * det;
        inv.m[2][3] = 0;
        inv.m[3][0] = -inv.m[0][0] * m[3][0] - inv.m[1][0] * m[3][1] - inv.m[2][0] * m[3][2];
        inv.m[3][1] = -inv.m[0][1] * m[3][0] - inv.m[1][1] * m[3][1] - inv.m[2][1] * m[3][2];
        inv.m[3][2] = -inv.m[0][2] * m[3][0] - inv.m[1][2] * m[3][1] - inv.m[2][2] * m[3][2];
        inv.m[3][3] = 1;
        inv.flagBits = flagBits;

        if (invertible)
            *invertible = true;
        return inv;
    }


    LMatrix4x4 inv(Uninitialized);

    const Double4x4 mm = copyToDoubles(m);

    double det = matrixDet4(mm);
    if (det == 0.0f) {
        if (invertible)
            *invertible = false;
        return LMatrix4x4();
    }
    det = 1.0f / det;

    inv.m[0][0] =  matrixDet3(mm, 1, 2, 3, 1, 2, 3) * det;
    inv.m[0][1] = -matrixDet3(mm, 0, 2, 3, 1, 2, 3) * det;
    inv.m[0][2] =  matrixDet3(mm, 0, 1, 3, 1, 2, 3) * det;
    inv.m[0][3] = -matrixDet3(mm, 0, 1, 2, 1, 2, 3) * det;
    inv.m[1][0] = -matrixDet3(mm, 1, 2, 3, 0, 2, 3) * det;
    inv.m[1][1] =  matrixDet3(mm, 0, 2, 3, 0, 2, 3) * det;
    inv.m[1][2] = -matrixDet3(mm, 0, 1, 3, 0, 2, 3) * det;
    inv.m[1][3] =  matrixDet3(mm, 0, 1, 2, 0, 2, 3) * det;
    inv.m[2][0] =  matrixDet3(mm, 1, 2, 3, 0, 1, 3) * det;
    inv.m[2][1] = -matrixDet3(mm, 0, 2, 3, 0, 1, 3) * det;
    inv.m[2][2] =  matrixDet3(mm, 0, 1, 3, 0, 1, 3) * det;
    inv.m[2][3] = -matrixDet3(mm, 0, 1, 2, 0, 1, 3) * det;
    inv.m[3][0] = -matrixDet3(mm, 1, 2, 3, 0, 1, 2) * det;
    inv.m[3][1] =  matrixDet3(mm, 0, 2, 3, 0, 1, 2) * det;
    inv.m[3][2] = -matrixDet3(mm, 0, 1, 3, 0, 1, 2) * det;
    inv.m[3][3] =  matrixDet3(mm, 0, 1, 2, 0, 1, 2) * det;
    inv.flagBits = flagBits;

    if (invertible)
        *invertible = true;
    return inv;
}

LMatrix3x3 LMatrix4x4::normalMatrix() const
{
    LMatrix3x3 inv;

    // Handle the simple cases first.
    if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Scale) {
        // Translation
        return inv;
    } else if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Rotation2D) {
        // Translation | Scale
        if (m[0][0] == 0.0f || m[1][1] == 0.0f || m[2][2] == 0.0f)
            return inv;
        inv.data()[0] = 1.0f / m[0][0];
        inv.data()[4] = 1.0f / m[1][1];
        inv.data()[8] = 1.0f / m[2][2];
        return inv;
    } else if ((flagBits & ~(LMatrix4x4Flag::Translation | LMatrix4x4Flag::Rotation2D | LMatrix4x4Flag::Rotation)) == LMatrix4x4Flag::Identity) {
        float *invm = inv.data();
        invm[0 + 0 * 3] = m[0][0];
        invm[1 + 0 * 3] = m[0][1];
        invm[2 + 0 * 3] = m[0][2];
        invm[0 + 1 * 3] = m[1][0];
        invm[1 + 1 * 3] = m[1][1];
        invm[2 + 1 * 3] = m[1][2];
        invm[0 + 2 * 3] = m[2][0];
        invm[1 + 2 * 3] = m[2][1];
        invm[2 + 2 * 3] = m[2][2];
        return inv;
    }

    const Double4x4 mm = copyToDoubles(m);
    double det = matrixDet3(mm, 0, 1, 2, 0, 1, 2);
    if (det == 0.0f)
        return inv;
    det = 1.0f / det;

    float *invm = inv.data();

    // Invert and transpose in a single step.
    invm[0 + 0 * 3] =  (mm[1][1] * mm[2][2] - mm[2][1] * mm[1][2]) * det;
    invm[1 + 0 * 3] = -(mm[1][0] * mm[2][2] - mm[1][2] * mm[2][0]) * det;
    invm[2 + 0 * 3] =  (mm[1][0] * mm[2][1] - mm[1][1] * mm[2][0]) * det;
    invm[0 + 1 * 3] = -(mm[0][1] * mm[2][2] - mm[2][1] * mm[0][2]) * det;
    invm[1 + 1 * 3] =  (mm[0][0] * mm[2][2] - mm[0][2] * mm[2][0]) * det;
    invm[2 + 1 * 3] = -(mm[0][0] * mm[2][1] - mm[0][1] * mm[2][0]) * det;
    invm[0 + 2 * 3] =  (mm[0][1] * mm[1][2] - mm[0][2] * mm[1][1]) * det;
    invm[1 + 2 * 3] = -(mm[0][0] * mm[1][2] - mm[0][2] * mm[1][0]) * det;
    invm[2 + 2 * 3] =  (mm[0][0] * mm[1][1] - mm[1][0] * mm[0][1]) * det;

    return inv;
}

LMatrix4x4 LMatrix4x4::transposed() const
{

        LMatrix4x4 result(Uninitialized);
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            result.m[col][row] = m[row][col];
        }
    }
    // When a translation is transposed, it becomes a perspective transformation.
    result.flagBits = (flagBits & LMatrix4x4Flag::Translation ? LMatrix4x4Flag::General : flagBits);
    return result;
}


LMatrix4x4& LMatrix4x4::operator/=(float divisor)
{
    m[0][0] /= divisor;
    m[0][1] /= divisor;
    m[0][2] /= divisor;
    m[0][3] /= divisor;
    m[1][0] /= divisor;
    m[1][1] /= divisor;
    m[1][2] /= divisor;
    m[1][3] /= divisor;
    m[2][0] /= divisor;
    m[2][1] /= divisor;
    m[2][2] /= divisor;
    m[2][3] /= divisor;
    m[3][0] /= divisor;
    m[3][1] /= divisor;
    m[3][2] /= divisor;
    m[3][3] /= divisor;
    flagBits = LMatrix4x4Flag::General;
    return *this;
}

/*!
    \relates LMatrix4x4

    Returns the result of dividing all elements of \a matrix by \a divisor.
*/
LMatrix4x4 operator/(const LMatrix4x4& matrix, float divisor)
{

        LMatrix4x4 m(Uninitialized);
    m.m[0][0] = matrix.m[0][0] / divisor;
    m.m[0][1] = matrix.m[0][1] / divisor;
    m.m[0][2] = matrix.m[0][2] / divisor;
    m.m[0][3] = matrix.m[0][3] / divisor;
    m.m[1][0] = matrix.m[1][0] / divisor;
    m.m[1][1] = matrix.m[1][1] / divisor;
    m.m[1][2] = matrix.m[1][2] / divisor;
    m.m[1][3] = matrix.m[1][3] / divisor;
    m.m[2][0] = matrix.m[2][0] / divisor;
    m.m[2][1] = matrix.m[2][1] / divisor;
    m.m[2][2] = matrix.m[2][2] / divisor;
    m.m[2][3] = matrix.m[2][3] / divisor;
    m.m[3][0] = matrix.m[3][0] / divisor;
    m.m[3][1] = matrix.m[3][1] / divisor;
    m.m[3][2] = matrix.m[3][2] / divisor;
    m.m[3][3] = matrix.m[3][3] / divisor;
    m.flagBits = LMatrix4x4Flag::General;
    return m;
}

bool lFuzzyCompare(const LMatrix4x4& m1, const LMatrix4x4& m2) noexcept
{
    return fuzzyCompare(m1.m[0][0], m2.m[0][0])
    && fuzzyCompare(m1.m[0][1], m2.m[0][1])
        && fuzzyCompare(m1.m[0][2], m2.m[0][2])
        && fuzzyCompare(m1.m[0][3], m2.m[0][3])
        && fuzzyCompare(m1.m[1][0], m2.m[1][0])
        && fuzzyCompare(m1.m[1][1], m2.m[1][1])
        && fuzzyCompare(m1.m[1][2], m2.m[1][2])
        && fuzzyCompare(m1.m[1][3], m2.m[1][3])
        && fuzzyCompare(m1.m[2][0], m2.m[2][0])
        && fuzzyCompare(m1.m[2][1], m2.m[2][1])
        && fuzzyCompare(m1.m[2][2], m2.m[2][2])
        && fuzzyCompare(m1.m[2][3], m2.m[2][3])
        && fuzzyCompare(m1.m[3][0], m2.m[3][0])
        && fuzzyCompare(m1.m[3][1], m2.m[3][1])
        && fuzzyCompare(m1.m[3][2], m2.m[3][2])
        && fuzzyCompare(m1.m[3][3], m2.m[3][3]);
}

void LMatrix4x4::scale(const LVector3D& vector)
{
    float vx = vector.x();
    float vy = vector.y();
    float vz = vector.z();
    if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Scale) {
        m[0][0] = vx;
        m[1][1] = vy;
        m[2][2] = vz;
    } else if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Rotation2D) {
        m[0][0] *= vx;
        m[1][1] *= vy;
        m[2][2] *= vz;
    } else if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Rotation) {
        m[0][0] *= vx;
        m[0][1] *= vx;
        m[1][0] *= vy;
        m[1][1] *= vy;
        m[2][2] *= vz;
    } else {
        m[0][0] *= vx;
        m[0][1] *= vx;
        m[0][2] *= vx;
        m[0][3] *= vx;
        m[1][0] *= vy;
        m[1][1] *= vy;
        m[1][2] *= vy;
        m[1][3] *= vy;
        m[2][0] *= vz;
        m[2][1] *= vz;
        m[2][2] *= vz;
        m[2][3] *= vz;
    }
    flagBits |= LMatrix4x4Flag::Scale;
}

void LMatrix4x4::scale(float x, float y)
{
    if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Scale) {
        m[0][0] = x;
        m[1][1] = y;
    } else if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Rotation2D) {
        m[0][0] *= x;
        m[1][1] *= y;
    } else if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Rotation) {
        m[0][0] *= x;
        m[0][1] *= x;
        m[1][0] *= y;
        m[1][1] *= y;
    } else {
        m[0][0] *= x;
        m[0][1] *= x;
        m[0][2] *= x;
        m[0][3] *= x;
        m[1][0] *= y;
        m[1][1] *= y;
        m[1][2] *= y;
        m[1][3] *= y;
    }
    flagBits |= LMatrix4x4Flag::Scale;
}

/*!
    \overload

    Multiplies this matrix by another that scales coordinates by the
    components \a x, \a y, and \a z.

    \sa translate(), rotate()
*/
void LMatrix4x4::scale(float x, float y, float z)
{
    if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Scale) {
        m[0][0] = x;
        m[1][1] = y;
        m[2][2] = z;
    } else if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Rotation2D) {
        m[0][0] *= x;
        m[1][1] *= y;
        m[2][2] *= z;
    } else if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Rotation) {
        m[0][0] *= x;
        m[0][1] *= x;
        m[1][0] *= y;
        m[1][1] *= y;
        m[2][2] *= z;
    } else {
        m[0][0] *= x;
        m[0][1] *= x;
        m[0][2] *= x;
        m[0][3] *= x;
        m[1][0] *= y;
        m[1][1] *= y;
        m[1][2] *= y;
        m[1][3] *= y;
        m[2][0] *= z;
        m[2][1] *= z;
        m[2][2] *= z;
        m[2][3] *= z;
    }
    flagBits |= LMatrix4x4Flag::Scale;
}

/*!
    \overload

    Multiplies this matrix by another that scales coordinates by the
    given \a factor.

    \sa translate(), rotate()
*/
void LMatrix4x4::scale(float factor)
{
    if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Scale) {
        m[0][0] = factor;
        m[1][1] = factor;
        m[2][2] = factor;
    } else if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Rotation2D) {
        m[0][0] *= factor;
        m[1][1] *= factor;
        m[2][2] *= factor;
    } else if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Rotation) {
        m[0][0] *= factor;
        m[0][1] *= factor;
        m[1][0] *= factor;
        m[1][1] *= factor;
        m[2][2] *= factor;
    } else {
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
    }
    flagBits |= LMatrix4x4Flag::Scale;
}

void LMatrix4x4::translate(const LVector3D& vector)
{
    translate(vector.x(), vector.y(), vector.z());
}

void LMatrix4x4::translate(float x, float y)
{
    if (flagBits == LMatrix4x4Flag::Identity) {
        m[3][0] = x;
        m[3][1] = y;
    } else if (flagBits == LMatrix4x4Flag::Translation) {
        m[3][0] += x;
        m[3][1] += y;
    } else if (flagBits == LMatrix4x4Flag::Scale) {
        m[3][0] = m[0][0] * x;
        m[3][1] = m[1][1] * y;
    } else if (flagBits == (LMatrix4x4Flag::Translation | LMatrix4x4Flag::Scale)) {
        m[3][0] += m[0][0] * x;
        m[3][1] += m[1][1] * y;
    } else if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Rotation) {
        m[3][0] += m[0][0] * x + m[1][0] * y;
        m[3][1] += m[0][1] * x + m[1][1] * y;
    } else {
        m[3][0] += m[0][0] * x + m[1][0] * y;
        m[3][1] += m[0][1] * x + m[1][1] * y;
        m[3][2] += m[0][2] * x + m[1][2] * y;
        m[3][3] += m[0][3] * x + m[1][3] * y;
    }
    flagBits |= LMatrix4x4Flag::Translation;
}

void LMatrix4x4::translate(float x, float y, float z)
{
    if (flagBits == LMatrix4x4Flag::Identity) {
        m[3][0] = x;
        m[3][1] = y;
        m[3][2] = z;
    } else if (flagBits == LMatrix4x4Flag::Translation) {
        m[3][0] += x;
        m[3][1] += y;
        m[3][2] += z;
    } else if (flagBits == LMatrix4x4Flag::Scale) {
        m[3][0] = m[0][0] * x;
        m[3][1] = m[1][1] * y;
        m[3][2] = m[2][2] * z;
    } else if (flagBits == (LMatrix4x4Flag::Translation | LMatrix4x4Flag::Scale)) {
        m[3][0] += m[0][0] * x;
        m[3][1] += m[1][1] * y;
        m[3][2] += m[2][2] * z;
    } else if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Rotation) {
        m[3][0] += m[0][0] * x + m[1][0] * y;
        m[3][1] += m[0][1] * x + m[1][1] * y;
        m[3][2] += m[2][2] * z;
    } else {
        m[3][0] += m[0][0] * x + m[1][0] * y + m[2][0] * z;
        m[3][1] += m[0][1] * x + m[1][1] * y + m[2][1] * z;
        m[3][2] += m[0][2] * x + m[1][2] * y + m[2][2] * z;
        m[3][3] += m[0][3] * x + m[1][3] * y + m[2][3] * z;
    }
    flagBits |= LMatrix4x4Flag::Translation;
}


void LMatrix4x4::rotate(float angle, const LVector3D& vector)
{
    rotate(angle, vector.x(), vector.y(), vector.z());
}

void LMatrix4x4::rotate(float angle, float x, float y, float z)
{
    if (angle == 0.0f)
        return;
    float c, s;
    if (angle == 90.0f || angle == -270.0f) {
        s = 1.0f;
        c = 0.0f;
    } else if (angle == -90.0f || angle == 270.0f) {
        s = -1.0f;
        c = 0.0f;
    } else if (angle == 180.0f || angle == -180.0f) {
        s = 0.0f;
        c = -1.0f;
    } else {
        float a = lDegreesToRadians(angle);
        c = std::cos(a);
        s = std::sin(a);
    }
    if (x == 0.0f) {
        if (y == 0.0f) {
            if (z != 0.0f) {
                // Rotate around the Z axis.
                if (z < 0)
                    s = -s;
                float tmp;
                m[0][0] = (tmp = m[0][0]) * c + m[1][0] * s;
                m[1][0] = m[1][0] * c - tmp * s;
                m[0][1] = (tmp = m[0][1]) * c + m[1][1] * s;
                m[1][1] = m[1][1] * c - tmp * s;
                m[0][2] = (tmp = m[0][2]) * c + m[1][2] * s;
                m[1][2] = m[1][2] * c - tmp * s;
                m[0][3] = (tmp = m[0][3]) * c + m[1][3] * s;
                m[1][3] = m[1][3] * c - tmp * s;

                flagBits |= LMatrix4x4Flag::Rotation2D;
                return;
            }
        } else if (z == 0.0f) {
            // Rotate around the Y axis.
            if (y < 0)
                s = -s;
            float tmp;
            m[2][0] = (tmp = m[2][0]) * c + m[0][0] * s;
            m[0][0] = m[0][0] * c - tmp * s;
            m[2][1] = (tmp = m[2][1]) * c + m[0][1] * s;
            m[0][1] = m[0][1] * c - tmp * s;
            m[2][2] = (tmp = m[2][2]) * c + m[0][2] * s;
            m[0][2] = m[0][2] * c - tmp * s;
            m[2][3] = (tmp = m[2][3]) * c + m[0][3] * s;
            m[0][3] = m[0][3] * c - tmp * s;

            flagBits |= LMatrix4x4Flag::Rotation;
            return;
        }
    } else if (y == 0.0f && z == 0.0f) {
        // Rotate around the X axis.
        if (x < 0)
            s = -s;
        float tmp;
        m[1][0] = (tmp = m[1][0]) * c + m[2][0] * s;
        m[2][0] = m[2][0] * c - tmp * s;
        m[1][1] = (tmp = m[1][1]) * c + m[2][1] * s;
        m[2][1] = m[2][1] * c - tmp * s;
        m[1][2] = (tmp = m[1][2]) * c + m[2][2] * s;
        m[2][2] = m[2][2] * c - tmp * s;
        m[1][3] = (tmp = m[1][3]) * c + m[2][3] * s;
        m[2][3] = m[2][3] * c - tmp * s;

        flagBits |= LMatrix4x4Flag::Rotation;
        return;
    }

    double len = double(x) * double(x) +
                 double(y) * double(y) +
                 double(z) * double(z);
    if (!lFuzzyCompare(len, 1.0) && !lFuzzyIsZero(len)) {
        len = std::sqrt(len);
        x = float(double(x) / len);
        y = float(double(y) / len);
        z = float(double(z) / len);
    }
    float ic = 1.0f - c;

    LMatrix4x4 rot(Uninitialized);
    rot.m[0][0] = x * x * ic + c;
    rot.m[1][0] = x * y * ic - z * s;
    rot.m[2][0] = x * z * ic + y * s;
    rot.m[3][0] = 0.0f;
    rot.m[0][1] = y * x * ic + z * s;
    rot.m[1][1] = y * y * ic + c;
    rot.m[2][1] = y * z * ic - x * s;
    rot.m[3][1] = 0.0f;
    rot.m[0][2] = x * z * ic - y * s;
    rot.m[1][2] = y * z * ic + x * s;
    rot.m[2][2] = z * z * ic + c;
    rot.m[3][2] = 0.0f;
    rot.m[0][3] = 0.0f;
    rot.m[1][3] = 0.0f;
    rot.m[2][3] = 0.0f;
    rot.m[3][3] = 1.0f;
    rot.flagBits = LMatrix4x4Flag::Rotation;
    *this *= rot;
}


void LMatrix4x4::projectedRotate(float angle, float x, float y, float z, float distanceToPlane)
{
    if (lIsZero(distanceToPlane))
        return rotate(angle, x, y, z);
    if (angle == 0.0f)
        return;

    float c, s;
    if (angle == 90.0f || angle == -270.0f) {
        s = 1.0f;
        c = 0.0f;
    } else if (angle == -90.0f || angle == 270.0f) {
        s = -1.0f;
        c = 0.0f;
    } else if (angle == 180.0f || angle == -180.0f) {
        s = 0.0f;
        c = -1.0f;
    } else {
        float a = lDegreesToRadians(angle);
        c = std::cos(a);
        s = std::sin(a);
    }

    const double d = 1.0 / distanceToPlane;
    if (x == 0.0f) {
        if (y == 0.0f) {
            if (z != 0.0f) {
                // Rotate around the Z axis.
                if (z < 0)
                    s = -s;
                float tmp;
                m[0][0] = (tmp = m[0][0]) * c + m[1][0] * s;
                m[1][0] = m[1][0] * c - tmp * s;
                m[0][1] = (tmp = m[0][1]) * c + m[1][1] * s;
                m[1][1] = m[1][1] * c - tmp * s;
                m[0][2] = (tmp = m[0][2]) * c + m[1][2] * s;
                m[1][2] = m[1][2] * c - tmp * s;
                m[0][3] = (tmp = m[0][3]) * c + m[1][3] * s;
                m[1][3] = m[1][3] * c - tmp * s;

                flagBits |= LMatrix4x4Flag::Rotation2D;
                return;
            }
        } else if (z == 0.0f) {
            // Rotate around the Y axis.
            if (y < 0)
                s = -s;
            s *= d;
            m[0][0] = m[0][0] * c + m[3][0] * s;
            m[0][1] = m[0][1] * c + m[3][1] * s;
            m[0][2] = m[0][2] * c + m[3][2] * s;
            m[0][3] = m[0][3] * c + m[3][3] * s;
            flagBits = LMatrix4x4Flag::General;
            return;
        }
    } else if (y == 0.0f && z == 0.0f) {
        // Rotate around the X axis.
        if (x < 0)
            s = -s;
        s *= d;
        m[1][0] = m[1][0] * c - m[3][0] * s;
        m[1][1] = m[1][1] * c - m[3][1] * s;
        m[1][2] = m[1][2] * c - m[3][2] * s;
        m[1][3] = m[1][3] * c - m[3][3] * s;
        flagBits = LMatrix4x4Flag::General;
        return;
    }
    double len = double(x) * double(x) +
                 double(y) * double(y) +
                 double(z) * double(z);
    if (!lFuzzyCompare(len, 1.0) && !lFuzzyIsZero(len)) {
        len = std::sqrt(len);
        x = float(double(x) / len);
        y = float(double(y) / len);
        z = float(double(z) / len);
    }
    const float ic = 1.0f - c;

    LMatrix4x4 rot(Uninitialized);
    rot.m[0][0] = x * x * ic + c;
    rot.m[1][0] = x * y * ic - z * s;
    rot.m[2][0] = 0.0f;
    rot.m[3][0] = 0.0f;
    rot.m[0][1] = y * x * ic + z * s;
    rot.m[1][1] = y * y * ic + c;
    rot.m[2][1] = 0.0f;
    rot.m[3][1] = 0.0f;
    rot.m[0][2] = 0.0f;
    rot.m[1][2] = 0.0f;
    rot.m[2][2] = 1.0f;
    rot.m[3][2] = 0.0f;
    rot.m[0][3] = (x * z * ic - y * s) * -d;
    rot.m[1][3] = (y * z * ic + x * s) * -d;
    rot.m[2][3] = 0.0f;
    rot.m[3][3] = 1.0f;
    rot.flagBits = LMatrix4x4Flag::General;
    *this *= rot;
}

void LMatrix4x4::rotate(const LQuaternion& quaternion)
{
    // Algorithm from:
    // http://www.j3d.org/matrix_faq/matrfaq_latest.html#Q54
    LMatrix4x4 m(Uninitialized);

    const float f2x = quaternion.x() + quaternion.x();
    const float f2y = quaternion.y() + quaternion.y();
    const float f2z = quaternion.z() + quaternion.z();
    const float f2xw = f2x * quaternion.scalar();
    const float f2yw = f2y * quaternion.scalar();
    const float f2zw = f2z * quaternion.scalar();
    const float f2xx = f2x * quaternion.x();
    const float f2xy = f2x * quaternion.y();
    const float f2xz = f2x * quaternion.z();
    const float f2yy = f2y * quaternion.y();
    const float f2yz = f2y * quaternion.z();
    const float f2zz = f2z * quaternion.z();

    m.m[0][0] = 1.0f - (f2yy + f2zz);
    m.m[1][0] =         f2xy - f2zw;
    m.m[2][0] =         f2xz + f2yw;
    m.m[3][0] = 0.0f;
    m.m[0][1] =         f2xy + f2zw;
    m.m[1][1] = 1.0f - (f2xx + f2zz);
    m.m[2][1] =         f2yz - f2xw;
    m.m[3][1] = 0.0f;
    m.m[0][2] =         f2xz - f2yw;
    m.m[1][2] =         f2yz + f2xw;
    m.m[2][2] = 1.0f - (f2xx + f2yy);
    m.m[3][2] = 0.0f;
    m.m[0][3] = 0.0f;
    m.m[1][3] = 0.0f;
    m.m[2][3] = 0.0f;
    m.m[3][3] = 1.0f;
    m.flagBits = LMatrix4x4Flag::Rotation;
    *this *= m;
}

void LMatrix4x4::ortho(const LRect<int> & rect)
{
    // Note: rect.right() and rect.bottom() subtract 1 in LRect<int> ,
    // which gives the location of a pixel within the rectangle,
    // instead of the extent of the rectangle.  We want the extent.
    // LRect<double>  expresses the extent properly.
    ortho(rect.m_x, rect.m_x + rect.m_width, rect.m_y + rect.m_height, rect.m_y, -1.0f, 1.0f);
}

/*!
    \overload

    Multiplies this matrix by another that applies an orthographic
    projection for a window with boundaries specified by \a rect.
    The near and far clipping planes will be -1 and 1 respectively.

    \sa frustum(), perspective()
*/
void LMatrix4x4::ortho(const LRect<double> & rect)
{
    ortho(rect.m_x, rect.x2(), rect.y2(), rect.m_y, -1.0f, 1.0f);
}

/*!
    Multiplies this matrix by another that applies an orthographic
    projection for a window with lower-left corner (\a left, \a bottom),
    upper-right corner (\a right, \a top), and the specified \a nearPlane
    and \a farPlane clipping planes.

    \sa frustum(), perspective()
*/
void LMatrix4x4::ortho(float left, float right, float bottom, float top, float nearPlane, float farPlane)
{
    // Bail out if the projection volume is zero-sized.
    if (left == right || bottom == top || nearPlane == farPlane)
        return;

    // Construct the projection.
    const float width = right - left;
    const float invheight = top - bottom;
    const float clip = farPlane - nearPlane;

   LMatrix4x4 m(Uninitialized);
    m.m[0][0] = 2.0f / width;
    m.m[1][0] = 0.0f;
    m.m[2][0] = 0.0f;
    m.m[3][0] = -(left + right) / width;
    m.m[0][1] = 0.0f;
    m.m[1][1] = 2.0f / invheight;
    m.m[2][1] = 0.0f;
    m.m[3][1] = -(top + bottom) / invheight;
    m.m[0][2] = 0.0f;
    m.m[1][2] = 0.0f;
    m.m[2][2] = -2.0f / clip;
    m.m[3][2] = -(nearPlane + farPlane) / clip;
    m.m[0][3] = 0.0f;
    m.m[1][3] = 0.0f;
    m.m[2][3] = 0.0f;
    m.m[3][3] = 1.0f;
    m.flagBits = LMatrix4x4Flag::Translation | LMatrix4x4Flag::Scale;

    // Apply the projection.
    *this *= m;
}

void LMatrix4x4::frustum(float left, float right, float bottom, float top, float nearPlane, float farPlane)
{
    // Bail out if the projection volume is zero-sized.
    if (left == right || bottom == top || nearPlane == farPlane)
        return;

    // Construct the projection.

    LMatrix4x4 m(Uninitialized);
    const float width = right - left;
    const float invheight = top - bottom;
    const float clip = farPlane - nearPlane;
    m.m[0][0] = 2.0f * nearPlane / width;
    m.m[1][0] = 0.0f;
    m.m[2][0] = (left + right) / width;
    m.m[3][0] = 0.0f;
    m.m[0][1] = 0.0f;
    m.m[1][1] = 2.0f * nearPlane / invheight;
    m.m[2][1] = (top + bottom) / invheight;
    m.m[3][1] = 0.0f;
    m.m[0][2] = 0.0f;
    m.m[1][2] = 0.0f;
    m.m[2][2] = -(nearPlane + farPlane) / clip;
    m.m[3][2] = -2.0f * nearPlane * farPlane / clip;
    m.m[0][3] = 0.0f;
    m.m[1][3] = 0.0f;
    m.m[2][3] = -1.0f;
    m.m[3][3] = 0.0f;
    m.flagBits = LMatrix4x4Flag::General;

    // Apply the projection.
    *this *= m;
}


void LMatrix4x4::perspective(float verticalAngle, float aspectRatio, float nearPlane, float farPlane)
{
    // Bail out if the projection volume is zero-sized.
    if (nearPlane == farPlane || aspectRatio == 0.0f)
        return;

    // Construct the projection.

    LMatrix4x4 m(Uninitialized);
    const float radians = lDegreesToRadians(verticalAngle / 2.0f);
    const float sine = std::sin(radians);
    if (sine == 0.0f)
        return;
    float cotan = std::cos(radians) / sine;
    float clip = farPlane - nearPlane;
    m.m[0][0] = cotan / aspectRatio;
    m.m[1][0] = 0.0f;
    m.m[2][0] = 0.0f;
    m.m[3][0] = 0.0f;
    m.m[0][1] = 0.0f;
    m.m[1][1] = cotan;
    m.m[2][1] = 0.0f;
    m.m[3][1] = 0.0f;
    m.m[0][2] = 0.0f;
    m.m[1][2] = 0.0f;
    m.m[2][2] = -(nearPlane + farPlane) / clip;
    m.m[3][2] = -(2.0f * nearPlane * farPlane) / clip;
    m.m[0][3] = 0.0f;
    m.m[1][3] = 0.0f;
    m.m[2][3] = -1.0f;
    m.m[3][3] = 0.0f;
    m.flagBits = LMatrix4x4Flag::General;

    // Apply the projection.
    *this *= m;
}

void LMatrix4x4::lookAt(const LVector3D& eye, const LVector3D& center, const LVector3D& up)
{
    LVector3D forward = center - eye;
    if (lFuzzyIsZero(forward.x()) && lFuzzyIsZero(forward.y()) && lFuzzyIsZero(forward.z()))
        return;

    forward.normalize();
    LVector3D side = LVector3D::crossProduct(forward, up).normalized();
    LVector3D upVector = LVector3D::crossProduct(side, forward);


    LMatrix4x4 m(Uninitialized);
    m.m[0][0] = side.x();
    m.m[1][0] = side.y();
    m.m[2][0] = side.z();
    m.m[3][0] = 0.0f;
    m.m[0][1] = upVector.x();
    m.m[1][1] = upVector.y();
    m.m[2][1] = upVector.z();
    m.m[3][1] = 0.0f;
    m.m[0][2] = -forward.x();
    m.m[1][2] = -forward.y();
    m.m[2][2] = -forward.z();
    m.m[3][2] = 0.0f;
    m.m[0][3] = 0.0f;
    m.m[1][3] = 0.0f;
    m.m[2][3] = 0.0f;
    m.m[3][3] = 1.0f;
    m.flagBits = LMatrix4x4Flag::Rotation;

    *this *= m;
    translate(-eye);
}

void LMatrix4x4::viewport(float left, float bottom, float width, float height, float nearPlane, float farPlane)
{
    const float w2 = width / 2.0f;
    const float h2 = height / 2.0f;

    LMatrix4x4 m(Uninitialized);
    m.m[0][0] = w2;
    m.m[1][0] = 0.0f;
    m.m[2][0] = 0.0f;
    m.m[3][0] = left + w2;
    m.m[0][1] = 0.0f;
    m.m[1][1] = h2;
    m.m[2][1] = 0.0f;
    m.m[3][1] = bottom + h2;
    m.m[0][2] = 0.0f;
    m.m[1][2] = 0.0f;
    m.m[2][2] = (farPlane - nearPlane) / 2.0f;
    m.m[3][2] = (nearPlane + farPlane) / 2.0f;
    m.m[0][3] = 0.0f;
    m.m[1][3] = 0.0f;
    m.m[2][3] = 0.0f;
    m.m[3][3] = 1.0f;
    m.flagBits = LMatrix4x4Flag::General;

    *this *= m;
}

void LMatrix4x4::flipCoordinates()
{
    // Multiplying the y and z coordinates with -1 does NOT flip between right-handed and
    // left-handed coordinate systems, it just rotates 180 degrees around the x axis, so
    // I'm deprecating this function.
    if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Rotation2D) {
        // Translation | Scale
        m[1][1] = -m[1][1];
        m[2][2] = -m[2][2];
    } else {
        m[1][0] = -m[1][0];
        m[1][1] = -m[1][1];
        m[1][2] = -m[1][2];
        m[1][3] = -m[1][3];
        m[2][0] = -m[2][0];
        m[2][1] = -m[2][1];
        m[2][2] = -m[2][2];
        m[2][3] = -m[2][3];
    }
    flagBits |= LMatrix4x4Flag::Scale;
}

/*!
    Retrieves the 16 items in this matrix and copies them to \a values
    in row-major order.
*/
void LMatrix4x4::copyDataTo(float *values) const
{
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            values[row * 4 + col] = float(m[col][row]);
}

LTransform2D LMatrix4x4::toTransform() const
{
    return LTransform2D(m[0][0], m[0][1], m[0][3],
                      m[1][0], m[1][1], m[1][3],
                      m[3][0], m[3][1], m[3][3]);
}

LTransform2D LMatrix4x4::toTransform(float distanceToPlane) const
{
    if (distanceToPlane == 1024.0f) {
        // Optimize the common case with constants.
        return LTransform2D(m[0][0], m[0][1], m[0][3] - m[0][2] * inv_dist_to_plane,
                          m[1][0], m[1][1], m[1][3] - m[1][2] * inv_dist_to_plane,
                          m[3][0], m[3][1], m[3][3] - m[3][2] * inv_dist_to_plane);
    } else if (distanceToPlane != 0.0f) {
        // The following projection matrix is pre-multiplied with "matrix":
        //      | 1 0 0 0 |
        //      | 0 1 0 0 |
        //      | 0 0 1 0 |
        //      | 0 0 d 1 |
        // where d = -1 / distanceToPlane.  After projection, row 3 and
        // column 3 are dropped to form the final LTransform2D.
        float d = 1.0f / distanceToPlane;
        return LTransform2D(m[0][0], m[0][1], m[0][3] - m[0][2] * d,
                          m[1][0], m[1][1], m[1][3] - m[1][2] * d,
                          m[3][0], m[3][1], m[3][3] - m[3][2] * d);
    } else {
        // Orthographic projection: drop row 3 and column 3.
        return LTransform2D(m[0][0], m[0][1], m[0][3],
                          m[1][0], m[1][1], m[1][3],
                          m[3][0], m[3][1], m[3][3]);
    }
}

LRect<int>  LMatrix4x4::mapRect(const LRect<int> & rect) const
{
    if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Scale) {
        // Translation
        return LRect<int> (std::lround(rect.m_x + m[3][0]),
                     std::lround(rect.m_y + m[3][1]),
                     rect.m_width, rect.m_height);
    } else if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Rotation2D) {
        // Translation | Scale
        float x = rect.m_x * m[0][0] + m[3][0];
        float y = rect.m_y * m[1][1] + m[3][1];
        float w = rect.m_width * m[0][0];
        float h = rect.m_height * m[1][1];
        if (w < 0) {
            w = -w;
            x -= w;
        }
        if (h < 0) {
            h = -h;
            y -= h;
        }
        return LRect<int> (std::lround(x), std::lround(y), std::lround(w), std::lround(h));
    }

    LPoint<int> tl = map(rect.topLeft());
    LPoint<int> tr = map(LPoint<int>(rect.m_x + rect.m_width, rect.m_y));
    LPoint<int> bl = map(LPoint<int>(rect.m_x, rect.m_y + rect.m_height));
    LPoint<int> br = map(LPoint<int>(rect.m_x + rect.m_width,
                           rect.m_y + rect.m_height));

    int xmin = std::min<int>(std::min<int>(tl.m_x, tr.m_x), std::min<int>(bl.m_x, br.m_x));
    int xmax = std::max<int>(std::max<int>(tl.m_x, tr.m_x), std::max<int>(bl.m_x, br.m_x));
    int ymin = std::min<int>(std::min<int>(tl.m_y, tr.m_y), std::min<int>(bl.m_y, br.m_y));
    int ymax = std::max<int>(std::max<int>(tl.m_y, tr.m_y), std::max<int>(bl.m_y, br.m_y));

    return LRect<int> (xmin, ymin, xmax - xmin, ymax - ymin);
}


LRect<double>  LMatrix4x4::mapRect(const LRect<double> & rect) const
{
    if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Scale) {
        // Translation
        return rect.translated(m[3][0], m[3][1]);
    } else if ((uint32_t)flagBits < (uint32_t)LMatrix4x4Flag::Rotation2D) {
        // Translation | Scale
        float x = rect.m_x * m[0][0] + m[3][0];
        float y = rect.m_y * m[1][1] + m[3][1];
        float w = rect.m_width * m[0][0];
        float h = rect.m_height * m[1][1];
        if (w < 0) {
            w = -w;
            x -= w;
        }
        if (h < 0) {
            h = -h;
            y -= h;
        }
        return LRect<double> (x, y, w, h);
    }

    LPoint<double> tl = map(rect.topLeft()); LPoint<double> tr = map(rect.topRight());
    LPoint<double> bl = map(rect.bottomLeft()); LPoint<double> br = map(rect.bottomRight());

    float xmin = std::min(std::min(tl.m_x, tr.m_x), std::min(bl.m_x, br.m_x));
    float xmax = std::max(std::max(tl.m_x, tr.m_x), std::max(bl.m_x, br.m_x));
    float ymin = std::min(std::min(tl.m_y, tr.m_y), std::min(bl.m_y, br.m_y));
    float ymax = std::max(std::max(tl.m_y, tr.m_y), std::max(bl.m_y, br.m_y));

    return LRect<double> (LPoint<double>(xmin, ymin), LPoint<double>(xmax, ymax));
}
// Helper routine for inverting orthonormal matrices that consist
// of just rotations and translations.
LMatrix4x4 LMatrix4x4::orthonormalInverse() const
{

    LMatrix4x4 result(Uninitialized);

    result.m[0][0] = m[0][0];
    result.m[1][0] = m[0][1];
    result.m[2][0] = m[0][2];

    result.m[0][1] = m[1][0];
    result.m[1][1] = m[1][1];
    result.m[2][1] = m[1][2];

    result.m[0][2] = m[2][0];
    result.m[1][2] = m[2][1];
    result.m[2][2] = m[2][2];

    result.m[0][3] = 0.0f;
    result.m[1][3] = 0.0f;
    result.m[2][3] = 0.0f;

    result.m[3][0] = -(result.m[0][0] * m[3][0] + result.m[1][0] * m[3][1] + result.m[2][0] * m[3][2]);
    result.m[3][1] = -(result.m[0][1] * m[3][0] + result.m[1][1] * m[3][1] + result.m[2][1] * m[3][2]);
    result.m[3][2] = -(result.m[0][2] * m[3][0] + result.m[1][2] * m[3][1] + result.m[2][2] * m[3][2]);
    result.m[3][3] = 1.0f;

    result.flagBits = flagBits;

    return result;
}

void LMatrix4x4::optimize()
{
    // If the last row is not (0, 0, 0, 1), the matrix is not a special type.
    flagBits = LMatrix4x4Flag::General;
    if (m[0][3] != 0 || m[1][3] != 0 || m[2][3] != 0 || m[3][3] != 1)
        return;

    flagBits &= ~(uint32_t)LMatrix4x4Flag::Perspective;

    // If the last column is (0, 0, 0, 1), then there is no translation.
    if (m[3][0] == 0 && m[3][1] == 0 && m[3][2] == 0)
        flagBits &= ~(uint32_t)LMatrix4x4Flag::Translation;

    // If the two first elements of row 3 and column 3 are 0, then any rotation must be about Z.
    if (!m[0][2] && !m[1][2] && !m[2][0] && !m[2][1]) {
        flagBits &= ~(uint32_t)LMatrix4x4Flag::Rotation;
        // If the six non-diagonal elements in the top left 3x3 matrix are 0, there is no rotation.
        if (!m[0][1] && !m[1][0]) {
            flagBits &= ~(uint32_t)LMatrix4x4Flag::Rotation2D;
            // Check for identity.
            if (m[0][0] == 1 && m[1][1] == 1 && m[2][2] == 1)
                flagBits &= ~(uint32_t)LMatrix4x4Flag::Scale;
        } else {
            // If the columns are orthonormal and form a right-handed system, then there is no scale.
            const Double4x4 mm = copyToDoubles(m);
            double det = matrixDet2(mm, 0, 1, 0, 1);
            double lenX = mm[0][0] * mm[0][0] + mm[0][1] * mm[0][1];
            double lenY = mm[1][0] * mm[1][0] + mm[1][1] * mm[1][1];
            double lenZ = mm[2][2];
            if (lFuzzyCompare(det, 1.0) && lFuzzyCompare(lenX, 1.0)
                && lFuzzyCompare(lenY, 1.0) && lFuzzyCompare(lenZ, 1.0))
            {
                flagBits &= ~(uint32_t)LMatrix4x4Flag::Scale;
            }
        }
    } else {
        // If the columns are orthonormal and form a right-handed system, then there is no scale.
        const Double4x4 mm = copyToDoubles(m);
        double det = matrixDet3(mm, 0, 1, 2, 0, 1, 2);
        double lenX = mm[0][0] * mm[0][0] + mm[0][1] * mm[0][1] + mm[0][2] * mm[0][2];
        double lenY = mm[1][0] * mm[1][0] + mm[1][1] * mm[1][1] + mm[1][2] * mm[1][2];
        double lenZ = mm[2][0] * mm[2][0] + mm[2][1] * mm[2][1] + mm[2][2] * mm[2][2];
        if (lFuzzyCompare(det, 1.0) && lFuzzyCompare(lenX, 1.0)
            && lFuzzyCompare(lenY, 1.0) && lFuzzyCompare(lenZ, 1.0))
        {
            flagBits &= ~(uint32_t)LMatrix4x4Flag::Scale;
        }
    }
}
LK_NAMESPACE_END