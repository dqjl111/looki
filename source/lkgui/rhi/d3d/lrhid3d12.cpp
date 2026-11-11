#include "lrhid3d12.h"

LK_NAMESPACE_BEGIN


LD3D12Buffer::LD3D12Buffer(std::shared_ptr<LRhiImplementation> rhi, Type type, UsageFlags usage, uint32_t size)
 : LRhiBuffer(rhi, type, usage, size)
{
    for(int i=0; i< LD3D12_FRAMES_IN_FLIGHT; ++i){
        pendingHostWrites[i].reserve(16);
    }
}

LD3D12Buffer::~LD3D12Buffer()
{
    destroy();
}

void LD3D12Buffer::destroy()
{
    if (handles[0].isNull())
        return;

    std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(m_rhi.lock());
    // destroy() implementations, unlike other functions, are expected to test
    // for m_rhi (rhiD) being null, to allow surviving in case one attempts to
    // destroy a (leaked) resource after the QRhi.
    //
    // If there is no QRhi anymore, we do not deferred-release but that's fine
    // since the QRhi already released everything that was in the resourcePool.

    for (int i = 0; i < LD3D12_FRAMES_IN_FLIGHT; ++i) {
        if (rhiD!=nullptr){
            rhiD->releaseQueue.deferredReleaseResource(handles[i]);
        }
        handles[i] = {};
        pendingHostWrites[i].clear();
    }

    if (rhiD){
        rhiD->unregisterResource(this);
    }
}

bool LD3D12Buffer::create()
{

}

LRhiBuffer::NativeBuffer LD3D12Buffer::nativeBuffer()
{

}

char *LD3D12Buffer::beginFullDynamicBufferUpdateForCurrentFrame()
{

}

void LD3D12Buffer::endFullDynamicBufferUpdateForCurrentFrame()
{

}

void LD3D12Buffer::executeHostWritesForFrameSlot(int frameSlot)
{

}



LD3D12ShaderResourceBindings::LD3D12ShaderResourceBindings(std::shared_ptr<LRhiImplementation> rhi)
    :LRhiShaderResourceBindings(rhi)
{
}

LRhiD3d12::LRhiD3d12(std::shared_ptr<LRhiD3D12InitParams> params, std::shared_ptr<LRhiD3D12NativeHandles> importDevice)
{
    activeReadbacks.reserve(4);
}

bool LRhiD3d12::create(LFlags<LRhiFlag> flags)
{
    return false;
}

void LRhiD3d12::destroy()
{

}

std::shared_ptr<LRhiGraphicsPipeline> LRhiD3d12::createGraphicsPipeline()
{
    return nullptr;
}


std::shared_ptr<LRhiSwapChain> LRhiD3d12::createSwapChain()
{
    return nullptr;
}

std::shared_ptr<LRhiShadingRateMap> LRhiD3d12::createShadingRateMap()
{
    return nullptr;
}

std::shared_ptr<LRhiTextureRenderTarget> LRhiD3d12::createTextureRenderTarget(const LRhiTextureRenderTargetDescription &desc, LRhiTextureRenderTarget::Flags flags)
{
    return nullptr;
}

std::shared_ptr<LRhiSampler> LRhiD3d12::createSampler(LRhiSampler::Filter magFilter, LRhiSampler::Filter minFilter, LRhiSampler::Filter mipmapMode, LRhiSampler::AddressMode u, LRhiSampler::AddressMode v, LRhiSampler::AddressMode w)
{
    return nullptr;
}

std::shared_ptr<LRhiTexture> LRhiD3d12::createTexture(LRhiTexture::Format format, const LSize<int> &pixelSize, int depth, int arraySize, int sampleCount, LRhiTexture::Flags flags)
{
    return nullptr;
}

std::shared_ptr<LRhiRenderBuffer> LRhiD3d12::createRenderBuffer(LRhiRenderBuffer::Type type, const LSize<int> &pixelSize, int sampleCount, bool usedWithSwapChainOnly, LRhiTexture::Format backingFormatHint)
{
    return nullptr;
}

std::shared_ptr<LRhiBuffer> LRhiD3d12::createBuffer(LRhiBuffer::Type type, LRhiBuffer::UsageFlags usage, uint32_t size)
{
    return nullptr;
}

std::shared_ptr<LRhiShaderResourceBindings> LRhiD3d12::createShaderResourceBindings()
{
    return nullptr;
}

std::shared_ptr<LRhiComputePipeline> LRhiD3d12::createComputePipeline()
{
    return nullptr;
}


LRhi::FrameOpResult LRhiD3d12::beginFrame(std::shared_ptr<LRhiSwapChain> swapChain, LRhi::BeginFrameFlags flags)
{
    return LRhi::FrameOpResult();
}

LRhi::FrameOpResult LRhiD3d12::endFrame(std::shared_ptr<LRhiSwapChain> swapChain, LRhi::EndFrameFlags flags)
{
    return LRhi::FrameOpResult();
}

LRhi::FrameOpResult LRhiD3d12::beginOffscreenFrame(std::shared_ptr<LRhiCommandBuffer> *cb, LRhi::BeginFrameFlags flags)
{
    return LRhi::FrameOpResult();
}

LRhi::FrameOpResult LRhiD3d12::endOffscreenFrame(LRhi::EndFrameFlags flags)
{
    return LRhi::FrameOpResult();
}

LRhi::FrameOpResult LRhiD3d12::finish()
{
    return LRhi::FrameOpResult();
}

void LRhiD3d12::resourceUpdate(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates)
{

}

void LRhiD3d12::beginPass(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiRenderTarget> rt, const LColor &colorClearValue, const LRhiDepthStencilClearValue &depthStencilClearValue, std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates, LRhiCommandBuffer::BeginPassFlags flags)
{

}

void LRhiD3d12::endPass(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates)
{

}

void LRhiD3d12::setGraphicsPipeline(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiGraphicsPipeline> ps)
{

}

void LRhiD3d12::setShaderResources(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiShaderResourceBindings> srb, int dynamicOffsetCount, const LRhiCommandBuffer::DynamicOffset *dynamicOffsets)
{

}

void LRhiD3d12::setVertexInput(std::shared_ptr<LRhiCommandBuffer> cb, int startBinding, int bindingCount, const LRhiCommandBuffer::VertexInput *bindings, std::shared_ptr<LRhiBuffer> indexBuf, uint32_t indexOffset, LRhiCommandBuffer::IndexFormat indexFormat)
{

}

void LRhiD3d12::setViewport(std::shared_ptr<LRhiCommandBuffer> cb, const LRhiViewport &viewport)
{

}

void LRhiD3d12::setScissor(std::shared_ptr<LRhiCommandBuffer> cb, const LRhiScissor &scissor)
{

}

void LRhiD3d12::setBlendConstants(std::shared_ptr<LRhiCommandBuffer> cb, const LColor &c)
{

}

void LRhiD3d12::setStencilRef(std::shared_ptr<LRhiCommandBuffer> cb, uint32_t refValue)
{

}

void LRhiD3d12::setShadingRate(std::shared_ptr<LRhiCommandBuffer> cb, const LSize<int> &coarsePixelSize)
{

}

void LRhiD3d12::draw(std::shared_ptr<LRhiCommandBuffer> cb, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{

}

void LRhiD3d12::drawIndexed(std::shared_ptr<LRhiCommandBuffer> cb, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{

}

void LRhiD3d12::debugMarkBegin(std::shared_ptr<LRhiCommandBuffer> cb, const std::string &name)
{

}

void LRhiD3d12::debugMarkEnd(std::shared_ptr<LRhiCommandBuffer> cb)
{

}

void LRhiD3d12::debugMarkMsg(std::shared_ptr<LRhiCommandBuffer> cb, const std::string &msg)
{

}

void LRhiD3d12::beginComputePass(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates, LRhiCommandBuffer::BeginPassFlags flags)
{

}

void LRhiD3d12::endComputePass(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates)
{

}

void LRhiD3d12::setComputePipeline(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiComputePipeline> ps)
{

}

void LRhiD3d12::dispatch(std::shared_ptr<LRhiCommandBuffer> cb, int x, int y, int z)
{

}

const std::shared_ptr<LRhiNativeHandles> LRhiD3d12::nativeHandles(std::shared_ptr<LRhiCommandBuffer> cb)
{
    return nullptr;
}


void LRhiD3d12::beginExternal(std::shared_ptr<LRhiCommandBuffer> cb)
{

}

void LRhiD3d12::endExternal(std::shared_ptr<LRhiCommandBuffer> cb)
{

}

double LRhiD3d12::lastCompletedGpuTime(std::shared_ptr<LRhiCommandBuffer> cb)
{
    return 0;
}

std::list<int> LRhiD3d12::supportedSampleCounts() const
{
    return std::list<int>();
}

int LRhiD3d12::ubufAlignment() const
{
    return 0;
}

std::list<LSize<int> > LRhiD3d12::supportedShadingRates(int sampleCount) const
{
    return std::list<LSize<int> >();
}

bool LRhiD3d12::isYUpInFramebuffer() const
{
    return false;
}

bool LRhiD3d12::isYUpInNDC() const
{
    return false;
}

bool LRhiD3d12::isClipDepthZeroToOne() const
{
    return false;
}

LMatrix4x4 LRhiD3d12::clipSpaceCorrMatrix() const
{
    return LMatrix4x4();
}

bool LRhiD3d12::isTextureFormatSupported(LRhiTexture::Format format, LRhiTexture::Flags flags) const
{
    return false;
}

bool LRhiD3d12::isFeatureSupported(LRhi::Feature feature) const
{
    return false;
}

int LRhiD3d12::resourceLimit(LRhi::ResourceLimit limit) const
{
    return 0;
}

const std::shared_ptr<LRhiNativeHandles> LRhiD3d12::nativeHandles()
{
    return nullptr;
}

LRhiDriverInfo LRhiD3d12::driverInfo() const
{
    return LRhiDriverInfo();
}

LRhiStats LRhiD3d12::statistics()
{
    return LRhiStats();
}

bool LRhiD3d12::makeThreadLocalNativeContextCurrent()
{
    return false;
}

void LRhiD3d12::setQueueSubmitParams(std::shared_ptr<LRhiNativeHandles> params)
{

}

void LRhiD3d12::releaseCachedResources()
{

}

bool LRhiD3d12::isDeviceLost() const
{
    return false;
}

std::vector<uint8_t> LRhiD3d12::pipelineCacheData()
{
    return std::vector<uint8_t>();
}

void LRhiD3d12::setPipelineCacheData(const std::vector<uint8_t> &data)
{

}




LK_NAMESPACE_END