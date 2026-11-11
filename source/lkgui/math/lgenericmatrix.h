#ifndef LGENERICMATRIX_H
#define LGENERICMATRIX_H

#include "lkcore/lcoreenums.h"

LK_NAMESPACE_BEGIN

template <int N, int M, typename T>
class LGenericMatrix
{
public:
    LGenericMatrix();
    explicit LGenericMatrix(Initialization) {}
    explicit LGenericMatrix(const T *values);

    const T& operator()(int row, int column) const;
    T& operator()(int row, int column);

    bool isIdentity() const;
    void setToIdentity();

    void fill(T value);

    [[nodiscard]] LGenericMatrix<M, N, T> transposed() const;

    LGenericMatrix<N, M, T>& operator+=(const LGenericMatrix<N, M, T>& other);
    LGenericMatrix<N, M, T>& operator-=(const LGenericMatrix<N, M, T>& other);
    LGenericMatrix<N, M, T>& operator*=(T factor);
    LGenericMatrix<N, M, T>& operator/=(T divisor);
    bool operator==(const LGenericMatrix<N, M, T>& other) const;
    bool operator!=(const LGenericMatrix<N, M, T>& other) const;

    void copyDataTo(T *values) const;

    T *data() { return *m; }
    const T *data() const { return *m; }
    const T *constData() const { return *m; }

    template<int NN, int MM, typename TT>
    friend LGenericMatrix<NN, MM, TT> operator+(const LGenericMatrix<NN, MM, TT>& m1, const LGenericMatrix<NN, MM, TT>& m2);
    template<int NN, int MM, typename TT>
    friend LGenericMatrix<NN, MM, TT> operator-(const LGenericMatrix<NN, MM, TT>& m1, const LGenericMatrix<NN, MM, TT>& m2);
    template<int NN, int M1, int M2, typename TT>
    friend LGenericMatrix<M1, M2, TT> operator*(const LGenericMatrix<NN, M2, TT>& m1, const LGenericMatrix<M1, NN, TT>& m2);
    template<int NN, int MM, typename TT>
    friend LGenericMatrix<NN, MM, TT> operator-(const LGenericMatrix<NN, MM, TT>& matrix);
    template<int NN, int MM, typename TT>
    friend LGenericMatrix<NN, MM, TT> operator*(TT factor, const LGenericMatrix<NN, MM, TT>& matrix);
    template<int NN, int MM, typename TT>
    friend LGenericMatrix<NN, MM, TT> operator*(const LGenericMatrix<NN, MM, TT>& matrix, TT factor);
    template<int NN, int MM, typename TT>
    friend LGenericMatrix<NN, MM, TT> operator/(const LGenericMatrix<NN, MM, TT>& matrix, TT divisor);

private:
    T m[N][M];    // Column-major order to match OpenGL.

    template <int NN, int MM, typename TT>
    friend class LGenericMatrix;
};

template <int N, int M, typename T>
inline LGenericMatrix<N, M, T>::LGenericMatrix()
{
    setToIdentity();
}

template <int N, int M, typename T>
inline LGenericMatrix<N, M, T>::LGenericMatrix(const T *values)
{
    for (int col = 0; col < N; ++col)
        for (int row = 0; row < M; ++row)
            m[col][row] = values[row * N + col];
}

template <int N, int M, typename T>
inline const T& LGenericMatrix<N, M, T>::operator()(int row, int column) const
{
    return m[column][row];
}

template <int N, int M, typename T>
inline T& LGenericMatrix<N, M, T>::operator()(int row, int column)
{
    return m[column][row];
}

template <int N, int M, typename T>
inline bool LGenericMatrix<N, M, T>::isIdentity() const
{
    for (int col = 0; col < N; ++col) {
        for (int row = 0; row < M; ++row) {
            if (row == col) {
                if (m[col][row] != 1.0f)
                    return false;
            } else {
                if (m[col][row] != 0.0f)
                    return false;
            }
        }
    }
    return true;
}

template <int N, int M, typename T>
inline void LGenericMatrix<N, M, T>::setToIdentity()
{
    for (int col = 0; col < N; ++col) {
        for (int row = 0; row < M; ++row) {
            if (row == col)
                m[col][row] = 1.0f;
            else
                m[col][row] = 0.0f;
        }
    }
}

template <int N, int M, typename T>
inline void LGenericMatrix<N, M, T>::fill(T value)
{
    for (int col = 0; col < N; ++col)
        for (int row = 0; row < M; ++row)
            m[col][row] = value;
}

template <int N, int M, typename T>
inline LGenericMatrix<M, N, T> LGenericMatrix<N, M, T>::transposed() const
{
    LGenericMatrix<M, N, T> result(Uninitialized);
    for (int row = 0; row < M; ++row)
        for (int col = 0; col < N; ++col)
            result.m[row][col] = m[col][row];
    return result;
}

template <int N, int M, typename T>
inline LGenericMatrix<N, M, T>& LGenericMatrix<N, M, T>::operator+=(const LGenericMatrix<N, M, T>& other)
{
    for (int row = 0; row < M; ++row)
        for (int col = 0; col < N; ++col)
            m[col][row] += other.m[col][row];
    return *this;
}

template <int N, int M, typename T>
inline LGenericMatrix<N, M, T>& LGenericMatrix<N, M, T>::operator-=(const LGenericMatrix<N, M, T>& other)
{
    for (int row = 0; row < M; ++row)
        for (int col = 0; col < N; ++col)
            m[col][row] -= other.m[col][row];
    return *this;
}

template <int N, int M, typename T>
inline LGenericMatrix<N, M, T>& LGenericMatrix<N, M, T>::operator*=(T factor)
{
    for (int row = 0; row < M; ++row)
        for (int col = 0; col < N; ++col)
            m[col][row] *= factor;
    return *this;
}


template <int N, int M, typename T>
inline bool LGenericMatrix<N, M, T>::operator==(const LGenericMatrix<N, M, T>& other) const
{
    for (int row = 0; row < M; ++row)
        for (int col = 0; col < N; ++col)  {
            if (m[col][row] != other.m[col][row])
                return false;
        }
    return true;
}

template <int N, int M, typename T>
inline bool LGenericMatrix<N, M, T>::operator!=(const LGenericMatrix<N, M, T>& other) const
{
    return !(*this == other);
}


template <int N, int M, typename T>
inline LGenericMatrix<N, M, T>& LGenericMatrix<N, M, T>::operator/=(T divisor)
{
    for (int row = 0; row < M; ++row)
        for (int col = 0; col < N; ++col)
            m[col][row] /= divisor;
    return *this;
}

template <int N, int M, typename T>
inline LGenericMatrix<N, M, T> operator+(const LGenericMatrix<N, M, T>& m1, const LGenericMatrix<N, M, T>& m2)
{
    LGenericMatrix<N, M, T> result(Uninitialized);
    for (int row = 0; row < M; ++row)
        for (int col = 0; col < N; ++col)
            result.m[col][row] = m1.m[col][row] + m2.m[col][row];
    return result;
}

template <int N, int M, typename T>
inline LGenericMatrix<N, M, T> operator-(const LGenericMatrix<N, M, T>& m1, const LGenericMatrix<N, M, T>& m2)
{
    LGenericMatrix<N, M, T> result(Uninitialized);
    for (int row = 0; row < M; ++row)
        for (int col = 0; col < N; ++col)
            result.m[col][row] = m1.m[col][row] - m2.m[col][row];
    return result;
}

template <int N, int M1, int M2, typename T>
inline LGenericMatrix<M1, M2, T> operator*(const LGenericMatrix<N, M2, T>& m1, const LGenericMatrix<M1, N, T>& m2)
{
    LGenericMatrix<M1, M2, T> result(Uninitialized);
    for (int row = 0; row < M2; ++row) {
        for (int col = 0; col < M1; ++col) {
            T sum(0.0f);
            for (int j = 0; j < N; ++j)
                sum += m1.m[j][row] * m2.m[col][j];
            result.m[col][row] = sum;
        }
    }
    return result;
}

template <int N, int M, typename T>
inline LGenericMatrix<N, M, T> operator-(const LGenericMatrix<N, M, T>& matrix)
{
    LGenericMatrix<N, M, T> result(Uninitialized);
    for (int row = 0; row < M; ++row)
        for (int col = 0; col < N; ++col)
            result.m[col][row] = -matrix.m[col][row];
    return result;
}

template <int N, int M, typename T>
inline LGenericMatrix<N, M, T> operator*(T factor, const LGenericMatrix<N, M, T>& matrix)
{
    LGenericMatrix<N, M, T> result(Uninitialized);
    for (int row = 0; row < M; ++row)
        for (int col = 0; col < N; ++col)
            result.m[col][row] = matrix.m[col][row] * factor;
    return result;
}

template <int N, int M, typename T>
inline LGenericMatrix<N, M, T> operator*(const LGenericMatrix<N, M, T>& matrix, T factor)
{
    LGenericMatrix<N, M, T> result(Uninitialized);
    for (int row = 0; row < M; ++row)
        for (int col = 0; col < N; ++col)
            result.m[col][row] = matrix.m[col][row] * factor;
    return result;
}

template <int N, int M, typename T>
inline LGenericMatrix<N, M, T> operator/(const LGenericMatrix<N, M, T>& matrix, T divisor)
{
    LGenericMatrix<N, M, T> result(Uninitialized);
    for (int row = 0; row < M; ++row)
        for (int col = 0; col < N; ++col)
            result.m[col][row] = matrix.m[col][row] / divisor;
    return result;
}

template <int N, int M, typename T>
inline void LGenericMatrix<N, M, T>::copyDataTo(T *values) const
{
    for (int col = 0; col < N; ++col)
        for (int row = 0; row < M; ++row)
            values[row * N + col] = T(m[col][row]);
}

// Define aliases for the useful variants of LGenericMatrix.
typedef LGenericMatrix<2, 2, float> LMatrix2x2;
typedef LGenericMatrix<2, 3, float> LMatrix2x3;
typedef LGenericMatrix<2, 4, float> LMatrix2x4;
typedef LGenericMatrix<3, 2, float> LMatrix3x2;
typedef LGenericMatrix<3, 3, float> LMatrix3x3;
typedef LGenericMatrix<3, 4, float> LMatrix3x4;
typedef LGenericMatrix<4, 2, float> LMatrix4x2;
typedef LGenericMatrix<4, 3, float> LMatrix4x3;


LK_NAMESPACE_END
#endif // LGENERICMATRIX_H
