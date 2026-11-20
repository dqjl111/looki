#include "LRhiD3D12.h"

#ifdef max
#  undef max
#endif
#ifdef min
#  undef min
#endif

#include <algorithm>
#include <cstring>

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

    // TODO: D3D12 后端补齐资源登记/注销后恢复
}

bool LD3D12Buffer::create()
{
    std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(m_rhi.lock());
    if (rhiD == nullptr || m_size == 0) {
        return false;
    }
    destroy();

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = m_size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    const bool isStorage = m_usage.testFlag(LRhiBufferUsage::Storage);
    if (isStorage) {
        desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    // 顶点/索引/常量缓冲都放在上传堆：可以直接映射写，省掉一整条
    // default-heap + staging copy + 围栏的路子，先保证能跑起来。
    const D3D12_HEAP_TYPE heapType = isStorage ? D3D12_HEAP_TYPE_DEFAULT
                                               : D3D12_HEAP_TYPE_UPLOAD;
    const D3D12_RESOURCE_STATES state = isStorage
            ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS
            : D3D12_RESOURCE_STATE_GENERIC_READ;

    D3D12MA::Allocation *alloc = nullptr;
    ID3D12Resource *res = nullptr;
    if (FAILED(rhiD->vma.createResource(heapType, &desc, state, nullptr, &alloc,
                                        __uuidof(ID3D12Resource),
                                        reinterpret_cast<void **>(&res)))) {
        lWarn() << "rhi/d3d12: buffer creation failed, size =" << int(m_size);
        return false;
    }

    void *mapped = nullptr;
    if (heapType == D3D12_HEAP_TYPE_UPLOAD) {
        if (FAILED(res->Map(0, nullptr, &mapped))) {
            mapped = nullptr;
        }
    }

    const LD3D12ObjectHandle h = LD3D12Resource::addToPool(&rhiD->resourcePool, res,
                                                           state, alloc, mapped);
    for (int i = 0; i < LD3D12_FRAMES_IN_FLIGHT; ++i) {
        handles[i] = h;
    }
    return true;
}

LRhiBuffer::NativeBuffer LD3D12Buffer::nativeBuffer()
{
    NativeBuffer nb{};
    std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(m_rhi.lock());
    if (rhiD != nullptr) {
        if (LD3D12Resource *r = rhiD->resourcePool.lookupRef(handles[0])) {
            nb.objects[0] = r->resource;
            nb.slotCount = 1;
        }
    }
    return nb;
}

char *LD3D12Buffer::beginFullDynamicBufferUpdateForCurrentFrame()
{
    std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(m_rhi.lock());
    if (rhiD == nullptr) {
        return nullptr;
    }
    LD3D12Resource *r = rhiD->resourcePool.lookupRef(handles[0]);
    return r != nullptr ? static_cast<char *>(r->cpuMapPtr) : nullptr;
}

void LD3D12Buffer::endFullDynamicBufferUpdateForCurrentFrame()
{
    // 上传堆是持久映射，写完即生效，不需要额外提交
}

void LD3D12Buffer::executeHostWritesForFrameSlot(int frameSlot)
{
    (void)frameSlot;
}



LD3D12ShaderResourceBindings::LD3D12ShaderResourceBindings(std::shared_ptr<LRhiImplementation> rhi)
    :LRhiShaderResourceBindings(rhi)
{
}

LRhiD3D12::LRhiD3D12(std::shared_ptr<LRhiD3D12InitParams> params, std::shared_ptr<LRhiD3D12NativeHandles> importDevice)
{
    activeReadbacks.reserve(4);
}

bool LRhiD3D12::create(LFlags<LRhiFlag> flags)
{
    rhiFlags = flags;
    debugLayer = bool(flags & LRhiFlag::EnableDebugMarkers);

    if (dev != nullptr) {
        return true;
    }

    if (debugLayer) {
        ID3D12Debug *dbg = nullptr;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg)))) {
            dbg->EnableDebugLayer();
            dbg->Release();
        }
    }

    UINT factoryFlags = debugLayer ? UINT(DXGI_CREATE_FACTORY_DEBUG) : 0U;
    if (FAILED(CreateDXGIFactory2(factoryFlags, __uuidof(IDXGIFactory2),
                                  reinterpret_cast<void **>(&dxgiFactory)))) {
        factoryFlags = 0;
        if (FAILED(CreateDXGIFactory2(factoryFlags, __uuidof(IDXGIFactory2),
                                      reinterpret_cast<void **>(&dxgiFactory)))) {
            lWarn() << "rhi/d3d12: CreateDXGIFactory2 failed";
            return false;
        }
    }

    const D3D_FEATURE_LEVEL wantedLevel = minimumFeatureLevel != D3D_FEATURE_LEVEL(0)
            ? minimumFeatureLevel
            : D3D_FEATURE_LEVEL_11_0;

    for (UINT i = 0; ; ++i) {
        IDXGIAdapter1 *candidate = nullptr;
        if (dxgiFactory->EnumAdapters1(i, &candidate) == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        DXGI_ADAPTER_DESC1 desc{};
        candidate->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            candidate->Release();
            continue;
        }
        if (SUCCEEDED(D3D12CreateDevice(candidate, wantedLevel,
                                        __uuidof(ID3D12Device), nullptr))) {
            activeAdapter = candidate;
            adapterLuid = desc.AdapterLuid;
            driverInfoStruct.m_deviceName = "D3D12 adapter";
            driverInfoStruct.m_deviceId = desc.DeviceId;
            driverInfoStruct.m_vendorId = desc.VendorId;
            driverInfoStruct.m_deviceType = LRhiDriverInfo::DiscreteDevice;
            break;
        }
        candidate->Release();
    }

    if (activeAdapter == nullptr) {
        lWarn() << "rhi/d3d12: no adapter supports D3D12 feature level 11_0";
        destroy();
        return false;
    }

    ID3D12Device *device0 = nullptr;
    if (FAILED(D3D12CreateDevice(activeAdapter, wantedLevel, __uuidof(ID3D12Device),
                                 reinterpret_cast<void **>(&device0)))) {
        lWarn() << "rhi/d3d12: D3D12CreateDevice failed";
        destroy();
        return false;
    }
    if (FAILED(device0->QueryInterface(__uuidof(ID3D12Device2),
                                       reinterpret_cast<void **>(&dev)))) {
        lWarn() << "rhi/d3d12: ID3D12Device2 unavailable";
        device0->Release();
        destroy();
        return false;
    }
    device0->Release();

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    if (FAILED(dev->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue),
                                       reinterpret_cast<void **>(&cmdQueue)))) {
        lWarn() << "rhi/d3d12: CreateCommandQueue failed";
        destroy();
        return false;
    }

    if (FAILED(dev->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence),
                                reinterpret_cast<void **>(&fullFence)))) {
        lWarn() << "rhi/d3d12: CreateFence failed";
        destroy();
        return false;
    }
    fullFenceCounter = 1;
    fullFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (fullFenceEvent == nullptr) {
        lWarn() << "rhi/d3d12: CreateEvent failed";
        destroy();
        return false;
    }

    for (int i = 0; i < LD3D12_FRAMES_IN_FLIGHT; ++i) {
        if (FAILED(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                               __uuidof(ID3D12CommandAllocator),
                                               reinterpret_cast<void **>(&cmdAllocators[i])))) {
            lWarn() << "rhi/d3d12: CreateCommandAllocator failed";
            destroy();
            return false;
        }
    }

    if (!vma.create(dev, activeAdapter)) {
        lWarn() << "rhi/d3d12: could not create D3D12MA allocator";
        destroy();
        return false;
    }

    rtvPool.create(dev, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, "rtv");
    dsvPool.create(dev, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, "dsv");
    cbvSrvUavPool.create(dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, "cbv_srv_uav");

    resourcePool.create("resources");
    pipelinePool.create("pipelines");
    rootSignaturePool.create("root_signatures");
    releaseQueue.create(&resourcePool, &pipelinePool, &rootSignaturePool);
    barrierGen.create(&resourcePool);

    for (int i = 0; i < LD3D12_FRAMES_IN_FLIGHT; ++i) {
        if (!smallStagingAreas[i].create(this, SMALL_STAGING_AREA_BYTES_PER_FRAME,
                                         D3D12_HEAP_TYPE_UPLOAD)) {
            lWarn() << "rhi/d3d12: staging area creation failed";
            destroy();
            return false;
        }
    }

    if (!shaderVisibleCbvSrvUavHeap.create(dev, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                                           SHADER_VISIBLE_CBV_SRV_UAV_HEAP_PER_FRAME_START_SIZE)) {
        lWarn() << "rhi/d3d12: shader visible heap creation failed";
        destroy();
        return false;
    }
    if (!samplerMgr.create(dev)) {
        lWarn() << "rhi/d3d12: sampler manager creation failed";
        destroy();
        return false;
    }

    return true;
}

void LRhiD3D12::destroy()
{
    if (dev == nullptr && dxgiFactory == nullptr) {
        return;
    }

    for (int i = 0; i < LD3D12_FRAMES_IN_FLIGHT; ++i) {
        smallStagingAreas[i].destroy();
    }
    samplerMgr.destroy();
    shaderVisibleCbvSrvUavHeap.destroy();

    for (const std::shared_ptr<LD3D12SwapChain> &sc : swapchains) {
        if (sc != nullptr) {
            sc->destroy();
        }
    }
    swapchains.clear();
    currentSwapChain.reset();

    releaseQueue.releaseAll();
    resourcePool.destroy();
    pipelinePool.destroy();
    rootSignaturePool.destroy();

    rtvPool.destroy();
    dsvPool.destroy();
    cbvSrvUavPool.destroy();
    vma.destroy();

    for (int i = 0; i < LD3D12_FRAMES_IN_FLIGHT; ++i) {
        if (cmdAllocators[i] != nullptr) {
            cmdAllocators[i]->Release();
            cmdAllocators[i] = nullptr;
        }
    }
    if (fullFence != nullptr) {
        fullFence->Release();
        fullFence = nullptr;
    }
    if (fullFenceEvent != nullptr) {
        CloseHandle(fullFenceEvent);
        fullFenceEvent = nullptr;
    }
    if (cmdQueue != nullptr) {
        cmdQueue->Release();
        cmdQueue = nullptr;
    }
    if (dev != nullptr) {
        dev->Release();
        dev = nullptr;
    }
    if (activeAdapter != nullptr) {
        activeAdapter->Release();
        activeAdapter = nullptr;
    }
    if (dxgiFactory != nullptr) {
        dxgiFactory->Release();
        dxgiFactory = nullptr;
    }
}

std::shared_ptr<LRhiGraphicsPipeline> LRhiD3D12::createGraphicsPipeline()
{
    return makeInit<LD3D12GraphicsPipeline>(getPtrT<LRhiImplementation>());
}


std::shared_ptr<LRhiSwapChain> LRhiD3D12::createSwapChain()
{
    std::shared_ptr<LD3D12SwapChain> sc = makeInit<LD3D12SwapChain>(getPtrT<LRhiImplementation>());
    if (sc != nullptr) {
        swapchains.insert(sc);
    }
    return sc;
}

std::shared_ptr<LRhiShadingRateMap> LRhiD3D12::createShadingRateMap()
{
    return nullptr;
}

std::shared_ptr<LRhiTextureRenderTarget> LRhiD3D12::createTextureRenderTarget(const LRhiTextureRenderTargetDescription &desc, LRhiTextureRenderTarget::Flags flags)
{
    return nullptr;
}

std::shared_ptr<LRhiSampler> LRhiD3D12::createSampler(LRhiSampler::Filter magFilter, LRhiSampler::Filter minFilter, LRhiSampler::Filter mipmapMode, LRhiSampler::AddressMode u, LRhiSampler::AddressMode v, LRhiSampler::AddressMode w)
{
    return makeInit<LD3D12Sampler>(getPtrT<LRhiImplementation>(), magFilter, minFilter,
                                   mipmapMode, u, v, w);
}

std::shared_ptr<LRhiTexture> LRhiD3D12::createTexture(LRhiTexture::Format format, const LSize<int> &pixelSize, int depth, int arraySize, int sampleCount, LRhiTexture::Flags flags)
{
    return makeInit<LD3D12Texture>(getPtrT<LRhiImplementation>(), format, pixelSize,
                                   depth, arraySize, sampleCount, flags);
}

std::shared_ptr<LRhiRenderBuffer> LRhiD3D12::createRenderBuffer(LRhiRenderBuffer::Type type, const LSize<int> &pixelSize, int sampleCount, bool usedWithSwapChainOnly, LRhiTexture::Format backingFormatHint)
{
    return makeInit<LD3D12RenderBuffer>(getPtrT<LRhiImplementation>(), type, pixelSize,
                                        sampleCount, usedWithSwapChainOnly, backingFormatHint);
}

std::shared_ptr<LRhiBuffer> LRhiD3D12::createBuffer(LRhiBuffer::Type type, LRhiBuffer::UsageFlags usage, uint32_t size)
{
    return makeInit<LD3D12Buffer>(getPtrT<LRhiImplementation>(), type, usage, size);
}

std::shared_ptr<LRhiShaderResourceBindings> LRhiD3D12::createShaderResourceBindings()
{
    return makeInit<LD3D12ShaderResourceBindings>(getPtrT<LRhiImplementation>());
}

std::shared_ptr<LRhiComputePipeline> LRhiD3D12::createComputePipeline()
{
    return nullptr;
}


LRhi::FrameOpResult LRhiD3D12::beginFrame(std::shared_ptr<LRhiSwapChain> swapChain, LRhi::BeginFrameFlags flags)
{
    (void)flags;
    std::shared_ptr<LD3D12SwapChain> sc = std::dynamic_pointer_cast<LD3D12SwapChain>(swapChain);
    if (sc == nullptr || !sc->createOrResize()) {
        return LRhi::FrameOpError;
    }

    currentSwapChain = sc;
    const int slot = sc->currentFrameSlot;
    shaderVisibleCbvSrvUavHeap.resetFrame(slot);
    samplerMgr.currentFrameSlot = slot;
    samplerMgr.shaderVisibleSamplerHeap.resetFrame(slot);
    sc->waitCommandCompletionForFrameSlot(slot);
    releaseQueue.executeDeferredReleases(slot);
    releaseQueue.activatePendingDeferredReleaseRequests(slot);

    if (cmdAllocators[slot] == nullptr || sc->frameRes[slot].cmdList == nullptr) {
        return LRhi::FrameOpError;
    }
    if (FAILED(cmdAllocators[slot]->Reset()) ||
        FAILED(sc->frameRes[slot].cmdList->Reset(cmdAllocators[slot], nullptr))) {
        lWarn() << "rhi/d3d12: failed to reset command allocator/list";
        return LRhi::FrameOpError;
    }

    sc->cbWrapper.cmdList = sc->frameRes[slot].cmdList;
    sc->cbWrapper.resetState();
    sc->cbWrapper.nativeHandlesStruct.commandList = sc->frameRes[slot].cmdList;

    sc->currentBackBufferIndex = sc->swapChain->GetCurrentBackBufferIndex();
    sc->transitionBackBuffer(sc->frameRes[slot].cmdList, D3D12_RESOURCE_STATE_RENDER_TARGET);
    return LRhi::FrameOpSuccess;
}

LRhi::FrameOpResult LRhiD3D12::endFrame(std::shared_ptr<LRhiSwapChain> swapChain, LRhi::EndFrameFlags flags)
{
    std::shared_ptr<LD3D12SwapChain> sc = std::dynamic_pointer_cast<LD3D12SwapChain>(swapChain);
    if (sc == nullptr || sc->swapChain == nullptr) {
        return LRhi::FrameOpError;
    }

    const int slot = sc->currentFrameSlot;
    ID3D12GraphicsCommandList *cmdList = sc->frameRes[slot].cmdList;
    if (cmdList != nullptr) {
        sc->transitionBackBuffer(cmdList, D3D12_RESOURCE_STATE_PRESENT);
        if (FAILED(cmdList->Close())) {
            return LRhi::FrameOpError;
        }
        ID3D12CommandList *lists[] = { cmdList };
        cmdQueue->ExecuteCommandLists(1, lists);
        sc->addCommandCompletionSignalForCurrentFrameSlot();
        sc->cbWrapper.cmdList = nullptr;
    }

    sc->currentFrameSlot = (slot + 1) % LD3D12_FRAMES_IN_FLIGHT;

    if (flags & LRhiEndFrameFlag::SkipPresent) {
        return LRhi::FrameOpSuccess;
    }
    const HRESULT hr = sc->swapChain->Present(sc->swapInterval, 0);
    if (hr == DXGI_STATUS_OCCLUDED) {
        return LRhi::FrameOpSuccess;
    }
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        deviceLost = true;
        return LRhi::FrameOpDeviceLost;
    }
    return SUCCEEDED(hr) ? LRhi::FrameOpSuccess : LRhi::FrameOpError;
}

LRhi::FrameOpResult LRhiD3D12::beginOffscreenFrame(std::shared_ptr<LRhiCommandBuffer> *cb, LRhi::BeginFrameFlags flags)
{
    return LRhi::FrameOpResult();
}

LRhi::FrameOpResult LRhiD3D12::endOffscreenFrame(LRhi::EndFrameFlags flags)
{
    return LRhi::FrameOpResult();
}

LRhi::FrameOpResult LRhiD3D12::finish()
{
    if (cmdQueue == nullptr || fullFence == nullptr || fullFenceEvent == nullptr) {
        return LRhi::FrameOpError;
    }
    const UINT64 fenceValue = fullFenceCounter + 1;
    if (FAILED(cmdQueue->Signal(fullFence, fenceValue))) {
        return LRhi::FrameOpError;
    }
    fullFenceCounter = fenceValue;
    if (fullFence->GetCompletedValue() < fenceValue) {
        if (FAILED(fullFence->SetEventOnCompletion(fenceValue, fullFenceEvent))) {
            return LRhi::FrameOpError;
        }
        WaitForSingleObject(fullFenceEvent, INFINITE);
    }
    return LRhi::FrameOpSuccess;
}

void LRhiD3D12::resourceUpdate(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates)
{
    if (resourceUpdates == nullptr) {
        return;
    }
    LD3D12CommandBuffer *cbD = dynamic_cast<LD3D12CommandBuffer *>(cb.get());
    if (cbD == nullptr || cbD->cmdList == nullptr) {
        return;
    }

    for (const std::shared_ptr<LRhiResourceUpdateBatch::TextureOp> &op : resourceUpdates->textureOps) {
        if (op == nullptr || op->type != LRhiResourceUpdateBatch::TextureOp::Upload ||
            op->dst == nullptr || op->subresDesc.empty()) {
            continue;
        }
        std::shared_ptr<LD3D12Texture> tex = std::dynamic_pointer_cast<LD3D12Texture>(op->dst);
        if (tex == nullptr || tex->handle.isNull()) {
            continue;
        }
        LD3D12Resource *dstResource = resourcePool.lookupRef(tex->handle);
        if (dstResource == nullptr) {
            continue;
        }
        D3D12_RESOURCE_DESC resDesc = dstResource->resource->GetDesc();

        for (size_t layer = 0; layer < op->subresDesc.size(); ++layer) {
            for (int level = 0; level < LRhi::MAX_MIP_LEVELS; ++level) {
                for (const LRhiTextureSubresourceUploadDescription &src : op->subresDesc[layer][level]) {
                    if (src.m_data.empty()) {
                        continue;
                    }
                    const UINT subres = UINT(level) + UINT(layer) * tex->mipLevelCount;
                    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
                    UINT rows = 0;
                    UINT64 rowBytes = 0;
                    UINT64 totalBytes = 0;
                    dev->GetCopyableFootprints(&resDesc, subres, 1, 0,
                                               &footprint, &rows, &rowBytes, &totalBytes);
                    if (footprint.Footprint.RowPitch == 0 || rows == 0) {
                        continue;
                    }

                    const UINT64 uploadBytes = UINT64(footprint.Footprint.RowPitch) * rows;
                    D3D12_RESOURCE_DESC uploadDesc{};
                    uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
                    uploadDesc.Width = uploadBytes;
                    uploadDesc.Height = 1;
                    uploadDesc.DepthOrArraySize = 1;
                    uploadDesc.MipLevels = 1;
                    uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
                    uploadDesc.SampleDesc.Count = 1;
                    uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

                    D3D12MA::Allocation *alloc = nullptr;
                    ID3D12Resource *upload = nullptr;
                    if (FAILED(vma.createResource(D3D12_HEAP_TYPE_UPLOAD, &uploadDesc,
                                                  D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                  &alloc, __uuidof(ID3D12Resource),
                                                  reinterpret_cast<void **>(&upload)))) {
                        lWarn() << "rhi/d3d12: texture upload allocation failed";
                        continue;
                    }

                    uint8_t *mapped = nullptr;
                    if (FAILED(upload->Map(0, nullptr, reinterpret_cast<void **>(&mapped)))) {
                        upload->Release();
                        alloc->Release();
                        continue;
                    }
                    const uint32_t srcStride = src.m_dataStride != 0 ? src.m_dataStride : UINT(rowBytes);
                    const UINT bytesPerRow = UINT((std::min)(UINT64(rowBytes), UINT64(srcStride)));
                    for (UINT row = 0; row < rows; ++row) {
                        const size_t srcOffset = size_t(src.m_sourceTopLeft.m_y + int(row)) * srcStride
                                + size_t(std::max(0, src.m_sourceTopLeft.m_x)) * bytesPerRow;
                        if (srcOffset >= src.m_data.size()) {
                            break;
                        }
                        uint8_t *dst = mapped + UINT64(row) * footprint.Footprint.RowPitch;
                        std::memcpy(dst, src.m_data.data() + srcOffset,
                                    (std::min)(size_t(bytesPerRow), src.m_data.size() - srcOffset));
                    }
                    upload->Unmap(0, nullptr);

                    D3D12_TEXTURE_COPY_LOCATION srcLoc{};
                    srcLoc.pResource = upload;
                    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                    srcLoc.PlacedFootprint = footprint;
                    D3D12_TEXTURE_COPY_LOCATION dstLoc{};
                    dstLoc.pResource = dstResource->resource;
                    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                    dstLoc.SubresourceIndex = subres;
                    cbD->cmdList->CopyTextureRegion(
                                &dstLoc,
                                UINT(std::max(0, src.m_destinationTopLeft.m_x)),
                                UINT(std::max(0, src.m_destinationTopLeft.m_y)), 0,
                                &srcLoc, nullptr);
                    upload->Release();
                    alloc->Release();
                }
            }
        }

        barrierGen.addTransitionBarrier(tex->handle, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        barrierGen.enqueueBufferedTransitionBarriers(cbD);
    }
}

void LRhiD3D12::beginPass(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiRenderTarget> rt, const LColor &colorClearValue, const LRhiDepthStencilClearValue &depthStencilClearValue, std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates, LRhiCommandBuffer::BeginPassFlags flags)
{
    (void)resourceUpdates;
    (void)flags;
    LD3D12CommandBuffer *cbD = dynamic_cast<LD3D12CommandBuffer *>(cb.get());
    std::shared_ptr<LD3D12SwapChainRenderTarget> rtSc =
            std::dynamic_pointer_cast<LD3D12SwapChainRenderTarget>(rt);
    if (cbD == nullptr || cbD->cmdList == nullptr || rtSc == nullptr) {
        return;
    }

    std::shared_ptr<LD3D12SwapChain> sc =
            std::dynamic_pointer_cast<LD3D12SwapChain>(rtSc->swapChain());
    if (sc == nullptr) {
        return;
    }

    rtSc->d.pixelSize = sc->pixelSize;
    rtSc->d.sampleCount = 1;
    rtSc->d.colorAttCount = 1;
    rtSc->d.dsAttCount = sc->ds != nullptr ? 1 : 0;
    rtSc->d.rtv[0] = sc->rtvs[sc->currentBackBufferIndex].cpuHandle;
    rtSc->d.dsv = sc->ds != nullptr ? sc->ds->dsv.cpuHandle : D3D12_CPU_DESCRIPTOR_HANDLE{};
    rtSc->m_renderPassDesc = sc->m_renderPassDesc;

    D3D12_CPU_DESCRIPTOR_HANDLE *dsvPtr = rtSc->d.dsAttCount > 0 ? &rtSc->d.dsv : nullptr;
    cbD->cmdList->OMSetRenderTargets(1, &rtSc->d.rtv[0], FALSE, dsvPtr);

    const float clearColor[4] = {
        colorClearValue.m_red / 255.0f,
        colorClearValue.m_green / 255.0f,
        colorClearValue.m_blue / 255.0f,
        colorClearValue.m_alpha / 255.0f
    };
    cbD->cmdList->ClearRenderTargetView(rtSc->d.rtv[0], clearColor, 0, nullptr);
    if (dsvPtr != nullptr) {
        cbD->cmdList->ClearDepthStencilView(
                    *dsvPtr, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                    float(depthStencilClearValue.m_depthClearValue),
                    UINT8(depthStencilClearValue.m_stencilClearValue), 0, nullptr);
    }

    const LSize<int> size = sc->pixelSize;
    D3D12_VIEWPORT viewport{};
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = float(size.m_x);
    viewport.Height = float(size.m_y);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    cbD->cmdList->RSSetViewports(1, &viewport);

    D3D12_RECT scissor{ 0, 0, LONG(size.m_x), LONG(size.m_y) };
    cbD->cmdList->RSSetScissorRects(1, &scissor);

    cbD->resetPerPassState();
    cbD->recordingPass = LD3D12CommandBuffer::RenderPass;
    cbD->currentTarget = rt.get();
}

void LRhiD3D12::endPass(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates)
{
    (void)resourceUpdates;
    LD3D12CommandBuffer *cbD = dynamic_cast<LD3D12CommandBuffer *>(cb.get());
    if (cbD == nullptr) {
        return;
    }
    cbD->resetPerPassState();
    cbD->recordingPass = LD3D12CommandBuffer::NoPass;
    cbD->currentTarget = nullptr;
}

void LRhiD3D12::setGraphicsPipeline(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiGraphicsPipeline> ps)
{
    LD3D12CommandBuffer *cbD = dynamic_cast<LD3D12CommandBuffer *>(cb.get());
    std::shared_ptr<LD3D12GraphicsPipeline> psD = std::dynamic_pointer_cast<LD3D12GraphicsPipeline>(ps);
    if (cbD == nullptr || cbD->cmdList == nullptr || psD == nullptr || psD->handle.isNull()) {
        return;
    }
    std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(getPtrT<LRhiImplementation>());
    ID3D12PipelineState *pso = rhiD->pipelinePool.lookup(psD->handle).pso;
    ID3D12RootSignature *rootSig = rhiD->rootSignaturePool.lookup(psD->rootSigHandle).rootSig;
    if (pso == nullptr || rootSig == nullptr) {
        return;
    }
    cbD->cmdList->SetPipelineState(pso);
    cbD->cmdList->SetGraphicsRootSignature(rootSig);
    cbD->cmdList->IASetPrimitiveTopology(psD->topology);
    cbD->currentGraphicsPipeline = psD.get();
    cbD->currentPipelineGeneration = psD->generation;
}

void LRhiD3D12::setShaderResources(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiShaderResourceBindings> srb, int dynamicOffsetCount, const LRhiCommandBuffer::DynamicOffset *dynamicOffsets)
{
    (void)dynamicOffsetCount;
    (void)dynamicOffsets;
    LD3D12CommandBuffer *cbD = dynamic_cast<LD3D12CommandBuffer *>(cb.get());
    std::shared_ptr<LD3D12ShaderResourceBindings> srbD =
            std::dynamic_pointer_cast<LD3D12ShaderResourceBindings>(srb);
    if (cbD == nullptr || cbD->cmdList == nullptr || srbD == nullptr || srbD->rootSigHandle.isNull()) {
        return;
    }
    srbD->updateResources();

    const int slot = currentSwapChain != nullptr ? currentSwapChain->currentFrameSlot : 0;
    ID3D12DescriptorHeap *heaps[] = {
        shaderVisibleCbvSrvUavHeap.heap.heap,
        samplerMgr.shaderVisibleSamplerHeap.heap.heap
    };
    UINT heapCount = 0;
    for (ID3D12DescriptorHeap *heap : heaps) {
        if (heap != nullptr) {
            heaps[heapCount++] = heap;
        }
    }
    if (heapCount > 0) {
        cbD->cmdList->SetDescriptorHeaps(heapCount, heaps);
    }
    if (srbD->srvCount > 0 && srbD->srvTable[slot].gpuHandle.ptr != 0) {
        cbD->cmdList->SetGraphicsRootDescriptorTable(0, srbD->srvTable[slot].gpuHandle);
    }
    if (srbD->samplerCount > 0 && srbD->samplerTable[slot].gpuHandle.ptr != 0) {
        cbD->cmdList->SetGraphicsRootDescriptorTable(1, srbD->samplerTable[slot].gpuHandle);
    }
}

void LRhiD3D12::setVertexInput(std::shared_ptr<LRhiCommandBuffer> cb, int startBinding, int bindingCount, const LRhiCommandBuffer::VertexInput *bindings, std::shared_ptr<LRhiBuffer> indexBuf, uint32_t indexOffset, LRhiCommandBuffer::IndexFormat indexFormat)
{
    LD3D12CommandBuffer *cbD = dynamic_cast<LD3D12CommandBuffer *>(cb.get());
    std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(getPtrT<LRhiImplementation>());
    if (cbD == nullptr || cbD->cmdList == nullptr || rhiD == nullptr) {
        return;
    }
    for (int i = 0; i < bindingCount; ++i) {
        std::shared_ptr<LD3D12Buffer> buf = std::dynamic_pointer_cast<LD3D12Buffer>(bindings[i].first);
        if (buf == nullptr || buf->handles[0].isNull()) {
            continue;
        }
        LD3D12Resource *r = rhiD->resourcePool.lookupRef(buf->handles[0]);
        if (r == nullptr) {
            continue;
        }
        const int slot = startBinding + i;
        if (slot < 0 || slot >= D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT) {
            continue;
        }
        uint32_t stride = 0;
        if (cbD->currentGraphicsPipeline != nullptr &&
            slot < int(cbD->currentGraphicsPipeline->m_vertexInputLayout.m_bindings.size())) {
            stride = cbD->currentGraphicsPipeline->m_vertexInputLayout.m_bindings[slot].m_stride;
        }
        D3D12_VERTEX_BUFFER_VIEW view{};
        view.BufferLocation = r->resource->GetGPUVirtualAddress() + bindings[i].second;
        view.SizeInBytes = UINT(buf->m_size - std::min(buf->m_size, bindings[i].second));
        view.StrideInBytes = stride;
        cbD->cmdList->IASetVertexBuffers(UINT(slot), 1, &view);
        cbD->currentVertexBuffers[slot] = buf->handles[0];
        cbD->currentVertexOffsets[slot] = bindings[i].second;
    }
    if (indexBuf != nullptr) {
        std::shared_ptr<LD3D12Buffer> ib = std::dynamic_pointer_cast<LD3D12Buffer>(indexBuf);
        if (ib != nullptr && !ib->handles[0].isNull()) {
            LD3D12Resource *r = rhiD->resourcePool.lookupRef(ib->handles[0]);
            if (r != nullptr) {
                D3D12_INDEX_BUFFER_VIEW view{};
                view.BufferLocation = r->resource->GetGPUVirtualAddress() + indexOffset;
                view.SizeInBytes = UINT(ib->m_size - std::min(ib->m_size, indexOffset));
                view.Format = indexFormat == LRhiCommandBuffer::IndexUInt32
                        ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
                cbD->cmdList->IASetIndexBuffer(&view);
                cbD->currentIndexBuffer = ib->handles[0];
                cbD->currentIndexOffset = indexOffset;
                cbD->currentIndexFormat = view.Format;
            }
        }
    }
}

void LRhiD3D12::setViewport(std::shared_ptr<LRhiCommandBuffer> cb, const LRhiViewport &viewport)
{
    LD3D12CommandBuffer *cbD = dynamic_cast<LD3D12CommandBuffer *>(cb.get());
    if (cbD == nullptr || cbD->cmdList == nullptr) {
        return;
    }
    D3D12_VIEWPORT vp{};
    vp.TopLeftX = float(viewport.m_rect.m_x);
    vp.TopLeftY = float(viewport.m_rect.m_y);
    vp.Width = float(viewport.m_rect.m_width);
    vp.Height = float(viewport.m_rect.m_height);
    vp.MinDepth = float(viewport.m_minDepth);
    vp.MaxDepth = float(viewport.m_maxDepth);
    cbD->cmdList->RSSetViewports(1, &vp);
}

void LRhiD3D12::setScissor(std::shared_ptr<LRhiCommandBuffer> cb, const LRhiScissor &scissor)
{
    LD3D12CommandBuffer *cbD = dynamic_cast<LD3D12CommandBuffer *>(cb.get());
    if (cbD == nullptr || cbD->cmdList == nullptr) {
        return;
    }
    D3D12_RECT r{ LONG(scissor.m_x), LONG(scissor.m_y),
                  LONG(scissor.m_x + scissor.m_width),
                  LONG(scissor.m_y + scissor.m_height) };
    cbD->cmdList->RSSetScissorRects(1, &r);
}

void LRhiD3D12::setBlendConstants(std::shared_ptr<LRhiCommandBuffer> cb, const LColor &c)
{

}

void LRhiD3D12::setStencilRef(std::shared_ptr<LRhiCommandBuffer> cb, uint32_t refValue)
{

}

void LRhiD3D12::setShadingRate(std::shared_ptr<LRhiCommandBuffer> cb, const LSize<int> &coarsePixelSize)
{

}

void LRhiD3D12::draw(std::shared_ptr<LRhiCommandBuffer> cb, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{

}

void LRhiD3D12::drawIndexed(std::shared_ptr<LRhiCommandBuffer> cb, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
    LD3D12CommandBuffer *cbD = dynamic_cast<LD3D12CommandBuffer *>(cb.get());
    if (cbD == nullptr || cbD->cmdList == nullptr || cbD->currentGraphicsPipeline == nullptr) {
        return;
    }
    cbD->cmdList->DrawIndexedInstanced(indexCount, instanceCount, firstIndex,
                                       vertexOffset, firstInstance);
}

void LRhiD3D12::debugMarkBegin(std::shared_ptr<LRhiCommandBuffer> cb, const std::string &name)
{

}

void LRhiD3D12::debugMarkEnd(std::shared_ptr<LRhiCommandBuffer> cb)
{

}

void LRhiD3D12::debugMarkMsg(std::shared_ptr<LRhiCommandBuffer> cb, const std::string &msg)
{

}

void LRhiD3D12::beginComputePass(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates, LRhiCommandBuffer::BeginPassFlags flags)
{

}

void LRhiD3D12::endComputePass(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates)
{

}

void LRhiD3D12::setComputePipeline(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiComputePipeline> ps)
{

}

void LRhiD3D12::dispatch(std::shared_ptr<LRhiCommandBuffer> cb, int x, int y, int z)
{

}

const std::shared_ptr<LRhiNativeHandles> LRhiD3D12::nativeHandles(std::shared_ptr<LRhiCommandBuffer> cb)
{
    LD3D12CommandBuffer *cbD = dynamic_cast<LD3D12CommandBuffer *>(cb.get());
    if (cbD == nullptr) {
        return nullptr;
    }
    auto h = std::make_shared<LRhiD3D12CommandBufferNativeHandles>();
    h->commandList = cbD->cmdList;
    return h;
}


void LRhiD3D12::beginExternal(std::shared_ptr<LRhiCommandBuffer> cb)
{

}

void LRhiD3D12::endExternal(std::shared_ptr<LRhiCommandBuffer> cb)
{

}

double LRhiD3D12::lastCompletedGpuTime(std::shared_ptr<LRhiCommandBuffer> cb)
{
    return 0;
}

std::list<int> LRhiD3D12::supportedSampleCounts() const
{
    return std::list<int>();
}

int LRhiD3D12::ubufAlignment() const
{
    return 256;   // D3D12 常量缓冲视图要求 256 字节对齐
}

std::list<LSize<int> > LRhiD3D12::supportedShadingRates(int sampleCount) const
{
    return std::list<LSize<int> >();
}

bool LRhiD3D12::isYUpInFramebuffer() const
{
    return false;
}

bool LRhiD3D12::isYUpInNDC() const
{
    return true;   // D3D 的 NDC Y 轴向上
}

bool LRhiD3D12::isClipDepthZeroToOne() const
{
    return true;   // D3D 深度范围 0..1
}

LMatrix4x4 LRhiD3D12::clipSpaceCorrMatrix() const
{
    return LMatrix4x4();
}

bool LRhiD3D12::isTextureFormatSupported(LRhiTexture::Format format, LRhiTexture::Flags flags) const
{
    return false;
}

bool LRhiD3D12::isFeatureSupported(LRhi::Feature feature) const
{
    return false;
}

int LRhiD3D12::resourceLimit(LRhi::ResourceLimit limit) const
{
    return 0;
}

const std::shared_ptr<LRhiNativeHandles> LRhiD3D12::nativeHandles()
{
    auto h = std::make_shared<LRhiD3D12NativeHandles>();
    h->dev = dev;
    h->commandQueue = cmdQueue;
    h->adapterLuidLow = uint32_t(adapterLuid.LowPart);
    h->adapterLuidHigh = int32_t(adapterLuid.HighPart);
    return h;
}

LRhiDriverInfo LRhiD3D12::driverInfo() const
{
    return LRhiDriverInfo();
}

LRhiStats LRhiD3D12::statistics()
{
    return LRhiStats();
}

bool LRhiD3D12::makeThreadLocalNativeContextCurrent()
{
    return false;
}

void LRhiD3D12::setQueueSubmitParams(std::shared_ptr<LRhiNativeHandles> params)
{

}

void LRhiD3D12::releaseCachedResources()
{

}

bool LRhiD3D12::isDeviceLost() const
{
    return false;
}

std::vector<uint8_t> LRhiD3D12::pipelineCacheData()
{
    return std::vector<uint8_t>();
}

void LRhiD3D12::setPipelineCacheData(const std::vector<uint8_t> &data)
{

}




// ---------------------------------------------------------------------------
// 资源屏障
// ---------------------------------------------------------------------------
void LD3D12ResourceBarrierGenerator::addTransitionBarrier(const LD3D12ObjectHandle &resourceHandle,
                                                          D3D12_RESOURCE_STATES stateAfter)
{
    if (resourcePool == nullptr || resourceHandle.isNull()) {
        return;
    }
    LD3D12Resource *r = resourcePool->lookupRef(resourceHandle);
    if (r == nullptr || r->state == stateAfter) {
        return;
    }
    TransitionResourceBarrier barrier{ resourceHandle, r->state, stateAfter };
    transitionResourceBarriers.push_back(barrier);
}

void LD3D12ResourceBarrierGenerator::enqueueBufferedTransitionBarriers(LD3D12CommandBuffer *cbD)
{
    if (cbD == nullptr || cbD->cmdList == nullptr || transitionResourceBarriers.empty()) {
        return;
    }
    std::vector<D3D12_RESOURCE_BARRIER> barriers;
    barriers.reserve(transitionResourceBarriers.size());
    for (const TransitionResourceBarrier &b : transitionResourceBarriers) {
        LD3D12Resource *r = resourcePool != nullptr ? resourcePool->lookupRef(b.resourceHandle) : nullptr;
        if (r == nullptr) {
            continue;
        }
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = r->resource;
        barrier.Transition.StateBefore = b.stateBefore;
        barrier.Transition.StateAfter = b.stateAfter;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers.push_back(barrier);
        r->state = b.stateAfter;
    }
    transitionResourceBarriers.clear();
    if (!barriers.empty()) {
        cbD->cmdList->ResourceBarrier(UINT(barriers.size()), barriers.data());
    }
}

// ---------------------------------------------------------------------------
// 描述符堆
// ---------------------------------------------------------------------------
bool LD3D12DescriptorHeap::create(ID3D12Device *device,
                             uint32_t descriptorCount,
                             D3D12_DESCRIPTOR_HEAP_TYPE heapType_,
                             D3D12_DESCRIPTOR_HEAP_FLAGS heapFlags_)
{
    destroy();
    if (device == nullptr || descriptorCount == 0) {
        return false;
    }
    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.NumDescriptors = descriptorCount;
    desc.Type = heapType_;
    desc.Flags = heapFlags_;
    if (FAILED(device->CreateDescriptorHeap(&desc, __uuidof(ID3D12DescriptorHeap),
                                            reinterpret_cast<void **>(&heap)))) {
        return false;
    }
    capacity = descriptorCount;
    descriptorByteSize = device->GetDescriptorHandleIncrementSize(heapType_);
    heapType = heapType_;
    heapFlags = heapFlags_;
    heapStart.cpuHandle = heap->GetCPUDescriptorHandleForHeapStart();
    if (heapFlags_ & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) {
        heapStart.gpuHandle = heap->GetGPUDescriptorHandleForHeapStart();
    }
    head = 0;
    return true;
}

void LD3D12DescriptorHeap::createWithExisting(const LD3D12DescriptorHeap &other,
                                         uint32_t offsetInDescriptors,
                                         uint32_t descriptorCount)
{
    destroy();
    heap = other.heap;
    if (heap != nullptr) {
        heap->AddRef();
    }
    capacity = descriptorCount;
    descriptorByteSize = other.descriptorByteSize;
    heapType = other.heapType;
    heapFlags = other.heapFlags;
    heapStart = other.incremented(other.heapStart, offsetInDescriptors);
    head = 0;
}

void LD3D12DescriptorHeap::destroy()
{
    if (heap != nullptr) {
        heap->Release();
        heap = nullptr;
    }
    capacity = 0;
    head = 0;
    heapStart = {};
    descriptorByteSize = 0;
}

void LD3D12DescriptorHeap::destroyWithDeferredRelease(LD3D12ReleaseQueue *releaseQueue)
{
    if (heap == nullptr) {
        destroy();
        return;
    }
    if (releaseQueue != nullptr) {
        releaseQueue->deferredReleaseDescriptorHeap(heap);
    } else {
        heap->Release();
    }
    heap = nullptr;
    capacity = 0;
    head = 0;
    heapStart = {};
    descriptorByteSize = 0;
}

LD3D12Descriptor LD3D12DescriptorHeap::get(uint32_t count)
{
    if (count == 0 || head + count > capacity) {
        return {};
    }
    const LD3D12Descriptor result = incremented(heapStart, head);
    head += count;
    return result;
}

LD3D12Descriptor LD3D12DescriptorHeap::at(uint32_t index) const
{
    if (index >= capacity) {
        return {};
    }
    return incremented(heapStart, index);
}

// ---------------------------------------------------------------------------
// CPU 描述符池
// ---------------------------------------------------------------------------
bool LD3D12CpuDescriptorPool::create(ID3D12Device *device_,
                                D3D12_DESCRIPTOR_HEAP_TYPE heapType_,
                                const char *debugName_)
{
    destroy();
    device = device_;
    descriptorByteSize = device_ != nullptr
            ? device_->GetDescriptorHandleIncrementSize(heapType_)
            : 0;
    heapType = heapType_;
    debugName = debugName_;
    return device != nullptr;
}

void LD3D12CpuDescriptorPool::destroy()
{
    for (HeapWithMap &h : heaps) {
        h.heap.destroy();
    }
    heaps.clear();
}

LD3D12Descriptor LD3D12CpuDescriptorPool::allocate(uint32_t count)
{
    if (device == nullptr || count == 0) {
        return {};
    }
    for (HeapWithMap &h : heaps) {
        const uint32_t size = uint32_t(h.map.size());
        for (uint32_t i = 0; i + count <= size; ++i) {
            bool free = true;
            for (uint32_t k = 0; k < count; ++k) {
                if (h.map[i + k]) {
                    free = false;
                    break;
                }
            }
            if (!free) {
                continue;
            }
            for (uint32_t k = 0; k < count; ++k) {
                h.map[i + k] = true;
            }
            return h.heap.at(i);
        }
    }

    LD3D12DescriptorHeap newHeap;
    if (!newHeap.create(device, DESCRIPTORS_PER_HEAP, heapType,
                        D3D12_DESCRIPTOR_HEAP_FLAG_NONE)) {
        return {};
    }
    HeapWithMap h = HeapWithMap::init(newHeap, DESCRIPTORS_PER_HEAP);
    for (uint32_t k = 0; k < count; ++k) {
        h.map[k] = true;
    }
    const LD3D12Descriptor result = h.heap.at(0);
    heaps.push_back(std::move(h));
    return result;
}

void LD3D12CpuDescriptorPool::release(const LD3D12Descriptor &descriptor, uint32_t count)
{
    if (descriptor.cpuHandle.ptr == 0 || count == 0 || descriptorByteSize == 0) {
        return;
    }
    for (HeapWithMap &h : heaps) {
        const SIZE_T begin = h.heap.heapStart.cpuHandle.ptr;
        const SIZE_T end = begin + SIZE_T(h.heap.capacity) * h.heap.descriptorByteSize;
        if (descriptor.cpuHandle.ptr < begin || descriptor.cpuHandle.ptr >= end) {
            continue;
        }
        const uint32_t index = uint32_t((descriptor.cpuHandle.ptr - begin)
                                        / h.heap.descriptorByteSize);
        for (uint32_t k = 0; k < count && index + k < h.map.size(); ++k) {
            h.map[index + k] = false;
        }
        return;
    }
}

// ---------------------------------------------------------------------------
// 延迟释放队列
// ---------------------------------------------------------------------------
void LD3D12ReleaseQueue::deferredReleaseResource(const LD3D12ObjectHandle &handle)
{
    if (handle.isNull()) {
        return;
    }
    DeferredReleaseEntry entry;
    entry.type = DeferredReleaseEntry::Resource;
    entry.handle = handle;
    queue.push_back(entry);
}

void LD3D12ReleaseQueue::deferredReleaseResourceWithViews(const LD3D12ObjectHandle &handle,
                                                          LD3D12CpuDescriptorPool *pool,
                                                          const LD3D12Descriptor &viewsStart,
                                                          int viewCount)
{
    deferredReleaseResource(handle);
    if (pool != nullptr && viewCount > 0) {
        deferredReleaseViews(pool, viewsStart, viewCount);
    }
}

void LD3D12ReleaseQueue::deferredReleasePipeline(const LD3D12ObjectHandle &handle)
{
    if (handle.isNull()) {
        return;
    }
    DeferredReleaseEntry entry;
    entry.type = DeferredReleaseEntry::Pipeline;
    entry.handle = handle;
    queue.push_back(entry);
}

void LD3D12ReleaseQueue::deferredReleaseRootSignature(const LD3D12ObjectHandle &handle)
{
    if (handle.isNull()) {
        return;
    }
    DeferredReleaseEntry entry;
    entry.type = DeferredReleaseEntry::RootSignature;
    entry.handle = handle;
    queue.push_back(entry);
}

void LD3D12ReleaseQueue::deferredReleaseCallback(std::function<void (void *)> callback, void *userData)
{
    if (!callback) {
        return;
    }
    DeferredReleaseEntry entry;
    entry.type = DeferredReleaseEntry::Callback;
    entry.callback = std::move(callback);
    entry.callbackUserData = userData;
    queue.push_back(entry);
}

void LD3D12ReleaseQueue::deferredReleaseResourceAndAllocation(ID3D12Resource *resource,
                                                              D3D12MA::Allocation *allocation)
{
    if (resource == nullptr) {
        return;
    }
    DeferredReleaseEntry entry;
    entry.type = DeferredReleaseEntry::ResourceAndAllocation;
    entry.resourceAndAllocation = { resource, allocation };
    queue.push_back(entry);
}

void LD3D12ReleaseQueue::deferredReleaseDescriptorHeap(ID3D12DescriptorHeap *heap)
{
    if (heap == nullptr) {
        return;
    }
    DeferredReleaseEntry entry;
    entry.type = DeferredReleaseEntry::DescriptorHeap;
    entry.descriptorHeap = heap;
    queue.push_back(entry);
}

void LD3D12ReleaseQueue::deferredReleaseViews(LD3D12CpuDescriptorPool *pool,
                                              const LD3D12Descriptor &viewsStart,
                                              int viewCount)
{
    if (pool == nullptr || viewCount <= 0) {
        return;
    }
    DeferredReleaseEntry entry;
    entry.type = DeferredReleaseEntry::Views;
    entry.poolForViews = pool;
    entry.viewsStart = viewsStart;
    entry.viewCount = viewCount;
    queue.push_back(entry);
}

void LD3D12ReleaseQueue::activatePendingDeferredReleaseRequests(int frameSlot)
{
    for (DeferredReleaseEntry &entry : queue) {
        if (!entry.frameSlotToBeReleasedIn.has_value()) {
            entry.frameSlotToBeReleasedIn = frameSlot;
        }
    }
}

void LD3D12ReleaseQueue::executeDeferredReleases(int frameSlot, bool forced)
{
    for (auto it = queue.begin(); it != queue.end();) {
        const bool due = forced
                || (it->frameSlotToBeReleasedIn.has_value()
                    && *it->frameSlotToBeReleasedIn == frameSlot);
        if (!due) {
            ++it;
            continue;
        }
        switch (it->type) {
        case DeferredReleaseEntry::Resource:
            if (resourcePool != nullptr) {
                resourcePool->remove(it->handle);
            }
            break;
        case DeferredReleaseEntry::Pipeline:
            if (pipelinePool != nullptr) {
                pipelinePool->remove(it->handle);
            }
            break;
        case DeferredReleaseEntry::RootSignature:
            if (rootSignaturePool != nullptr) {
                rootSignaturePool->remove(it->handle);
            }
            break;
        case DeferredReleaseEntry::Callback:
            it->callback(it->callbackUserData);
            break;
        case DeferredReleaseEntry::ResourceAndAllocation:
            if (it->resourceAndAllocation.first != nullptr) {
                it->resourceAndAllocation.first->Release();
            }
            if (it->resourceAndAllocation.second != nullptr) {
                it->resourceAndAllocation.second->Release();
            }
            break;
        case DeferredReleaseEntry::DescriptorHeap:
            if (it->descriptorHeap != nullptr) {
                it->descriptorHeap->Release();
            }
            break;
        case DeferredReleaseEntry::Views:
            it->poolForViews->release(it->viewsStart, uint32_t(it->viewCount));
            break;
        }
        it = queue.erase(it);
    }
}

void LD3D12ReleaseQueue::releaseAll()
{
    executeDeferredReleases(0, true);
    queue.clear();
}

// ---------------------------------------------------------------------------
// D3D12MA 内存分配器
// ---------------------------------------------------------------------------
bool LD3D12MemoryAllocator::create(ID3D12Device *device_, IDXGIAdapter1 *adapter)
{
    destroy();
    device = device_;
    if (device == nullptr) {
        return false;
    }
    D3D12MA::ALLOCATOR_DESC desc{};
    desc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;
    desc.pDevice = device;
    desc.pAdapter = adapter;
    const HRESULT hr = D3D12MA::CreateAllocator(&desc, &allocator);
    if (FAILED(hr)) {
        lWarn() << "rhi/d3d12: D3D12MA::CreateAllocator failed, hr =" << int(hr);
        allocator = nullptr;
        device = nullptr;
        return false;
    }
    return true;
}

void LD3D12MemoryAllocator::destroy()
{
    if (allocator != nullptr) {
        allocator->Release();
        allocator = nullptr;
    }
    device = nullptr;
}

HRESULT LD3D12MemoryAllocator::createResource(D3D12_HEAP_TYPE heapType,
                                              const D3D12_RESOURCE_DESC *resourceDesc,
                                              D3D12_RESOURCE_STATES initialState,
                                              const D3D12_CLEAR_VALUE *optimizedClearValue,
                                              D3D12MA::Allocation **maybeAllocation,
                                              REFIID riidResource,
                                              void **ppvResource)
{
    if (allocator == nullptr || resourceDesc == nullptr) {
        return E_FAIL;
    }
    D3D12MA::ALLOCATION_DESC allocDesc{};
    allocDesc.HeapType = heapType;
    D3D12MA::Allocation *allocation = nullptr;
    const HRESULT hr = allocator->CreateResource(&allocDesc, resourceDesc, initialState,
                                                 optimizedClearValue, &allocation,
                                                 riidResource, ppvResource);
    if (SUCCEEDED(hr) && maybeAllocation != nullptr) {
        *maybeAllocation = allocation;
    }
    return hr;
}

void LD3D12MemoryAllocator::getBudget(D3D12MA::Budget *localBudget, D3D12MA::Budget *nonLocalBudget)
{
    if (allocator == nullptr) {
        return;
    }
    allocator->GetBudget(localBudget, nonLocalBudget);
}

// ---------------------------------------------------------------------------
// 上传暂存区
// ---------------------------------------------------------------------------
bool LD3D12StagingArea::create(LRhiD3D12 *rhi, uint32_t capacity_, D3D12_HEAP_TYPE heapType)
{
    destroy();
    if (rhi == nullptr || capacity_ == 0) {
        return false;
    }

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = capacity_;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12MA::Allocation *alloc = nullptr;
    ID3D12Resource *res = nullptr;
    if (FAILED(rhi->vma.createResource(heapType, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
                                       nullptr, &alloc, __uuidof(ID3D12Resource),
                                       reinterpret_cast<void **>(&res)))) {
        return false;
    }

    void *p = nullptr;
    if (heapType == D3D12_HEAP_TYPE_UPLOAD) {
        if (FAILED(res->Map(0, nullptr, &p))) {
            res->Release();
            if (alloc != nullptr) {
                alloc->Release();
            }
            return false;
        }
    }

    resource = res;
    allocation = alloc;
    mem.buffer = res;
    mem.p = static_cast<uint8_t *>(p);
    mem.gpuAddr = res->GetGPUVirtualAddress();
    mem.bufferOffset = 0;
    capacity = capacity_;
    head = 0;
    return true;
}

void LD3D12StagingArea::destroy()
{
    if (resource != nullptr) {
        if (mem.p != nullptr) {
            resource->Unmap(0, nullptr);
        }
        resource->Release();
        resource = nullptr;
    }
    if (allocation != nullptr) {
        allocation->Release();
        allocation = nullptr;
    }
    mem = {};
    capacity = 0;
    head = 0;
}

void LD3D12StagingArea::destroyWithDeferredRelease(LD3D12ReleaseQueue *releaseQueue)
{
    if (releaseQueue != nullptr && (resource != nullptr || allocation != nullptr)) {
        if (mem.p != nullptr && resource != nullptr) {
            resource->Unmap(0, nullptr);
        }
        releaseQueue->deferredReleaseResourceAndAllocation(resource, allocation);
    } else {
        if (resource != nullptr) {
            if (mem.p != nullptr) {
                resource->Unmap(0, nullptr);
            }
            resource->Release();
        }
        if (allocation != nullptr) {
            allocation->Release();
        }
    }
    resource = nullptr;
    allocation = nullptr;
    mem = {};
    capacity = 0;
    head = 0;
}

LD3D12StagingArea::Allocation LD3D12StagingArea::get(uint32_t byteSize)
{
    if (resource == nullptr || byteSize == 0) {
        return {};
    }
    const uint32_t aligned = allocSizeForArray(byteSize);
    if (head + aligned > capacity) {
        return {};
    }
    Allocation a = mem;
    a.p = mem.p != nullptr ? mem.p + head : nullptr;
    a.gpuAddr = mem.gpuAddr + head;
    a.bufferOffset = head;
    head += aligned;
    return a;
}

// ---------------------------------------------------------------------------
// 查询堆
// ---------------------------------------------------------------------------
bool LD3D12QueryHeap::create(ID3D12Device *device,
                             uint32_t queryCount,
                             D3D12_QUERY_HEAP_TYPE heapType)
{
    destroy();
    if (device == nullptr || queryCount == 0) {
        return false;
    }
    D3D12_QUERY_HEAP_DESC desc{};
    desc.Count = queryCount;
    desc.Type = heapType;
    desc.NodeMask = 0;
    if (FAILED(device->CreateQueryHeap(&desc, __uuidof(ID3D12QueryHeap),
                                       reinterpret_cast<void **>(&heap)))) {
        heap = nullptr;
        return false;
    }
    capacity = queryCount;
    return true;
}

void LD3D12QueryHeap::destroy()
{
    if (heap != nullptr) {
        heap->Release();
        heap = nullptr;
    }
    capacity = 0;
}

// ---------------------------------------------------------------------------
// 着色器可见描述符堆
// ---------------------------------------------------------------------------
bool LD3D12ShaderVisibleDescriptorHeap::create(ID3D12Device *device,
                                               D3D12_DESCRIPTOR_HEAP_TYPE type,
                                               uint32_t perFrameDescriptorCount)
{
    destroy();
    if (device == nullptr || perFrameDescriptorCount == 0) {
        return false;
    }
    const uint32_t total = perFrameDescriptorCount * LD3D12_FRAMES_IN_FLIGHT;
    if (!heap.create(device, total, type, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)) {
        return false;
    }
    for (int i = 0; i < LD3D12_FRAMES_IN_FLIGHT; ++i) {
        perFrameHeapSlice[i].createWithExisting(heap, uint32_t(i) * perFrameDescriptorCount,
                                                perFrameDescriptorCount);
    }
    return true;
}

void LD3D12ShaderVisibleDescriptorHeap::destroy()
{
    for (int i = 0; i < LD3D12_FRAMES_IN_FLIGHT; ++i) {
        perFrameHeapSlice[i].destroy();
    }
    heap.destroy();
}

void LD3D12ShaderVisibleDescriptorHeap::destroyWithDeferredRelease(LD3D12ReleaseQueue *releaseQueue)
{
    for (int i = 0; i < LD3D12_FRAMES_IN_FLIGHT; ++i) {
        perFrameHeapSlice[i].destroy();
    }
    heap.destroyWithDeferredRelease(releaseQueue);
}

// ---------------------------------------------------------------------------
// 采样器管理器
// ---------------------------------------------------------------------------
bool LD3D12SamplerManager::create(ID3D12Device *device_)
{
    destroy();
    if (device_ == nullptr) {
        return false;
    }
    device = device_;
    gpuMap.clear();
    return shaderVisibleSamplerHeap.create(device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
                                           MAX_SAMPLERS / LD3D12_FRAMES_IN_FLIGHT);
}

void LD3D12SamplerManager::destroy()
{
    gpuMap.clear();
    shaderVisibleSamplerHeap.destroy();
    device = nullptr;
}

LD3D12Descriptor LD3D12SamplerManager::getShaderVisibleDescriptor(const D3D12_SAMPLER_DESC &desc)
{
    if (device == nullptr || currentFrameSlot < 0 || currentFrameSlot >= LD3D12_FRAMES_IN_FLIGHT) {
        return {};
    }
    LD3D12Descriptor d = shaderVisibleSamplerHeap.perFrameHeapSlice[currentFrameSlot].get(1);
    if (!d.isValid()) {
        return {};
    }
    device->CreateSampler(&desc, d.cpuHandle);
    return d;
}

// ---------------------------------------------------------------------------
// 着色器字节码缓存
// ---------------------------------------------------------------------------
void LD3D12ShaderBytecodeCache::insertWithCapacityLimit(const LRhiShaderStage &key,
                                                        const Shader &s)
{
    static const size_t kMaxEntries = 64;
    const auto it = data.find(key);
    if (it == data.end() && data.size() >= kMaxEntries) {
        return;
    }
    data[key] = s;
}

// ---------------------------------------------------------------------------
// 着色器资源绑定
// ---------------------------------------------------------------------------
LD3D12ShaderResourceBindings::~LD3D12ShaderResourceBindings()
{
    destroy();
}

void LD3D12ShaderResourceBindings::destroy()
{
    if (!rootSigHandle.isNull()) {
        std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(m_rhi.lock());
        if (rhiD != nullptr) {
            rhiD->releaseQueue.deferredReleaseRootSignature(rootSigHandle);
        }
        rootSigHandle = {};
    }
    for (int i = 0; i < LD3D12_FRAMES_IN_FLIGHT; ++i) {
        srvTable[i] = {};
        samplerTable[i] = {};
    }
    srvCount = 0;
    samplerCount = 0;
}

LD3D12ObjectHandle LD3D12ShaderResourceBindings::createRootSignature(const LD3D12ShaderStageData *, int)
{
    return rootSigHandle;
}

bool LD3D12ShaderResourceBindings::create()
{
    std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(m_rhi.lock());
    if (rhiD == nullptr || rhiD->dev == nullptr) {
        return false;
    }
    destroy();

    std::vector<BindingInfo> infos = bindingInfos();
    std::vector<D3D12_DESCRIPTOR_RANGE> srvRanges;
    std::vector<D3D12_DESCRIPTOR_RANGE> samplerRanges;
    srvCount = 0;
    samplerCount = 0;

    for (const BindingInfo &info : infos) {
        if (info.type == LRhiShaderResourceBinding::SampledTexture ||
            info.type == LRhiShaderResourceBinding::Texture) {
            if (info.arraySize <= 0) {
                continue;
            }
            D3D12_DESCRIPTOR_RANGE range{};
            range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            range.NumDescriptors = UINT(info.arraySize);
            range.BaseShaderRegister = UINT(info.binding);
            range.RegisterSpace = 0;
            range.OffsetInDescriptorsFromTableStart = srvCount;
            srvRanges.push_back(range);
            srvCount += UINT(info.arraySize);
        }
        if (info.type == LRhiShaderResourceBinding::Sampler ||
            info.type == LRhiShaderResourceBinding::SampledTexture) {
            const int count = info.type == LRhiShaderResourceBinding::Sampler ? 1 : info.arraySize;
            if (count <= 0) {
                continue;
            }
            D3D12_DESCRIPTOR_RANGE range{};
            range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
            range.NumDescriptors = UINT(count);
            range.BaseShaderRegister = UINT(info.binding);
            range.RegisterSpace = 0;
            range.OffsetInDescriptorsFromTableStart = samplerCount;
            samplerRanges.push_back(range);
            samplerCount += UINT(count);
        }
    }

    std::vector<D3D12_ROOT_PARAMETER> params;
    if (!srvRanges.empty()) {
        D3D12_ROOT_PARAMETER p{};
        p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        p.DescriptorTable.NumDescriptorRanges = UINT(srvRanges.size());
        p.DescriptorTable.pDescriptorRanges = srvRanges.data();
        p.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params.push_back(p);
    }
    if (!samplerRanges.empty()) {
        D3D12_ROOT_PARAMETER p{};
        p.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        p.DescriptorTable.NumDescriptorRanges = UINT(samplerRanges.size());
        p.DescriptorTable.pDescriptorRanges = samplerRanges.data();
        p.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params.push_back(p);
    }

    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = UINT(params.size());
    desc.pParameters = params.empty() ? nullptr : params.data();
    desc.NumStaticSamplers = 0;
    desc.pStaticSamplers = nullptr;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ID3DBlob *signature = nullptr;
    ID3DBlob *error = nullptr;
    if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
                                           &signature, &error))) {
        if (error != nullptr) {
            lWarn() << "rhi/d3d12: SRB root signature failed:"
                    << static_cast<const char *>(error->GetBufferPointer());
            error->Release();
        }
        if (signature != nullptr) {
            signature->Release();
        }
        return false;
    }
    if (error != nullptr) {
        error->Release();
    }

    ID3D12RootSignature *rootSig = nullptr;
    const HRESULT hr = rhiD->dev->CreateRootSignature(0, signature->GetBufferPointer(),
                                                      signature->GetBufferSize(),
                                                      __uuidof(ID3D12RootSignature),
                                                      reinterpret_cast<void **>(&rootSig));
    signature->Release();
    if (FAILED(hr)) {
        lWarn() << "rhi/d3d12: SRB CreateRootSignature failed, hr =" << int(hr);
        return false;
    }
    rootSigHandle = LD3D12RootSignature::addToPool(&rhiD->rootSignaturePool, rootSig);
    ++generation;
    return true;
}

void LD3D12ShaderResourceBindings::updateResources(bool bindingsAreSorted)
{
    (void)bindingsAreSorted;
    std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(m_rhi.lock());
    if (rhiD == nullptr || rhiD->dev == nullptr || rootSigHandle.isNull()) {
        return;
    }
    const int slot = rhiD->currentSwapChain != nullptr ? rhiD->currentSwapChain->currentFrameSlot : 0;
    LD3D12DescriptorHeap &srvHeap = rhiD->shaderVisibleCbvSrvUavHeap.perFrameHeapSlice[slot];
    LD3D12DescriptorHeap &samplerHeap = rhiD->samplerMgr.shaderVisibleSamplerHeap.perFrameHeapSlice[slot];

    if (srvCount > 0) {
        srvTable[slot] = srvHeap.get(srvCount);
    }
    if (samplerCount > 0) {
        samplerTable[slot] = samplerHeap.get(samplerCount);
    }

    uint32_t srvIndex = 0;
    uint32_t samplerIndex = 0;
    for (const BindingInfo &info : bindingInfos()) {
        if ((info.type == LRhiShaderResourceBinding::SampledTexture ||
             info.type == LRhiShaderResourceBinding::Texture) && !info.textureSamplers.empty()) {
            for (const LRhiShaderResourceBinding::TextureAndSampler &ts : info.textureSamplers) {
                std::shared_ptr<LD3D12Texture> tex = std::dynamic_pointer_cast<LD3D12Texture>(ts.tex);
                if (tex != nullptr && !tex->handle.isNull() && srvIndex < srvCount) {
                    LD3D12Resource *r = rhiD->resourcePool.lookupRef(tex->handle);
                    if (r != nullptr) {
                        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
                        srvDesc.Format = tex->srvFormat;
                        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                        srvDesc.Texture2D.MostDetailedMip = 0;
                        srvDesc.Texture2D.MipLevels = tex->mipLevelCount;
                        rhiD->dev->CreateShaderResourceView(
                                    r->resource, &srvDesc,
                                    srvHeap.incremented(srvTable[slot], srvIndex).cpuHandle);
                    }
                }
                ++srvIndex;

                std::shared_ptr<LD3D12Sampler> sampler = std::dynamic_pointer_cast<LD3D12Sampler>(ts.sampler);
                if (sampler != nullptr && samplerIndex < samplerCount) {
                    rhiD->dev->CreateSampler(
                                &sampler->desc,
                                samplerHeap.incremented(samplerTable[slot], samplerIndex).cpuHandle);
                    ++samplerIndex;
                }
            }
        } else if (info.type == LRhiShaderResourceBinding::Sampler && info.sampler != nullptr) {
            std::shared_ptr<LD3D12Sampler> sampler = std::dynamic_pointer_cast<LD3D12Sampler>(info.sampler);
            if (sampler != nullptr && samplerIndex < samplerCount) {
                rhiD->dev->CreateSampler(
                            &sampler->desc,
                            samplerHeap.incremented(samplerTable[slot], samplerIndex).cpuHandle);
                ++samplerIndex;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// 格式转换与小型工具
// ---------------------------------------------------------------------------
namespace {

DXGI_FORMAT lD3d12TextureFormat(LRhiTexture::Format f)
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

DXGI_FORMAT lD3d12SrgbFormat(DXGI_FORMAT f)
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

bool lD3d12IsDepthFormat(LRhiTexture::Format f)
{
    return f == LRhiTexture::D16 || f == LRhiTexture::D24 || f == LRhiTexture::D24S8
            || f == LRhiTexture::D32F || f == LRhiTexture::D32FS8;
}

uint32_t lD3d12MipLevels(const LSize<int> &size)
{
    uint32_t levels = 1;
    int w = size.m_x;
    int h = size.m_y;
    while (w > 1 || h > 1) {
        w = w > 1 ? w / 2 : 1;
        h = h > 1 ? h / 2 : 1;
        ++levels;
    }
    return levels;
}

D3D12_FILTER lD3d12Filter(LRhiSampler::Filter mag, LRhiSampler::Filter min, LRhiSampler::Filter mip)
{
    const bool magLinear = mag == LRhiSampler::Linear;
    const bool minLinear = min == LRhiSampler::Linear;
    const bool mipLinear = mip == LRhiSampler::Linear;
    if (minLinear && mipLinear) {
        return magLinear ? D3D12_FILTER_MIN_MAG_MIP_LINEAR
                         : D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
    }
    if (minLinear) {
        return D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT;
    }
    if (magLinear) {
        return D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
    }
    return D3D12_FILTER_MIN_MAG_MIP_POINT;
}

D3D12_TEXTURE_ADDRESS_MODE lD3d12AddressMode(LRhiSampler::AddressMode m)
{
    switch (m) {
    case LRhiSampler::Repeat:       return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    case LRhiSampler::Mirror:       return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    case LRhiSampler::ClampToEdge:  return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    default:                        return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// 纹理
// ---------------------------------------------------------------------------
LD3D12Texture::LD3D12Texture(std::shared_ptr<LRhiImplementation> rhi, Format format,
                             const LSize<int> &pixelSize, int depth, int arraySize,
                             int sampleCount, Flags flags)
    : LRhiTexture(rhi, format, pixelSize, depth, arraySize, sampleCount, flags),
      dxgiFormat(DXGI_FORMAT_UNKNOWN),
      srvFormat(DXGI_FORMAT_UNKNOWN),
      rtFormat(DXGI_FORMAT_UNKNOWN),
      mipLevelCount(1)
{
}

LD3D12Texture::~LD3D12Texture()
{
    destroy();
}

void LD3D12Texture::destroy()
{
    if (handle.isNull() && !srv.isValid()) {
        return;
    }

    std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(m_rhi.lock());
    if (rhiD != nullptr) {
        rhiD->releaseQueue.deferredReleaseResourceWithViews(handle, &rhiD->cbvSrvUavPool,
                                                             srv, srv.isValid() ? 1 : 0);
    }
    handle = {};
    srv = {};
}

bool LD3D12Texture::prepareCreate(LSize<int> *adjustedSize)
{
    (void)adjustedSize;
    return true;
}

bool LD3D12Texture::finishCreate()
{
    return true;
}

bool LD3D12Texture::create()
{
    std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(m_rhi.lock());
    if (rhiD == nullptr) {
        return false;
    }
    destroy();

    dxgiFormat = lD3d12TextureFormat(m_format);
    if (dxgiFormat == DXGI_FORMAT_UNKNOWN) {
        lWarn() << "rhi/d3d12: unsupported texture format";
        return false;
    }
    const bool srgb = m_flags.testFlag(LRhiTextureFlag::sRGB);
    srvFormat = srgb ? lD3d12SrgbFormat(dxgiFormat) : dxgiFormat;
    rtFormat = srvFormat;
    const bool isDepth = lD3d12IsDepthFormat(m_format);
    mipLevelCount = m_flags.testFlag(LRhiTextureFlag::MipMapped) ? lD3d12MipLevels(m_pixelSize) : 1;
    sampleDesc.Count = uint32_t(std::max(1, m_sampleCount));
    sampleDesc.Quality = 0;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = UINT64(std::max(1, m_pixelSize.m_x));
    desc.Height = UINT(std::max(1, m_pixelSize.m_y));
    desc.DepthOrArraySize = UINT16(std::max(1, m_arraySize));
    desc.MipLevels = UINT16(mipLevelCount);
    desc.Format = dxgiFormat;
    desc.SampleDesc = sampleDesc;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;
    if (m_flags.testFlag(LRhiTextureFlag::RenderTarget)) {
        desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
    if (m_flags.testFlag(LRhiTextureFlag::UsedWithLoadStore)) {
        desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COPY_DEST;
    if (isDepth) {
        state = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    } else if (m_flags.testFlag(LRhiTextureFlag::RenderTarget)) {
        state = D3D12_RESOURCE_STATE_RENDER_TARGET;
    }

    D3D12MA::Allocation *alloc = nullptr;
    ID3D12Resource *res = nullptr;
    if (FAILED(rhiD->vma.createResource(D3D12_HEAP_TYPE_DEFAULT, &desc, state, nullptr,
                                        &alloc, __uuidof(ID3D12Resource),
                                        reinterpret_cast<void **>(&res)))) {
        lWarn() << "rhi/d3d12: texture creation failed";
        return false;
    }

    handle = LD3D12Resource::addToPool(&rhiD->resourcePool, res, state, alloc, nullptr);

    if (!isDepth) {
        srv = rhiD->cbvSrvUavPool.allocate(1);
        if (srv.cpuHandle.ptr != 0) {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Format = srvFormat;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = mipLevelCount;
            srvDesc.Texture2D.PlaneSlice = 0;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
            rhiD->dev->CreateShaderResourceView(res, &srvDesc, srv.cpuHandle);
        }
    }

    ++generation;
    return true;
}

bool LD3D12Texture::createFrom(NativeTexture src)
{
    if (src.object == 0) {
        return false;
    }
    std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(m_rhi.lock());
    if (rhiD == nullptr) {
        return false;
    }
    destroy();

    ID3D12Resource *res = reinterpret_cast<ID3D12Resource *>(uintptr_t(src.object));
    const D3D12_RESOURCE_DESC desc = res->GetDesc();
    dxgiFormat = desc.Format;
    srvFormat = desc.Format;
    rtFormat = desc.Format;
    mipLevelCount = desc.MipLevels;
    sampleDesc = desc.SampleDesc;
    m_pixelSize = LSize<int>(int(desc.Width), int(desc.Height));

    handle = LD3D12Resource::addNonOwningToPool(&rhiD->resourcePool, res,
                                                D3D12_RESOURCE_STATES(src.layout));

    srv = rhiD->cbvSrvUavPool.allocate(1);
    if (srv.cpuHandle.ptr != 0) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Format = srvFormat;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = mipLevelCount;
        srvDesc.Texture2D.PlaneSlice = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        rhiD->dev->CreateShaderResourceView(res, &srvDesc, srv.cpuHandle);
    }

    ++generation;
    return true;
}

LRhiTexture::NativeTexture LD3D12Texture::nativeTexture()
{
    NativeTexture nt{};
    std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(m_rhi.lock());
    if (rhiD != nullptr) {
        if (LD3D12Resource *r = rhiD->resourcePool.lookupRef(handle)) {
            nt.object = reinterpret_cast<uint64_t>(r->resource);
            nt.layout = int(r->state);
        }
    }
    return nt;
}

void LD3D12Texture::setNativeLayout(int layout)
{
    std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(m_rhi.lock());
    if (rhiD == nullptr) {
        return;
    }
    if (LD3D12Resource *r = rhiD->resourcePool.lookupRef(handle)) {
        r->state = D3D12_RESOURCE_STATES(layout);
    }
}

// ---------------------------------------------------------------------------
// 采样器
// ---------------------------------------------------------------------------
LD3D12Sampler::LD3D12Sampler(std::shared_ptr<LRhiImplementation> rhi, Filter magFilter,
                             Filter minFilter, Filter mipmapMode, AddressMode u,
                             AddressMode v, AddressMode w)
    : LRhiSampler(rhi, magFilter, minFilter, mipmapMode, u, v, w)
{
}

LD3D12Sampler::~LD3D12Sampler()
{
    destroy();
}

void LD3D12Sampler::destroy()
{
    shaderVisibleDescriptor = {};
}

bool LD3D12Sampler::create()
{
    desc.Filter = lD3d12Filter(m_magFilter, m_minFilter, m_mipmapMode);
    desc.AddressU = lD3d12AddressMode(m_addressU);
    desc.AddressV = lD3d12AddressMode(m_addressV);
    desc.AddressW = lD3d12AddressMode(m_addressW);
    desc.MipLODBias = 0.0f;
    desc.MaxAnisotropy = 1;
    desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    desc.BorderColor[0] = 0.0f;
    desc.BorderColor[1] = 0.0f;
    desc.BorderColor[2] = 0.0f;
    desc.BorderColor[3] = 0.0f;
    desc.MinLOD = 0.0f;
    desc.MaxLOD = D3D12_FLOAT32_MAX;
    shaderVisibleDescriptor = {};
    return true;
}

LD3D12Descriptor LD3D12Sampler::lookupOrCreateShaderVisibleDescriptor()
{
    std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(m_rhi.lock());
    if (rhiD == nullptr) {
        return {};
    }
    shaderVisibleDescriptor = rhiD->samplerMgr.getShaderVisibleDescriptor(desc);
    return shaderVisibleDescriptor;
}

// ---------------------------------------------------------------------------
// 渲染后备缓冲
// ---------------------------------------------------------------------------
LD3D12RenderBuffer::LD3D12RenderBuffer(std::shared_ptr<LRhiImplementation> rhi, Type type,
                                       const LSize<int> &pixelSize, int sampleCount,
                                       bool usedWithSwapChainOnly,
                                       LRhiTexture::Format backingFormatHint)
    : LRhiRenderBuffer(rhi, type, pixelSize, sampleCount, usedWithSwapChainOnly, backingFormatHint),
      dxgiFormat(DXGI_FORMAT_UNKNOWN)
{
}

LD3D12RenderBuffer::~LD3D12RenderBuffer()
{
    destroy();
}

void LD3D12RenderBuffer::destroy()
{
    if (handle.isNull() && !rtv.isValid() && !dsv.isValid()) {
        return;
    }

    std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(m_rhi.lock());
    if (rhiD != nullptr) {
        if (m_type == DepthStencil) {
            rhiD->releaseQueue.deferredReleaseResourceWithViews(handle, &rhiD->dsvPool,
                                                                 dsv, dsv.isValid() ? 1 : 0);
        } else {
            rhiD->releaseQueue.deferredReleaseResourceWithViews(handle, &rhiD->rtvPool,
                                                                 rtv, rtv.isValid() ? 1 : 0);
        }
    }
    handle = {};
    rtv = {};
    dsv = {};
}

LRhiTexture::Format LD3D12RenderBuffer::backingFormat() const
{
    if (m_type == DepthStencil) {
        return LRhiTexture::D24S8;
    }
    return m_backingFormatHint != LRhiTexture::UnknownFormat ? m_backingFormatHint
                                                            : LRhiTexture::BGRA8;
}

bool LD3D12RenderBuffer::create()
{
    std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(m_rhi.lock());
    if (rhiD == nullptr) {
        return false;
    }
    destroy();

    const bool isDepth = (m_type == DepthStencil);
    dxgiFormat = isDepth ? DS_FORMAT : lD3d12TextureFormat(backingFormat());
    if (dxgiFormat == DXGI_FORMAT_UNKNOWN) {
        dxgiFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    }
    sampleDesc.Count = uint32_t(std::max(1, m_sampleCount));
    sampleDesc.Quality = 0;

    D3D12_RESOURCE_DESC desc{};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = UINT64(std::max(1, m_pixelSize.m_x));
    desc.Height = UINT(std::max(1, m_pixelSize.m_y));
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = dxgiFormat;
    desc.SampleDesc = sampleDesc;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = isDepth ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
                         : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    const D3D12_RESOURCE_STATES state = isDepth ? D3D12_RESOURCE_STATE_DEPTH_WRITE
                                                : D3D12_RESOURCE_STATE_RENDER_TARGET;

    D3D12_CLEAR_VALUE clearValue{};
    D3D12_CLEAR_VALUE *clearPtr = nullptr;
    if (isDepth) {
        clearValue.Format = dxgiFormat;
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.DepthStencil.Stencil = 0;
        clearPtr = &clearValue;
    } else {
        clearValue.Format = dxgiFormat;
        clearValue.Color[0] = 0.0f;
        clearValue.Color[1] = 0.0f;
        clearValue.Color[2] = 0.0f;
        clearValue.Color[3] = 0.0f;
        clearPtr = &clearValue;
    }

    D3D12MA::Allocation *alloc = nullptr;
    ID3D12Resource *res = nullptr;
    if (FAILED(rhiD->vma.createResource(D3D12_HEAP_TYPE_DEFAULT, &desc, state, clearPtr,
                                        &alloc, __uuidof(ID3D12Resource),
                                        reinterpret_cast<void **>(&res)))) {
        lWarn() << "rhi/d3d12: render buffer creation failed";
        return false;
    }

    handle = LD3D12Resource::addToPool(&rhiD->resourcePool, res, state, alloc, nullptr);

    if (isDepth) {
        dsv = rhiD->dsvPool.allocate(1);
        if (dsv.cpuHandle.ptr != 0) {
            D3D12_DEPTH_STENCIL_VIEW_DESC vd{};
            vd.Format = dxgiFormat;
            vd.ViewDimension = sampleDesc.Count > 1 ? D3D12_DSV_DIMENSION_TEXTURE2DMS
                                                    : D3D12_DSV_DIMENSION_TEXTURE2D;
            rhiD->dev->CreateDepthStencilView(res, &vd, dsv.cpuHandle);
        }
    } else {
        rtv = rhiD->rtvPool.allocate(1);
        if (rtv.cpuHandle.ptr != 0) {
            rhiD->dev->CreateRenderTargetView(res, nullptr, rtv.cpuHandle);
        }
    }

    ++generation;
    return true;
}

// ---------------------------------------------------------------------------
// Render pass descriptor
// ---------------------------------------------------------------------------
LD3D12RenderPassDescriptor::LD3D12RenderPassDescriptor(std::shared_ptr<LRhiImplementation> rhi)
    : LRhiRenderPassDescriptor(rhi), dsFormat(-1)
{
    for (int i = 0; i < MAX_COLOR_ATTACHMENTS; ++i) {
        colorFormat[i] = -1;
    }
    updateSerializedFormat();
}

LD3D12RenderPassDescriptor::~LD3D12RenderPassDescriptor()
{
    destroy();
}

void LD3D12RenderPassDescriptor::destroy()
{
    serializedFormatData.clear();
}

bool LD3D12RenderPassDescriptor::isCompatible(const std::shared_ptr<LRhiRenderPassDescriptor> other) const
{
    std::shared_ptr<LD3D12RenderPassDescriptor> o =
            std::dynamic_pointer_cast<LD3D12RenderPassDescriptor>(other);
    return o != nullptr && o->serializedFormat() == serializedFormat();
}

std::shared_ptr<LRhiRenderPassDescriptor> LD3D12RenderPassDescriptor::newCompatibleRenderPassDescriptor() const
{
    std::shared_ptr<LD3D12RenderPassDescriptor> d =
            makeInit<LD3D12RenderPassDescriptor>(m_rhi.lock());
    d->colorAttachmentCount = colorAttachmentCount;
    d->hasDepthStencil = hasDepthStencil;
    d->sampleCount = sampleCount;
    d->dsFormat = dsFormat;
    d->hasShadingRateMap = hasShadingRateMap;
    for (int i = 0; i < MAX_COLOR_ATTACHMENTS; ++i) {
        d->colorFormat[i] = colorFormat[i];
    }
    d->updateSerializedFormat();
    return d;
}

std::vector<uint32_t> LD3D12RenderPassDescriptor::serializedFormat() const
{
    return serializedFormatData;
}

void LD3D12RenderPassDescriptor::updateSerializedFormat()
{
    serializedFormatData.clear();
    serializedFormatData.push_back(uint32_t(colorAttachmentCount));
    serializedFormatData.push_back(uint32_t(hasDepthStencil));
    serializedFormatData.push_back(uint32_t(sampleCount));
    for (int i = 0; i < colorAttachmentCount && i < MAX_COLOR_ATTACHMENTS; ++i) {
        serializedFormatData.push_back(uint32_t(colorFormat[i]));
    }
    serializedFormatData.push_back(uint32_t(dsFormat));
}

// ---------------------------------------------------------------------------
// 交换链渲染目标
// ---------------------------------------------------------------------------
LD3D12SwapChainRenderTarget::LD3D12SwapChainRenderTarget(
        std::shared_ptr<LRhiImplementation> rhi, std::shared_ptr<LRhiSwapChain> swapchain)
    : LRhiSwapChainRenderTarget(rhi, std::move(swapchain)),
      d(rhi)
{
}

LD3D12SwapChainRenderTarget::~LD3D12SwapChainRenderTarget()
{
    destroy();
}

void LD3D12SwapChainRenderTarget::destroy()
{
    d.rtv[0] = {};
    d.dsv = {};
}

LSize<int> LD3D12SwapChainRenderTarget::pixelSize() const
{
    std::shared_ptr<LD3D12SwapChain> sc = std::dynamic_pointer_cast<LD3D12SwapChain>(m_swapchain);
    return sc != nullptr ? sc->pixelSize : LSize<int>(1, 1);
}

float LD3D12SwapChainRenderTarget::devicePixelRatio() const
{
    return 1.0f;
}

int LD3D12SwapChainRenderTarget::sampleCount() const
{
    return 1;
}

// ---------------------------------------------------------------------------
// 命令缓冲
// ---------------------------------------------------------------------------
LD3D12CommandBuffer::LD3D12CommandBuffer(std::shared_ptr<LRhiImplementation> rhi)
    : LRhiCommandBuffer(rhi)
{
    resetState();
}

LD3D12CommandBuffer::~LD3D12CommandBuffer()
{
    destroy();
}

void LD3D12CommandBuffer::destroy()
{
    cmdList = nullptr;
    nativeHandlesStruct.commandList = nullptr;
}

const LRhiNativeHandles *LD3D12CommandBuffer::nativeHandles()
{
    nativeHandlesStruct.commandList = cmdList;
    return &nativeHandlesStruct;
}

// ---------------------------------------------------------------------------
// 交换链
// ---------------------------------------------------------------------------
LD3D12SwapChain::LD3D12SwapChain(std::shared_ptr<LRhiImplementation> rhi)
    : LRhiSwapChain(rhi),
      pixelSize(1, 1),
      colorFormat(DXGI_FORMAT_B8G8R8A8_UNORM),
      srgbAdjustedColorFormat(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB),
      hdrColorSpace(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709),
      rtWrapper(rhi, nullptr),
      rtWrapperRight(rhi, nullptr),
      cbWrapper(rhi)
{
    sampleDesc.Count = 1;
    sampleDesc.Quality = 0;
    window = m_window;
}

LD3D12SwapChain::~LD3D12SwapChain()
{
    destroy();
}

void LD3D12SwapChain::destroy()
{
    if (sourceSwapChain1 == nullptr && swapChain == nullptr) {
        return;
    }

    for (int i = 0; i < LD3D12_FRAMES_IN_FLIGHT; ++i) {
        waitCommandCompletionForFrameSlot(i);
    }

    for (int i = 0; i < LD3D12_FRAMES_IN_FLIGHT; ++i) {
        FrameResources &fr = frameRes[i];
        if (fr.cmdList != nullptr) {
            fr.cmdList->Release();
            fr.cmdList = nullptr;
        }
        if (fr.fence != nullptr) {
            fr.fence->Release();
            fr.fence = nullptr;
        }
        if (fr.fenceEvent != nullptr) {
            CloseHandle(fr.fenceEvent);
            fr.fenceEvent = nullptr;
        }
    }

    releaseBuffers();

    if (dcompVisual != nullptr) {
        dcompVisual->Release();
        dcompVisual = nullptr;
    }
    if (dcompTarget != nullptr) {
        dcompTarget->Release();
        dcompTarget = nullptr;
    }
    if (swapChain != nullptr) {
        swapChain->Release();
        swapChain = nullptr;
    }
    if (sourceSwapChain1 != nullptr) {
        sourceSwapChain1->Release();
        sourceSwapChain1 = nullptr;
    }
    frameLatencyWaitableObject = nullptr;
}

std::shared_ptr<LRhiCommandBuffer> LD3D12SwapChain::currentFrameCommandBuffer()
{
    return std::shared_ptr<LRhiCommandBuffer>(&cbWrapper, [](LRhiCommandBuffer *) {});
}

std::shared_ptr<LRhiRenderTarget> LD3D12SwapChain::currentFrameRenderTarget()
{
    return std::shared_ptr<LRhiRenderTarget>(&rtWrapper, [](LRhiRenderTarget *) {});
}

std::shared_ptr<LRhiRenderTarget> LD3D12SwapChain::currentFrameRenderTarget(StereoTargetBuffer targetBuffer)
{
    return targetBuffer == RightBuffer
            ? std::shared_ptr<LRhiRenderTarget>(&rtWrapperRight, [](LRhiRenderTarget *) {})
            : currentFrameRenderTarget();
}

HWND LD3D12SwapChain::nativeHwnd() const
{
    if (nativeWindowHandle != nullptr) {
        return reinterpret_cast<HWND>(nativeWindowHandle);
    }
    if (m_window != nullptr) {
        return reinterpret_cast<HWND>(static_cast<uintptr_t>(m_window->winId()));
    }
    return nullptr;
}

LSize<int> LD3D12SwapChain::surfacePixelSize()
{
    const HWND hwnd = nativeHwnd();
    if (hwnd != nullptr) {
        RECT rc{};
        if (GetClientRect(hwnd, &rc)) {
            return LSize<int>(int((std::max)(LONG(1), rc.right - rc.left)),
                              int((std::max)(LONG(1), rc.bottom - rc.top)));
        }
    }
    return LSize<int>(1, 1);
}

bool LD3D12SwapChain::isFormatSupported(Format f)
{
    return f == SDR;
}

LRhiSwapChainHdrInfo LD3D12SwapChain::hdrInfo()
{
    return LRhiSwapChainHdrInfo();
}

std::shared_ptr<LRhiRenderPassDescriptor> LD3D12SwapChain::newCompatibleRenderPassDescriptor()
{
    if (m_renderPassDesc != nullptr) {
        return m_renderPassDesc->newCompatibleRenderPassDescriptor();
    }
    return nullptr;
}

bool LD3D12SwapChain::ensureFrameResources()
{
    std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(m_rhi.lock());
    if (rhiD == nullptr || rhiD->dev == nullptr) {
        return false;
    }
    for (int i = 0; i < LD3D12_FRAMES_IN_FLIGHT; ++i) {
        FrameResources &fr = frameRes[i];
        if (fr.cmdList == nullptr) {
            if (rhiD->cmdAllocators[i] == nullptr ||
                FAILED(rhiD->dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                    rhiD->cmdAllocators[i], nullptr,
                                                    __uuidof(ID3D12GraphicsCommandList),
                                                    reinterpret_cast<void **>(&fr.cmdList)))) {
                lWarn() << "rhi/d3d12: CreateCommandList failed";
                return false;
            }
            fr.cmdList->Close();
        }
        if (fr.fence == nullptr) {
            if (FAILED(rhiD->dev->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                              __uuidof(ID3D12Fence),
                                              reinterpret_cast<void **>(&fr.fence)))) {
                lWarn() << "rhi/d3d12: swapchain fence creation failed";
                return false;
            }
        }
        if (fr.fenceEvent == nullptr) {
            fr.fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
            if (fr.fenceEvent == nullptr) {
                lWarn() << "rhi/d3d12: swapchain fence event creation failed";
                return false;
            }
        }
    }
    return true;
}

void LD3D12SwapChain::releaseBuffers()
{
    std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(m_rhi.lock());
    if (rhiD == nullptr) {
        return;
    }
    for (int i = 0; i < BUFFER_COUNT; ++i) {
        if (!colorBuffers[i].isNull()) {
            rhiD->releaseQueue.deferredReleaseResource(colorBuffers[i]);
            colorBuffers[i] = {};
        }
        if (rtvs[i].isValid()) {
            rhiD->rtvPool.release(rtvs[i], 1);
            rtvs[i] = {};
        }
        if (rtvsRight[i].isValid()) {
            rhiD->rtvPool.release(rtvsRight[i], 1);
            rtvsRight[i] = {};
        }
        if (!msaaBuffers[i].isNull()) {
            rhiD->releaseQueue.deferredReleaseResource(msaaBuffers[i]);
            msaaBuffers[i] = {};
        }
        if (msaaRtvs[i].isValid()) {
            rhiD->rtvPool.release(msaaRtvs[i], 1);
            msaaRtvs[i] = {};
        }
    }
}

void LD3D12SwapChain::waitCommandCompletionForFrameSlot(int frameSlot)
{
    if (frameSlot < 0 || frameSlot >= LD3D12_FRAMES_IN_FLIGHT) {
        return;
    }
    FrameResources &fr = frameRes[frameSlot];
    if (fr.fence == nullptr || fr.fenceEvent == nullptr || fr.fenceCounter == 0) {
        return;
    }
    if (fr.fence->GetCompletedValue() < fr.fenceCounter) {
        if (SUCCEEDED(fr.fence->SetEventOnCompletion(fr.fenceCounter, fr.fenceEvent))) {
            WaitForSingleObject(fr.fenceEvent, INFINITE);
        }
    }
}

void LD3D12SwapChain::addCommandCompletionSignalForCurrentFrameSlot()
{
    std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(m_rhi.lock());
    if (rhiD == nullptr || rhiD->cmdQueue == nullptr) {
        return;
    }
    FrameResources &fr = frameRes[currentFrameSlot];
    if (fr.fence == nullptr) {
        return;
    }
    ++fr.fenceCounter;
    rhiD->cmdQueue->Signal(fr.fence, fr.fenceCounter);
}

void LD3D12SwapChain::transitionBackBuffer(ID3D12GraphicsCommandList *cmdList,
                                           D3D12_RESOURCE_STATES state)
{
    std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(m_rhi.lock());
    if (cmdList == nullptr || rhiD == nullptr || currentBackBufferIndex >= BUFFER_COUNT) {
        return;
    }
    LD3D12Resource *r = rhiD->resourcePool.lookupRef(colorBuffers[currentBackBufferIndex]);
    if (r == nullptr || r->state == state) {
        return;
    }
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = r->resource;
    barrier.Transition.StateBefore = r->state;
    barrier.Transition.StateAfter = state;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);
    r->state = state;
}

void LD3D12SwapChain::chooseFormats()
{
    colorFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
    srgbAdjustedColorFormat = m_flags.testFlag(LRhiSwapChainFlag::sRGB)
            ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
            : colorFormat;
    hdrColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
}

bool LD3D12SwapChain::createOrResize()
{
    std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(m_rhi.lock());
    if (rhiD == nullptr || rhiD->dev == nullptr || rhiD->cmdQueue == nullptr ||
        rhiD->dxgiFactory == nullptr) {
        lWarn() << "rhi/d3d12: swapchain requires device, queue and factory";
        return false;
    }
    const HWND id = nativeHwnd();
    if (id == nullptr) {
        lWarn() << "rhi/d3d12: window has no native handle yet";
        return false;
    }
    if (!ensureFrameResources()) {
        return false;
    }
    window = m_window;

    const std::shared_ptr<LRhiSwapChain> self = getPtrT<LRhiSwapChain>();
    rtWrapper.setSwapChain(self);
    rtWrapperRight.setSwapChain(self);

    const LSize<int> newSize = surfacePixelSize();
    const bool creating = sourceSwapChain1 == nullptr;
    if (!creating) {
        if (newSize.m_x == pixelSize.m_x && newSize.m_y == pixelSize.m_y) {
            currentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();
            return true;
        }
        for (int i = 0; i < LD3D12_FRAMES_IN_FLIGHT; ++i) {
            waitCommandCompletionForFrameSlot(i);
        }
        releaseBuffers();
        const HRESULT hr = swapChain->ResizeBuffers(0, UINT(newSize.m_x), UINT(newSize.m_y),
                                                    DXGI_FORMAT_UNKNOWN, swapChainFlags);
        if (FAILED(hr)) {
            lWarn() << "rhi/d3d12: ResizeBuffers failed, hr =" << int(hr);
            return false;
        }
    } else {
        chooseFormats();
    }

    pixelSize = newSize;
    m_currentPixelSize = newSize;
    sampleDesc.Count = 1;
    sampleDesc.Quality = 0;

    if (creating) {
        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = UINT(newSize.m_x);
        desc.Height = UINT(newSize.m_y);
        desc.Format = colorFormat;
        desc.SampleDesc = sampleDesc;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = BUFFER_COUNT;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.Flags = swapChainFlags;

        const HRESULT hr = rhiD->dxgiFactory->CreateSwapChainForHwnd(
                    rhiD->cmdQueue, id, &desc, nullptr, nullptr, &sourceSwapChain1);
        if (FAILED(hr)) {
            lWarn() << "rhi/d3d12: CreateSwapChainForHwnd failed, hr =" << int(hr);
            return false;
        }
        if (FAILED(sourceSwapChain1->QueryInterface(__uuidof(IDXGISwapChain3),
                                                    reinterpret_cast<void **>(&swapChain)))) {
            lWarn() << "rhi/d3d12: IDXGISwapChain3 is unavailable";
            sourceSwapChain1->Release();
            sourceSwapChain1 = nullptr;
            return false;
        }
    }

    for (UINT i = 0; i < BUFFER_COUNT; ++i) {
        ID3D12Resource *backBuffer = nullptr;
        if (FAILED(swapChain->GetBuffer(i, __uuidof(ID3D12Resource),
                                        reinterpret_cast<void **>(&backBuffer)))) {
            lWarn() << "rhi/d3d12: GetBuffer failed for back buffer" << int(i);
            return false;
        }
        colorBuffers[i] = LD3D12Resource::addNonOwningToPool(&rhiD->resourcePool,
                                                             backBuffer,
                                                             D3D12_RESOURCE_STATE_PRESENT);
        rtvs[i] = rhiD->rtvPool.allocate(1);
        if (rtvs[i].isValid()) {
            rhiD->dev->CreateRenderTargetView(backBuffer, nullptr, rtvs[i].cpuHandle);
        }
        backBuffer->Release();
    }

    if (m_depthStencil == nullptr || m_depthStencil->m_pixelSize.m_x != pixelSize.m_x ||
        m_depthStencil->m_pixelSize.m_y != pixelSize.m_y) {
        m_depthStencil = rhiD->createRenderBuffer(LRhiRenderBuffer::DepthStencil,
                                                  pixelSize, 1, true,
                                                  LRhiTexture::D24S8);
        if (m_depthStencil == nullptr || !m_depthStencil->create()) {
            lWarn() << "rhi/d3d12: swapchain depth buffer creation failed";
            return false;
        }
        ds = std::dynamic_pointer_cast<LD3D12RenderBuffer>(m_depthStencil);
    }

    if (m_renderPassDesc == nullptr) {
        std::shared_ptr<LD3D12RenderPassDescriptor> rp =
                makeInit<LD3D12RenderPassDescriptor>(m_rhi.lock());
        m_renderPassDesc = rp;
    }
    std::shared_ptr<LD3D12RenderPassDescriptor> rpDesc =
            std::dynamic_pointer_cast<LD3D12RenderPassDescriptor>(m_renderPassDesc);
    rpDesc->colorAttachmentCount = 1;
    rpDesc->hasDepthStencil = ds != nullptr;
    rpDesc->sampleCount = 1;
    rpDesc->dsFormat = ds != nullptr ? int(LRhiTexture::D24S8) : -1;
    rpDesc->colorFormat[0] = int(LRhiTexture::BGRA8);
    rpDesc->updateSerializedFormat();

    rtWrapper.d.rp = rpDesc.get();
    rtWrapper.d.pixelSize = pixelSize;
    rtWrapper.d.dpr = 1.0f;
    rtWrapper.d.sampleCount = 1;
    rtWrapper.d.colorAttCount = 1;
    rtWrapper.d.dsAttCount = ds != nullptr ? 1 : 0;
    rtWrapper.d.dsv = ds != nullptr ? ds->dsv.cpuHandle : D3D12_CPU_DESCRIPTOR_HANDLE{};
    rtWrapper.m_renderPassDesc = m_renderPassDesc;

    currentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();
    rtWrapper.d.rtv[0] = rtvs[currentBackBufferIndex].cpuHandle;
    return true;
}

// ---------------------------------------------------------------------------
// 图形管线
// ---------------------------------------------------------------------------
namespace {

const LShaderCode *lD3d12FindShaderCode(const LRhiShaderStage &stage)
{
    LShaderKey key;
    key.m_variant = stage.m_shaderVariant;
    key.m_source = LShader::DxbcShader;
    auto it = stage.m_shader.m_shaders.find(key);
    if (it != stage.m_shader.m_shaders.end() && it->second != nullptr) {
        return it->second.get();
    }
    key.m_source = LShader::DxilShader;
    it = stage.m_shader.m_shaders.find(key);
    if (it != stage.m_shader.m_shaders.end() && it->second != nullptr) {
        return it->second.get();
    }
    return nullptr;
}

D3D12_BLEND lD3d12Blend(LRhiGraphicsPipeline::BlendFactor f)
{
    using F = LRhiGraphicsPipeline::BlendFactor;
    switch (f) {
    case F::Zero:                   return D3D12_BLEND_ZERO;
    case F::One:                    return D3D12_BLEND_ONE;
    case F::SrcColor:               return D3D12_BLEND_SRC_COLOR;
    case F::OneMinusSrcColor:       return D3D12_BLEND_INV_SRC_COLOR;
    case F::DstColor:               return D3D12_BLEND_DEST_COLOR;
    case F::OneMinusDstColor:       return D3D12_BLEND_INV_DEST_COLOR;
    case F::SrcAlpha:               return D3D12_BLEND_SRC_ALPHA;
    case F::OneMinusSrcAlpha:       return D3D12_BLEND_INV_SRC_ALPHA;
    case F::DstAlpha:               return D3D12_BLEND_DEST_ALPHA;
    case F::OneMinusDstAlpha:       return D3D12_BLEND_INV_DEST_ALPHA;
    case F::ConstantColor:          return D3D12_BLEND_BLEND_FACTOR;
    case F::OneMinusConstantColor:  return D3D12_BLEND_INV_BLEND_FACTOR;
    case F::SrcAlphaSaturate:       return D3D12_BLEND_SRC_ALPHA_SAT;
    default:                        return D3D12_BLEND_ONE;
    }
}

D3D12_BLEND_OP lD3d12BlendOp(LRhiGraphicsPipeline::BlendOp op)
{
    using O = LRhiGraphicsPipeline::BlendOp;
    switch (op) {
    case O::Subtract:        return D3D12_BLEND_OP_SUBTRACT;
    case O::ReverseSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
    case O::Min:             return D3D12_BLEND_OP_MIN;
    case O::Max:             return D3D12_BLEND_OP_MAX;
    default:                 return D3D12_BLEND_OP_ADD;
    }
}

D3D12_COMPARISON_FUNC lD3d12CompareOp(LRhiGraphicsPipeline::CompareOp op)
{
    using C = LRhiGraphicsPipeline::CompareOp;
    switch (op) {
    case C::Never:          return D3D12_COMPARISON_FUNC_NEVER;
    case C::Less:           return D3D12_COMPARISON_FUNC_LESS;
    case C::Equal:          return D3D12_COMPARISON_FUNC_EQUAL;
    case C::LessOrEqual:    return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    case C::Greater:        return D3D12_COMPARISON_FUNC_GREATER;
    case C::NotEqual:       return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    case C::GreaterOrEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    default:                return D3D12_COMPARISON_FUNC_ALWAYS;
    }
}

D3D12_STENCIL_OP lD3d12StencilOp(LRhiGraphicsPipeline::StencilOp op)
{
    using S = LRhiGraphicsPipeline::StencilOp;
    switch (op) {
    case S::StencilZero:          return D3D12_STENCIL_OP_ZERO;
    case S::Keep:                 return D3D12_STENCIL_OP_KEEP;
    case S::Replace:              return D3D12_STENCIL_OP_REPLACE;
    case S::IncrementAndClamp:    return D3D12_STENCIL_OP_INCR_SAT;
    case S::DecrementAndClamp:    return D3D12_STENCIL_OP_DECR_SAT;
    case S::Invert:               return D3D12_STENCIL_OP_INVERT;
    case S::IncrementAndWrap:     return D3D12_STENCIL_OP_INCR;
    case S::DecrementAndWrap:     return D3D12_STENCIL_OP_DECR;
    default:                      return D3D12_STENCIL_OP_KEEP;
    }
}

DXGI_FORMAT lD3d12VertexFormat(LRhiVertexInputAttribute::Format f)
{
    using V = LRhiVertexInputAttribute::Format;
    switch (f) {
    case V::Float4:     return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case V::Float3:     return DXGI_FORMAT_R32G32B32_FLOAT;
    case V::Float2:     return DXGI_FORMAT_R32G32_FLOAT;
    case V::Float:      return DXGI_FORMAT_R32_FLOAT;
    case V::UNormByte4: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case V::UNormByte2: return DXGI_FORMAT_R8G8_UNORM;
    case V::UNormByte:  return DXGI_FORMAT_R8_UNORM;
    case V::UInt4:      return DXGI_FORMAT_R32G32B32A32_UINT;
    case V::UInt3:      return DXGI_FORMAT_R32G32B32_UINT;
    case V::UInt2:      return DXGI_FORMAT_R32G32_UINT;
    case V::UInt:       return DXGI_FORMAT_R32_UINT;
    case V::SInt4:      return DXGI_FORMAT_R32G32B32A32_SINT;
    case V::SInt3:      return DXGI_FORMAT_R32G32B32_SINT;
    case V::SInt2:      return DXGI_FORMAT_R32G32_SINT;
    case V::SInt:       return DXGI_FORMAT_R32_SINT;
    case V::Half4:      return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case V::Half3:      return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case V::Half2:      return DXGI_FORMAT_R16G16_FLOAT;
    case V::Half:       return DXGI_FORMAT_R16_FLOAT;
    case V::UShort4:    return DXGI_FORMAT_R16G16B16A16_UINT;
    case V::UShort3:    return DXGI_FORMAT_R16G16B16A16_UINT;
    case V::UShort2:    return DXGI_FORMAT_R16G16_UINT;
    case V::UShort:     return DXGI_FORMAT_R16_UINT;
    case V::SShort4:    return DXGI_FORMAT_R16G16B16A16_SINT;
    case V::SShort3:    return DXGI_FORMAT_R16G16B16A16_SINT;
    case V::SShort2:    return DXGI_FORMAT_R16G16_SINT;
    case V::SShort:     return DXGI_FORMAT_R16_SINT;
    default:            return DXGI_FORMAT_UNKNOWN;
    }
}

void lD3d12SplitSemantic(const std::string &src, std::string *name, uint32_t *index)
{
    size_t pos = src.size();
    while (pos > 0 && src[pos - 1] >= '0' && src[pos - 1] <= '9') {
        --pos;
    }
    if (pos < src.size()) {
        *name = src.substr(0, pos);
        *index = uint32_t(std::stoul(src.substr(pos)));
    } else {
        *name = src;
        *index = 0;
    }
    if (name->empty()) {
        *name = "TEXCOORD";
    }
}

LD3D12ObjectHandle lD3d12CreateEmptyRootSignature(LRhiD3D12 *rhiD)
{
    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.NumParameters = 0;
    desc.pParameters = nullptr;
    desc.NumStaticSamplers = 0;
    desc.pStaticSamplers = nullptr;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ID3DBlob *signature = nullptr;
    ID3DBlob *error = nullptr;
    if (FAILED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1_0,
                                           &signature, &error))) {
        if (error != nullptr) {
            lWarn() << "rhi/d3d12: root signature serialization failed:"
                    << static_cast<const char *>(error->GetBufferPointer());
            error->Release();
        }
        if (signature != nullptr) {
            signature->Release();
        }
        return {};
    }
    if (error != nullptr) {
        error->Release();
    }

    ID3D12RootSignature *rootSig = nullptr;
    const HRESULT hr = rhiD->dev->CreateRootSignature(0, signature->GetBufferPointer(),
                                                      signature->GetBufferSize(),
                                                      __uuidof(ID3D12RootSignature),
                                                      reinterpret_cast<void **>(&rootSig));
    signature->Release();
    if (FAILED(hr)) {
        lWarn() << "rhi/d3d12: CreateRootSignature failed, hr =" << int(hr);
        return {};
    }
    return LD3D12RootSignature::addToPool(&rhiD->rootSignaturePool, rootSig);
}

} // namespace

LD3D12GraphicsPipeline::LD3D12GraphicsPipeline(std::shared_ptr<LRhiImplementation> rhi)
    : LRhiGraphicsPipeline(rhi), topology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST)
{
}

LD3D12GraphicsPipeline::~LD3D12GraphicsPipeline()
{
    destroy();
}

void LD3D12GraphicsPipeline::destroy()
{
    if (handle.isNull() && (!ownsRootSig || rootSigHandle.isNull())) {
        return;
    }
    std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(m_rhi.lock());
    if (rhiD != nullptr) {
        rhiD->releaseQueue.deferredReleasePipeline(handle);
        if (ownsRootSig) {
            rhiD->releaseQueue.deferredReleaseRootSignature(rootSigHandle);
        }
    }
    handle = {};
    if (ownsRootSig) {
        rootSigHandle = {};
    }
}

bool LD3D12GraphicsPipeline::create()
{
    std::shared_ptr<LRhiD3D12> rhiD = std::dynamic_pointer_cast<LRhiD3D12>(m_rhi.lock());
    if (rhiD == nullptr || rhiD->dev == nullptr) {
        return false;
    }
    destroy();
    ownsRootSig = true;

    std::shared_ptr<LD3D12ShaderResourceBindings> srbD =
            std::dynamic_pointer_cast<LD3D12ShaderResourceBindings>(m_shaderResourceBindings);
    if (srbD != nullptr) {
        if (srbD->rootSigHandle.isNull() && !srbD->create()) {
            lWarn() << "rhi/d3d12: pipeline SRB root signature creation failed";
            return false;
        }
        rootSigHandle = srbD->rootSigHandle;
        ownsRootSig = false;
    } else {
        rootSigHandle = lD3d12CreateEmptyRootSignature(rhiD.get());
    }
    if (rootSigHandle.isNull()) {
        return false;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
    desc.pRootSignature = rhiD->rootSignaturePool.lookup(rootSigHandle).rootSig;

    bool hasVertex = false;
    bool hasFragment = false;
    for (const LRhiShaderStage &stage : m_shaderStages) {
        const LShaderCode *code = lD3d12FindShaderCode(stage);
        if (code == nullptr || code->m_shader.empty()) {
            continue;
        }
        D3D12_SHADER_BYTECODE bytecode{ code->m_shader.data(), code->m_shader.size() };
        if (stage.m_type == LRhiShaderStage::Vertex) {
            desc.VS = bytecode;
            hasVertex = true;
        } else if (stage.m_type == LRhiShaderStage::Fragment) {
            desc.PS = bytecode;
            hasFragment = true;
        }
    }
    if (!hasVertex || !hasFragment) {
        lWarn() << "rhi/d3d12: graphics pipeline requires vertex and fragment shaders";
        return false;
    }

    struct SemanticInfo { std::string name; uint32_t index; };
    std::vector<SemanticInfo> semantics;
    semantics.reserve(m_vertexInputLayout.m_attributes.size());
    for (const LRhiVertexInputAttribute &a : m_vertexInputLayout.m_attributes) {
        std::string src = a.m_name.empty() ? std::string("TEXCOORD") + std::to_string(a.m_location)
                                           : a.m_name;
        SemanticInfo info;
        lD3d12SplitSemantic(src, &info.name, &info.index);
        semantics.push_back(std::move(info));
    }
    std::vector<D3D12_INPUT_ELEMENT_DESC> elements;
    elements.reserve(m_vertexInputLayout.m_attributes.size());
    for (size_t i = 0; i < m_vertexInputLayout.m_attributes.size(); ++i) {
        const LRhiVertexInputAttribute &a = m_vertexInputLayout.m_attributes[i];
        D3D12_INPUT_ELEMENT_DESC e{};
        e.SemanticName = semantics[i].name.c_str();
        e.SemanticIndex = semantics[i].index;
        e.Format = lD3d12VertexFormat(a.m_format);
        e.InputSlot = uint32_t(a.m_binding);
        e.AlignedByteOffset = a.m_offset;
        e.InputSlotClass = a.m_binding < int(m_vertexInputLayout.m_bindings.size()) &&
                m_vertexInputLayout.m_bindings[a.m_binding].m_classification ==
                LRhiVertexInputBinding::PerInstance
                ? D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
                : D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        e.InstanceDataStepRate = e.InputSlotClass == D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA
                ? 1 : 0;
        if (e.Format == DXGI_FORMAT_UNKNOWN) {
            lWarn() << "rhi/d3d12: unsupported vertex attribute format";
            return false;
        }
        elements.push_back(e);
    }
    desc.InputLayout.pInputElementDescs = elements.empty() ? nullptr : elements.data();
    desc.InputLayout.NumElements = UINT(elements.size());

    desc.BlendState.AlphaToCoverageEnable = FALSE;
    desc.BlendState.IndependentBlendEnable = m_targetBlends.size() > 1 ? TRUE : FALSE;
    if (m_targetBlends.empty()) {
        desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    } else {
        for (size_t i = 0; i < m_targetBlends.size() && i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
            const LRhiGraphicsPipeline::TargetBlend &src = m_targetBlends[i];
            D3D12_RENDER_TARGET_BLEND_DESC &dst = desc.BlendState.RenderTarget[i];
            dst.BlendEnable = src.enable ? TRUE : FALSE;
            dst.SrcBlend = lD3d12Blend(src.srcColor);
            dst.DestBlend = lD3d12Blend(src.dstColor);
            dst.BlendOp = lD3d12BlendOp(src.opColor);
            dst.SrcBlendAlpha = lD3d12Blend(src.srcAlpha);
            dst.DestBlendAlpha = lD3d12Blend(src.dstAlpha);
            dst.BlendOpAlpha = lD3d12BlendOp(src.opAlpha);
            dst.RenderTargetWriteMask = uint8_t(src.colorWrite.operator uint32_t() & 0xF);
        }
    }

    desc.RasterizerState.FillMode = m_polygonMode == LRhiGraphicsPipeline::Line
            ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
    desc.RasterizerState.CullMode = m_cullMode == LRhiGraphicsPipeline::None
            ? D3D12_CULL_MODE_NONE
            : (m_cullMode == LRhiGraphicsPipeline::Front ? D3D12_CULL_MODE_FRONT
                                                         : D3D12_CULL_MODE_BACK);
    desc.RasterizerState.FrontCounterClockwise = m_frontFace == LRhiGraphicsPipeline::CCW ? TRUE : FALSE;
    desc.RasterizerState.DepthBias = m_depthBias;
    desc.RasterizerState.SlopeScaledDepthBias = m_slopeScaledDepthBias;

    desc.DepthStencilState.DepthEnable = m_depthTest ? TRUE : FALSE;
    desc.DepthStencilState.DepthWriteMask = m_depthWrite
            ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    desc.DepthStencilState.DepthFunc = lD3d12CompareOp(m_depthOp);
    desc.DepthStencilState.StencilEnable = m_stencilTest ? TRUE : FALSE;
    desc.DepthStencilState.StencilReadMask = uint8_t(m_stencilReadMask);
    desc.DepthStencilState.StencilWriteMask = uint8_t(m_stencilWriteMask);
    desc.DepthStencilState.FrontFace.StencilFailOp = lD3d12StencilOp(m_stencilFront.failOp);
    desc.DepthStencilState.FrontFace.StencilDepthFailOp = lD3d12StencilOp(m_stencilFront.depthFailOp);
    desc.DepthStencilState.FrontFace.StencilPassOp = lD3d12StencilOp(m_stencilFront.passOp);
    desc.DepthStencilState.FrontFace.StencilFunc = lD3d12CompareOp(m_stencilFront.compareOp);
    desc.DepthStencilState.BackFace.StencilFailOp = lD3d12StencilOp(m_stencilBack.failOp);
    desc.DepthStencilState.BackFace.StencilDepthFailOp = lD3d12StencilOp(m_stencilBack.depthFailOp);
    desc.DepthStencilState.BackFace.StencilPassOp = lD3d12StencilOp(m_stencilBack.passOp);
    desc.DepthStencilState.BackFace.StencilFunc = lD3d12CompareOp(m_stencilBack.compareOp);

    switch (m_topology) {
    case LRhiGraphicsPipeline::Triangles:
    case LRhiGraphicsPipeline::TriangleStrip:
    case LRhiGraphicsPipeline::TriangleFan:
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        topology = m_topology == LRhiGraphicsPipeline::TriangleStrip
                ? D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
                : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        break;
    case LRhiGraphicsPipeline::Lines:
    case LRhiGraphicsPipeline::LineStrip:
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        topology = m_topology == LRhiGraphicsPipeline::LineStrip
                ? D3D_PRIMITIVE_TOPOLOGY_LINESTRIP
                : D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        break;
    case LRhiGraphicsPipeline::Points:
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        break;
    default:
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        break;
    }

    desc.NumRenderTargets = 1;
    desc.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
    if (m_renderPassDesc != nullptr) {
        std::shared_ptr<LD3D12RenderPassDescriptor> rp =
                std::dynamic_pointer_cast<LD3D12RenderPassDescriptor>(m_renderPassDesc);
        if (rp != nullptr && rp->colorAttachmentCount > 0 && rp->colorFormat[0] >= 0) {
            desc.RTVFormats[0] = lD3d12TextureFormat(LRhiTexture::Format(rp->colorFormat[0]));
        }
    }
    desc.SampleDesc.Count = UINT((std::max)(1, m_sampleCount));
    desc.SampleDesc.Quality = 0;
    desc.SampleMask = 0xFFFFFFFFu;
    desc.NodeMask = 0;

    ID3D12PipelineState *pso = nullptr;
    const HRESULT hr = rhiD->dev->CreateGraphicsPipelineState(&desc,
                                                              __uuidof(ID3D12PipelineState),
                                                              reinterpret_cast<void **>(&pso));
    if (FAILED(hr)) {
        lWarn() << "rhi/d3d12: CreateGraphicsPipelineState failed, hr =" << int(hr);
        return false;
    }
    handle = LD3D12Pipeline::addToPool(&rhiD->pipelinePool, LD3D12Pipeline::Graphics, pso);
    ++generation;
    return true;
}

LK_NAMESPACE_END