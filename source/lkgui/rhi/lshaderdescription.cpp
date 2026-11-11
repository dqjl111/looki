#include "lshaderdescription.h"

LK_NAMESPACE_BEGIN

LShaderDescription::LShaderDescription() {}

bool LShaderDescription::isValid() const
{
    return !m_inVars.empty() || !m_outVars.empty()
    || !m_uniformBlocks.empty() || !m_pushConstantBlocks.empty() || !m_storageBlocks.empty()
        || !m_combinedImageSamplers.empty() || !m_storageImages.empty()
        || !m_separateImages.empty() || !m_separateSamplers.empty()
        || !m_inBuiltins.empty() || !m_outBuiltins.empty();
}


bool operator==(const LShaderDescription &lhs, const LShaderDescription &rhs) noexcept
{
    return lhs.m_inVars == rhs.m_inVars
           && lhs.m_outVars == rhs.m_outVars
           && lhs.m_uniformBlocks == rhs.m_uniformBlocks
           && lhs.m_pushConstantBlocks == rhs.m_pushConstantBlocks
           && lhs.m_storageBlocks == rhs.m_storageBlocks
           && lhs.m_combinedImageSamplers == rhs.m_combinedImageSamplers
           && lhs.m_separateImages == rhs.m_separateImages
           && lhs.m_separateSamplers == rhs.m_separateSamplers
           && lhs.m_storageImages == rhs.m_storageImages
           && lhs.m_inBuiltins == rhs.m_inBuiltins
           && lhs.m_outBuiltins == rhs.m_outBuiltins
           && lhs.m_localSize == rhs.m_localSize
           && lhs.m_tessOutVertCount == rhs.m_tessOutVertCount
           && lhs.m_tessMode == rhs.m_tessMode
           && lhs.m_tessWind == rhs.m_tessWind
           && lhs.m_tessPart == rhs.m_tessPart;
}


bool operator==(const LShaderDescription::InOutVariable &lhs, const LShaderDescription::InOutVariable &rhs) noexcept
{
    return lhs.name == rhs.name
           && lhs.type == rhs.type
           && lhs.location == rhs.location
           && lhs.binding == rhs.binding
           && lhs.descriptorSet == rhs.descriptorSet
           && lhs.imageFormat == rhs.imageFormat
           && lhs.imageFlags == rhs.imageFlags
           && lhs.arrayDims == rhs.arrayDims
           && lhs.perPatch == rhs.perPatch
           && lhs.structMembers == rhs.structMembers;
}

bool operator==(const LShaderDescription::BlockVariable &lhs, const LShaderDescription::BlockVariable &rhs) noexcept
{
    return lhs.name == rhs.name
           && lhs.type == rhs.type
           && lhs.offset == rhs.offset
           && lhs.size == rhs.size
           && lhs.arrayDims == rhs.arrayDims
           && lhs.arrayStride == rhs.arrayStride
           && lhs.matrixStride == rhs.matrixStride
           && lhs.matrixIsRowMajor == rhs.matrixIsRowMajor
           && lhs.structMembers == rhs.structMembers;
}

bool operator==(const LShaderDescription::UniformBlock &lhs, const LShaderDescription::UniformBlock &rhs) noexcept
{
    return lhs.blockName == rhs.blockName
           && lhs.structName == rhs.structName
           && lhs.size == rhs.size
           && lhs.binding == rhs.binding
           && lhs.descriptorSet == rhs.descriptorSet
           && lhs.members == rhs.members;
}


bool operator==(const LShaderDescription::PushConstantBlock &lhs, const LShaderDescription::PushConstantBlock &rhs) noexcept
{
    return lhs.name == rhs.name
           && lhs.size == rhs.size
           && lhs.members == rhs.members;
}

bool operator==(const LShaderDescription::StorageBlock &lhs, const LShaderDescription::StorageBlock &rhs) noexcept
{
    return lhs.blockName == rhs.blockName
           && lhs.instanceName == rhs.instanceName
           && lhs.knownSize == rhs.knownSize
           && lhs.binding == rhs.binding
           && lhs.descriptorSet == rhs.descriptorSet
           && lhs.runtimeArrayStride == rhs.runtimeArrayStride
           && lhs.qualifierFlags == rhs.qualifierFlags
           && lhs.members == rhs.members;
}


bool operator==(const LShaderDescription::BuiltinVariable &lhs, const LShaderDescription::BuiltinVariable &rhs) noexcept
{
    return lhs.type == rhs.type
           && lhs.varType == rhs.varType
           && lhs.arrayDims == rhs.arrayDims;
}
LK_NAMESPACE_END