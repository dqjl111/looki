// lkmedia 测试程序：验证 LVideoWidget 的打开/播放/暂停/停止/seek/音量/
// 静音/循环/宽高比，以及帧轮询与状态机。
//
// 构建（Windows / VS2022）：
//   cmake -S . -B build -G "Visual Studio 17 2022" -A x64 ^
//         -Dlooki_CMAKE_ENABLE_TEST=ON -Dlooki_BUILD_VIDEO=ON -Dlooki_VIDEO_BACKEND=MF
//   cmake --build build --target lktestvideo --config Debug
//   build\bin\lktestvideo.exe D:\demo.mp4
//
// 不带视频参数启动则只开窗口，用「浏览」按钮选文件。
// 第三个参数（可选）是自动退出秒数，用于自动化冒烟测试：
//   build\bin\lktestvideo.exe D:\demo.mp4 8

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <cwctype>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <shellapi.h>
#  pragma comment(lib, "Shell32.lib")
#endif

#include "lkcore/ldebug.h"
#include "lkcore/lcoreapplication.h"
#include "lkcore/lookiconfig.h"
#include "lkcore/lsignal.h"
#include "lkcore/time/ltimer.h"
#include "lkwidgets/core/lapplication.h"
#include "lkwidgets/core/lboxlayout.h"
#include "lkwidgets/dialog/lfiledialog.h"
#include "lkwidgets/widget/lbutton.h"
#include "lkwidgets/widget/llabel.h"
#include "lkwidgets/widget/lcombobox.h"
#include "lkwidgets/widget/lmainwindow.h"
#include "lkwidgets/widget/lslider.h"
#include "lkmedia/lvideo.h"
#include "lkmedia/lvideowidget.h"
#include "lkcore/lstringutil.h"
#include "lkgui/lplatformwindow.h"

#if LOOKI_NAMESPACE
using namespace lk;
#endif

namespace {

std::wstring formatSeconds(double seconds)
{
    if (!std::isfinite(seconds) || seconds < 0.0) {
        seconds = 0.0;
    }
    const int total = static_cast<int>(seconds);
    return std::format(L"{:02}:{:02}", total / 60, total % 60);
}

const wchar_t *stateName(LVideoState s)
{
    switch (s) {
    case LVideoState::Idle:    return L"空闲";
    case LVideoState::Opening: return L"打开中";
    case LVideoState::Ready:   return L"就绪";
    case LVideoState::Playing: return L"播放中";
    case LVideoState::Paused:  return L"已暂停";
    case LVideoState::Ended:   return L"已结束";
    case LVideoState::Error:   return L"出错";
    }
    return L"未知";
}

const wchar_t *aspectName(LVideoAspectRatioMode m)
{
    switch (m) {
    case LVideoAspectRatioMode::Ignore:     return L"拉伸";
    case LVideoAspectRatioMode::Keep:       return L"等比留边";
    case LVideoAspectRatioMode::KeepExpand: return L"等比裁切";
    }
    return L"等比留边";
}

} // namespace


class LVideoTestWindow : public LMainWindow
{
public:
    // autoQuitSeconds > 0 时，窗口会在指定秒数后自己关闭并让 app->exec() 返回，
    // 便于自动化冒烟测试（避免被外部强杀时解码器线程产生假崩溃）。
    // autoBrowseMs > 0 时，在指定毫秒后自动弹一次模态文件对话框，
    // 用于验证「对话框占用 GUI 线程时画面是否仍在刷新」。
    LVideoTestWindow(std::shared_ptr<LWidget> parent,
                     const std::wstring &initialPath,
                     int autoQuitSeconds,
                     int autoBrowseMs = 0,
                     int autoSeekSeconds = 0)
        : LMainWindow(parent, 0)
        , m_initialPath(initialPath)
        , m_autoQuitBeats(autoQuitSeconds > 0 ? autoQuitSeconds * 5 : 0)
        , m_autoBrowseTicks(autoBrowseMs > 0 ? (std::max)(1, autoBrowseMs / 200) : 0)
        , m_autoSeekSeconds(autoSeekSeconds)
    {}

protected:
    void initSpecific() override
    {
        const std::wstring backend = LStringUtil::utf8ToWide(LVideoPlayer::backendId());
        const bool available = LVideoPlayer::backendAvailable();

        setBaseSize(1000, 680);
        setWindowTitle(std::format(L"lkmedia 测试 [{} 后端]", backend));

        std::shared_ptr<LVBoxLayout> root = std::make_shared<LVBoxLayout>();
        root->setSpacing(6);
        setLayout(root);

        // 说明行
        // 只有视频区在布局中 Expanding，其余控件保持 Preferred：
        // 额外空间全部给视频，按钮/标签/滑块维持各自的 sizeHint 正常高度，
        // 否则根布局会把富余高度平摊到每一行，把按钮行拉高到 ~60px。
        const LSizePolicy preferred(LSizePolicy::Preferred, LSizePolicy::Preferred);
        const LSizePolicy expanding(LSizePolicy::Expanding, LSizePolicy::Expanding);

        std::shared_ptr<LLabel> tip = makeInit<LLabel>(available
            ? L"用法：点「浏览」选择视频，或在命令行传入路径。播放/暂停、跳转、音量、静音、循环、宽高比均可测试。"
            : L"当前构建为 Null 后端：UI 与状态机可测，但不会出画。用 -Dlooki_VIDEO_BACKEND=MF 重新配置即可启用解码。");
        tip->setMinimumHeight(24);
        tip->setSizePolicy(preferred);
        root->addWidget(tip);

        // 文件行
        std::shared_ptr<LHBoxLayout> fileRow = std::make_shared<LHBoxLayout>();
        fileRow->setSpacing(6);
        m_pathCombo = makeInit<LComboBox>();
        m_pathCombo->setMinimumHeight(30);
        m_pathCombo->setSizePolicy(preferred);
        fileRow->addWidget(m_pathCombo);

        std::shared_ptr<LButton> browseBtn = makeInit<LButton>(L"浏览...");
        browseBtn->setMinimumHeight(30);
        browseBtn->setSizePolicy(preferred);
        browseBtn->sigClicked.connect(this, &LVideoTestWindow::onBrowse);
        fileRow->addWidget(browseBtn);

        std::shared_ptr<LButton> openBtn = makeInit<LButton>(L"打开");
        openBtn->setMinimumHeight(30);
        openBtn->setSizePolicy(preferred);
        openBtn->sigClicked.connect(this, &LVideoTestWindow::onOpen);
        fileRow->addWidget(openBtn);
        root->addItem(fileRow);

        // 视频区
        m_video = makeInit<LVideoWidget>();
        m_video->setName("video");
        m_video->setMinimumHeight(380);
        m_video->setSizePolicy(expanding);
        m_video->setBackgroundColor(LColor(12, 12, 16, 255));
        m_video->setAspectRatioMode(LVideoAspectRatioMode::Keep);
        m_video->setFrameRate(10);
        m_video->setAudioDucking(true, 0.35);   // 方案 A：播放时压低 lkmedia 主音量
        root->addWidget(m_video);

        // 进度行
        std::shared_ptr<LHBoxLayout> seekRow = std::make_shared<LHBoxLayout>();
        seekRow->setSpacing(6);
        m_timeLabel = makeInit<LLabel>(L"00:00 / 00:00");
        m_timeLabel->setMinimumHeight(26);
        m_timeLabel->setSizePolicy(preferred);
        seekRow->addWidget(m_timeLabel);
        m_seekSlider = makeInit<LSlider>();
        m_seekSlider->setRange(0, 1000);
        m_seekSlider->setMinimumHeight(26);
        m_seekSlider->setSizePolicy(preferred);
        seekRow->addWidget(m_seekSlider);
        root->addItem(seekRow);

        // 控制行
        std::shared_ptr<LHBoxLayout> ctrlRow = std::make_shared<LHBoxLayout>();
        ctrlRow->setSpacing(6);
        m_playBtn = makeInit<LButton>(L"播放");
        m_playBtn->setMinimumHeight(30);
        m_playBtn->setSizePolicy(preferred);
        ctrlRow->addWidget(m_playBtn);

        std::shared_ptr<LButton> stopBtn = makeInit<LButton>(L"停止");
        stopBtn->setMinimumHeight(30);
        stopBtn->setSizePolicy(preferred);
        stopBtn->sigClicked.connect(this, &LVideoTestWindow::onStop);
        ctrlRow->addWidget(stopBtn);

        std::shared_ptr<LButton> backBtn = makeInit<LButton>(L"-10s");
        backBtn->setMinimumHeight(30);
        backBtn->setSizePolicy(preferred);
        backBtn->sigClicked.connect(this, &LVideoTestWindow::onBack10);
        ctrlRow->addWidget(backBtn);

        std::shared_ptr<LButton> fwdBtn = makeInit<LButton>(L"+10s");
        fwdBtn->setMinimumHeight(30);
        fwdBtn->setSizePolicy(preferred);
        fwdBtn->sigClicked.connect(this, &LVideoTestWindow::onForward10);
        ctrlRow->addWidget(fwdBtn);

        m_muteBtn = makeInit<LButton>(L"静音");
        m_muteBtn->setCheckable(true);
        m_muteBtn->setMinimumHeight(30);
        m_muteBtn->setSizePolicy(preferred);
        ctrlRow->addWidget(m_muteBtn);

        m_loopCheck = makeInit<LCheckBox>(L"循环");
        m_loopCheck->setMinimumHeight(30);
        m_loopCheck->setSizePolicy(preferred);
        ctrlRow->addWidget(m_loopCheck);

        m_topCheck = makeInit<LCheckBox>(L"置顶");
        m_topCheck->setMinimumHeight(30);
        m_topCheck->setSizePolicy(preferred);
        ctrlRow->addWidget(m_topCheck);

        m_aspectBtn = makeInit<LButton>(L"等比留边");
        m_aspectBtn->setMinimumHeight(30);
        m_aspectBtn->setSizePolicy(preferred);
        ctrlRow->addWidget(m_aspectBtn);
        root->addItem(ctrlRow);

        // 音量行
        std::shared_ptr<LHBoxLayout> volRow = std::make_shared<LHBoxLayout>();
        volRow->setSpacing(6);
        std::shared_ptr<LLabel> volLabel = makeInit<LLabel>(L"音量");
        volLabel->setMinimumHeight(26);
        volLabel->setSizePolicy(preferred);
        volRow->addWidget(volLabel);
        m_volumeSlider = makeInit<LSlider>();
        m_volumeSlider->setRange(0, 100);
        m_volumeSlider->setValue(100);      // 在连接信号之前设初值
        m_volumeSlider->setMinimumHeight(26);
        m_volumeSlider->setSizePolicy(preferred);
        volRow->addWidget(m_volumeSlider);
        root->addItem(volRow);

        // 状态行
        m_statusLabel = makeInit<LLabel>(std::format(L"后端 {} | 状态 空闲", backend));
        m_statusLabel->setMinimumHeight(24);
        m_statusLabel->setSizePolicy(preferred);
        root->addWidget(m_statusLabel);

        // 信号连接（初值都设完了，避免 setValue 触发误 seek / 误调音量）
        m_playBtn->sigClicked.connect(this, &LVideoTestWindow::onPlayPause);
        m_muteBtn->sigToggled.connect(this, &LVideoTestWindow::onMuteToggled);
        m_loopCheck->sigToggled.connect(this, &LVideoTestWindow::onLoopToggled);
        m_topCheck->sigToggled.connect(this, &LVideoTestWindow::onTopMostToggled);
        m_aspectBtn->sigClicked.connect(this, &LVideoTestWindow::onAspect);
        m_seekSlider->sigValueChanged.connect(this, &LVideoTestWindow::onSeekChanged);
        m_volumeSlider->sigValueChanged.connect(this, &LVideoTestWindow::onVolumeChanged);

        // GUI 线程上刷新进度 / 状态。LTimer 在池线程触发，必须 Queued。
        m_uiTimer = std::make_shared<LTimer>();
        m_uiTimer->sigTimeout.connect(this, &LVideoTestWindow::onUiTick,
                                      ConnectionType::Queued);
        m_uiTimer->start(200, true);

        // 命令行的文件等窗口真正创建、D2D device 就绪后再打开。
        if (!m_initialPath.empty()) {
            // 把首个文件和同目录下的其它视频都放进候选，方便直接切换测试
            addPathItem(m_initialPath);
            scanVideoDir(m_initialPath);
            m_openTimer = std::make_shared<LTimer>();
            m_openTimer->sigTimeout.connect(this, &LVideoTestWindow::onInitialOpen,
                                            ConnectionType::Queued);
            m_openTimer->start(300, false);
        }

        // 自动退出心跳：直连（Direct）在定时器线程执行，只做日志和置标志，
        // 真正的 quit() 由 GUI 线程上的 onUiTick 执行。
        if (m_autoQuitBeats > 0) {
            m_heartbeatTimer = std::make_shared<LTimer>();
            m_heartbeatTimer->sigTimeout.connect(this, &LVideoTestWindow::onHeartbeat,
                                                 ConnectionType::Direct);
            m_heartbeatTimer->start(200, true);
        }
    }

private:
    /** 把路径加入下拉候选（去重），并选中它 */
    void addPathItem(const std::wstring &path)
    {
        if(path.empty() || m_pathCombo == nullptr){
            return;
        }
        for(size_t i = 0; i < m_paths.size(); ++i){
            if(m_paths[i] == path){
                m_pathCombo->setCurrentIndex((int)i);
                return;
            }
        }
        m_paths.push_back(path);
        m_pathCombo->addItem(path);
        m_pathCombo->setCurrentIndex((int)m_paths.size() - 1);
    }

    /** 扫描首个视频所在目录，把同类型视频都放进候选 */
    void scanVideoDir(const std::wstring &file)
    {
        std::error_code ec;
        const std::filesystem::path dir = std::filesystem::path(file).parent_path();
        if(dir.empty()){
            return;
        }
        for(const auto &entry : std::filesystem::directory_iterator(dir, ec)){
            if(ec){
                break;
            }
            if(!entry.is_regular_file(ec)){
                continue;
            }
            std::wstring ext = entry.path().extension().wstring();
            std::transform(ext.begin(), ext.end(), ext.begin(),
                           [](wchar_t c){ return (wchar_t)std::towlower(c); });
            if(ext == L".mp4" || ext == L".mkv" || ext == L".mov" || ext == L".avi"
                    || ext == L".webm" || ext == L".m4v" || ext == L".wmv"){
                addPathItem(entry.path().wstring());
            }
        }
    }

    void openPath(const std::wstring &path, bool autoPlay)
    {
        if (path.empty()) {
            return;
        }
        addPathItem(path);

        const std::string uri = LStringUtil::wideToUtf8(path);
        m_autoPlay = false;
        if (!m_video->open(uri)) {
            m_statusLabel->setText(std::format(L"打开失败：{}（详见日志）", path));
            lWarn() << "lkmedia test: open failed:" << uri;
            return;
        }
        lWarn() << "lkmedia test: opened" << uri;
        // MF 的 SetSource/Load 是异步的，等 CANPLAY → Ready 后由 UI 定时器启动播放。
        m_autoPlay = autoPlay;

        // 自检：约 3 秒后（15 次 UI tick）把状态打进日志，方便自动化冒烟测试
        // 确认真的解出了帧（duration / position / 原始分辨率都不为 0）。
        m_probeTick = m_uiTicks + 15;
        refreshUi();
    }

    void onBrowse(bool)
    {
        // 取帧与呈现已在独立渲染线程，模态对话框不再阻塞画面，无需暂停。
        const std::wstring file = LFileDialog::getOpenFile(
            L"", L"*.mp4;;*.mkv;;*.mov;;*.avi;;*.webm;;*.m4v;;*.*");
        if (!file.empty()) {
            openPath(file, true);
        }
    }

    void onOpen(bool)
    {
        const std::wstring path = m_pathCombo != nullptr ? m_pathCombo->currentText()
                                                         : std::wstring();
        if (path.empty()) {
            onBrowse(false);
            return;
        }
        openPath(path, true);
    }

    void onInitialOpen()
    {
        openPath(m_initialPath, true);
    }

    void onProbe()
    {
        uint32_t w = 0;
        uint32_t h = 0;
        const std::shared_ptr<LVideoPlayer> player = m_video->player();
        if (player != nullptr) {
            player->nativeVideoSize(w, h);
        }
        lWarn() << "lkmedia test: probe state =" << int(m_video->state())
                << " duration =" << m_video->duration()
                << " position =" << m_video->position()
                << " native size =" << w << "x" << h
                << " backend =" << LVideoPlayer::backendId();
    }

    void onHeartbeat()
    {
#if defined(_WIN32)
        // 原生句柄只在 GUI 线程(onUiTick)里取，心跳属于定时器线程，
        // 跨线程调 internalWinId() 可能拿到空值，WM_NULL/WM_CLOSE 就都发不出去。
        if (m_hwnd != nullptr) {
            // 无交互的桌面会话里 GUI 消息循环可能一直空转，Queued 回调得不到
            // 派发（表现为 UI 刷新 / 视频帧轮询停住）。每拍投一条 WM_NULL
            // 唤醒它，让队列里的回调正常执行。
            PostMessage(reinterpret_cast<HWND>(m_hwnd), WM_NULL, 0, 0);
        }
#endif
        ++m_heartbeats;
        if (m_heartbeats % 10 == 0) {
            lWarn() << "lkmedia test: heartbeat" << m_heartbeats
                    << "ui ticks" << m_uiTicks;
        }

        // 自动退出的兜底计时：无交互桌面会话里 GUI 的 Queued 回调可能整体停摆
        // （WM_NULL / WM_CLOSE 都排不进去），此时不能让进程一直挂着等外部强杀。
        if (m_autoQuitGrace > 0 && --m_autoQuitGrace == 0) {
            lWarn() << "lkmedia test: GUI loop stalled, forcing clean exit";
#if defined(_WIN32)
            TerminateProcess(GetCurrentProcess(), 0);
#else
            std::_Exit(0);
#endif
        }
        if (m_autoQuitGrace > 0 || m_autoQuitBeats <= 0
            || m_heartbeats < m_autoQuitBeats) {
            return;
        }

        m_autoQuitBeats = 0;
        m_autoQuitGrace = 12;   // 2.4 秒内没走正常退出就兜底
        m_autoQuitRequested.store(true, std::memory_order_relaxed);
        lWarn() << "lkmedia test: auto quit requested";
        // 不投 WM_CLOSE：它只销毁窗口，不保证 app->exec() 的事件循环退出，
        // 反而会让定时器随窗口一起停掉、兜底也跑不了。统一由 onUiTick()
        // 在 GUI 线程调 lApp->quit(0)；若 Queued 回调停摆则由上面的 grace 兜底。
    }

    void onPlayPause(bool)
    {
        const LVideoState st = m_video->state();
        m_autoPlay = false;
        if (st == LVideoState::Playing) {
            m_video->pause();
        } else if (st == LVideoState::Idle || st == LVideoState::Error) {
            // 重新打开当前路径，让引擎重新 Load 一次
            openPath(m_pathCombo->currentText(), true);
            return;
        } else {
            m_video->play();
        }
        refreshUi();
    }

    void onStop(bool)
    {
        m_autoPlay = false;
        m_video->stop();
        refreshUi();
    }

    void onBack10(bool)
    {
        seekRelative(-10.0);
    }

    void onForward10(bool)
    {
        seekRelative(10.0);
    }

    void seekRelative(double delta)
    {
        const double target = m_video->position() + delta;
        m_video->seek(target > 0.0 ? target : 0.0);
        refreshUi();
    }

    void onMuteToggled(bool on)
    {
        m_video->setMuted(on);
    }

    void onLoopToggled(bool on)
    {
        m_video->setLoop(on);
    }

    void onTopMostToggled(bool on)
    {
        // 通过平台层应用窗口标志，不直接操作原生句柄。
        std::shared_ptr<LWindow> w = windowHandle(WinHandleMode::TopLevel);
        if (w == nullptr) {
            return;
        }
        LFlags<LWindowType> flags = w->flags();
        if (on) {
            flags |= LWindowType::WindowStaysOnTopHint;
        } else {
            flags &= ~LFlags<LWindowType>(LWindowType::WindowStaysOnTopHint);
        }
        w->setFlags(flags);
        if (w->handle() != nullptr) {
            w->handle()->setWindowFlags(flags);
        }
    }

    void onAspect(bool)
    {
        const int next = (static_cast<int>(m_video->aspectRatioMode()) + 1) % 3;
        const LVideoAspectRatioMode mode = static_cast<LVideoAspectRatioMode>(next);
        m_video->setAspectRatioMode(mode);
        m_aspectBtn->setText(aspectName(mode));
    }

    void onSeekChanged(int value)
    {
        if (m_updatingSlider || m_video == nullptr) {
            return;
        }
        const double duration = m_video->duration();
        if (duration > 0.0) {
            m_video->seek(duration * double(value) / 1000.0);
        }
    }

    void onVolumeChanged(int value)
    {
        if (m_video != nullptr) {
            m_video->setVolume(double(value) / 100.0);
        }
    }

    void onUiTick()
    {
#if defined(_WIN32)
        if (m_hwnd == nullptr) {
            // 本回调在 GUI 线程派发，此时窗口已 show，原生句柄必定有效。
            m_hwnd = reinterpret_cast<void *>(internalWinId());
        }
#endif
        if (m_autoQuitRequested.load(std::memory_order_relaxed)) {
            // quit() 必须在主线程（主事件循环所在线程）调用。
            lApp->quit(0);
            return;
        }
        if (m_video == nullptr) {
            return;
        }
        ++m_uiTicks;
        if (m_autoBrowseTicks > 0 && --m_autoBrowseTicks == 0) {
            lWarn() << "lkmedia test: auto browse, dialog now blocks GUI thread";
            onBrowse(false);
        }
        if (m_probeTick > 0 && m_uiTicks >= m_probeTick) {
            m_probeTick = 0;
            onProbe();
        }
        const LVideoState st = m_video->state();
        if (m_autoSeekSeconds > 0 && !m_autoSeekDone
                && st == LVideoState::Playing && m_video->duration() > 0.0) {
            m_autoSeekDone = true;
            m_video->setLoop(true);   // 自动开启循环，验证结尾回绕
            m_video->seek(double(m_autoSeekSeconds));
            lWarn() << "lkmedia test: auto seek to" << m_autoSeekSeconds;
        }
        if (m_autoPlay) {
            if (st == LVideoState::Ready) {
                m_autoPlay = false;
                m_video->play();
                lWarn() << "lkmedia test: CANPLAY -> play";
            } else if (st == LVideoState::Error) {
                // 换源期间会短暂经过 Idle，不能据此取消自动播放
                m_autoPlay = false;
            }
        }
        refreshUi();
    }

    void refreshUi()
    {
        if (m_video == nullptr || m_statusLabel == nullptr) {
            return;
        }
        const double duration = m_video->duration();
        const double position = m_video->position();
        const LVideoState st = m_video->state();

        m_updatingSlider = true;
        if (duration > 0.0) {
            const int value = static_cast<int>(position / duration * 1000.0);
            m_seekSlider->setValue(value < 0 ? 0 : (value > 1000 ? 1000 : value));
        }
        m_updatingSlider = false;

        m_timeLabel->setText(std::format(L"{} / {}",
                                        formatSeconds(position),
                                        formatSeconds(duration)));
        m_playBtn->setText(st == LVideoState::Playing ? L"暂停" : L"播放");

        uint32_t w = 0;
        uint32_t h = 0;
        const std::shared_ptr<LVideoPlayer> player = m_video->player();
        if (player != nullptr) {
            player->nativeVideoSize(w, h);
        }
        std::wstring ducking = L"关";
        if (player != nullptr && player->audioDucking()) {
            ducking = std::format(L"{:.2f}", player->duckingGain());
        }
        m_statusLabel->setText(std::format(
            L"后端 {} | 状态 {} | 原始分辨率 {}x{} | 轮询 {} fps | 宽高比 {} | ducking {}",
            LStringUtil::utf8ToWide(LVideoPlayer::backendId()),
            stateName(st),
            w, h,
            m_video->frameRate(),
            aspectName(m_video->aspectRatioMode()),
            ducking));
    }

private:
    std::wstring m_initialPath;
    bool m_autoPlay = false;
    bool m_updatingSlider = false;
    int m_uiTicks = 0;
    int m_probeTick = 0;
    int m_heartbeats = 0;
    int m_autoQuitBeats = 0;
    int m_autoQuitGrace = 0;
    int m_autoBrowseTicks = 0;
    int m_autoSeekSeconds = 0;
    bool m_autoSeekDone = false;
    void *m_hwnd = nullptr;
    std::atomic<bool> m_autoQuitRequested{false};

    std::shared_ptr<LVideoWidget> m_video;
    std::shared_ptr<LComboBox> m_pathCombo;
    std::vector<std::wstring> m_paths;
    std::shared_ptr<LLabel> m_timeLabel;
    std::shared_ptr<LLabel> m_statusLabel;
    std::shared_ptr<LSlider> m_seekSlider;
    std::shared_ptr<LSlider> m_volumeSlider;
    std::shared_ptr<LButton> m_playBtn;
    std::shared_ptr<LButton> m_muteBtn;
    std::shared_ptr<LCheckBox> m_loopCheck;
    std::shared_ptr<LCheckBox> m_topCheck;
    std::shared_ptr<LButton> m_aspectBtn;
    std::shared_ptr<LTimer> m_uiTimer;
    std::shared_ptr<LTimer> m_openTimer;
    std::shared_ptr<LTimer> m_heartbeatTimer;
};


int main(int argc, char *argv[])
{
#if defined(_WIN32)
    LLoger::defaultLogEncode = GetACP() == CP_UTF8 ? "" : "gbk";
#endif
    std::wstring initialPath;
    if (argc > 1) {
        const std::filesystem::path p(LStringUtil::localToWide(argv[1]));
        initialPath = p.lexically_normal().wstring();
    }
    int autoQuitSeconds = argc > 2 ? std::atoi(argv[2]) : 0;
    // 第 3 个参数：N 毫秒后自动弹一次文件对话框（验证模态期间画面是否继续）。
    int autoBrowseMs = argc > 3 ? std::atoi(argv[3]) : 0;
    // 第 4 个参数：播放开始后自动 seek 到指定秒数（验证循环回绕）。
    int autoSeekSeconds = argc > 4 ? std::atoi(argv[4]) : 0;

    std::shared_ptr<LApplication> app = makeInit<LApplication>(argc, argv);
    std::shared_ptr<LMainWindow> win =
        makeInit<LVideoTestWindow>(nullptr, initialPath, autoQuitSeconds,
                                   autoBrowseMs, autoSeekSeconds);
    win->resize(LSize<int>(900, 760));
    win->show();

    lWarn() << "lkmedia test: backend =" << LVideoPlayer::backendId()
            << ", available =" << int(LVideoPlayer::backendAvailable());
    return app->exec();
}
