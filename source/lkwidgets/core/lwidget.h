#ifndef LWIDGET_H
#define LWIDGET_H

#include "lkcore/ltreeobj.h"
#include "lkgui/paint/lpaintdevice.h"
#include "lkgui/paint/lpainter.h"
#include "lwidgetdata.h"
#include "lkgui/lwindow.h"
#include "lkwidgets/core/lwidgetrepaintman.h"
#include "lkwidgets/core/llayout.h"
#include "lkgui/lguievent.h"
#include "lkwidgets/core/lsizepolicy.h"
#include "lkgui/text/lfontmetrics.h"
#include "lkwidgets/draw/lstyleopt.h"

LK_NAMESPACE_BEGIN


enum class LDrawWidgetOpt : uint8_t{
    DrawAsRoot = 0x01,
    DrawPaintOnScreen = 0x02,
    DrawRecursive = 0x04,
    DrawInvisible = 0x08,
    DontSubtractOpaqueChildren = 0x10,
    DontDrawOpaqueChildren = 0x20,
    DontDrawNativeChildren = 0x40,
    DontSetCompositionMode = 0x80
};

L_EN_FLAGS(LDrawWidgetOpt)

enum class LCloseMode {
    NoEvent,
    WithEvent,
    WithSpontaneousEvent
};

class LApplication;
class LWidgetItem;
class LMenu;
class LDialog;
class LKWIDGETSDEF LWidget : public LTreeObj
{
public:
    _LIVarTag(LWidget)

    LWidget(std::shared_ptr<LWidget> parent = nullptr, LFlags<LWindowType> f = 0);
    virtual ~LWidget();


    inline LWindowType windowType() const
    {
        return LWindowType(uint32_t(m_widgetData.windowFlags & LWindowType::WindowTypeMask));
    }
    inline  LFlags<LWindowType> windowFlags() const
    {
        return m_widgetData.windowFlags;
    }

    void setWindowFlags(LFlags<LWindowType> flags);
    void setWindowFlag(LWindowType, bool on = true);

    bool testAttribute(LWidgetAttribute pos) const;
    void setAttribute(LWidgetAttribute pos, bool on = true);

    virtual void setName(const std::string& v) override;


    inline bool isEnabled() const
    {
        return !testAttribute(LWidgetAttribute::Disabled);
    }

    void setEnabled(bool);

    inline bool isVisible() const
    {
        return testAttribute(LWidgetAttribute::WStateVisible);
    }

    inline bool isHidden() const
    {
        return testAttribute(LWidgetAttribute::WStateHidden);
    }

    bool isVisibleTo(const std::shared_ptr<LWidget> ancestor) const;

    inline bool updatesEnabled() const{
        return !testAttribute(LWidgetAttribute::UpdatesDisabled);
    }
    void setUpdatesEnabled(bool enable);
    

    inline LFlags<LWindowState> windowState() const{
        return LFlags<LWindowState>(m_widgetData.windowState);
    }

    void setWindowState(LFlags<LWindowState> state);


    inline bool isMaximized() const
    { return m_widgetData.windowState & uint32_t(LWindowState::Maximized); }

    inline bool isMinimized() const
    { return m_widgetData.windowState & uint32_t(LWindowState::Minimized); }

    inline bool isFullScreen() const
    { return m_widgetData.windowState & uint32_t(LWindowState::FullScreen); }

    bool isWindow() const {
        return uint32_t(windowType()) & uint32_t(LWindowType::Window);
    }

    virtual bool isWidgetType() const override{
        return true;
    }

    std::shared_ptr<LWidget> window() const;
    std::shared_ptr<LWidget> nativeParentWidget() const;

    bool hasWindowContainer();

    void show();
    virtual void setVisible(bool visible);
    void setHidden(bool hidden);
    void showIfNotHidden();

    void showMinimized();
    void showMaximized();
    void showFullScreen();
    void showNormal();

    bool close();
    void raise();
    void lower();

    void adjustSize();
    bool isActiveWindow() const;
    void activateWindow();

    enum class WinHandleMode {
        Direct,
        Closest,
        TopLevel
    };

    std::shared_ptr<LWindow> windowHandle(WinHandleMode mode = WinHandleMode::Direct) const;

    LRect<int> frameGeometry() const;
    inline const LRect<int> &geometry() const
    { return m_widgetData.crect; }

    void setGeometry(const LRect<int> &);

    inline void setGeometry(int ax, int ay, int aw, int ah)
    { setGeometry(LRect<int>(ax, ay, aw, ah, false)); }

    void move(const LPoint<int> &);
    inline void move(int x, int y){
        move(LPoint<int>(x, y));
    }

    inline LRect<int> rect() const
    { return LRect<int>(0,0,m_widgetData.crect.m_width, m_widgetData.crect.m_height, false); }

    inline LRect<double> rectF() const
    { return LRect<double>(0,0,m_widgetData.crect.m_width, m_widgetData.crect.m_height, false); }

    inline LSize<int> size() const
    { return LSize<int>(m_widgetData.crect.m_width, m_widgetData.crect.m_height); }

    inline LSize<double> sizeF() const
    { return LSize<double>(m_widgetData.crect.m_width, m_widgetData.crect.m_height); }

    LMargins<int> contentsMargins() const;
    LRect<int> contentsRect() const;
    virtual LRect<int> customRect() const; //Rect used for childrens, for group box, custom rect different with contentsRect

    void setContentsMargins(int left, int top, int right, int bottom);
    inline void setContentsMargins(const LMargins<int> &m){
        setContentsMargins(m.m_left, m.m_top, m.m_right, m.m_bottom);
    }

    LPoint<int> mapToGlobal(const LPoint<int> &pos) const;
    LPoint<int> mapFromGlobal(const LPoint<int> &pos) const;
    LPoint<int> mapToParent(const LPoint<int> &pos) const;
    LPoint<int> mapFromParent(const LPoint<int> &pos) const;
    LPoint<int> mapTo(const std::shared_ptr<LWidget> parent, const LPoint<int> &pos) const;
    LPoint<int> mapFrom(const std::shared_ptr<LWidget> parent, const LPoint<int> &pos) const;

    inline void setMouseTracking(bool enable){
        setAttribute(LWidgetAttribute::MouseTracking, enable);
    }
    bool hasMouseTracking() const{
        return testAttribute(LWidgetAttribute::MouseTracking);
    }
    bool underMouse() const{
        return testAttribute(LWidgetAttribute::UnderMouse);
    }

    virtual bool handleEvent(std::shared_ptr<LEvent> event) override;
    LPoint<int> pos() const;

    LSize<int> minimumSize() const;
    LSize<int> maximumSize() const;
    int minimumWidth() const;
    int minimumHeight() const;
    int maximumWidth() const;
    int maximumHeight() const;
    inline void setMinimumSize(const LSize<int> &s){
        setMinimumSize(s.m_x, s.m_y);
    }
    void setMinimumSize(int minw, int minh);
    inline void setMaximumSize(const LSize<int> &s){
        setMaximumSize(s.m_x, s.m_y);
    }
    void setMaximumSize(int maxw, int maxh);
    void setMinimumWidth(int minw);
    void setMinimumHeight(int minh);
    void setMaximumWidth(int maxw);
    void setMaximumHeight(int maxh);

    void setFixedSize(const LSize<int> &s);
    void setFixedSize(int w, int h);
    void setFixedWidth(int w);
    void setFixedHeight(int h);

    LSize<int> sizeIncrement() const;
    inline void setSizeIncrement(const LSize<int> &s){
        setSizeIncrement(s.m_x, s.m_y);
    }
    void setSizeIncrement(int w, int h);
    LSize<int> baseSize() const;
    inline void setBaseSize(const LSize<int> &s){
        setBaseSize(s.m_x, s.m_y);
    }
    void setBaseSize(int basew, int baseh);


    inline void resize(int w, int h)
    { resize(LSize<int>(w, h)); }

    void resize(const LSize<int> &s);

    const LSizePolicy& sizePolicy() const;
    virtual void setSizePolicy(LSizePolicy sp);

    inline WId internalWinId() const { return m_widgetData.winId; }
    WId winId() const;
    WId effectiveWinId() const;
    void createWinId(); // internal, going away

    void setWindowIcon(std::shared_ptr<LTexture> icon);
    std::shared_ptr<LTexture> windowIcon() const;

    inline bool isModal() const
    { return m_widgetData.windowModality != (uint32_t)LWindowModality::NonModal; }

    LWindowModality windowModality() const;
    void setWindowModality(LWindowModality windowModality);

    virtual void setParent(std::shared_ptr<LTreeObj> parent) override;
    void setParent(std::shared_ptr<LWidget> parent, LFlags<LWindowType> f);

    std::shared_ptr<LDialog> dialogParent() const;

    bool isAncestorOf(std::shared_ptr<LWidget> child) const;
    void clearFocus();
    bool hasFocus() const;
    void setFocus(LFocusReason reason = LFocusReason::Other);
    std::shared_ptr<LWidget> focusWidget() const;
    std::shared_ptr<LWidget> nextInFocusChain() const;
    std::shared_ptr<LWidget> previousInFocusChain() const;

    virtual std::shared_ptr<LObject> focusObject();

    std::shared_ptr<LWidget> childAt(const LPoint<int>& pos) const;

    LFocusPolicy focusPolicy() const;
    void setFocusPolicy(LFocusPolicy policy);

    static std::shared_ptr<LWidget> find(WId id);

    void setWindowTitle(const std::wstring &title);
    std::wstring windowTitle() const;

    void setFocusProxy(std::shared_ptr<LWidget> w);
    std::shared_ptr<LWidget> focusProxy() const;

    void update();
    void update(const LRect<int>&);
    void update(const std::shared_ptr<LRegion> region);

    void repaint();
    void repaint(const LRect<int>& r);
    void repaint(const std::shared_ptr<LRegion> region);

    bool autoFillBackground() const;
    void setAutoFillBackground(bool enabled);

    void setBackgroundRole(LPalette::ColorRole role);
    LPalette::ColorRole backgroundRole() const;

    void setForegroundRole(LPalette::ColorRole role);
    LPalette::ColorRole foregroundRole() const;


    int x() const;
    int y() const;
    inline int width() const{
        return m_widgetData.crect.width();
    }
    int height() const{
        return m_widgetData.crect.height();
    }

    virtual int heightForWidth(int) const;
    virtual bool hasHeightForWidth() const;
    virtual LSize<int> sizeHint() const;
    virtual LSize<int> minimumSizeHint() const;

    std::shared_ptr<LPalette> palette() const;
    LPalette::ColorGroup colorGroup();

    inline std::shared_ptr<LLayout> layout(){
        return m_layout;
    }
    void setLayout(std::shared_ptr<LLayout> l);
    void updateGeometry();

    void setLayoutDirection(LLayoutDirection direction);
    LLayoutDirection layoutDirection() const;
    void unsetLayoutDirection();
    inline bool isRightToLeft() const { return layoutDirection() == LLayoutDirection::RightToLeft; }
    inline bool isLeftToRight() const { return layoutDirection() == LLayoutDirection::LeftToRight; }

    // GUI style setting
    virtual std::shared_ptr<LStyle> style() const;
    void setStyleSheet(const std::string& styleSheet, bool isRaw = false, bool shieldParent = true);
    void setStyleSheet(const std::shared_ptr<LWidgetStyleSheet> styleSheet);
    const std::shared_ptr<LWidgetStyleSheet> styleSheet();

    std::shared_ptr<LFont> font() const;
    void setFont(std::shared_ptr<LFont> font);

    inline LFontMetrics fontMetrics() const
    { return LFontMetrics(font()); }

    void grabMouse(const LCursor& c);
    void releaseMouse();

    static std::shared_ptr<LWidget> mouseGrabber();
    static std::shared_ptr<LWidget> keyboardGrabber();

    LCursor cursor() const;
    void setCursor(const LCursor &);
    void unsetCursor();
    virtual void blink(){}

    bool acceptDrops() const;
    void setAcceptDrops(bool on);

    double dpi() const;
    double dpiScaled(double value) const;

    std::shared_ptr<LScreen> screen() const;
    bool setScreen(std::shared_ptr<LScreen> s);
    bool setScreenForPoint(const LPoint<int> &pos);

    virtual LVariant inputMethodQuery(LInputMethodQuery, const LVariant& args = LVariant()) const override;
    LFlags<LInputMethodHint> inputMethodHints() const;
    void setInputMethodHints(LFlags<LInputMethodHint> hints);
    void setPaintLayout(bool isPaint);

    inline const LWidgetStyle& elementStyle() const{
        return m_elementStyle;
    }

    LRect<int> childrenRect() const;
    std::shared_ptr<LRegion> childrenRegion() const;

    void setMask(std::shared_ptr<LRegion> r);
    std::shared_ptr<LRegion> mask() const;
    void clearMask();
protected:
    LWidgetData m_widgetData;
    std::unique_ptr<LWidgetExtra> m_pExtraData;
    std::shared_ptr<LLayout> m_layout = nullptr;
    std::weak_ptr<LWidget> m_focusChild;
    std::weak_ptr<LWidget> m_focusNext;
    std::weak_ptr<LWidget> m_focusPrev;

    LWidgetStyle m_elementStyle;

    union{
        uint32_t m_flags = 0;
        struct{
            uint32_t m_dirtyOpaqueChildren : 1;
            uint32_t m_isOpaque : 1;
            uint32_t m_retainSizeWhenHiddenChanged : 1;
            uint32_t m_inDirtyList : 1;
            uint32_t m_isScrolled : 1;
            uint32_t m_isMoved : 1;
            uint32_t m_usesDoubleBufferedGLContext : 1;
            uint32_t m_mustHaveWindowHandle : 1;
            uint32_t m_renderToTexture : 1;
            uint32_t m_textureChildSeen : 1;
            uint32_t m_childrenHiddenByWState : 1;
            uint32_t m_childrenShownByExpose : 1;
            uint32_t m_mouseGrabWithCursor : 1;
            uint32_t m_inheritsInputMethodHints :1;
            uint32_t m_paintLayout: 1;
        };
    };

    uint32_t m_directFontResolveMask = 0;
    uint32_t m_inheritedFontResolveMask = 0;
    uint32_t m_directPaletteResolveMask = 0;
    uint32_t m_inheritedPaletteResolveMask = 0;
    std::shared_ptr<LRegion> m_dirty = nullptr;
    std::shared_ptr<LRegion> m_needsFlush = nullptr;
    std::shared_ptr<LRegion> m_opaqueChildren = nullptr;
    std::shared_ptr<LPaintDevice> m_redirectDev;

    LPalette::ColorRole m_fgRole = LPalette::NoRole;
    LPalette::ColorRole m_bgRole = LPalette::NoRole;

    LSizePolicy m_sizePolicy = LSizePolicy(LSizePolicy::Preferred, LSizePolicy::Preferred);

    short m_leftMargin = 0;
    short m_topMargin = 0;
    short m_rightMargin = 0;
    short m_bottomMargin = 0;
    LFlags<LInputMethodHint> m_imHints;

    void initPainter(LPainter *painter) const;
    void initWidget(std::shared_ptr<LWidget> parent = nullptr, LFlags<LWindowType> f = 0);
    virtual void initSpecific();
    void setLayoutDirectionHelper(LLayoutDirection direction);
    void create(WId id= 0);
    virtual void createUserExtra(){}

    static void adjustFlags(LFlags<LWindowType>& flags, std::shared_ptr<LWidget> w);

    virtual bool focusNextPrevChild(bool next);
    inline bool focusNextChild() { return focusNextPrevChild(true); }
    inline bool focusPreviousChild() { return focusNextPrevChild(false); }

    virtual void mousePressEvent(std::shared_ptr<LMouseEvent> event);
    virtual void mouseReleaseEvent(std::shared_ptr<LMouseEvent> event);
    virtual void mouseDoubleClickEvent(std::shared_ptr<LMouseEvent> event);
    virtual void mouseMoveEvent(std::shared_ptr<LMouseEvent> event);
    virtual void wheelEvent(std::shared_ptr<LWheelEvent> event);

    virtual void keyPressEvent(std::shared_ptr<LKeyEvent> event);
    virtual void keyReleaseEvent(std::shared_ptr<LKeyEvent>  event);
    virtual void focusInEvent(std::shared_ptr<LFocusEvent> event);
    virtual void focusOutEvent(std::shared_ptr<LFocusEvent> event);
    virtual void enterEvent(std::shared_ptr<LEvent> event);
    virtual void leaveEvent(std::shared_ptr<LEvent> event);
    virtual void paintEvent(std::shared_ptr<LPaintEvent> event);
    virtual void moveEvent(std::shared_ptr<LMoveEvent> event);
    virtual void resizeEvent(std::shared_ptr<LResizeEvent> event);
    virtual void closeEvent(std::shared_ptr<LEvent> event);
    virtual void contextMenuEvent(std::shared_ptr<LContextMenuEvent> e);

    virtual void showEvent(std::shared_ptr<LEvent> event);
    virtual void hideEvent(std::shared_ptr<LEvent> event);
    virtual void changeEvent(std::shared_ptr<LEvent> event);

    virtual void inputMethodEvent(std::shared_ptr<LInputMethodEvent> event);
    virtual void updateElementStyle(const LStyleOpt &opt);
    virtual bool enterSubItem(const LPoint<int>&);


    virtual void beginBackingStorePainting() { }
    virtual void endBackingStorePainting() { }
    inline std::shared_ptr<LPaintDevice> paintDevice(){
        return m_redirectDev;
    }

    virtual void initStyleOpt(LStyleOpt& opt) const;
    virtual uint16_t uiElementState(LUiElementType type = LUiElementType::None) const ;

    bool closeHelper(LCloseMode mode);

    std::list<std::shared_ptr<LWidgetStyleSheet> > effectStyleSheets();
    bool clearCachedStyle();
    void setCursorSys(bool isForce);

    inline void handleSoftwareInputPanel(LMouseButton button, bool clickCausedFocus)
    {
        if (button == LMouseButton::Left){
            handleSoftwareInputPanel(clickCausedFocus);
        }
    }

    void handleSoftwareInputPanel(bool clickCausedFocus = false);
    int boundWidth(LDirection dir = LDirection::ToLeft, int flags = LRenderRule::All) const;
    LMargins<int> boundWidths(int flags = LRenderRule::All) const;
private:    
    void initDefaultQuitOnCloseAttribute();
    void createTLExtra();
    void createExtra();
    void createRecursively();
    void createInner();
    void createTLSysExtra();
    void destroy(bool destroyWindow = true,
                 bool destroySubWindows = true);
    std::shared_ptr<LLayout> takeLayout();

    void updateIsOpaque();
    void setOpaque(bool opaque);
    void updateIsTranslucent();
    std::shared_ptr<LWidget> deepestFocusProxy() const;
    void updateFocusChild();
    bool updateBrushOrigin(LPainter *, const std::shared_ptr<LBrush> brush) const;

    void setWindowIconSys();
    void setWindowIconHelper();
    void setWindowTitleHelper(const std::wstring &t);

    void setWindowTitleSys(const std::wstring &caption);
    void setModalSys();
    void setFocusSys();
    void setParentSys(std::shared_ptr<LWidget> parent, LFlags<LWindowType> f);
    void raiseSys();
    void lowerSys();


    void reparentFocusWidgets(std::shared_ptr<LWidget> oldtlw);
    void sendPendingMoveAndResizeEvents(bool recursive = false, bool disableUpdates = false);
    void showChildren(bool spontaneous);
    void hideChildren(bool spontaneous);
    void showRecursive();

    LMargins<int> frameStrut() const;
    void updateFrameStrut();
    void setGeometrySys(int x, int y, int w, int h, bool isMove);
    void setDirtyOpaqueRegion();

    void moveRect(const LRect<int> &rect, int dx, int dy);
    std::shared_ptr<LRegion> overlappedRegion(const LRect<int> &rect, bool breakAfterFirst = false) const;

    void showSys();
    void showHelper();
    void hideSys();
    void hideHelper();
    void updateGeometryHelper(bool forceUpdate);
    void setEnabledHelper(bool enable);
    void setUpdatesEnabledHelper(bool enable);
    bool setMinimumSizeHelper(int &minw, int &minh);
    bool setMaximumSizeHelper(int &maxw, int &maxh);
    void invalidateBackingStoreResizeHelper(const LPoint<int> &oldPos, const LSize<int> &oldSize);

    void setConstraintsSys();
    void fixPosIncludesFrame();

    void setMaskSys(std::shared_ptr<LRegion> mask);
    void setWindowModifiedHelper();
    void setWinId(WId);

    void resolveFont();
    void resolvePalette();
    void resolveLayoutDirection();
    void resolveLocale();

    void inheritStyle();
    bool shouldDiscardSyncRequest() const;
    bool shouldPaintOnScreen() const;
    void paintOnScreen(const std::shared_ptr<LRegion> region);

    void updateWidgetTransform(std::shared_ptr<LEvent> event);
    void clipToEffectiveMask(std::shared_ptr<LRegion> region) const;

    void syncBackingStore();
    void syncBackingStore(const std::shared_ptr<LRegion> region);

    std::shared_ptr<LBackingStore> backingStore() const;
    void setBackingStore(std::shared_ptr<LBackingStore> store);

    LRect<int> clipRect() const;
    std::shared_ptr<LRegion> clipRegion() const;
    std::shared_ptr<LRegion> getOpaqueChildren();

    void sendPaintEvent(const LRect<int>& toBePainted);
    void sendPaintEvent(const std::shared_ptr<LRegion> toBePainted);
    virtual std::shared_ptr<LPaintDevice> paintScreenDevice();

    void paintSiblingsRecursive(std::shared_ptr<LPaintDevice> pdev,
                                const std::list<std::shared_ptr<LTreeObj> >& siblings,
                                int index,
                                const std::shared_ptr<LRegion> rgn,
                                const LPoint<int> &offset,
                                LFlags<LDrawWidgetOpt> flags,
                                LPainter *sharedPainter,
                                LWidgetRepaintMan *repaintManager);

    void drawWidget(std::shared_ptr<LPaintDevice> pdev,
                    std::shared_ptr<LRegion> rgn,
                    const LPoint<int> &offset,
                    LFlags<LDrawWidgetOpt> flags,
                    LPainter *sharedPainter = nullptr,
                    LWidgetRepaintMan *repaintManager = nullptr);

    void paintBackground(LPainter *painter, std::shared_ptr<LRegion> rgn, LFlags<LDrawWidgetOpt> flags = LDrawWidgetOpt::DrawAsRoot) const;

    void subtractOpaqueChildren(std::shared_ptr<LRegion> rgn, const LRect<int> &clipRect) const;
    void subtractOpaqueSiblings(std::shared_ptr<LRegion> source,
                                bool *hasDirtySiblingsAbove = nullptr,
                                bool alsoNonOpaque = false) const;

    void deactivateWidgetCleanup();
    void updateContentsRect();

    bool pointInsideRectAndMask(const LPoint<int> &p) const;
    std::shared_ptr<LWidget> childAtRecursiveHelper(const LPoint<int> &p) const;

    void enforceNativeChildren();
    inline bool nativeChildrenForced() const
    {
        if(m_pExtraData != nullptr){
            return m_pExtraData->nativeChildrenForced;
        }
        return false;
    }

    inline LWidgetExtra *extraData() const
    {
        return m_pExtraData.get();
    }

    inline LTLWExtra *topData() const
    {
        const_cast<LWidget*>(this)->createTLExtra();
        return m_pExtraData->topExtra.get();
    }

    inline LTLWExtra *maybeTopData() const
    {
        return m_pExtraData ? m_pExtraData->topExtra.get() : nullptr;
    }

    inline LWidgetRepaintMan *maybeRepaintManager() const{
        LTLWExtra *x = maybeTopData();
        return x ? x->repaintMan.get() : nullptr;
    }

    void setVisibleInner(bool isVisible);
    void invalidateBackingStore(const LRect<int> &r);
    void invalidateBackingStore(const std::shared_ptr<LRegion> r);   

    inline bool isDirtyEmpty(){
        if(m_dirty == nullptr){
            return true;
        }
        return m_dirty->isEmpty();
    }

    LWidgetRepaintMan *maybeRepaintMan() const;

    void setRedirected(std::shared_ptr<LPaintDevice> replacement, const LPoint<int> &offset);
    void restoreRedirected();

    void setSystemClip(std::shared_ptr<LPaintEngine> paintEngine,
                       double devicePixelRatio,
                       const std::shared_ptr<LRegion> region);


    bool stealKeyboardGrab(bool grab);
    bool stealMouseGrab(bool grab);
    LSize<int> adjustedSize() const;

    static std::weak_ptr<LWidget> m_mouseGrab;
    LCursor m_mouseGrabBackCursor;

    static std::weak_ptr<LWidget> m_keyboardGrab;

    friend class LApplication;
    friend class LWidgetRepaintMan;
    friend class LWidgetWindow;
    friend class LWidgetItem;
    friend class LLayout;
    friend class LStyle;
    friend class LWindowContainer;
    friend class LWidgetStyle;
};


#define LWIDGETSIZE_MAX ((1<<24)-1)

LK_NAMESPACE_END
#endif // LWIDGET_H
