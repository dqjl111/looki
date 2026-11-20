#include "lkmedia/lvideowindow.h"
#include "lkcore/ldebug.h"
#include <algorithm>
#include <chrono>
#include <cstring>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <d3d11.h>
#  include <d3dcompiler.h>
#  include <wrl.h>
#  ifdef max
#    undef max
#  endif
#  ifdef min
#    undef min
#  endif
#endif

#include "lkgui/rhi/lrhiplatform.h"

LK_NAMESPACE_BEGIN

namespace {

#ifdef _WIN32
LShader lCompileVideoShader(const char *source, const char *target, LShader::Stage stage)
{
    ID3DBlob *code = nullptr;
    ID3DBlob *error = nullptr;
    const HRESULT hr = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr,
                                  "main", target, 0, 0, &code, &error);
    if (FAILED(hr)) {
        if (error != nullptr) {
            lWarn() << "lkmedia/RHI: shader compile failed:"
                    << static_cast<const char *>(error->GetBufferPointer());
            error->Release();
        }
        return LShader();
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

    LShader shader;
    shader.m_stage = stage;
    LShaderKey key;
    key.m_source = LShader::DxbcShader;
    key.m_variant = LShader::StandardShader;
    shader.m_shaders[key] = shaderCode;
    return shader;
}

#endif

} // namespace

LVideoWindow::LVideoWindow()
{
    ensurePlayer();
}

LVideoWindow::~LVideoWindow()
{
    // 必须先停渲染线程：它持有 MediaEngine 和 RHI 资源。
    stopRenderThread();
    if (m_timer != nullptr) {
        m_timer->stop();
    }
    if (m_player != nullptr) {
        m_player->shutdown();
    }
}

void LVideoWindow::ensurePlayer()
{
    if (m_player == nullptr) {
        m_player = std::make_shared<LVideoPlayer>();
    }
}

bool LVideoWindow::ensureRhi()
{
#ifdef _WIN32
    if (m_rhiReady) {
        return true;
    }
    if (winId() == 0) {
        return false;
    }
    m_rhi = LRhi::create(LRhi::D3D11);
    if (m_rhi == nullptr) {
        return false;
    }
    m_swapChain = m_rhi->newSwapChain();
    if (m_swapChain == nullptr) {
        return false;
    }
    m_swapChain->m_window = getPtrT<LWindow>();
    m_rhiReady = true;

    if (m_player != nullptr) {
        // 之前若已用兜底 RHI 初始化过，必须重建，确保帧纹理与本窗口同一 device。
        if (m_player->isInitialized()) {
            m_player->shutdown();
        }
        LVideoBackendDesc desc;
        desc.rhi = m_rhi;
        if (!m_player->initialize(desc)) {
            lWarn() << "lkmedia: video window failed to initialize player";
            return false;
        }
    }

    // RHI 与播放器就绪后再启动渲染线程。
    startRenderThread();
    return true;
#else
    return false;
#endif
}

void LVideoWindow::resetVideoResources()
{
    // MF 后端换源时会销毁并重建共享纹理，之前 OpenSharedResource 导入的
    // RHI 纹理与缓存的 native 指针全部失效。若不清空，新纹理指针复用旧
    // 地址时会命中失效纹理，表现为第二次打开文件卡死或崩溃。
    m_videoTexture.reset();
    m_videoTextureKeys[0] = nullptr;
    m_videoTextureKeys[1] = nullptr;
    m_videoTextures[0].reset();
    m_videoTextures[1].reset();
    m_videoGeneration = 0;
    m_videoFenceValue = 0;
    // srb 绑定的是旧纹理，pipeline 又持有 srb 引用，两者一起重建。
    m_srb.reset();
    m_pipeline.reset();
}

bool LVideoWindow::open(const std::string &uri)
{
    ensurePlayer();
    // 必须让播放器用本窗口的 RHI 初始化：否则 LVideoPlayer::open() 会退回
    // 自建的兜底 device，帧纹理就属于另一个 device，无法在本窗口采样。
    if (!ensureRhi()) {
        lWarn() << "lkmedia: video window RHI not ready, open aborted:" << uri;
        return false;
    }
    // 与渲染线程串行：换源会重建纹理并销毁 MediaEngine。
    std::lock_guard<std::mutex> lock(m_engineMutex);
    resetVideoResources();
    m_latestFrame = LVideoFrame();
    m_frameWidth = 0;
    m_frameHeight = 0;
    const bool ok = m_player->open(uri);
    if (!ok) {
        lWarn() << "lkmedia: open failed:" << uri;
    }
    restartTimer();
    return ok;
}

void LVideoWindow::close()
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    if (m_player != nullptr) {
        m_player->close();
    }
    resetVideoResources();
    m_latestFrame = LVideoFrame();
    m_frameWidth = 0;
    m_frameHeight = 0;
    restartTimer();
}

void LVideoWindow::play()
{
    ensurePlayer();
    std::lock_guard<std::mutex> lock(m_engineMutex);
    m_player->play();
    restartTimer();
}

void LVideoWindow::pause()
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    if (m_player != nullptr) {
        m_player->pause();
    }
    restartTimer();
}

void LVideoWindow::stop()
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    if (m_player != nullptr) {
        m_player->stop();
    }
    m_latestFrame = LVideoFrame();
    restartTimer();
}

bool LVideoWindow::seek(double seconds)
{
    std::lock_guard<std::mutex> lock(m_engineMutex);
    return m_player != nullptr && m_player->seek(seconds);
}

void LVideoWindow::setLoop(bool loop)
{
    if (m_player != nullptr) {
        m_player->setLoop(loop);
    }
}

void LVideoWindow::setVolume(double volume)
{
    if (m_player != nullptr) {
        m_player->setVolume(volume);
    }
}

void LVideoWindow::setMuted(bool muted)
{
    if (m_player != nullptr) {
        m_player->setMuted(muted);
    }
}

LVideoState LVideoWindow::state() const
{
    return m_player != nullptr ? m_player->state() : LVideoState::Idle;
}

double LVideoWindow::duration() const
{
    return m_player != nullptr ? m_player->duration() : 0.0;
}

double LVideoWindow::position() const
{
    return m_player != nullptr ? m_player->position() : 0.0;
}

std::shared_ptr<LVideoPlayer> LVideoWindow::player() const
{
    return m_player;
}

void LVideoWindow::setAspectRatioMode(LVideoAspectRatioMode mode)
{
    // 渲染线程每帧都会重绘，改完标志即可生效。
    m_aspectMode = mode;
}

LVideoAspectRatioMode LVideoWindow::aspectRatioMode() const
{
    return m_aspectMode;
}

void LVideoWindow::setBackgroundColor(const LColor &color)
{
    m_background = color;
}

LColor LVideoWindow::backgroundColor() const
{
    return m_background;
}

void LVideoWindow::setFrameRate(int fps)
{
    m_frameRate = std::clamp(fps, 1, 240);
    restartTimer();
}

int LVideoWindow::frameRate() const
{
    return m_frameRate;
}

void LVideoWindow::setAudioDucking(bool enabled, double gain)
{
    ensurePlayer();
    m_player->setAudioDucking(enabled, gain);
}

void LVideoWindow::restartTimer()
{
    // 取帧/呈现已迁移到独立渲染线程，这里只需同步帧率并唤醒它。
    m_renderFrameRate.store(std::max(1, m_frameRate), std::memory_order_relaxed);
    m_waitCv.notify_all();
}

bool LVideoWindow::ensureVideoResources(const LVideoFrame &frame)
{
#ifdef _WIN32
    if (!ensureRhi()) {
        return false;
    }

    if (frame.texture != nullptr) {
        m_videoTexture = frame.texture;
    } else {
        // MF 与 RHI 共用同一个 device：nativeTexture 就是本 device 上的
        // ID3D11Texture2D，直接 createFrom 包装即可。只有跨 device 时才
        // 需要走共享句柄 + OpenSharedResource。
        if (frame.nativeTexture == nullptr && frame.sharedTextureHandle == nullptr) {
            static int noTexLog = 0;
            if ((noTexLog++ % 60) == 0) {
                lWarn() << "lkmedia/RHI: frame has neither native nor shared texture";
            }
            return false;
        }
        void *key = frame.nativeTexture != nullptr ? frame.nativeTexture
                                                   : frame.sharedTextureHandle;
        int slot = -1;
        for (int i = 0; i < 2; ++i) {
            if (m_videoTextureKeys[i] == key) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            ID3D11Texture2D *srcTexture = nullptr;
            bool ownsSrcRef = false;
            if (frame.nativeTexture != nullptr) {
                srcTexture = static_cast<ID3D11Texture2D *>(frame.nativeTexture);
            } else {
                auto native = std::dynamic_pointer_cast<LRhiD3D11NativeHandles>(
                            m_rhi->nativeHandles());
                if (native == nullptr || native->dev == nullptr) {
                    lWarn() << "lkmedia/RHI: no D3D11 native device for import";
                    return false;
                }
                ID3D11Device *device = static_cast<ID3D11Device *>(native->dev);
                const HRESULT openHr = device->OpenSharedResource(
                            reinterpret_cast<HANDLE>(frame.sharedTextureHandle),
                            __uuidof(ID3D11Texture2D),
                            reinterpret_cast<void **>(&srcTexture));
                if (FAILED(openHr)) {
                    static int openFailLog = 0;
                    if ((openFailLog++ % 20) == 0) {
                        lWarn() << "lkmedia/RHI: OpenSharedResource failed, hr ="
                                << int(openHr) << "handle =" << frame.sharedTextureHandle;
                    }
                    return false;
                }
                ownsSrcRef = true;
            }
            auto tex = m_rhi->newTexture(LRhiTexture::BGRA8,
                                         LSize<int>(int(frame.width), int(frame.height)));
            LRhiTexture::NativeTexture src{};
            src.object = reinterpret_cast<uint64_t>(srcTexture);
            const bool created = tex != nullptr && tex->createFrom(src);
            if (ownsSrcRef) {
                srcTexture->Release();
            }
            if (!created) {
                lWarn() << "lkmedia/RHI: createFrom(video texture) failed";
                return false;
            }
            slot = 0;
            for (int i = 0; i < 2; ++i) {
                if (m_videoTextureKeys[i] == nullptr) {
                    slot = i;
                    break;
                }
            }
            m_videoTextureKeys[slot] = key;
            m_videoTextures[slot] = tex;
        }
        m_videoTexture = m_videoTextures[slot];
    }
    m_videoGeneration = frame.generation;

    if (m_sampler == nullptr) {
        m_sampler = m_rhi->newSampler(LRhiSampler::Linear, LRhiSampler::Linear,
                                      LRhiSampler::Linear, LRhiSampler::ClampToEdge,
                                      LRhiSampler::ClampToEdge, LRhiSampler::ClampToEdge);
        if (m_sampler == nullptr || !m_sampler->create()) {
            lWarn() << "lkmedia/RHI: create video sampler failed";
            return false;
        }
    }

    if (m_srb == nullptr) {
        m_srb = m_rhi->newShaderResourceBindings();
        if (m_srb == nullptr) {
            lWarn() << "lkmedia/RHI: newShaderResourceBindings failed";
            return false;
        }
        m_srb->m_bindings.push_back(LRhiShaderResourceBinding::sampledTexture(
                    0, LRhiShaderResourceBindingStage::Fragment, m_videoTexture, m_sampler));
        if (!m_srb->create()) {
            lWarn() << "lkmedia/RHI: create video srb failed";
            return false;
        }

        const LShader vs = lCompileVideoShader(
            "struct VSIn { float2 pos : POSITION; float2 uv : TEXCOORD0; };\n"
            "struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };\n"
            "VSOut main(VSIn i) { VSOut o; o.pos = float4(i.pos, 0.0f, 1.0f); o.uv = i.uv; return o; }\n",
            "vs_5_0", LShader::VertexStage);
        const LShader fs = lCompileVideoShader(
            "Texture2D<float4> tex : register(t0);\n"
            "SamplerState samp : register(s0);\n"
            "float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target\n"
            "{ return tex.Sample(samp, uv); }\n",
            "ps_5_0", LShader::FragmentStage);

        m_pipeline = m_rhi->newGraphicsPipeline();
        m_pipeline->m_shaderStages.push_back(
                    LRhiShaderStage(LRhiShaderStage::Vertex, vs));
        m_pipeline->m_shaderStages.push_back(
                    LRhiShaderStage(LRhiShaderStage::Fragment, fs));
        LRhiVertexInputBinding vb;
        vb.m_stride = 4 * sizeof(float);
        LRhiVertexInputAttribute posAttr;
        posAttr.m_format = LRhiVertexInputAttribute::Float2;
        posAttr.m_offset = 0;
        posAttr.m_name = "POSITION";
        LRhiVertexInputAttribute uvAttr;
        uvAttr.m_location = 1;
        uvAttr.m_format = LRhiVertexInputAttribute::Float2;
        uvAttr.m_offset = 2 * sizeof(float);
        uvAttr.m_name = "TEXCOORD";
        m_pipeline->m_vertexInputLayout.m_bindings.push_back(vb);
        m_pipeline->m_vertexInputLayout.m_attributes.push_back(posAttr);
        m_pipeline->m_vertexInputLayout.m_attributes.push_back(uvAttr);
        m_pipeline->m_renderPassDesc = m_swapChain->newCompatibleRenderPassDescriptor();
        m_pipeline->m_shaderResourceBindings = m_srb;
        if (!m_pipeline->create()) {
            lWarn() << "lkmedia/RHI: create video pipeline failed";
            return false;
        }

        struct Vertex { float x, y, u, v; };
        const Vertex vertices[3] = {
            { -1.0f, -1.0f, 0.0f, 1.0f },
            {  3.0f, -1.0f, 2.0f, 1.0f },
            { -1.0f,  3.0f, 0.0f, -1.0f }
        };
        const uint16_t indices[3] = { 0, 1, 2 };
        // 顶点/索引数据通过 Map + memcpy 写入，必须用 Dynamic 缓冲：
        // Static 缓冲是 D3D11_USAGE_DEFAULT，Map(WRITE_DISCARD) 会失败，
        // 数据写不进去导致三角形退化为点、画面全黑。
        m_vertexBuffer = m_rhi->newBuffer(LRhiBuffer::Dynamic, LRhiBufferUsage::Vertex,
                                          sizeof(vertices));
        m_indexBuffer = m_rhi->newBuffer(LRhiBuffer::Dynamic, LRhiBufferUsage::Index,
                                         sizeof(indices));
        if (!m_vertexBuffer->create() || !m_indexBuffer->create()) {
            return false;
        }
        if (char *dst = m_vertexBuffer->beginFullDynamicBufferUpdateForCurrentFrame()) {
            std::memcpy(dst, vertices, sizeof(vertices));
            m_vertexBuffer->endFullDynamicBufferUpdateForCurrentFrame();
        }
        if (char *dst = m_indexBuffer->beginFullDynamicBufferUpdateForCurrentFrame()) {
            std::memcpy(dst, indices, sizeof(indices));
            m_indexBuffer->endFullDynamicBufferUpdateForCurrentFrame();
        }
    } else {
        m_srb->setSampledTexture(0, m_videoTexture, m_sampler);
        m_srb->updateResources();
    }

    return m_videoTexture != nullptr && m_pipeline != nullptr;
#else
    (void)frame;
    return false;
#endif
}

void LVideoWindow::renderFrame()
{
#ifdef _WIN32
    // 只在渲染线程调用：窗口尺寸来自 GUI 线程维护的原子量。
    const int surfaceWidth = m_renderWidth.load(std::memory_order_relaxed);
    const int surfaceHeight = m_renderHeight.load(std::memory_order_relaxed);
    if (surfaceWidth <= 0 || surfaceHeight <= 0) {
        return;
    }
    if (!m_rhiReady || m_rhi == nullptr || m_swapChain == nullptr) {
        return;
    }

    const LRhi::FrameOpResult beginResult = m_rhi->beginFrame(m_swapChain);
    if (beginResult != LRhi::FrameOpSuccess) {
        lWarn() << "lkmedia/RHI: beginFrame failed =" << int(beginResult);
        return;
    }

    std::shared_ptr<LRhiCommandBuffer> cb = m_swapChain->currentFrameCommandBuffer();
    cb->beginPass(m_swapChain->currentFrameRenderTarget(), m_background,
                  LRhiDepthStencilClearValue());

    if (m_latestFrame.valid && ensureVideoResources(m_latestFrame)) {
        LRhiViewport viewport;
        viewport.m_rect = fittedRect();
        viewport.m_minDepth = 0.0;
        viewport.m_maxDepth = 1.0;
        cb->setViewport(viewport);

        const LSize<int> pixelSize = m_swapChain->currentPixelSize();
        cb->setScissor(LRect<int>(0, 0, pixelSize.m_x, pixelSize.m_y, false));
        cb->setGraphicsPipeline(m_pipeline);
        cb->setShaderResources(m_srb);
        LRhiCommandBuffer::VertexInput bindings[] = { { m_vertexBuffer, 0 } };
        cb->setVertexInput(0, 1, bindings, m_indexBuffer, 0, LRhiCommandBuffer::IndexUInt16);
        cb->drawIndexed(3);
    }

    cb->endPass();
    if (m_rhi->endFrame(m_swapChain) != LRhi::FrameOpSuccess) {
        lWarn() << "lkmedia/RHI: endFrame failed";
    }
#endif
}

LRect<double> LVideoWindow::fittedRect() const
{
    LRect<double> r(0.0, 0.0,
                    double(m_renderWidth.load(std::memory_order_relaxed)),
                    double(m_renderHeight.load(std::memory_order_relaxed)));
    if (m_aspectMode == LVideoAspectRatioMode::Ignore ||
        m_frameWidth == 0 || m_frameHeight == 0 ||
        r.m_width <= 0.0 || r.m_height <= 0.0) {
        return r;
    }

    const double srcAspect = double(m_frameWidth) / double(m_frameHeight);
    const double dstAspect = r.m_width / r.m_height;
    const bool expand = (m_aspectMode == LVideoAspectRatioMode::KeepExpand);

    double w = r.m_width;
    double h = r.m_height;
    if ((srcAspect > dstAspect) != expand) {
        h = r.m_width / srcAspect;
    } else {
        w = r.m_height * srcAspect;
    }
    return LRect<double>((r.m_width - w) / 2.0,
                         (r.m_height - h) / 2.0,
                         w, h, false);
}

void LVideoWindow::exposeEvent(std::shared_ptr<LExposeEvent> e)
{
    LWindow::exposeEvent(e);
    // 重绘由渲染线程负责，这里只唤醒它。
    m_waitCv.notify_all();
}

void LVideoWindow::resizeEvent(std::shared_ptr<LResizeEvent> e)
{
    LWindow::resizeEvent(e);
    m_renderWidth.store(width(), std::memory_order_relaxed);
    m_renderHeight.store(height(), std::memory_order_relaxed);
    m_waitCv.notify_all();
}

void LVideoWindow::showEvent(std::shared_ptr<LEvent> e)
{
    LWindow::showEvent(e);
    m_renderWidth.store(width(), std::memory_order_relaxed);
    m_renderHeight.store(height(), std::memory_order_relaxed);
    m_renderVisible.store(true, std::memory_order_relaxed);
    ensureRhi();
    restartTimer();
    m_waitCv.notify_all();
}

void LVideoWindow::hideEvent(std::shared_ptr<LEvent> e)
{
    LWindow::hideEvent(e);
    m_renderVisible.store(false, std::memory_order_relaxed);
    m_waitCv.notify_all();
}

void LVideoWindow::startRenderThread()
{
#ifdef _WIN32
    if (m_renderThread.joinable()) {
        return;
    }
    m_renderRunning.store(true, std::memory_order_relaxed);
    m_renderThread = std::thread(&LVideoWindow::renderThreadMain, this);
#endif
}

void LVideoWindow::stopRenderThread()
{
#ifdef _WIN32
    if (!m_renderThread.joinable()) {
        return;
    }
    m_renderRunning.store(false, std::memory_order_relaxed);
    m_waitCv.notify_all();
    m_renderThread.join();
#endif
}

void LVideoWindow::renderThreadMain()
{
#ifdef _WIN32
    // frame-server 取帧（OnVideoStreamTick/TransferVideoFrame）要求线程有消息泵。
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    while (m_renderRunning.load(std::memory_order_relaxed)) {
        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        {
            std::lock_guard<std::mutex> lock(m_engineMutex);
            if (m_renderVisible.load(std::memory_order_relaxed) && m_player != nullptr) {
                const LVideoFrame frame = m_player->acquireFrame();
                if (frame.valid) {
                    m_latestFrame = frame;
                    if (frame.width != m_frameWidth || frame.height != m_frameHeight) {
                        m_frameWidth = frame.width;
                        m_frameHeight = frame.height;
                    }
                }
                // 即使没有新帧也重绘：宽高比/背景色变化需要即时生效。
                renderFrame();
            }
        }

        const int fps = std::max(1, m_renderFrameRate.load(std::memory_order_relaxed));
        const int intervalMs = std::max(1, 1000 / fps);
        std::unique_lock<std::mutex> lock(m_waitMutex);
        m_waitCv.wait_for(lock, std::chrono::milliseconds(intervalMs),
                          [this] { return !m_renderRunning.load(std::memory_order_relaxed); });
    }

    {
        std::lock_guard<std::mutex> lock(m_engineMutex);
        m_latestFrame = LVideoFrame();
        resetVideoResources();
    }

    CoUninitialize();
    lWarn() << "lkmedia/RHI: render thread stopped";
#endif
}

LK_NAMESPACE_END
