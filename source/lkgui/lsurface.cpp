#include "lsurface.h"

LK_NAMESPACE_BEGIN

LSurfaceFormat::LSurfaceFormat(LFlags<LSurfaceFmtOpt> options): m_opts(options)
{

}

bool LSurfaceFormat::hasAlpha() const
{
    return m_alphaBufferSize > 0;
}

LSurface::LSurface()
{

}

LSurface::LSurface(SurfaceClass type) :m_type(type){
}


LK_NAMESPACE_END


