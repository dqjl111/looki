#ifndef LPLATFORMTHEME_H
#define LPLATFORMTHEME_H
#include "lkcore/lookiconfig.h"
#include "lkcore/lvariant.h"

LK_NAMESPACE_BEGIN

class LKGUIDEF LPlatformTheme
{
public:
    LPlatformTheme();

    enum class Hint {
        CursorFlashTime,
        KeyboardInputInterval,
        KeyboardAutoRepeatRate,
        MouseDoubleClickInterval,
        StartDragDistance,
        StartDragTime,
        StartDragVelocity,
        UseFullScreenForPopup,
        WindowAutoPlacement,
        ShowShortcutsInContextMenus,
        TextCursorWidth,
        DropShadow,
        MaximumScrollBarDragDistance,
        KeyboardScheme,
        UiEffects,
        DialogSnapToDefaultButton,
        ContextMenuOnMouseRelease,
        MousePressAndHoldInterval,
        MouseDoubleClickDistance,
        WheelScrollLines,
        TouchDoubleTapDistance,
        MouseQuickSelectionThreshold,
        ShowIsFullScreen,
        ShowIsMaximized
    };

    enum class DialogType {
        File,
        Color,
        Font,
        Message
    };

    enum class Palette {
        System,
        ToolTip,
        ToolButton,
        Butto,
        CheckBox,
        RadioButton,
        Header,
        ComboBox,
        ItemView,
        MessageBoxLabel,
        TabBar,
        Label,
        GroupBox,
        Menu,
        MenuBar,
        TextEdit,
        TextLineEdit,
        NPalettes
    };

    enum class Font {
        System,
        Menu,
        MenuBar,
        MenuItem,
        MessageBox,
        Label,
        TipLabel,
        StatusBar,
        TitleBar,
        MdiSubWindowTitle,
        DockWidgetTitle,
        PushButton,
        CheckBox,
        RadioButton,
        ToolButton,
        ItemView,
        ListView,
        HeaderView,
        ListBox,
        ComboMenuItem,
        ComboLineEdit,
        Small,
        Mini,
        Fixed,
        GroupBoxTitle,
        TabButton,
        Editor,
        NFonts
    };

    enum class Icon {
        TitleBarMenu,
        TitleBarMin,
        TitleBarMax,
        TitleBarClose,
        TitleBarNormal,
        TitleBarShade,
        TitleBarUnshade,
        TitleBarContextHelp,
        DockWidgetClose,
        MessageBoxInformation,
        MessageBoxWarning,
        MessageBoxCritical,
        MessageBoxQuestion,
        Desktop,
        Trash,
        Computer,
        DriveFD,
        DriveHD,
        DriveCD,
        DriveDVD,
        DriveNet,
        DirOpen,
        DirClosed,
        DirLink,
        DirLinkOpen,
        File,
        FileLink,
        ToolBarHorizontalExtension,
        ToolBarVerticalExtension,
        FileDialogStart,
        FileDialogEnd,
        FileDialogToParent,
        FileDialogNewFolder,
        FileDialogDetailedView,
        FileDialogInfoView,
        FileDialogContentsView,
        FileDialogListView,
        FileDialogBack,
        Dir,
        DialogOk,
        DialogCancel,
        DialogHelp,
        DialogOpen,
        DialogSave,
        DialogClose,
        DialogApply,
        DialogReset,
        DialogDiscard,
        DialogYes,
        DialogNo,
        ArrowUp,
        ArrowDown,
        ArrowLeft,
        ArrowRight,
        ArrowBack,
        ArrowForward,
        DirHome,
        CommandLink,
        VistaShield,
        BrowserReload,
        BrowserStop,
        MediaPlay,
        MediaStop,
        MediaPause,
        MediaSkipForward,
        MediaSkipBackward,
        MediaSeekForward,
        MediaSeekBackward,
        MediaVolume,
        MediaVolumeMuted,
        LineEditClear,
        CustomBase = 0xffU
    };

    enum class KeyboardScheme
    {
        Windows,
        Mac,
        X11,
        Kde,
        Gnome,
        Cde
    };

    enum class UiEffect
    {
        General = 0x1,
        AnimateMenu = 0x2,
        FadeMenu = 0x4,
        AnimateCombo = 0x8,
        AnimateTooltip = 0x10,
        FadeTooltip = 0x20,
        AnimateToolBox = 0x40,
        Hover = 0x80
    };

    static LVariant defaultThemeHint(Hint hint);
};

LK_NAMESPACE_END

#endif // LPLATFORMTHEME_H
