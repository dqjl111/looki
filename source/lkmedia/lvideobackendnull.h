#ifndef LVIDEOBACKENDNULL_H
#define LVIDEOBACKENDNULL_H

#include "lkmedia/lvideobackend.h"

LK_NAMESPACE_BEGIN

// 无可用平台后端时的占位实现：保证任何平台都能编译、运行，
// 但所有播放操作返回失败。启用 GStreamer / MF 后不会被 create() 选中。
class LKMEDIADEF LVideoBackendNull : public LVideoBackend
{
public:
    LVideoBackendNull() = default;
    virtual ~LVideoBackendNull() = default;

    virtual bool initialize(const LVideoBackendDesc &desc) override;
    virtual void shutdown() override;
    virtual const char *name() const override;

    virtual bool open(const std::string &uri) override;
    virtual void close() override;
    virtual bool play() override;
    virtual void pause() override;
    virtual void stop() override;
    virtual bool seek(double seconds) override;
    virtual void setLoop(bool loop) override;
    virtual void setVolume(double volume) override;
    virtual void setMuted(bool muted) override;

    virtual LVideoState state() const override;
    virtual double duration() const override;
    virtual double position() const override;
    virtual void nativeVideoSize(uint32_t &w, uint32_t &h) const override;
    virtual LVideoFrame acquireFrame() override;
};

LK_NAMESPACE_END

#endif // LVIDEOBACKENDNULL_H
