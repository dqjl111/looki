#ifndef LRHIPLATFORM_H
#define LRHIPLATFORM_H

#include "lkgui/rhi/lrhi.h"

LK_NAMESPACE_BEGIN

struct LKGUIDEF LRhiNullInitParams : public LRhiInitParams
{
};

struct LKGUIDEF LRhiNullNativeHandles : public LRhiNativeHandles
{
};


#ifdef WIN32
struct LKGUIDEF LRhiD3D12InitParams : public LRhiInitParams
{
    bool enableDebugLayer = false;
};

struct LKGUIDEF LRhiD3D12NativeHandles : public LRhiNativeHandles
{
    // to import a device
    void *dev = nullptr;
    int minimumFeatureLevel = 0;
    // to just specify the adapter to use, set these and leave dev set to null
    uint32_t adapterLuidLow = 0;
    int32_t adapterLuidHigh = 0;
    // in addition, can specify the command queue to use
    void *commandQueue = nullptr;
};

struct LKGUIDEF LRhiD3D12CommandBufferNativeHandles : public LRhiNativeHandles
{
    void *commandList = nullptr; // ID3D12GraphicsCommandList1
};

// ---------------------------------------------------------------------------
// D3D11 后端
// ---------------------------------------------------------------------------
struct LKGUIDEF LRhiD3D11InitParams : public LRhiInitParams
{
    bool enableDebugLayer = false;
    /** 允许用 D3DCompile 现场编译 HLSL（关掉则只接受预编译 DXBC） */
    bool compileHlsl = true;
};

struct LKGUIDEF LRhiD3D11NativeHandles : public LRhiNativeHandles
{
    void *dev = nullptr;        // ID3D11Device*，非空则导入而不新建
    void *context = nullptr;    // ID3D11DeviceContext*
    uint32_t adapterLuidLow = 0;
    int32_t adapterLuidHigh = 0;
    int featureLevel = 0;
};

struct LKGUIDEF LRhiD3D11CommandBufferNativeHandles : public LRhiNativeHandles
{
    void *context = nullptr;    // ID3D11DeviceContext1*
};
#endif // MACOS/IOS/QDOC

LK_NAMESPACE_END
#endif // LRHIPLATFORM_H
