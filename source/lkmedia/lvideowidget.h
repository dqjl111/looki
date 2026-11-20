#ifndef LVIDEOWIDGET_H
#define LVIDEOWIDGET_H

#include "lkwidgets/core/lwidget.h"
#include "lkmedia/lvideowindow.h"
#include <memory>

LK_NAMESPACE_BEGIN

class LWindowContainer;

// 视频控件：只作为容器，把 LVideoWindow 通过 LWindowContainer 嵌入布局。
// 真正的解码、RHI 绘制和帧定时器都在 LVideoWindow 内部。
class LKMEDIADEF LVideoWidget : public LWidget
{
public:
    _LIVarTagEx(LVideoWidget, LWidget)

    LVideoWidget(std::shared_ptr<LWidget> parent = nullptr, LFlags<LWindowType> f = 0);
    virtual ~LVideoWidget();

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
    std::shared_ptr<LVideoWindow> videoWindow() const;

    void setAspectRatioMode(LVideoAspectRatioMode mode);
    LVideoAspectRatioMode aspectRatioMode() const;
    void setBackgroundColor(const LColor &color);
    LColor backgroundColor() const;

    void setFrameRate(int fps);
    int frameRate() const;

    void setAudioDucking(bool enabled, double gain = 0.35);

    virtual LSize<int> sizeHint() const override;

protected:
    virtual void paintEvent(std::shared_ptr<LPaintEvent> e) override;
    virtual void resizeEvent(std::shared_ptr<LResizeEvent> e) override;
    virtual void showEvent(std::shared_ptr<LEvent> e) override;
    virtual void hideEvent(std::shared_ptr<LEvent> e) override;

private:
    void ensureVideoWindow();
    void syncContainerGeometry();

    std::shared_ptr<LVideoWindow> m_videoWindow;
    std::shared_ptr<LWindowContainer> m_container;
    LVideoAspectRatioMode m_pendingAspect = LVideoAspectRatioMode::Keep;
    LColor m_pendingBackground = LColor(0, 0, 0, 255);
    int m_pendingFrameRate = 30;
};

LK_NAMESPACE_END

#endif // LVIDEOWIDGET_H
