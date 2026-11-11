#include "lmatrix2d.h"

LK_NAMESPACE_BEGIN

LMatrix2D::LMatrix2D()
{

}

LMatrix2D::LMatrix2D(double m11, double m12, double m21, double m22, double dx, double dy)
    :m_11(m11), m_12(m12), m_21(m21), m_22(m22), m_dx(dx), m_dy(dy)
{

}

LMatrix2D LMatrix2D::inverted(bool *invertible) const
{
    double dtr = determinant();
    if (dtr == 0.0) {
        if (invertible){
            *invertible = false;                // singular matrix
        }
        return LMatrix2D();
    }
    else {                                        // invertible matrix
        if (invertible){
            *invertible = true;
        }
        double dinv = 1.0/dtr;
        return LMatrix2D((m_22*dinv), (-m_12*dinv),
                       (-m_21*dinv), (m_11*dinv),
                       ((m_21*m_dy - m_22*m_dx)*dinv),
                       ((m_12*m_dx - m_11*m_dy)*dinv));
    }
}

void LMatrix2D::reset()
{
    m_11=1; m_12=0;  m_21=0; m_22=1; m_dx=0; m_dy=0;
}

LK_NAMESPACE_END
