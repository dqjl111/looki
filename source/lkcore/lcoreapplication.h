#ifndef LCOREAPPLICATION_H
#define LCOREAPPLICATION_H

#include "lkcore/leventdispatcher.h"
#include "lkcore/time/ltimerpool.h"
LK_NAMESPACE_BEGIN

#define lApp LCoreApplication::instance()
#define lAppShare LCoreApplication::instShare()

enum class LAppAttribute
{
    DontShowIconsInMenus = 2,
    NativeWindows = 3,
    DontCreateNativeWidgetSiblings = 4,
    PluginApplication = 5,
    DontUseNativeMenuBar = 6,
    MacDontSwapCtrlAndMeta = 7,
    Use96Dpi = 8,
    DisableNativeVirtualKeyboard = 9,
    SynthesizeTouchForUnhandledMouseEvents = 11,
    SynthesizeMouseForUnhandledTouchEvents = 12,
    UseHighDpiPixmaps = 13,
    ForceRasterWidgets = 14,
    UseDesktopOpenGL = 15,
    UseOpenGLES = 16,
    UseSoftwareOpenGL = 17,
    ShareOpenGLContexts = 18,
    SetPalette = 19,
    EnableHighDpiScaling = 20,
    DisableHighDpiScaling = 21,
    UseStyleSheetPropagationInWidgetStyles = 22,
    DontUseNativeDialogs = 23,
    SynthesizeMouseForUnhandledTabletEvents = 24,
    CompressHighFrequencyEvents = 25,
    DontCheckOpenGLContextThreadAffinity = 26,
    DisableShaderDiskCache = 27,
    DontShowShortcutsInContextMenus = 28,
    CompressTabletEvents = 29,
    DisableSessionManager = 31,
    // Add new attributes before this line
    AttributeCount
};

class LKCOREDEF LCoreApplication : public LObject {
public:
    LCoreApplication(int &aargc, char **aargv);
    virtual ~LCoreApplication() override;

    void quit(int exitCode = 0);
    static void exit(int exitCode = 0);
    static int exec();

    static void setAttribute(LAppAttribute attribute, bool on = true);
    static inline bool testAttribute(LAppAttribute attribute){
        return m_attribs & (1 << int(attribute));
    }

    static LCoreApplication *instance() { return m_self; }
    static std::shared_ptr<LCoreApplication> instShare() {
        return m_self==nullptr ? nullptr : m_self->getPtrT<LCoreApplication>();
    }
    static std::shared_ptr<LTimerPool> timePool();

    virtual bool notify(std::shared_ptr<LObject> receiver, std::shared_ptr<LEvent> event);

    static bool sendEvent(std::shared_ptr<LObject> receiver, std::shared_ptr<LEvent> event);
    static void postEvent(std::shared_ptr<LObject> receiver, std::shared_ptr<LEvent> event);

    static std::shared_ptr<LEventDispatcher> coreEventDispatcher()
    {
        if (LCoreApplication::instance()){
            return LCoreApplication::instance()->m_data->threadData()->m_eventDispatcher;
        }else{
            return nullptr;
        }
    }

    virtual void init() override;

    inline bool inExec(){
        return m_inExec;
    }

    static void processEvents();

    static inline bool startingUp()
    {
        return !LCoreApplication::m_isAppRunning;
    }

    static inline bool closingDown()
    {
        return LCoreApplication::m_isAppClosing;
    }

    static bool isQuitLockEnabled();
    static void setQuitLockEnabled(bool enabled);

    void maybeQuit();
    static bool forwardEvent(std::shared_ptr<LObject> receiver,
                             std::shared_ptr<LEvent> event,
                             std::shared_ptr<LEvent> orgEvent = nullptr);

    static std::filesystem::path applicationFilePath();

protected:
    virtual std::shared_ptr<LEventDispatcher> createEventDispatcher();
    virtual void eventDispatcherReady();
    virtual void execCleanup();
    virtual bool handleEvent(std::shared_ptr<LEvent> event) override;

    virtual bool shouldQuit() {
        return true;
    }

    static bool notifyInternal(std::shared_ptr<LObject> reciever, std::shared_ptr<LEvent> event);
    static bool threadRequiresCoreApplication();
    static bool notifyHelper(std::shared_ptr<LObject> receiver, std::shared_ptr<LEvent> event);
    static bool doNotify(std::shared_ptr<LObject> receiver, std::shared_ptr<LEvent> event);
    bool sendThroughApplicationEventFilters(std::shared_ptr<LObject> receiver,
                                            std::shared_ptr<LEvent> event);
    static bool sendThroughObjectEventFilters(std::shared_ptr<LObject> receiver,
                                              std::shared_ptr<LEvent> event);

    static bool sendSpontaneousEvent(std::shared_ptr<LObject> receiver,
                                     std::shared_ptr<LEvent> event);


    int &m_argc;
    char **m_argv;

    std::shared_ptr<LEventDispatcher> m_eventDispatcher;
    bool m_inExec = false;

    static uint32_t m_attribs;
    static bool m_isAppRunning;
    static bool m_isAppClosing;

    LTimerPool::PoolHandle m_timepool;
    static LCoreApplication *m_self;
};

LK_NAMESPACE_END
#endif // LCOREAPPLICATION_H
