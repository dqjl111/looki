#include "lvectornd.h"
#include "lmatrix4x4.h"

LK_NAMESPACE_BEGIN

bool lFuzzyCompare(const LVector2D& v1, const LVector2D& v2) noexcept
{
    return fuzzyCompare(v1.v[0], v2.v[0])
    && fuzzyCompare(v1.v[1], v2.v[1]);
}

LVector3D LVector3D::project(const LMatrix4x4 &modelView, const LMatrix4x4 &projection, const LRect<int> &viewport) const
{
    LVector4D tmp(*this, 1.0f);
    tmp = projection * modelView * tmp;
    if (lFuzzyIsZero(tmp.w()))
        tmp.setW(1.0f);
    tmp /= tmp.w();

    tmp = tmp * 0.5f + LVector4D(0.5f, 0.5f, 0.5f, 0.5f);
    tmp.setX(tmp.x() * viewport.m_width + viewport.m_x);
    tmp.setY(tmp.y() * viewport.m_height + viewport.m_y);

    return tmp.toVector3D();
}

LVector3D LVector3D::unproject(const LMatrix4x4 &modelView, const LMatrix4x4 &projection, const LRect<int> &viewport) const
{
    LMatrix4x4 inverse = LMatrix4x4( projection * modelView ).inverted();

    LVector4D tmp(*this, 1.0f);
    tmp.setX((tmp.x() - float(viewport.m_x)) / float(viewport.width()));
    tmp.setY((tmp.y() - float(viewport.m_y)) / float(viewport.height()));
    tmp = tmp * 2.0f - LVector4D(1.0f, 1.0f, 1.0f, 1.0f);

    LVector4D obj = inverse * tmp;
    if (lFuzzyIsZero(obj.w()))
        obj.setW(1.0f);
    obj /= obj.w();
    return obj.toVector3D();
}

bool lFuzzyCompare(const LVector3D& v1, const LVector3D& v2) noexcept
{
    return fuzzyCompare(v1.v[0], v2.v[0])
    && fuzzyCompare(v1.v[1], v2.v[1])
        && fuzzyCompare(v1.v[2], v2.v[2]);
}

bool lFuzzyCompare(const LVector4D& v1, const LVector4D& v2) noexcept
{
    return fuzzyCompare(v1.v[0], v2.v[0])
    && fuzzyCompare(v1.v[1], v2.v[1])
        && fuzzyCompare(v1.v[2], v2.v[2])
        && fuzzyCompare(v1.v[3], v2.v[3]);
}

LK_NAMESPACE_END