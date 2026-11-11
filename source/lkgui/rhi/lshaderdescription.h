#ifndef LSHADERDESCRIPTION_H
#define LSHADERDESCRIPTION_H
#include <list>
#include <array>
#include "lkcore/lflags.h"

LK_NAMESPACE_BEGIN

enum class LShaderImageFlag : uint8_t {
    ReadOnly = 1 << 0,
    WriteOnly = 1 << 1
};
L_EN_FLAGS(LShaderImageFlag);

enum class LShaderQualifierFlag : uint8_t {
    ReadOnly = 1 << 0,
    WriteOnly = 1 << 1,
    Coherent = 1 << 2,
    Volatile = 1 << 3,
    Restrict = 1 << 4,
};
L_EN_FLAGS(LShaderQualifierFlag)

class LKGUIDEF LShaderDescription
{
public:
    LShaderDescription();

    bool isValid() const;

    enum VariableType {
        Unknown = 0,

        // do not reorder
        Float,
        Vec2,
        Vec3,
        Vec4,
        Mat2,
        Mat2x3,
        Mat2x4,
        Mat3,
        Mat3x2,
        Mat3x4,
        Mat4,
        Mat4x2,
        Mat4x3,

        Int,
        Int2,
        Int3,
        Int4,

        Uint,
        Uint2,
        Uint3,
        Uint4,

        Bool,
        Bool2,
        Bool3,
        Bool4,

        Double,
        Double2,
        Double3,
        Double4,
        DMat2,
        DMat2x3,
        DMat2x4,
        DMat3,
        DMat3x2,
        DMat3x4,
        DMat4,
        DMat4x2,
        DMat4x3,

        Sampler1D,
        Sampler2D,
        Sampler2DMS,
        Sampler3D,
        SamplerCube,
        Sampler1DArray,
        Sampler2DArray,
        Sampler2DMSArray,
        Sampler3DArray,
        SamplerCubeArray,
        SamplerRect,
        SamplerBuffer,
        SamplerExternalOES,
        Sampler,

        Image1D,
        Image2D,
        Image2DMS,
        Image3D,
        ImageCube,
        Image1DArray,
        Image2DArray,
        Image2DMSArray,
        Image3DArray,
        ImageCubeArray,
        ImageRect,
        ImageBuffer,

        Struct,

        Half,
        Half2,
        Half3,
        Half4
    };

    enum ImageFormat {
        // must match SPIR-V's ImageFormat
        ImageFormatUnknown = 0,
        ImageFormatRgba32f = 1,
        ImageFormatRgba16f = 2,
        ImageFormatR32f = 3,
        ImageFormatRgba8 = 4,
        ImageFormatRgba8Snorm = 5,
        ImageFormatRg32f = 6,
        ImageFormatRg16f = 7,
        ImageFormatR11fG11fB10f = 8,
        ImageFormatR16f = 9,
        ImageFormatRgba16 = 10,
        ImageFormatRgb10A2 = 11,
        ImageFormatRg16 = 12,
        ImageFormatRg8 = 13,
        ImageFormatR16 = 14,
        ImageFormatR8 = 15,
        ImageFormatRgba16Snorm = 16,
        ImageFormatRg16Snorm = 17,
        ImageFormatRg8Snorm = 18,
        ImageFormatR16Snorm = 19,
        ImageFormatR8Snorm = 20,
        ImageFormatRgba32i = 21,
        ImageFormatRgba16i = 22,
        ImageFormatRgba8i = 23,
        ImageFormatR32i = 24,
        ImageFormatRg32i = 25,
        ImageFormatRg16i = 26,
        ImageFormatRg8i = 27,
        ImageFormatR16i = 28,
        ImageFormatR8i = 29,
        ImageFormatRgba32ui = 30,
        ImageFormatRgba16ui = 31,
        ImageFormatRgba8ui = 32,
        ImageFormatR32ui = 33,
        ImageFormatRgb10a2ui = 34,
        ImageFormatRg32ui = 35,
        ImageFormatRg16ui = 36,
        ImageFormatRg8ui = 37,
        ImageFormatR16ui = 38,
        ImageFormatR8ui = 39
    };

    // Optional data (like decorations) usually default to an otherwise invalid value (-1 or 0). This is intentional.

    struct BlockVariable {
        std::string name;
        VariableType type = Unknown;
        int offset = 0;
        int size = 0;
        std::list<int> arrayDims;
        int arrayStride = 0;
        int matrixStride = 0;
        bool matrixIsRowMajor = false;
        std::list<BlockVariable> structMembers;
    };

    struct InOutVariable {
        std::string name;
        VariableType type = Unknown;
        int location = -1;
        int binding = -1;
        int descriptorSet = -1;
        ImageFormat imageFormat = ImageFormatUnknown;
        LFlags<LShaderImageFlag> imageFlags;
        std::list<int> arrayDims;
        bool perPatch = false;
        std::list<BlockVariable> structMembers;
    };

    struct UniformBlock {
        std::string blockName;
        std::string structName; // instanceName
        int size = 0;
        int binding = -1;
        int descriptorSet = -1;
        std::list<BlockVariable> members;
    };

    struct PushConstantBlock {
        std::string name;
        int size = 0;
        std::list<BlockVariable> members;
    };

    struct StorageBlock {
        std::string blockName;
        std::string instanceName;
        int knownSize = 0;
        int binding = -1;
        int descriptorSet = -1;
        std::list<BlockVariable> members;
        int runtimeArrayStride = 0;
        LFlags<LShaderQualifierFlag> qualifierFlags;
    };

    enum BuiltinType {
        // must match SpvBuiltIn
        PositionBuiltin = 0,
        PointSizeBuiltin = 1,
        ClipDistanceBuiltin = 3,
        CullDistanceBuiltin = 4,
        VertexIdBuiltin = 5,
        InstanceIdBuiltin = 6,
        PrimitiveIdBuiltin = 7,
        InvocationIdBuiltin = 8,
        LayerBuiltin = 9,
        ViewportIndexBuiltin = 10,
        TessLevelOuterBuiltin = 11,
        TessLevelInnerBuiltin = 12,
        TessCoordBuiltin = 13,
        PatchVerticesBuiltin = 14,
        FragCoordBuiltin = 15,
        PointCoordBuiltin = 16,
        FrontFacingBuiltin = 17,
        SampleIdBuiltin = 18,
        SamplePositionBuiltin = 19,
        SampleMaskBuiltin = 20,
        FragDepthBuiltin = 22,
        NumWorkGroupsBuiltin = 24,
        WorkgroupSizeBuiltin = 25,
        WorkgroupIdBuiltin = 26,
        LocalInvocationIdBuiltin = 27,
        GlobalInvocationIdBuiltin = 28,
        LocalInvocationIndexBuiltin = 29,
        VertexIndexBuiltin = 42,
        InstanceIndexBuiltin = 43,
        ViewIndexBuiltin = 4440,
    };

    struct BuiltinVariable {
        BuiltinType type;
        VariableType varType;
        std::list<int> arrayDims;
    };

    enum TessellationMode {
        UnknownTessellationMode,
        TrianglesTessellationMode,
        QuadTessellationMode,
        IsolineTessellationMode
    };

    enum TessellationWindingOrder {
        UnknownTessellationWindingOrder,
        CwTessellationWindingOrder,
        CcwTessellationWindingOrder
    };

    enum TessellationPartitioning {
        UnknownTessellationPartitioning,
        EqualTessellationPartitioning,
        FractionalEvenTessellationPartitioning,
        FractionalOddTessellationPartitioning
    };

    std::list<InOutVariable> m_inVars;
    std::list<InOutVariable> m_outVars;
    std::list<UniformBlock> m_uniformBlocks;
    std::list<PushConstantBlock> m_pushConstantBlocks;
    std::list<StorageBlock> m_storageBlocks;
    std::list<InOutVariable> m_combinedImageSamplers;
    std::list<InOutVariable> m_separateImages;
    std::list<InOutVariable> m_separateSamplers;
    std::list<InOutVariable> m_storageImages;
    std::list<BuiltinVariable> m_inBuiltins;
    std::list<BuiltinVariable> m_outBuiltins;
    std::array<uint32_t, 3> m_localSize = {};
    uint32_t m_tessOutVertCount = 0;
    TessellationMode m_tessMode = UnknownTessellationMode;
    TessellationWindingOrder m_tessWind = UnknownTessellationWindingOrder;
    TessellationPartitioning m_tessPart = UnknownTessellationPartitioning;
};

LKGUIDEF bool operator==(const LShaderDescription& lhs, const LShaderDescription &rhs) noexcept;
LKGUIDEF bool operator==(const LShaderDescription::InOutVariable &lhs, const LShaderDescription::InOutVariable &rhs) noexcept;
LKGUIDEF bool operator==(const LShaderDescription::BlockVariable &lhs, const LShaderDescription::BlockVariable &rhs) noexcept;
LKGUIDEF bool operator==(const LShaderDescription::UniformBlock &lhs, const LShaderDescription::UniformBlock &rhs) noexcept;
LKGUIDEF bool operator==(const LShaderDescription::PushConstantBlock &lhs, const LShaderDescription::PushConstantBlock &rhs) noexcept;
LKGUIDEF bool operator==(const LShaderDescription::StorageBlock &lhs, const LShaderDescription::StorageBlock &rhs) noexcept;
LKGUIDEF bool operator==(const LShaderDescription::BuiltinVariable &lhs, const LShaderDescription::BuiltinVariable &rhs) noexcept;

inline bool operator!=(const LShaderDescription &lhs, const LShaderDescription &rhs) noexcept
{
    return !(lhs == rhs);
}

inline bool operator!=(const LShaderDescription::InOutVariable &lhs, const LShaderDescription::InOutVariable &rhs) noexcept
{
    return !(lhs == rhs);
}

inline bool operator!=(const LShaderDescription::BlockVariable &lhs, const LShaderDescription::BlockVariable &rhs) noexcept
{
    return !(lhs == rhs);
}

inline bool operator!=(const LShaderDescription::UniformBlock &lhs, const LShaderDescription::UniformBlock &rhs) noexcept
{
    return !(lhs == rhs);
}

inline bool operator!=(const LShaderDescription::PushConstantBlock &lhs, const LShaderDescription::PushConstantBlock &rhs) noexcept
{
    return !(lhs == rhs);
}

inline bool operator!=(const LShaderDescription::StorageBlock &lhs, const LShaderDescription::StorageBlock &rhs) noexcept
{
    return !(lhs == rhs);
}

inline bool operator!=(const LShaderDescription::BuiltinVariable &lhs, const LShaderDescription::BuiltinVariable &rhs) noexcept
{
    return !(lhs == rhs);
}

LK_NAMESPACE_END
#endif // LSHADERDESCRIPTION_H
