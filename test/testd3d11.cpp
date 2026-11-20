#include <chrono>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <d3dcompiler.h>
#endif

#include "lkcore/ldebug.h"
#include "lkcore/livar.h"
#include "lkgui/lguiapplication.h"
#include "lkgui/lwindow.h"
#include "lkgui/rhi/lrhi.h"

#if LOOKI_NAMESPACE
using namespace lk;
#endif

static LShader compileHlsl(const char *hlsl, const char *target, LShader::Stage stage)
{
    LShader result;
#ifdef _WIN32
    ID3DBlob *code = nullptr;
    ID3DBlob *error = nullptr;
    const HRESULT hr = D3DCompile(hlsl, std::strlen(hlsl), nullptr, nullptr, nullptr,
                                  "main", target, 0, 0, &code, &error);
    if (FAILED(hr)) {
        if (error != nullptr) {
            lWarn() << "[d3d11] HLSL compile failed:" << static_cast<const char *>(error->GetBufferPointer());
            error->Release();
        }
        return result;
    }
    if (error != nullptr) {
        error->Release();
    }

    auto shaderCode = std::make_shared<LShaderCode>();
    const uint8_t *begin = static_cast<const uint8_t *>(code->GetBufferPointer());
    shaderCode->m_shader.assign(begin, begin + code->GetBufferSize());
    const char *entry = "main";
    shaderCode->m_entryPoint.assign(entry, entry + 4);
    code->Release();

    LShaderKey key;
    key.m_source = LShader::DxbcShader;
    key.m_variant = LShader::StandardShader;
    result.m_stage = stage;
    result.m_shaders[key] = shaderCode;
#endif
    return result;
}

int main(int argc, char **argv)
{
#ifdef _WIN32
    LLoger::defaultLogEncode = GetACP() == CP_UTF8 ? "" : "gbk";
#endif

    std::shared_ptr<LGuiApplication> app = makeInit<LGuiApplication>(argc, argv);
    std::shared_ptr<LWindow> window = makeInit<LWindow>();
    window->setTitle(L"Looki D3D11 RHI Test");
    window->resize(LSize<int>(800, 600));
    window->create();
    window->setVisible(true);

    const LShader vs = compileHlsl(
        "Texture2D<float4> tex : register(t0);\n"
        "SamplerState samp : register(s0);\n"
        "struct VSIn { float4 pos : POSITION; float4 color : COLOR; float2 uv : TEXCOORD0; };\n"
        "struct VSOut { float4 pos : SV_Position; float4 color : COLOR; float2 uv : TEXCOORD0; };\n"
        "VSOut main(VSIn i) { VSOut o; o.pos = i.pos; o.color = i.color; o.uv = i.uv; return o; }\n",
        "vs_5_0", LShader::VertexStage);
    const LShader fs = compileHlsl(
        "Texture2D<float4> tex : register(t0);\n"
        "SamplerState samp : register(s0);\n"
        "struct VSOut { float4 pos : SV_Position; float4 color : COLOR; float2 uv : TEXCOORD0; };\n"
        "float4 main(VSOut i) : SV_Target { return tex.Sample(samp, i.uv) * i.color; }\n",
        "ps_5_0", LShader::FragmentStage);

    auto rhi = LRhi::create(LRhi::D3D11);
    if (rhi == nullptr) {
        lWarn() << "[d3d11] failed to create RHI";
        return 1;
    }
    lWarn() << "[d3d11] backend created";

    std::shared_ptr<LRhiSwapChain> sc = rhi->newSwapChain();
    sc->m_window = window;

    auto tex = rhi->newTexture(LRhiTexture::RGBA8, LSize<int>(4, 4));
    auto sampler = rhi->newSampler(LRhiSampler::Linear, LRhiSampler::Linear,
                                   LRhiSampler::Linear, LRhiSampler::Repeat,
                                   LRhiSampler::Repeat, LRhiSampler::Repeat);
    if (!tex->create() || !sampler->create()) {
        lWarn() << "[d3d11] texture or sampler creation failed";
        return 1;
    }
    auto srb = rhi->newShaderResourceBindings();
    srb->m_bindings.push_back(LRhiShaderResourceBinding::sampledTexture(
                0, LRhiShaderResourceBindingStage::Fragment, tex, sampler));
    if (!srb->create()) {
        lWarn() << "[d3d11] SRB creation failed";
        return 1;
    }

    auto ps = rhi->newGraphicsPipeline();
    ps->m_shaderResourceBindings = srb;
    ps->m_renderPassDesc = sc->newCompatibleRenderPassDescriptor();
    ps->m_depthTest = true;
    ps->m_depthWrite = true;
    ps->m_depthOp = LRhiGraphicsPipeline::Less;
    ps->m_shaderStages.push_back(LRhiShaderStage(LRhiShaderStage::Vertex, vs));
    ps->m_shaderStages.push_back(LRhiShaderStage(LRhiShaderStage::Fragment, fs));

    LRhiVertexInputBinding vb;
    vb.m_stride = 10 * sizeof(float);
    LRhiVertexInputAttribute posAttr;
    posAttr.m_binding = 0;
    posAttr.m_location = 0;
    posAttr.m_format = LRhiVertexInputAttribute::Float4;
    posAttr.m_offset = 0;
    posAttr.m_name = "POSITION";
    LRhiVertexInputAttribute colorAttr;
    colorAttr.m_binding = 0;
    colorAttr.m_location = 1;
    colorAttr.m_format = LRhiVertexInputAttribute::Float4;
    colorAttr.m_offset = 4 * sizeof(float);
    colorAttr.m_name = "COLOR";
    LRhiVertexInputAttribute uvAttr;
    uvAttr.m_binding = 0;
    uvAttr.m_location = 2;
    uvAttr.m_format = LRhiVertexInputAttribute::Float2;
    uvAttr.m_offset = 8 * sizeof(float);
    uvAttr.m_name = "TEXCOORD";
    ps->m_vertexInputLayout.m_bindings.push_back(vb);
    ps->m_vertexInputLayout.m_attributes.push_back(posAttr);
    ps->m_vertexInputLayout.m_attributes.push_back(colorAttr);
    ps->m_vertexInputLayout.m_attributes.push_back(uvAttr);
    if (!ps->create()) {
        lWarn() << "[d3d11] pipeline creation failed";
        return 1;
    }

    auto vbuf = rhi->newBuffer(LRhiBuffer::Dynamic, LRhiBufferUsage::Vertex,
                               3 * 10 * sizeof(float));
    auto ibuf = rhi->newBuffer(LRhiBuffer::Dynamic, LRhiBufferUsage::Index,
                               3 * sizeof(uint16_t));
    if (!vbuf->create() || !ibuf->create()) {
        lWarn() << "[d3d11] buffer creation failed";
        return 1;
    }
    const uint16_t indices[] = { 0, 1, 2 };
    if (char *dst = ibuf->beginFullDynamicBufferUpdateForCurrentFrame()) {
        std::memcpy(dst, indices, sizeof(indices));
        ibuf->endFullDynamicBufferUpdateForCurrentFrame();
    }

    std::vector<uint8_t> texData(4 * 4 * 4);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            const uint8_t *color = ((x / 2) ^ (y / 2)) == 0
                    ? reinterpret_cast<const uint8_t *>("\xff\x40\x20\xff")
                    : reinterpret_cast<const uint8_t *>("\x20\x40\xff\xff");
            std::memcpy(texData.data() + (y * 4 + x) * 4, color, 4);
        }
    }
    LRhiTextureSubresourceUploadDescription texUpload(texData);
    texUpload.m_dataStride = 4 * 4;
    texUpload.m_sourceSize = LSize<int>(4, 4);
    LRhiTextureUploadDescription texDesc(LRhiTextureUploadEntry(0, 0, texUpload));
    auto updates = rhi->newResourceUpdateBatch();
    updates->uploadTexture(tex, texDesc);

    const auto startTime = std::chrono::steady_clock::now();
    const HWND hwnd = reinterpret_cast<HWND>(static_cast<uintptr_t>(window->winId()));
    lWarn() << "[d3d11] entering render loop, hwnd =" << reinterpret_cast<void *>(hwnd);

    int frameNo = 0;
    for (;;) {
#ifdef _WIN32
        MSG msg{};
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                return 0;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!IsWindow(hwnd)) {
            break;
        }
        if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
            break;
        }
#endif

        const float t = float(std::chrono::duration<double>(
                                  std::chrono::steady_clock::now() - startTime).count());
        const float base[3][3] = {
            {  0.00f,  0.55f,  0.35f },
            { -0.50f, -0.45f,  0.00f },
            {  0.50f, -0.45f, -0.35f }
        };
        LMatrix4x4 model;
        model.rotate(float(t * 60.0), 0.0f, 1.0f, 0.0f);
        const float aspect = 800.0f / 600.0f;
        const float focal = 1.0f / std::tan(float(30.0) * 3.14159265f / 180.0f);
        const float nearPlane = 0.1f;
        const float farPlane = 100.0f;

        float vertices[3][10] = {};
        for (int i = 0; i < 3; ++i) {
            LVector4D pos = LVector4D(base[i][0], base[i][1], base[i][2], 1.0f) * model;
            pos.setZ(pos.z() - 1.6f);
            const float w = -pos.z();
            const float z = pos.z() * farPlane / (farPlane - nearPlane)
                    - nearPlane * farPlane / (farPlane - nearPlane);
            vertices[i][0] = pos.x() * focal / aspect;
            vertices[i][1] = pos.y() * focal;
            vertices[i][2] = z;
            vertices[i][3] = w;
            vertices[i][4] = i == 0 ? 1.0f : 0.30f;
            vertices[i][5] = i == 1 ? 1.0f : 0.45f;
            vertices[i][6] = i == 2 ? 1.0f : 0.40f;
            vertices[i][7] = 1.0f;
            vertices[i][8] = i == 0 ? 0.5f : float(i);
            vertices[i][9] = i == 0 ? 0.0f : 1.0f;
        }
        if (char *dst = vbuf->beginFullDynamicBufferUpdateForCurrentFrame()) {
            std::memcpy(dst, vertices, sizeof(vertices));
            vbuf->endFullDynamicBufferUpdateForCurrentFrame();
        }

        if (rhi->beginFrame(sc) != LRhi::FrameOpSuccess) {
            lWarn() << "[d3d11] beginFrame failed";
            break;
        }
        if (frameNo == 0) {
            const LSize<int> ps = sc->currentPixelSize();
            lWarn() << "[d3d11] frame0 size =" << ps.m_x << "x" << ps.m_y;
        }
        std::shared_ptr<LRhiCommandBuffer> cb = sc->currentFrameCommandBuffer();
        if (frameNo == 0) {
            cb->resourceUpdate(updates);
        }
        cb->beginPass(sc->currentFrameRenderTarget(), LColor(24, 38, 64),
                      LRhiDepthStencilClearValue());
        cb->setGraphicsPipeline(ps);
        cb->setShaderResources(srb);
        LRhiCommandBuffer::VertexInput bindings[] = { { vbuf, 0 } };
        cb->setVertexInput(0, 1, bindings, ibuf, 0, LRhiCommandBuffer::IndexUInt16);
        cb->drawIndexed(3);
        cb->endPass();
        const LRhi::FrameOpResult frameResult = rhi->endFrame(sc);
        if (frameNo == 0) {
            lWarn() << "[d3d11] frame0 end result =" << int(frameResult);
        }
        if (frameResult != LRhi::FrameOpSuccess) {
            lWarn() << "[d3d11] endFrame failed";
            break;
        }
        ++frameNo;
    }

    rhi->finish();
    lWarn() << "[d3d11] render loop stopped";
    return 0;
}
