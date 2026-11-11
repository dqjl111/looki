#include "lplatformsurface.h"

LK_NAMESPACE_BEGIN

LPlatformSurface::LPlatformSurface(std::shared_ptr<LSurface> surface): m_surface(surface)
{

}

LPlatformSurface::~LPlatformSurface()
{

}

LSurfaceFormat LPlatformSurface::format() const
{
    return LSurfaceFormat();
}

std::shared_ptr<LPlatformScreen> LPlatformSurface::screen() const
{
    return nullptr;
}


LK_NAMESPACE_END
