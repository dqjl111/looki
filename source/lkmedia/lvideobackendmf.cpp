#include "lkmedia/lvideobackendmf.h"
#include "lkcore/lookiconfig.h"

#if !LOOKI_VIDEO_BACKEND_MF
#  error "lvideobackendmf.cpp must only be compiled when LOOKI_VIDEO_BACKEND_MF=1"
#endif

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objbase.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mfmediaengine.h>
#include <d3d10_1.h>
#include <d3d11.h>
#include <dxgi.h>
#include <wrl.h>

#include "lkcore/ldebug.h"
#include "lkgui/rhi/lrhi.h"
#include "lkgui/rhi/lrhiplatform.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <filesystem>
#include <string>

namespace
{
// Media Foundation 是进程级的：静态对象保证 MFStartup 只执行一次，
// 并在进程退出（静态析构）时配对调用 MFShutdown，避免平台库不清理。
struct LMFPlatformGuard
{
    bool ok = false;
    LMFPlatformGuard()
    {
        ok = SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_FULL));
    }
    ~LMFPlatformGuard()
    {
        if (ok) {
            MFShutdown();
        }
    }
};

// 回调线程与 GUI 线程共享的状态。只存原子量，回调里绝不触碰 D2D / GUI 对象。
struct LMFSharedState
{
    std::atomic<uint8_t> state{uint8_t(_LK::LVideoState::Idle)};
    std::atomic<bool> canPlay{false};
    std::atomic<bool> ended{false};
    std::atomic<bool> failed{false};
    // 格式/分辨率变化时才需要重新查询原生尺寸
    std::atomic<bool> formatChanged{false};
};

// 回调线程只写「事件标志位」，绝不写 state。
// state 由 GUI 线程的 open/play/pause/stop 独占管理，state() 里再根据这些
// 标志推导出 Ready/Ended/Error。原因：SetSource 会异步发 ABORT，若回调把
// state 改成 Idle，紧随其后的 CANPLAY 就无法把 Opening 推进到 Ready，
// 上层 autoPlay 永远等不到 Ready → 从不调用 play() → 进度不动、无画面。
void lApplyEngineEvent(uint32_t event, const std::shared_ptr<LMFSharedState> &s)
{
    switch (event) {
    case MF_MEDIA_ENGINE_EVENT_CANPLAY:
    case MF_MEDIA_ENGINE_EVENT_CANPLAYTHROUGH:
        s->canPlay.store(true, std::memory_order_relaxed);
        break;
    case MF_MEDIA_ENGINE_EVENT_PLAYING:
        s->ended.store(false, std::memory_order_relaxed);
        break;
    case MF_MEDIA_ENGINE_EVENT_ENDED:
        s->ended.store(true, std::memory_order_relaxed);
        break;
    case MF_MEDIA_ENGINE_EVENT_FORMATCHANGE:
        s->formatChanged.store(true, std::memory_order_relaxed);
        break;
    case MF_MEDIA_ENGINE_EVENT_ERROR:
        s->failed.store(true, std::memory_order_relaxed);
        break;
    default:
        // ABORT / PAUSE / PLAY 等不再触碰 state，避免与 GUI 线程竞争。
        break;
    }
}

class LMFNotify : public IMFMediaEngineNotify
{
public:
    explicit LMFNotify(std::shared_ptr<LMFSharedState> state)
        : m_state(std::move(state))
    {}

    STDMETHODIMP QueryInterface(REFIID riid, void **ppv) override
    {
        if (ppv == nullptr) {
            return E_POINTER;
        }
        if (riid == IID_IUnknown || riid == __uuidof(IMFMediaEngineNotify)) {
            *ppv = static_cast<IMFMediaEngineNotify *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override
    {
        return m_ref.fetch_add(1, std::memory_order_relaxed) + 1;
    }

    STDMETHODIMP_(ULONG) Release() override
    {
        const ULONG ref = m_ref.fetch_sub(1, std::memory_order_acq_rel) - 1;
        if (ref == 0) {
            delete this;
        }
        return ref;
    }

    STDMETHODIMP EventNotify(DWORD event, DWORD_PTR, DWORD) override
    {
        lApplyEngineEvent(uint32_t(event), m_state);
        return S_OK;
    }

private:
    std::atomic<ULONG> m_ref{1};
    std::shared_ptr<LMFSharedState> m_state;
};

std::wstring lToMediaUrl(const std::string &uri)
{
    const int len = MultiByteToWideChar(CP_UTF8, 0, uri.c_str(), -1, nullptr, 0);
    std::wstring wide(len > 1 ? size_t(len - 1) : 0, L'\0');
    if (len > 1) {
        MultiByteToWideChar(CP_UTF8, 0, uri.c_str(), -1, wide.data(), len);
    }

    if (wide.find(L"://") != std::wstring::npos) {
        return wide;
    }

    std::error_code ec;
    std::filesystem::path normalized = std::filesystem::path(wide).lexically_normal();
    std::filesystem::path absolutePath = std::filesystem::absolute(normalized, ec);
    std::wstring result = absolutePath.wstring();
    std::replace(result.begin(), result.end(), L'\\', L'/');
    return L"file:///" + result;
}
} // namespace

LK_NAMESPACE_BEGIN

struct LVideoFrameTexture
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    HANDLE sharedHandle = nullptr;
};

struct LVideoBackendMF::Data
{
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> deviceContext;
    Microsoft::WRL::ComPtr<IMFDXGIDeviceManager> dxgiManager;
    Microsoft::WRL::ComPtr<IMFMediaEngine> engine;
    Microsoft::WRL::ComPtr<LMFNotify> notify;
    // 双缓冲：frameTextures 是 TransferVideoFrame 的纯 render target；
    // readTextures 是 CopyResource 后的 shader resource，直接交给 RHI 采样。
    LVideoFrameTexture frameTextures[2];
    LVideoFrameTexture readTextures[2];
    std::shared_ptr<LRhi> rhi;
    int writeIndex = 0;

    std::shared_ptr<LMFSharedState> shared = std::make_shared<LMFSharedState>();

    // GUI 线程只读这些缓存，不直接调用 MediaEngine（engine 由渲染线程驱动）。
    std::atomic<double> cachedDuration{0.0};
    std::atomic<double> cachedPosition{0.0};
    std::atomic<uint32_t> cachedWidth{0};
    std::atomic<uint32_t> cachedHeight{0};

    UINT resetToken = 0;
    uint32_t textureWidth = 0;
    uint32_t textureHeight = 0;
    uint64_t generation = 0;

    double volume = 1.0;
    bool muted = false;
    // GUI 线程写、渲染线程读，必须是原子量。
    std::atomic<bool> loop{false};
    bool initialized = false;
    // true 仅当本后端自己成功 CoInitializeEx（返回 S_OK）。
    // GUI 线程已由 LWindowsCtx::OleInitialize 初始化为 STA，此时返回 S_FALSE，
    // 我们不能配对 CoUninitialize。
    bool ownCom = false;
    bool hasSource = false;
    uint32_t transferFailures = 0;
};

LVideoBackendMF::LVideoBackendMF()
    : d(std::make_unique<Data>())
{
}

LVideoBackendMF::~LVideoBackendMF()
{
    shutdown();
}

const char *LVideoBackendMF::name() const
{
    return "MediaFoundation";
}

void LVideoBackendMF::onEngineEvent(uint32_t event)
{
    lApplyEngineEvent(event, d->shared);
}

bool LVideoBackendMF::initialize(const LVideoBackendDesc &desc)
{
    using Microsoft::WRL::ComPtr;

    d->rhi = desc.rhi;
    if (d->initialized) {
        return true;
    }

    // 必须用 MFSTARTUP_FULL：LITE 不加载 MFT，MediaEngine 无法创建解码器，
    // 表现为 GetDuration=nan / GetNativeVideoSize=E_FAIL / 永远取不到视频帧。
    static LMFPlatformGuard mfGuard;
    if (!mfGuard.ok) {
        lWarn() << "lkmedia/MF: MFStartup failed";
        return false;
    }

    // GUI 线程通常已由 LWindowsCtx::OleInitialize 初始化为 STA 并有消息泵。
    // 若尚未初始化，我们补一次 STA；S_FALSE 表示别人已初始化，不能由我们释放。
    const HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    d->ownCom = (comHr == S_OK);

    // 本环境实测：MF 只在 RHI 创建的 device 上能正常解码。独立 device
    // （无论是否带 VIDEO_SUPPORT）都会出现 GetDuration=nan、
    // GetNativeVideoSize=E_FAIL、OnVideoStreamTick 恒 S_FALSE。
    // 因此继续复用 RHI 的 device，但用 SetMultithreadProtected 让 D3D11
    // 对 device/context 调用加锁，降低解码线程与 GUI 线程的并发风险；
    // 帧回传只走同一 device 内的纹理副本，不再使用共享句柄。
    if (d->rhi != nullptr) {
        auto native = std::dynamic_pointer_cast<LRhiD3D11NativeHandles>(d->rhi->nativeHandles());
        if (native != nullptr && native->dev != nullptr && native->context != nullptr) {
            ID3D11Device *sharedDevice = static_cast<ID3D11Device *>(native->dev);
            ID3D11DeviceContext *sharedContext = static_cast<ID3D11DeviceContext *>(native->context);
            sharedDevice->AddRef();
            sharedContext->AddRef();
            d->device.Attach(sharedDevice);
            d->deviceContext.Attach(sharedContext);
            lWarn() << "lkmedia/MF: using RHI's shared D3D11 device";
        }
    }

    if (d->device == nullptr || d->deviceContext == nullptr) {
        static const D3D_FEATURE_LEVEL levels[] = {
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0
        };
        UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT;
        D3D_FEATURE_LEVEL createdLevel = D3D_FEATURE_LEVEL_11_0;
        const HRESULT deviceHr = D3D11CreateDevice(
                    nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags,
                    levels, UINT(sizeof(levels) / sizeof(levels[0])), D3D11_SDK_VERSION,
                    d->device.GetAddressOf(), &createdLevel, d->deviceContext.GetAddressOf());
        if (FAILED(deviceHr)) {
            lWarn() << "lkmedia/MF: D3D11CreateDevice failed, hr =" << int(deviceHr);
            return false;
        }
        lWarn() << "lkmedia/MF: using independent D3D11 device";
    }

    // D3D11 默认应已开启多线程保护，这里显式打开一次，避免驱动/包装层
    // 在默认值上有差异时出现并发破坏。
    {
        Microsoft::WRL::ComPtr<ID3D10Multithread> multithread;
        if (SUCCEEDED(d->device.As(&multithread))) {
            multithread->SetMultithreadProtected(TRUE);
        }
    }

    HRESULT hr = MFCreateDXGIDeviceManager(&d->resetToken, d->dxgiManager.GetAddressOf());
    if (FAILED(hr)) {
        lWarn() << "lkmedia/MF: MFCreateDXGIDeviceManager failed";
        shutdown();
        return false;
    }
    hr = d->dxgiManager->ResetDevice(d->device.Get(), d->resetToken);
    if (FAILED(hr)) {
        lWarn() << "lkmedia/MF: IMFDXGIDeviceManager::ResetDevice failed";
        shutdown();
        return false;
    }

    if (!createEngine()) {
        shutdown();
        return false;
    }

    d->initialized = true;
    return true;
}

bool LVideoBackendMF::createEngine()
{
    using Microsoft::WRL::ComPtr;

    d->notify.Attach(new LMFNotify(d->shared));

    ComPtr<IMFAttributes> attributes;
    HRESULT hr = MFCreateAttributes(attributes.GetAddressOf(), 4);
    if (FAILED(hr)) {
        lWarn() << "lkmedia/MF: MFCreateAttributes failed";
        return false;
    }
    attributes->SetUnknown(MF_MEDIA_ENGINE_CALLBACK, d->notify.Get());
    attributes->SetUnknown(MF_MEDIA_ENGINE_DXGI_MANAGER, d->dxgiManager.Get());
    // 让 MediaEngine 直接输出 BGRA8，省掉一次颜色转换。
    attributes->SetUINT32(MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT,
                          UINT32(DXGI_FORMAT_B8G8R8A8_UNORM));

    ComPtr<IMFMediaEngineClassFactory> factory;
    hr = CoCreateInstance(CLSID_MFMediaEngineClassFactory, nullptr,
                          CLSCTX_INPROC_SERVER, IID_PPV_ARGS(factory.GetAddressOf()));
    if (FAILED(hr)) {
        lWarn() << "lkmedia/MF: CoCreateInstance(MediaEngineClassFactory) failed";
        return false;
    }

    // windowless 模式：由 GUI 线程的 acquireFrame() 自己 TransferVideoFrame。
    hr = factory->CreateInstance(0, attributes.Get(), d->engine.GetAddressOf());
    if (FAILED(hr) || d->engine == nullptr) {
        lWarn() << "lkmedia/MF: CreateInstance(IMFMediaEngine) failed";
        return false;
    }

    d->engine->SetAutoPlay(FALSE);
    d->engine->SetLoop(d->loop.load(std::memory_order_relaxed) ? TRUE : FALSE);
    d->engine->SetMuted(d->muted ? TRUE : FALSE);
    d->engine->SetVolume(d->volume);
    return true;
}

void LVideoBackendMF::destroyEngine()
{
    if (d->engine != nullptr) {
        d->engine->Shutdown();
        d->engine.Reset();
    }
    d->notify.Reset();
    for (LVideoFrameTexture &ft : d->frameTextures) {
        ft.texture.Reset();
        ft.sharedHandle = nullptr;
    }
    for (LVideoFrameTexture &ft : d->readTextures) {
        ft.texture.Reset();
        ft.sharedHandle = nullptr;
    }
    d->writeIndex = 0;
    d->textureWidth = 0;
    d->textureHeight = 0;
    d->hasSource = false;
    d->cachedDuration.store(0.0, std::memory_order_relaxed);
    d->cachedPosition.store(0.0, std::memory_order_relaxed);
    d->cachedWidth.store(0, std::memory_order_relaxed);
    d->cachedHeight.store(0, std::memory_order_relaxed);
    d->shared->state.store(uint8_t(_LK::LVideoState::Idle), std::memory_order_relaxed);
    d->shared->canPlay.store(false, std::memory_order_relaxed);
    d->shared->ended.store(false, std::memory_order_relaxed);
    d->shared->failed.store(false, std::memory_order_relaxed);
}

void LVideoBackendMF::shutdown()
{
    destroyEngine();
    d->dxgiManager.Reset();
    d->deviceContext.Reset();
    d->device.Reset();

    if (d->ownCom) {
        CoUninitialize();
        d->ownCom = false;
    }
    d->initialized = false;
}

bool LVideoBackendMF::open(const std::string &uri)
{
    lWarn() << "lkmedia/MF: open BEGIN" << uri;
    if (!d->initialized) {
        return false;
    }

    // 换源：复用同一个 MediaEngine 再 SetSource 不稳定，直接重建一个干净的 engine。
    destroyEngine();
    if (!createEngine()) {
        d->shared->state.store(uint8_t(_LK::LVideoState::Error), std::memory_order_relaxed);
        return false;
    }

    d->transferFailures = 0;
    d->shared->formatChanged.store(true, std::memory_order_relaxed);
    d->shared->canPlay.store(false, std::memory_order_relaxed);
    d->shared->ended.store(false, std::memory_order_relaxed);
    d->shared->failed.store(false, std::memory_order_relaxed);

    const std::wstring url = lToMediaUrl(uri);
    BSTR source = SysAllocString(url.c_str());
    if (source == nullptr) {
        return false;
    }
    const HRESULT hr = d->engine->SetSource(source);
    SysFreeString(source);
    if (FAILED(hr)) {
        lWarn() << "lkmedia/MF: SetSource failed:" << uri << "hr =" << int(hr);
        d->shared->state.store(uint8_t(_LK::LVideoState::Error), std::memory_order_relaxed);
        return false;
    }

    d->shared->state.store(uint8_t(_LK::LVideoState::Opening), std::memory_order_relaxed);
    // Load 在 GUI 线程（STA + 消息泵）上同步完成。
    d->engine->Load();
    // Load 返回后媒体信息（duration/native size）已可用（日志已证实）。
    // 不依赖异步 CANPLAY 事件推进 Ready——那个事件与换源时的 ABORT 竞争，
    // 曾导致 state 永远停在 Idle、上层 autoPlay 从不触发 play()。
    d->shared->canPlay.store(true, std::memory_order_relaxed);
    d->shared->state.store(uint8_t(_LK::LVideoState::Ready), std::memory_order_relaxed);
    lWarn() << "lkmedia/MF: open DONE state=Ready";
    d->hasSource = true;

    // 缓存媒体信息：GUI 线程只读缓存，不直接调用 MediaEngine。
    const double mediaDuration = d->engine->GetDuration();
    d->cachedDuration.store(
                std::isfinite(mediaDuration) && mediaDuration > 0.0 ? mediaDuration : 0.0,
                std::memory_order_relaxed);
    DWORD cx = 0;
    DWORD cy = 0;
    if (SUCCEEDED(d->engine->GetNativeVideoSize(&cx, &cy))) {
        d->cachedWidth.store(cx, std::memory_order_relaxed);
        d->cachedHeight.store(cy, std::memory_order_relaxed);
    }
    return true;
}

void LVideoBackendMF::close()
{
    destroyEngine();
}

bool LVideoBackendMF::play()
{
    if (!d->initialized || !d->hasSource || d->engine == nullptr) {
        return false;
    }
    d->engine->SetDefaultPlaybackRate(1.0);
    d->engine->SetPlaybackRate(1.0);
    const HRESULT hr = d->engine->Play();
    if (FAILED(hr)) {
        lWarn() << "lkmedia/MF: Play failed, hr =" << int(hr)
                << "state =" << int(state());
        return false;
    }
    d->shared->state.store(uint8_t(_LK::LVideoState::Playing), std::memory_order_relaxed);
    return true;
}

void LVideoBackendMF::pause()
{
    if (d->engine != nullptr) {
        d->engine->Pause();
    }
    d->shared->state.store(uint8_t(_LK::LVideoState::Paused), std::memory_order_relaxed);
}

void LVideoBackendMF::stop()
{
    if (d->engine != nullptr) {
        d->engine->Pause();
        d->engine->SetCurrentTime(0.0);
    }
    d->shared->state.store(uint8_t(_LK::LVideoState::Idle), std::memory_order_relaxed);
}

bool LVideoBackendMF::seek(double seconds)
{
    if (d->engine == nullptr || !d->hasSource) {
        return false;
    }
    const HRESULT hr = d->engine->SetCurrentTime(seconds);
    if (FAILED(hr)) {
        lWarn() << "lkmedia/MF: seek failed, hr =" << int(hr);
    }
    if (SUCCEEDED(hr)) {
        d->cachedPosition.store(seconds > 0.0 ? seconds : 0.0, std::memory_order_relaxed);
    }
    return SUCCEEDED(hr);
}

void LVideoBackendMF::setLoop(bool loop)
{
    d->loop.store(loop, std::memory_order_relaxed);
    if (d->engine != nullptr) {
        d->engine->SetLoop(loop ? TRUE : FALSE);
    }
}

void LVideoBackendMF::setVolume(double volume)
{
    d->volume = std::clamp(volume, 0.0, 1.0);
    if (d->engine != nullptr) {
        d->engine->SetVolume(d->volume);
    }
}

void LVideoBackendMF::setMuted(bool muted)
{
    d->muted = muted;
    if (d->engine != nullptr) {
        d->engine->SetMuted(muted ? TRUE : FALSE);
    }
}

LVideoState LVideoBackendMF::state() const
{
    using _LK::LVideoState;
    const uint8_t raw = d->shared->state.load(std::memory_order_relaxed);
    // 事件标志由 MF 回调线程写；这里把它们折算成对外状态。
    if (d->shared->failed.load(std::memory_order_relaxed)) {
        return LVideoState::Error;
    }
    if (raw == uint8_t(LVideoState::Opening)) {
        // Load() 完成后 MF 会发 CANPLAY → canPlay=true，此时源已就绪。
        if (d->shared->canPlay.load(std::memory_order_relaxed)) {
            return LVideoState::Ready;
        }
        return LVideoState::Opening;
    }
    if (raw == uint8_t(LVideoState::Playing) &&
            d->shared->ended.load(std::memory_order_relaxed)) {
        return LVideoState::Ended;
    }
    return LVideoState(raw);
}

double LVideoBackendMF::duration() const
{
    // 只读缓存：MediaEngine 由渲染线程独占驱动，GUI 线程不能直接调用它。
    return d->cachedDuration.load(std::memory_order_relaxed);
}

double LVideoBackendMF::position() const
{
    return d->cachedPosition.load(std::memory_order_relaxed);
}

void LVideoBackendMF::nativeVideoSize(uint32_t &w, uint32_t &h) const
{
    w = d->cachedWidth.load(std::memory_order_relaxed);
    h = d->cachedHeight.load(std::memory_order_relaxed);
}

LVideoFrame LVideoBackendMF::acquireFrame()
{
    LVideoFrame frame;
    if (!d->initialized || !d->hasSource || d->engine == nullptr) {
        return frame;
    }

    // 循环兜底：部分媒体源即使 SetLoop(TRUE)，播放到结尾仍会发 ENDED
    // 并停住（不走引擎自身的循环）。循环模式下这里主动回绕到 0 继续播。
    if (d->loop.load(std::memory_order_relaxed)
            && d->shared->ended.exchange(false, std::memory_order_relaxed)) {
        lWarn() << "lkmedia/MF: loop -> rewind to 0 and play again";
        d->engine->SetCurrentTime(0.0);
        d->engine->Play();
        d->shared->state.store(uint8_t(_LK::LVideoState::Playing),
                               std::memory_order_relaxed);
        d->cachedPosition.store(0.0, std::memory_order_relaxed);
    }

    if (d->shared->state.load(std::memory_order_relaxed) !=
            uint8_t(_LK::LVideoState::Playing)) {
        return frame;
    }

    LONGLONG pts = 0;
    const HRESULT tickHr = d->engine->OnVideoStreamTick(&pts);
    if (tickHr != S_OK) {
        return frame;
    }

    // 渲染线程顺带刷新媒体信息缓存，GUI 线程只读。
    const double currentTime = d->engine->GetCurrentTime();
    d->cachedPosition.store(
                std::isfinite(currentTime) && currentTime > 0.0 ? currentTime : 0.0,
                std::memory_order_relaxed);
    if (d->cachedDuration.load(std::memory_order_relaxed) <= 0.0) {
        const double dur = d->engine->GetDuration();
        if (std::isfinite(dur) && dur > 0.0) {
            d->cachedDuration.store(dur, std::memory_order_relaxed);
        }
    }

    // 第一次出帧或格式变化时按原生尺寸重建纹理。
    const bool needSize = (d->frameTextures[0].texture == nullptr ||
                           d->frameTextures[1].texture == nullptr) ||
            d->shared->formatChanged.exchange(false, std::memory_order_relaxed);
    if (needSize) {
        DWORD cx = 0;
        DWORD cy = 0;
        const HRESULT sizeHr = d->engine->GetNativeVideoSize(&cx, &cy);
        if (FAILED(sizeHr) || cx == 0 || cy == 0) {
            lWarn() << "lkmedia/MF: GetNativeVideoSize failed, hr =" << int(sizeHr);
            return frame;
        }
        d->cachedWidth.store(cx, std::memory_order_relaxed);
        d->cachedHeight.store(cy, std::memory_order_relaxed);
        if (d->textureWidth != cx || d->textureHeight != cy ||
                d->frameTextures[0].texture == nullptr) {
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = cx;
            desc.Height = cy;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
            desc.Usage = D3D11_USAGE_DEFAULT;

            for (LVideoFrameTexture &ft : d->frameTextures) {
                ft.texture.Reset();
                ft.sharedHandle = nullptr;
            }
            for (LVideoFrameTexture &ft : d->readTextures) {
                ft.texture.Reset();
                ft.sharedHandle = nullptr;
            }

            // TransferVideoFrame 的目标：纯 render target。
            desc.BindFlags = D3D11_BIND_RENDER_TARGET;
            desc.MiscFlags = 0;
            for (LVideoFrameTexture &ft : d->frameTextures) {
                if (FAILED(d->device->CreateTexture2D(
                        &desc, nullptr, ft.texture.ReleaseAndGetAddressOf()))) {
                    lWarn() << "lkmedia/MF: could not create frame texture";
                    return frame;
                }
            }

            // 交给 RHI 的纹理：同一 device 内的普通 shader resource，
            // 不需要跨 device 共享句柄，RHI 直接包装该指针即可采样。
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            desc.MiscFlags = 0;
            for (LVideoFrameTexture &ft : d->readTextures) {
                if (FAILED(d->device->CreateTexture2D(
                        &desc, nullptr, ft.texture.ReleaseAndGetAddressOf()))) {
                    lWarn() << "lkmedia/MF: could not create read texture";
                    return frame;
                }
            }
            d->textureWidth = cx;
            d->textureHeight = cy;
            d->writeIndex = 0;
            ++d->generation;
        }
    }

    const uint32_t cx = d->textureWidth;
    const uint32_t cy = d->textureHeight;
    if (cx == 0 || cy == 0 || d->frameTextures[0].texture == nullptr ||
            d->frameTextures[1].texture == nullptr ||
            d->readTextures[0].texture == nullptr ||
            d->readTextures[1].texture == nullptr) {
        return frame;
    }

    LVideoFrameTexture &targetSlot = d->frameTextures[d->writeIndex];
    ID3D11Texture2D *target = targetSlot.texture.Get();

    Microsoft::WRL::ComPtr<IDXGISurface> surface;
    HRESULT hr = target->QueryInterface(
        __uuidof(IDXGISurface), reinterpret_cast<void **>(surface.GetAddressOf()));
    if (FAILED(hr)) {
        return frame;
    }

    // 传完整的 source normalized rect + border color，而不是 nullptr。
    // 部分 MF 实现不接受 pSrc=nullptr，会返回 E_INVALIDARG。
    MFVideoNormalizedRect srcRect{0.0f, 0.0f, 1.0f, 1.0f};
    MFARGB border{0, 0, 0, 255};
    RECT dst{0, 0, LONG(cx), LONG(cy)};
    hr = d->engine->TransferVideoFrame(surface.Get(), &srcRect, &dst, &border);
    if (FAILED(hr)) {
        if ((d->transferFailures++ % 60) == 0) {
            lWarn() << "lkmedia/MF: TransferVideoFrame failed, hr =" << int(hr);
        }
        return frame;
    }

    // 同一 device 内拷到 shader resource 纹理：RHI 采样 SRV 而不是 render target。
    const int outIndex = d->writeIndex;
    LVideoFrameTexture &readOutSlot = d->readTextures[outIndex];
    d->deviceContext->CopyResource(readOutSlot.texture.Get(), target);
    d->deviceContext->Flush();

    frame.nativeTexture = readOutSlot.texture.Get();
    frame.sharedTextureHandle = nullptr;
    frame.sharedFenceHandle = nullptr;
    frame.fenceValue = 0;
    d->writeIndex = 1 - d->writeIndex;
    frame.width = cx;
    frame.height = cy;
    frame.generation = d->generation;
    frame.pts100ns = int64_t(pts);
    frame.valid = true;
    return frame;
}

LK_NAMESPACE_END
