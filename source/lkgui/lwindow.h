#ifndef LWINDOW_H
#define LWINDOW_H

#include "lkcore/ltreeobj.h"
#include "lkcore/lflags.h"
#include "lkgui/lsurface.h"
#include "lkgui/lrect.h"
#include "lkgui/lguienums.h"
#include "lkgui/image/ltexture.h"
#include "lkgui/paint/lregion.h"
#include "lkcore/time/lelapsedtimer.h"
#include "lkgui/lcursor.h"
#include "lkgui/lguievent.h"
#include "lkcore/lsignal.h"
#include <vector>

LK_NAMESPACE_BEGIN

#define LWINDOWSIZE_MAX ((1<<24)-1)

class LScreen;
class LPlatformWindow;
class LGuiApplication;
class LKGUIDEF LWindow : public LObject, public LSurface
{
public:
    LWindow();
    virtual ~LWindow();

    static std::shared_ptr<LWindow> fromWinId(WId id);

    virtual bool isWindowType() const override{
        return true;
    }

    enum PositionPolicy
    {
        WindowFrameInclusive,
        WindowFrameExclusive
    };

    enum Visibility {
        Hidden = 0,
        AutomaticVisibility,
        Windowed,
        Minimized,
        Maximized,
        FullScreen
    };

    enum AncestorMode {
        ExcludeTransients,
        IncludeTransients
    };

    inline void setFlags(LFlags<LWindowType> flags){
        m_windowFlags = flags;
    }

    void setFlag(LWindowType flag, bool on = true);
    inline LFlags<LWindowType> flags() const{
        return m_windowFlags;
    }

    inline LWindowType type() const{
        return static_cast<LWindowType>((m_windowFlags & LWindowType::WindowTypeMask).value());
    }

    inline PositionPolicy positionPolicy() const{
        return m_positionPolicy;
    }

    inline void setPositionPolicy(PositionPolicy policy){
        m_positionPolicy = policy;
    }

    inline std::shared_ptr<LWindow> parent() const
    {
        return m_parentWindow.lock();
    }

    std::shared_ptr<LWindow> parent(AncestorMode mode) const;
    void setParent(std::shared_ptr<LWindow> parent);

    std::shared_ptr<LScreen> screen() const;
    void setScreen(std::shared_ptr<LScreen> screen);

    std::shared_ptr<LPlatformWindow> handle() const;

    inline bool visible(){
        return m_visible;
    }

    void setVisible(bool isVisible);
    void create();

    bool isExposed() const;

    bool isTopLevel() const;
    WId winId();
    std::shared_ptr<LWindow> topLevelWindow(LWindow::AncestorMode mode = LWindow::IncludeTransients) const;


    bool isActive() const;
    inline bool isModal() const{
        return m_modality  != LWindowModality::NonModal;
    }
    inline LWindowModality modality() const{
        return m_modality;
    }
    void setModality(LWindowModality modality);

    const LRect<int>  geometry() const;

    LMargins<int> frameMargins() const;
    LRect<int>  frameGeometry() const;

    LPoint<int> framePosition() const;
    void setFramePosition(const LPoint<int> &point);

    inline int width() const { return geometry().width(); }
    inline int height() const { return geometry().height(); }
    inline int x() const { return geometry().m_x; }
    inline int y() const { return geometry().m_y; }

    LSize<int> size() const override { return geometry().size(); }
    inline LPoint<int> position() const { return geometry().topLeft(); }

    void setPosition(const LPoint<int> &pt);
    void setPosition(int posx, int posy);

    void setX(int arg);
    void setY(int arg);
    void setWidth(int arg);
    void setHeight(int arg);

    void setGeometry(const LRect<int> &rect);
    inline void setGeometry(int posx, int posy, int w, int h){
        setGeometry(LRect<int>(posx, posy, w, h, false));
    }

    bool close();
    void raise();
    void lower();

    LWindowState windowState() const;
    LFlags<LWindowState> windowStates() const;
    void setWindowState(LWindowState state);
    void setWindowStates(LFlags<LWindowState> states);

    void setTransientParent(std::shared_ptr<LWindow> parent);
    std::shared_ptr<LWindow> transientParent() const;

    bool isAncestorOf(const std::shared_ptr<LWindow> child, AncestorMode mode = IncludeTransients) const;

    inline bool blockedByModalWindow(){
        return m_blockedByModalWindow;
    }
    void inline setBlockedByModalWindow(bool isBlock){
        m_blockedByModalWindow = isBlock;
    }

    std::vector<std::weak_ptr<LWindow> >& children(){
        return m_children;
    }

    inline bool compositing() const{
        return m_compositing;
    }

    inline const LElapsedTimer& lastComposeTime() const{
        return m_lastComposeTime;
    }

    inline int minimumWidth() const { return m_minimumSize.width(); }
    inline int minimumHeight() const { return m_minimumSize.height(); }
    inline int maximumWidth() const { return m_maximumSize.width(); }
    inline int maximumHeight() const { return m_maximumSize.height(); }

    inline void setMinimumWidth(int w){
        m_minimumSize.setWidth(w);
    }
    inline void setMinimumHeight(int h){
        m_minimumSize.setHeight(h);
    }
    inline void setMaximumWidth(int w){
        m_maximumSize.setWidth(w);
    }
    inline void setMaximumHeight(int h){
        m_maximumSize.setHeight(h);
    }

    inline const LSize<int>& minimumSize() const{
        return m_minimumSize;
    }
    inline const LSize<int>& maximumSize() const{
        return m_maximumSize;
    }

    inline void setMinimumSize(const LSize<int> &size){
        m_minimumSize = size;
    }
    inline void setMaximumSize(const LSize<int> &size){
        m_maximumSize = size;
    }

    void setOpacity(double level);
    double opacity() const;


    inline const LSize<int>& baseSize() const{
        return m_baseSize;
    }

    inline const LSize<int>& sizeIncrement() const{
        return m_sizeIncrement;
    }

    inline void setBaseSize(const LSize<int> &v){
        m_baseSize = v;
    }
    inline void setSizeIncrement(const LSize<int> &v){
        m_sizeIncrement = v;
    }

    inline bool resizeAutomatic(){
        return m_resizeAutomatic;
    }
    inline void setResizeAutomatic(bool isAuto){
        m_resizeAutomatic = isAuto;
    }

    virtual std::shared_ptr<LObject> focusObject(){
        return objPtr();
    }

    inline std::shared_ptr<LPlatformWindow> platformWindow(){
        return m_platformWindow;
    }

    inline std::wstring title() const{
        return m_title;
    }

    void setTitle(const std::wstring& title);

    inline bool isResizeEventPending(){
        return m_resizeEventPending;
    }

    LPoint<int> mapToGlobal(const LPoint<int> &pos) const;
    LPoint<int> mapFromGlobal(const LPoint<int> &pos) const;

    LPoint<int> globalPosition() const;

    void setIcon(const std::shared_ptr<LTexture> icon);
    std::shared_ptr<LTexture> icon() const;

    void requestActivate();
    static LWindowState effectiveState(LFlags<LWindowState> state);


    void setFormat(const LSurfaceFormat &format);
    LSurfaceFormat format() const override;
    LSurfaceFormat requestedFormat() const;

    void setSurfaceType(SurfaceType surfaceType);
    SurfaceType surfaceType() const override;

    void setMask(std::shared_ptr<LRegion> region);
    std::shared_ptr<LRegion> mask() const;

    inline bool positionAutomatic(){
        return m_positionAutomatic;
    }

    void resize(const LSize<int> &newSize);
    void destroy();

    double devicePixelRatio() const;

    bool isPopup() const { return (m_windowFlags & LWindowType::WindowTypeMask) == LWindowType::Popup; }
    bool setKeyboardGrabEnabled(bool grab);
    bool setMouseGrabEnabled(bool grab);

    virtual void init() override;
    virtual bool nativeEvent(const std::string &eventType, void *message, lintptr *result);
    virtual LRect<double> closestAcceptableGeometry(const LRect<double> &rect) const;
    virtual bool isWidgetWindow();

    const LCursor& cursor() const;
    void setCursor(const LCursor &);
    void unsetCursor();
protected:
    LFlags<LWindowType> m_windowFlags = LWindowType::Window;
    std::weak_ptr<LWindow> m_parentWindow;
    PositionPolicy m_positionPolicy = WindowFrameExclusive;
    std::weak_ptr<LScreen> m_topLevelScreen;
    std::shared_ptr<LPlatformWindow> m_platformWindow;
    bool m_visible= false;
    Visibility m_visibility;
    LFlags<LWindowState> m_windowState = LWindowState::NoState;
    LWindowModality m_modality = LWindowModality::NonModal;
    LRect<int> m_geometry;
    bool m_updateRequestPending = false;
    bool m_compositing = false;
    bool m_resizeAutomatic = true;
    std::wstring m_title;
    bool m_resizeEventPending = true;
    bool m_receivedExpose = false;
    bool m_exposed = false;
    std::shared_ptr<LTexture> m_windowIcon;
    double m_opacity= 1;
    LSurfaceFormat m_requestedFormat;
    SurfaceType m_surfaceType = LSurface::RasterSurface;
    bool m_positionAutomatic = true;
    std::shared_ptr<LRegion> m_mask;

    LSize<int> m_minimumSize = {0, 0};
    LSize<int> m_maximumSize = {LWINDOWSIZE_MAX, LWINDOWSIZE_MAX};

    LSize<int> m_baseSize;
    LSize<int> m_sizeIncrement;

    std::vector<std::weak_ptr<LWindow> > m_children;

    std::weak_ptr<LWindow> m_transientParent;
    bool m_blockedByModalWindow = false;

    LElapsedTimer m_lastComposeTime;

    LCursor m_cursor;

    void initWin(std::shared_ptr<LScreen> targetScreen = nullptr);

    void setTopLevelScreen(std::shared_ptr<LScreen> newScreen);
    void updateVisibility();

    void create(bool recursive, WId nativeHandle=0);
    std::shared_ptr<LPlatformSurface> surfaceHandle() const override;
    std::shared_ptr<LScreen> screenForGeometry(const LRect<int> &rect) const;
    virtual void processSafeAreaMarginsChanged() {}
    bool applyCursor();
    void maybeQuitOnLastWindowClosed();

    virtual bool handleEvent(std::shared_ptr<LEvent> ev) override;

    virtual void mouseMoveEvent(std::shared_ptr<LMouseEvent> ev);
    virtual void mousePressEvent(std::shared_ptr<LMouseEvent> ev);
    virtual void mouseReleaseEvent(std::shared_ptr<LMouseEvent> ev);
    virtual void mouseDoubleClickEvent(std::shared_ptr<LMouseEvent> ev);
    virtual void moveEvent(std::shared_ptr<LMoveEvent> ev);

    virtual void resizeEvent(std::shared_ptr<LResizeEvent> ev);
    virtual void keyPressEvent(std::shared_ptr<LKeyEvent> ev);
    virtual void keyReleaseEvent(std::shared_ptr<LKeyEvent> ev);
    virtual void focusInEvent(std::shared_ptr<LFocusEvent> ev);
    virtual void focusOutEvent(std::shared_ptr<LFocusEvent> ev);

    virtual void wheelEvent(std::shared_ptr<LWheelEvent> ev);
    virtual void exposeEvent(std::shared_ptr<LExposeEvent> ev);
    virtual void showEvent(std::shared_ptr<LEvent> ev);
    virtual void hideEvent(std::shared_ptr<LEvent> ev);

    friend class LPlatformWindow;
    friend class LGuiApplication;
};


LK_NAMESPACE_END

#endif // LWINDOW_H
