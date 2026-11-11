#ifndef LWIDGETDATA_H
#define LWIDGETDATA_H

#include "lkcore/lookiconfig.h"
#include "lkgui/lsize.h"
#include "lkgui/lrect.h"
#include "lkgui/lguienums.h"
#include "lkgui/image/ltexture.h"
#include "lkgui/lbackingstore.h"
#include "lwidgetrepaintman.h"
#include "lkgui/lpalette.h"
#include <array>

LK_NAMESPACE_BEGIN

enum class LWidgetAttribute : uint32_t{
    Disabled,
    UnderMouse,
    MouseTracking,
    QuitOnClose,
    ReleaseParentOnClose,
    WindowModified,
    Resized,
    Moved,
    PendingUpdate,
    InvalidSize,
    WStateVisible,
    WStateHidden,
    WStateCreated,
    WStateCompressKeys,
    WStateInPaintEvent,
    WStateReparented,
    WStateConfigPending,
    WStatePolished,
    WStateOwnSizePolicy,
    WStateExplicitShowHide,
    NoMouseReplay,
    NoMousePropagation,
    Hover,
    DontShowOnScreen,
    KeyboardFocusChange,
    NativeWindow,
    DontCreateNativeAncestors,
    DropSiteRegistered,
    SetWindowIcon,
    OpaquePaintEvent,
    PaintOnScreen,
    ShowModal,
    SetWindowModality,
    StyleSheet,
    InputMethodEnabled,
    UpdatesDisabled,
    ForceDisabled,
    ForceUpdatesDisabled,
    Mapped,
    OutsideWSRange,
    TranslucentBackground,
    StyledBackground,
    NoSystemBackground,
    StaticContents,
    TintedBackground,
    LayoutUsesWidgetRect,
    RightToLeft,
    SetLayoutDirection,
    LaidOut,
    GroupLeader,
    SetCursor,
    AcceptDrops,
    MouseNoMask,
    TransparentForMouseEvents,
    CanEnterSubItem
};

class LWidgetData
{
public:
    LWidgetData(){}
    WId winId=0;
    LFlags<LWindowType> windowFlags = LWindowType::Widget;
    uint32_t windowState : 4;
    uint32_t focusPolicy : 4;
    uint32_t sizeHintForced :1;
    uint32_t isClosing :1;
    uint32_t inShow : 1;
    uint32_t inSetWindowState : 1;
    mutable uint32_t fstrutDirty : 1;
    uint32_t contextMenuPolicy : 3;
    uint32_t windowModality : 2;
    uint32_t inDestructor : 1;
    uint32_t unused : 13;
    LRect<int> crect;
    std::array<uint32_t, 2> attributes;
    std::shared_ptr<LFont> font;
};

class LWidgetWindow;
class LTLWExtra {
public:
    LTLWExtra(){}

    std::unique_ptr<LWidgetRepaintMan> repaintMan;
    std::shared_ptr<LWidgetWindow> window;
    std::shared_ptr<LTexture> icon;
    std::shared_ptr<LBackingStore> backingStore;
    // Implicit pointers (shared_null).
    std::wstring caption; // widget caption
    // Other variables.
    short incw, inch; // size increments
    short basew, baseh; // base sizes
    LMargins<int> frameStrut;
    LFlags<LWindowType> savedFlags; // Save widget flags while showing fullscreen
    int initialScreenIndex; // Screen number when passing a LDesktop[Screen]Widget as parent.
    // *************************** Cross-platform bit fields ****************************
    uint32_t opacity : 8;
    uint32_t posIncludesFrame : 1;
    uint32_t sizeAdjusted : 1;
    uint32_t embedded : 1;
};

class LStyle;
class LWidgetExtra {

public:
    LWidgetExtra(){}

    std::unique_ptr<LTLWExtra> topExtra;  // only useful for top level windows
    std::weak_ptr<LWidget> focusProxy;
    std::shared_ptr<LRegion> mask; // widget mask
    LCursor cursor;
    int minw;
    int minh; // minimum size
    int maxw;
    int maxh; // maximum size
    LSize<int> staticContentsSize;

    // *************************** Cross-platform bit fields ****************************
    uint32_t explicitMinSize : 2;
    uint32_t explicitMaxSize : 2;
    uint32_t autoFillBackground : 1;
    uint32_t nativeChildrenForced : 1;
    uint32_t inRenderWithPainter : 1;
    uint32_t hasWindowContainer : 1;
};



LK_NAMESPACE_END

#endif // LWIDGETDATA_H
