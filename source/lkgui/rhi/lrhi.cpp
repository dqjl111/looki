#include "lrhiimplementation.h"

LK_NAMESPACE_BEGIN


LRhiInitParams::LRhiInitParams()
{

}

LRhiNativeHandles::LRhiNativeHandles()
{

}

uint64_t newRhiResourceId()
{
    static std::atomic_int64_t counter = 0;
    return counter.fetch_add(1, std::memory_order::relaxed) + 1;
}

LRhiResource::LRhiResource(std::shared_ptr<LRhiImplementation> rhi) : m_rhi(rhi)
{
    m_id = newRhiResourceId();
}

LRhiResource::~LRhiResource()
{

}

std::shared_ptr<LRhi> LRhiResource::rhi() const
{
    std::shared_ptr<LRhiImplementation> t = m_rhi.lock();
    if(t){
        return t->rhi();
    }
    return nullptr;
}


LRhiBuffer::LRhiBuffer(std::shared_ptr<LRhiImplementation> rhi, Type type_, UsageFlags usage_, uint32_t size_)
    :LRhiResource(rhi),m_type(type_), m_usage(usage_), m_size(size_)
{

}

LRhiResource::Type LRhiBuffer::resourceType() const
{
    return Buffer;
}

char *LRhiBuffer::beginFullDynamicBufferUpdateForCurrentFrame()
{
    return nullptr;
}

void LRhiBuffer::endFullDynamicBufferUpdateForCurrentFrame()
{

}

void LRhiBuffer::fullDynamicBufferUpdateForCurrentFrame(const void *data, uint32_t size)
{
    char *p = beginFullDynamicBufferUpdateForCurrentFrame();
    if (p) {
        memcpy(p, data, size > 0 ? size : m_size);
        endFullDynamicBufferUpdateForCurrentFrame();
    }
}


LRhiBuffer::NativeBuffer LRhiBuffer::nativeBuffer()
{
    return { {}, 0 };
}



LRhiResource::Type LRhiTexture::resourceType() const
{
    return Texture;
}

bool LRhiTexture::createFrom(NativeTexture src)
{
    return false;
}

void LRhiTexture::setNativeLayout(int layout)
{

}

LRhiTexture::NativeTexture LRhiTexture::nativeTexture()
{
    return {};
}

LRhiTexture::LRhiTexture(std::shared_ptr<LRhiImplementation> rhi, Format format_, const LSize<int> &pixelSize_, int depth_, int arraySize_, int sampleCount_, Flags flags_)
    : LRhiResource(rhi),
    m_format(format_), m_pixelSize(pixelSize_), m_depth(depth_),
    m_arraySize(arraySize_), m_sampleCount(sampleCount_), m_flags(flags_)
{

}

LRhiSampler::LRhiSampler(std::shared_ptr<LRhiImplementation> rhi, Filter magFilter_, Filter minFilter_, Filter mipmapMode_, AddressMode u_, AddressMode v_, AddressMode w_)
    : LRhiResource(rhi),
    m_magFilter(magFilter_), m_minFilter(minFilter_), m_mipmapMode(mipmapMode_),
    m_addressU(u_), m_addressV(v_), m_addressW(w_),
    m_compareOp(LRhiSampler::Never)
{

}


LRhiResource::Type LRhiSampler::resourceType() const
{
    return Sampler;
}


LRhiTextureSubresourceUploadDescription::LRhiTextureSubresourceUploadDescription(const std::shared_ptr<LImage> image): m_image(image)
{

}

LRhiTextureSubresourceUploadDescription::LRhiTextureSubresourceUploadDescription(const std::vector<uint8_t> &data) : m_data(data)
{

}


LRhiTextureUploadEntry::LRhiTextureUploadEntry(int layer, int level, const LRhiTextureSubresourceUploadDescription &desc)
    : m_layer(layer),
    m_level(level),
    m_desc(desc)
{

}

LRhiTextureUploadDescription::LRhiTextureUploadDescription(const LRhiTextureUploadEntry &entry)
{
    m_entries.push_back(entry);
}

LRhiTextureUploadDescription::LRhiTextureUploadDescription(std::initializer_list<LRhiTextureUploadEntry> list)
    :m_entries(list)
{

}

LRhiRenderBuffer::LRhiRenderBuffer(std::shared_ptr<LRhiImplementation> rhi, Type type_, const LSize<int> &pixelSize_, int sampleCount_, bool usedWithSwapChainOnly_, LRhiTexture::Format backingFormatHint_)
    : LRhiResource(rhi),
    m_type(type_), m_pixelSize(pixelSize_), m_sampleCount(sampleCount_), m_usedWithSwapChainOnly(usedWithSwapChainOnly_),
    m_backingFormatHint(backingFormatHint_)
{

}

LRhiResource::Type LRhiRenderBuffer::resourceType() const
{
    return RenderBuffer;
}

bool LRhiRenderBuffer::createFrom(NativeRenderBuffer src)
{
    return false;
}


LRhiShadingRateMap::LRhiShadingRateMap(std::shared_ptr<LRhiImplementation> rhi): LRhiResource(rhi)
{

}


LRhiResource::Type LRhiShadingRateMap::resourceType() const
{
    return ShadingRateMap;
}

bool LRhiShadingRateMap::createFrom(NativeShadingRateMap src)
{
    return false;
}

bool LRhiShadingRateMap::createFrom(std::shared_ptr<LRhiTexture> src)
{
    return false;
}


LRhiRenderPassDescriptor::LRhiRenderPassDescriptor(std::shared_ptr<LRhiImplementation> rhi): LRhiResource(rhi)
{

}

LRhiResource::Type LRhiRenderPassDescriptor::resourceType() const
{
    return RenderPassDescriptor;
}

const std::shared_ptr<LRhiNativeHandles> LRhiRenderPassDescriptor::nativeHandles()
{
    return nullptr;
}


LRhiRenderTarget::LRhiRenderTarget(std::shared_ptr<LRhiImplementation> rhi): LRhiResource(rhi)
{

}

bool LRhiShaderResourceBinding::isLayoutCompatible(const LRhiShaderResourceBinding &other) const
{
    return binding == other.binding
           && stage == other.stage
           && type == other.type
           && arraySize() == other.arraySize();
}

LRhiShaderResourceBinding LRhiShaderResourceBinding::uniformBuffer(int binding, StageFlags stage, std::shared_ptr<LRhiBuffer> buf)
{
    LRhiShaderResourceBinding b;
    b.binding = binding;
    b.stage = stage;
    b.type = UniformBuffer;

    UniformBufferData& d = std::get<UniformBufferData>(b.data);
    d.buf = buf;
    d.offset = 0;
    d.maybeSize = 0;
    d.hasDynamicOffset = false;
    return b;
}

LRhiShaderResourceBinding LRhiShaderResourceBinding::uniformBuffer(int binding, StageFlags stage, std::shared_ptr<LRhiBuffer> buf, uint32_t offset, uint32_t size)
{
    LRhiShaderResourceBinding b;
    b.binding = binding;
    b.stage = stage;
    b.type = UniformBuffer;

    UniformBufferData& d = std::get<UniformBufferData>(b.data);
    d.buf = buf;
    d.offset = offset;
    d.maybeSize = size;
    d.hasDynamicOffset = false;
    return b;
}

LRhiShaderResourceBinding LRhiShaderResourceBinding::uniformBufferWithDynamicOffset(int binding, StageFlags stage, std::shared_ptr<LRhiBuffer> buf, uint32_t size)
{
    LRhiShaderResourceBinding b;
    b.binding = binding;
    b.stage = stage;
    b.type = UniformBuffer;

    UniformBufferData& d = std::get<UniformBufferData>(b.data);
    d.buf = buf;
    d.offset = 0;
    d.maybeSize = size;
    d.hasDynamicOffset = true;
    return b;
}

LRhiShaderResourceBinding LRhiShaderResourceBinding::sampledTexture(int binding, StageFlags stage, std::shared_ptr<LRhiTexture> tex, std::shared_ptr<LRhiSampler> sampler)
{
    LRhiShaderResourceBinding b;
    b.binding = binding;
    b.stage = stage;
    b.type = SampledTexture;

    TextureAndOrSamplerData& d = std::get<TextureAndOrSamplerData>(b.data);
    d.count = 1;
    d.texSamplers[0] = { tex, sampler };
    return b;
}

LRhiShaderResourceBinding LRhiShaderResourceBinding::sampledTextures(int binding, StageFlags stage, int count, const TextureAndSampler *texSamplers)
{
    assert(count >= 1 && count <= MAX_TEX_SAMPLER_ARRAY_SIZE);
    LRhiShaderResourceBinding b;
    b.binding = binding;
    b.stage = stage;
    b.type = SampledTexture;

    TextureAndOrSamplerData& d = std::get<TextureAndOrSamplerData>(b.data);
    d.count = count;
    for (int i = 0; i < count; ++i) {
        if (texSamplers){
            d.texSamplers[i] = texSamplers[i];
        }else{
            d.texSamplers[i] = { nullptr, nullptr };
        }
    }
    return b;
}

LRhiShaderResourceBinding LRhiShaderResourceBinding::texture(int binding, StageFlags stage, std::shared_ptr<LRhiTexture> tex)
{
    LRhiShaderResourceBinding b;
    b.binding = binding;
    b.stage = stage;
    b.type = Texture;

    TextureAndOrSamplerData& d = std::get<TextureAndOrSamplerData>(b.data);
    d.count = 1;
    d.texSamplers[0] = { tex, nullptr };
    return b;
}

LRhiShaderResourceBinding LRhiShaderResourceBinding::textures(int binding, StageFlags stage, int count, std::shared_ptr<LRhiTexture> *tex)
{
    assert(count >= 1 && count <= MAX_TEX_SAMPLER_ARRAY_SIZE);
    LRhiShaderResourceBinding b;
    b.binding = binding;
    b.stage = stage;
    b.type = SampledTexture;

    TextureAndOrSamplerData& d = std::get<TextureAndOrSamplerData>(b.data);
    d.count = count;
    for (int i = 0; i < count; ++i) {
        if (tex){
            d.texSamplers[i] = { tex[i], nullptr };
        }else{
            d.texSamplers[i] = { nullptr, nullptr };
        }
    }
    return b;
}

LRhiShaderResourceBinding LRhiShaderResourceBinding::sampler(int binding, StageFlags stage, std::shared_ptr<LRhiSampler> sampler)
{
    LRhiShaderResourceBinding b;
    b.binding = binding;
    b.stage = stage;
    b.type = Sampler;

    TextureAndOrSamplerData& d = std::get<TextureAndOrSamplerData>(b.data);
    d.count = 1;
    d.texSamplers[0] = { nullptr, sampler };
    return b;
}

LRhiShaderResourceBinding LRhiShaderResourceBinding::imageLoad(int binding, StageFlags stage, std::shared_ptr<LRhiTexture> tex, int level)
{
    LRhiShaderResourceBinding b;
    b.binding = binding;
    b.stage = stage;
    b.type = ImageLoad;

    StorageImageData& d = std::get<StorageImageData>(b.data);
    d.tex = tex;
    d.level = level;
    return b;
}

LRhiShaderResourceBinding LRhiShaderResourceBinding::imageStore(int binding, StageFlags stage, std::shared_ptr<LRhiTexture> tex, int level)
{
    LRhiShaderResourceBinding b;
    b.binding = binding;
    b.stage = stage;
    b.type = ImageStore;

    StorageImageData& d = std::get<StorageImageData>(b.data);
    d.tex = tex;
    d.level = level;
    return b;
}

LRhiShaderResourceBinding LRhiShaderResourceBinding::imageLoadStore(int binding, StageFlags stage, std::shared_ptr<LRhiTexture> tex, int level)
{
    LRhiShaderResourceBinding b;
    b.binding = binding;
    b.stage = stage;
    b.type = ImageLoadStore;

    StorageImageData& d = std::get<StorageImageData>(b.data);
    d.tex = tex;
    d.level = level;
    return b;
}

LRhiShaderResourceBinding LRhiShaderResourceBinding::bufferLoad(int binding, StageFlags stage, std::shared_ptr<LRhiBuffer> buf)
{
    LRhiShaderResourceBinding b;
    b.binding = binding;
    b.stage = stage;
    b.type = BufferLoad;

    StorageBufferData& d = std::get<StorageBufferData>(b.data);
    d.buf = buf;
    d.offset = 0;
    d.maybeSize = 0;
    return b;
}

LRhiShaderResourceBinding LRhiShaderResourceBinding::bufferLoad(int binding, StageFlags stage, std::shared_ptr<LRhiBuffer> buf, uint32_t offset, uint32_t size)
{
    LRhiShaderResourceBinding b;
    b.binding = binding;
    b.stage = stage;
    b.type = BufferLoad;

    StorageBufferData& d = std::get<StorageBufferData>(b.data);
    d.buf = buf;
    d.offset = offset;
    d.maybeSize = size;
    return b;
}

LRhiShaderResourceBinding LRhiShaderResourceBinding::bufferStore(int binding, StageFlags stage, std::shared_ptr<LRhiBuffer> buf)
{
    LRhiShaderResourceBinding b;
    b.binding = binding;
    b.stage = stage;
    b.type = BufferStore;

    StorageBufferData& d = std::get<StorageBufferData>(b.data);
    d.buf = buf;
    d.offset = 0;
    d.maybeSize = 0;
    return b;
}

LRhiShaderResourceBinding LRhiShaderResourceBinding::bufferStore(int binding, StageFlags stage, std::shared_ptr<LRhiBuffer> buf, uint32_t offset, uint32_t size)
{
    LRhiShaderResourceBinding b;
    b.binding = binding;
    b.stage = stage;
    b.type = BufferStore;

    StorageBufferData& d = std::get<StorageBufferData>(b.data);
    d.buf = buf;
    d.offset = offset;
    d.maybeSize = size;
    return b;
}

LRhiShaderResourceBinding LRhiShaderResourceBinding::bufferLoadStore(int binding, StageFlags stage, std::shared_ptr<LRhiBuffer> buf)
{
    LRhiShaderResourceBinding b;
    b.binding = binding;
    b.stage = stage;
    b.type = BufferLoadStore;

    StorageBufferData& d = std::get<StorageBufferData>(b.data);
    d.buf = buf;
    d.offset = 0;
    d.maybeSize = 0;
    return b;
}

LRhiShaderResourceBinding LRhiShaderResourceBinding::bufferLoadStore(int binding, StageFlags stage, std::shared_ptr<LRhiBuffer> buf, uint32_t offset, uint32_t size)
{
    LRhiShaderResourceBinding b;
    b.binding = binding;
    b.stage = stage;
    b.type = BufferLoadStore;

    StorageBufferData& d = std::get<StorageBufferData>(b.data);
    d.buf = buf;
    d.offset = offset;
    d.maybeSize = size;
    return b;
}



LRhiShaderResourceBindings::LRhiShaderResourceBindings(std::shared_ptr<LRhiImplementation> rhi) : LRhiResource(rhi)
{
    constexpr int s = BINDING_PREALLOC * LRhiShaderResourceBinding::LAYOUT_DESC_ENTRIES_PER_BINDING;
    m_bindings.reserve(s);
    m_layoutDesc.reserve(s);
}


LRhiResource::Type LRhiShaderResourceBindings::resourceType() const
{
    return ShaderResourceBindings;
}

bool LRhiShaderResourceBindings::isLayoutCompatible(const std::shared_ptr<LRhiShaderResourceBindings> other) const
{
    if (other.get() == this){
        return true;
    }

    if (other == nullptr){
        return false;
    }

    return m_layoutDescHash == other->m_layoutDescHash
           && m_layoutDesc == other->m_layoutDesc;
}


LRhiShaderStage::LRhiShaderStage(Type type, const LShader &shader, LShader::Variant v)
    : m_type(type),
    m_shader(shader),
    m_shaderVariant(v)
{

}

LRhiVertexInputLayout::LRhiVertexInputLayout()
{
    m_bindings.reserve(8);
    m_attributes.reserve(8);
}

LRhiGraphicsPipeline::LRhiGraphicsPipeline(std::shared_ptr<LRhiImplementation> rhi): LRhiResource(rhi)
{
    m_targetBlends.reserve(8);
    m_shaderStages.reserve(4);
}

LRhiResource::Type LRhiGraphicsPipeline::resourceType() const
{
    return GraphicsPipeline;
}

LRhiComputePipeline::LRhiComputePipeline(std::shared_ptr<LRhiImplementation> rhi) : LRhiResource(rhi)
{

}

LRhiResource::Type LRhiComputePipeline::resourceType() const
{
    return ComputePipeline;
}

LRhiCommandBuffer::LRhiCommandBuffer(std::shared_ptr<LRhiImplementation> rhi) : LRhiResource(rhi)
{

}

LRhiResource::Type LRhiCommandBuffer::resourceType() const
{
    return CommandBuffer;
}

void LRhiCommandBuffer::resourceUpdate(std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates)
{
    if (resourceUpdates != nullptr){
        std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
        if(p != nullptr){
            p->resourceUpdate(getPtrT<LRhiCommandBuffer>(), resourceUpdates);
        }
    }
}

void LRhiCommandBuffer::beginPass(std::shared_ptr<LRhiRenderTarget> rt, const LColor &colorClearValue, const LRhiDepthStencilClearValue &depthStencilClearValue, std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates, BeginPassFlags flags)
{
    std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
    if(p != nullptr){
        p->beginPass(getPtrT<LRhiCommandBuffer>(), rt, colorClearValue, depthStencilClearValue, resourceUpdates, flags);
    }
}

void LRhiCommandBuffer::endPass(std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates)
{
    std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
    if(p != nullptr){
        p->endPass(getPtrT<LRhiCommandBuffer>(), resourceUpdates);
    }
}

void LRhiCommandBuffer::setGraphicsPipeline(std::shared_ptr<LRhiGraphicsPipeline> ps)
{
    std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
    if(p != nullptr){
        p->setGraphicsPipeline(getPtrT<LRhiCommandBuffer>(), ps);
    }
}

void LRhiCommandBuffer::setShaderResources(std::shared_ptr<LRhiShaderResourceBindings> srb, int dynamicOffsetCount, const DynamicOffset *dynamicOffsets)
{
    std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
    if(p != nullptr){
        p->setShaderResources(getPtrT<LRhiCommandBuffer>(), srb, dynamicOffsetCount, dynamicOffsets);
    }
}

void LRhiCommandBuffer::setVertexInput(int startBinding, int bindingCount, const VertexInput *bindings, std::shared_ptr<LRhiBuffer> indexBuf, uint32_t indexOffset, IndexFormat indexFormat)
{
    std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
    if(p != nullptr){
        p->setVertexInput(getPtrT<LRhiCommandBuffer>(), startBinding, bindingCount, bindings, indexBuf, indexOffset, indexFormat);
    }
}

void LRhiCommandBuffer::setViewport(const LRhiViewport &viewport)
{
    std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
    if(p != nullptr){
        p->setViewport(getPtrT<LRhiCommandBuffer>(), viewport);
    }
}

void LRhiCommandBuffer::setScissor(const LRhiScissor &scissor)
{
    std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
    if(p != nullptr){
        p->setScissor(getPtrT<LRhiCommandBuffer>(), scissor);
    }
}

void LRhiCommandBuffer::setBlendConstants(const LColor &c)
{
    std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
    if(p != nullptr){
        p->setBlendConstants(getPtrT<LRhiCommandBuffer>(), c);
    }
}

void LRhiCommandBuffer::setStencilRef(uint32_t refValue)
{
    std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
    if(p != nullptr){
        p->setStencilRef(getPtrT<LRhiCommandBuffer>(), refValue);
    }
}

void LRhiCommandBuffer::setShadingRate(const LSize<int> &coarsePixelSize)
{
    std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
    if(p != nullptr){
        p->setShadingRate(getPtrT<LRhiCommandBuffer>(), coarsePixelSize);
    }
}

void LRhiCommandBuffer::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
    if(p != nullptr){
        p->draw(getPtrT<LRhiCommandBuffer>(), vertexCount, instanceCount, firstVertex, firstInstance);
    }
}

void LRhiCommandBuffer::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
    std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
    if(p != nullptr){
        p->drawIndexed(getPtrT<LRhiCommandBuffer>(), indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
    }
}

void LRhiCommandBuffer::debugMarkBegin(const std::string &name)
{
    std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
    if(p != nullptr){
        p->debugMarkBegin(getPtrT<LRhiCommandBuffer>(), name);
    }
}

void LRhiCommandBuffer::debugMarkEnd()
{
    std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
    if(p != nullptr){
        p->debugMarkEnd(getPtrT<LRhiCommandBuffer>());
    }
}

void LRhiCommandBuffer::debugMarkMsg(const std::string &msg)
{
    std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
    if(p != nullptr){
        p->debugMarkMsg(getPtrT<LRhiCommandBuffer>(), msg);
    }
}

void LRhiCommandBuffer::beginComputePass(std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates, BeginPassFlags flags)
{
    std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
    if(p != nullptr){
        p->beginComputePass(getPtrT<LRhiCommandBuffer>(), resourceUpdates, flags);
    }
}

void LRhiCommandBuffer::endComputePass(std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates)
{
    std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
    if(p != nullptr){
        p->endComputePass(getPtrT<LRhiCommandBuffer>(), resourceUpdates);
    }
}

void LRhiCommandBuffer::setComputePipeline(std::shared_ptr<LRhiComputePipeline>  ps)
{
    std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
    if(p != nullptr){
        p->setComputePipeline(getPtrT<LRhiCommandBuffer>(), ps);
    }
}

void LRhiCommandBuffer::dispatch(int x, int y, int z)
{
    std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
    if(p != nullptr){
        p->dispatch(getPtrT<LRhiCommandBuffer>(), x, y, z);
    }
}

const std::shared_ptr<LRhiNativeHandles> LRhiCommandBuffer::nativeHandles()
{
    std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
    if(p != nullptr){
        return p->nativeHandles(getPtrT<LRhiCommandBuffer>());
    }
    return nullptr;
}

void LRhiCommandBuffer::beginExternal()
{
    std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
    if(p != nullptr){
        p->beginExternal(getPtrT<LRhiCommandBuffer>());
    }
}

void LRhiCommandBuffer::endExternal()
{
    std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
    if(p != nullptr){
        p->endExternal(getPtrT<LRhiCommandBuffer>());
    }
}

double LRhiCommandBuffer::lastCompletedGpuTime()
{
    std::shared_ptr<LRhiImplementation> p = m_rhi.lock();
    if(p != nullptr){
        return p->lastCompletedGpuTime(getPtrT<LRhiCommandBuffer>());
    }
    return 0;
}


LRhiSwapChain::LRhiSwapChain(std::shared_ptr<LRhiImplementation> rhi) : LRhiResource(rhi)
{

}

LRhiResource::Type LRhiSwapChain::resourceType() const
{
    return SwapChain;
}

std::shared_ptr<LRhiRenderTarget> LRhiSwapChain::currentFrameRenderTarget(StereoTargetBuffer targetBuffer)
{
    return currentFrameRenderTarget();
}

LRhiSwapChainHdrInfo LRhiSwapChain::hdrInfo()
{
    LRhiSwapChainHdrInfo info;
    info.limitsType = LRhiSwapChainHdrInfo::LuminanceInNits;
    info.limits.luminanceInNits.minLuminance = 0.0f;
    info.limits.luminanceInNits.maxLuminance = 1000.0f;
    info.luminanceBehavior = LRhiSwapChainHdrInfo::SceneReferred;
    info.sdrWhiteLevel = 200.0f;
    return info;
}



LRhiSwapChainRenderTarget::LRhiSwapChainRenderTarget(std::shared_ptr<LRhiImplementation> rhi, std::shared_ptr<LRhiSwapChain> swapchain_)
    : LRhiRenderTarget(rhi), m_swapchain(swapchain_)
{

}

LRhiResource::Type LRhiSwapChainRenderTarget::resourceType() const
{
    return SwapChainRenderTarget;
}

LRhiColorAttachment::LRhiColorAttachment(std::shared_ptr<LRhiTexture> texture): m_texture(texture)
{

}

LRhiColorAttachment::LRhiColorAttachment(std::shared_ptr<LRhiRenderBuffer> renderBuffer): m_renderBuffer(renderBuffer)
{

}


LRhiTextureRenderTargetDescription::LRhiTextureRenderTargetDescription(const LRhiColorAttachment &colorAttachment)
{
     m_colorAttachments.push_back(colorAttachment);
}

LRhiTextureRenderTargetDescription::LRhiTextureRenderTargetDescription(const LRhiColorAttachment &colorAttachment, std::shared_ptr<LRhiRenderBuffer> depthStencilBuffer)
    : m_depthStencilBuffer(depthStencilBuffer)
{
    m_colorAttachments.push_back(colorAttachment);
}

LRhiTextureRenderTargetDescription::LRhiTextureRenderTargetDescription(const LRhiColorAttachment &colorAttachment, std::shared_ptr<LRhiTexture> depthTexture)
    : m_depthTexture(depthTexture)
{
    m_colorAttachments.push_back(colorAttachment);
}

LRhiTextureRenderTarget::LRhiTextureRenderTarget(std::shared_ptr<LRhiImplementation> rhi, const LRhiTextureRenderTargetDescription &desc_, Flags flags_)
    : LRhiRenderTarget(rhi), m_desc(desc_), m_flags(flags_)
{
}


LRhiResource::Type LRhiTextureRenderTarget::resourceType() const
{
    return TextureRenderTarget;
}


LRhiAdapter::~LRhiAdapter()
{

}

LRhi::LRhi() {}

std::shared_ptr<LRhi> LRhi::create(Implementation impl,
                                   std::shared_ptr<LRhiInitParams> params,
                                   Flags flags,
                                   std::shared_ptr<LRhiNativeHandles> importDevice,
                                   std::shared_ptr<LRhiAdapter> adapter)
{
    if(adapter != nullptr && importDevice != nullptr){
         lWarn() << "adapter and importDevice should not both be non-null in LRhi::create()";
    }

    std::shared_ptr<LRhiImplementation> rd = LRhiImplementation::newInstance(impl,params, importDevice);
    if (!rd){
        return nullptr;
    }

    std::shared_ptr<LRhi> r = std::allocate_shared<LRhi>(std::allocator<LRhi>(), LRhi());
    r->m_impl = rd;
    rd->prepareForCreate(r, impl, flags, adapter);
    if (!rd->create(flags)){
        return nullptr;
    }
    return r;
}

LRhi::Implementation LRhi::backend() const
{
    return m_impl->m_implType;
}

const std::string LRhi::backendName() const
{
    return backendName(m_impl->m_implType);
}

std::string LRhi::backendName(Implementation impl)
{
    switch (impl) {
    case Vulkan:
        return "Vulkan";
    case Metal:
        return "Metal";
    case D3D:
        return "D3D12";
    default:{
        return "";
    }
    }
}

LRhiDriverInfo LRhi::driverInfo() const
{
    if(m_impl != nullptr){
        return m_impl->driverInfo();
    }
    return LRhiDriverInfo();
}

static void changeToDynamicUpdate(std::shared_ptr<LRhiResourceUpdateBatch::BufferOp> op, std::shared_ptr<LRhiBuffer> buf, uint32_t offset, uint32_t size, const void *data)
{
    op->type = LRhiResourceUpdateBatch::BufferOp::DynamicUpdate;
    op->buf = buf;
    op->offset = offset;
    const int effectiveSize = size ? size : buf->m_size;
    op->data.assign(reinterpret_cast<const uint8_t *>(data), reinterpret_cast<const uint8_t *>(data) + effectiveSize);
}

static std::shared_ptr<LRhiResourceUpdateBatch::BufferOp> dynamicUpdate(std::shared_ptr<LRhiBuffer> buf, uint32_t offset, uint32_t size, const void *data)
{
    std::shared_ptr<LRhiResourceUpdateBatch::BufferOp>  op = std::make_shared<LRhiResourceUpdateBatch::BufferOp>();
    changeToDynamicUpdate(op, buf, offset, size, data);
    return op;
}

static void changeToDynamicUpdate(std::shared_ptr<LRhiResourceUpdateBatch::BufferOp> op, std::shared_ptr<LRhiBuffer> buf, uint32_t offset, const std::vector<uint8_t>& data)
{
    op->type = LRhiResourceUpdateBatch::BufferOp::DynamicUpdate;
    op->buf = buf;
    op->offset = offset;
    op->data = std::move(data);
}

static std::shared_ptr<LRhiResourceUpdateBatch::BufferOp> dynamicUpdate(std::shared_ptr<LRhiBuffer> buf, uint32_t offset, const std::vector<uint8_t>& data)
{
    std::shared_ptr<LRhiResourceUpdateBatch::BufferOp>  op = std::make_shared<LRhiResourceUpdateBatch::BufferOp>();
    changeToDynamicUpdate(op, buf, offset, std::move(data));
    return op;
}

static void changeToStaticUpload(std::shared_ptr<LRhiResourceUpdateBatch::BufferOp> op, std::shared_ptr<LRhiBuffer> buf, uint32_t offset, uint32_t size, const void *data)
{
    op->type = LRhiResourceUpdateBatch::BufferOp::StaticUpload;
    op->buf = buf;
    op->offset = offset;
    const int effectiveSize = size ? size : buf->m_size;
    op->data.assign(reinterpret_cast<const uint8_t *>(data), reinterpret_cast<const uint8_t *>(data) + effectiveSize);
}

static std::shared_ptr<LRhiResourceUpdateBatch::BufferOp> staticUpload(std::shared_ptr<LRhiBuffer> buf, uint32_t offset, uint32_t size, const void *data)
{
    std::shared_ptr<LRhiResourceUpdateBatch::BufferOp>  op = std::make_shared<LRhiResourceUpdateBatch::BufferOp>();
    changeToStaticUpload(op, buf, offset, size, data);
    return op;
}


static void changeToStaticUpload(std::shared_ptr<LRhiResourceUpdateBatch::BufferOp> op, std::shared_ptr<LRhiBuffer> buf, uint32_t offset, const std::vector<uint8_t>& data)
{
    op->type = LRhiResourceUpdateBatch::BufferOp::StaticUpload;
    op->buf = buf;
    op->offset = offset;
    op->data = data;
}

static std::shared_ptr<LRhiResourceUpdateBatch::BufferOp> staticUpload(std::shared_ptr<LRhiBuffer> buf, uint32_t offset, const std::vector<uint8_t>& data)
{
    std::shared_ptr<LRhiResourceUpdateBatch::BufferOp>  op = std::make_shared<LRhiResourceUpdateBatch::BufferOp>();
    changeToStaticUpload(op, buf, offset, data);
    return op;
}

static std::shared_ptr<LRhiResourceUpdateBatch::BufferOp> read(std::shared_ptr<LRhiBuffer> buf, uint32_t offset, uint32_t size, std::shared_ptr<LRhiReadbackResult> result)
{
    std::shared_ptr<LRhiResourceUpdateBatch::BufferOp>  op = std::make_shared<LRhiResourceUpdateBatch::BufferOp>();
    op->type = LRhiResourceUpdateBatch::BufferOp::Read;
    op->buf = buf;
    op->offset = offset;
    op->readSize = size;
    op->result = result;
    return op;
}

static std::shared_ptr<LRhiResourceUpdateBatch::TextureOp> upload(std::shared_ptr<LRhiTexture> tex, const LRhiTextureUploadDescription &desc)
{
    std::shared_ptr<LRhiResourceUpdateBatch::TextureOp> op = std::make_shared<LRhiResourceUpdateBatch::TextureOp>();
    op->type = LRhiResourceUpdateBatch::TextureOp::Upload;
    op->dst = tex;
    int maxLayer = -1;
    for (auto it = desc.m_entries.cbegin(), itEnd = desc.m_entries.cend(); it != itEnd; ++it) {
        if (it->m_layer > maxLayer){
            maxLayer = it->m_layer;
        }
    }
    op->subresDesc.resize(maxLayer + 1);
    for (auto it = desc.m_entries.cbegin(), itEnd = desc.m_entries.cend(); it != itEnd; ++it){
        op->subresDesc[it->m_layer][it->m_level].push_back(it->m_desc);
    }
    return op;
}

static std::shared_ptr<LRhiResourceUpdateBatch::TextureOp> copy(std::shared_ptr<LRhiTexture> dst, std::shared_ptr<LRhiTexture> src, const LRhiTextureCopyDescription &desc)
{
    std::shared_ptr<LRhiResourceUpdateBatch::TextureOp> op = std::make_shared<LRhiResourceUpdateBatch::TextureOp>();
    op->type = LRhiResourceUpdateBatch::TextureOp::Copy;
    op->dst = dst;
    op->src = src;
    op->desc = desc;
    return op;
}

static std::shared_ptr<LRhiResourceUpdateBatch::TextureOp> read(const LRhiReadbackDescription &rb, std::shared_ptr<LRhiReadbackResult> result)
{
    std::shared_ptr<LRhiResourceUpdateBatch::TextureOp> op = std::make_shared<LRhiResourceUpdateBatch::TextureOp>();
    op->type = LRhiResourceUpdateBatch::TextureOp::Read;
    op->rb = rb;
    op->result = result;
    return op;
}

static std::shared_ptr<LRhiResourceUpdateBatch::TextureOp> genMips(std::shared_ptr<LRhiTexture> tex)
{
    std::shared_ptr<LRhiResourceUpdateBatch::TextureOp> op = std::make_shared<LRhiResourceUpdateBatch::TextureOp>();
    op->type = LRhiResourceUpdateBatch::TextureOp::GenMips;
    op->dst = tex;
    return op;
}


LRhiResourceUpdateBatch::LRhiResourceUpdateBatch(std::shared_ptr<LRhiImplementation> rhi_) : rhi(rhi_)
{
    bufferOps.reserve(BUFFER_OPS_STATIC_ALLOC);
    textureOps.reserve(TEXTURE_OPS_STATIC_ALLOC);
}


LRhiResourceUpdateBatch::~LRhiResourceUpdateBatch()
{

}

void LRhiResourceUpdateBatch::release()
{
    uint32_t bufferLargeAllocTotal = 0;
    for (const std::shared_ptr<BufferOp> &op : std::as_const(bufferOps)) {
        bufferLargeAllocTotal += op->data.capacity();
    }

    activeBufferOpCount = 0;
    activeTextureOpCount = 0;

    const uint64_t mask = 1ULL << uint64_t(poolIndex);
    rhi->resUpdPoolMap &= ~mask;
    poolIndex = -1;

    textureOps.clear();
    if (bufferLargeAllocTotal > 1024 * 1024){
        bufferOps.clear();
    }
}

void LRhiResourceUpdateBatch::merge(std::shared_ptr<LRhiResourceUpdateBatch> other)
{
    int combinedSize = activeBufferOpCount + other->activeBufferOpCount;
    if (bufferOps.size() < combinedSize){
        bufferOps.resize(combinedSize);
    }
    for (int i = activeBufferOpCount; i < combinedSize; ++i){
        bufferOps[i] = other->bufferOps[i - activeBufferOpCount];
    }
    activeBufferOpCount += other->activeBufferOpCount;

    combinedSize = activeTextureOpCount + other->activeTextureOpCount;
    if (textureOps.size() < combinedSize){
        textureOps.resize(combinedSize);
    }
    for (int i = activeTextureOpCount; i < combinedSize; ++i){
        textureOps[i] = other->textureOps[i - activeTextureOpCount];
    }
    activeTextureOpCount += other->activeTextureOpCount;
}

bool LRhiResourceUpdateBatch::hasOptimalCapacity() const
{
    return activeBufferOpCount < BUFFER_OPS_STATIC_ALLOC - 4
           && activeTextureOpCount < TEXTURE_OPS_STATIC_ALLOC - 4;
}

void LRhiResourceUpdateBatch::updateDynamicBuffer(std::shared_ptr<LRhiBuffer> buf, uint32_t offset, uint32_t size, const void *data)
{
    if (size > 0) {
        const int idx = activeBufferOpCount++;
        const int opListSize = bufferOps.size();
        if (idx < opListSize){
            changeToDynamicUpdate(bufferOps[idx], buf, offset, size, data);
        }else{
            bufferOps.push_back(dynamicUpdate(buf, offset, size, data));
        }
    }
}

void LRhiResourceUpdateBatch::updateDynamicBuffer(std::shared_ptr<LRhiBuffer> buf, uint32_t offset, const std::vector<uint8_t>& data)
{
    if (!data.empty()) {
        const int idx = activeBufferOpCount++;
        const int opListSize = bufferOps.size();
        if (idx < opListSize){
            changeToDynamicUpdate(bufferOps[idx], buf, offset, data);
        }else{
            bufferOps.push_back(dynamicUpdate(buf, offset, data));
        }
    }
}

void LRhiResourceUpdateBatch::uploadStaticBuffer(std::shared_ptr<LRhiBuffer> buf, uint32_t offset, uint32_t size, const void *data)
{
    if (size > 0) {
        const int idx = activeBufferOpCount++;
        if (idx < bufferOps.size()){
            changeToStaticUpload(bufferOps[idx], buf, offset, size, data);
        }else{
            bufferOps.push_back(staticUpload(buf, offset, size, data));
        }
    }
}

void LRhiResourceUpdateBatch::uploadStaticBuffer(std::shared_ptr<LRhiBuffer> buf, uint32_t offset, const std::vector<uint8_t>& data)
{
    if (!data.empty()) {
        const int idx = activeBufferOpCount++;
        if (idx < bufferOps.size()){
            changeToStaticUpload(bufferOps[idx], buf, offset, std::move(data));
        }else{
            bufferOps.push_back(staticUpload(buf, offset, std::move(data)));
        }
    }
}

void LRhiResourceUpdateBatch::uploadStaticBuffer(std::shared_ptr<LRhiBuffer> buf, const void *data)
{
    if (buf->m_size > 0) {
        const int idx = activeBufferOpCount++;
        if (idx < bufferOps.size()){
            changeToStaticUpload(bufferOps[idx], buf, 0, 0, data);
        }else{
            bufferOps.push_back(staticUpload(buf, 0, 0, data));
        }
    }
}

void LRhiResourceUpdateBatch::uploadStaticBuffer(std::shared_ptr<LRhiBuffer> buf, const std::vector<uint8_t>& data)
{
    if (buf->m_size > 0 && uint32_t(data.size()) == buf->m_size) {
        const int idx = activeBufferOpCount++;
        if (idx < bufferOps.size()){
            changeToStaticUpload(bufferOps[idx], buf, 0, std::move(data));
        }else{
            bufferOps.push_back(staticUpload(buf, 0, std::move(data)));
        }
    }
}

void LRhiResourceUpdateBatch::readBackBuffer(std::shared_ptr<LRhiBuffer> buf, uint32_t offset, uint32_t size, std::shared_ptr<LRhiReadbackResult> result)
{
    const int idx = activeBufferOpCount++;
    if (idx < bufferOps.size()){
        bufferOps[idx] = read(buf, offset, size, result);
    }else{
        bufferOps.push_back(read(buf, offset, size, result));
    }
}

void LRhiResourceUpdateBatch::uploadTexture(std::shared_ptr<LRhiTexture> tex, const LRhiTextureUploadDescription &desc)
{
    if (!desc.m_entries.empty()) {
        const int idx = activeTextureOpCount++;
        if (idx < textureOps.size()){
            textureOps[idx] = upload(tex, desc);
        }else{
            textureOps.push_back(upload(tex, desc));
        }
    }
}

void LRhiResourceUpdateBatch::uploadTexture(std::shared_ptr<LRhiTexture> tex, const std::shared_ptr<LImage> image)
{
    uploadTexture(tex, LRhiTextureUploadEntry(0, 0, LRhiTextureSubresourceUploadDescription(image)));
}

void LRhiResourceUpdateBatch::copyTexture(std::shared_ptr<LRhiTexture> dst, std::shared_ptr<LRhiTexture> src, const LRhiTextureCopyDescription &desc)
{
    const int idx = activeTextureOpCount++;
    if (idx < textureOps.size()){
        textureOps[idx] = copy(dst, src, desc);
    }else{
        textureOps.push_back(copy(dst, src, desc));
    }
}

void LRhiResourceUpdateBatch::readBackTexture(const LRhiReadbackDescription &rb, std::shared_ptr<LRhiReadbackResult> result)
{
    const int idx = activeTextureOpCount++;
    if (idx < textureOps.size()){
        textureOps[idx] = read(rb, result);
    }else{
        textureOps.push_back(read(rb, result));
    }
}

void LRhiResourceUpdateBatch::generateMips(std::shared_ptr<LRhiTexture> tex)
{
    const int idx = activeTextureOpCount++;
    if (idx < textureOps.size()){
        textureOps[idx] = genMips(tex);
    }else{
        textureOps.push_back(genMips(tex));
    }
}

LK_NAMESPACE_END
