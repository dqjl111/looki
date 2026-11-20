#ifndef LVIDEOWINDOW_H
#define LVIDEOWINDOW_H

#include "lkgui/lwindow.h"
#include "lkgui/paint/lcolor.h"
#include "lkgui/rhi/lrhi.h"
#include "lkcore/time/ltimer.h"
#include "lkmedia/lvideo.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

LK_NAMESPACE_BEGIN

enum class LVideoAspectRatioMode : uint8_t
{
    Ignore,      // 拉伸填满
    Keep,        // 保持比例，留黑边
    KeepExpand   // 保持比例，裁切填满
};

// 独立的视频渲染窗口。内部持有播放器、RHI swapchain 和帧定时器，
// 由 LVideoWidget 通过 LWindowContainer 嵌入到普通控件布局中。
class LKMEDIADEF LVideoWindow : public LWindow
{
public:
    LVideoWindow();
    virtual ~LVideoWindow();

    bool open(const std::string &uri);
    void close();
    void play();
    void pause();
    void stop();
    bool seek(double seconds);
    void setLoop(bool loop);
    void setVolume(double volume);
    void setMuted(bool muted);

    LVideoState state() const;
    double duration() const;
    double position() const;
    std::shared_ptr<LVideoPlayer> player() const;

    void setAspectRatioMode(LVideoAspectRatioMode mode);
    LVideoAspectRatioMode aspectRatioMode() const;
    void setBackgroundColor(const LColor &color);
    LColor backgroundColor() const;
    void setFrameRate(int fps);
    int frameRate() const;
    void setAudioDucking(bool enabled, double gain = 0.35);

protected:
    virtual void exposeEvent(std::shared_ptr<LExposeEvent> e) override;
    virtual void resizeEvent(std::shared_ptr<LResizeEvent> e) override;
    virtual void showEvent(std::shared_ptr<LEvent> e) override;
    virtual void hideEvent(std::shared_ptr<LEvent> e) override;

private:
    void ensurePlayer();
    bool ensureRhi();
    bool ensureVideoResources(const LVideoFrame &frame);
    /** 换源/关闭时清空所有跟某一帧纹理绑定的缓存，避免复用失效的共享纹理 */
    void resetVideoResources();
    void renderFrame();
    void restartTimer();
    LRect<double> fittedRect() const;

    /** 独立渲染线程：自带消息泵，负责取帧 + Present，GUI 线程被模态对话框
     *  阻塞时画面仍能继续刷新。 */
    void startRenderThread();
    void stopRenderThread();
    void renderThreadMain();

    std::shared_ptr<LVideoPlayer> m_player;
    std::shared_ptr<LTimer> m_timer;
    LVideoFrame m_latestFrame;
    uint32_t m_frameWidth = 0;
    uint32_t m_frameHeight = 0;

    std::shared_ptr<LRhi> m_rhi;
    std::shared_ptr<LRhiSwapChain> m_swapChain;
    std::shared_ptr<LRhiGraphicsPipeline> m_pipeline;
    std::shared_ptr<LRhiBuffer> m_vertexBuffer;
    std::shared_ptr<LRhiBuffer> m_indexBuffer;
    std::shared_ptr<LRhiSampler> m_sampler;
    std::shared_ptr<LRhiTexture> m_videoTexture;
    void *m_videoTextureKeys[2] = { nullptr, nullptr };
    std::shared_ptr<LRhiTexture> m_videoTextures[2];
    std::shared_ptr<LRhiShaderResourceBindings> m_srb;
    uint64_t m_videoGeneration = 0;
    uint64_t m_videoFenceValue = 0;
    bool m_rhiReady = false;

    LVideoAspectRatioMode m_aspectMode = LVideoAspectRatioMode::Keep;
    LColor m_background = LColor(0, 0, 0, 255);
    int m_frameRate = 30;

    // 渲染线程及跨线程共享状态。GUI 线程只写可见性/尺寸/fps，
    // MediaEngine 与 RHI 视频资源的访问由 m_engineMutex 串行化。
    std::thread m_renderThread;
    std::atomic<bool> m_renderRunning{false};
    std::atomic<bool> m_renderVisible{false};
    std::atomic<int> m_renderWidth{0};
    std::atomic<int> m_renderHeight{0};
    std::atomic<int> m_renderFrameRate{30};
    std::mutex m_engineMutex;
    std::mutex m_waitMutex;
    std::condition_variable m_waitCv;
};

LK_NAMESPACE_END

#endif // LVIDEOWINDOW_H
