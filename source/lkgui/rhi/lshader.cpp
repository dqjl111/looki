#include "lshader.h"

LK_NAMESPACE_BEGIN

LShaderCode::LShaderCode()
{

}


LShader::LShader() {

}

bool LShader::isValid() const
{
    return !m_shaders.empty();
}

std::list<LShaderKey> LShader::availableShaders() const
{
    std::list<LShaderKey> keys;
    for(const auto& kv : m_shaders) {
        keys.push_back(kv.first);
    }
    return keys;
}

bool operator==(const LShader &lhs, const LShader &rhs) noexcept
{
    return lhs.m_stage == rhs.m_stage
           && lhs.m_shaders == rhs.m_shaders
           && lhs.m_bindings == rhs.m_bindings;
}

bool operator==(const LShaderKey &lhs, const LShaderKey &rhs) noexcept
{
    return lhs.m_source == rhs.m_source && lhs.m_variant == rhs.m_variant;
}

bool operator<(const LShaderKey &lhs, const LShaderKey &rhs) noexcept
{
    if (int(lhs.m_source) < int(rhs.m_source))
        return true;

    if (int(lhs.m_source) == int(rhs.m_source)) {
        if (int(lhs.m_variant) < int(rhs.m_variant)){
            return true;
        }
    }

    return false;
}

bool operator==(const LShaderCode &lhs, const LShaderCode &rhs) noexcept
{
    return lhs.m_shader == rhs.m_shader && lhs.m_entryPoint == rhs.m_entryPoint;
}

LK_NAMESPACE_END
