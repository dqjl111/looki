#ifndef LVIDEOBACKEND_H
#define LVIDEOBACKEND_H

#include "lkcore/livar.h"
#include <cstdint>
#include <memory>
#include <string>

LK_NAMESPACE_BEGIN

class LRhi;
class LRhiTexture;

enum class LVideoState : uint8_t
{
    Idle = 0,
    Opening,
    Ready,
    Playing,
    Paused,
    Ended,
    Error
};

// 平台无关的初始化描述。视频解码设备由各后端自己创建，
// 上层不再传 D3D11/D2D 设备；渲染统一走 LRhi。
struct LVideoBackendDesc
{
    std::shared_ptr<LRhi> rhi;
    void *nativeWindow = nullptr;
    void *reserved = nullptr;
};

// 一帧的原生描述。纹理生命周期归后端所有，GUI 侧只读。
// generation 在后端重建纹理时递增，GUI 侧据此重建 ID2D1Bitmap1 / 纹理视图。
struct LVideoFrame
{
    /** 播放器已导入到 RHI 的纹理，GUI 直接用它采样 */
    std::shared_ptr<LRhiTexture> texture;
    void *nativeTexture = nullptr;   // Windows: ID3D11Texture2D* (BGRA8)
    void *sharedTextureHandle = nullptr; // D3D11 shared HANDLE, 可导入 D3D12
    void *sharedFenceHandle = nullptr;   // D3D11 shared fence HANDLE，用于跨 API 同步
    uint64_t fenceValue = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint64_t generation = 0;
    int64_t pts100ns = 0;
    bool valid = false;              // false = 这一次轮询没有新帧
};

class LKMEDIADEF LVideoBackend
{
public:
    virtual ~LVideoBackend() = default;

    virtual bool initialize(const LVideoBackendDesc &desc) = 0;
    virtual void shutdown() = 0;
    virtual const char *name() const = 0;

    virtual bool open(const std::string &uri) = 0;
    virtual void close() = 0;
    virtual bool play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual bool seek(double seconds) = 0;
    virtual void setLoop(bool loop) = 0;
    virtual void setVolume(double volume) = 0;   // 0.0 .. 1.0
    virtual void setMuted(bool muted) = 0;

    virtual LVideoState state() const = 0;
    virtual double duration() const = 0;         // 秒；未知返回 0
    virtual double position() const = 0;         // 秒
    virtual void nativeVideoSize(uint32_t &w, uint32_t &h) const = 0;

    // 必须由 GUI 线程周期调用：内部完成「是否有新帧」判定 + 把帧写进原生纹理。
    // 后端回调线程不得触碰 D2D / GUI 对象。
    virtual LVideoFrame acquireFrame() = 0;

    // 工厂：实现在 lvideobackend.cpp，由 CMake 生成的平台宏分流。
    static std::shared_ptr<LVideoBackend> create();
};

LK_NAMESPACE_END

#endif // LVIDEOBACKEND_H
