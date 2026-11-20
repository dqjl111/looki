#include "lkmedia/lvideobackendnull.h"
#include "lkcore/ldebug.h"

LK_NAMESPACE_BEGIN

bool LVideoBackendNull::initialize(const LVideoBackendDesc &)
{
    return true;
}

void LVideoBackendNull::shutdown() {}

const char *LVideoBackendNull::name() const
{
    return "null";
}

bool LVideoBackendNull::open(const std::string &)
{
    lWarn() << "lkmedia: no video backend compiled in. "
               "Enable looki_BUILD_VIDEO with looki_VIDEO_BACKEND=MF (Windows) "
               "or supply a GStreamer backend on Linux.";
    return false;
}

void LVideoBackendNull::close() {}
bool LVideoBackendNull::play() { return false; }
void LVideoBackendNull::pause() {}
void LVideoBackendNull::stop() {}
bool LVideoBackendNull::seek(double) { return false; }
void LVideoBackendNull::setLoop(bool) {}
void LVideoBackendNull::setVolume(double) {}
void LVideoBackendNull::setMuted(bool) {}

LVideoState LVideoBackendNull::state() const { return LVideoState::Idle; }
double LVideoBackendNull::duration() const { return 0.0; }
double LVideoBackendNull::position() const { return 0.0; }
void LVideoBackendNull::nativeVideoSize(uint32_t &w, uint32_t &h) const
{
    w = 0;
    h = 0;
}

LVideoFrame LVideoBackendNull::acquireFrame()
{
    return LVideoFrame();
}

LK_NAMESPACE_END
