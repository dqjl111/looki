#include "lrhiimplementation.h"
#include "lrhiplatform.h"
#ifdef WIN32
#include "lkgui/rhi/d3d/lrhid3d12.h"
#endif

LK_NAMESPACE_BEGIN

LRhiImplementation::LRhiImplementation() {}

std::vector<std::shared_ptr<LRhiAdapter> > LRhiImplementation::enumerateAdaptersBeforeCreate(std::shared_ptr<LRhiNativeHandles> nativeHandles) const
{
    return std::vector<std::shared_ptr<LRhiAdapter> >();
}

std::shared_ptr<LRhiImplementation> LRhiImplementation::newInstance(LRhi::Implementation impl, std::shared_ptr<LRhiInitParams> params, std::shared_ptr<LRhiNativeHandles> importDevice)
{
    std::shared_ptr<LRhiImplementation> d = nullptr;

    switch (impl) {
    case LRhi::D3D:{
#ifdef WIN32
        std::shared_ptr<LRhiD3D12InitParams> d3dParams = std::dynamic_pointer_cast<LRhiD3D12InitParams>(params);
        std::shared_ptr<LRhiD3D12NativeHandles> d3dDev = std::dynamic_pointer_cast<LRhiD3D12NativeHandles>(importDevice);
        d = makeInit<LRhiD3d12>(d3dParams, d3dDev);
        break;
#endif
    }
    default:{
        break;
    }
    }
    return d;
}

void LRhiImplementation::prepareForCreate(std::shared_ptr<LRhi> rhi, LRhi::Implementation impl, LRhi::Flags flags, std::shared_ptr<LRhiAdapter> adapter)
{
    m_rhi = rhi;

    m_debugMarkers = flags | LRhiFlag::EnableDebugMarkers;
    m_implType = impl;
    m_requestedRhiAdapter = adapter;
}


LK_NAMESPACE_END
