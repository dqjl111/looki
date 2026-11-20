#ifndef LVIDEO_H
#define LVIDEO_H

#include "lkmedia/lvideobackend.h"
#include <memory>

LK_NAMESPACE_BEGIN

struct LVideoPlayerData;

// 平台无关的播放器门面。所有平台相关状态都在 LVideoPlayerData / 后端里，
// 公共 ABI 不含任何 Windows 或 GUI 原生类型。
class LKMEDIADEF LVideoPlayer
{
public:
    LVideoPlayer();
    virtual ~LVideoPlayer();

    LVideoPlayer(const LVideoPlayer &) = delete;
    LVideoPlayer &operator=(const LVideoPlayer &) = delete;

    // 无参版本使用平台适配层提供的默认图形设备（Windows 上是 GUI 的 D3D11 device）。
    bool initialize();
    bool initialize(const LVideoBackendDesc &desc);
    void shutdown();
    bool isInitialized() const;

    bool open(const std::string &uri);
    void close();
    bool play();
    void pause();
    void stop();
    bool seek(double seconds);
    void setLoop(bool loop);
    void setVolume(double volume);
    void setMuted(bool muted);

    LVideoState state() const;
    double duration() const;
    double position() const;
    void nativeVideoSize(uint32_t &w, uint32_t &h) const;

    // GUI 线程调用；frame.valid == false 表示这一拍没有新帧。
    LVideoFrame acquireFrame();

    const char *backendName() const;
    std::shared_ptr<LRhi> rhi() const;
    static bool backendAvailable();   // Null 后端返回 false
    static const char *backendId();   // "MF" / "GStreamer" / "Null"

    // 方案 A：视频播放时把 lkmedia 的主音量压低，停止后恢复。
    // 视频音轨仍由后端自己的音频渲染器输出，两路在系统混音器汇合。
    void setAudioDucking(bool enabled, double gain = 0.35);
    bool audioDucking() const;
    double duckingGain() const;

private:
    void applyDucking(bool ducked);
    std::unique_ptr<LVideoPlayerData> d;
};

LK_NAMESPACE_END

#endif // LVIDEO_H
