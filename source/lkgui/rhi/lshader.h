#ifndef LSHADER_H
#define LSHADER_H

#include "lkcore/livar.h"

LK_NAMESPACE_BEGIN

struct LKGUIDEF LShaderKey{
    union{
        uint16_t m_value = 0;
        struct{
            uint16_t m_source:4;
            uint16_t m_variant:4;
            uint16_t m_res : 8;
        };
    };
};

class LKGUIDEF LShaderCode : public LIVar
{
public:
    LShaderCode();
    std::vector<uint8_t> m_shader;
    std::vector<uint8_t> m_entryPoint;
};

class LShaderDescription;
class LKGUIDEF LShader : public LIVar
{
public:
    LShader();

    enum Stage {
        VertexStage = 0,
        TessellationControlStage,
        TessellationEvaluationStage,
        GeometryStage,
        FragmentStage,
        ComputeStage
    };

    enum Source {
        SpirvShader = 0,
        GlslShader,
        HlslShader,
        DxbcShader, // fxc
        MslShader,
        DxilShader, // dxc
        MetalLibShader, // xcrun metal + xcrun metallib
        WgslShader
    };

    enum Variant {
        StandardShader = 0,
        BatchableVertexShader,
        UInt16IndexedVertexAsComputeShader,
        UInt32IndexedVertexAsComputeShader,
        NonIndexedVertexAsComputeShader
    };

    bool isValid() const;
    std::list<LShaderKey> availableShaders() const;

    using NativeResourceBindingMap = std::map<int, std::pair<int, int>>; // binding -> native_binding[, native_binding]

    struct SeparateToCombinedImageSamplerMapping {
        std::vector<uint8_t> combinedSamplerName;
        int textureBinding;
        int samplerBinding;
    };
    using SeparateToCombinedImageSamplerMappingList = std::list<SeparateToCombinedImageSamplerMapping>;
    struct NativeShaderInfo {
        int flags = 0;
        std::map<int, int> extraBufferBindings;
    };

    LShader::Stage m_stage = VertexStage;
    std::shared_ptr<LShaderDescription> m_description;

    std::map<LShaderKey, std::shared_ptr<LShaderCode> > m_shaders;
    std::map<LShaderKey, NativeResourceBindingMap> m_bindings;
    std::map<LShaderKey, SeparateToCombinedImageSamplerMappingList> m_combinedImageMap;
    std::map<LShaderKey, NativeShaderInfo> m_nativeShaderInfoMap;

    friend LKGUIDEF bool operator==(const LShader &, const LShader &) noexcept;
};


LKGUIDEF bool operator==(const LShader &lhs, const LShader &rhs) noexcept;
inline bool operator!=(const LShader &lhs, const LShader &rhs) noexcept
{
    return !(lhs == rhs);
}

LKGUIDEF bool operator==(const LShaderKey &lhs, const LShaderKey &rhs) noexcept;
LKGUIDEF bool operator<(const LShaderKey &lhs, const LShaderKey &rhs) noexcept;
LKGUIDEF bool operator==(const LShaderCode &lhs, const LShaderCode &rhs) noexcept;


inline bool operator!=(const LShaderKey &lhs, const LShaderKey &rhs) noexcept
{
    return !(lhs == rhs);
}

inline bool operator!=(const LShaderCode &lhs, const LShaderCode &rhs) noexcept
{
    return !(lhs == rhs);
}

LK_NAMESPACE_END


template<>
struct std::hash<_LK::LShaderCode>{
    std::size_t operator()(const _LK::LShaderCode& key) const noexcept
    {
        return XXH32(key.m_shader.data(), key.m_shader.size(), 0);
    }
};

template<>
struct std::hash<_LK::LShader>{
    std::size_t operator()(const _LK::LShader& key) const noexcept
    {
        uint32_t seed=0;
        if (!key.m_shaders.empty()) {
            std::map<_LK::LShaderKey, std::shared_ptr<_LK::LShaderCode> >::const_iterator iter = key.m_shaders.cbegin();
            seed = _LK::lHashCombine(seed, iter->first.m_value);
            seed = _LK::lHashCombine(seed, iter->second);
        }
        return seed;
    }
};

#endif // LSHADER_H
