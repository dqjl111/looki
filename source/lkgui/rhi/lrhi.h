#ifndef LRHI_H
#define LRHI_H

#include "lkgui/lrect.h"
#include "lkcore/lobject.h"
#include "lkgui/paint/lcolor.h"
#include "lkgui/image/limage.h"
#include "lkgui/rhi/lshader.h"
#include "lkgui/lwindow.h"
#include "lkgui/math/lmatrix4x4.h"
#include <list>
#include <variant>

LK_NAMESPACE_BEGIN
class LRhiImplementation;
class LRhiD3D12;
struct LKGUIDEF LRhiDepthStencilClearValue
{
    double m_depthClearValue = 1.0;
    uint32_t m_stencilClearValue = 0;
};


struct LKGUIDEF LRhiViewport
{
    LRect<double> m_rect;
    double m_minDepth = 0.0;
    double m_maxDepth = 1.0;
};

typedef LRect<int> LRhiScissor;

struct LKGUIDEF LRhiVertexInputBinding
{
    enum Classification {
        PerVertex,
        PerInstance
    };

    uint32_t m_stride = 0;
    Classification m_classification = PerVertex;
    uint32_t m_instanceStepRate = 1;

    friend bool operator==(const LRhiVertexInputBinding &a, const LRhiVertexInputBinding &b) noexcept
    {
        return a.m_stride == b.m_stride && a.m_classification == b.m_classification && a.m_instanceStepRate == b.m_instanceStepRate;
    }
};


struct LKGUIDEF LRhiVertexInputAttribute
{
public:
    enum Format {
        Float4,
        Float3,
        Float2,
        Float,
        UNormByte4,
        UNormByte2,
        UNormByte,
        UInt4,
        UInt3,
        UInt2,
        UInt,
        SInt4,
        SInt3,
        SInt2,
        SInt,
        Half4,
        Half3,
        Half2,
        Half,
        UShort4,
        UShort3,
        UShort2,
        UShort,
        SShort4,
        SShort3,
        SShort2,
        SShort,
    };

    int m_binding = 0;
    int m_location = 0;
    Format m_format = Float4;
    uint32_t m_offset = 0;
    int m_matrixSlice = -1;
    /** HLSL 语义名（POSITION / TEXCOORD / NORMAL ...）；为空时按 TEXCOORD+location 兜底 */
    std::string m_name;

    friend bool operator==(const LRhiVertexInputAttribute &a, const LRhiVertexInputAttribute &b) noexcept
    {
        return a.m_binding == b.m_binding
            && a.m_location == b.m_location
            && a.m_format == b.m_format
            && a.m_offset == b.m_offset;
    }
};

class LKGUIDEF LRhiInitParams : LIVar
{
public:
    LRhiInitParams();
};


class LKGUIDEF LRhiNativeHandles: LIVar
{
public:
    LRhiNativeHandles();
};

struct LKGUIDEF LRhiDriverInfo
{
    enum DeviceType {
        UnknownDevice,
        IntegratedDevice,
        DiscreteDevice,
        ExternalDevice,
        VirtualDevice,
        CpuDevice
    };

    std::string m_deviceName;
    uint64_t m_deviceId = 0;
    uint64_t m_vendorId = 0;
    DeviceType m_deviceType = UnknownDevice;
};


class LRhi;
class LKGUIDEF LRhiResource : public LIVar
{
public:
    enum Type {
        Buffer,
        Texture,
        Sampler,
        RenderBuffer,
        RenderPassDescriptor,
        SwapChainRenderTarget,
        TextureRenderTarget,
        ShaderResourceBindings,
        GraphicsPipeline,
        SwapChain,
        ComputePipeline,
        CommandBuffer,
        ShadingRateMap
    };

    virtual ~LRhiResource();

    virtual Type resourceType() const = 0;
    /** 提前释放底层 GPU 资源；默认无操作，后端可在析构里统一清理 */
    virtual void destroy() {}


    inline uint64_t globalResourceId() const{
        return m_id;
    }
    std::shared_ptr<LRhi> rhi() const;
    std::string m_name;
protected:
    LRhiResource(std::shared_ptr<LRhiImplementation> rhi);
    std::weak_ptr<LRhiImplementation> m_rhi ;
    uint64_t m_id;

};


enum LRhiBufferUsage {
    Vertex = 1 << 0,
    Index = 1 << 1,
    Uniform = 1 << 2,
    Storage = 1 << 3
};
L_EN_FLAGS(LRhiBufferUsage)


class LKGUIDEF LRhiBuffer : public LRhiResource
{
public:
    enum Type {
        Immutable,
        Static,
        Dynamic
    };


    struct NativeBuffer {
        const void *objects[3];
        int slotCount;
    };

    using UsageFlags =  LFlags<LRhiBufferUsage>;

    LRhiResource::Type resourceType() const override;

    Type m_type;
    UsageFlags m_usage;
    uint32_t  m_size;

    virtual bool create() = 0;

    virtual NativeBuffer nativeBuffer();

    virtual char *beginFullDynamicBufferUpdateForCurrentFrame();
    virtual void endFullDynamicBufferUpdateForCurrentFrame();
    virtual void fullDynamicBufferUpdateForCurrentFrame(const void *data, uint32_t  size = 0);
protected:
    LRhiBuffer(std::shared_ptr<LRhiImplementation> rhi, Type type_, UsageFlags usage_, uint32_t  size_);
};

enum LRhiTextureFlag  : uint16_t{
    RenderTarget = 1 << 0,
    CubeMap = 1 << 2,
    MipMapped = 1 << 3,
    sRGB = 1 << 4,
    UsedAsTransferSource = 1 << 5,
    UsedWithGenerateMips = 1 << 6,
    UsedWithLoadStore = 1 << 7,
    UsedAsCompressedAtlas = 1 << 8,
    ExternalOES = 1 << 9,
    ThreeDimensional = 1 << 10,
    TextureRectangleGL = 1 << 11,
    TextureArray = 1 << 12,
    OneDimensional = 1 << 13,
    UsedAsShadingRateMap = 1 << 14
};
L_EN_FLAGS(LRhiTextureFlag)

class LKGUIDEF LRhiTexture : public LRhiResource
{
public:
    enum Format {
        UnknownFormat,

        RGBA8,
        BGRA8,
        R8,
        RG8,
        R16,
        RG16,
        RED_OR_ALPHA8,

        RGBA16F,
        RGBA32F,
        R16F,
        R32F,

        RGB10A2,

        R8SI,
        R32SI,
        RG32SI,
        RGBA32SI,

        R8UI,
        R32UI,
        RG32UI,
        RGBA32UI,

        D16,
        D24,
        D24S8,
        D32F,
        D32FS8,

        BC1,
        BC2,
        BC3,
        BC4,
        BC5,
        BC6H,
        BC7,

        ETC2_RGB8,
        ETC2_RGB8A1,
        ETC2_RGBA8,

        ASTC_4x4,
        ASTC_5x4,
        ASTC_5x5,
        ASTC_6x5,
        ASTC_6x6,
        ASTC_8x5,
        ASTC_8x6,
        ASTC_8x8,
        ASTC_10x5,
        ASTC_10x6,
        ASTC_10x8,
        ASTC_10x10,
        ASTC_12x10,
        ASTC_12x12
    };

    struct NativeTexture {
        uint64_t object;
        int layout; // or state
    };

    using Flags = LFlags<LRhiTextureFlag>;

    LRhiResource::Type resourceType() const override;
    struct ViewFormat {
        LRhiTexture::Format format;
        bool srgb;
    };


    Format m_format;
    LSize<int> m_pixelSize;
    int m_depth;
    int m_arraySize;
    int m_sampleCount;
    Flags m_flags;
    int m_arrayRangeStart = -1;
    int m_arrayRangeLength = -1;
    ViewFormat m_readViewFormat = { UnknownFormat, false };
    ViewFormat m_writeViewFormat = { UnknownFormat, false };

    virtual bool create() = 0;
    virtual NativeTexture nativeTexture();
    virtual bool createFrom(NativeTexture src);
    virtual void setNativeLayout(int layout);

protected:
    LRhiTexture(std::shared_ptr<LRhiImplementation> rhi, Format format_, const LSize<int> &pixelSize_, int depth_,
                int arraySize_, int sampleCount_, Flags flags_);
};

class LKGUIDEF LRhiSampler : public LRhiResource
{
public:
    enum Filter {
        None,
        Nearest,
        Linear
    };

    enum AddressMode {
        Repeat,
        ClampToEdge,
        Mirror,
    };

    enum CompareOp {
        Never,
        Less,
        Equal,
        LessOrEqual,
        Greater,
        NotEqual,
        GreaterOrEqual,
        Always
    };

    LRhiResource::Type resourceType() const override;
    virtual bool create() = 0;

    Filter m_magFilter;
    Filter m_minFilter;
    Filter m_mipmapMode;
    AddressMode m_addressU;
    AddressMode m_addressV;
    AddressMode m_addressW;
    CompareOp m_compareOp;
protected:
    LRhiSampler(std::shared_ptr<LRhiImplementation> rhi,
                Filter magFilter_, Filter minFilter_, Filter mipmapMode_,
                AddressMode u_, AddressMode v_, AddressMode w_);

};


class LKGUIDEF LRhiRenderBuffer : public LRhiResource
{
public:
    enum Type {
        DepthStencil,
        Color
    };

    struct NativeRenderBuffer {
        uint64_t object;
    };

    LRhiResource::Type resourceType() const override;
    virtual bool create() = 0;
    virtual bool createFrom(NativeRenderBuffer src);

    virtual LRhiTexture::Format backingFormat() const = 0;

    Type m_type;
    LSize<int> m_pixelSize;
    int m_sampleCount;
    bool m_usedWithSwapChainOnly;
    LRhiTexture::Format m_backingFormatHint;

protected:
    LRhiRenderBuffer(std::shared_ptr<LRhiImplementation> rhi, Type type_, const LSize<int> &pixelSize_,
                     int sampleCount_, bool usedWithSwapChainOnly, LRhiTexture::Format backingFormatHint_);

};

class LKGUIDEF LRhiShadingRateMap : public LRhiResource
{
public:
    struct NativeShadingRateMap {
        uint64_t object;
    };

    LRhiResource::Type resourceType() const override;

    virtual bool createFrom(NativeShadingRateMap src);
    virtual bool createFrom(std::shared_ptr<LRhiTexture> src);

protected:
    LRhiShadingRateMap(std::shared_ptr<LRhiImplementation> rhi);
};

class LKGUIDEF LRhiRenderPassDescriptor : public LRhiResource
{
public:
    LRhiResource::Type resourceType() const override;

    virtual bool isCompatible(const std::shared_ptr<LRhiRenderPassDescriptor> other) const = 0;
    virtual const std::shared_ptr<LRhiNativeHandles> nativeHandles();
    virtual std::shared_ptr<LRhiRenderPassDescriptor> newCompatibleRenderPassDescriptor() const = 0;
    virtual std::vector<uint32_t> serializedFormat() const = 0;

protected:
    LRhiRenderPassDescriptor(std::shared_ptr<LRhiImplementation> rhi);
};

class LKGUIDEF LRhiRenderTarget : public LRhiResource
{
public:
    virtual LSize<int> pixelSize() const = 0;
    virtual float devicePixelRatio() const = 0;
    virtual int sampleCount() const = 0;

    std::shared_ptr<LRhiRenderPassDescriptor> m_renderPassDesc = nullptr;
protected:
    LRhiRenderTarget(std::shared_ptr<LRhiImplementation> rhi);

};

enum class LRhiShaderResourceBindingStage {
    Vertex = 1 << 0,
    TessellationControl = 1 << 1,
    TessellationEvaluation = 1 << 2,
    Geometry = 1 << 3,
    Fragment = 1 << 4,
    Compute = 1 << 5
};

L_EN_FLAGS(LRhiShaderResourceBindingStage)

class LKGUIDEF LRhiShaderResourceBinding
{
public:
    enum Type {
        UniformBuffer,
        SampledTexture,
        Texture,
        Sampler,
        ImageLoad,
        ImageStore,
        ImageLoadStore,
        BufferLoad,
        BufferStore,
        BufferLoadStore
    };

    LRhiShaderResourceBinding() = default;

    bool isLayoutCompatible(const LRhiShaderResourceBinding &other) const;

    using  StageFlags = LFlags<LRhiShaderResourceBindingStage>;

    static LRhiShaderResourceBinding uniformBuffer(int binding, StageFlags stage, std::shared_ptr<LRhiBuffer> buf);
    static LRhiShaderResourceBinding uniformBuffer(int binding, StageFlags stage, std::shared_ptr<LRhiBuffer> buf, uint32_t offset, uint32_t size);
    static LRhiShaderResourceBinding uniformBufferWithDynamicOffset(int binding, StageFlags stage, std::shared_ptr<LRhiBuffer> buf, uint32_t size);

    static LRhiShaderResourceBinding sampledTexture(int binding, StageFlags stage, std::shared_ptr<LRhiTexture> tex, std::shared_ptr<LRhiSampler> sampler);

    struct TextureAndSampler {
        std::shared_ptr<LRhiTexture> tex;
        std::shared_ptr<LRhiSampler> sampler;
    };
    static LRhiShaderResourceBinding sampledTextures(int binding, StageFlags stage, int count, const TextureAndSampler *texSamplers);

    static LRhiShaderResourceBinding texture(int binding, StageFlags stage, std::shared_ptr<LRhiTexture> tex);
    static LRhiShaderResourceBinding textures(int binding, StageFlags stage, int count, std::shared_ptr<LRhiTexture>* tex);
    static LRhiShaderResourceBinding sampler(int binding, StageFlags stage, std::shared_ptr<LRhiSampler> sampler);

    static LRhiShaderResourceBinding imageLoad(int binding, StageFlags stage, std::shared_ptr<LRhiTexture> tex, int level);
    static LRhiShaderResourceBinding imageStore(int binding, StageFlags stage, std::shared_ptr<LRhiTexture> tex, int level);
    static LRhiShaderResourceBinding imageLoadStore(int binding, StageFlags stage, std::shared_ptr<LRhiTexture> tex, int level);

    static LRhiShaderResourceBinding bufferLoad(int binding, StageFlags stage, std::shared_ptr<LRhiBuffer> buf);
    static LRhiShaderResourceBinding bufferLoad(int binding, StageFlags stage, std::shared_ptr<LRhiBuffer> buf, uint32_t offset, uint32_t size);
    static LRhiShaderResourceBinding bufferStore(int binding, StageFlags stage, std::shared_ptr<LRhiBuffer> buf);
    static LRhiShaderResourceBinding bufferStore(int binding, StageFlags stage, std::shared_ptr<LRhiBuffer> buf, uint32_t offset, uint32_t size);
    static LRhiShaderResourceBinding bufferLoadStore(int binding, StageFlags stage, std::shared_ptr<LRhiBuffer> buf);
    static LRhiShaderResourceBinding bufferLoadStore(int binding, StageFlags stage, std::shared_ptr<LRhiBuffer> buf, uint32_t offset, uint32_t size);

    struct UniformBufferData{
        std::shared_ptr<LRhiBuffer> buf;
        uint32_t offset;
        uint32_t maybeSize;
        bool hasDynamicOffset;
    };
    static constexpr int MAX_TEX_SAMPLER_ARRAY_SIZE = 16;
    struct TextureAndOrSamplerData{
        int count;
        TextureAndSampler texSamplers[MAX_TEX_SAMPLER_ARRAY_SIZE];
    };

    struct StorageImageData{
        std::shared_ptr<LRhiTexture> tex;
        int level;
    };
    struct StorageBufferData{
        std::shared_ptr<LRhiBuffer> buf;
        uint32_t offset;
        uint32_t maybeSize;
    };

    int arraySize() const
    {
        if(type == LRhiShaderResourceBinding::SampledTexture || type == LRhiShaderResourceBinding::Texture){
            return std::get<TextureAndOrSamplerData>(data).count;
        }
        return 1;
    }

    static constexpr int LAYOUT_DESC_ENTRIES_PER_BINDING = 4;
private:
    int binding;
    LRhiShaderResourceBinding::StageFlags stage;
    LRhiShaderResourceBinding::Type type;
    std::variant<UniformBufferData,TextureAndOrSamplerData,StorageImageData,StorageBufferData> data;
    friend class LRhiImplementation;
    // 后端子类要遍历自己的绑定，只能通过下面 bindingInfos() 的快照
    friend class LRhiShaderResourceBindings;
};

class LKGUIDEF LRhiShaderResourceBindings : public LRhiResource
{
public:
    LRhiResource::Type resourceType() const override;
    bool isLayoutCompatible(const std::shared_ptr<LRhiShaderResourceBindings> other) const;

    const std::vector<uint32_t>& serializedLayoutDescription() const { return m_layoutDesc; }

    virtual bool create() = 0;
    virtual void updateResources(bool bindingsAreSorted = false) = 0;

    /** 替换某个采样纹理绑定引用的纹理，布局保持不变时可避免重建 SRB */
    void setSampledTexture(int binding, std::shared_ptr<LRhiTexture> tex,
                           std::shared_ptr<LRhiSampler> sampler);

    std::vector<LRhiShaderResourceBinding> m_bindings;
    /** 绑定的只读快照：后端无法直接访问 LRhiShaderResourceBinding 的私有数据 */
    struct BindingInfo {
        int binding = 0;
        int type = 0;               // LRhiShaderResourceBinding::Type
        int stage = 0;
        int arraySize = 1;
        uint32_t offset = 0;
        int frameLevel = 0;         // 存储映像用的 mip 级别
        std::shared_ptr<LRhiBuffer> buffer;
        std::shared_ptr<LRhiSampler> sampler;
        std::vector<LRhiShaderResourceBinding::TextureAndSampler> textureSamplers;
    };

protected:
    static constexpr int BINDING_PREALLOC = 12;
    LRhiShaderResourceBindings(std::shared_ptr<LRhiImplementation> rhi);

    /** 供后端子类在 create()/updateResources() 里读取绑定内容 */
    std::vector<BindingInfo> bindingInfos() const;

    size_t m_layoutDescHash = 0;
    std::vector<uint32_t> m_layoutDesc;
    friend class LRhiImplementation;
};

class LKGUIDEF LRhiShaderStage
{
public:
    enum Type {
        Vertex,
        TessellationControl,
        TessellationEvaluation,
        Geometry,
        Fragment,
        Compute
    };

    LRhiShaderStage() = default;
    LRhiShaderStage(Type type, const LShader &shader,
                    LShader::Variant v = LShader::StandardShader);


    Type m_type = Vertex;
    LShader m_shader;
    LShader::Variant m_shaderVariant = LShader::StandardShader;

    friend bool operator==(const LRhiShaderStage &a, const LRhiShaderStage &b) noexcept
    {
        return a.m_type == b.m_type
               && a.m_shader == b.m_shader
               && a.m_shaderVariant == b.m_shaderVariant;
    }

    friend bool operator!=(const LRhiShaderStage &a, const LRhiShaderStage &b) noexcept
    {
        return !(a == b);
    }
};

class LKGUIDEF LRhiVertexInputLayout
{
public:
    LRhiVertexInputLayout();

    std::vector<LRhiVertexInputBinding> m_bindings;
    std::vector<LRhiVertexInputAttribute> m_attributes;

    friend bool operator==(const LRhiVertexInputLayout &a, const LRhiVertexInputLayout &b) noexcept
    {
        return a.m_bindings == b.m_bindings && a.m_attributes == b.m_attributes;
    }

    friend bool operator!=(const LRhiVertexInputLayout &a, const LRhiVertexInputLayout &b) noexcept
    {
        return !(a == b);
    }

};

enum class LRhiGraphicsPipelineFlag : uint8_t{
    UsesBlendConstants = 1 << 0,
    UsesStencilRef = 1 << 1,
    UsesScissor = 1 << 2,
    CompileShadersWithDebugInfo = 1 << 3,
    UsesShadingRate = 1 << 4
};
L_EN_FLAGS(LRhiGraphicsPipelineFlag)

enum class LRhiColorMask  : uint8_t {
    R = 1 << 0,
    G = 1 << 1,
    B = 1 << 2,
    A = 1 << 3
};

L_EN_FLAGS(LRhiColorMask)

class LKGUIDEF LRhiGraphicsPipeline : public LRhiResource
{
public:
    enum Topology {
        Triangles,
        TriangleStrip,
        TriangleFan,
        Lines,
        LineStrip,
        Points,
        Patches
    };

    enum CullMode {
        None,
        Front,
        Back
    };

    enum FrontFace {
        CCW,
        CW
    };

    enum BlendFactor {
        Zero,
        One,
        SrcColor,
        OneMinusSrcColor,
        DstColor,
        OneMinusDstColor,
        SrcAlpha,
        OneMinusSrcAlpha,
        DstAlpha,
        OneMinusDstAlpha,
        ConstantColor,
        OneMinusConstantColor,
        ConstantAlpha,
        OneMinusConstantAlpha,
        SrcAlphaSaturate,
        Src1Color,
        OneMinusSrc1Color,
        Src1Alpha,
        OneMinusSrc1Alpha
    };

    enum BlendOp {
        Add,
        Subtract,
        ReverseSubtract,
        Min,
        Max
    };

    struct TargetBlend {
        LFlags<LRhiColorMask> colorWrite = 0xFU; // R | G | B | A
        bool enable = false;
        BlendFactor srcColor = One;
        BlendFactor dstColor = OneMinusSrcAlpha;
        BlendOp opColor = Add;
        BlendFactor srcAlpha = One;
        BlendFactor dstAlpha = OneMinusSrcAlpha;
        BlendOp opAlpha = Add;
    };

    enum CompareOp {
        Never,
        Less,
        Equal,
        LessOrEqual,
        Greater,
        NotEqual,
        GreaterOrEqual,
        Always
    };

    enum StencilOp {
        StencilZero,
        Keep,
        Replace,
        IncrementAndClamp,
        DecrementAndClamp,
        Invert,
        IncrementAndWrap,
        DecrementAndWrap
    };

    struct StencilOpState {
        StencilOp failOp = Keep;
        StencilOp depthFailOp = Keep;
        StencilOp passOp = Keep;
        CompareOp compareOp = Always;
    };

    enum PolygonMode {
        Fill,
        Line
    };

    LRhiResource::Type resourceType() const override;

    LFlags<LRhiGraphicsPipelineFlag> m_flags;
    Topology m_topology = Triangles;
    CullMode m_cullMode = None;
    FrontFace m_frontFace = CCW;
    std::vector<TargetBlend> m_targetBlends;
    bool m_depthTest = false;
    bool m_depthWrite = false;
    bool m_depthClamp = false;
    CompareOp m_depthOp = Less;
    bool m_stencilTest = false;
    StencilOpState m_stencilFront;
    StencilOpState m_stencilBack;
    uint32_t m_stencilReadMask = 0xFF;
    uint32_t m_stencilWriteMask = 0xFF;
    int m_sampleCount = 1;
    float m_lineWidth = 1.0f;
    int m_depthBias = 0;
    float m_slopeScaledDepthBias = 0.0f;
    int m_patchControlPointCount = 3;
    PolygonMode m_polygonMode = Fill;
    int m_multiViewCount = 0;
    std::vector<LRhiShaderStage> m_shaderStages;
    LRhiVertexInputLayout m_vertexInputLayout;
    std::shared_ptr<LRhiShaderResourceBindings> m_shaderResourceBindings = nullptr;
    std::shared_ptr<LRhiRenderPassDescriptor> m_renderPassDesc = nullptr;
    virtual bool create() = 0;
protected:
    LRhiGraphicsPipeline(std::shared_ptr<LRhiImplementation> rhi);
};

class LKGUIDEF LRhiComputePipeline : public LRhiResource
{
public:
    LRhiResource::Type resourceType() const override;
    virtual bool create() = 0;

    bool m_compileShadersWithDebugInfo = false;
    LRhiShaderStage m_shaderStage;
    std::shared_ptr<LRhiShaderResourceBindings> m_shaderResourceBindings = nullptr;
protected:
    LRhiComputePipeline(std::shared_ptr<LRhiImplementation> rhi);
};


struct LKGUIDEF LRhiReadbackResult
{
    std::function<void()> m_completed = nullptr;
    LRhiTexture::Format m_format;
    LSize<int> m_pixelSize;
    std::vector<uint8_t> m_data;
};

class LKGUIDEF LRhiTextureSubresourceUploadDescription
{
public:
    LRhiTextureSubresourceUploadDescription() = default;
    explicit LRhiTextureSubresourceUploadDescription(const std::shared_ptr<LImage> image);
    explicit LRhiTextureSubresourceUploadDescription(const std::vector<uint8_t> &data);

    std::shared_ptr<LImage> m_image;
    std::vector<uint8_t> m_data;
    uint32_t m_dataStride = 0;
    LPoint<int> m_destinationTopLeft;
    LSize<int> m_sourceSize;
    LPoint<int> m_sourceTopLeft;
};

class LKGUIDEF LRhiTextureUploadEntry
{
public:
    LRhiTextureUploadEntry() = default;
    LRhiTextureUploadEntry(int layer, int level, const LRhiTextureSubresourceUploadDescription &desc);
    int m_layer = 0;
    int m_level = 0;
    LRhiTextureSubresourceUploadDescription m_desc;
};


class LKGUIDEF LRhiTextureUploadDescription
{
public:
    LRhiTextureUploadDescription() = default;
    LRhiTextureUploadDescription(const LRhiTextureUploadEntry &entry);
    LRhiTextureUploadDescription(std::initializer_list<LRhiTextureUploadEntry> list);
    std::vector<LRhiTextureUploadEntry> m_entries;
};

struct LKGUIDEF LRhiTextureCopyDescription
{
    LSize<int> m_pixelSize;
    int m_sourceLayer = 0;
    int m_sourceLevel = 0;
    LPoint<int> m_sourceTopLeft;
    int m_destinationLayer = 0;
    int m_destinationLevel = 0;
    LPoint<int> m_destinationTopLeft;
};

struct LKGUIDEF LRhiReadbackDescription
{
    std::shared_ptr<LRhiTexture> m_texture = nullptr;
    int m_layer = 0;
    int m_level = 0;
    LRect<int> m_rect;
};


enum class LRhiCommandBufferBeginPass {
    ExternalContent = 0x01,
    DoNotTrackResourcesForCompute = 0x02
};
L_EN_FLAGS(LRhiCommandBufferBeginPass)

class LRhiResourceUpdateBatch;
class LKGUIDEF LRhiCommandBuffer : public LRhiResource
{
public:
    enum IndexFormat {
        IndexUInt16,
        IndexUInt32
    };

    using  BeginPassFlags = LFlags<LRhiCommandBufferBeginPass>;

    LRhiResource::Type resourceType() const override;

    void resourceUpdate(std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates);

    void beginPass(std::shared_ptr<LRhiRenderTarget> rt,
                   const LColor &colorClearValue,
                   const LRhiDepthStencilClearValue &depthStencilClearValue,
                   std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates = nullptr,
                   BeginPassFlags flags = {});
    void endPass(std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates = nullptr);

    void setGraphicsPipeline(std::shared_ptr<LRhiGraphicsPipeline> ps);
    using DynamicOffset = std::pair<int, uint32_t>; // binding, offset
    void setShaderResources(std::shared_ptr<LRhiShaderResourceBindings> srb = nullptr,
                            int dynamicOffsetCount = 0,
                            const DynamicOffset *dynamicOffsets = nullptr);
    using VertexInput = std::pair<std::shared_ptr<LRhiBuffer>, uint32_t>; // buffer, offset
    void setVertexInput(int startBinding, int bindingCount, const VertexInput *bindings,
                        std::shared_ptr<LRhiBuffer>indexBuf = nullptr, uint32_t indexOffset = 0,
                        IndexFormat indexFormat = IndexUInt16);

    void setViewport(const LRhiViewport &viewport);
    void setScissor(const LRhiScissor &scissor);
    void setBlendConstants(const LColor &c);
    void setStencilRef(uint32_t refValue);
    void setShadingRate(const LSize<int> &coarsePixelSize);

    void draw(uint32_t vertexCount,
              uint32_t instanceCount = 1,
              uint32_t firstVertex = 0,
              uint32_t firstInstance = 0);

    void drawIndexed(uint32_t indexCount,
                     uint32_t instanceCount = 1,
                     uint32_t firstIndex = 0,
                     int32_t vertexOffset = 0,
                     uint32_t firstInstance = 0);

    void debugMarkBegin(const std::string &name);
    void debugMarkEnd();
    void debugMarkMsg(const std::string &msg);

    void beginComputePass(std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates = nullptr, BeginPassFlags flags = {});
    void endComputePass(std::shared_ptr<LRhiResourceUpdateBatch> resourceUpdates = nullptr);
    void setComputePipeline(std::shared_ptr<LRhiComputePipeline> ps);
    void dispatch(int x, int y, int z);

    const std::shared_ptr<LRhiNativeHandles> nativeHandles();
    void beginExternal();
    void endExternal();

    double lastCompletedGpuTime();

protected:
    LRhiCommandBuffer(std::shared_ptr<LRhiImplementation> rhi);
};


struct LKGUIDEF LRhiSwapChainHdrInfo
{
    enum LimitsType {
        LuminanceInNits,
        ColorComponentValue
    };

    enum LuminanceBehavior {
        SceneReferred,
        DisplayReferred
    };

    LimitsType limitsType;
    union {
        struct {
            float minLuminance;
            float maxLuminance;
        } luminanceInNits;
        struct {
            float maxColorComponentValue;
            float maxPotentialColorComponentValue;
        } colorComponentValue;
    } limits;
    LuminanceBehavior luminanceBehavior;
    float sdrWhiteLevel;
};

struct LKGUIDEF LRhiSwapChainProxyData
{
    void *reserved[2] = {};
};

enum class LRhiSwapChainFlag {
    SurfaceHasPreMulAlpha = 1 << 0,
    SurfaceHasNonPreMulAlpha = 1 << 1,
    sRGB = 1 << 2,
    UsedAsTransferSource = 1 << 3,
    NoVSync = 1 << 4,
    MinimalBufferCount = 1 << 5
};

L_EN_FLAGS(LRhiSwapChainFlag)

class LKGUIDEF LRhiSwapChain : public LRhiResource
{
public:
    using Flags = LFlags<LRhiSwapChainFlag>;

    enum Format {
        SDR,
        HDRExtendedSrgbLinear,
        HDR10,
        HDRExtendedDisplayP3Linear
    };

    enum StereoTargetBuffer {
        LeftBuffer,
        RightBuffer
    };

    LRhiResource::Type resourceType() const override;
    LSize<int> currentPixelSize() const { return m_currentPixelSize; }

    virtual std::shared_ptr<LRhiCommandBuffer> currentFrameCommandBuffer() = 0;
    virtual std::shared_ptr<LRhiRenderTarget> currentFrameRenderTarget() = 0;
    virtual std::shared_ptr<LRhiRenderTarget> currentFrameRenderTarget(StereoTargetBuffer targetBuffer);
    virtual LSize<int> surfacePixelSize() = 0;
    virtual bool isFormatSupported(Format f) = 0;
    virtual std::shared_ptr<LRhiRenderPassDescriptor> newCompatibleRenderPassDescriptor() = 0;
    virtual bool createOrResize() = 0;
    virtual LRhiSwapChainHdrInfo hdrInfo();

    std::shared_ptr<LWindow> m_window = nullptr;
    /** 可选：直接使用原生窗口句柄（HWND/NSView 等），用于没有 LWindow 包装的子控件 */
    void *nativeWindowHandle = nullptr;
    Flags m_flags;
    Format m_format = SDR;
    std::shared_ptr<LRhiRenderBuffer> m_depthStencil = nullptr;
    int m_sampleCount = 1;
    std::shared_ptr<LRhiRenderPassDescriptor> m_renderPassDesc = nullptr;
    LRhiSwapChainProxyData m_proxyData;
    std::shared_ptr<LRhiShadingRateMap> m_shadingRateMap = nullptr;
protected:
    LRhiSwapChain(std::shared_ptr<LRhiImplementation> rhi);
    LSize<int> m_currentPixelSize;

};


class LKGUIDEF LRhiSwapChainRenderTarget : public LRhiRenderTarget
{
public:
    LRhiResource::Type resourceType() const override;
    std::shared_ptr<LRhiSwapChain> swapChain() const { return m_swapchain; }

protected:
    LRhiSwapChainRenderTarget(std::shared_ptr<LRhiImplementation> rhi, std::shared_ptr<LRhiSwapChain> swapchain_);
    std::shared_ptr<LRhiSwapChain> m_swapchain;
};


class LKGUIDEF LRhiColorAttachment
{
public:
    LRhiColorAttachment() = default;
    LRhiColorAttachment(std::shared_ptr<LRhiTexture> texture);
    LRhiColorAttachment(std::shared_ptr<LRhiRenderBuffer> renderBuffer);

    std::shared_ptr<LRhiTexture> m_texture = nullptr;
    std::shared_ptr<LRhiRenderBuffer> m_renderBuffer = nullptr;
    int m_layer = 0;
    int m_level = 0;
    std::shared_ptr<LRhiTexture> m_resolveTexture = nullptr;
    int m_resolveLayer = 0;
    int m_resolveLevel = 0;
    int m_multiViewCount = 0;
};


class LKGUIDEF LRhiTextureRenderTargetDescription
{
public:
    LRhiTextureRenderTargetDescription() = default;;
    LRhiTextureRenderTargetDescription(const LRhiColorAttachment &colorAttachment);
    LRhiTextureRenderTargetDescription(const LRhiColorAttachment &colorAttachment, std::shared_ptr<LRhiRenderBuffer> depthStencilBuffer);
    LRhiTextureRenderTargetDescription(const LRhiColorAttachment &colorAttachment, std::shared_ptr<LRhiTexture> depthTexture);

    std::vector<LRhiColorAttachment> m_colorAttachments;
    std::shared_ptr<LRhiRenderBuffer> m_depthStencilBuffer = nullptr;
    std::shared_ptr<LRhiTexture> m_depthTexture = nullptr;
    std::shared_ptr<LRhiTexture> m_depthResolveTexture = nullptr;
    std::shared_ptr<LRhiShadingRateMap> m_shadingRateMap = nullptr;
};

enum class LRhiTextureRenderTargetFlag {
    PreserveColorContents = 1 << 0,
    PreserveDepthStencilContents = 1 << 1,
    DoNotStoreDepthStencilContents = 1 << 2
};

L_EN_FLAGS(LRhiTextureRenderTargetFlag)


class LKGUIDEF LRhiTextureRenderTarget : public LRhiRenderTarget
{
public:
    using Flags = LFlags<LRhiTextureRenderTargetFlag>;

    LRhiResource::Type resourceType() const override;
    virtual std::shared_ptr<LRhiRenderPassDescriptor> newCompatibleRenderPassDescriptor() = 0;
    virtual bool create() = 0;

    LRhiTextureRenderTargetDescription m_desc;
    Flags m_flags;
protected:
    LRhiTextureRenderTarget(std::shared_ptr<LRhiImplementation> rhi, const LRhiTextureRenderTargetDescription &desc_, Flags flags_);
};


struct LKGUIDEF LRhiStats
{
    int64_t totalPipelineCreationTime = 0;
    // Vulkan or D3D12 memory allocator statistics
    uint32_t blockCount = 0;
    uint32_t allocCount = 0;
    uint64_t usedBytes = 0;
    uint64_t unusedBytes = 0;
    // D3D12 only, from IDXGIAdapter3::QueryVideoMemoryInfo(), incl. all resources
    uint64_t totalUsageBytes = 0;
};

class LKGUIDEF LRhiAdapter
{
public:
    virtual ~LRhiAdapter();
    virtual LRhiDriverInfo info() const = 0;
};

enum LRhiFlag : uint8_t{
    EnableDebugMarkers = 1 << 0,
    PreferSoftwareRenderer = 1 << 1,
    EnablePipelineCacheDataSave = 1 << 2,
    EnableTimestamps = 1 << 3,
    SuppressSmokeTestWarnings = 1 << 4
};
L_EN_FLAGS(LRhiFlag)

enum LRhiBeginFrameFlag : uint8_t{
};
L_EN_FLAGS(LRhiBeginFrameFlag)

enum LRhiEndFrameFlag : uint8_t{
    SkipPresent = 1 << 0
};
L_EN_FLAGS(LRhiEndFrameFlag)


/**
 * Rendering Hardware Interface
 **/
class LKGUIDEF LRhi : public LObject
{
public:
    static constexpr int MAX_MIP_LEVELS = 16; // -> max width or height is 65536
    enum Implementation {
        Null,
        Vulkan,
        Metal,
        D3D,        // D3D12
        D3D11
    };

    enum FrameOpResult {
        FrameOpSuccess = 0,
        FrameOpError,
        FrameOpSwapChainOutOfDate,
        FrameOpDeviceLost
    };

    enum Feature {
        MultisampleTexture = 1,
        MultisampleRenderBuffer,
        DebugMarkers,
        Timestamps,
        Instancing,
        CustomInstanceStepRate,
        PrimitiveRestart,
        NonDynamicUniformBuffers,
        NonFourAlignedEffectiveIndexBufferOffset,
        NPOTTextureRepeat,
        RedOrAlpha8IsRed,
        ElementIndexUint,
        Compute,
        WideLines,
        VertexShaderPointSize,
        BaseVertex,
        BaseInstance,
        TriangleFanTopology,
        ReadBackNonUniformBuffer,
        ReadBackNonBaseMipLevel,
        TexelFetch,
        RenderToNonBaseMipLevel,
        IntAttributes,
        ScreenSpaceDerivatives,
        ReadBackAnyTextureFormat,
        PipelineCacheDataLoadSave,
        ImageDataStride,
        RenderBufferImport,
        ThreeDimensionalTextures,
        RenderTo3DTextureSlice,
        TextureArrays,
        Tessellation,
        GeometryShader,
        TextureArrayRange,
        NonFillPolygonMode,
        OneDimensionalTextures,
        OneDimensionalTextureMipmaps,
        HalfAttributes,
        RenderToOneDimensionalTexture,
        ThreeDimensionalTextureMipmaps,
        MultiView,
        TextureViewFormat,
        ResolveDepthStencil,
        VariableRateShading,
        VariableRateShadingMap,
        VariableRateShadingMapWithTexture,
        PerRenderTargetBlending,
        SampleVariables,
        InstanceIndexIncludesBaseInstance,
        DepthClamp
    };

    using Flags = LFlags<LRhiFlag>;
    using BeginFrameFlags = LFlags<LRhiBeginFrameFlag>;
    using EndFrameFlags = LFlags<LRhiEndFrameFlag>;

    enum ResourceLimit {
        TextureSizeMin = 1,
        TextureSizeMax,
        MaxColorAttachments,
        FramesInFlight,
        MaxAsyncReadbackFrames,
        MaxThreadGroupsPerDimension,
        MaxThreadsPerThreadGroup,
        MaxThreadGroupX,
        MaxThreadGroupY,
        MaxThreadGroupZ,
        TextureArraySizeMax,
        MaxUniformBufferRange,
        MaxVertexInputs,
        MaxVertexOutputs,
        ShadingRateImageTileSize
    };

    static std::shared_ptr<LRhi> create(Implementation impl,
                                        std::shared_ptr<LRhiInitParams> params = nullptr,
                                        Flags flags = 0,
                                        std::shared_ptr<LRhiNativeHandles> importDevice  = nullptr,
                                        std::shared_ptr<LRhiAdapter> adapter = nullptr);

    Implementation backend() const;
    const std::string backendName() const;
    static std::string backendName(Implementation impl);
    LRhiDriverInfo driverInfo() const;

    // ---- 资源工厂：全部转发到当前后端实现 ----
    std::shared_ptr<LRhiResourceUpdateBatch> newResourceUpdateBatch();
    std::shared_ptr<LRhiGraphicsPipeline> newGraphicsPipeline();
    std::shared_ptr<LRhiComputePipeline> newComputePipeline();
    std::shared_ptr<LRhiShaderResourceBindings> newShaderResourceBindings();
    std::shared_ptr<LRhiBuffer> newBuffer(LRhiBuffer::Type type,
                                          LRhiBuffer::UsageFlags usage,
                                          uint32_t size);
    std::shared_ptr<LRhiRenderBuffer> newRenderBuffer(LRhiRenderBuffer::Type type,
                                                      const LSize<int> &pixelSize,
                                                      int sampleCount,
                                                      bool usedWithSwapChainOnly = false,
                                                      LRhiTexture::Format backingFormatHint = LRhiTexture::UnknownFormat);
    std::shared_ptr<LRhiTexture> newTexture(LRhiTexture::Format format,
                                           const LSize<int> &pixelSize,
                                           int depth = 1,
                                           int arraySize = 1,
                                           int sampleCount = 1,
                                           LRhiTexture::Flags flags = {});
    std::shared_ptr<LRhiSampler> newSampler(LRhiSampler::Filter magFilter,
                                            LRhiSampler::Filter minFilter,
                                            LRhiSampler::Filter mipmapMode,
                                            LRhiSampler::AddressMode u,
                                            LRhiSampler::AddressMode v,
                                            LRhiSampler::AddressMode w);
    std::shared_ptr<LRhiTextureRenderTarget> newTextureRenderTarget(
            const LRhiTextureRenderTargetDescription &desc,
            LRhiTextureRenderTarget::Flags flags = {});
    std::shared_ptr<LRhiShadingRateMap> newShadingRateMap();
    std::shared_ptr<LRhiSwapChain> newSwapChain();

    // ---- 帧循环 ----
    FrameOpResult beginFrame(std::shared_ptr<LRhiSwapChain> swapChain,
                             BeginFrameFlags flags = {});
    FrameOpResult endFrame(std::shared_ptr<LRhiSwapChain> swapChain,
                           EndFrameFlags flags = {});
    FrameOpResult beginOffscreenFrame(std::shared_ptr<LRhiCommandBuffer> *cb,
                                      BeginFrameFlags flags = {});
    FrameOpResult endOffscreenFrame(EndFrameFlags flags = {});
    FrameOpResult finish();

    // ---- 能力查询 ----
    std::list<int> supportedSampleCounts() const;
    int ubufAlignment() const;
    bool isYUpInFramebuffer() const;
    bool isYUpInNDC() const;
    bool isClipDepthZeroToOne() const;
    LMatrix4x4 clipSpaceCorrMatrix() const;
    bool isTextureFormatSupported(LRhiTexture::Format format,
                                  LRhiTexture::Flags flags = {}) const;
    bool isFeatureSupported(Feature feature) const;
    int resourceLimit(ResourceLimit limit) const;
    LRhiStats statistics();
    const std::shared_ptr<LRhiNativeHandles> nativeHandles();
    const std::shared_ptr<LRhiNativeHandles> nativeHandles(std::shared_ptr<LRhiCommandBuffer> cb);
    void releaseCachedResources();
    bool isDeviceLost() const;

    /** 探测某个后端能否在当前环境创建（创建成功即销毁），失败返回 Null */
    static Implementation probe(Implementation impl,
                                std::shared_ptr<LRhiInitParams> params = nullptr,
                                Flags flags = 0);

protected:
    LRhi();
    std::shared_ptr<LRhiImplementation> m_impl;
};


class LKGUIDEF LRhiResourceUpdateBatch
{
public:
    ~LRhiResourceUpdateBatch();

    void release();

    void merge(std::shared_ptr<LRhiResourceUpdateBatch> other);
    bool hasOptimalCapacity() const;

    void updateDynamicBuffer(std::shared_ptr<LRhiBuffer> buf, uint32_t offset, uint32_t size, const void *data);
    void updateDynamicBuffer(std::shared_ptr<LRhiBuffer> buf, uint32_t offset, const std::vector<uint8_t>& data);
    void uploadStaticBuffer(std::shared_ptr<LRhiBuffer> buf, uint32_t offset, uint32_t size, const void *data);
    void uploadStaticBuffer(std::shared_ptr<LRhiBuffer> buf, uint32_t offset, const std::vector<uint8_t>& data);
    void uploadStaticBuffer(std::shared_ptr<LRhiBuffer> buf, const void *data);
    void uploadStaticBuffer(std::shared_ptr<LRhiBuffer> buf, const std::vector<uint8_t>& data);
    void readBackBuffer(std::shared_ptr<LRhiBuffer> buf, uint32_t offset, uint32_t size, std::shared_ptr<LRhiReadbackResult> result);
    void uploadTexture(std::shared_ptr<LRhiTexture> tex, const LRhiTextureUploadDescription &desc);
    void uploadTexture(std::shared_ptr<LRhiTexture> tex, const std::shared_ptr<LImage> image);
    void copyTexture(std::shared_ptr<LRhiTexture> dst, std::shared_ptr<LRhiTexture> src, const LRhiTextureCopyDescription &desc = LRhiTextureCopyDescription());
    void readBackTexture(const LRhiReadbackDescription &rb, std::shared_ptr<LRhiReadbackResult> result);
    void generateMips(std::shared_ptr<LRhiTexture> tex);


    struct BufferOp {
        enum Type {
            DynamicUpdate,
            StaticUpload,
            Read
        };
        Type type;
        std::shared_ptr<LRhiBuffer> buf;
        uint32_t offset;
        std::vector<uint8_t> data;
        uint32_t readSize;
        std::shared_ptr<LRhiReadbackResult> result;


        BufferOp(){
            data.reserve(1024);
        }
    };

    struct TextureOp {
        enum Type {
            Upload,
            Copy,
            Read,
            GenMips
        };
        Type type;
        std::shared_ptr<LRhiTexture> dst;
        // Specifying multiple uploads for a subresource must be supported.
        // In the backend this can then end up, where applicable, as a
        // single, batched copy operation with only one set of barriers.
        // This helps when doing for example glyph cache fills.
        using MipLevelUploadList = std::array<std::vector<LRhiTextureSubresourceUploadDescription>, LRhi::MAX_MIP_LEVELS>;
        std::vector<MipLevelUploadList> subresDesc;
        std::shared_ptr<LRhiTexture> src;
        LRhiTextureCopyDescription desc;
        LRhiReadbackDescription rb;
        std::shared_ptr<LRhiReadbackResult> result;

        TextureOp(){
            subresDesc.reserve(6);
        }
    };

private:
    LRhiResourceUpdateBatch(std::shared_ptr<LRhiImplementation> rhi_);

    int activeBufferOpCount = 0; // this is the real number of used elements in bufferOps, not bufferOps.count()
    static const int BUFFER_OPS_STATIC_ALLOC = 64;
    std::vector<std::shared_ptr<BufferOp>> bufferOps;

    int activeTextureOpCount = 0; // this is the real number of used elements in textureOps, not textureOps.count()
    static const int TEXTURE_OPS_STATIC_ALLOC = 32;
    std::vector<std::shared_ptr<TextureOp>>textureOps;

    std::shared_ptr<LRhiImplementation>  rhi = nullptr;
    int poolIndex = -1;

    friend class LRhi;
    // 后端实现需要遍历 bufferOps / textureOps 才能完成上传与拷贝
    friend class LRhiImplementation;
    friend class LRhiD3D12;
    friend class LRhiD3d11;
};

LK_NAMESPACE_END

template<>
struct std::hash<_LK::LRhiShaderStage>{
    std::size_t operator()(const _LK::LRhiShaderStage& key) const noexcept
    {
        uint32_t seed=0;
        seed = _LK::lHashCombine(seed, key.m_type);
        seed = _LK::lHashCombine(seed, key.m_shader);
        seed = _LK::lHashCombine(seed, key.m_shaderVariant);
        return seed;
    }
};
#endif // LRHI_H
