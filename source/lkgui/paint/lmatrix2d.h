#ifndef LMATRIX2D_H
#define LMATRIX2D_H

#include "lkcore/lookiconfig.h"

LK_NAMESPACE_BEGIN

class LKGUIDEF LMatrix2D
{
public:
    LMatrix2D();
    LMatrix2D(double m11, double m12,
              double m21, double m22,
              double dx, double dy);

    double m_11=1, m_12=0;
    double m_21=0, m_22=1;
    double m_dx=0, m_dy=0;

    LMatrix2D inverted(bool *invertible = nullptr) const;
    inline double determinant() const { return m_11*m_22 - m_12*m_21; }
    void reset();
};


LK_NAMESPACE_END
#endif // LMATRIX_H
