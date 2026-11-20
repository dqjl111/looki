#include "lkmedia/lvideowidget.h"
#include "lkwidgets/core/lwindowcontainer.h"
#include "lkcore/ldebug.h"

LK_NAMESPACE_BEGIN

LVideoWidget::LVideoWidget(std::shared_ptr<LWidget> parent, LFlags<LWindowType> f)
    : LWidget(parent, f)
{
    setAutoFillBackground(false);
}

void LVideoWidget::syncContainerGeometry()
{
    if (m_container != nullptr) {
        m_container->move(0, 0);
        if (m_container->size() != size()) {
            m_container->resize(size());
        }
    }
}

LVideoWidget::~LVideoWidget()
{
    if (m_container != nullptr) {
        m_container->setHidden(true);
    }
    if (m_videoWindow != nullptr) {
        m_videoWindow->setVisible(false);
        m_videoWindow->close();
    }
}

void LVideoWidget::ensureVideoWindow()
{
    if (m_videoWindow == nullptr) {
        m_videoWindow = makeInit<LVideoWindow>();
        m_videoWindow->setAspectRatioMode(m_pendingAspect);
        m_videoWindow->setBackgroundColor(m_pendingBackground);
        m_videoWindow->setFrameRate(m_pendingFrameRate);
    }
    if (m_container == nullptr) {
        m_container = makeInit<LWindowContainer>(m_videoWindow, getPtrT<LWidget>());
        m_container->move(0, 0);
        m_container->resize(size());
        m_container->show();
    }
}

bool LVideoWidget::open(const std::string &uri)
{
    ensureVideoWindow();
    m_container->show();
    return m_videoWindow->open(uri);
}

void LVideoWidget::close()
{
    if (m_videoWindow != nullptr) {
        m_videoWindow->close();
    }
}

void LVideoWidget::play()
{
    ensureVideoWindow();
    m_videoWindow->play();
}

void LVideoWidget::pause()
{
    if (m_videoWindow != nullptr) {
        m_videoWindow->pause();
    }
}

void LVideoWidget::stop()
{
    if (m_videoWindow != nullptr) {
        m_videoWindow->stop();
    }
}

bool LVideoWidget::seek(double seconds)
{
    return m_videoWindow != nullptr && m_videoWindow->seek(seconds);
}

void LVideoWidget::setLoop(bool loop)
{
    if (m_videoWindow != nullptr) {
        m_videoWindow->setLoop(loop);
    }
}

void LVideoWidget::setVolume(double volume)
{
    if (m_videoWindow != nullptr) {
        m_videoWindow->setVolume(volume);
    }
}

void LVideoWidget::setMuted(bool muted)
{
    if (m_videoWindow != nullptr) {
        m_videoWindow->setMuted(muted);
    }
}

LVideoState LVideoWidget::state() const
{
    return m_videoWindow != nullptr ? m_videoWindow->state() : LVideoState::Idle;
}

double LVideoWidget::duration() const
{
    return m_videoWindow != nullptr ? m_videoWindow->duration() : 0.0;
}

double LVideoWidget::position() const
{
    return m_videoWindow != nullptr ? m_videoWindow->position() : 0.0;
}

std::shared_ptr<LVideoPlayer> LVideoWidget::player() const
{
    return m_videoWindow != nullptr ? m_videoWindow->player() : nullptr;
}

std::shared_ptr<LVideoWindow> LVideoWidget::videoWindow() const
{
    return m_videoWindow;
}

void LVideoWidget::setAspectRatioMode(LVideoAspectRatioMode mode)
{
    m_pendingAspect = mode;
    if (m_videoWindow != nullptr) {
        m_videoWindow->setAspectRatioMode(mode);
    }
}

LVideoAspectRatioMode LVideoWidget::aspectRatioMode() const
{
    return m_videoWindow != nullptr ? m_videoWindow->aspectRatioMode() : m_pendingAspect;
}

void LVideoWidget::setBackgroundColor(const LColor &color)
{
    m_pendingBackground = color;
    if (m_videoWindow != nullptr) {
        m_videoWindow->setBackgroundColor(color);
    }
}

LColor LVideoWidget::backgroundColor() const
{
    return m_videoWindow != nullptr ? m_videoWindow->backgroundColor() : m_pendingBackground;
}

void LVideoWidget::setFrameRate(int fps)
{
    m_pendingFrameRate = fps;
    if (m_videoWindow != nullptr) {
        m_videoWindow->setFrameRate(fps);
    }
}

int LVideoWidget::frameRate() const
{
    return m_videoWindow != nullptr ? m_videoWindow->frameRate() : m_pendingFrameRate;
}

void LVideoWidget::setAudioDucking(bool enabled, double gain)
{
    ensureVideoWindow();
    m_videoWindow->setAudioDucking(enabled, gain);
}

LSize<int> LVideoWidget::sizeHint() const
{
    return LSize<int>(640, 360);
}

void LVideoWidget::paintEvent(std::shared_ptr<LPaintEvent> e)
{
    (void)e;
    ensureVideoWindow();
    syncContainerGeometry();
}

void LVideoWidget::resizeEvent(std::shared_ptr<LResizeEvent> e)
{
    LWidget::resizeEvent(e);
    ensureVideoWindow();
    syncContainerGeometry();
}

void LVideoWidget::showEvent(std::shared_ptr<LEvent> e)
{
    LWidget::showEvent(e);
    ensureVideoWindow();
    m_container->show();
    syncContainerGeometry();
}

void LVideoWidget::hideEvent(std::shared_ptr<LEvent> e)
{
    LWidget::hideEvent(e);
    if (m_videoWindow != nullptr) {
        m_videoWindow->setVisible(false);
    }
    if (m_container != nullptr) {
        m_container->setHidden(true);
    }
}

LK_NAMESPACE_END
