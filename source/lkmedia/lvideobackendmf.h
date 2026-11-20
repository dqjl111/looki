#ifndef LVIDEOBACKENDMF_H
#define LVIDEOBACKENDMF_H

#include "lkmedia/lvideobackend.h"
#include <cstdint>
#include <memory>

LK_NAMESPACE_BEGIN

// Windows Media Foundation (IMFMediaEngine) 后端。
//
// 线程模型（参考 Qt6 的 QWindowsMediaEngine）：MediaEngine 以及它使用的
// D3D11 device / immediate context 全部属于 GUI 线程，并且也只在 GUI 线程
// 上调用。GUI 的 LEventDispatcherWin 提供了真正的 Win32 消息泵，
// OleInitialize 把 GUI 线程初始化为 STA——这正是 IMFMediaEngine 要求的宿主
// 环境。帧的拉取（OnVideoStreamTick + TransferVideoFrame + CopyResource）
// 在 acquireFrame() 内同步完成，由 GUI 的定时器驱动。
//
// 因此这里没有任何解码线程/取帧线程/命令队列：跨套间直接调用 COM 指针、
// 跨线程使用 D3D11 immediate context 都会导致无声无画、控制无响应和重开卡死。
//
// 本头文件不含任何 Windows 类型：所有 COM / D3D 对象都在 .cpp 的 Data 里。
class LKMEDIADEF LVideoBackendMF : public LVideoBackend
{
public:
    LVideoBackendMF();
    virtual ~LVideoBackendMF() override;

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

    // 仅供 IMFMediaEngineNotify 回调调用；只写原子标志，绝不触碰 GUI 对象。
    void onEngineEvent(uint32_t event);

private:
    /** （重新）创建 MediaEngine；复用当前的 dxgiManager */
    bool createEngine();
    /** 释放 MediaEngine 并丢弃上一路视频的纹理 */
    void destroyEngine();

    struct Data;
    std::unique_ptr<Data> d;
};

LK_NAMESPACE_END

#endif // LVIDEOBACKENDMF_H
