#ifndef LRHID3D12_H
#define LRHID3D12_H
#include "lkgui/rhi/lrhiimplementation.h"
#include "lkgui/rhi/lrhiplatform.h"
#include "lkgui/lwindow.h"

#include <optional>
#include <array>

#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <dcomp.h>
#include <set>

#include "D3D12MemAlloc.h"

LK_NAMESPACE_BEGIN

using D3D12GraphicsCommandList = ID3D12GraphicsCommandList5;

static const int LD3D12_FRAMES_IN_FLIGHT = 2;

class LRhiD3D12;

struct LD3D12Descriptor
{
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = {};
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = {};

    bool isValid() const { return cpuHandle.ptr != 0; }
};

struct LD3D12ReleaseQueue;

struct LD3D12DescriptorHeap
{
    bool isValid() const { return heap && capacity; }
    bool create(ID3D12Device *device,
                uint32_t descriptorCount,
                D3D12_DESCRIPTOR_HEAP_TYPE heapType,
                D3D12_DESCRIPTOR_HEAP_FLAGS heapFlags);
    void createWithExisting(const LD3D12DescriptorHeap &other,
                            uint32_t offsetInDescriptors,
                            uint32_t descriptorCount);
    void destroy();
    void destroyWithDeferredRelease(LD3D12ReleaseQueue *releaseQueue);

    LD3D12Descriptor get(uint32_t count);
    LD3D12Descriptor at(uint32_t index) const;
    uint32_t remainingCapacity() const { return capacity - head; }

    LD3D12Descriptor incremented(const LD3D12Descriptor &descriptor, uint32_t offsetInDescriptors) const
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = descriptor.cpuHandle;
        cpuHandle.ptr += offsetInDescriptors * descriptorByteSize;
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = descriptor.gpuHandle;
        if (gpuHandle.ptr)
            gpuHandle.ptr += offsetInDescriptors * descriptorByteSize;
        return { cpuHandle, gpuHandle };
    }

    ID3D12DescriptorHeap *heap = nullptr;
    uint32_t capacity = 0;
    LD3D12Descriptor heapStart;
    uint32_t head = 0;
    uint32_t descriptorByteSize = 0;
    D3D12_DESCRIPTOR_HEAP_TYPE heapType;
    D3D12_DESCRIPTOR_HEAP_FLAGS heapFlags;
};

struct LD3D12CpuDescriptorPool
{
    bool isValid() const { return !heaps.empty(); }
    bool create(ID3D12Device *device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, const char *debugName = "");
    void destroy();

    LD3D12Descriptor allocate(uint32_t count);
    void release(const LD3D12Descriptor &descriptor, uint32_t count);

    static const int DESCRIPTORS_PER_HEAP = 256;

    struct HeapWithMap {
        LD3D12DescriptorHeap heap;
        std::vector<bool> map;
        static HeapWithMap init(const LD3D12DescriptorHeap &heap, uint32_t descriptorCount) {
            HeapWithMap result;
            result.heap = heap;
            result.map.resize(descriptorCount);
            return result;
        }
    };

    ID3D12Device *device;
    uint32_t descriptorByteSize;
    std::vector<HeapWithMap> heaps;
    const char *debugName;
};

struct LD3D12QueryHeap
{
    bool isValid() const { return heap && capacity; }
    bool create(ID3D12Device *device,
                uint32_t queryCount,
                D3D12_QUERY_HEAP_TYPE heapType);
    void destroy();

    ID3D12QueryHeap *heap = nullptr;
    uint32_t capacity = 0;
};

struct LD3D12StagingArea
{
    static const uint32_t ALIGNMENT = D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT; // 512 so good enough both for cb and texdata

    struct Allocation {
        uint8_t *p = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS gpuAddr = 0;
        ID3D12Resource *buffer = nullptr;
        uint32_t bufferOffset = 0;
        bool isValid() const { return p != nullptr; }
    };

    bool isValid() const { return allocation && mem.isValid(); }
    bool create(LRhiD3D12 *rhi, uint32_t capacity, D3D12_HEAP_TYPE heapType);
    void destroy();
    void destroyWithDeferredRelease(LD3D12ReleaseQueue *releaseQueue);

    Allocation get(uint32_t byteSize);

    uint32_t remainingCapacity() const
    {
        return capacity - head;
    }

    static uint32_t allocSizeForArray(uint32_t size, int count = 1)
    {
        return count * ((size + ALIGNMENT - 1) & ~(ALIGNMENT - 1));
    }

    Allocation mem;
    ID3D12Resource *resource =  nullptr;
    D3D12MA::Allocation *allocation = nullptr;
    uint32_t head;
    uint32_t capacity;
};

struct LD3D12ObjectHandle
{
    uint32_t index = 0;
    uint32_t generation = 0;

    // the default, null handle is guaranteed to give ObjectPool::isValid() == false
    bool isNull() const { return index == 0 && generation == 0; }
};

inline bool operator==(const LD3D12ObjectHandle &a, const LD3D12ObjectHandle &b) noexcept
{
    return a.index == b.index && a.generation == b.generation;
}

inline bool operator!=(const LD3D12ObjectHandle &a, const LD3D12ObjectHandle &b) noexcept
{
    return !(a == b);
}

template<typename T>
struct LD3D12ObjectPool
{
    void create(const char *debugName = "")
    {
        this->debugName = debugName;
        assert(data.empty());
        data.append(Data()); // index 0 is always invalid
    }

    void destroy() {
        int leakCount = 0; // will nicely destroy everything here, but warn about it if enabled
        for (Data &d : data) {
            if (d.object.has_value()) {
                leakCount += 1;
                d.object->releaseResources();
            }
        }
        data.clear();
#ifndef QT_NO_DEBUG
        // debug builds: just do it always
        static bool leakCheck = true;
#else
        // release builds: opt-in
        static bool leakCheck = qEnvironmentVariableIntValue("QT_RHI_LEAK_CHECK");
#endif
        if (leakCheck) {
            if (leakCount > 0) {
                qWarning("LD3D12ObjectPool::destroy(): Pool %p '%s' had %d unreleased objects",
                         this, debugName, leakCount);
            }
        }
    }

    bool isValid(const LD3D12ObjectHandle &handle) const
    {
        return handle.index > 0
               && handle.index < uint32_t(data.count())
               && handle.generation > 0
               && handle.generation == data[handle.index].generation
               && data[handle.index].object.has_value();
    }

    T lookup(const LD3D12ObjectHandle &handle) const
    {
        return isValid(handle) ? *data[handle.index].object : T();
    }

    const T *lookupRef(const LD3D12ObjectHandle &handle) const
    {
        return isValid(handle) ? &*data[handle.index].object : nullptr;
    }

    T *lookupRef(const LD3D12ObjectHandle &handle)
    {
        return isValid(handle) ? &*data[handle.index].object : nullptr;
    }

    LD3D12ObjectHandle add(const T &object)
    {
        assert(!data.empty());
        const uint32_t count = uint32_t(data.size());
        uint32_t index = 1; // index 0 is always invalid
        for (; index < count; ++index) {
            if (!data[index].object.has_value())
                break;
        }
        if (index < count) {
            data[index].object = object;
            uint32_t &generation = data[index].generation;
            generation += 1u;
            return { index, generation };
        } else {
            data.push_back({ object, 1 });
            return { count, 1 };
        }
    }

    void remove(const LD3D12ObjectHandle &handle)
    {
        if (T *object = lookupRef(handle)) {
            object->releaseResources();
            data[handle.index].object.reset();
        }
    }

    const char *debugName;
    struct Data {
        std::optional<T> object;
        uint32_t generation = 0;
    };
    std::vector<Data> data;
};

struct LD3D12Resource
{
    ID3D12Resource *resource;
    D3D12_RESOURCE_STATES state;
    D3D12_RESOURCE_DESC desc;
    D3D12MA::Allocation *allocation;
    void *cpuMapPtr;
    enum { UavUsageRead = 0x01, UavUsageWrite = 0x02 };
    int uavUsage;
    bool owns;

    // note that this assumes the allocation (if there is one) and the resource
    // are separately releaseable, see D3D12MemAlloc docs
    static LD3D12ObjectHandle addToPool(LD3D12ObjectPool<LD3D12Resource> *pool,
                                        ID3D12Resource *resource,
                                        D3D12_RESOURCE_STATES state,
                                        D3D12MA::Allocation *allocation = nullptr,
                                        void *cpuMapPtr = nullptr)
    {
        assert(resource);
        return pool->add({ resource, state, resource->GetDesc(), allocation, cpuMapPtr, 0, true });
    }

    // for LRhiTexture::createFrom() where the ID3D12Resource is not owned by us
    static LD3D12ObjectHandle addNonOwningToPool(LD3D12ObjectPool<LD3D12Resource> *pool,
                                                 ID3D12Resource *resource,
                                                 D3D12_RESOURCE_STATES state)
    {
        assert(resource);
        return pool->add({ resource, state, resource->GetDesc(), nullptr, nullptr, 0, false });
    }

    void releaseResources()
    {
        if (owns) {
            // order matters: resource first, then the allocation
            resource->Release();
            if (allocation)
                allocation->Release();
        }
    }
};

struct LD3D12Pipeline
{
    enum Type {
        Graphics,
        Compute
    };
    Type type;
    ID3D12PipelineState *pso;

    static LD3D12ObjectHandle addToPool(LD3D12ObjectPool<LD3D12Pipeline> *pool,
                                        Type type,
                                        ID3D12PipelineState *pso)
    {
        return pool->add({ type, pso });
    }

    void releaseResources()
    {
        pso->Release();
    }
};

struct LD3D12RootSignature
{
    ID3D12RootSignature *rootSig;

    static LD3D12ObjectHandle addToPool(LD3D12ObjectPool<LD3D12RootSignature> *pool,
                                        ID3D12RootSignature *rootSig)
    {
        return pool->add({ rootSig });
    }

    void releaseResources()
    {
        rootSig->Release();
    }
};

struct LD3D12ReleaseQueue
{
    void create(LD3D12ObjectPool<LD3D12Resource> *resourcePool,
                LD3D12ObjectPool<LD3D12Pipeline> *pipelinePool,
                LD3D12ObjectPool<LD3D12RootSignature> *rootSignaturePool)
    {
        this->resourcePool = resourcePool;
        this->pipelinePool = pipelinePool;
        this->rootSignaturePool = rootSignaturePool;
    }

    void deferredReleaseResource(const LD3D12ObjectHandle &handle);
    void deferredReleaseResourceWithViews(const LD3D12ObjectHandle &handle,
                                          LD3D12CpuDescriptorPool *pool,
                                          const LD3D12Descriptor &viewsStart,
                                          int viewCount);
    void deferredReleasePipeline(const LD3D12ObjectHandle &handle);
    void deferredReleaseRootSignature(const LD3D12ObjectHandle &handle);
    void deferredReleaseCallback(std::function<void(void*)> callback, void *userData);
    void deferredReleaseResourceAndAllocation(ID3D12Resource *resource,
                                              D3D12MA::Allocation *allocation);
    void deferredReleaseDescriptorHeap(ID3D12DescriptorHeap *heap);
    void deferredReleaseViews(LD3D12CpuDescriptorPool *pool,
                              const LD3D12Descriptor &viewsStart,
                              int viewCount);

    void activatePendingDeferredReleaseRequests(int frameSlot);
    void executeDeferredReleases(int frameSlot, bool forced = false);
    void releaseAll();

    struct DeferredReleaseEntry {
        enum Type {
            Resource,
            Pipeline,
            RootSignature,
            Callback,
            ResourceAndAllocation,
            DescriptorHeap,
            Views
        };
        Type type = Resource;
        std::optional<int> frameSlotToBeReleasedIn;
        LD3D12ObjectHandle handle;
        LD3D12CpuDescriptorPool *poolForViews = nullptr;
        LD3D12Descriptor viewsStart;
        int viewCount = 0;
        std::function<void(void*)> callback = nullptr;
        void *callbackUserData = nullptr;
        std::pair<ID3D12Resource *, D3D12MA::Allocation *> resourceAndAllocation = {};
        ID3D12DescriptorHeap *descriptorHeap = nullptr;
    };
    std::vector<DeferredReleaseEntry> queue;
    LD3D12ObjectPool<LD3D12Resource> *resourcePool = nullptr;
    LD3D12ObjectPool<LD3D12Pipeline> *pipelinePool = nullptr;
    LD3D12ObjectPool<LD3D12RootSignature> *rootSignaturePool = nullptr;
};

struct LD3D12CommandBuffer;

struct LD3D12ResourceBarrierGenerator
{
    static const int PREALLOC = 16;

    void create(LD3D12ObjectPool<LD3D12Resource> *resourcePool)
    {
        this->resourcePool = resourcePool;
    }

    void addTransitionBarrier(const LD3D12ObjectHandle &resourceHandle, D3D12_RESOURCE_STATES stateAfter);
    void enqueueBufferedTransitionBarriers(LD3D12CommandBuffer *cbD);
    void enqueueSubresourceTransitionBarrier(LD3D12CommandBuffer *cbD,
                                             const LD3D12ObjectHandle &resourceHandle,
                                             UINT subresource,
                                             D3D12_RESOURCE_STATES stateBefore,
                                             D3D12_RESOURCE_STATES stateAfter);
    void enqueueUavBarrier(LD3D12CommandBuffer *cbD, const LD3D12ObjectHandle &resourceHandle);

    struct TransitionResourceBarrier {
        LD3D12ObjectHandle resourceHandle;
        D3D12_RESOURCE_STATES stateBefore;
        D3D12_RESOURCE_STATES stateAfter;
    };

    LD3D12ResourceBarrierGenerator(){
        transitionResourceBarriers.reserve(PREALLOC);
    }
    std::vector<TransitionResourceBarrier> transitionResourceBarriers;
    LD3D12ObjectPool<LD3D12Resource> *resourcePool = nullptr;
};

struct LD3D12ShaderBytecodeCache
{
    struct Shader {
        Shader() = default;
        Shader(const std::vector<uint8_t> &bytecode, const LShader::NativeResourceBindingMap &rbm)
            : bytecode(bytecode), nativeResourceBindingMap(rbm)
        { }
        std::vector<uint8_t> bytecode;
        LShader::NativeResourceBindingMap nativeResourceBindingMap;
    };

    std::unordered_map<LRhiShaderStage, Shader> data;

    void insertWithCapacityLimit(const LRhiShaderStage &key, const Shader &s);
};

struct LD3D12ShaderVisibleDescriptorHeap
{
    bool create(ID3D12Device *device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t perFrameDescriptorCount);
    void destroy();
    void destroyWithDeferredRelease(LD3D12ReleaseQueue *releaseQueue);

    LD3D12DescriptorHeap heap;
    LD3D12DescriptorHeap perFrameHeapSlice[LD3D12_FRAMES_IN_FLIGHT];
};

// wrap foreign struct so we can legally supply equality operators and qHash:
struct L_D3D12_SAMPLER_DESC
{
    D3D12_SAMPLER_DESC desc;

    friend bool operator==(const L_D3D12_SAMPLER_DESC &lhs, const L_D3D12_SAMPLER_DESC &rhs) noexcept
    {
        return lhs.desc.Filter == rhs.desc.Filter
               && lhs.desc.AddressU == rhs.desc.AddressU
               && lhs.desc.AddressV == rhs.desc.AddressV
               && lhs.desc.AddressW == rhs.desc.AddressW
               && lhs.desc.MipLODBias == rhs.desc.MipLODBias
               && lhs.desc.MaxAnisotropy == rhs.desc.MaxAnisotropy
               && lhs.desc.ComparisonFunc == rhs.desc.ComparisonFunc
               // BorderColor is never used, skip it
               && lhs.desc.MinLOD == rhs.desc.MinLOD
               && lhs.desc.MaxLOD == rhs.desc.MaxLOD;
    }

    friend bool operator!=(const L_D3D12_SAMPLER_DESC &lhs, const L_D3D12_SAMPLER_DESC &rhs) noexcept
    {
        return !(lhs == rhs);
    }
};
LK_NAMESPACE_END

template<>
struct std::hash<_LK::L_D3D12_SAMPLER_DESC>{
    std::size_t operator()(const _LK::L_D3D12_SAMPLER_DESC& key) const noexcept
    {
        uint32_t seed=0;
        seed = _LK::lHashCombine(seed, key.desc.Filter);
        seed = _LK::lHashCombine(seed, key.desc.AddressU);
        seed = _LK::lHashCombine(seed, key.desc.AddressV);
        seed = _LK::lHashCombine(seed, key.desc.AddressW);
        seed = _LK::lHashCombine(seed, key.desc.MipLODBias);
        seed = _LK::lHashCombine(seed, key.desc.MaxAnisotropy);
        seed = _LK::lHashCombine(seed, key.desc.ComparisonFunc);
        // BorderColor is never used, skip it
        seed = _LK::lHashCombine(seed, key.desc.MinLOD);
        seed = _LK::lHashCombine(seed, key.desc.MaxLOD);
        return seed;
    }
};

LK_NAMESPACE_BEGIN
struct LD3D12SamplerManager
{
    const uint32_t MAX_SAMPLERS = 512;

    bool create(ID3D12Device *device);
    void destroy();

    LD3D12Descriptor getShaderVisibleDescriptor(const D3D12_SAMPLER_DESC &desc);

    ID3D12Device *device = nullptr;
    LD3D12ShaderVisibleDescriptorHeap shaderVisibleSamplerHeap;
    std::unordered_map<L_D3D12_SAMPLER_DESC, LD3D12Descriptor> gpuMap;
};

enum LD3D12Stage { VS = 0, HS, DS, GS, PS, CS };

static inline LD3D12Stage LD3D12_stage(LRhiShaderStage::Type type)
{
    switch (type) {
    case LRhiShaderStage::Vertex:
        return VS;
    case LRhiShaderStage::TessellationControl:
        return HS;
    case LRhiShaderStage::TessellationEvaluation:
        return DS;
    case LRhiShaderStage::Geometry:
        return GS;
    case LRhiShaderStage::Fragment:
        return PS;
    case LRhiShaderStage::Compute:
        return CS;
    }
    return (VS);
}

static inline D3D12_SHADER_VISIBILITY LD3D12_stageToVisibility(LD3D12Stage s)
{
    switch (s) {
    case VS:
        return D3D12_SHADER_VISIBILITY_VERTEX;
    case HS:
        return D3D12_SHADER_VISIBILITY_HULL;
    case DS:
        return D3D12_SHADER_VISIBILITY_DOMAIN;
    case GS:
        return D3D12_SHADER_VISIBILITY_GEOMETRY;
    case PS:
        return D3D12_SHADER_VISIBILITY_PIXEL;
    case CS:
        return D3D12_SHADER_VISIBILITY_ALL;
    }
    return (D3D12_SHADER_VISIBILITY_ALL);
}

static inline LFlags<LRhiShaderResourceBindingStage> LD3D12_stageToSrb(LD3D12Stage s)
{
    switch (s) {
    case VS:
        return LRhiShaderResourceBindingStage::Vertex;
    case HS:
        return LRhiShaderResourceBindingStage::TessellationControl;
    case DS:
        return LRhiShaderResourceBindingStage::TessellationEvaluation;
    case GS:
        return LRhiShaderResourceBindingStage::Geometry;
    case PS:
        return LRhiShaderResourceBindingStage::Fragment;
    case CS:
        return LRhiShaderResourceBindingStage::Compute;
    }
    return (LRhiShaderResourceBindingStage::Vertex);
}

struct LD3D12ShaderStageData
{
    bool valid = false; // to allow simple arrays where unused stages are indicated by !valid
    LD3D12Stage stage = VS;
    LShader::NativeResourceBindingMap nativeResourceBindingMap;
};

struct LD3D12ShaderResourceBindings;

struct LD3D12ShaderResourceVisitor
{
    enum StorageOp { Load = 0, Store, LoadStore };

    LD3D12ShaderResourceVisitor(const LD3D12ShaderResourceBindings *srb,
                                const LD3D12ShaderStageData *stageData,
                                int stageCount)
        : srb(srb),
        stageData(stageData),
        stageCount(stageCount)
    {
    }

    std::function<void(LD3D12Stage, const LRhiShaderResourceBinding::UniformBufferData &, int, int)> uniformBuffer = nullptr;
    std::function<void(LD3D12Stage, const LRhiShaderResourceBinding::TextureAndSampler &, int)> texture = nullptr;
    std::function<void(LD3D12Stage, const LRhiShaderResourceBinding::TextureAndSampler &, int)> sampler = nullptr;
    std::function<void(LD3D12Stage, const LRhiShaderResourceBinding::StorageImageData &, StorageOp, int)> storageImage = nullptr;
    std::function<void(LD3D12Stage, const LRhiShaderResourceBinding::StorageBufferData &, StorageOp, int)> storageBuffer = nullptr;

    void visit();

    const LD3D12ShaderResourceBindings *srb;
    const LD3D12ShaderStageData *stageData;
    int stageCount;
};

struct LD3D12Readback
{
    // common
    int frameSlot = -1;
    LRhiReadbackResult *result = nullptr;
    LD3D12StagingArea staging;
    uint32_t byteSize = 0;
    // textures
    uint32_t bytesPerLine = 0;
    LSize<int> pixelSize;
    LRhiTexture::Format format = LRhiTexture::UnknownFormat;
    uint32_t stagingRowPitch = 0;
};

struct LD3D12MipmapGenerator
{
    bool create(LRhiD3D12 *rhiD);
    void destroy();
    void generate(LD3D12CommandBuffer *cbD, const LD3D12ObjectHandle &textureHandle);

    LRhiD3D12 *rhiD;
    LD3D12ObjectHandle rootSigHandle;
    LD3D12ObjectHandle pipelineHandle;
};

struct LD3D12MipmapGenerator3D
{
    bool create(LRhiD3D12 *rhiD);
    void destroy();
    void generate(LD3D12CommandBuffer *cbD, const LD3D12ObjectHandle &textureHandle);

    LRhiD3D12 *rhiD;
    LD3D12ObjectHandle rootSigHandle;
    LD3D12ObjectHandle pipelineHandle;
};

struct LD3D12MemoryAllocator
{
    bool create(ID3D12Device *device, IDXGIAdapter1 *adapter);
    void destroy();

    HRESULT createResource(D3D12_HEAP_TYPE heapType,
                           const D3D12_RESOURCE_DESC *resourceDesc,
                           D3D12_RESOURCE_STATES initialState,
                           const D3D12_CLEAR_VALUE *optimizedClearValue,
                           D3D12MA::Allocation **maybeAllocation,
                           REFIID riidResource,
                           void **ppvResource);

    void getBudget(D3D12MA::Budget *localBudget, D3D12MA::Budget *nonLocalBudget);

    bool isUsingD3D12MA() const { return allocator != nullptr; }

    ID3D12Device *device = nullptr;
    D3D12MA::Allocator *allocator = nullptr;
};

struct LD3D12Buffer : public LRhiBuffer
{
    LD3D12Buffer(std::shared_ptr<LRhiImplementation> rhi, Type type, UsageFlags usage, uint32_t size);
    ~LD3D12Buffer();

    void destroy() override;
    bool create() override;
    LRhiBuffer::NativeBuffer nativeBuffer() override;
    char *beginFullDynamicBufferUpdateForCurrentFrame() override;
    void endFullDynamicBufferUpdateForCurrentFrame() override;

    void executeHostWritesForFrameSlot(int frameSlot);

    LD3D12ObjectHandle handles[LD3D12_FRAMES_IN_FLIGHT] = {};
    struct HostWrite {
        uint32_t offset;
        std::vector<uint8_t> data;
    };
    std::vector<HostWrite> pendingHostWrites[LD3D12_FRAMES_IN_FLIGHT];
    friend class LRhiD3D12;
    friend struct LD3D12CommandBuffer;
};

struct LD3D12RenderBuffer : public LRhiRenderBuffer
{
    LD3D12RenderBuffer(std::shared_ptr<LRhiImplementation> rhi,
                       Type type,
                       const LSize<int> &pixelSize,
                       int sampleCount,
                       bool usedWithSwapChainOnly,
                       LRhiTexture::Format backingFormatHint);
    ~LD3D12RenderBuffer();
    void destroy() override;
    bool create() override;
    LRhiTexture::Format backingFormat() const override;

    static const DXGI_FORMAT DS_FORMAT = DXGI_FORMAT_D24_UNORM_S8_UINT;

    LD3D12ObjectHandle handle;
    LD3D12Descriptor rtv;
    LD3D12Descriptor dsv;
    DXGI_FORMAT dxgiFormat;
    DXGI_SAMPLE_DESC sampleDesc;
    uint32_t generation = 0;
    friend class LRhiD3D12;
};

struct LD3D12Texture : public LRhiTexture
{
    LD3D12Texture(std::shared_ptr<LRhiImplementation> rhi, Format format, const LSize<int> &pixelSize, int depth,
                  int arraySize, int sampleCount, Flags flags);
    ~LD3D12Texture();
    void destroy() override;
    bool create() override;
    bool createFrom(NativeTexture src) override;
    NativeTexture nativeTexture() override;
    void setNativeLayout(int layout) override;

    bool prepareCreate(LSize<int> *adjustedSize = nullptr);
    bool finishCreate();

    LD3D12ObjectHandle handle;
    LD3D12Descriptor srv;
    DXGI_FORMAT dxgiFormat;
    DXGI_FORMAT srvFormat;
    DXGI_FORMAT rtFormat; // RTV/DSV/UAV
    uint32_t mipLevelCount;
    DXGI_SAMPLE_DESC sampleDesc;
    uint32_t generation = 0;
    friend class LRhiD3D12;
    friend struct LD3D12CommandBuffer;
};

struct LD3D12Sampler : public LRhiSampler
{
    LD3D12Sampler(std::shared_ptr<LRhiImplementation> rhi, Filter magFilter, Filter minFilter, Filter mipmapMode,
                  AddressMode u, AddressMode v, AddressMode w);
    ~LD3D12Sampler();
    void destroy() override;
    bool create() override;

    LD3D12Descriptor lookupOrCreateShaderVisibleDescriptor();

    D3D12_SAMPLER_DESC desc = {};
    LD3D12Descriptor shaderVisibleDescriptor;
};

struct LD3D12ShadingRateMap : public LRhiShadingRateMap
{
    LD3D12ShadingRateMap(std::shared_ptr<LRhiImplementation> rhi);
    ~LD3D12ShadingRateMap();
    void destroy() override;
    bool createFrom(std::shared_ptr<LRhiTexture> src) override;

    LD3D12ObjectHandle handle; // just copied from the texture
    friend class LRhiD3D12;
};

struct LD3D12RenderPassDescriptor : public LRhiRenderPassDescriptor
{
    LD3D12RenderPassDescriptor(std::shared_ptr<LRhiImplementation> rhi);
    ~LD3D12RenderPassDescriptor();
    void destroy() override;
    bool isCompatible(const std::shared_ptr<LRhiRenderPassDescriptor> other) const override;
    std::shared_ptr<LRhiRenderPassDescriptor> newCompatibleRenderPassDescriptor() const override;
    std::vector<uint32_t> serializedFormat() const override;

    void updateSerializedFormat();

    static const int MAX_COLOR_ATTACHMENTS = 8;
    int colorAttachmentCount = 0;
    bool hasDepthStencil = false;
    int colorFormat[MAX_COLOR_ATTACHMENTS];
    int dsFormat;
    bool hasShadingRateMap = false;
    std::vector<uint32_t> serializedFormatData;
};

struct LD3D12RenderTargetData
{
    LD3D12RenderTargetData(std::shared_ptr<LRhiImplementation> ) { }

    LD3D12RenderPassDescriptor *rp = nullptr;
    LSize<int> pixelSize;
    float dpr = 1;
    int sampleCount = 1;
    int colorAttCount = 0;
    int dsAttCount = 0;
    LRhiRenderTargetAttachmentTracker::ResIdList currentResIdList;
    static const int MAX_COLOR_ATTACHMENTS = LD3D12RenderPassDescriptor::MAX_COLOR_ATTACHMENTS;
    D3D12_CPU_DESCRIPTOR_HANDLE rtv[MAX_COLOR_ATTACHMENTS];
    D3D12_CPU_DESCRIPTOR_HANDLE dsv;
};

struct LD3D12SwapChainRenderTarget : public LRhiSwapChainRenderTarget
{
    LD3D12SwapChainRenderTarget(std::shared_ptr<LRhiImplementation> rhi, LRhiSwapChain *swapchain);
    ~LD3D12SwapChainRenderTarget();
    void destroy() override;

    LSize<int> pixelSize() const override;
    float devicePixelRatio() const override;
    int sampleCount() const override;

    LD3D12RenderTargetData d;
};

struct LD3D12TextureRenderTarget : public LRhiTextureRenderTarget
{
    LD3D12TextureRenderTarget(std::shared_ptr<LRhiImplementation> rhi,
                              const LRhiTextureRenderTargetDescription &desc,
                              Flags flags);
    ~LD3D12TextureRenderTarget();
    void destroy() override;

    LSize<int> pixelSize() const override;
    float devicePixelRatio() const override;
    int sampleCount() const override;

    std::shared_ptr<LRhiRenderPassDescriptor> newCompatibleRenderPassDescriptor() override;
    bool create() override;

    LD3D12RenderTargetData d;
    bool ownsRtv[LD3D12RenderTargetData::MAX_COLOR_ATTACHMENTS];
    LD3D12Descriptor rtv[LD3D12RenderTargetData::MAX_COLOR_ATTACHMENTS];
    bool ownsDsv = false;
    LD3D12Descriptor dsv;
    friend class LRhiD3D12;
};

struct LD3D12GraphicsPipeline;
struct LD3D12ComputePipeline;

struct LD3D12ShaderResourceBindings : public LRhiShaderResourceBindings
{
    LD3D12ShaderResourceBindings(std::shared_ptr<LRhiImplementation> rhi);
    ~LD3D12ShaderResourceBindings();
    void destroy() override;
    bool create() override;
    void updateResources(bool bindingsAreSorted = false) override;

    LD3D12ObjectHandle createRootSignature(const LD3D12ShaderStageData *stageData, int stageCount);

    struct VisitorData {
        std::vector<D3D12_ROOT_PARAMETER1> cbParams[6];

        D3D12_ROOT_PARAMETER1 srvTables[6] = {};
        std::vector<D3D12_DESCRIPTOR_RANGE1> srvRanges[6];
        uint32_t currentSrvRangeOffset[6] = {};

        std::vector<D3D12_ROOT_PARAMETER1> samplerTables[6];
        std::array<D3D12_DESCRIPTOR_RANGE1, 16> samplerRanges[6] = {};
        int samplerRangeHeads[6] = {};

        D3D12_ROOT_PARAMETER1 uavTables[6] = {};
        std::vector<D3D12_DESCRIPTOR_RANGE1> uavRanges[6];
        uint32_t currentUavRangeOffset[6] = {};

        VisitorData(){
            for(int i=0; i< 6; ++i){
                cbParams[i].reserve(2);
                srvRanges[i].reserve(4);
                samplerTables[i].reserve(4);
                uavRanges[i].reserve(4);
            }
        }
    } visitorData;


    void visitUniformBuffer(LD3D12Stage s,
                            const LRhiShaderResourceBinding::UniformBufferData &d,
                            int shaderRegister,
                            int binding);
    void visitTexture(LD3D12Stage s,
                      const LRhiShaderResourceBinding::TextureAndSampler &d,
                      int shaderRegister);
    void visitSampler(LD3D12Stage s,
                      const LRhiShaderResourceBinding::TextureAndSampler &d,
                      int shaderRegister);
    void visitStorageBuffer(LD3D12Stage s,
                            const LRhiShaderResourceBinding::StorageBufferData &d,
                            LD3D12ShaderResourceVisitor::StorageOp op,
                            int shaderRegister);
    void visitStorageImage(LD3D12Stage s,
                           const LRhiShaderResourceBinding::StorageImageData &d,
                           LD3D12ShaderResourceVisitor::StorageOp op,
                           int shaderRegister);

    bool hasDynamicOffset = false;
    uint32_t generation = 0;
    LD3D12GraphicsPipeline *lastUsedGraphicsPipeline = nullptr;
    LD3D12ComputePipeline *lastUsedComputePipeline = nullptr;

    friend class LRhiD3D12;
    friend struct LD3D12ShaderResourceVisitor;
};

struct LD3D12GraphicsPipeline : public LRhiGraphicsPipeline
{
    LD3D12GraphicsPipeline(std::shared_ptr<LRhiImplementation> rhi);
    ~LD3D12GraphicsPipeline();
    void destroy() override;
    bool create() override;

    LD3D12ObjectHandle handle;
    LD3D12ObjectHandle rootSigHandle;
    std::array<LD3D12ShaderStageData, 5> stageData;
    D3D12_PRIMITIVE_TOPOLOGY topology;
    UINT viewInstanceMask = 0;
    uint32_t generation = 0;
    friend class LRhiD3D12;
};

struct LD3D12ComputePipeline : public LRhiComputePipeline
{
    LD3D12ComputePipeline(std::shared_ptr<LRhiImplementation> rhi);
    ~LD3D12ComputePipeline();
    void destroy() override;
    bool create() override;

    LD3D12ObjectHandle handle;
    LD3D12ObjectHandle rootSigHandle;
    LD3D12ShaderStageData stageData;
    uint32_t generation = 0;
    friend class LRhiD3D12;
};

struct LD3D12CommandBuffer : public LRhiCommandBuffer
{
    LD3D12CommandBuffer(std::shared_ptr<LRhiImplementation> rhi);
    ~LD3D12CommandBuffer();
    void destroy() override;

    const LRhiNativeHandles *nativeHandles();

    D3D12GraphicsCommandList *cmdList = nullptr; // not owned
    LRhiD3D12CommandBufferNativeHandles nativeHandlesStruct;

    enum PassType {
        NoPass,
        RenderPass,
        ComputePass
    };

    void resetState()
    {
        recordingPass = NoPass;
        currentTarget = nullptr;

        resetPerPassState();
    }

    void resetPerPassState()
    {
        currentGraphicsPipeline = nullptr;
        currentComputePipeline = nullptr;
        currentPipelineGeneration = 0;
        currentGraphicsSrb = nullptr;
        currentComputeSrb = nullptr;
        currentSrbGeneration = 0;
        currentIndexBuffer = {};
        currentIndexOffset = 0;
        currentIndexFormat = DXGI_FORMAT_R16_UINT;
        currentVertexBuffers = {};
        currentVertexOffsets = {};
        hasShadingRateSet = false;
        hasShadingRateMapSet = false;
    }

    // per-frame
    PassType recordingPass;
    LRhiRenderTarget *currentTarget;

    // per-pass
    LD3D12GraphicsPipeline *currentGraphicsPipeline;
    LD3D12ComputePipeline *currentComputePipeline;
    uint32_t currentPipelineGeneration;
    LRhiShaderResourceBindings *currentGraphicsSrb;
    LRhiShaderResourceBindings *currentComputeSrb;
    uint32_t currentSrbGeneration;
    LD3D12ObjectHandle currentIndexBuffer;
    uint32_t currentIndexOffset;
    DXGI_FORMAT currentIndexFormat;
    std::array<LD3D12ObjectHandle, D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> currentVertexBuffers;
    std::array<uint32_t, D3D12_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT> currentVertexOffsets;
    bool hasShadingRateSet;
    bool hasShadingRateMapSet;

    // global
    double lastGpuTime = 0;

    // per-setShaderResources
    struct VisitorData {
        std::vector<std::pair<LD3D12ObjectHandle, uint32_t>> cbufs[6];
        std::vector<LD3D12Descriptor> srvs[6];
        std::vector<LD3D12Descriptor> samplers[6];
        std::vector<std::pair<LD3D12ObjectHandle, D3D12_UNORDERED_ACCESS_VIEW_DESC>> uavs[6];

        VisitorData(){
            for(int i=0;i<6; ++i){
                cbufs[i].reserve(4);
                srvs[i].reserve(8);
                samplers[i].reserve(8);
                uavs[i].reserve(4);
            }
        }
    } visitorData;

    void visitUniformBuffer(LD3D12Stage s,
                            const LRhiShaderResourceBinding::UniformBufferData &d,
                            int shaderRegister,
                            int binding,
                            int dynamicOffsetCount,
                            const LRhiCommandBuffer::DynamicOffset *dynamicOffsets);
    void visitTexture(LD3D12Stage s,
                      const LRhiShaderResourceBinding::TextureAndSampler &d,
                      int shaderRegister);
    void visitSampler(LD3D12Stage s,
                      const LRhiShaderResourceBinding::TextureAndSampler &d,
                      int shaderRegister);
    void visitStorageBuffer(LD3D12Stage s,
                            const LRhiShaderResourceBinding::StorageBufferData &d,
                            LD3D12ShaderResourceVisitor::StorageOp op,
                            int shaderRegister);
    void visitStorageImage(LD3D12Stage s,
                           const LRhiShaderResourceBinding::StorageImageData &d,
                           LD3D12ShaderResourceVisitor::StorageOp op,
                           int shaderRegister);
};

struct LD3D12SwapChain : public LRhiSwapChain
{
    LD3D12SwapChain(std::shared_ptr<LRhiImplementation> rhi);
    ~LD3D12SwapChain();
    void destroy() override;

    std::shared_ptr<LRhiCommandBuffer> currentFrameCommandBuffer() override;
    std::shared_ptr<LRhiRenderTarget> currentFrameRenderTarget() override;
    std::shared_ptr<LRhiRenderTarget> currentFrameRenderTarget(StereoTargetBuffer targetBuffer) override;

    LSize<int> surfacePixelSize() override;
    bool isFormatSupported(Format f) override;
    LRhiSwapChainHdrInfo hdrInfo() override;

    std::shared_ptr<LRhiRenderPassDescriptor> newCompatibleRenderPassDescriptor() override;
    bool createOrResize() override;

    void releaseBuffers();
    void waitCommandCompletionForFrameSlot(int frameSlot);
    void addCommandCompletionSignalForCurrentFrameSlot();
    void chooseFormats();

    std::shared_ptr<LWindow> window = nullptr;
    IDXGISwapChain1 *sourceSwapChain1 = nullptr;
    IDXGISwapChain3 *swapChain = nullptr;
    LSize<int> pixelSize;
    UINT swapInterval = 1;
    UINT swapChainFlags = 0;
    BOOL stereo = false;
    DXGI_FORMAT colorFormat;
    DXGI_FORMAT srgbAdjustedColorFormat;
    DXGI_COLOR_SPACE_TYPE hdrColorSpace;
    IDCompositionTarget *dcompTarget = nullptr;
    IDCompositionVisual *dcompVisual = nullptr;
    static const UINT BUFFER_COUNT = 3;
    LD3D12ObjectHandle colorBuffers[BUFFER_COUNT];
    LD3D12Descriptor rtvs[BUFFER_COUNT];
    LD3D12Descriptor rtvsRight[BUFFER_COUNT];
    DXGI_SAMPLE_DESC sampleDesc;
    LD3D12ObjectHandle msaaBuffers[BUFFER_COUNT];
    LD3D12Descriptor msaaRtvs[BUFFER_COUNT];
    LD3D12RenderBuffer *ds = nullptr;
    UINT currentBackBufferIndex = 0;
    LD3D12SwapChainRenderTarget rtWrapper;
    LD3D12SwapChainRenderTarget rtWrapperRight;
    LD3D12CommandBuffer cbWrapper;
    HANDLE frameLatencyWaitableObject = nullptr;

    struct FrameResources {
        ID3D12Fence *fence = nullptr;
        HANDLE fenceEvent = nullptr;
        UINT64 fenceCounter = 0;
        D3D12GraphicsCommandList *cmdList = nullptr;
    } frameRes[LD3D12_FRAMES_IN_FLIGHT];

    int currentFrameSlot = 0; // index in frameRes
    int lastFrameLatencyWaitSlot = -1;
};

template<typename T, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type>
struct alignas(void*) LD3D12PipelineStateSubObject
{
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE type = Type;
    T object = {};
};

class LD3D12Adapter : public LRhiAdapter
{
public:
    LRhiDriverInfo info() const override;

    LUID luid;
    LRhiDriverInfo adapterInfo;
};


class LRhiD3d12 : public LRhiImplementation
{
public:
    // gets no space from this will get their own temporary staging areas.
    static const uint32_t SMALL_STAGING_AREA_BYTES_PER_FRAME = 16 * 1024 * 1024;
    static const uint32_t SHADER_VISIBLE_CBV_SRV_UAV_HEAP_PER_FRAME_START_SIZE = 16384;

    LRhiD3d12(std::shared_ptr<LRhiD3D12InitParams> params,
              std::shared_ptr<LRhiD3D12NativeHandles> importDevice = nullptr);

    virtual bool create(LFlags<LRhiFlag> flags) override;
    virtual void destroy() override;

    virtual std::shared_ptr<LRhiGraphicsPipeline> createGraphicsPipeline() override;
    virtual std::shared_ptr<LRhiComputePipeline> createComputePipeline() override;
    virtual std::shared_ptr<LRhiShaderResourceBindings> createShaderResourceBindings() override;
    virtual std::shared_ptr<LRhiBuffer> createBuffer(LRhiBuffer::Type type,
                                                     LRhiBuffer::UsageFlags usage,
                                                     uint32_t size) override;
    virtual std::shared_ptr<LRhiRenderBuffer> createRenderBuffer(LRhiRenderBuffer::Type type,
                                                                 const LSize<int> &pixelSize,
                                                                 int sampleCount,
                                                                 bool usedWithSwapChainOnly,
                                                                 LRhiTexture::Format backingFormatHint) override;
    virtual std::shared_ptr<LRhiTexture> createTexture(LRhiTexture::Format format,
                                                       const LSize<int> &pixelSize,
                                                       int depth,
                                                       int arraySize,
                                                       int sampleCount,
                                                       LRhiTexture::Flags flags) override;
    virtual std::shared_ptr<LRhiSampler> createSampler(LRhiSampler::Filter magFilter,
                                                       LRhiSampler::Filter minFilter,
                                                       LRhiSampler::Filter mipmapMode,
                                                       LRhiSampler:: AddressMode u,
                                                       LRhiSampler::AddressMode v,
                                                       LRhiSampler::AddressMode w) override;

    virtual std::shared_ptr<LRhiTextureRenderTarget> createTextureRenderTarget(const LRhiTextureRenderTargetDescription &desc,
                                                                               LRhiTextureRenderTarget::Flags flags) override;

    virtual std::shared_ptr<LRhiShadingRateMap> createShadingRateMap() override;

    virtual std::shared_ptr<LRhiSwapChain> createSwapChain() override;
    virtual LRhi::FrameOpResult beginFrame(std::shared_ptr<LRhiSwapChain> swapChain, LRhi::BeginFrameFlags flags) override;
    virtual LRhi::FrameOpResult endFrame(std::shared_ptr<LRhiSwapChain> swapChain, LRhi::EndFrameFlags flags) override;
    virtual LRhi::FrameOpResult beginOffscreenFrame(std::shared_ptr<LRhiCommandBuffer> *cb, LRhi::BeginFrameFlags flags) override;
    virtual LRhi::FrameOpResult endOffscreenFrame(LRhi::EndFrameFlags flags) override;
    virtual LRhi::FrameOpResult finish() override;

    virtual void resourceUpdate(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates) override;

    virtual void beginPass(std::shared_ptr<LRhiCommandBuffer> cb,
                           std::shared_ptr<LRhiRenderTarget> rt,
                           const LColor &colorClearValue,
                           const LRhiDepthStencilClearValue &depthStencilClearValue,
                           std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates,
                           LRhiCommandBuffer::BeginPassFlags flags) override;
    virtual void endPass(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates) override;

    virtual void setGraphicsPipeline(std::shared_ptr<LRhiCommandBuffer> cb,
                                     std::shared_ptr<LRhiGraphicsPipeline> ps) override;

    virtual void setShaderResources(std::shared_ptr<LRhiCommandBuffer> cb,
                                    std::shared_ptr<LRhiShaderResourceBindings> srb,
                                    int dynamicOffsetCount,
                                    const LRhiCommandBuffer::DynamicOffset *dynamicOffsets) override;

    virtual void setVertexInput(std::shared_ptr<LRhiCommandBuffer> cb,
                                int startBinding, int bindingCount, const LRhiCommandBuffer::VertexInput *bindings,
                                std::shared_ptr<LRhiBuffer> indexBuf, uint32_t indexOffset,
                                LRhiCommandBuffer::IndexFormat indexFormat) override;

    virtual void setViewport(std::shared_ptr<LRhiCommandBuffer> cb, const LRhiViewport &viewport) override;
    virtual void setScissor(std::shared_ptr<LRhiCommandBuffer> cb, const LRhiScissor &scissor) override;
    virtual void setBlendConstants(std::shared_ptr<LRhiCommandBuffer> cb, const LColor &c) override;
    virtual void setStencilRef(std::shared_ptr<LRhiCommandBuffer> cb, uint32_t refValue) override;
    virtual void setShadingRate(std::shared_ptr<LRhiCommandBuffer> cb, const LSize<int> &coarsePixelSize) override;

    virtual void draw(std::shared_ptr<LRhiCommandBuffer> cb, uint32_t vertexCount,
                      uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance) override;
    virtual void drawIndexed(std::shared_ptr<LRhiCommandBuffer> cb, uint32_t indexCount,
                             uint32_t instanceCount, uint32_t firstIndex,
                             int32_t vertexOffset, uint32_t firstInstance) override;

    virtual void debugMarkBegin(std::shared_ptr<LRhiCommandBuffer> cb, const std::string &name) override;
    virtual void debugMarkEnd(std::shared_ptr<LRhiCommandBuffer> cb) override;
    virtual void debugMarkMsg(std::shared_ptr<LRhiCommandBuffer> cb, const std::string &msg) override;

    virtual void beginComputePass(std::shared_ptr<LRhiCommandBuffer> cb,
                                  std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates,
                                  LRhiCommandBuffer::BeginPassFlags flags) override;
    virtual void endComputePass(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates) override;
    virtual void setComputePipeline(std::shared_ptr<LRhiCommandBuffer> cb, std::shared_ptr<LRhiComputePipeline> ps) override;
    virtual void dispatch(std::shared_ptr<LRhiCommandBuffer> cb, int x, int y, int z) override;

    virtual const std::shared_ptr<LRhiNativeHandles> nativeHandles(std::shared_ptr<LRhiCommandBuffer> cb) override;
    virtual void beginExternal(std::shared_ptr<LRhiCommandBuffer> cb) override;
    virtual void endExternal(std::shared_ptr<LRhiCommandBuffer> cb) override;
    virtual double lastCompletedGpuTime(std::shared_ptr<LRhiCommandBuffer> cb) override;

    virtual std::list<int> supportedSampleCounts() const override;
    virtual int ubufAlignment() const override;
    virtual std::list<LSize<int>> supportedShadingRates(int sampleCount) const override;
    virtual bool isYUpInFramebuffer() const override;
    virtual bool isYUpInNDC() const override;
    virtual bool isClipDepthZeroToOne() const override;
    virtual LMatrix4x4 clipSpaceCorrMatrix() const override;
    virtual bool isTextureFormatSupported(LRhiTexture::Format format, LRhiTexture::Flags flags) const override;
    virtual bool isFeatureSupported(LRhi::Feature feature) const override;
    virtual int resourceLimit(LRhi::ResourceLimit limit) const override;
    virtual const std::shared_ptr<LRhiNativeHandles> nativeHandles() override;
    virtual LRhiDriverInfo driverInfo() const override;
    virtual LRhiStats statistics() override;
    virtual bool makeThreadLocalNativeContextCurrent() override;
    virtual void setQueueSubmitParams(std::shared_ptr<LRhiNativeHandles> params) override;
    virtual void releaseCachedResources() override;
    virtual bool isDeviceLost() const override;

    virtual std::vector<uint8_t> pipelineCacheData() override;
    virtual void setPipelineCacheData(const std::vector<uint8_t> &data) override;

    bool debugLayer = false;
    UINT maxFrameLatency = 2; // 1-3, use 2 to keep CPU-GPU parallelism while reducing lag compared to tripple buffering
    ID3D12Device2 *dev = nullptr;
    D3D_FEATURE_LEVEL minimumFeatureLevel = D3D_FEATURE_LEVEL(0);
    LUID adapterLuid = {};
    bool importedDevice = false;
    bool importedCommandQueue = false;
    LRhi::Flags rhiFlags;
    IDXGIFactory2 *dxgiFactory = nullptr;
    bool supportsAllowTearing = false;
    IDXGIAdapter1 *activeAdapter = nullptr;
    LRhiDriverInfo driverInfoStruct;
    LRhiD3D12NativeHandles nativeHandlesStruct;
    bool deviceLost = false;
    ID3D12CommandQueue *cmdQueue = nullptr;
    ID3D12Fence *fullFence = nullptr;
    HANDLE fullFenceEvent = nullptr;
    UINT64 fullFenceCounter = 0;
    ID3D12CommandAllocator *cmdAllocators[LD3D12_FRAMES_IN_FLIGHT] = {};
    LD3D12MemoryAllocator vma;
    LD3D12CpuDescriptorPool rtvPool;
    LD3D12CpuDescriptorPool dsvPool;
    LD3D12CpuDescriptorPool cbvSrvUavPool;
    LD3D12ObjectPool<LD3D12Resource> resourcePool;
    LD3D12ObjectPool<LD3D12Pipeline> pipelinePool;
    LD3D12ObjectPool<LD3D12RootSignature> rootSignaturePool;
    LD3D12ReleaseQueue releaseQueue;
    LD3D12ResourceBarrierGenerator barrierGen;
    LD3D12SamplerManager samplerMgr;
    LD3D12MipmapGenerator mipmapGen;
    LD3D12MipmapGenerator3D mipmapGen3D;
    LD3D12StagingArea smallStagingAreas[LD3D12_FRAMES_IN_FLIGHT];
    LD3D12ShaderVisibleDescriptorHeap shaderVisibleCbvSrvUavHeap;
    UINT64 timestampTicksPerSecond = 0;
    LD3D12QueryHeap timestampQueryHeap;
    LD3D12StagingArea timestampReadbackArea;
    IDCompositionDevice *dcompDevice = nullptr;

    std::shared_ptr<LD3D12SwapChain> currentSwapChain = nullptr;
    std::set<std::shared_ptr<LD3D12SwapChain> > swapchains;
    LD3D12ShaderBytecodeCache shaderBytecodeCache;
    std::vector<LD3D12Readback> activeReadbacks;
    bool offscreenActive = false;
    LD3D12CommandBuffer *offscreenCb[LD3D12_FRAMES_IN_FLIGHT] = {};
    UINT shadingRateImageTileSize = 0;

    struct {
        bool multiView = false;
        bool textureViewFormat = false;
        bool vrs = false;
        bool vrsMap = false;
        bool vrsAdditionalRates = false;
    } caps;
};

LK_NAMESPACE_END


#endif // LRHID3D12_H
