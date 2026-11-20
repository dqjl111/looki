#include "lkmedia/lvideo.h"
#include "lkmedia/lvideoaudiobridge.h"
#include "lkgui/rhi/lrhi.h"
#include "lkcore/lookiconfig.h"
#include "lkcore/ldebug.h"
#include <algorithm>

LK_NAMESPACE_BEGIN

struct LVideoPlayerData
{
    std::shared_ptr<LVideoBackend> backend;
    std::shared_ptr<LRhi> rhi;
    bool initialized = false;
    bool duckingEnabled = false;
    double duckingGain = 0.35;
    bool ducked = false;
};

LVideoPlayer::LVideoPlayer()
    : d(std::make_unique<LVideoPlayerData>())
{
    d->backend = LVideoBackend::create();
}

LVideoPlayer::~LVideoPlayer()
{
    shutdown();
}

bool LVideoPlayer::initialize()
{
    return initialize(LVideoBackendDesc());
}

bool LVideoPlayer::initialize(const LVideoBackendDesc &desc)
{
    if (d->initialized) {
        return true;
    }
    if (d->backend == nullptr) {
        d->backend = LVideoBackend::create();
    }
    if (d->backend == nullptr) {
        return false;
    }

    LVideoBackendDesc backendDesc = desc;
    if (backendDesc.rhi == nullptr) {
        backendDesc.rhi = LRhi::create(LRhi::D3D11);
    }
    if (backendDesc.rhi == nullptr) {
        lWarn() << "lkmedia: failed to create RHI for video backend";
        return false;
    }
    d->rhi = backendDesc.rhi;

    if (!d->backend->initialize(backendDesc)) {
        lWarn() << "lkmedia: failed to initialize backend:" << d->backend->name();
        return false;
    }
    d->initialized = true;
    return true;
}

void LVideoPlayer::shutdown()
{
    applyDucking(false);
    if (d->backend != nullptr) {
        d->backend->shutdown();
        d->backend.reset();
    }
    d->rhi.reset();
    d->initialized = false;
}

bool LVideoPlayer::isInitialized() const
{
    return d->initialized;
}

bool LVideoPlayer::open(const std::string &uri)
{
    if (!initialize()) {
        return false;
    }
    return d->backend->open(uri);
}

void LVideoPlayer::close()
{
    applyDucking(false);
    if (d->initialized) {
        d->backend->close();
    }
}

bool LVideoPlayer::play()
{
    if (!initialize()) {
        return false;
    }
    const bool ok = d->backend->play();
    if (ok) {
        applyDucking(true);
    }
    return ok;
}

void LVideoPlayer::pause()
{
    applyDucking(false);
    if (d->initialized) {
        d->backend->pause();
    }
}

void LVideoPlayer::stop()
{
    applyDucking(false);
    if (d->initialized) {
        d->backend->stop();
    }
}

bool LVideoPlayer::seek(double seconds)
{
    return d->initialized && d->backend->seek(seconds);
}

void LVideoPlayer::setLoop(bool loop)
{
    if (d->initialized) {
        d->backend->setLoop(loop);
    }
}

void LVideoPlayer::setVolume(double volume)
{
    if (d->initialized) {
        d->backend->setVolume(std::clamp(volume, 0.0, 1.0));
    }
}

void LVideoPlayer::setMuted(bool muted)
{
    if (d->initialized) {
        d->backend->setMuted(muted);
    }
}

LVideoState LVideoPlayer::state() const
{
    return d->initialized ? d->backend->state() : LVideoState::Idle;
}

double LVideoPlayer::duration() const
{
    return d->initialized ? d->backend->duration() : 0.0;
}

double LVideoPlayer::position() const
{
    return d->initialized ? d->backend->position() : 0.0;
}

void LVideoPlayer::nativeVideoSize(uint32_t &w, uint32_t &h) const
{
    w = 0;
    h = 0;
    if (d->initialized) {
        d->backend->nativeVideoSize(w, h);
    }
}

LVideoFrame LVideoPlayer::acquireFrame()
{
    if (!d->initialized) {
        return LVideoFrame();
    }
    return d->backend->acquireFrame();
}

const char *LVideoPlayer::backendName() const
{
    return d->backend != nullptr ? d->backend->name() : "none";
}

std::shared_ptr<LRhi> LVideoPlayer::rhi() const
{
    return d->rhi;
}

bool LVideoPlayer::backendAvailable()
{
#if LOOKI_VIDEO_BACKEND_MF || LOOKI_VIDEO_BACKEND_GSTREAMER
    return true;
#else
    return false;
#endif
}

const char *LVideoPlayer::backendId()
{
#if LOOKI_VIDEO_BACKEND_MF
    return "MF";
#elif LOOKI_VIDEO_BACKEND_GSTREAMER
    return "GStreamer";
#else
    return "Null";
#endif
}

void LVideoPlayer::setAudioDucking(bool enabled, double gain)
{
    d->duckingGain = std::clamp(gain, 0.0, 1.0);
    const bool wasEnabled = d->duckingEnabled;
    d->duckingEnabled = enabled;

    if (wasEnabled && !enabled && d->ducked) {
        lVideoSetAudioDucking(false, d->duckingGain);
        d->ducked = false;
    } else if (enabled) {
        d->ducked = false;   // 强制重新应用，gain 变化也要生效
        if (state() == LVideoState::Playing) {
            applyDucking(true);
        }
    }
}

bool LVideoPlayer::audioDucking() const
{
    return d->duckingEnabled;
}

double LVideoPlayer::duckingGain() const
{
    return d->duckingGain;
}

void LVideoPlayer::applyDucking(bool ducked)
{
    if (!d->duckingEnabled || d->ducked == ducked) {
        return;
    }
    lVideoSetAudioDucking(ducked, d->duckingGain);
    d->ducked = ducked;
}

LK_NAMESPACE_END
