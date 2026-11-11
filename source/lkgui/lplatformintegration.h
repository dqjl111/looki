#ifndef LPLATFORMINTEGRATION_H
#define LPLATFORMINTEGRATION_H
#include "lkgui/lplatforminputctx.h"
#include "lkcore/leventdispatcher.h"
#include "lkgui/lwindow.h"
#include "lkgui/lguienums.h"
#include "lkcore/lvariant.h"
#include "lkgui/lplatformbackingstore.h"
#include "lkgui/text/lplatformfontdatabase.h"
#include "lkgui/lplatformtheme.h"
#include "lkgui/lplatformclipboard.h"
#include "lkgui/lplatformdialoghelper.h"

LK_NAMESPACE_BEGIN
enum class LPlatformCapability {
    ThreadedPixmaps = 1,
    OpenGL,
    ThreadedOpenGL,
    SharedGraphicsCache,
    BufferQueueingOpenGL,
    WindowMasks,
    MultipleWindows,
    ApplicationState,
    ForeignWindows,
    NonFullScreenWindows,
    NativeWidgets,
    WindowManagement,
    WindowActivation, // whether requestActivate is supported
    SyncState,
    RasterGLSurface,
    AllGLFunctionsQueryable,
    ApplicationIcon,
    SwitchableWidgetComposition,
    TopStackedNativeChildWindows,
    OpenGLOnRasterSurface,
    MaximizeUsingFullscreenGeometry
};

enum class LPlatformStyleHint {
    CursorFlashTime,
    KeyboardInputInterval,
    MouseDoubleClickInterval,
    StartDragDistance,
    StartDragTime,
    KeyboardAutoRepeatRate,
    ShowIsFullScreen,
    PasswordMaskDelay,
    FontSmoothingGamma,
    StartDragVelocity,
    UseRtlExtensions,
    PasswordMaskCharacter,
    SetFocusOnTouchRelease,
    ShowIsMaximized,
    MousePressAndHoldInterval,
    TabFocusBehavior,
    ReplayMousePressOutsidePopup,
    ItemViewActivateItemOnSingleClick,
    UiEffects,
    WheelScrollLines,
    ShowShortcutsInContextMenus,
    MouseQuickSelectionThreshold
};


class LKGUIDEF LPlatformIntegration
{
public:
    LPlatformIntegration();

    virtual bool hasPointerEventCursor(){
        return false;
    }

    virtual void initialize();
    virtual void destroy();

    virtual std::shared_ptr<LPlatformPixmap> createPlatformPixmap() const;
    virtual std::shared_ptr<LEventDispatcher> createEventDispatcher() const = 0;
    virtual std::shared_ptr<LPlatformBackingStore> createPlatformBackingStore(std::shared_ptr<LWindow> window) const = 0;

    virtual LVariant styleHint(LPlatformTheme::Hint hint) const;
    virtual LWindowState defaultWindowState(LFlags<LWindowType> flags) const;

    virtual bool hasCapability(LPlatformCapability cap) const;
    virtual std::shared_ptr<LPaintEngine> createImagePaintEngine(std::shared_ptr<LPaintDevice> paintDevice) const;

    virtual std::shared_ptr<LPlatformWindow> createPlatformWindow(std::shared_ptr<LWindow> window) const = 0;
    virtual std::shared_ptr<LPlatformWindow> createForeignWindow(std::shared_ptr<LWindow>, WId) const { return nullptr; }
    virtual std::shared_ptr<LPlatformFontDatabase> fontDatabase() const;
    virtual std::shared_ptr<LPlatformClipboard> clipboard() const;
    virtual std::shared_ptr<LPlatformDialogHelper> createPlatformDialogHelper(LPlatformDialogHelper::Type type) const;

    virtual void applicationStateChanged(LApplicationState state);

    inline std::shared_ptr<LPlatformInputCtx> inputContext() const{
        return m_inputCtx;
    }

    virtual LSize<int> systemCursorSize() const;
    virtual std::list<uint32_t> possibleKeys(const std::shared_ptr<LKeyEvent> e) const;
protected:
    std::shared_ptr<LPlatformInputCtx> m_inputCtx = nullptr;
};

LK_NAMESPACE_END
#endif // LPLATFORMINTEGRATION_H
