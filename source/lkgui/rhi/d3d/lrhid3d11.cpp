#include "lrhid3d11.h"
#include "lkgui/paint/lcolor.h"
#include "lkcore/ldebug.h"

#include <algorithm>
#include <cstring>
#include <utility>

// windows.h 把 max/min 定义成宏，会破坏下面用到的 std::max/std::min
#ifdef max
#  undef max
#endif
#ifdef min
#  undef min
#endif

LK_NAMESPACE_BEGIN

// ---------------------------------------------------------------------------
// 格式映射
// ---------------------------------------------------------------------------
DXGI_FORMAT LRhiD3d11::toDxgiFormat(LRhiTexture::Format f)
{
    switch (f) {
    case LRhiTexture::RGBA8:        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case LRhiTexture::BGRA8:        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case LRhiTexture::R8:           return DXGI_FORMAT_R8_UNORM;
    case LRhiTexture::RG8:          return DXGI_FORMAT_R8G8_UNORM;
    case LRhiTexture::R16:          return DXGI_FORMAT_R16_UNORM;
    case LRhiTexture::RG16:         return DXGI_FORMAT_R16G16_UNORM;
    case LRhiTexture::RGBA16F:      return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case LRhiTexture::RGBA32F:      return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case LRhiTexture::R16F:         return DXGI_FORMAT_R16_FLOAT;
    case LRhiTexture::R32F:         return DXGI_FORMAT_R32_FLOAT;
    case LRhiTexture::RGB10A2:      return DXGI_FORMAT_R10G10B10A2_UNORM;
    case LRhiTexture::R8UI:         return DXGI_FORMAT_R8_UINT;
    case LRhiTexture::R32UI:        return DXGI_FORMAT_R32_UINT;
    case LRhiTexture::RGBA32UI:     return DXGI_FORMAT_R32G32B32A32_UINT;
    case LRhiTexture::D16:          return DXGI_FORMAT_D16_UNORM;
    case LRhiTexture::D24:          return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case LRhiTexture::D24S8:        return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case LRhiTexture::D32F:         return DXGI_FORMAT_D32_FLOAT;
    case LRhiTexture::D32FS8:       return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    case LRhiTexture::BC1:          return DXGI_FORMAT_BC1_UNORM;
    case LRhiTexture::BC2:          return DXGI_FORMAT_BC2_UNORM;
    case LRhiTexture::BC3:          return DXGI_FORMAT_BC3_UNORM;
    case LRhiTexture::BC4:          return DXGI_FORMAT_BC4_UNORM;
    case LRhiTexture::BC5:          return DXGI_FORMAT_BC5_UNORM;
    case LRhiTexture::BC6H:         return DXGI_FORMAT_BC6H_UF16;
    case LRhiTexture::BC7:          return DXGI_FORMAT_BC7_UNORM;
    default:                        return DXGI_FORMAT_UNKNOWN;
    }
}

bool LRhiD3d11::shaderByteCode(const LShader &shader, LShaderKey key, std::vector<uint8_t> *out)
{
    auto it = shader.m_shaders.find(key);
    if (it == shader.m_shaders.end() || it->second == nullptr) {
        return false;
    }
    if (out != nullptr) {
        *out = it->second->m_shader;
    }
    return !it->second->m_shader.empty();
}

// ---------------------------------------------------------------------------
// LRhiD3d11
// ---------------------------------------------------------------------------
LRhiD3d11::LRhiD3d11(std::shared_ptr<LRhiD3D11InitParams> params,
                     std::shared_ptr<LRhiD3D11NativeHandles> importDevice)
    : m_params(std::move(params))
    , m_import(std::move(importDevice))
{
}

LRhiD3d11::~LRhiD3d11()
{
    destroy();
}

bool LRhiD3d11::createDevice()
{
    if (m_import != nullptr && m_import->dev != nullptr) {
        m_device = static_cast<ID3D11Device *>(m_import->dev);
        m_ownsDevice = false;
        if (m_import->context != nullptr) {
            m_context = static_cast<ID3D11DeviceContext1 *>(m_import->context);
        }
    }

    if (m_device == nullptr) {
        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        if (m_params != nullptr && m_params->enableDebugLayer) {
            flags |= D3D11_CREATE_DEVICE_DEBUG;
        }
        static const D3D_FEATURE_LEVEL levels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0,
        };
        D3D_FEATURE_LEVEL got = D3D_FEATURE_LEVEL_11_0;
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                       flags, levels, UINT(std::size(levels)),
                                       D3D11_SDK_VERSION,
                                       m_device.GetAddressOf(), &got, nullptr);
        if (FAILED(hr)) {
            lWarn() << "rhi/d3d11: D3D11CreateDevice failed, hr =" << int(hr);
            return false;
        }
        m_featureLevel = got;
        m_ownsDevice = true;
    }

    if (m_context == nullptr) {
        LD3D11ComPtr<ID3D11DeviceContext> ctx;
        m_device->GetImmediateContext(ctx.GetAddressOf());
        if (ctx == nullptr) {
            lWarn() << "rhi/d3d11: GetImmediateContext failed";
            return false;
        }
        ctx.As(&m_context);
        if (m_context == nullptr) {
            // 老驱动可能拿不到 1 版本，退回基础版本使用时再降级
            lWarn() << "rhi/d3d11: ID3D11DeviceContext1 unavailable";
            return false;
        }
    }

    // 拿 DXGI 工厂，交换链要用
    LD3D11ComPtr<IDXGIDevice> dxgiDevice;
    if (SUCCEEDED(m_device.As(&dxgiDevice)) && dxgiDevice != nullptr) {
        LD3D11ComPtr<IDXGIAdapter> adapter;
        dxgiDevice->GetAdapter(adapter.GetAddressOf());
        if (adapter != nullptr) {
            adapter.As(&m_adapter);
            if (m_adapter != nullptr) {
                DXGI_ADAPTER_DESC1 desc{};
                m_adapter->GetDesc1(&desc);
                m_driverInfo.m_deviceName = "D3D11 adapter";
                m_driverInfo.m_deviceId = desc.DeviceId;
                m_driverInfo.m_vendorId = desc.VendorId;
                m_driverInfo.m_deviceType = desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE
                        ? LRhiDriverInfo::CpuDevice
                        : LRhiDriverInfo::DiscreteDevice;
            }
            LD3D11ComPtr<IDXGIFactory2> factory;
            if (SUCCEEDED(adapter->GetParent(__uuidof(IDXGIFactory2),
                                             reinterpret_cast<void **>(factory.GetAddressOf())))
                    && factory != nullptr) {
                m_factory = factory;
            }
        }
    }

    m_native.dev = m_device.Get();
    m_native.context = m_context.Get();
    m_native.featureLevel = int(m_featureLevel);
    return true;
}

void LRhiD3d11::destroyDevice()
{
    m_cb.reset();
    m_context.Reset();
    m_factory.Reset();
    m_adapter.Reset();
    if (m_ownsDevice) {
        m_device.Reset();
    } else {
        m_device.Reset();
        m_ownsDevice = true;
    }
    m_native.dev = nullptr;
    m_native.context = nullptr;
}

bool LRhiD3d11::create(LFlags<LRhiFlag> flags)
{
    (void)flags;
    if (!createDevice()) {
        return false;
    }
    m_debugMarkers = m_debugMarkers || m_featureLevel >= D3D_FEATURE_LEVEL_11_0;

    m_cb = makeInit<LRhiD3d11CommandBuffer>(getPtrT<LRhiImplementation>());
    if (m_cb != nullptr) {
        m_cb->m_context = m_context;
    }
    return true;
}

void LRhiD3d11::destroy()
{
    m_cb.reset();
    destroyDevice();
}

// ---- 资源工厂 ----
std::shared_ptr<LRhiGraphicsPipeline> LRhiD3d11::createGraphicsPipeline()
{
    return makeInit<LRhiD3d11GraphicsPipeline>(getPtrT<LRhiImplementation>());
}

std::shared_ptr<LRhiComputePipeline> LRhiD3d11::createComputePipeline()
{
    return makeInit<LRhiD3d11ComputePipeline>(getPtrT<LRhiImplementation>());
}

std::shared_ptr<LRhiShaderResourceBindings> LRhiD3d11::createShaderResourceBindings()
{
    return makeInit<LRhiD3d11ShaderResourceBindings>(getPtrT<LRhiImplementation>());
}

std::shared_ptr<LRhiBuffer> LRhiD3d11::createBuffer(LRhiBuffer::Type type,
                                                    LRhiBuffer::UsageFlags usage,
                                                    uint32_t size)
{
    return makeInit<LRhiD3d11Buffer>(getPtrT<LRhiImplementation>(), type, usage, size);
}

std::shared_ptr<LRhiRenderBuffer> LRhiD3d11::createRenderBuffer(LRhiRenderBuffer::Type type,
                                                                const LSize<int> &pixelSize,
                                                                int sampleCount,
                                                                bool usedWithSwapChainOnly,
                                                                LRhiTexture::Format backingFormatHint)
{
    return makeInit<LRhiD3d11RenderBuffer>(getPtrT<LRhiImplementation>(), type, pixelSize,
                                           sampleCount, usedWithSwapChainOnly,
                                           backingFormatHint);
}

std::shared_ptr<LRhiTexture> LRhiD3d11::createTexture(LRhiTexture::Format format,
                                                      const LSize<int> &pixelSize,
                                                      int depth, int arraySize,
                                                      int sampleCount, LRhiTexture::Flags flags)
{
    return makeInit<LRhiD3d11Texture>(getPtrT<LRhiImplementation>(), format, pixelSize,
                                      depth, arraySize, sampleCount, flags);
}

std::shared_ptr<LRhiSampler> LRhiD3d11::createSampler(LRhiSampler::Filter magFilter,
                                                      LRhiSampler::Filter minFilter,
                                                      LRhiSampler::Filter mipmapMode,
                                                      LRhiSampler::AddressMode u,
                                                      LRhiSampler::AddressMode v,
                                                      LRhiSampler::AddressMode w)
{
    return makeInit<LRhiD3d11Sampler>(getPtrT<LRhiImplementation>(), magFilter, minFilter,
                                      mipmapMode, u, v, w);
}

std::shared_ptr<LRhiTextureRenderTarget> LRhiD3d11::createTextureRenderTarget(
        const LRhiTextureRenderTargetDescription &desc,
        LRhiTextureRenderTarget::Flags flags)
{
    return makeInit<LRhiD3d11TextureRenderTarget>(getPtrT<LRhiImplementation>(), desc, flags);
}

std::shared_ptr<LRhiShadingRateMap> LRhiD3d11::createShadingRateMap()
{
    return makeInit<LRhiD3d11ShadingRateMap>(getPtrT<LRhiImplementation>());
}

std::shared_ptr<LRhiSwapChain> LRhiD3d11::createSwapChain()
{
    return makeInit<LRhiD3d11SwapChain>(getPtrT<LRhiImplementation>());
}

// ---- 帧循环 ----
LRhi::FrameOpResult LRhiD3d11::beginFrame(std::shared_ptr<LRhiSwapChain> swapChain,
                                         LRhi::BeginFrameFlags flags)
{
    (void)flags;
    if (swapChain == nullptr) {
        return LRhi::FrameOpError;
    }
    std::shared_ptr<LRhiD3d11SwapChain> sc = std::dynamic_pointer_cast<LRhiD3d11SwapChain>(swapChain);
    if (sc == nullptr || !sc->createOrResize()) {
        return LRhi::FrameOpError;
    }
    m_inFrame = true;
    return LRhi::FrameOpSuccess;
}

LRhi::FrameOpResult LRhiD3d11::endFrame(std::shared_ptr<LRhiSwapChain> swapChain,
                                       LRhi::EndFrameFlags flags)
{
    if (swapChain == nullptr) {
        return LRhi::FrameOpError;
    }
    std::shared_ptr<LRhiD3d11SwapChain> sc = std::dynamic_pointer_cast<LRhiD3d11SwapChain>(swapChain);
    if (sc == nullptr || sc->m_swapChain == nullptr) {
        return LRhi::FrameOpError;
    }
    m_inFrame = false;
    if (flags & LRhiEndFrameFlag::SkipPresent) {
        return LRhi::FrameOpSuccess;
    }
    const HRESULT hr = sc->m_swapChain->Present(1, 0);
    if (hr == DXGI_STATUS_OCCLUDED) {
        return LRhi::FrameOpSuccess;
    }
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        m_deviceLost = true;
        return LRhi::FrameOpDeviceLost;
    }
    return SUCCEEDED(hr) ? LRhi::FrameOpSuccess : LRhi::FrameOpError;
}

LRhi::FrameOpResult LRhiD3d11::beginOffscreenFrame(std::shared_ptr<LRhiCommandBuffer> *cb,
                                                   LRhi::BeginFrameFlags flags)
{
    (void)flags;
    if (cb == nullptr) {
        return LRhi::FrameOpError;
    }
    *cb = m_cb;
    m_inFrame = true;
    return LRhi::FrameOpSuccess;
}

LRhi::FrameOpResult LRhiD3d11::endOffscreenFrame(LRhi::EndFrameFlags flags)
{
    (void)flags;
    m_inFrame = false;
    return LRhi::FrameOpSuccess;
}

LRhi::FrameOpResult LRhiD3d11::finish()
{
    if (m_context != nullptr) {
        m_context->Flush();
    }
    return LRhi::FrameOpSuccess;
}

// ---- 能力查询 ----
std::list<int> LRhiD3d11::supportedSampleCounts() const
{
    return { 1, 2, 4, 8 };
}

int LRhiD3d11::ubufAlignment() const
{
    return 16; // D3D11 要求常量缓冲按 16 字节对齐
}

bool LRhiD3d11::isYUpInFramebuffer() const
{
    return false; // D3D 视口原点在左上，Y 向下
}

bool LRhiD3d11::isYUpInNDC() const
{
    return true;  // D3D 的 NDC Y 向上
}

bool LRhiD3d11::isClipDepthZeroToOne() const
{
    return true;  // D3D 深度范围 0..1
}

LMatrix4x4 LRhiD3d11::clipSpaceCorrMatrix() const
{
    return LMatrix4x4();
}

bool LRhiD3d11::isTextureFormatSupported(LRhiTexture::Format format, LRhiTexture::Flags flags) const
{
    (void)flags;
    return toDxgiFormat(format) != DXGI_FORMAT_UNKNOWN;
}

bool LRhiD3d11::isFeatureSupported(LRhi::Feature feature) const
{
    switch (feature) {
    case LRhi::MultisampleTexture:
    case LRhi::MultisampleRenderBuffer:
    case LRhi::Instancing:
    case LRhi::NPOTTextureRepeat:
    case LRhi::ElementIndexUint:
    case LRhi::Compute:
    case LRhi::BaseVertex:
    case LRhi::IntAttributes:
    case LRhi::TextureArrays:
    case LRhi::GeometryShader:
    case LRhi::NonFillPolygonMode:
    case LRhi::PerRenderTargetBlending:
    case LRhi::DepthClamp:
    case LRhi::HalfAttributes:
        return true;
    default:
        return false;
    }
}

int LRhiD3d11::resourceLimit(LRhi::ResourceLimit limit) const
{
    switch (limit) {
    case LRhi::TextureSizeMin:          return 1;
    case LRhi::TextureSizeMax:          return D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    case LRhi::MaxColorAttachments:     return D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
    case LRhi::FramesInFlight:          return 1;
    case LRhi::MaxAsyncReadbackFrames:  return 1;
    case LRhi::MaxUniformBufferRange:   return D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16;
    case LRhi::TextureArraySizeMax:     return D3D11_REQ_TEXTURE2D_ARRAY_AXIS_DIMENSION;
    default:                            return 0;
    }
}

LRhiStats LRhiD3d11::statistics()
{
    LRhiStats s;
    return s;
}

const std::shared_ptr<LRhiNativeHandles> LRhiD3d11::nativeHandles()
{
    auto h = std::make_shared<LRhiD3D11NativeHandles>();
    h->dev = m_device.Get();
    h->context = m_context.Get();
    h->featureLevel = int(m_featureLevel);
    return h;
}

const std::shared_ptr<LRhiNativeHandles> LRhiD3d11::nativeHandles(std::shared_ptr<LRhiCommandBuffer> cb)
{
    (void)cb;
    auto h = std::make_shared<LRhiD3D11CommandBufferNativeHandles>();
    h->context = m_context.Get();
    return h;
}

LRhiDriverInfo LRhiD3d11::driverInfo() const
{
    return m_driverInfo;
}

bool LRhiD3d11::makeThreadLocalNativeContextCurrent()
{
    return true;
}

void LRhiD3d11::setQueueSubmitParams(std::shared_ptr<LRhiNativeHandles> params)
{
    (void)params;
}

void LRhiD3d11::releaseCachedResources()
{
}

bool LRhiD3d11::isDeviceLost() const
{
    return m_deviceLost;
}

std::vector<uint8_t> LRhiD3d11::pipelineCacheData()
{
    return {};
}

void LRhiD3d11::setPipelineCacheData(const std::vector<uint8_t> &data)
{
    (void)data;
}

std::list<LSize<int>> LRhiD3d11::supportedShadingRates(int sampleCount) const
{
    (void)sampleCount;
    return {};
}

void LRhiD3d11::setShadingRate(std::shared_ptr<LRhiCommandBuffer> cb, const LSize<int> &coarsePixelSize)
{
    (void)cb;
    (void)coarsePixelSize;
}

// ---- pass / pipeline / 绘制 ----
void LRhiD3d11::resourceUpdate(std::shared_ptr<LRhiCommandBuffer> cb,
                              std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates)
{
    (void)cb;
    if (m_context == nullptr || resourceUpdates == nullptr) {
        return;
    }

    for (const std::shared_ptr<LRhiResourceUpdateBatch::TextureOp> &op : resourceUpdates->textureOps) {
        if (op == nullptr || op->type != LRhiResourceUpdateBatch::TextureOp::Upload) {
            continue;
        }
        auto dst = std::dynamic_pointer_cast<LRhiD3d11Texture>(op->dst);
        if (dst == nullptr || dst->m_texture == nullptr) {
            continue;
        }
        D3D11_TEXTURE2D_DESC texDesc{};
        dst->m_texture->GetDesc(&texDesc);
        for (int layer = 0; layer < int(op->subresDesc.size()); ++layer) {
            for (int level = 0; level < LRhi::MAX_MIP_LEVELS; ++level) {
                for (const LRhiTextureSubresourceUploadDescription &sub : op->subresDesc[layer][level]) {
                    if (sub.m_data.empty()) {
                        continue;
                    }
                    const UINT mipW = (std::max)(1u, texDesc.Width >> level);
                    const UINT mipH = (std::max)(1u, texDesc.Height >> level);
                    const UINT w = UINT(sub.m_sourceSize.m_x > 0 ? sub.m_sourceSize.m_x : mipW);
                    const UINT h = UINT(sub.m_sourceSize.m_y > 0 ? sub.m_sourceSize.m_y : mipH);
                    D3D11_BOX box{};
                    box.left = UINT(sub.m_destinationTopLeft.m_x);
                    box.top = UINT(sub.m_destinationTopLeft.m_y);
                    box.front = 0;
                    box.right = box.left + w;
                    box.bottom = box.top + h;
                    box.back = 1;
                    m_context->UpdateSubresource(
                                dst->m_texture.Get(),
                                D3D11CalcSubresource(UINT(level), UINT(layer), texDesc.MipLevels),
                                &box, sub.m_data.data(), sub.m_dataStride, 0);
                }
            }
        }
    }
}

void LRhiD3d11::beginPass(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiRenderTarget> rt,
                          const LColor &colorClearValue,
                          const LRhiDepthStencilClearValue &depthStencilClearValue,
                          std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates,
                          LRhiCommandBuffer::BeginPassFlags flags)
{
    (void)flags;
    if (m_context == nullptr || rt == nullptr) {
        return;
    }
    if (resourceUpdates != nullptr) {
        resourceUpdate(cb, resourceUpdates);
    }

    LD3D11ComPtr<ID3D11RenderTargetView> rtv;
    LD3D11ComPtr<ID3D11DepthStencilView> dsv;
    const LSize<int> size = rt->pixelSize();

    if (auto scrt = std::dynamic_pointer_cast<LRhiD3d11SwapChainRenderTarget>(rt)) {
        if (auto sc = std::dynamic_pointer_cast<LRhiD3d11SwapChain>(scrt->swapChain())) {
            sc->ensureBackBufferViews();
            rtv = sc->m_rtv;
            dsv = sc->m_dsv;
        }
    } else if (auto trt = std::dynamic_pointer_cast<LRhiD3d11TextureRenderTarget>(rt)) {
        trt->buildViews();
        if (!trt->m_rtvs.empty()) {
            rtv = trt->m_rtvs.front();
        }
        dsv = trt->m_dsv;
    }

    ID3D11RenderTargetView *rtvs[1] = { rtv.Get() };
    m_context->OMSetRenderTargets(rtv != nullptr ? 1 : 0, rtvs, dsv.Get());

    if (rtv != nullptr) {
        const float c[4] = { colorClearValue.m_red / 255.0f,
                             colorClearValue.m_green / 255.0f,
                             colorClearValue.m_blue / 255.0f,
                             colorClearValue.m_alpha / 255.0f };
        m_context->ClearRenderTargetView(rtv.Get(), c);
    }
    if (dsv != nullptr) {
        m_context->ClearDepthStencilView(dsv.Get(),
                                         D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
                                         float(depthStencilClearValue.m_depthClearValue),
                                         UINT8(depthStencilClearValue.m_stencilClearValue));
    }

    D3D11_VIEWPORT vp{};
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    vp.Width = float(std::max(1, size.m_x));
    vp.Height = float(std::max(1, size.m_y));
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &vp);

    if (auto d3dcb = std::dynamic_pointer_cast<LRhiD3d11CommandBuffer>(cb)) {
        d3dcb->m_inPass = true;
        d3dcb->m_currentTarget = rt;
        d3dcb->m_usingSwapChainPass =
                std::dynamic_pointer_cast<LRhiD3d11SwapChainRenderTarget>(rt) != nullptr;
    }
}

void LRhiD3d11::endPass(std::shared_ptr<LRhiCommandBuffer> cb,
                        std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates)
{
    if (resourceUpdates != nullptr) {
        resourceUpdate(cb, resourceUpdates);
    }
    if (m_context != nullptr) {
        ID3D11RenderTargetView *nullRtv[1] = { nullptr };
        m_context->OMSetRenderTargets(1, nullRtv, nullptr);
    }
    if (auto d3dcb = std::dynamic_pointer_cast<LRhiD3d11CommandBuffer>(cb)) {
        d3dcb->m_inPass = false;
        d3dcb->m_currentTarget.reset();
    }
}

void LRhiD3d11::setGraphicsPipeline(std::shared_ptr<LRhiCommandBuffer> cb,
                                    std::shared_ptr<LRhiGraphicsPipeline> ps)
{
    (void)cb;
    std::shared_ptr<LRhiD3d11GraphicsPipeline> p =
            std::dynamic_pointer_cast<LRhiD3d11GraphicsPipeline>(ps);
    if (p == nullptr || m_context == nullptr) {
        return;
    }
    m_context->IASetInputLayout(p->m_inputLayout.Get());
    m_context->IASetPrimitiveTopology(p->m_d3dTopology);
    m_context->VSSetShader(p->m_vs.Get(), nullptr, 0);
    m_context->PSSetShader(p->m_ps.Get(), nullptr, 0);
    m_context->GSSetShader(p->m_gs.Get(), nullptr, 0);
    m_context->RSSetState(p->m_rasterState.Get());
    m_context->OMSetDepthStencilState(p->m_dsState.Get(), p->m_stencilRef);
    const float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_context->OMSetBlendState(p->m_blendState.Get(), blendFactor, 0xffffffff);
    m_currentGraphicsPipeline = p;
}

void LRhiD3d11::setShaderResources(std::shared_ptr<LRhiCommandBuffer> cb,
                                   std::shared_ptr<LRhiShaderResourceBindings> srb,
                                   int dynamicOffsetCount,
                                   const LRhiCommandBuffer::DynamicOffset *dynamicOffsets)
{
    (void)cb;
    (void)dynamicOffsetCount;
    (void)dynamicOffsets;
    auto b = std::dynamic_pointer_cast<LRhiD3d11ShaderResourceBindings>(srb);
    if (b == nullptr || m_context == nullptr) {
        return;
    }
    for (const auto &kv : b->m_constantBuffers) {
        ID3D11Buffer *buf = kv.second.Get();
        m_context->VSSetConstantBuffers(UINT(kv.first), 1, &buf);
        m_context->PSSetConstantBuffers(UINT(kv.first), 1, &buf);
    }
    for (const auto &kv : b->m_srvs) {
        ID3D11ShaderResourceView *srv = kv.second.Get();
        m_context->VSSetShaderResources(UINT(kv.first), 1, &srv);
        m_context->PSSetShaderResources(UINT(kv.first), 1, &srv);
    }
    for (const auto &kv : b->m_samplers) {
        ID3D11SamplerState *s = kv.second.Get();
        m_context->VSSetSamplers(UINT(kv.first), 1, &s);
        m_context->PSSetSamplers(UINT(kv.first), 1, &s);
    }
}

void LRhiD3d11::setVertexInput(std::shared_ptr<LRhiCommandBuffer> cb,
                               int startBinding, int bindingCount,
                               const LRhiCommandBuffer::VertexInput *bindings,
                               std::shared_ptr<LRhiBuffer> indexBuf, uint32_t indexOffset,
                               LRhiCommandBuffer::IndexFormat indexFormat)
{
    (void)cb;
    if (m_context == nullptr) {
        return;
    }
    std::vector<ID3D11Buffer *> buffers;
    std::vector<UINT> strides;
    std::vector<UINT> offsets;
    for (int i = 0; i < bindingCount; ++i) {
        auto b = std::dynamic_pointer_cast<LRhiD3d11Buffer>(bindings[i].first);
        buffers.push_back(b != nullptr ? b->m_buffer.Get() : nullptr);
        uint32_t stride = 0;
        const int slot = startBinding + i;
        if (m_currentGraphicsPipeline != nullptr && slot >= 0
                && slot < int(m_currentGraphicsPipeline->m_vertexInputLayout.m_bindings.size())) {
            stride = m_currentGraphicsPipeline->m_vertexInputLayout.m_bindings[slot].m_stride;
        }
        strides.push_back(stride);
        offsets.push_back(UINT(bindings[i].second));
    }
    if (!buffers.empty()) {
        m_context->IASetVertexBuffers(UINT(startBinding), UINT(buffers.size()),
                                      buffers.data(), strides.data(), offsets.data());
    }
    if (auto ib = std::dynamic_pointer_cast<LRhiD3d11Buffer>(indexBuf)) {
        m_context->IASetIndexBuffer(ib->m_buffer.Get(),
                                    indexFormat == LRhiCommandBuffer::IndexUInt32
                                            ? DXGI_FORMAT_R32_UINT
                                            : DXGI_FORMAT_R16_UINT,
                                    indexOffset);
    }
}

void LRhiD3d11::setViewport(std::shared_ptr<LRhiCommandBuffer> cb, const LRhiViewport &viewport)
{
    (void)cb;
    if (m_context == nullptr) {
        return;
    }
    D3D11_VIEWPORT vp{};
    vp.TopLeftX = float(viewport.m_rect.m_x);
    vp.TopLeftY = float(viewport.m_rect.m_y);
    vp.Width = float(std::max(1.0, viewport.m_rect.m_width));
    vp.Height = float(std::max(1.0, viewport.m_rect.m_height));
    vp.MinDepth = float(viewport.m_minDepth);
    vp.MaxDepth = float(viewport.m_maxDepth);
    m_context->RSSetViewports(1, &vp);
}

void LRhiD3d11::setScissor(std::shared_ptr<LRhiCommandBuffer> cb, const LRhiScissor &scissor)
{
    (void)cb;
    if (m_context == nullptr) {
        return;
    }
    D3D11_RECT r{};
    r.left = LONG(scissor.m_x);
    r.top = LONG(scissor.m_y);
    r.right = LONG(scissor.m_x + std::max(0, scissor.m_width));
    r.bottom = LONG(scissor.m_y + std::max(0, scissor.m_height));
    m_context->RSSetScissorRects(1, &r);
}

void LRhiD3d11::setBlendConstants(std::shared_ptr<LRhiCommandBuffer> cb, const LColor &c)
{
    (void)cb;
    (void)c;
    // D3D11 的混合常量是 OMSetBlendState 参数，需要和 pipeline 一起提交，
    // 这里暂存的话要额外状态，先留空由 pipeline 固定
}

void LRhiD3d11::setStencilRef(std::shared_ptr<LRhiCommandBuffer> cb, uint32_t refValue)
{
    (void)cb;
    (void)refValue;
}

void LRhiD3d11::draw(std::shared_ptr<LRhiCommandBuffer> cb, uint32_t vertexCount,
                     uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    (void)cb;
    if (m_context == nullptr) {
        return;
    }
    if (instanceCount <= 1) {
        m_context->Draw(vertexCount, firstVertex);
    } else {
        m_context->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
    }
}

void LRhiD3d11::drawIndexed(std::shared_ptr<LRhiCommandBuffer> cb, uint32_t indexCount,
                            uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset,
                            uint32_t firstInstance)
{
    (void)cb;
    if (m_context == nullptr) {
        return;
    }
    if (instanceCount <= 1) {
        m_context->DrawIndexed(indexCount, firstIndex, vertexOffset);
    } else {
        m_context->DrawIndexedInstanced(indexCount, instanceCount, firstIndex,
                                        vertexOffset, firstInstance);
    }
}

void LRhiD3d11::debugMarkBegin(std::shared_ptr<LRhiCommandBuffer> cb, const std::string &name)
{
    (void)cb;
    (void)name;
}

void LRhiD3d11::debugMarkEnd(std::shared_ptr<LRhiCommandBuffer> cb)
{
    (void)cb;
}

void LRhiD3d11::debugMarkMsg(std::shared_ptr<LRhiCommandBuffer> cb, const std::string &msg)
{
    (void)cb;
    (void)msg;
}

void LRhiD3d11::beginComputePass(std::shared_ptr<LRhiCommandBuffer> cb,
                                 std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates,
                                 LRhiCommandBuffer::BeginPassFlags flags)
{
    (void)cb;
    (void)resourceUpdates;
    (void)flags;
}

void LRhiD3d11::endComputePass(std::shared_ptr<LRhiCommandBuffer> cb,
                               std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates)
{
    (void)cb;
    (void)resourceUpdates;
}

void LRhiD3d11::setComputePipeline(std::shared_ptr<LRhiCommandBuffer> cb,
                                   std::shared_ptr<LRhiComputePipeline> ps)
{
    (void)cb;
    auto p = std::dynamic_pointer_cast<LRhiD3d11ComputePipeline>(ps);
    if (p != nullptr && m_context != nullptr) {
        m_context->CSSetShader(p->m_cs.Get(), nullptr, 0);
    }
}

void LRhiD3d11::dispatch(std::shared_ptr<LRhiCommandBuffer> cb, int x, int y, int z)
{
    (void)cb;
    if (m_context != nullptr) {
        m_context->Dispatch(UINT(x), UINT(y), UINT(z));
    }
}

void LRhiD3d11::beginExternal(std::shared_ptr<LRhiCommandBuffer> cb)
{
    (void)cb;
}

void LRhiD3d11::endExternal(std::shared_ptr<LRhiCommandBuffer> cb)
{
    (void)cb;
}

double LRhiD3d11::lastCompletedGpuTime(std::shared_ptr<LRhiCommandBuffer> cb)
{
    (void)cb;
    return 0.0;
}

// ---------------------------------------------------------------------------
// 资源类实现
// ---------------------------------------------------------------------------
namespace {

LRhiD3d11 *lImpl(const std::weak_ptr<LRhiImplementation> &w)
{
    std::shared_ptr<LRhiImplementation> p = w.lock();
    return p != nullptr ? dynamic_cast<LRhiD3d11 *>(p.get()) : nullptr;
}

DXGI_FORMAT lSrgbFormat(DXGI_FORMAT f)
{
    switch (f) {
    case DXGI_FORMAT_R8G8B8A8_UNORM:    return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case DXGI_FORMAT_B8G8R8A8_UNORM:    return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case DXGI_FORMAT_BC1_UNORM:         return DXGI_FORMAT_BC1_UNORM_SRGB;
    case DXGI_FORMAT_BC2_UNORM:         return DXGI_FORMAT_BC2_UNORM_SRGB;
    case DXGI_FORMAT_BC3_UNORM:         return DXGI_FORMAT_BC3_UNORM_SRGB;
    case DXGI_FORMAT_BC7_UNORM:         return DXGI_FORMAT_BC7_UNORM_SRGB;
    default:                            return f;
    }
}

DXGI_FORMAT lVertexFormat(LRhiVertexInputAttribute::Format f)
{
    switch (f) {
    case LRhiVertexInputAttribute::Float4:      return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case LRhiVertexInputAttribute::Float3:      return DXGI_FORMAT_R32G32B32_FLOAT;
    case LRhiVertexInputAttribute::Float2:      return DXGI_FORMAT_R32G32_FLOAT;
    case LRhiVertexInputAttribute::Float:       return DXGI_FORMAT_R32_FLOAT;
    case LRhiVertexInputAttribute::UNormByte4:  return DXGI_FORMAT_R8G8B8A8_UNORM;
    case LRhiVertexInputAttribute::UNormByte2:  return DXGI_FORMAT_R8G8_UNORM;
    case LRhiVertexInputAttribute::UNormByte:   return DXGI_FORMAT_R8_UNORM;
    case LRhiVertexInputAttribute::UInt4:       return DXGI_FORMAT_R32G32B32A32_UINT;
    case LRhiVertexInputAttribute::UInt3:       return DXGI_FORMAT_R32G32B32_UINT;
    case LRhiVertexInputAttribute::UInt2:       return DXGI_FORMAT_R32G32_UINT;
    case LRhiVertexInputAttribute::UInt:        return DXGI_FORMAT_R32_UINT;
    case LRhiVertexInputAttribute::SInt4:       return DXGI_FORMAT_R32G32B32A32_SINT;
    case LRhiVertexInputAttribute::SInt3:       return DXGI_FORMAT_R32G32B32_SINT;
    case LRhiVertexInputAttribute::SInt2:       return DXGI_FORMAT_R32G32_SINT;
    case LRhiVertexInputAttribute::SInt:        return DXGI_FORMAT_R32_SINT;
    case LRhiVertexInputAttribute::Half4:       return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case LRhiVertexInputAttribute::Half3:       return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case LRhiVertexInputAttribute::Half2:       return DXGI_FORMAT_R16G16_FLOAT;
    case LRhiVertexInputAttribute::Half:        return DXGI_FORMAT_R16_FLOAT;
    case LRhiVertexInputAttribute::UShort4:     return DXGI_FORMAT_R16G16B16A16_UNORM;
    case LRhiVertexInputAttribute::UShort3:     return DXGI_FORMAT_R16G16B16A16_UNORM;
    case LRhiVertexInputAttribute::UShort2:     return DXGI_FORMAT_R16G16_UNORM;
    case LRhiVertexInputAttribute::UShort:      return DXGI_FORMAT_R16_UNORM;
    case LRhiVertexInputAttribute::SShort4:     return DXGI_FORMAT_R16G16B16A16_SNORM;
    case LRhiVertexInputAttribute::SShort3:     return DXGI_FORMAT_R16G16B16A16_SNORM;
    case LRhiVertexInputAttribute::SShort2:     return DXGI_FORMAT_R16G16_SNORM;
    case LRhiVertexInputAttribute::SShort:      return DXGI_FORMAT_R16_SNORM;
    default:                                    return DXGI_FORMAT_UNKNOWN;
    }
}

D3D11_FILTER lFilter(LRhiSampler::Filter mag, LRhiSampler::Filter min, LRhiSampler::Filter mip)
{
    const bool magLinear = mag == LRhiSampler::Linear;
    const bool minLinear = min == LRhiSampler::Linear;
    const bool mipLinear = mip == LRhiSampler::Linear;
    if (!magLinear && !minLinear && !mipLinear) {
        return D3D11_FILTER_MIN_MAG_MIP_POINT;
    }
    if (magLinear && !minLinear && !mipLinear) {
        return D3D11_FILTER_MIN_MAG_MIP_POINT;
    }
    if (minLinear && mipLinear) {
        return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    }
    return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
}

D3D11_TEXTURE_ADDRESS_MODE lAddress(LRhiSampler::AddressMode m)
{
    switch (m) {
    case LRhiSampler::Repeat:       return D3D11_TEXTURE_ADDRESS_WRAP;
    case LRhiSampler::Mirror:       return D3D11_TEXTURE_ADDRESS_MIRROR;
    case LRhiSampler::ClampToEdge:  return D3D11_TEXTURE_ADDRESS_CLAMP;
    default:                        return D3D11_TEXTURE_ADDRESS_CLAMP;
    }
}

D3D11_PRIMITIVE_TOPOLOGY lTopology(LRhiGraphicsPipeline::Topology t)
{
    switch (t) {
    case LRhiGraphicsPipeline::Triangles:       return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case LRhiGraphicsPipeline::TriangleStrip:   return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    case LRhiGraphicsPipeline::Lines:           return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
    case LRhiGraphicsPipeline::LineStrip:       return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
    case LRhiGraphicsPipeline::Points:          return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
    default:                                    return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}

D3D11_COMPARISON_FUNC lCompare(LRhiGraphicsPipeline::CompareOp op)
{
    switch (op) {
    case LRhiGraphicsPipeline::Never:           return D3D11_COMPARISON_NEVER;
    case LRhiGraphicsPipeline::Less:            return D3D11_COMPARISON_LESS;
    case LRhiGraphicsPipeline::Equal:           return D3D11_COMPARISON_EQUAL;
    case LRhiGraphicsPipeline::LessOrEqual:     return D3D11_COMPARISON_LESS_EQUAL;
    case LRhiGraphicsPipeline::Greater:         return D3D11_COMPARISON_GREATER;
    case LRhiGraphicsPipeline::NotEqual:        return D3D11_COMPARISON_NOT_EQUAL;
    case LRhiGraphicsPipeline::GreaterOrEqual:  return D3D11_COMPARISON_GREATER_EQUAL;
    default:                                    return D3D11_COMPARISON_ALWAYS;
    }
}

/** 从 LShader 里取一份可用的字节码（优先标准变体） */
bool lPickShaderCode(const LShader &shader, std::vector<uint8_t> *out)
{
    if (shader.m_shaders.empty()) {
        return false;
    }
    LShaderKey key{};
    key.m_variant = LShader::StandardShader;
    auto exact = shader.m_shaders.find(key);
    const auto &picked = exact != shader.m_shaders.end() ? exact : shader.m_shaders.begin();
    if (picked->second == nullptr || picked->second->m_shader.empty()) {
        return false;
    }
    if (out != nullptr) {
        *out = picked->second->m_shader;
    }
    return true;
}

} // namespace

// ---- Buffer ----
LRhiD3d11Buffer::LRhiD3d11Buffer(std::shared_ptr<LRhiImplementation> rhi,
                                 Type type, UsageFlags usage, uint32_t size)
    : LRhiBuffer(rhi, type, usage, size)
{
}

LRhiD3d11Buffer::~LRhiD3d11Buffer()
{
    destroyBuffer();
}

void LRhiD3d11Buffer::destroyBuffer()
{
    LRhiD3d11 *impl = lImpl(m_rhi);
    if (m_mapped != nullptr && m_buffer != nullptr && impl != nullptr && impl->context() != nullptr) {
        impl->context()->Unmap(m_buffer.Get(), 0);
        m_mapped = nullptr;
    }
    m_buffer.Reset();
}

bool LRhiD3d11Buffer::create()
{
    LRhiD3d11 *impl = lImpl(m_rhi);
    if (impl == nullptr || impl->device() == nullptr || m_size == 0) {
        return false;
    }
    destroyBuffer();

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = m_size;
    if (m_usage & LRhiBufferUsage::Vertex) {
        desc.BindFlags |= D3D11_BIND_VERTEX_BUFFER;
    }
    if (m_usage & LRhiBufferUsage::Index) {
        desc.BindFlags |= D3D11_BIND_INDEX_BUFFER;
    }
    if (m_usage & LRhiBufferUsage::Uniform) {
        desc.BindFlags |= D3D11_BIND_CONSTANT_BUFFER;
    }
    if (m_usage & LRhiBufferUsage::Storage) {
        desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    }
    if (desc.BindFlags == 0) {
        desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    }

    if (m_type == Dynamic) {
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    } else {
        desc.Usage = D3D11_USAGE_DEFAULT;
    }

    const HRESULT hr = impl->device()->CreateBuffer(&desc, nullptr, m_buffer.GetAddressOf());
    if (FAILED(hr)) {
        lWarn() << "rhi/d3d11: CreateBuffer failed, size =" << int(m_size)
                << "hr =" << int(hr);
        return false;
    }
    return true;
}

LRhiBuffer::NativeBuffer LRhiD3d11Buffer::nativeBuffer()
{
    NativeBuffer nb{};
    nb.objects[0] = m_buffer.Get();
    nb.objects[1] = nullptr;
    nb.objects[2] = nullptr;
    nb.slotCount = m_buffer != nullptr ? 1 : 0;
    return nb;
}

char *LRhiD3d11Buffer::beginFullDynamicBufferUpdateForCurrentFrame()
{
    LRhiD3d11 *impl = lImpl(m_rhi);
    if (impl == nullptr || impl->context() == nullptr || m_buffer == nullptr) {
        return nullptr;
    }
    D3D11_MAPPED_SUBRESOURCE mapped{};
    const HRESULT hr = impl->context()->Map(m_buffer.Get(), 0,
                                            D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) {
        return nullptr;
    }
    m_mapped = static_cast<char *>(mapped.pData);
    m_mappedThisFrame = true;
    return m_mapped;
}

void LRhiD3d11Buffer::endFullDynamicBufferUpdateForCurrentFrame()
{
    LRhiD3d11 *impl = lImpl(m_rhi);
    if (m_mapped != nullptr && m_buffer != nullptr && impl != nullptr && impl->context() != nullptr) {
        impl->context()->Unmap(m_buffer.Get(), 0);
    }
    m_mapped = nullptr;
    m_mappedThisFrame = false;
}

void LRhiD3d11Buffer::fullDynamicBufferUpdateForCurrentFrame(const void *data, uint32_t size)
{
    if (data == nullptr) {
        return;
    }
    char *dst = beginFullDynamicBufferUpdateForCurrentFrame();
    if (dst == nullptr) {
        return;
    }
    const uint32_t n = size != 0 ? std::min<uint32_t>(size, m_size) : m_size;
    std::memcpy(dst, data, n);
    endFullDynamicBufferUpdateForCurrentFrame();
}

// ---- Texture ----
LRhiD3d11Texture::LRhiD3d11Texture(std::shared_ptr<LRhiImplementation> rhi, Format format,
                                   const LSize<int> &pixelSize, int depth, int arraySize,
                                   int sampleCount, Flags flags)
    : LRhiTexture(rhi, format, pixelSize, depth, arraySize, sampleCount, flags)
{
}

LRhiD3d11Texture::~LRhiD3d11Texture()
{
    destroyTexture();
}

void LRhiD3d11Texture::destroyTexture()
{
    m_srv.Reset();
    m_rtv.Reset();
    m_uav.Reset();
    m_texture.Reset();
}

bool LRhiD3d11Texture::create()
{
    LRhiD3d11 *impl = lImpl(m_rhi);
    if (impl == nullptr || impl->device() == nullptr) {
        return false;
    }
    destroyTexture();

    DXGI_FORMAT format = LRhiD3d11::toDxgiFormat(m_format);
    if (format == DXGI_FORMAT_UNKNOWN) {
        lWarn() << "rhi/d3d11: unsupported texture format";
        return false;
    }
    if (m_flags & LRhiTextureFlag::sRGB) {
        format = lSrgbFormat(format);
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = UINT(std::max(1, m_pixelSize.m_x));
    desc.Height = UINT(std::max(1, m_pixelSize.m_y));
    desc.MipLevels = (m_flags & LRhiTextureFlag::MipMapped) ? 0 : 1;
    desc.ArraySize = UINT(std::max(1, m_arraySize));
    desc.Format = format;
    desc.SampleDesc.Count = UINT(std::max(1, m_sampleCount));
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (m_flags & LRhiTextureFlag::RenderTarget) {
        desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    }
    if (m_flags & LRhiTextureFlag::UsedWithLoadStore) {
        desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    }
    if (m_flags & LRhiTextureFlag::MipMapped) {
        desc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
    }

    HRESULT hr = impl->device()->CreateTexture2D(&desc, nullptr, m_texture.GetAddressOf());
    if (FAILED(hr)) {
        lWarn() << "rhi/d3d11: CreateTexture2D failed, hr =" << int(hr);
        return false;
    }

    const bool isDepth = m_format == D16 || m_format == D24 || m_format == D24S8
            || m_format == D32F || m_format == D32FS8;
    if (!isDepth) {
        hr = impl->device()->CreateShaderResourceView(m_texture.Get(), nullptr,
                                                      m_srv.GetAddressOf());
        if (FAILED(hr)) {
            lWarn() << "rhi/d3d11: CreateShaderResourceView failed";
        }
    }
    if (m_flags & LRhiTextureFlag::RenderTarget) {
        hr = impl->device()->CreateRenderTargetView(m_texture.Get(), nullptr,
                                                    m_rtv.GetAddressOf());
        if (FAILED(hr)) {
            lWarn() << "rhi/d3d11: CreateRenderTargetView failed";
        }
    }
    return true;
}

LRhiTexture::NativeTexture LRhiD3d11Texture::nativeTexture()
{
    NativeTexture nt{};
    nt.object = reinterpret_cast<uint64_t>(m_texture.Get());
    nt.layout = 0;
    return nt;
}

bool LRhiD3d11Texture::createFrom(NativeTexture src)
{
    if (src.object == 0) {
        return false;
    }
    destroyTexture();
    m_texture = reinterpret_cast<ID3D11Texture2D *>(static_cast<uintptr_t>(src.object));
    if (m_texture == nullptr) {
        return false;
    }
    m_imported = true;

    D3D11_TEXTURE2D_DESC desc{};
    m_texture->GetDesc(&desc);
    m_pixelSize = LSize<int>(int(desc.Width), int(desc.Height));
    m_arraySize = int(desc.ArraySize);
    m_sampleCount = int(desc.SampleDesc.Count);

    LRhiD3d11 *impl = lImpl(m_rhi);
    if (impl != nullptr && impl->device() != nullptr) {
        const bool isDepth = m_format == D16 || m_format == D24 || m_format == D24S8
                || m_format == D32F || m_format == D32FS8;
        if (!isDepth) {
            impl->device()->CreateShaderResourceView(m_texture.Get(), nullptr,
                                                     m_srv.GetAddressOf());
        }
    }
    return true;
}

// ---- Sampler ----
LRhiD3d11Sampler::LRhiD3d11Sampler(std::shared_ptr<LRhiImplementation> rhi,
                                   Filter magFilter, Filter minFilter, Filter mipmapMode,
                                   AddressMode u, AddressMode v, AddressMode w)
    : LRhiSampler(rhi, magFilter, minFilter, mipmapMode, u, v, w)
{
}

LRhiD3d11Sampler::~LRhiD3d11Sampler()
{
    destroySampler();
}

void LRhiD3d11Sampler::destroySampler()
{
    m_sampler.Reset();
}

bool LRhiD3d11Sampler::create()
{
    LRhiD3d11 *impl = lImpl(m_rhi);
    if (impl == nullptr || impl->device() == nullptr) {
        return false;
    }
    destroySampler();

    D3D11_SAMPLER_DESC desc{};
    desc.Filter = lFilter(m_magFilter, m_minFilter, m_mipmapMode);
    desc.AddressU = lAddress(m_addressU);
    desc.AddressV = lAddress(m_addressV);
    desc.AddressW = lAddress(m_addressW);
    desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    desc.MaxLOD = D3D11_FLOAT32_MAX;

    const HRESULT hr = impl->device()->CreateSamplerState(&desc, m_sampler.GetAddressOf());
    if (FAILED(hr)) {
        lWarn() << "rhi/d3d11: CreateSamplerState failed";
        return false;
    }
    return true;
}

// ---- RenderBuffer ----
LRhiD3d11RenderBuffer::LRhiD3d11RenderBuffer(std::shared_ptr<LRhiImplementation> rhi, Type type,
                                             const LSize<int> &pixelSize, int sampleCount,
                                             bool usedWithSwapChainOnly,
                                             LRhiTexture::Format backingFormatHint)
    : LRhiRenderBuffer(rhi, type, pixelSize, sampleCount, usedWithSwapChainOnly, backingFormatHint)
{
}

LRhiD3d11RenderBuffer::~LRhiD3d11RenderBuffer()
{
    destroyRenderBuffer();
}

void LRhiD3d11RenderBuffer::destroyRenderBuffer()
{
    m_rtv.Reset();
    m_dsv.Reset();
    m_texture.Reset();
}

LRhiTexture::Format LRhiD3d11RenderBuffer::backingFormat() const
{
    if (m_type == DepthStencil) {
        return LRhiTexture::D24S8;
    }
    return LRhiTexture::BGRA8;
}

bool LRhiD3d11RenderBuffer::create()
{
    LRhiD3d11 *impl = lImpl(m_rhi);
    if (impl == nullptr || impl->device() == nullptr) {
        return false;
    }
    destroyRenderBuffer();

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = UINT(std::max(1, m_pixelSize.m_x));
    desc.Height = UINT(std::max(1, m_pixelSize.m_y));
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.SampleDesc.Count = UINT(std::max(1, m_sampleCount));
    desc.Usage = D3D11_USAGE_DEFAULT;

    if (m_type == DepthStencil) {
        desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    } else {
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
    }

    HRESULT hr = impl->device()->CreateTexture2D(&desc, nullptr, m_texture.GetAddressOf());
    if (FAILED(hr)) {
        lWarn() << "rhi/d3d11: render buffer texture creation failed";
        return false;
    }
    if (m_type == DepthStencil) {
        hr = impl->device()->CreateDepthStencilView(m_texture.Get(), nullptr,
                                                    m_dsv.GetAddressOf());
    } else {
        hr = impl->device()->CreateRenderTargetView(m_texture.Get(), nullptr,
                                                   m_rtv.GetAddressOf());
    }
    return SUCCEEDED(hr);
}

// ---- RenderPassDescriptor ----
LRhiD3d11RenderPassDescriptor::LRhiD3d11RenderPassDescriptor(std::shared_ptr<LRhiImplementation> rhi)
    : LRhiRenderPassDescriptor(rhi)
{
}

bool LRhiD3d11RenderPassDescriptor::isCompatible(
        const std::shared_ptr<LRhiRenderPassDescriptor> other) const
{
    auto o = std::dynamic_pointer_cast<LRhiD3d11RenderPassDescriptor>(other);
    if (o == nullptr) {
        return false;
    }
    return m_colorFormats == o->m_colorFormats
            && m_depthFormat == o->m_depthFormat
            && m_sampleCount == o->m_sampleCount;
}

std::shared_ptr<LRhiRenderPassDescriptor>
LRhiD3d11RenderPassDescriptor::newCompatibleRenderPassDescriptor() const
{
    auto p = makeInit<LRhiD3d11RenderPassDescriptor>(m_rhi.lock());
    p->m_colorFormats = m_colorFormats;
    p->m_depthFormat = m_depthFormat;
    p->m_sampleCount = m_sampleCount;
    return p;
}

std::vector<uint32_t> LRhiD3d11RenderPassDescriptor::serializedFormat() const
{
    std::vector<uint32_t> out;
    out.reserve(m_colorFormats.size() + 1);
    for (LRhiTexture::Format f : m_colorFormats) {
        out.push_back(uint32_t(f));
    }
    out.push_back(uint32_t(m_depthFormat));
    out.push_back(uint32_t(m_sampleCount));
    return out;
}

// ---- TextureRenderTarget ----
LRhiD3d11TextureRenderTarget::LRhiD3d11TextureRenderTarget(
        std::shared_ptr<LRhiImplementation> rhi,
        const LRhiTextureRenderTargetDescription &desc,
        Flags flags)
    : LRhiTextureRenderTarget(rhi, desc, flags)
{
}

bool LRhiD3d11TextureRenderTarget::buildViews()
{
    LRhiD3d11 *impl = lImpl(m_rhi);
    if (impl == nullptr || impl->device() == nullptr) {
        return false;
    }
    m_rtvs.clear();
    m_dsv.Reset();

    for (const LRhiColorAttachment &att : m_desc.m_colorAttachments) {
        if (att.m_texture != nullptr) {
            auto tex = std::dynamic_pointer_cast<LRhiD3d11Texture>(att.m_texture);
            if (tex == nullptr || tex->m_texture == nullptr) {
                continue;
            }
            LD3D11ComPtr<ID3D11RenderTargetView> rtv;
            impl->device()->CreateRenderTargetView(tex->m_texture.Get(), nullptr, rtv.GetAddressOf());
            m_rtvs.push_back(rtv);
            m_pixelSize = tex->m_pixelSize;
        } else if (att.m_renderBuffer != nullptr) {
            auto rb = std::dynamic_pointer_cast<LRhiD3d11RenderBuffer>(att.m_renderBuffer);
            if (rb != nullptr && rb->m_rtv != nullptr) {
                m_rtvs.push_back(rb->m_rtv);
                m_pixelSize = rb->m_pixelSize;
            }
        }
    }

    if (m_desc.m_depthTexture != nullptr) {
        auto tex = std::dynamic_pointer_cast<LRhiD3d11Texture>(m_desc.m_depthTexture);
        if (tex != nullptr && tex->m_texture != nullptr) {
            impl->device()->CreateDepthStencilView(tex->m_texture.Get(), nullptr,
                                                   m_dsv.GetAddressOf());
        }
    } else if (m_desc.m_depthStencilBuffer != nullptr) {
        auto rb = std::dynamic_pointer_cast<LRhiD3d11RenderBuffer>(m_desc.m_depthStencilBuffer);
        if (rb != nullptr) {
            m_dsv = rb->m_dsv;
        }
    }
    return !m_rtvs.empty();
}

bool LRhiD3d11TextureRenderTarget::create()
{
    return buildViews();
}

std::shared_ptr<LRhiRenderPassDescriptor>
LRhiD3d11TextureRenderTarget::newCompatibleRenderPassDescriptor()
{
    auto p = makeInit<LRhiD3d11RenderPassDescriptor>(m_rhi.lock());
    for (const LRhiColorAttachment &att : m_desc.m_colorAttachments) {
        if (att.m_texture != nullptr) {
            p->m_colorFormats.push_back(att.m_texture->m_format);
        } else if (att.m_renderBuffer != nullptr) {
            p->m_colorFormats.push_back(att.m_renderBuffer->backingFormat());
        }
    }
    if (m_desc.m_depthTexture != nullptr) {
        p->m_depthFormat = m_desc.m_depthTexture->m_format;
    } else if (m_desc.m_depthStencilBuffer != nullptr) {
        p->m_depthFormat = m_desc.m_depthStencilBuffer->backingFormat();
    }
    p->m_sampleCount = 1;
    return p;
}

void LRhiD3d11TextureRenderTarget::destroyRenderTarget()
{
    m_rtvs.clear();
    m_dsv.Reset();
}

// ---- SwapChain ----
LRhiD3d11SwapChainRenderTarget::LRhiD3d11SwapChainRenderTarget(
        std::shared_ptr<LRhiImplementation> rhi,
        std::shared_ptr<LRhiSwapChain> swapchain)
    : LRhiSwapChainRenderTarget(rhi, std::move(swapchain))
{
}

LSize<int> LRhiD3d11SwapChainRenderTarget::pixelSize() const
{
    auto sc = std::dynamic_pointer_cast<LRhiD3d11SwapChain>(m_swapchain);
    return sc != nullptr ? sc->m_pixelSize : LSize<int>(1, 1);
}

int LRhiD3d11SwapChainRenderTarget::sampleCount() const
{
    return 1;
}

LRhiD3d11SwapChain::LRhiD3d11SwapChain(std::shared_ptr<LRhiImplementation> rhi)
    : LRhiSwapChain(rhi)
{
    m_passDesc = makeInit<LRhiD3d11RenderPassDescriptor>(m_rhi.lock());
    if (m_passDesc != nullptr) {
        m_passDesc->m_colorFormats = { LRhiTexture::BGRA8 };
        m_passDesc->m_depthFormat = LRhiTexture::D24S8;
        m_passDesc->m_sampleCount = 1;
        m_renderPassDesc = m_passDesc;
    }
}

LRhiD3d11SwapChain::~LRhiD3d11SwapChain()
{
    destroySwapChain();
}

std::shared_ptr<LRhiCommandBuffer> LRhiD3d11SwapChain::currentFrameCommandBuffer()
{
    return m_cb;
}

std::shared_ptr<LRhiRenderTarget> LRhiD3d11SwapChain::currentFrameRenderTarget()
{
    return m_rt;
}

LSize<int> LRhiD3d11SwapChain::surfacePixelSize()
{
    if (m_window != nullptr) {
        const WId id = m_window->winId();
        if (id != 0) {
            RECT rc{};
            if (GetClientRect(reinterpret_cast<HWND>(static_cast<uintptr_t>(id)), &rc)) {
                return LSize<int>(int(std::max<LONG>(1, rc.right - rc.left)),
                                  int(std::max<LONG>(1, rc.bottom - rc.top)));
            }
        }
    }
    return LSize<int>(1, 1);
}

bool LRhiD3d11SwapChain::isFormatSupported(Format f)
{
    return f == SDR;
}

std::shared_ptr<LRhiRenderPassDescriptor>
LRhiD3d11SwapChain::newCompatibleRenderPassDescriptor()
{
    return m_passDesc != nullptr ? m_passDesc->newCompatibleRenderPassDescriptor() : nullptr;
}

bool LRhiD3d11SwapChain::createSwapChain()
{
    LRhiD3d11 *impl = lImpl(m_rhi);
    if (impl == nullptr || impl->factory() == nullptr || m_window == nullptr) {
        lWarn() << "rhi/d3d11: swapchain requires a window and DXGI factory";
        return false;
    }
    const WId id = m_window->winId();
    if (id == 0) {
        lWarn() << "rhi/d3d11: window has no native handle yet";
        return false;
    }
    HWND hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(id));

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = UINT(std::max(1, m_pixelSize.m_x));
    desc.Height = UINT(std::max(1, m_pixelSize.m_y));
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    // flip model（FLIP_DISCARD / FLIP_SEQUENTIAL）不支持 WS_CHILD 子窗口：
    // 视频窗口是嵌入在容器里的子窗口，此时 Present 会静默成功但内容不显示。
    // 所以这里使用 blt model，保证子窗口也能正常呈现。
    desc.BufferCount = 1;
    desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    desc.Scaling = DXGI_SCALING_STRETCH;

    LD3D11ComPtr<IDXGISwapChain1> sc;
    const HRESULT hr = impl->factory()->CreateSwapChainForHwnd(
                impl->device(), hwnd, &desc, nullptr, nullptr, sc.GetAddressOf());
    if (FAILED(hr)) {
        lWarn() << "rhi/d3d11: CreateSwapChainForHwnd failed, hr =" << int(hr);
        return false;
    }
    m_swapChain = sc;

    if (m_depthStencil == nullptr) {
        auto depth = impl->createRenderBuffer(LRhiRenderBuffer::DepthStencil, m_pixelSize,
                                              1, true, LRhiTexture::D24S8);
        m_depthStencil = depth;
    } else {
        m_depthStencil->m_pixelSize = m_pixelSize;
    }
    m_depthStencil->create();

    m_cb = makeInit<LRhiD3d11CommandBuffer>(m_rhi.lock());
    if (m_cb != nullptr) {
        m_cb->m_context = impl->context();
    }
    if (m_rt == nullptr) {
        m_rt = makeInit<LRhiD3d11SwapChainRenderTarget>(m_rhi.lock(), getPtrT<LRhiSwapChain>());
    }

    m_valid = ensureBackBufferViews();
    return m_valid;
}

void LRhiD3d11SwapChain::destroySwapChain()
{
    m_rtv.Reset();
    m_dsv.Reset();
    m_backBuffer.Reset();
    m_depthTexture.Reset();
    m_swapChain.Reset();
    m_rt.reset();
    m_cb.reset();
    m_valid = false;
}

bool LRhiD3d11SwapChain::ensureBackBufferViews()
{
    LRhiD3d11 *impl = lImpl(m_rhi);
    if (impl == nullptr || impl->device() == nullptr || m_swapChain == nullptr) {
        return false;
    }
    m_backBuffer.Reset();
    m_rtv.Reset();
    if (FAILED(m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                      reinterpret_cast<void **>(m_backBuffer.GetAddressOf())))) {
        return false;
    }
    if (FAILED(impl->device()->CreateRenderTargetView(m_backBuffer.Get(), nullptr,
                                                      m_rtv.GetAddressOf()))) {
        return false;
    }
    if (m_depthStencil != nullptr) {
        auto rb = std::dynamic_pointer_cast<LRhiD3d11RenderBuffer>(m_depthStencil);
        if (rb != nullptr) {
            m_dsv = rb->m_dsv;
        }
    }
    m_valid = true;
    return true;
}

bool LRhiD3d11SwapChain::createOrResize()
{
    const LSize<int> size = surfacePixelSize();
    // currentPixelSize() 读的是基类 m_currentPixelSize；D3D11 侧必须自己同步，
    // 否则它会一直是默认的 0x0，使用方按它设置 scissor 时会把整帧裁掉。
    m_currentPixelSize = size;
    if (m_swapChain == nullptr) {
        m_pixelSize = size;
        return createSwapChain();
    }
    if (size.m_x == m_pixelSize.m_x && size.m_y == m_pixelSize.m_y) {
        return m_valid || ensureBackBufferViews();
    }

    m_backBuffer.Reset();
    m_rtv.Reset();
    m_dsv.Reset();
    m_pixelSize = size;

    const HRESULT hr = m_swapChain->ResizeBuffers(0, UINT(size.m_x), UINT(size.m_y),
                                                  DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        lWarn() << "rhi/d3d11: ResizeBuffers failed, hr =" << int(hr);
        return false;
    }
    if (m_depthStencil != nullptr) {
        m_depthStencil->m_pixelSize = size;
        m_depthStencil->create();
    }
    return ensureBackBufferViews();
}

// ---- CommandBuffer ----
LRhiD3d11CommandBuffer::LRhiD3d11CommandBuffer(std::shared_ptr<LRhiImplementation> rhi)
    : LRhiCommandBuffer(rhi)
{
}

void LRhiD3d11CommandBuffer::resetRecording()
{
    m_inPass = false;
    m_currentTarget.reset();
    m_usingSwapChainPass = false;
}

// ---- ShaderResourceBindings ----
LRhiD3d11ShaderResourceBindings::LRhiD3d11ShaderResourceBindings(
        std::shared_ptr<LRhiImplementation> rhi)
    : LRhiShaderResourceBindings(rhi)
{
}

LRhiD3d11ShaderResourceBindings::~LRhiD3d11ShaderResourceBindings()
{
    destroyBindings();
}

void LRhiD3d11ShaderResourceBindings::destroyBindings()
{
    m_constantBuffers.clear();
    m_srvs.clear();
    m_samplers.clear();
}

bool LRhiD3d11ShaderResourceBindings::create()
{
    LRhiD3d11 *impl = lImpl(m_rhi);
    if (impl == nullptr) {
        return false;
    }
    destroyBindings();

    const std::vector<BindingInfo> infos = bindingInfos();
    for (const BindingInfo &b : infos) {
        switch (b.type) {
        case LRhiShaderResourceBinding::UniformBuffer: {
            auto buf = std::dynamic_pointer_cast<LRhiD3d11Buffer>(b.buffer);
            if (buf != nullptr && buf->m_buffer != nullptr) {
                m_constantBuffers[b.binding] = buf->m_buffer;
            }
            break;
        }
        case LRhiShaderResourceBinding::SampledTexture:
        case LRhiShaderResourceBinding::Texture: {
            for (size_t i = 0; i < b.textureSamplers.size(); ++i) {
                auto tex = std::dynamic_pointer_cast<LRhiD3d11Texture>(b.textureSamplers[i].tex);
                if (tex != nullptr && tex->m_srv != nullptr) {
                    m_srvs[b.binding + int(i)] = tex->m_srv;
                }
                auto sampler = std::dynamic_pointer_cast<LRhiD3d11Sampler>(b.textureSamplers[i].sampler);
                if (sampler != nullptr && sampler->m_sampler != nullptr) {
                    m_samplers[b.binding + int(i)] = sampler->m_sampler;
                }
            }
            break;
        }
        case LRhiShaderResourceBinding::Sampler: {
            auto sampler = std::dynamic_pointer_cast<LRhiD3d11Sampler>(b.sampler);
            if (sampler != nullptr && sampler->m_sampler != nullptr) {
                m_samplers[b.binding] = sampler->m_sampler;
            }
            break;
        }
        default:
            break;
        }
    }
    return true;
}

void LRhiD3d11ShaderResourceBindings::updateResources(bool bindingsAreSorted)
{
    (void)bindingsAreSorted;
    create();
}

// ---- GraphicsPipeline ----
LRhiD3d11GraphicsPipeline::LRhiD3d11GraphicsPipeline(std::shared_ptr<LRhiImplementation> rhi)
    : LRhiGraphicsPipeline(rhi)
{
}

LRhiD3d11GraphicsPipeline::~LRhiD3d11GraphicsPipeline()
{
    destroyPipeline();
}

void LRhiD3d11GraphicsPipeline::destroyPipeline()
{
    m_inputLayout.Reset();
    m_vs.Reset();
    m_ps.Reset();
    m_gs.Reset();
    m_rasterState.Reset();
    m_dsState.Reset();
    m_blendState.Reset();
}

bool LRhiD3d11GraphicsPipeline::create()
{
    LRhiD3d11 *impl = lImpl(m_rhi);
    if (impl == nullptr || impl->device() == nullptr) {
        return false;
    }
    destroyPipeline();

    std::vector<uint8_t> vsCode;
    std::vector<uint8_t> psCode;
    std::vector<uint8_t> gsCode;
    for (const LRhiShaderStage &s : m_shaderStages) {
        std::vector<uint8_t> code;
        if (!lPickShaderCode(s.m_shader, &code)) {
            continue;
        }
        switch (s.m_type) {
        case LRhiShaderStage::Vertex:
            vsCode = std::move(code);
            break;
        case LRhiShaderStage::Fragment:
            psCode = std::move(code);
            break;
        case LRhiShaderStage::Geometry:
            gsCode = std::move(code);
            break;
        default:
            break;
        }
    }
    if (vsCode.empty()) {
        lWarn() << "rhi/d3d11: graphics pipeline needs a vertex shader";
        return false;
    }

    ID3D11Device *dev = impl->device();
    if (FAILED(dev->CreateVertexShader(vsCode.data(), vsCode.size(), nullptr,
                                       m_vs.GetAddressOf()))) {
        lWarn() << "rhi/d3d11: CreateVertexShader failed";
        return false;
    }
    if (!psCode.empty()
            && FAILED(dev->CreatePixelShader(psCode.data(), psCode.size(), nullptr,
                                             m_ps.GetAddressOf()))) {
        lWarn() << "rhi/d3d11: CreatePixelShader failed";
    }
    if (!gsCode.empty()) {
        dev->CreateGeometryShader(gsCode.data(), gsCode.size(), nullptr, m_gs.GetAddressOf());
    }

    if (!m_vertexInputLayout.m_attributes.empty()) {
        std::vector<D3D11_INPUT_ELEMENT_DESC> elems;
        std::vector<std::string> names;
        elems.reserve(m_vertexInputLayout.m_attributes.size());
        names.reserve(m_vertexInputLayout.m_attributes.size());
        for (const LRhiVertexInputAttribute &a : m_vertexInputLayout.m_attributes) {
            names.push_back(a.m_name.empty()
                            ? ("TEXCOORD" + std::to_string(a.m_location))
                            : a.m_name);
            D3D11_INPUT_ELEMENT_DESC e{};
            e.SemanticName = names.back().c_str();
            e.SemanticIndex = 0;
            e.Format = lVertexFormat(a.m_format);
            e.InputSlot = UINT(a.m_binding);
            e.AlignedByteOffset = a.m_offset;
            e.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
            e.InstanceDataStepRate = 0;
            elems.push_back(e);
        }
        const HRESULT hr = dev->CreateInputLayout(elems.data(), UINT(elems.size()),
                                                  vsCode.data(), vsCode.size(),
                                                  m_inputLayout.GetAddressOf());
        if (FAILED(hr)) {
            lWarn() << "rhi/d3d11: CreateInputLayout failed, hr =" << int(hr);
        }
    }

    m_d3dTopology = lTopology(m_topology);

    D3D11_RASTERIZER_DESC rs{};
    rs.FillMode = m_polygonMode == LRhiGraphicsPipeline::Line ? D3D11_FILL_WIREFRAME
                                                              : D3D11_FILL_SOLID;
    rs.CullMode = m_cullMode == LRhiGraphicsPipeline::None ? D3D11_CULL_NONE
                  : (m_cullMode == LRhiGraphicsPipeline::Front ? D3D11_CULL_FRONT
                                                               : D3D11_CULL_BACK);
    rs.FrontCounterClockwise = m_frontFace == LRhiGraphicsPipeline::CCW;
    rs.DepthClipEnable = m_depthClamp;
    rs.ScissorEnable = (m_flags & LRhiGraphicsPipelineFlag::UsesScissor) ? TRUE : FALSE;
    rs.MultisampleEnable = m_sampleCount > 1;
    rs.DepthBias = m_depthBias;
    rs.SlopeScaledDepthBias = m_slopeScaledDepthBias;
    dev->CreateRasterizerState(&rs, m_rasterState.GetAddressOf());

    D3D11_DEPTH_STENCIL_DESC ds{};
    ds.DepthEnable = m_depthTest;
    ds.DepthWriteMask = m_depthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    ds.DepthFunc = lCompare(m_depthOp);
    ds.StencilEnable = m_stencilTest;
    ds.StencilReadMask = UINT8(m_stencilReadMask);
    ds.StencilWriteMask = UINT8(m_stencilWriteMask);
    ds.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    ds.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    ds.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    ds.FrontFace.StencilFunc = lCompare(m_stencilFront.compareOp);
    ds.BackFace = ds.FrontFace;
    dev->CreateDepthStencilState(&ds, m_dsState.GetAddressOf());

    D3D11_BLEND_DESC bd{};
    bd.AlphaToCoverageEnable = FALSE;
    bd.IndependentBlendEnable = FALSE;
    const LRhiGraphicsPipeline::TargetBlend tb =
            m_targetBlends.empty() ? LRhiGraphicsPipeline::TargetBlend()
                                   : m_targetBlends.front();
    bd.RenderTarget[0].BlendEnable = tb.enable ? TRUE : FALSE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    dev->CreateBlendState(&bd, m_blendState.GetAddressOf());
    return true;
}

// ---- ComputePipeline ----
LRhiD3d11ComputePipeline::LRhiD3d11ComputePipeline(std::shared_ptr<LRhiImplementation> rhi)
    : LRhiComputePipeline(rhi)
{
}

LRhiD3d11ComputePipeline::~LRhiD3d11ComputePipeline()
{
    destroyPipeline();
}

void LRhiD3d11ComputePipeline::destroyPipeline()
{
    m_cs.Reset();
}

bool LRhiD3d11ComputePipeline::create()
{
    LRhiD3d11 *impl = lImpl(m_rhi);
    if (impl == nullptr || impl->device() == nullptr) {
        return false;
    }
    destroyPipeline();

    std::vector<uint8_t> code;
    if (!lPickShaderCode(m_shaderStage.m_shader, &code)) {
        lWarn() << "rhi/d3d11: compute pipeline has no shader";
        return false;
    }
    const HRESULT hr = impl->device()->CreateComputeShader(code.data(), code.size(), nullptr,
                                                           m_cs.GetAddressOf());
    return SUCCEEDED(hr);
}

// ---- ShadingRateMap：D3D11 不支持 ----
LRhiD3d11ShadingRateMap::LRhiD3d11ShadingRateMap(std::shared_ptr<LRhiImplementation> rhi)
    : LRhiShadingRateMap(rhi)
{
}

LK_NAMESPACE_END
