#ifndef LGUIENUMS_H
#define LGUIENUMS_H

#include "lkcore/lcoreenums.h"

LK_NAMESPACE_BEGIN
enum class LWindowType : uint32_t {
    Widget = 0x00000000,
    Window = 0x00000001,
    Dialog = 0x00000002 | Window,
    Sheet = 0x00000004 | Window,
    Drawer = Sheet | Dialog,
    Popup = 0x00000008 | Window,
    Tool = Popup | Dialog,
    ToolTip = Popup | Sheet,
    SplashScreen = ToolTip | Dialog,
    Desktop = 0x00000010 | Window,
    SubWindow = 0x00000012,
    ForeignWindow = 0x00000020 | Window,
    CoverWindow = 0x00000040 | Window,

    WindowTypeMask = 0x000000ff,
    MSWindowsFixedSizeDialogHint = 0x00000100,
    MSWindowsOwnDC = 0x00000200,
    BypassWindowManagerHint = 0x00000400,
    X11BypassWindowManagerHint = BypassWindowManagerHint,
    FramelessWindowHint = 0x00000800,
    WindowTitleHint = 0x00001000,
    WindowSystemMenuHint = 0x00002000,
    WindowMinimizeButtonHint = 0x00004000,
    WindowMaximizeButtonHint = 0x00008000,
    WindowMinMaxButtonsHint = WindowMinimizeButtonHint | WindowMaximizeButtonHint,
    WindowContextHelpButtonHint = 0x00010000,
    WindowShadeButtonHint = 0x00020000,
    WindowStaysOnTopHint = 0x00040000,
    WindowTransparentForInput = 0x00080000,
    WindowOverridesSystemGestures = 0x00100000,
    WindowDoesNotAcceptFocus = 0x00200000,
    MaximizeUsingFullscreenGeometryHint = 0x00400000,

    Minimized  = 0x01000000,
    CustomizeWindowHint = 0x02000000,
    WindowStaysOnBottomHint = 0x04000000,
    WindowCloseButtonHint = 0x08000000,
    MacWindowToolBarButtonHint = 0x10000000,
    BypassGraphicsProxyWidget = 0x20000000,
    NoDropShadowWindowHint = 0x40000000,
    WindowFullscreenButtonHint = 0x80000000

    //inline operator uint() const noexcept { return *this; }
};

L_EN_FLAGS(LWindowType)

enum class LWindowState : uint8_t{
    NoState    = 0x00,
    Minimized  = 0x01,
    Maximized  = 0x02,
    FullScreen = 0x04,
    Active     = 0x08
};

L_EN_FLAGS(LWindowState)

enum class LImageConvertFlag : uint32_t{
    ColorModeMask          = 0x0003,
    AutoColor               = 0x0000,
    ColorOnly               = 0x0003,
    MonoOnly                = 0x0002,
    // Reserved             = 0x00000001,

    AlphaDitherMask        = 0x000c,
    ThresholdAlphaDither    = 0x0000,
    OrderedAlphaDither      = 0x0004,
    DiffuseAlphaDither      = 0x0008,
    NoAlpha                 = 0x000c, // Not supported

    DitherMask             = 0x0030,
    DiffuseDither           = 0x0000,
    OrderedDither           = 0x0010,
    ThresholdDither         = 0x0020,
    // ReservedDither       = 0x00000030,

    DitherModeMask         = 0x00c0,
    AutoDither              = 0x0000,
    PreferDither            = 0x0040,
    AvoidDither             = 0x0080,

    NoOpaqueDetection       = 0x0100,
    NoFormatConversion      = 0x0200
};
L_EN_FLAGS(LImageConvertFlag)

enum class LRenderHint {
    Antialiasing = 0x01,
    TextAntialiasing = 0x02,
    SmoothPixmapTransform = 0x04,
    LosslessImageRendering = 0x08,
};
L_EN_FLAGS(LRenderHint)


enum class LTextRenderHint : uint32_t{
    RightToLeft = 0x1,
    Overline = 0x10,
    Underline = 0x20,
    StrikeOut = 0x40,
    Dummy = 0xffffffff
};
L_EN_FLAGS(LTextRenderHint)

enum class LTextElide {
    Left,
    Right,
    Middle,
    None
};


enum class LAspectRatioMode : uint8_t {
    IgnoreAspectRatio,
    KeepAspectRatio,
    KeepAspectRatioByExpanding
};

enum class LTransMode  : uint8_t{
    Fast = 0,
    Smooth = 1,
    FastCanRetThis = 2,
    SmoothCanRetThis = 3
};

enum class LBGMode {
    Transparent,
    Opaque
};

enum class LLayoutDirection {
    LeftToRight,
    RightToLeft,
    Auto
};


enum class LContextMenuPolicy : uint8_t {
    No,
    Default,
    Custom,
    Prevent
};

enum class LWindowModality : uint8_t {
    NonModal,
    WindowModal,
    ApplicationModal
};

enum class LFocusReason : uint8_t {
    Mouse,
    Tab,
    Backtab,
    ActiveWindow,
    Popup,
    Shortcut,
    MenuBar,
    Other,
    No
};

enum class LFocusPolicy : uint8_t {
    No = 0,
    Tab = 0x1,
    Click = 0x2,
    Strong = Tab | Click | 0x8,
    Wheel = Strong | 0x4
};

enum class LTabFocusCtrls  : uint8_t{
    No   = 0x00,
    Text = 0x01,
    List = 0x02,
    All  = 0xff
};

enum class LSortOrder : uint8_t {
    Ascend,
    Descend
};

enum class LFillRule : uint8_t{
    OddEven,
    Winding
};

enum class LColorID : uint8_t{
    Transparent, // 0x00000000
    AliceBlue, // 0xfffff8f0
    AntiqueWhite, // 0xffd7ebfa
    Aqua, // 0xffffff00
    Aquamarine, // 0xffd4ff7f
    Azure, // 0xfffffff0
    Beige, // 0xffdcf5f5
    Bisque, // 0xffc4e4ff
    Black, // 0xff000000
    BlanchedAlmond, // 0xffcdebff
    Blue, // 0xffff0000
    BlueViolet, // 0xffe22b8a
    Brown, // 0xff2a2aa5
    BurlyWood, // 0xff87b8de
    CadetBlue, // 0xffa09e5f
    Chartreuse, // 0xff00ff7f
    Chocolate, // 0xff1e69d2
    Coral, // 0xff507fff
    CornflowerBlue, // 0xffed9564
    Cornsilk, // 0xffdcf8ff
    Crimson, // 0xff3c14dc
    Cyan, // 0xffffff00
    DarkBlue, // 0xff8b0000
    DarkCyan, // 0xff8b8b00
    DarkGoldenRod, // 0xff0b86b8
    DarkGray, // 0xffa9a9a9
    DarkGreen, // 0xff006400
    DarkGrey, // 0xffa9a9a9
    DarkKhaki, // 0xff6bb7bd
    DarkMagenta, // 0xff8b008b
    DarkOliveGreen, // 0xff2f6b55
    DarkOrange, // 0xff008cff
    DarkOrchid, // 0xffcc3299
    DarkRed, // 0xff00008b
    DarkSalmon, // 0xff7a96e9
    DarkSeaGreen, // 0xff8fbc8f
    DarkSlateBlue, // 0xff8b3d48
    DarkSlateGray, // 0xff4f4f2f
    DarkSlateGrey, // 0xff4f4f2f
    DarkTurquoise, // 0xffd1ce00
    DarkViolet, // 0xffd30094
    DeepPink, // 0xff9314ff
    DeepSkyBlue, // 0xffffbf00
    DimGray, // 0xff696969
    DimGrey, // 0xff696969
    DodgerBlue, // 0xffff901e
    FireBrick, // 0xff2222b2
    FloralWhite, // 0xfff0faff
    ForestGreen, // 0xff228b22
    Fuchsia, // 0xffff00ff
    Gainsboro, // 0xffdcdcdc
    GhostWhite, // 0xfffff8f8
    GoldenRod, // 0xff20a5da
    Gold, // 0xff00d7ff
    Gray, // 0xff808080
    Green, // 0xff008000
    GreenYellow, // 0xff2fffad
    Grey, // 0xff808080
    HoneyDew, // 0xfff0fff0
    HotPink, // 0xffb469ff
    IndianRed, // 0xff5c5ccd
    Indigo, // 0xff82004b
    Ivory, // 0xfff0ffff
    Khaki, // 0xff8ce6f0
    Lavenderblush, // 0xfff5f0ff
    Lavender, // 0xfffae6e6
    LawnGreen, // 0xff00fc7c
    Lemonchiffon, // 0xffcdfaff
    LightBlue, // 0xffe6d8ad
    LightCoral, // 0xff8080f0
    LightCyan, // 0xffffffe0
    LightGoldenRodYellow, // 0xffd2fafa
    LightGray, // 0xffd3d3d3
    LightGreen, // 0xff90ee90
    LightGrey, // 0xffd3d3d3
    LightPink, // 0xffc1b6ff
    LightSalmon, // 0xff7aa0ff
    LightSeaGreen, // 0xffaab220
    LightSkyBlue, // 0xffface87
    LightSlateGray, // 0xff998877
    //LightSlateGrey, // 0xff998877
    LightSteelBlue, // 0xffdec4b0
    LightYellow, // 0xffe0ffff
    Lime, // 0xff00ff00
    LimeGreen, // 0xff32cd32
    Linen, // 0xffe6f0fa
    Magenta, // 0xffff00ff
    Maroon, // 0xff000080
    MediumAquamarine, // 0xffaacd66
    MediumBlue, // 0xffcd0000
    MediumOrchid, // 0xffd355ba
    MediumPurple, // 0xffdb7093
    MediumSeaGreen, // 0xff71b33c
    MediumSlateBlue, // 0xffee687b
    MediumSpringGreen, // 0xff9afa00
    MediumTurquoise, // 0xffccd148
    MediumVioletRed, // 0xff8515c7
    MidnightBlue, // 0xff701919
    MintCream, // 0xfffafff5
    MistyRose, // 0xffe1e4ff
    Moccasin, // 0xffb5e4ff
    NavajoWhite, // 0xffaddeff
    Navy, // 0xff800000
    Oldlace, // 0xffe6f5fd
    Olive, // 0xff008080
    Olivedrab, // 0xff238e6b
    Orange, // 0xff00a5ff
    OrangeRed, // 0xff0045ff
    Orchid, // 0xffd670da
    PaleGoldenRod, // 0xffaae8ee
    PaleGreen, // 0xff98fb98
    PaleTurquoise, // 0xffeeeeaf
    PaleVioletRed, // 0xff9370db
    Papayawhip, // 0xffd5efff
    Peachpuff, // 0xffb9daff
    Peru, // 0xff3f85cd
    Pink, // 0xffcbc0ff
    Plum, // 0xffdda0dd
    PowderBlue, // 0xffe6e0b0
    Purple, // 0xff800080
    RebeccaPurple, // 0xff993366
    Red, // 0xff0000ff
    RosyBrown, // 0xff8f8fbc
    RoyalBlue, // 0xffe16941
    SaddleBrown, // 0xff13458b
    Salmon, // 0xff7280fa
    SandyBrown, // 0xff60a4f4
    SeaGreen, // 0xff578b2e
    SeaShell, // 0xffeef5ff
    Sienna, // 0xff2d52a0
    Silver, // 0xffc0c0c0
    SkyBlue, // 0xffebce87
    SlateBlue, // 0xffcd5a6a
    SlateGray, // 0xff908070
    SlateGrey, // 0xff908070
    Snow, // 0xfffafaff
    SpringGreen, // 0xff7fff00
    SteelBlue, // 0xffb48246
    Tan, // 0xff8cb4d2
    Teal, // 0xff808000
    Thistle, // 0xffd8bfd8
    Tomato, // 0xff4763ff
    Turquoise, // 0xffd0e040
    Violet, // 0xffee82ee
    Wheat, // 0xffb3def5
    White, // 0xffffffff
    WhiteSmoke, // 0xfff5f5f5
    Yellow, // 0xff00ffff
    YellowGreen, // 0xff32cd9a
    Num
};

enum class LPenStyle { // pen style
    No,
    Solid,
    Dash,
    Dot,
    DashDot,
    DashDotDot,
    CustomDash,
    MPenStyle = 0x0f
};

enum class LPenCapStyle{ // line endcap style
    Flat = 0x00,
    Square = 0x10,
    Round = 0x20,
    MPenStyle = 0x30
};

enum class LPenJoinStyle { // line join style
    Miter = 0x00,
    Bevel = 0x40,
    Round = 0x80,
    SvgMiter = 0x100,
    MPenStyle = 0x1c0
};


enum class LTextFlag {
    //align flags
    None = 0x0000,
    Left = 0x0001,
    Leading = Left,
    Right = 0x0002,
    Trailing = Right,
    HCenter = 0x0004,
    Justify = 0x0008,
    Absolute = 0x0010,
    LeftRightMask = Left | Right,
    HorizontalMask = Left | Right | HCenter | Justify | Absolute,
    Top = 0x0020,
    Bottom = 0x0040,
    VCenter = 0x0080,
    Baseline = 0x0100,
    // Note that 0x100 will clash with TextSingleLine = 0x100 due to what the comment above
    // this enum declaration states. However, since Baseline is only used by layouts,
    // it doesn't make sense to pass Baseline to LPainter::drawText(), so there
    // shouldn't really be any ambiguity between the two overlapping enum values.
    TopBottomMask = Top|Bottom,
    VerticalMask = Top | Bottom | VCenter | Baseline,
    Center = VCenter | HCenter,
    //Text flags
    SingleLine = Baseline,
    DontClip = 0x0200,
    ExpandTabs = 0x0400,
    ShowMnemonic = 0x0800,
    WordWrap = 0x1000,
    WrapAnywhere = 0x2000,
    DontPrint = 0x4000,
    IncludeTrailingSpaces = 0x08000000,
    HideMnemonic = 0x8000,
    JustificationForced = 0x10000,
    ForceLeftToRight = 0x20000,
    ForceRightToLeft = 0x40000,
    // Ensures that the longest variant is always used when computing the
    // size of a multi-variant string.
    LongestVariant = 0x80000
};
L_EN_FLAGS(LTextFlag)

typedef LTextFlag LAlignment;

enum class LMoveOper  : uint8_t{
    NoMove,

    Start,
    Up,
    StartOfLine,
    StartOfBlock,
    StartOfWord,
    PreviousBlock,
    PreviousCharacter,
    PreviousWord,
    Left,
    WordLeft,

    End,
    Down,
    EndOfLine,
    EndOfWord,
    EndOfBlock,
    NextBlock,
    NextCharacter,
    NextWord,
    Right,
    WordRight,

    NextCell,
    PreviousCell,
    NextRow,
    PreviousRow
};

enum class LMoveMode : uint8_t{
    MoveAnchor,
    KeepAnchor
};

enum class LSizeMode : uint8_t {
    Absolute,
    Relative
};

enum class LCursorShape : uint8_t{
    Arrow,
    UpArrow,
    Cross,
    Wait,
    IBeam,
    SizeVer,
    SizeHor,
    SizeBDiag,
    SizeFDiag,
    SizeAll,
    Blank,
    PointingHand,
    Forbidden,
    WhatsThis,
    Busy,
    OpenHand,
    ClosedHand,
    DragCopy,
    DragMove,
    DragLink,
    Bitmap,
    Custom,
    Last = Custom,
};

enum class LAxis : uint8_t{
    X,
    Y,
    Z,
    T
};

enum class LDirection : uint8_t{
    ToTop,
    ToRight,
    ToBottom,
    ToLeft
};

enum class LCorner : uint8_t{
    TopLeft,
    TopRight,
    BottomRight,
    BottomLeft,
};

enum class LTileRule : uint8_t {
    Stretch,
    Repeat,
    Round,
    Space
};

enum class LShortcutCtxType : uint8_t{
    Widget,
    Window,
    Application,
    WidgetWithChildren
};

enum class LScrollPolicy : uint8_t {
    AsNeeded,
    AlwaysOff,
    AlwaysOn
};


enum class LPathOption : uint8_t
{
    ShowDirsOnly                = 0x00000001,
    DontResolveSymlinks         = 0x00000002,
    DontConfirmOverwrite        = 0x00000004,
    DontUseNativeDialog         = 0x00000008,
    ReadOnly                    = 0x00000010,
    HideNameFilterDetails       = 0x00000020,
    DontUseCustomDirectoryIcons = 0x00000040
};
L_EN_FLAGS(LPathOption)


enum class LPathFilter : uint16_t{
  Dirs        = 0x001,
  Files       = 0x002,
  Drives      = 0x004,
  NoSymLinks  = 0x008,
  AllEntries  = Dirs | Files | Drives,
  TypeMask    = 0x00f,

  Readable    = 0x010,
  Writable    = 0x020,
  Executable  = 0x040,
  PermissionMask    = 0x070,

  Modified    = 0x080,
  Hidden      = 0x100,
  System      = 0x200,

  AccessMask  = 0x3F0,

  AllDirs       = 0x400,
  CaseSensitive = 0x800,
  NoDot         = 0x2000,
  NoDotDot      = 0x4000,
  NoDotAndDotDot = NoDot | NoDotDot,

  NoFilter = 0
};
L_EN_FLAGS(LPathFilter)

LK_NAMESPACE_END

#endif // LENUMS_H
