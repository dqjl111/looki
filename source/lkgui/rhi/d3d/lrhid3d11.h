#ifndef LRHID3D11_H
#define LRHID3D11_H

#include "lkgui/rhi/lrhiimplementation.h"
#include "lkgui/rhi/lrhiplatform.h"

#include <array>
#include <map>
#include <set>
#include <vector>

#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

LK_NAMESPACE_BEGIN

/**
 * D3D11 渲染后端。
 *
 * 与 D3D12 后端的关键差异，都来自 D3D11 是"立即模式 + 驱动管理资源生命周期"：
 *  - 没有命令列表/围栏，命令缓冲直接包一层 ID3D11DeviceContext，draw 即提交；
 *  - 交换链用 IDXGIFactory2::CreateSwapChainForHwnd，Present 时才真正翻页；
 *  - 资源由驱动在引用计数归零时释放，不需要 descriptor heap 与显存分配器。
 */
class LRhiD3d11;
class LRhiD3d11CommandBuffer;
class LRhiD3d11GraphicsPipeline;

// ---------------------------------------------------------------------------
// 小工具：COM 指针容器
// ---------------------------------------------------------------------------
template<typename T>
using LD3D11ComPtr = Microsoft::WRL::ComPtr<T>;

// ---------------------------------------------------------------------------
// Buffer
// ---------------------------------------------------------------------------
class LRhiD3d11Buffer : public LRhiBuffer
{
public:
    LRhiD3d11Buffer(std::shared_ptr<LRhiImplementation> rhi, Type type, UsageFlags usage, uint32_t size);
    ~LRhiD3d11Buffer() override;

    bool create() override;
    NativeBuffer nativeBuffer() override;

    char *beginFullDynamicBufferUpdateForCurrentFrame() override;
    void endFullDynamicBufferUpdateForCurrentFrame() override;
    void fullDynamicBufferUpdateForCurrentFrame(const void *data, uint32_t size) override;

    void destroyBuffer();

    LD3D11ComPtr<ID3D11Buffer> m_buffer;
    /** 动态缓冲的持久映射指针（D3D11_USAGE_DYNAMIC + MAP_WRITE_DISCARD） */
    char *m_mapped = nullptr;
    bool m_mappedThisFrame = false;
};

// ---------------------------------------------------------------------------
// Texture
// ---------------------------------------------------------------------------
class LRhiD3d11Texture : public LRhiTexture
{
public:
    LRhiD3d11Texture(std::shared_ptr<LRhiImplementation> rhi, Format format,
                     const LSize<int> &pixelSize, int depth, int arraySize,
                     int sampleCount, Flags flags);
    ~LRhiD3d11Texture() override;

    bool create() override;
    NativeTexture nativeTexture() override;
    bool createFrom(NativeTexture src) override;

    void destroyTexture();

    LD3D11ComPtr<ID3D11Texture2D> m_texture;
    LD3D11ComPtr<ID3D11ShaderResourceView> m_srv;
    LD3D11ComPtr<ID3D11RenderTargetView> m_rtv;
    LD3D11ComPtr<ID3D11UnorderedAccessView> m_uav;
    /** 导入的外部纹理（例如视频帧）不归我们释放，但要保留引用防悬垂 */
    bool m_imported = false;
};

// ---------------------------------------------------------------------------
// Sampler
// ---------------------------------------------------------------------------
class LRhiD3d11Sampler : public LRhiSampler
{
public:
    LRhiD3d11Sampler(std::shared_ptr<LRhiImplementation> rhi,
                     Filter magFilter, Filter minFilter, Filter mipmapMode,
                     AddressMode u, AddressMode v, AddressMode w);
    ~LRhiD3d11Sampler() override;

    bool create() override;
    void destroySampler();

    LD3D11ComPtr<ID3D11SamplerState> m_sampler;
};

// ---------------------------------------------------------------------------
// RenderBuffer（深度/颜色后备缓冲，主要用于交换链）
// ---------------------------------------------------------------------------
class LRhiD3d11RenderBuffer : public LRhiRenderBuffer
{
public:
    LRhiD3d11RenderBuffer(std::shared_ptr<LRhiImplementation> rhi, Type type,
                          const LSize<int> &pixelSize, int sampleCount,
                          bool usedWithSwapChainOnly, LRhiTexture::Format backingFormatHint);
    ~LRhiD3d11RenderBuffer() override;

    bool create() override;
    LRhiTexture::Format backingFormat() const override;
    void destroyRenderBuffer();

    LD3D11ComPtr<ID3D11Texture2D> m_texture;
    LD3D11ComPtr<ID3D11RenderTargetView> m_rtv;
    LD3D11ComPtr<ID3D11DepthStencilView> m_dsv;
};

// ---------------------------------------------------------------------------
// RenderPassDescriptor / RenderTarget
// ---------------------------------------------------------------------------
class LRhiD3d11RenderPassDescriptor : public LRhiRenderPassDescriptor
{
public:
    explicit LRhiD3d11RenderPassDescriptor(std::shared_ptr<LRhiImplementation> rhi);

    bool isCompatible(const std::shared_ptr<LRhiRenderPassDescriptor> other) const override;
    std::shared_ptr<LRhiRenderPassDescriptor> newCompatibleRenderPassDescriptor() const override;
    std::vector<uint32_t> serializedFormat() const override;

    std::vector<LRhiTexture::Format> m_colorFormats;
    LRhiTexture::Format m_depthFormat = LRhiTexture::UnknownFormat;
    int m_sampleCount = 1;
};

class LRhiD3d11TextureRenderTarget : public LRhiTextureRenderTarget
{
public:
    LRhiD3d11TextureRenderTarget(std::shared_ptr<LRhiImplementation> rhi,
                                 const LRhiTextureRenderTargetDescription &desc,
                                 Flags flags);

    bool create() override;
    std::shared_ptr<LRhiRenderPassDescriptor> newCompatibleRenderPassDescriptor() override;

    LSize<int> pixelSize() const override { return m_pixelSize; }
    float devicePixelRatio() const override { return 1.0f; }
    int sampleCount() const override { return m_desc.m_colorAttachments.empty() ? 1 : 1; }

    void destroyRenderTarget();
    bool buildViews();

    std::vector<LD3D11ComPtr<ID3D11RenderTargetView>> m_rtvs;
    LD3D11ComPtr<ID3D11DepthStencilView> m_dsv;
    LSize<int> m_pixelSize;
};

// ---------------------------------------------------------------------------
// SwapChain
// ---------------------------------------------------------------------------
class LRhiD3d11SwapChainRenderTarget : public LRhiSwapChainRenderTarget
{
public:
    LRhiD3d11SwapChainRenderTarget(std::shared_ptr<LRhiImplementation> rhi,
                                   std::shared_ptr<LRhiSwapChain> swapchain);
    LSize<int> pixelSize() const override;
    float devicePixelRatio() const override { return 1.0f; }
    int sampleCount() const override;
};

class LRhiD3d11SwapChain : public LRhiSwapChain
{
public:
    explicit LRhiD3d11SwapChain(std::shared_ptr<LRhiImplementation> rhi);
    ~LRhiD3d11SwapChain() override;

    std::shared_ptr<LRhiCommandBuffer> currentFrameCommandBuffer() override;
    std::shared_ptr<LRhiRenderTarget> currentFrameRenderTarget() override;
    LSize<int> surfacePixelSize() override;
    bool isFormatSupported(Format f) override;
    std::shared_ptr<LRhiRenderPassDescriptor> newCompatibleRenderPassDescriptor() override;
    bool createOrResize() override;

    bool createSwapChain();
    void destroySwapChain();
    bool ensureBackBufferViews();

    LD3D11ComPtr<IDXGISwapChain1> m_swapChain;
    LD3D11ComPtr<ID3D11Texture2D> m_backBuffer;
    LD3D11ComPtr<ID3D11RenderTargetView> m_rtv;
    LD3D11ComPtr<ID3D11Texture2D> m_depthTexture;
    LD3D11ComPtr<ID3D11DepthStencilView> m_dsv;
    std::shared_ptr<LRhiD3d11CommandBuffer> m_cb;
    std::shared_ptr<LRhiD3d11SwapChainRenderTarget> m_rt;
    std::shared_ptr<LRhiD3d11RenderPassDescriptor> m_passDesc;
    LSize<int> m_pixelSize;
    LSize<int> m_requestedSize;
    bool m_valid = false;
};

// ---------------------------------------------------------------------------
// CommandBuffer（立即模式，直接写 ID3D11DeviceContext）
// ---------------------------------------------------------------------------
class LRhiD3d11CommandBuffer : public LRhiCommandBuffer
{
public:
    explicit LRhiD3d11CommandBuffer(std::shared_ptr<LRhiImplementation> rhi);

    void resetRecording();

    LD3D11ComPtr<ID3D11DeviceContext1> m_context;
    std::shared_ptr<LRhiRenderTarget> m_currentTarget;
    bool m_inPass = false;
    bool m_usingSwapChainPass = false;
};

// ---------------------------------------------------------------------------
// ShaderResourceBindings
// ---------------------------------------------------------------------------
class LRhiD3d11ShaderResourceBindings : public LRhiShaderResourceBindings
{
public:
    explicit LRhiD3d11ShaderResourceBindings(std::shared_ptr<LRhiImplementation> rhi);
    ~LRhiD3d11ShaderResourceBindings() override;

    bool create() override;
    void updateResources(bool bindingsAreSorted = false) override;

    void destroyBindings();

    /** 按 binding 号记录三类常量/资源槽位，bind 时统一提交 */
    std::map<int, LD3D11ComPtr<ID3D11Buffer>> m_constantBuffers;
    std::map<int, LD3D11ComPtr<ID3D11ShaderResourceView>> m_srvs;
    std::map<int, LD3D11ComPtr<ID3D11SamplerState>> m_samplers;
};

// ---------------------------------------------------------------------------
// Pipeline
// ---------------------------------------------------------------------------
class LRhiD3d11GraphicsPipeline : public LRhiGraphicsPipeline
{
public:
    explicit LRhiD3d11GraphicsPipeline(std::shared_ptr<LRhiImplementation> rhi);
    ~LRhiD3d11GraphicsPipeline() override;

    bool create() override;
    void destroyPipeline();

    LD3D11ComPtr<ID3D11InputLayout> m_inputLayout;
    LD3D11ComPtr<ID3D11VertexShader> m_vs;
    LD3D11ComPtr<ID3D11PixelShader> m_ps;
    LD3D11ComPtr<ID3D11GeometryShader> m_gs;
    LD3D11ComPtr<ID3D11RasterizerState> m_rasterState;
    LD3D11ComPtr<ID3D11DepthStencilState> m_dsState;
    LD3D11ComPtr<ID3D11BlendState> m_blendState;
    /** 基类 m_topology 是 L 侧枚举，这里用 D3D 拓扑，避免同名遮蔽 */
    D3D11_PRIMITIVE_TOPOLOGY m_d3dTopology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    UINT m_stencilRef = 0;
};

class LRhiD3d11ComputePipeline : public LRhiComputePipeline
{
public:
    explicit LRhiD3d11ComputePipeline(std::shared_ptr<LRhiImplementation> rhi);
    ~LRhiD3d11ComputePipeline() override;

    bool create() override;
    void destroyPipeline();

    LD3D11ComPtr<ID3D11ComputeShader> m_cs;
};

// ---------------------------------------------------------------------------
// ShadingRateMap：D3D11 不支持可变着色率
// ---------------------------------------------------------------------------
class LRhiD3d11ShadingRateMap : public LRhiShadingRateMap
{
public:
    explicit LRhiD3d11ShadingRateMap(std::shared_ptr<LRhiImplementation> rhi);
};

// ---------------------------------------------------------------------------
// 后端本体
// ---------------------------------------------------------------------------
class LRhiD3d11 : public LRhiImplementation
{
public:
    LRhiD3d11(std::shared_ptr<LRhiD3D11InitParams> params,
              std::shared_ptr<LRhiD3D11NativeHandles> importDevice);
    ~LRhiD3d11() override;

    bool create(LFlags<LRhiFlag> flags) override;
    void destroy() override;

    std::shared_ptr<LRhiGraphicsPipeline> createGraphicsPipeline() override;
    std::shared_ptr<LRhiComputePipeline> createComputePipeline() override;
    std::shared_ptr<LRhiShaderResourceBindings> createShaderResourceBindings() override;
    std::shared_ptr<LRhiBuffer> createBuffer(LRhiBuffer::Type type,
                                             LRhiBuffer::UsageFlags usage,
                                             uint32_t size) override;
    std::shared_ptr<LRhiRenderBuffer> createRenderBuffer(LRhiRenderBuffer::Type type,
                                                         const LSize<int> &pixelSize,
                                                         int sampleCount,
                                                         bool usedWithSwapChainOnly,
                                                         LRhiTexture::Format backingFormatHint) override;
    std::shared_ptr<LRhiTexture> createTexture(LRhiTexture::Format format,
                                              const LSize<int> &pixelSize,
                                              int depth, int arraySize, int sampleCount,
                                              LRhiTexture::Flags flags) override;
    std::shared_ptr<LRhiSampler> createSampler(LRhiSampler::Filter magFilter,
                                               LRhiSampler::Filter minFilter,
                                               LRhiSampler::Filter mipmapMode,
                                               LRhiSampler::AddressMode u,
                                               LRhiSampler::AddressMode v,
                                               LRhiSampler::AddressMode w) override;
    std::shared_ptr<LRhiTextureRenderTarget> createTextureRenderTarget(
            const LRhiTextureRenderTargetDescription &desc,
            LRhiTextureRenderTarget::Flags flags) override;
    std::shared_ptr<LRhiShadingRateMap> createShadingRateMap() override;
    std::shared_ptr<LRhiSwapChain> createSwapChain() override;

    LRhi::FrameOpResult beginFrame(std::shared_ptr<LRhiSwapChain> swapChain,
                                  LRhi::BeginFrameFlags flags) override;
    LRhi::FrameOpResult endFrame(std::shared_ptr<LRhiSwapChain> swapChain,
                                LRhi::EndFrameFlags flags) override;
    LRhi::FrameOpResult beginOffscreenFrame(std::shared_ptr<LRhiCommandBuffer> *cb,
                                           LRhi::BeginFrameFlags flags) override;
    LRhi::FrameOpResult endOffscreenFrame(LRhi::EndFrameFlags flags) override;
    LRhi::FrameOpResult finish() override;

    void resourceUpdate(std::shared_ptr<LRhiCommandBuffer> cb,
                        std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates) override;

    void beginPass(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiRenderTarget> rt,
                   const LColor &colorClearValue,
                   const LRhiDepthStencilClearValue &depthStencilClearValue,
                   std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates,
                   LRhiCommandBuffer::BeginPassFlags flags) override;
    void endPass(std::shared_ptr<LRhiCommandBuffer> cb,
                 std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates) override;

    void setGraphicsPipeline(std::shared_ptr<LRhiCommandBuffer> cb,
                             std::shared_ptr<LRhiGraphicsPipeline> ps) override;
    void setShaderResources(std::shared_ptr<LRhiCommandBuffer> cb,
                            std::shared_ptr<LRhiShaderResourceBindings> srb,
                            int dynamicOffsetCount,
                            const LRhiCommandBuffer::DynamicOffset *dynamicOffsets) override;
    void setVertexInput(std::shared_ptr<LRhiCommandBuffer> cb,
                        int startBinding, int bindingCount,
                        const LRhiCommandBuffer::VertexInput *bindings,
                        std::shared_ptr<LRhiBuffer> indexBuf, uint32_t indexOffset,
                        LRhiCommandBuffer::IndexFormat indexFormat) override;

    void setViewport(std::shared_ptr<LRhiCommandBuffer> cb, const LRhiViewport &viewport) override;
    void setScissor(std::shared_ptr<LRhiCommandBuffer> cb, const LRhiScissor &scissor) override;
    void setBlendConstants(std::shared_ptr<LRhiCommandBuffer> cb, const LColor &c) override;
    void setStencilRef(std::shared_ptr<LRhiCommandBuffer> cb, uint32_t refValue) override;
    void setShadingRate(std::shared_ptr<LRhiCommandBuffer> cb, const LSize<int> &coarsePixelSize) override;

    void draw(std::shared_ptr<LRhiCommandBuffer> cb, uint32_t vertexCount,
              uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
    void drawIndexed(std::shared_ptr<LRhiCommandBuffer> cb, uint32_t indexCount,
                     uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset,
                     uint32_t firstInstance) override;

    void debugMarkBegin(std::shared_ptr<LRhiCommandBuffer> cb, const std::string &name) override;
    void debugMarkEnd(std::shared_ptr<LRhiCommandBuffer> cb) override;
    void debugMarkMsg(std::shared_ptr<LRhiCommandBuffer> cb, const std::string &msg) override;

    void beginComputePass(std::shared_ptr<LRhiCommandBuffer> cb,
                          std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates,
                          LRhiCommandBuffer::BeginPassFlags flags) override;
    void endComputePass(std::shared_ptr<LRhiCommandBuffer> cb,
                        std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates) override;
    void setComputePipeline(std::shared_ptr<LRhiCommandBuffer> cb,
                            std::shared_ptr<LRhiComputePipeline> ps) override;
    void dispatch(std::shared_ptr<LRhiCommandBuffer> cb, int x, int y, int z) override;

    const std::shared_ptr<LRhiNativeHandles> nativeHandles(std::shared_ptr<LRhiCommandBuffer> cb) override;
    void beginExternal(std::shared_ptr<LRhiCommandBuffer> cb) override;
    void endExternal(std::shared_ptr<LRhiCommandBuffer> cb) override;
    double lastCompletedGpuTime(std::shared_ptr<LRhiCommandBuffer> cb) override;

    std::list<int> supportedSampleCounts() const override;
    int ubufAlignment() const override;
    std::list<LSize<int>> supportedShadingRates(int sampleCount) const override;
    bool isYUpInFramebuffer() const override;
    bool isYUpInNDC() const override;
    bool isClipDepthZeroToOne() const override;
    LMatrix4x4 clipSpaceCorrMatrix() const override;
    bool isTextureFormatSupported(LRhiTexture::Format format, LRhiTexture::Flags flags) const override;
    bool isFeatureSupported(LRhi::Feature feature) const override;
    int resourceLimit(LRhi::ResourceLimit limit) const override;
    const std::shared_ptr<LRhiNativeHandles> nativeHandles() override;
    LRhiDriverInfo driverInfo() const override;
    LRhiStats statistics() override;
    bool makeThreadLocalNativeContextCurrent() override;
    void setQueueSubmitParams(std::shared_ptr<LRhiNativeHandles> params) override;
    void releaseCachedResources() override;
    bool isDeviceLost() const override;

    std::vector<uint8_t> pipelineCacheData() override;
    void setPipelineCacheData(const std::vector<uint8_t> &data) override;

    // ---- 供各资源类使用的内部工具 ----
    ID3D11Device *device() const { return m_device.Get(); }
    ID3D11DeviceContext1 *context() const { return m_context.Get(); }
    IDXGIFactory2 *factory() const { return m_factory.Get(); }
    std::shared_ptr<LRhiD3d11CommandBuffer> currentCommandBuffer() const { return m_cb; }

    /** LFormat -> DXGI_FORMAT */
    static DXGI_FORMAT toDxgiFormat(LRhiTexture::Format f);
    /** LShader 里取当前变体的 DXBC 字节码 */
    static bool shaderByteCode(const LShader &shader, LShaderKey key, std::vector<uint8_t> *out);

private:
    bool createDevice();
    void destroyDevice();
    bool createDefaultStates();

    std::shared_ptr<LRhiD3D11InitParams> m_params;
    std::shared_ptr<LRhiD3D11NativeHandles> m_import;
    LRhiD3D11NativeHandles m_native;

    LD3D11ComPtr<ID3D11Device> m_device;
    LD3D11ComPtr<ID3D11DeviceContext1> m_context;
    LD3D11ComPtr<IDXGIFactory2> m_factory;
    LD3D11ComPtr<IDXGIAdapter1> m_adapter;
    D3D_FEATURE_LEVEL m_featureLevel = D3D_FEATURE_LEVEL_11_0;
    bool m_ownsDevice = true;
    bool m_deviceLost = false;

    std::shared_ptr<LRhiD3d11CommandBuffer> m_cb;
    std::shared_ptr<LRhiD3d11GraphicsPipeline> m_currentGraphicsPipeline;
    LRhiDriverInfo m_driverInfo;
};

LK_NAMESPACE_END

#endif // LRHID3D11_H
