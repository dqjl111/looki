#ifndef LRHIIMPLEMENTATION_H
#define LRHIIMPLEMENTATION_H

#include "lkcore/lobject.h"
#include "lkgui/rhi/lrhi.h"
#include "lkgui/lsize.h"
#include "lkgui/math/lmatrix4x4.h"

LK_NAMESPACE_BEGIN

class LKGUIDEF LRhiImplementation : public LObject
{
public:
    LRhiImplementation();
    virtual bool create(LFlags<LRhiFlag> flags) = 0;
    virtual void destroy() = 0;

    virtual std::vector<std::shared_ptr<LRhiAdapter>> enumerateAdaptersBeforeCreate(std::shared_ptr<LRhiNativeHandles> nativeHandles) const;

    virtual std::shared_ptr<LRhiGraphicsPipeline> createGraphicsPipeline() = 0;
    virtual std::shared_ptr<LRhiComputePipeline> createComputePipeline() = 0;
    virtual std::shared_ptr<LRhiShaderResourceBindings> createShaderResourceBindings() = 0;
    virtual std::shared_ptr<LRhiBuffer> createBuffer(LRhiBuffer::Type type,
                                                 LRhiBuffer::UsageFlags usage,
                                                 uint32_t size) = 0;
    virtual std::shared_ptr<LRhiRenderBuffer> createRenderBuffer(LRhiRenderBuffer::Type type,
                                                 const LSize<int> &pixelSize,
                                                 int sampleCount,
                                                 bool usedWithSwapChainOnly,
                                                 LRhiTexture::Format backingFormatHint) = 0;
    virtual std::shared_ptr<LRhiTexture> createTexture(LRhiTexture::Format format,
                                       const LSize<int> &pixelSize,
                                       int depth,
                                       int arraySize,
                                       int sampleCount,
                                       LRhiTexture::Flags flags) = 0;
    virtual std::shared_ptr<LRhiSampler> createSampler(LRhiSampler::Filter magFilter,
                                       LRhiSampler::Filter minFilter,
                                       LRhiSampler::Filter mipmapMode,
                                       LRhiSampler:: AddressMode u,
                                       LRhiSampler::AddressMode v,
                                       LRhiSampler::AddressMode w) = 0;

    virtual std::shared_ptr<LRhiTextureRenderTarget> createTextureRenderTarget(const LRhiTextureRenderTargetDescription &desc,
                                                               LRhiTextureRenderTarget::Flags flags) = 0;

    virtual std::shared_ptr<LRhiShadingRateMap> createShadingRateMap() = 0;

    virtual std::shared_ptr<LRhiSwapChain> createSwapChain() = 0;
    virtual LRhi::FrameOpResult beginFrame(std::shared_ptr<LRhiSwapChain> swapChain, LRhi::BeginFrameFlags flags) = 0;
    virtual LRhi::FrameOpResult endFrame(std::shared_ptr<LRhiSwapChain> swapChain, LRhi::EndFrameFlags flags) = 0;
    virtual LRhi::FrameOpResult beginOffscreenFrame(std::shared_ptr<LRhiCommandBuffer> *cb, LRhi::BeginFrameFlags flags) = 0;
    virtual LRhi::FrameOpResult endOffscreenFrame(LRhi::EndFrameFlags flags) = 0;
    virtual LRhi::FrameOpResult finish() = 0;

    virtual void resourceUpdate(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates) = 0;

    virtual void beginPass(std::shared_ptr<LRhiCommandBuffer> cb,
                           std::shared_ptr<LRhiRenderTarget> rt,
                           const LColor &colorClearValue,
                           const LRhiDepthStencilClearValue &depthStencilClearValue,
                           std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates,
                           LRhiCommandBuffer::BeginPassFlags flags) = 0;
    virtual void endPass(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates) = 0;

    virtual void setGraphicsPipeline(std::shared_ptr<LRhiCommandBuffer> cb,
                                     std::shared_ptr<LRhiGraphicsPipeline> ps) = 0;

    virtual void setShaderResources(std::shared_ptr<LRhiCommandBuffer> cb,
                                    std::shared_ptr<LRhiShaderResourceBindings> srb,
                                    int dynamicOffsetCount,
                                    const LRhiCommandBuffer::DynamicOffset *dynamicOffsets) = 0;

    virtual void setVertexInput(std::shared_ptr<LRhiCommandBuffer> cb,
                                int startBinding, int bindingCount, const LRhiCommandBuffer::VertexInput *bindings,
                                std::shared_ptr<LRhiBuffer> indexBuf, uint32_t indexOffset,
                                LRhiCommandBuffer::IndexFormat indexFormat) = 0;

    virtual void setViewport(std::shared_ptr<LRhiCommandBuffer> cb, const LRhiViewport &viewport) = 0;
    virtual void setScissor(std::shared_ptr<LRhiCommandBuffer> cb, const LRhiScissor &scissor) = 0;
    virtual void setBlendConstants(std::shared_ptr<LRhiCommandBuffer> cb, const LColor &c) = 0;
    virtual void setStencilRef(std::shared_ptr<LRhiCommandBuffer> cb, uint32_t refValue) = 0;
    virtual void setShadingRate(std::shared_ptr<LRhiCommandBuffer> cb, const LSize<int> &coarsePixelSize) = 0;

    virtual void draw(std::shared_ptr<LRhiCommandBuffer> cb, uint32_t vertexCount,
                      uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) = 0;
    virtual void drawIndexed(std::shared_ptr<LRhiCommandBuffer> cb, uint32_t indexCount,
                             uint32_t instanceCount, uint32_t firstIndex,
                             int32_t vertexOffset, uint32_t firstInstance) = 0;

    virtual void debugMarkBegin(std::shared_ptr<LRhiCommandBuffer> cb, const std::string &name) = 0;
    virtual void debugMarkEnd(std::shared_ptr<LRhiCommandBuffer> cb) = 0;
    virtual void debugMarkMsg(std::shared_ptr<LRhiCommandBuffer> cb, const std::string &msg) = 0;

    virtual void beginComputePass(std::shared_ptr<LRhiCommandBuffer> cb,
                                  std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates,
                                  LRhiCommandBuffer::BeginPassFlags flags) = 0;
    virtual void endComputePass(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates) = 0;
    virtual void setComputePipeline(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiComputePipeline> ps) = 0;
    virtual void dispatch(std::shared_ptr<LRhiCommandBuffer> cb, int x, int y, int z) = 0;

    virtual const std::shared_ptr<LRhiNativeHandles> nativeHandles(std::shared_ptr<LRhiCommandBuffer> cb) = 0;
    virtual void beginExternal(std::shared_ptr<LRhiCommandBuffer> cb) = 0;
    virtual void endExternal(std::shared_ptr<LRhiCommandBuffer> cb) = 0;
    virtual double lastCompletedGpuTime(std::shared_ptr<LRhiCommandBuffer> cb) = 0;

    virtual std::list<int> supportedSampleCounts() const = 0;
    virtual int ubufAlignment() const = 0;
    virtual std::list<LSize<int>> supportedShadingRates(int sampleCount) const = 0;
    virtual bool isYUpInFramebuffer() const = 0;
    virtual bool isYUpInNDC() const = 0;
    virtual bool isClipDepthZeroToOne() const = 0;
    virtual LMatrix4x4 clipSpaceCorrMatrix() const = 0;
    virtual bool isTextureFormatSupported(LRhiTexture::Format format, LRhiTexture::Flags flags) const = 0;
    virtual bool isFeatureSupported(LRhi::Feature feature) const = 0;
    virtual int resourceLimit(LRhi::ResourceLimit limit) const = 0;
    virtual const std::shared_ptr<LRhiNativeHandles> nativeHandles() = 0;
    virtual LRhiDriverInfo driverInfo() const = 0;
    virtual LRhiStats statistics() = 0;
    virtual bool makeThreadLocalNativeContextCurrent() = 0;
    virtual void setQueueSubmitParams(std::shared_ptr<LRhiNativeHandles> params) = 0;
    virtual void releaseCachedResources() = 0;
    virtual bool isDeviceLost() const = 0;

    virtual std::vector<uint8_t> pipelineCacheData() = 0;
    virtual void setPipelineCacheData(const std::vector<uint8_t> &data) = 0;

    static std::shared_ptr<LRhiImplementation> newInstance(LRhi::Implementation impl, std::shared_ptr<LRhiInitParams> params, std::shared_ptr<LRhiNativeHandles> importDevice);

    void prepareForCreate(std::shared_ptr<LRhi> rhi, LRhi::Implementation impl, LRhi::Flags flags, std::shared_ptr<LRhiAdapter> adapter);

    inline std::shared_ptr<LRhi> rhi() const{
        return m_rhi.lock();
    }
protected:
    std::weak_ptr<LRhi> m_rhi;
    bool m_debugMarkers = false;
    int m_currentFrameSlot = 0;
    bool m_inFrame = false;
    LRhi::Implementation m_implType;
    std::shared_ptr<LRhiAdapter> m_requestedRhiAdapter = nullptr;
    uint64_t resUpdPoolMap = 0;

    friend class LRhi;
    friend class LRhiResourceUpdateBatch;
};

struct LRhiRenderTargetAttachmentTracker
{
    struct ResId { uint64_t id; uint32_t generation; };
    using ResIdList = std::vector<ResId>; // color, resolve, ds

    template<typename TexType, typename RenderBufferType>
    static void updateResIdList(const LRhiTextureRenderTargetDescription &desc, ResIdList *dst);

    template<typename TexType, typename RenderBufferType>
    static bool isUpToDate(const LRhiTextureRenderTargetDescription &desc, const ResIdList &currentResIdList);
};

inline bool operator==(const LRhiRenderTargetAttachmentTracker::ResId &a, const LRhiRenderTargetAttachmentTracker::ResId &b)
{
    return a.id == b.id && a.generation == b.generation;
}

inline bool operator!=(const LRhiRenderTargetAttachmentTracker::ResId &a, const LRhiRenderTargetAttachmentTracker::ResId &b)
{
    return !(a == b);
}

template<typename TexType, typename RenderBufferType>
void LRhiRenderTargetAttachmentTracker::updateResIdList(const LRhiTextureRenderTargetDescription &desc, ResIdList *dst)
{
    const bool hasDepthStencil = (desc.m_depthStencilBuffer != nullptr) || (desc.m_depthTexture != nullptr);
    dst->resize(desc.m_colorAttachments.size() * 2 + (hasDepthStencil ? 1 : 0));
    int n = 0;
    for (auto it = desc.m_colorAttachments.cbegin(), itEnd = desc.m_colorAttachments.cend(); it != itEnd; ++it, ++n) {
        const LRhiColorAttachment &colorAtt(*it);
        if (colorAtt.m_texture != nullptr) {
            std::shared_ptr<TexType> texD = std::dynamic_pointer_cast<TexType>(colorAtt.m_texture);
            (*dst)[n] = { texD->globalResourceId(), texD->generation };
        } else if (colorAtt.m_renderBuffer != nullptr) {
            std::shared_ptr<RenderBufferType> rbD = std::dynamic_pointer_cast<RenderBufferType>(colorAtt.m_renderBuffer);
            (*dst)[n] = { rbD->globalResourceId(), rbD->generation };
        } else {
            (*dst)[n] = { 0, 0 };
        }
        ++n;
        if (colorAtt.m_resolveTexture != nullptr) {
            std::shared_ptr<TexType> texD = std::dynamic_pointer_cast<TexType>(colorAtt.m_resolveTexture);
            (*dst)[n] = { texD->globalResourceId(), texD->generation };
        } else {
            (*dst)[n] = { 0, 0 };
        }
    }
    if (hasDepthStencil) {
        if (desc.m_depthTexture != nullptr) {
            std::shared_ptr<TexType> depthTexD = std::dynamic_pointer_cast<TexType>(desc.m_depthTexture);
            (*dst)[n] = { depthTexD->globalResourceId(), depthTexD->generation };
        } else if (desc.m_depthStencilBuffer != nullptr) {
            std::shared_ptr<RenderBufferType> depthRbD = std::dynamic_pointer_cast<RenderBufferType>(desc.m_depthStencilBuffer);
            (*dst)[n] = { depthRbD->globalResourceId(), depthRbD->generation };
        } else {
            (*dst)[n] = { 0, 0 };
        }
    }
}

template<typename TexType, typename RenderBufferType>
bool LRhiRenderTargetAttachmentTracker::isUpToDate(const LRhiTextureRenderTargetDescription &desc, const ResIdList &currentResIdList)
{
    // Just as setShaderResources() recognizes if an srb's referenced
    // resources have been rebuilt (got a create() since the srb's
    // create()), we should do the same for the textures and renderbuffers
    // referenced from the rendertarget. It is not uncommon that a texture
    // or ds buffer gets resized due to following a window size in some
    // form, which involves a create() on them. It is then nice if the
    // render target auto-rebuilds in beginPass().

    ResIdList resIdList;
    updateResIdList<TexType, RenderBufferType>(desc, &resIdList);
    return resIdList == currentResIdList;
}
LK_NAMESPACE_END

#endif // LRHIIMPLEMENTATION_H
