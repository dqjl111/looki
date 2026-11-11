#include "lwidget.h"
#include "lkcore/levent.h"
#include "lkgui/lguiapplication.h"
#include "lkgui/paint/ltransform2d.h"
#include "lkgui/linputmethod.h"
#include "lkgui/lplatformwindow.h"
#include "lapplication.h"
#include "lwidgetdata.h"
#include "lwidgetwindow.h"
#include "ldesktopwidget.h"
#include "lwindowcontainer.h"
#include "lkgui/paint/lbrush.h"
#include <functional>
#include <lkcore/ldebug.h>
#include <iterator>
#include <algorithm>
#include "lkwidgets/widget/labstractscrollarea.h"
#include "lkwidgets/util/laction.h"
#include "lkwidgets/widget/lmenu.h"
#include "lkwidgets/dialog/ldialog.h"

LK_NAMESPACE_BEGIN
std::weak_ptr<LWidget> LWidget::m_mouseGrab;
std::weak_ptr<LWidget> LWidget::m_keyboardGrab;

static std::vector<LRect<int>> getSortedRectsToScroll(const std::shared_ptr<LRegion> region, int dx, int dy)
{
    std::vector<LRect<int>> rects;
    std::copy(region->begin(), region->end(), std::back_inserter(rects));
    if (rects.size() > 1) {
        std::sort(rects.begin(), rects.end(), [=](const LRect<int> &r1, const LRect<int> &r2) {
            if (r1.m_y == r2.m_y) {
                if (dx > 0){
                    return r1.m_x > r2.m_x;
                }
                return r1.m_x < r2.m_x;
            }
            if (dy > 0){
                return r1.m_y > r2.m_y;
            }
            return r1.m_y< r2.m_y;
        });
    }
    return rects;
}

static bool hasPlatformWindow(std::shared_ptr<LWidget> widget)
{
    if(widget != nullptr){
        std::shared_ptr<LWindow> w = widget->windowHandle();
        if(w != nullptr){
            return w->handle() != nullptr;
        }
    }
    return false;
}

static inline void fillRegion(LPainter *painter,
                              const std::shared_ptr<LRegion> rgn,
                              const std::shared_ptr<LBrush> brush)
{
    if (brush->m_style == LBrush::TexturePattern) {
        const LRect<double> rect = rgn->boundingRect().toRectF();
        painter->setClipRegion(rgn);
        painter->drawTiledPixmap(rect, brush->texture(), rect.topLeft());
    }
    else
    {
        for (const LRect<int> &rect : *rgn){
            painter->fillRect(rect.toRectF(), brush);
        }
    }
}

static inline std::shared_ptr<LWindow> grabberWindow(const std::shared_ptr<LWidget>  w)
{
    std::shared_ptr<LWindow> window = w->windowHandle();
    if (window == nullptr){
        if (const std::shared_ptr<LWidget> nativeParent = w->nativeParentWidget()){
            window = nativeParent->windowHandle();
        }
    }
    return window;
}



LWidget::LWidget(std::shared_ptr<LWidget> parent, LFlags<LWindowType> f): LTreeObj(nullptr)
{
    m_initObj = std::bind(&LWidget::initWidget, this, parent, f);
}

LWidget::~LWidget()
{
    m_elementStyle.clearCachedStyle();
    m_widgetData.inDestructor = true;
    if(m_mouseGrabWithCursor){
        LGuiApplication::restoreOverrideCursor(m_mouseGrabBackCursor);
    }

    LApplication::removeWidget(nullptr); //清除变空的weak_ptr
    LApplication::m_mapper.erase(m_widgetData.winId);

    destroy();
}

void LWidget::setWindowFlags(LFlags<LWindowType> flags)
{
    if (m_widgetData.windowFlags == flags){
        return;
    }

    if ((m_widgetData.windowFlags | flags) & LWindowType::Window) {
        // the old type was a window and/or the new type is a window
        LPoint<int> oldPos = pos();
        bool visible = isVisible();
        const bool windowFlagChanged = (m_widgetData.windowFlags ^ flags) & LWindowType::Window;
        std::shared_ptr<LWidget> p = parentT<LWidget>();
        setParent(p, flags);

        // if both types are windows or neither of them are, we restore
        // the old position
        if (!windowFlagChanged && (visible || testAttribute(LWidgetAttribute::Moved))){
            move(oldPos);
        }
        // for backward-compatibility we change QuitOnClose attribute value only when the window was recreated.
        initDefaultQuitOnCloseAttribute();
    } else {
        m_widgetData.windowFlags = flags;
    }
}

void LWidget::setWindowFlag(LWindowType flag, bool on)
{
    if (on){
        setWindowFlags(m_widgetData.windowFlags | flag);
    }else{
        setWindowFlags(m_widgetData.windowFlags & ~uint32_t(flag));
    }
}


bool LWidget::testAttribute(LWidgetAttribute pos) const {
    uint32_t t = uint32_t(pos) / 32;
    if (t < m_widgetData.attributes.size()) {
        return (m_widgetData.attributes[t] >> (uint32_t(pos) % 32)) & 0x01;
    }
    return false;
}

void LWidget::setAttribute(LWidgetAttribute pos, bool on) {
    uint32_t t = uint32_t(pos) / 32;
    if (t < m_widgetData.attributes.size()) {
        if (on) {
            m_widgetData.attributes[t] |= (1 << (uint32_t(pos) % 32));
        }
        else {
            m_widgetData.attributes[t] &= ~(1 << (uint32_t(pos) % 32));
        }
    }
}

void LWidget::setName(const std::string &v)
{
    if(v != name()){
        LTreeObj::setName(v);

        if(m_elementStyle.m_hasCachedWithCssId){
            if(clearCachedStyle()){
                update();
            }
        }
    }
}

void LWidget::setEnabled(bool enable)
{
    setAttribute(LWidgetAttribute::ForceDisabled, !enable);
    setEnabledHelper(enable);
}

bool LWidget::isVisibleTo(const std::shared_ptr<LWidget> ancestor) const
{
    if (ancestor == nullptr){
        return isVisible();
    }
    std::shared_ptr<LWidget> w = getPtrT<LWidget>();
    while(true){
        if(w != nullptr){
            if(w->isHidden() || w->isWindow()){
                break;
            }
            std::shared_ptr<LWidget> p = parentT<LWidget>();
            if(p != nullptr){
                if(p != ancestor){
                    w = p;
                }else{
                    break;
                }
            }else{
                break;
            }
        }else{
            break;
        }
    }
    return !w->isHidden();
}

void LWidget::setUpdatesEnabled(bool enable)
{
    setAttribute(LWidgetAttribute::ForceUpdatesDisabled, !enable);
    setUpdatesEnabledHelper(enable);
}

void LWidget::initWidget(std::shared_ptr<LWidget> parent, LFlags<LWindowType> f)
{
//    lWarn() << "init widget";
    std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();
    m_elementStyle.m_widget = tThis;

    LApplication::insertWidget(tThis);

    m_widgetData.winId = 0;
    for (uint32_t i = 0; i < m_widgetData.attributes.size(); ++i) {
        m_widgetData.attributes.at(i) = 0;
    }

    m_widgetData.windowFlags = f;
    m_widgetData.windowState = 0;
    m_widgetData.focusPolicy = 0;
    m_widgetData.contextMenuPolicy = uint32_t(LContextMenuPolicy::Default);
    m_widgetData.windowModality = uint32_t(LWindowModality::NonModal);

    m_widgetData.sizeHintForced = 0;
    m_widgetData.isClosing = 0;
    m_widgetData.inShow = 0;
    m_widgetData.inSetWindowState = 0;
    m_widgetData.inDestructor = false;


    setAttribute(LWidgetAttribute::QuitOnClose, true);
    initDefaultQuitOnCloseAttribute();

    setAttribute(LWidgetAttribute::WStateHidden, true);

    m_widgetData.crect = (parent != nullptr) ? LRect<int>(0, 0, 100, 30, false) : LRect<int>(0, 0, 640, 480, false);
    if (windowType() == LWindowType::Desktop) {
        create();
    }
    else if (parent != nullptr) {
        setParent(parent, m_widgetData.windowFlags);
    }
    else {
        adjustFlags(m_widgetData.windowFlags, tThis);
        resolveLayoutDirection();
        // opaque system background?
        const std::shared_ptr<LBrush> background = palette()->brush(colorGroup(),LPalette::Window);
        setOpaque(isWindow() && background->m_style != LBrush::NoPattern && background->isOpaque());
    }

    // initAsStyle();
    initSpecific();
    std::shared_ptr<LEvent> e = std::make_shared<LEvent>(LEvent::Create);
    LCoreApplication::sendEvent(tThis, e);
}

void LWidget::initSpecific()
{

}

void LWidget::initDefaultQuitOnCloseAttribute() {
    std::shared_ptr<LWidget> p = parentT<LWidget>();
    if (p != nullptr) {
        LWindowType type = windowType();
        if (type != LWindowType::Widget 
            && type != LWindowType::Window 
            && type != LWindowType::Dialog
            && type != LWindowType::SubWindow) {
            setAttribute(LWidgetAttribute::QuitOnClose, false);
        }
    }
}

void LWidget::setWindowState(LFlags<LWindowState> newstate){
    LFlags<LWindowState> oldstate = windowState();
    if (newstate.testFlag(LWindowState::Minimized)){
        newstate.setFlags(LWindowState::Active, false);
    }
    if (oldstate == newstate){
        return;
    }
    if (isWindow() && !testAttribute(LWidgetAttribute::WStateCreated)){
        create();
    }

    m_widgetData.windowState = newstate;
    m_widgetData.inSetWindowState = 1;
    if (isWindow()) {
        // Ensure the initial size is valid, since we store it as normalGeometry below.
        if (!testAttribute(LWidgetAttribute::Resized) && !isVisible()){
            adjustSize();
        }

        createTLExtra();
        windowHandle()->setWindowStates(uint8_t(newstate) & ~uint8_t(LWindowState::Active));
    }
    m_widgetData.inSetWindowState = 0;

    if (newstate & LWindowState::Active){
        activateWindow();
    }

    std::shared_ptr<LWindowStateChangeEvent> e= std::make_shared<LWindowStateChangeEvent>(oldstate);
    LCoreApplication::sendEvent(objPtr(), e);
}

std::shared_ptr<LWidget> LWidget::window() const{
    std::shared_ptr<LWidget> w = getPtrT<LWidget>();
    std::shared_ptr<LWidget>  p = w->parentT<LWidget>();
    while (!w->isWindow() && (p != nullptr)) {
        w = p;
        p = p->parentT<LWidget>();
    }
    return w;
}

std::shared_ptr<LWidget> LWidget::nativeParentWidget() const
{
    std::shared_ptr<LWidget> parent = parentT<LWidget>();
    while (parent && !parent->internalWinId()){
        parent = parent->parentT<LWidget>();
    }
    return parent;
}

bool LWidget::hasWindowContainer()
{
    if (m_pExtraData != nullptr){
        return m_pExtraData->hasWindowContainer;
    }
    return false;
}

void LWidget::show() {
    LWindowState defaultState =
        LGuiApplication::platformIntegration()->defaultWindowState(m_widgetData.windowFlags);
    if (defaultState == LWindowState::FullScreen) {
        showFullScreen();
    }
    else if (defaultState == LWindowState::Maximized) {
        showMaximized();
    }
    else {
        setVisible(true);
    }
}

void LWidget::setVisible(bool visible){
    if (testAttribute(LWidgetAttribute::WStateExplicitShowHide)
        && testAttribute(LWidgetAttribute::WStateHidden) == !visible) {
        return;
    }

    // Remember that setVisible was called explicitly
    setAttribute(LWidgetAttribute::WStateExplicitShowHide);
    setVisibleInner(visible);
}

void LWidget::setVisibleInner(bool visible) {
    if (visible) { // show
        std::shared_ptr<LWidget> pw = parentT<LWidget>();

        // Designer uses a trick to make grabWidget work without showing
        if (!isWindow() && pw != nullptr && pw->isVisible()
            && !pw->testAttribute(LWidgetAttribute::WStateCreated)) {
            pw->window()->createRecursively();
        }

  
        if (!testAttribute(LWidgetAttribute::WStateCreated)
            && (isWindow() ||
                (pw!= nullptr && pw->testAttribute(LWidgetAttribute::WStateCreated)))) {
            create();
        }


        LFlags<LWindowState> initialWindowState = windowState();

        // polish if necessary
        //ensurePolished();
        // whether we need to inform the parent widget immediately
        bool needUpdateGeometry = !isWindow() && testAttribute(LWidgetAttribute::WStateHidden);
        // we are no longer hidden
        setAttribute(LWidgetAttribute::WStateHidden, false);

        if (needUpdateGeometry){
            updateGeometryHelper(true);
        }

        bool wasResized = testAttribute(LWidgetAttribute::Resized);
        // adjust size if necessary
        if (!wasResized){
            if(isWindow() || ((pw != nullptr) && (pw->m_layout == nullptr))) {
                adjustSize();
                if (isWindow() && (windowState() != initialWindowState)) {
                    setWindowState(initialWindowState);
                }
                setAttribute(LWidgetAttribute::Resized, false);
            }else{
                if (m_layout){
                    m_layout->activate();
                }
            }
        }else{
            if (m_layout){
                m_layout->activate();
            }
        }

        if (!isWindow()) {
            std::shared_ptr<LWidget> parent = pw;

            while ((parent!= nullptr)
                   && parent->isVisible()
                   && parent->m_layout
                   && !parent->m_widgetData.inShow) {
                parent->m_layout->activate();
                if (parent->isWindow()){
                    break;
                }
                parent = parent->parentT<LWidget>();
            }
            if (parent){
                parent->setDirtyOpaqueRegion();
            }
        }

        setAttribute(LWidgetAttribute::KeyboardFocusChange, false);

        std::shared_ptr<LWidget> tThis= getPtrT<LWidget>();
        if (isWindow() || ((pw != nullptr) && pw->isVisible())) {
            showHelper();
            ((LApplication*)lApp)->sendSyntheticEnterLeave(tThis);
        }
        std::shared_ptr<LEvent> showToParentEvent
            = std::make_shared<LEvent>(LEvent::ShowToParent);
        LCoreApplication::sendEvent(tThis, showToParentEvent);
    }
    else { // hide
        std::shared_ptr<LWidget> tThis= getPtrT<LWidget>();
        if (LApplication::m_hiddenFocusWidget.lock() == tThis){
            LApplication::m_hiddenFocusWidget.reset();
        }

        // hw: The test on getOpaqueRegion() needs to be more intelligent
        // currently it doesn't work if the widget is hidden (the region will
        // be clipped). The real check should be testing the cached region
        // (and dirty flag) directly.
        std::shared_ptr<LWidget> pw = parentT<LWidget>();
        if (!isWindow() && pw != nullptr){
            pw->setDirtyOpaqueRegion();
        }

        if (!testAttribute(LWidgetAttribute::WStateHidden)) {
            setAttribute(LWidgetAttribute::WStateHidden);
            if (testAttribute(LWidgetAttribute::WStateCreated)){
                hideHelper();
            }
        }

        // invalidate layout similar to updateGeometry()
        if (!isWindow() && pw != nullptr) {
            if (pw->m_layout != nullptr){
                pw->m_layout->invalidate();
            }
            else if (pw->isVisible()){
                std::shared_ptr<LEvent> LayoutRequestEvent
                    = std::make_shared<LEvent>(LEvent::LayoutRequest);
                LCoreApplication::postEvent(pw, LayoutRequestEvent);
            }
        }
        std::shared_ptr<LEvent> hideToParentEvent
            = std::make_shared<LEvent>(LEvent::HideToParent);
        LCoreApplication::sendEvent(tThis, hideToParentEvent);
    }
}

void LWidget::invalidateBackingStore(const LRect<int> &r)
{
    std::shared_ptr<LRegion> t = std::make_shared<LRegion>(r);
    invalidateBackingStore(t);
}

void LWidget::invalidateBackingStore(const std::shared_ptr<LRegion> r)
{
    if(r == nullptr){
        return;
    }

    if (r->isEmpty()){
        return;
    }

    if (LCoreApplication::closingDown()){
        return;
    }

    if (!isVisible() || !updatesEnabled()){
        return;
    }

    LTLWExtra *tlwExtra = window()->maybeTopData();
    if (!tlwExtra || !tlwExtra->backingStore){
        return;
    }

    std::shared_ptr<LRegion> clipped = std::make_shared<LRegion>(r);
    clipped->intersect(clipRect());
    if (clipped->isEmpty()){
        return;
    }

    if ( (m_pExtraData != nullptr) && (m_pExtraData->mask != nullptr)) {
        std::shared_ptr<LRegion> masked = std::make_shared<LRegion>(m_pExtraData->mask);
        masked->intersect(clipped);
        if (masked->isEmpty()){
            return;
        }

        std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();
        tlwExtra->repaintMan->markDirty(masked, tThis,
                                        LWidgetRepaintMan::UpdateLater, LWidgetRepaintMan::BufferInvalid);
    } else {
        std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();
        tlwExtra->repaintMan->markDirty(clipped, tThis,
                                        LWidgetRepaintMan::UpdateLater, LWidgetRepaintMan::BufferInvalid);
    }
}

LWidgetRepaintMan *LWidget::maybeRepaintMan() const
{
    std::shared_ptr<LWidget> win = window();
    LTLWExtra *ext = win->maybeTopData();
    return ext ? ext->repaintMan.get() : nullptr;
}

void LWidget::setRedirected(std::shared_ptr<LPaintDevice> replacement, const LPoint<int> &offset)
{
    m_redirectDev = replacement;
    m_redirectDev->m_startPos.m_x =offset.m_x;
    m_redirectDev->m_startPos.m_y =offset.m_y;
    m_redirectDev->m_initPaintFun =  std::bind(&LWidget::initPainter, this, std::placeholders::_1);
}

void LWidget::restoreRedirected()
{
    if(m_redirectDev != nullptr){
        m_redirectDev->m_startPos.m_x =0;
        m_redirectDev->m_startPos.m_y =0;
        m_redirectDev->m_initPaintFun = nullptr;
        m_redirectDev = nullptr;
    }
}

void LWidget::setSystemClip(std::shared_ptr<LPaintEngine> paintEngine,
                            double devicePixelRatio,
                            const std::shared_ptr<LRegion> region)
{
    LTransform2D scaleTransform;
    scaleTransform.scale(devicePixelRatio, devicePixelRatio);

    paintEngine->m_baseSystemClip = region;
    paintEngine->setSystemTransform(scaleTransform);
}

bool LWidget::closeHelper(LCloseMode mode)
{
    if (m_widgetData.isClosing){
        return true;
    }

    m_widgetData.isClosing = 1;


    std::shared_ptr<LWidget> that = getPtrT<LWidget>();
    std::shared_ptr<LWidget> parentWidget = parentT<LWidget>();
    if(parentWidget != nullptr){
        if(parentWidget->m_wasDeleted){
            parentWidget = nullptr;
        }
    }

    bool quitOnClose = testAttribute(LWidgetAttribute::QuitOnClose);
    if (mode != LCloseMode::NoEvent) {
        std::shared_ptr<LEvent> e = std::make_shared<LEvent>(LEvent::Close);
        if (mode == LCloseMode::WithSpontaneousEvent)
            LApplication::sendSpontaneousEvent(that, e);
        else{
            LCoreApplication::sendEvent(that, e);
        }

        if(!e->isAccepted()) {
            m_widgetData.isClosing = 0;
            return false;
        }
    }

    if (!isHidden()){
        setVisible(false);
    }

    // Attempt to close the application only if this has WA_QuitOnClose set and a non-visible parent
    quitOnClose = quitOnClose && (parentWidget == nullptr || !parentWidget->isVisible());

    if (quitOnClose) {
        std::list<std::shared_ptr<LWidget> > list = LApplication::topLevelWidgets();
        bool lastWindowClosed = true;
        for (const std::shared_ptr<LWidget>& w: list) {
            if(w->isVisible() == false){
                continue;
            }

            std::shared_ptr<LWidget> pw = w->parentT<LWidget>();
            if(pw != nullptr){
                continue;
            }

            if (!w->testAttribute(LWidgetAttribute::QuitOnClose)){
                continue;
            }
            lastWindowClosed = false;
            break;
        }

        if (lastWindowClosed) {
            lApp->quit();
        }
    }

    if(parentWidget != nullptr){
        if (testAttribute(LWidgetAttribute::ReleaseParentOnClose)) {
            setParent((std::shared_ptr<LWidget>)nullptr);
        }
    }
    m_widgetData.isClosing = 0;
    return false;
}


bool LWidget::stealKeyboardGrab(bool grab)
{
    std::shared_ptr<LWidget> q= getPtrT<LWidget>();
    std::shared_ptr<LWindow> window = grabberWindow(q);
    return window!=nullptr ? window->setKeyboardGrabEnabled(grab) : false;
}

bool LWidget::stealMouseGrab(bool grab)
{
    std::shared_ptr<LWidget> q= getPtrT<LWidget>();
    std::shared_ptr<LWindow> window = grabberWindow(q);
    return window ? window->setMouseGrabEnabled(grab) : false;
}

LSize<int> LWidget::adjustedSize() const
{
    LSize<int> s = sizeHint();

    if (isWindow()) {
        LFlags<LOrientation> exp;
        if (m_layout != nullptr) {
            if (m_layout->hasHeightForWidth()){
                s.setHeight(m_layout->totalHeightForWidth(s.width()));
            }
            exp = m_layout->expandingDirections();
        }
        else
        {
            if (m_sizePolicy.hasHeightForWidth()){
                s.setHeight(heightForWidth(s.width()));
            }
            exp = m_sizePolicy.expandingDirections();
        }

        LTLWExtra *extra = maybeTopData();
        if(extra != nullptr){
            if (exp & LOrientation::Horizontal){
                s.setWidth(std::max<int>(s.width(), extra->basew));
            }
            if (exp & LOrientation::Vertical){
                s.setHeight(std::max<int>(s.height(), extra->baseh));
            }
            extra->sizeAdjusted = true;
        }


        LRect<int> screen;
        if (const std::shared_ptr<LScreen> screenAtPoint = LGuiApplication::screenAt(pos())){
            screen = screenAtPoint->m_geometry;
        }else{
            screen = LGuiApplication::primaryScreen()->m_geometry;
        }

        s.setWidth(std::min<int>(s.width(), screen.width()*2/3));
        s.setHeight(std::min<int>(s.height(), screen.height()*2/3));
    }

    if (!s.isValid()) {
        LRect<int> r = childrenRect(); // get children rectangle
        if (r.isNull()){
            return s;
        }
        s = r.size() + LSize<int>(2 * r.m_x, 2 * r.m_y);
    }

    return s;
}


void LWidget::setHidden(bool hidden) {
    setVisible(!hidden);
}

void LWidget::showIfNotHidden()
{
    if ( !(isHidden() && testAttribute(LWidgetAttribute::WStateExplicitShowHide)) ){
        setVisible(true);
    }
}


void LWidget::showMinimized() {
    bool isMin = isMinimized();
    if (isMin && isVisible()){
        return;
    }


    if (!isMin){
        setWindowState((uint8_t(windowState()) & ~uint8_t(LWindowState::Active)) | uint8_t(LWindowState::Minimized));
    }
    setVisible(true);
}

void LWidget::showMaximized() {
    setWindowState((windowState() & ~(LWindowState::Minimized | LWindowState::FullScreen))
                   | LWindowState::Maximized);
    setVisible(true);

}

void LWidget::showFullScreen() {
    setWindowState((windowState() & ~(LWindowState::Minimized | LWindowState::Maximized))
                   | LWindowState::FullScreen);
    setVisible(true);

}

void LWidget::showNormal() {
    setWindowState(windowState() & ~(LWindowState::Minimized
                                     | LWindowState::Maximized
                                     | LWindowState::FullScreen));
    setVisible(true);
}

bool LWidget::close() {
    return closeHelper(LCloseMode::WithEvent);
}

void LWidget::raise() {
    std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();

    if (!isWindow()) {
        std::shared_ptr<LWidget> p = parentT<LWidget>();
        const int parentChildCount = p->childrenCount();
        if (parentChildCount < 2){
            return;
        }
        const int from = p->childIndex(tThis);
        // Do nothing if the widget is already in correct stacking order _and_ created.
        if (from >=0 && (from < (parentChildCount -1))){
            p->moveChild(from, parentChildCount - 1);
        }
        if (!testAttribute(LWidgetAttribute::WStateCreated) && p->testAttribute(LWidgetAttribute::WStateCreated)){
            create();
        }
        else if (from == parentChildCount - 1){
            return;
        }

        std::shared_ptr<LRegion> region = std::make_shared<LRegion>(rect());
        subtractOpaqueSiblings(region);
        invalidateBackingStore(region);
    }
    if (testAttribute(LWidgetAttribute::WStateCreated)){
        raiseSys();
    }

    if (m_pExtraData != nullptr && m_pExtraData->hasWindowContainer){
        LWindowContainer::parentWasRaised(tThis);
    }

    std::shared_ptr<LEvent> e = std::make_shared<LEvent>(LEvent::ZOrderChange);
    LCoreApplication::sendEvent(tThis, e);
}

void LWidget::lower() {
    std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();
    if (!isWindow()) {
        std::shared_ptr<LWidget> p = parentT<LWidget>();
        const int parentChildCount = p->childrenCount();
        if (parentChildCount < 2){
            return;
        }
        const int from = p->childIndex(tThis);
        if (from > 0){
            p->moveChild(from, 0);
        }
        if (!testAttribute(LWidgetAttribute::WStateCreated) && p->testAttribute(LWidgetAttribute::WStateCreated)){
            create();
        }
        else if (from == 0){
            return;
        }
    }
    if (testAttribute(LWidgetAttribute::WStateCreated)){
        lowerSys();
    }

    if (m_pExtraData != nullptr && m_pExtraData->hasWindowContainer){
        LWindowContainer::parentWasLowered(tThis);
    }

    std::shared_ptr<LEvent> e = std::make_shared<LEvent>(LEvent::ZOrderChange);
    LCoreApplication::sendEvent(tThis, e);
}

void LWidget::adjustSize(){

    LSize<int> s = adjustedSize();
    if (m_layout != nullptr){
        m_layout->activate();
    }
    if (s.isValid()){
        resize(s);
    }
}

bool LWidget::isActiveWindow() const{

    std::shared_ptr<LWidget>  tlw = window();
    if(tlw == LApplication::activeWindow() || (isVisible() && (tlw->windowType() == LWindowType::Popup))){
        return true;
    }

    if(tlw->windowType() == LWindowType::Tool && !tlw->isModal()){
         std::shared_ptr<LWidget> p = tlw->parentT<LWidget>();
         if(p == nullptr){
            return true;
         }

         if(p->isActiveWindow()){
            return true;
         }
    }

    std::shared_ptr<LWidget> w = LApplication::activeWindow();
    while((w!=nullptr) && tlw->windowType() == LWindowType::Tool && !w->isModal()) {
        std::shared_ptr<LWidget> p = w->parentT<LWidget>();
        if(p == nullptr){
            break;
        }else{
            w = p->window();
            if(w == tlw){
                return true;
            }
        }
    }

    // Check for an active window container
    if (std::shared_ptr<LWindow> ww = LGuiApplication::focusWindow()) {
        while (ww != nullptr) {
            std::shared_ptr<LWidgetWindow> lww = std::dynamic_pointer_cast<LWidgetWindow>(ww);
            if(lww != nullptr){
                std::shared_ptr<LWindowContainer> lwc =
                    std::dynamic_pointer_cast<LWindowContainer>(lww->widget());
                if(lwc != nullptr){
                    if(lwc->window() == tlw){
                        return true;
                    }
                }
            }
            ww = ww->parent();
        }
    }

    if (const std::shared_ptr<LWindow> w = tlw->windowHandle()) {
        if (const std::shared_ptr<LPlatformWindow> wh = w->handle()){
            return wh->isActive();
        }
    }
    return false;
}

void LWidget::activateWindow(){
    std::shared_ptr<LWindow> wnd = window()->windowHandle();
    if (wnd!= nullptr){
        wnd->requestActivate();
    }
}

std::shared_ptr<LWindow> LWidget::windowHandle(WinHandleMode mode) const{
    if (mode == WinHandleMode::Direct || mode == WinHandleMode::Closest) {
        if (LTLWExtra *x = maybeTopData()) {
            if (x->window != nullptr || mode == WinHandleMode::Direct){
                return x->window;
            }
        }
    }
    if (mode == WinHandleMode::Closest) {
        if (std::shared_ptr<LWidget> nativeParent = nativeParentWidget()) {
            if (std::shared_ptr<LWindow>  window = nativeParent->windowHandle()){
                return window;
            }
        }
    }
    if (mode == WinHandleMode::TopLevel || mode == WinHandleMode::Closest) {
        if (std::shared_ptr<LWidget> topLevel = window()) {
            if (std::shared_ptr<LWindow> window = topLevel ->windowHandle()){
                return window;
            }
        }
    }
    return nullptr;
}

LRect<int> LWidget::frameGeometry() const
{
    if (isWindow() && ! (windowType() == LWindowType::Popup)) {
        LMargins<int> fs = frameStrut();
        return LRect<int>(m_widgetData.crect.m_x - fs.m_left,
                     m_widgetData.crect.m_y - fs.m_top,
                     m_widgetData.crect.m_width + fs.m_left + fs.m_right,
                     m_widgetData.crect.m_height + fs.m_top + fs.m_bottom,
                     false);
    }
    return m_widgetData.crect;
}

void LWidget::setGeometry(const LRect<int> &r)
{
    setAttribute(LWidgetAttribute::Resized);
    setAttribute(LWidgetAttribute::Moved);
    if (isWindow()){
        topData()->posIncludesFrame = 0;
    }
    if (testAttribute(LWidgetAttribute::WStateCreated)) {
        setGeometrySys(r.m_x, r.m_y, r.width(), r.height(), true);
        setDirtyOpaqueRegion();
    } else {
        const auto oldRect = m_widgetData.crect;
        m_widgetData.crect.setTopLeft(r.topLeft());
        m_widgetData.crect.setSize(r.size().boundedTo(maximumSize()).expandedTo(minimumSize()));
        if (oldRect != m_widgetData.crect) {
            if(m_layout != nullptr){
                m_layout->invalidate();
            }
        }
    }

    if(m_pExtraData != nullptr){
        if(m_pExtraData->hasWindowContainer){
            std::shared_ptr<LWidget> q= getPtrT<LWidget>();
            LWindowContainer::parentWasMoved(q);
        }
    }
}

void LWidget::move(const LPoint<int>& p)
{
    setAttribute(LWidgetAttribute::Moved);
    if (testAttribute(LWidgetAttribute::WStateCreated)) {
        if (isWindow()){
            topData()->posIncludesFrame = false;
        }
        setGeometrySys(p.m_x + geometry().m_x - LWidget::x(),
                           p.m_y + geometry().m_y - LWidget::y(),
                           width(), height(), true);
        setDirtyOpaqueRegion();
    } else {
        if (isWindow()){
           topData()->posIncludesFrame = true;
        }
        m_widgetData.crect.moveTopLeft(p); // no frame yet
    }

    if(m_pExtraData != nullptr){
        if (m_pExtraData->hasWindowContainer){
            std::shared_ptr<LWidget> q= getPtrT<LWidget>();
            LWindowContainer::parentWasMoved(q);
        }
    }
}

LMargins<int> LWidget::contentsMargins() const
{
    return LMargins<int>(m_leftMargin, m_topMargin, m_rightMargin, m_bottomMargin);
}

LRect<int> LWidget::contentsRect() const
{
    return rect().marginsRemoved(contentsMargins());
}

LRect<int> LWidget::customRect() const
{
    return rect().marginsRemoved(contentsMargins());
}

void LWidget::setContentsMargins(int left, int top, int right, int bottom)
{
    if (left == m_leftMargin && top == m_topMargin
        && right == m_rightMargin && bottom == m_bottomMargin){
        return;
    }
    m_leftMargin = left;
    m_topMargin = top;
    m_rightMargin = right;
    m_bottomMargin = bottom;

    updateContentsRect();
}


struct MapToGlobalTransformResult {
    LTransform2D transform;
    std::shared_ptr<LWindow> window;
};

static inline bool canMapPosition(std::shared_ptr<LWindow> window)
{
    return (window->handle() != nullptr) && !window->isResizeEventPending();
}


static MapToGlobalTransformResult mapToGlobalTransform(std::shared_ptr<LWidget> w)
{
    MapToGlobalTransformResult result;
    result.window = nullptr;
    for ( ; w!= nullptr ; w = w->parentT<LWidget>()) {
        std::shared_ptr<LWindow> window = w->windowHandle();
        if (window != nullptr){
            if(canMapPosition(window)) {
                result.window = window;
                break;
            }
        }

        const LPoint<int> topLeft = w->geometry().topLeft();
        result.transform.translate(topLeft.m_x, topLeft.m_y);
        if (w->isWindow()){
            break;
        }
    }
    return result;
}

LPoint<int> LWidget::mapToGlobal(const LPoint<int> &pos) const
{
    std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();
    const MapToGlobalTransformResult t = mapToGlobalTransform(tThis);
    const LPoint<int> g = t.transform.map(pos);
    return t.window ? t.window->mapToGlobal(g) : g;
}


LPoint<int> LWidget::mapFromGlobal(const LPoint<int> &pos) const
{
    std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();
    const MapToGlobalTransformResult t = mapToGlobalTransform(tThis);
    const LPoint<int> windowLocal = t.window ? t.window->mapFromGlobal(pos) : pos;
    return t.transform.inverted().map(windowLocal);
}

LPoint<int> LWidget::mapToParent(const LPoint<int> &pos) const
{
    return pos + m_widgetData.crect.topLeft();
}

LPoint<int> LWidget::mapFromParent(const LPoint<int> &pos) const
{
    return pos - m_widgetData.crect.topLeft();
}

LPoint<int> LWidget::mapTo(const std::shared_ptr<LWidget> parent, const LPoint<int> &pos) const
{
    LPoint<int> p = pos;
    if (parent!= nullptr) {
        std::shared_ptr<LWidget> w = getPtrT<LWidget>();
        while (w != parent) {
            p = w->mapToParent(p);
            w = w->parentT<LWidget>();
        }
    }
    return p;
}

LPoint<int> LWidget::mapFrom(const std::shared_ptr<LWidget> parent, const LPoint<int> &pos) const
{
    LPoint<int> p = pos;
    if (parent!= nullptr) {
        std::shared_ptr<LWidget> w = getPtrT<LWidget>();
        while (w != parent) {
            p = w->mapFromParent(p);
            w = w->parentT<LWidget>();
        }
    }
    return p;
}

bool LWidget::handleEvent(std::shared_ptr<LEvent> event){
    // ignore mouse and key events when disabled
    if (!isEnabled()) {
        switch(event->type()) {
        case LEvent::TabletPress:
        case LEvent::TabletRelease:
        case LEvent::TabletMove:
        case LEvent::MouseButtonPress:
        case LEvent::MouseButtonRelease:
        case LEvent::MouseButtonDblClick:
        case LEvent::MouseMove:
        case LEvent::TouchBegin:
        case LEvent::TouchUpdate:
        case LEvent::TouchEnd:
        case LEvent::TouchCancel:
        case LEvent::ContextMenu:
        case LEvent::KeyPress:
        case LEvent::KeyRelease:
        case LEvent::Wheel:{
            return false;
        }
        default:{
            break;
        }
        }
    }

    switch (event->type()) {
    case LEvent::PlatformSurface: {
        std::shared_ptr<LPlatformSurfaceEvent> t = std::dynamic_pointer_cast<LPlatformSurfaceEvent>(event);
        switch (t->surfaceEventType()) {
        case LPlatformSurfaceEvent::SurfaceCreated:
            if (!testAttribute(LWidgetAttribute::WStateCreated)){
                create();
            }
            break;
        case LPlatformSurfaceEvent::SurfaceAboutToBeDestroyed:
            if (testAttribute(LWidgetAttribute::WStateCreated)) {
                destroy(false, false);
            }
            break;
        }
        break;
    }
    case LEvent::MouseMove:{
        std::shared_ptr<LMouseEvent> t = std::dynamic_pointer_cast<LMouseEvent>(event);
        mouseMoveEvent(t);
        break;
    }
    case LEvent::MouseButtonPress:{
        std::shared_ptr<LMouseEvent> t = std::dynamic_pointer_cast<LMouseEvent>(event);
        mousePressEvent(t);
        break;
    }
    case LEvent::MouseButtonRelease:{
        std::shared_ptr<LMouseEvent> t = std::dynamic_pointer_cast<LMouseEvent>(event);
        mouseReleaseEvent(t);
        break;
    }
    case LEvent::MouseButtonDblClick:{
        std::shared_ptr<LMouseEvent> t = std::dynamic_pointer_cast<LMouseEvent>(event);
        mouseDoubleClickEvent(t);
        break;
    }
    case LEvent::Wheel:{
        std::shared_ptr<LWheelEvent> t = std::dynamic_pointer_cast<LWheelEvent>(event);
        wheelEvent(t);
        break;
    }
    case LEvent::KeyPress: {
        std::shared_ptr<LKeyEvent> k = std::dynamic_pointer_cast<LKeyEvent>(event);
        bool res = false;
        if (!(k->modifiers() & (LKeyModifier::Ctrl | LKeyModifier::Alt))) {
            if (k->key() == int(LKey::Backtab)
                || (k->key() == int(LKey::Tab) && (k->modifiers() & LKeyModifier::Shift))){
                res = focusNextPrevChild(false);
            }
            else if (k->key() == int(LKey::Tab)){
                res = focusNextPrevChild(true);
            }
            if (res){
                break;
            }
        }
        keyPressEvent(k);
        break;
    }
    case LEvent::KeyRelease:{
        std::shared_ptr<LKeyEvent> k = std::dynamic_pointer_cast<LKeyEvent>(event);
        keyReleaseEvent(k);
        break;
    }
    case LEvent::ShortcutOverride:{
        break;
    }
    case LEvent::InputMethod:{
        std::shared_ptr<LInputMethodEvent> k = std::dynamic_pointer_cast<LInputMethodEvent>(event);
        inputMethodEvent(k);
        break;
    }
    case LEvent::FocusIn:{
        std::shared_ptr<LFocusEvent> k = std::dynamic_pointer_cast<LFocusEvent>(event);
        focusInEvent(k);
        updateWidgetTransform(event);
        break;
    }
    case LEvent::FocusOut:{
        std::shared_ptr<LFocusEvent> k = std::dynamic_pointer_cast<LFocusEvent>(event);
        focusOutEvent(k);
        break;
    }
    case LEvent::Enter:{
        enterEvent(event);
        break;
    }
    case LEvent::Leave:{
        leaveEvent(event);
        break;
    }
    case LEvent::Paint:{
        std::shared_ptr<LPaintEvent> k = std::dynamic_pointer_cast<LPaintEvent>(event);
        paintEvent(k);
        break;
    }
    case LEvent::Move:{
        std::shared_ptr<LMoveEvent> k = std::dynamic_pointer_cast<LMoveEvent>(event);
        moveEvent(k);
        updateWidgetTransform(k);
        break;
    }
    case LEvent::Resize:{
        std::shared_ptr<LResizeEvent> k = std::dynamic_pointer_cast<LResizeEvent>(event);
        resizeEvent(k);
        updateWidgetTransform(event);
        break;
    }
    case LEvent::Close:{
        closeEvent(event);
        break;
    }
    case LEvent::ContextMenu:{
        switch (m_widgetData.contextMenuPolicy) {
        case (uint32_t)LContextMenuPolicy::Prevent:{
            break;
        }
        case (uint32_t)LContextMenuPolicy::Default:
        case (uint32_t)LContextMenuPolicy::Custom:{
            std::shared_ptr<LContextMenuEvent> k = std::dynamic_pointer_cast<LContextMenuEvent>(event);
            contextMenuEvent(k);
            break;
        }
        default:{
            event->ignore();
            break;
        }
        }
        break;
    }
    case LEvent::Show:{
        showEvent(event);
        break;
    }
    case LEvent::Hide:{
        hideEvent(event);
        break;
    }
    case LEvent::ShowWindowRequest:{
        if (!isHidden()){
            showSys();
        }
        break;
    }
    case LEvent::ApplicationFontChange:{
        resolveFont();
        break;
    }
    case LEvent::ApplicationPaletteChange:{
        if (!(windowType() == LWindowType::Desktop)){
            resolvePalette();
        }
        break;
    }
    case LEvent::ToolBarChange:
    case LEvent::ActivationChange:
    case LEvent::EnabledChange:
    case LEvent::FontChange:
    case LEvent::StyleChange:
    case LEvent::PaletteChange:
    case LEvent::WindowTitleChange:
    case LEvent::IconTextChange:
    case LEvent::ModifiedChange:
    case LEvent::MouseTrackingChange:
    case LEvent::TabletTrackingChange:
    case LEvent::ParentChange:
    case LEvent::LocaleChange:
    case LEvent::MacSizeChange:
    case LEvent::ContentsRectChange:
    case LEvent::ThemeChange:
    case LEvent::ReadOnlyChange:{
        changeEvent(event);
        break;
    }
    case LEvent::WindowStateChange: {
        std::shared_ptr<LWindowStateChangeEvent> evt = std::dynamic_pointer_cast<LWindowStateChangeEvent>(event);
        const bool wasMinimized = evt->oldState() & LWindowState::Minimized;
        if (wasMinimized != isMinimized()) {
            std::shared_ptr<LWidget> widget = getPtrT<LWidget>();
            if (wasMinimized) {
                // Always send the spontaneous events here, otherwise it can break the application!
                if (!m_childrenShownByExpose) {
                    // Show widgets only when they are not yet shown by the expose event
                    showChildren(true);
                    std::shared_ptr<LEvent> showEvent = std::make_shared<LEvent>(LEvent::Show);
                    LApplication::sendSpontaneousEvent(widget, showEvent);
                }
                m_childrenHiddenByWState = false; // Set it always to "false" when window is restored
            } else {
                std::shared_ptr<LEvent> hideEvent = std::make_shared<LEvent>(LEvent::Hide);
                LApplication::sendSpontaneousEvent(widget, hideEvent);
                hideChildren(true);
                m_childrenHiddenByWState = true;
            }
            m_childrenShownByExpose = false; // Set it always to "false" when window state changes
        }
        changeEvent(event);
        break;
    }
    case LEvent::WindowActivate:
    case LEvent::WindowDeactivate: {
         if (isVisible() && !palette()->isEqual(LPalette::Active, LPalette::Inactive)){
            update();
         }

         for(std::shared_ptr<LTreeObj>& child : m_children){
            std::shared_ptr<LWidget> w = std::dynamic_pointer_cast<LWidget>(child);
            if(w != nullptr){
                if(w->isVisible()&& !w->isWindow()){
                    LCoreApplication::sendEvent(w, event);
                }
            }
        }
        break;
    }
    case LEvent::UpdateRequest:{
        syncBackingStore();
        break;
    }
    case LEvent::UpdateLater:{
        std::shared_ptr<LUpdateLaterEvent> k = std::dynamic_pointer_cast<LUpdateLaterEvent>(event);
        update(k->region());
        break;
    }
    case LEvent::StyleAnimationUpdate:{
        if (isVisible() && !window()->isMinimized()) {
            event->accept();
            update();
        }
        break;
    }
    default:{
        return LTreeObj::handleEvent(event);
    }
    }

    return true;
}

LPoint<int> LWidget::pos() const
{
    LPoint<int> result = m_widgetData.crect.topLeft();
    if (isWindow() && (!(windowType() == LWindowType::Popup))){
        if(m_pExtraData != nullptr)
        {
            if(m_pExtraData->topExtra != nullptr){
                if(m_pExtraData->topExtra->posIncludesFrame){
                    LMargins<int> fs = frameStrut();
                    result.m_x -= fs.m_left;
                    result.m_y -= fs.m_top;
                }
            }
        }
    }
    return result;
}

LSize<int> LWidget::minimumSize() const
{
    if(m_pExtraData!= nullptr){
        return LSize<int>(m_pExtraData->minw, m_pExtraData->minh);
    }
    return LSize<int>(0, 0);
}

LSize<int> LWidget::maximumSize() const
{
    if(m_pExtraData!= nullptr){
        return LSize<int>(m_pExtraData->maxw, m_pExtraData->maxh);
    }
    return LSize<int>(LWIDGETSIZE_MAX, LWIDGETSIZE_MAX);
}

int LWidget::minimumWidth() const
{
    if(m_pExtraData!= nullptr){
        return m_pExtraData->minw;
    }
    return 0;
}

int LWidget::minimumHeight() const
{
    if(m_pExtraData!= nullptr){
        return m_pExtraData->minh;
    }
    return 0;
}

int LWidget::maximumWidth() const
{
    if(m_pExtraData!= nullptr){
        return m_pExtraData->maxw;
    }
    return LWIDGETSIZE_MAX;
}

int LWidget::maximumHeight() const
{
    if(m_pExtraData!= nullptr){
        return m_pExtraData->maxh;
    }
    return LWIDGETSIZE_MAX;
}


void LWidget::setMinimumSize(int minw, int minh)
{
    if (!setMinimumSizeHelper(minw, minh)){
        return;
    }

    if (isWindow()){
        setConstraintsSys();
    }
    if (minw > width() || minh > height()) {
        bool resized = testAttribute(LWidgetAttribute::Resized);
        bool maximized = isMaximized();
        resize(std::max(minw,width()), std::max(minh,height()));
        setAttribute(LWidgetAttribute::Resized, resized); //not a user resize
        if (maximized){
            m_widgetData.windowState = m_widgetData.windowState | uint32_t(LWindowState::Maximized);
        }
    }
    updateGeometryHelper((m_pExtraData->minw == m_pExtraData->maxw)
                         && (m_pExtraData->minh == m_pExtraData->maxh));
}

void LWidget::setMaximumSize(int maxw, int maxh)
{
    if (!setMaximumSizeHelper(maxw, maxh)){
        return;
    }

    if (isWindow()){
        setConstraintsSys();
    }
    if (maxw < width() || maxh < height()) {
        bool resized = testAttribute(LWidgetAttribute::Resized);
        resize(std::min(maxw,width()), std::min(maxh,height()));
        setAttribute(LWidgetAttribute::Resized, resized); //not a user resize
    }

    updateGeometryHelper(m_pExtraData->minw == m_pExtraData->maxw
                         && m_pExtraData->minh == m_pExtraData->maxh);
}

void LWidget::setMinimumWidth(int w)
{
    createExtra();
    uint32_t expl = m_pExtraData->explicitMinSize | (w ? (uint32_t)LOrientation::Horizontal : 0);
    setMinimumSize(w, minimumSize().height());
    m_pExtraData->explicitMinSize = expl;
}

void LWidget::setMinimumHeight(int h)
{
    createExtra();
    uint32_t expl = m_pExtraData->explicitMinSize | (h ? (uint32_t)LOrientation::Vertical : 0);
    setMinimumSize(minimumSize().width(), h);
    m_pExtraData->explicitMinSize = expl;
}

void LWidget::setMaximumWidth(int w)
{
    createExtra();
    uint32_t expl = m_pExtraData->explicitMaxSize | (w == LWIDGETSIZE_MAX ? 0 : (uint32_t)LOrientation::Horizontal);
    setMaximumSize(w, maximumSize().height());
    m_pExtraData->explicitMaxSize = expl;
}

void LWidget::setMaximumHeight(int h)
{
    createExtra();
    uint32_t expl = m_pExtraData->explicitMaxSize | (h == LWIDGETSIZE_MAX ? 0 : (uint32_t)LOrientation::Vertical);
    setMaximumSize(maximumSize().width(), h);
    m_pExtraData->explicitMaxSize = expl;
}

void LWidget::setFixedSize(const LSize<int> &s)
{
    setFixedSize(s.m_x, s.m_y);
}

void LWidget::setFixedSize(int w, int h)
{
    bool minSizeSet = setMinimumSizeHelper(w, h);
    bool maxSizeSet = setMaximumSizeHelper(w, h);
    if (!minSizeSet && !maxSizeSet){
        return;
    }

    if (isWindow()){
        setConstraintsSys();
    }else{
        updateGeometryHelper(true);
    }

    if (w != LWIDGETSIZE_MAX || h != LWIDGETSIZE_MAX){
        resize(w, h);
    }
}

void LWidget::setFixedWidth(int w)
{
    createExtra();
    uint32_t explMin = m_pExtraData->explicitMinSize | (uint32_t)LOrientation::Horizontal;
    uint32_t explMax = m_pExtraData->explicitMaxSize | (uint32_t)LOrientation::Horizontal;
    setMinimumSize(w, minimumSize().height());
    setMaximumSize(w, maximumSize().height());
    m_pExtraData->explicitMinSize = explMin;
    m_pExtraData->explicitMaxSize = explMax;

}

void LWidget::setFixedHeight(int h)
{
    createExtra();
    uint32_t explMin = m_pExtraData->explicitMinSize | (uint32_t)LOrientation::Vertical;
    uint32_t explMax = m_pExtraData->explicitMaxSize | (uint32_t)LOrientation::Vertical;
    setMinimumSize(minimumSize().width(), h);
    setMaximumSize(maximumSize().width(), h);
    m_pExtraData->explicitMinSize = explMin;
    m_pExtraData->explicitMaxSize = explMax;
}

LSize<int> LWidget::sizeIncrement() const
{
    LSize<int> ret;
    if(m_pExtraData != nullptr){
        if(m_pExtraData->topExtra!= nullptr){
            ret.m_x = m_pExtraData->topExtra->incw;
            ret.m_y = m_pExtraData->topExtra->inch;
        }
    }
    return ret;
}

void LWidget::setSizeIncrement(int w, int h)
{
    createTLExtra();
    LTLWExtra* x = topData();
    if (x->incw == w && x->inch == h){
        return;
    }
    x->incw = w;
    x->inch = h;
    if (isWindow()){
        setConstraintsSys();
    }
}

LSize<int> LWidget::baseSize() const
{
    LSize<int> ret;
    if(m_pExtraData != nullptr){
        if(m_pExtraData->topExtra!= nullptr){
            ret.m_x = m_pExtraData->topExtra->basew;
            ret.m_y = m_pExtraData->topExtra->baseh;
        }
    }
    return ret;
}


void LWidget::setBaseSize(int basew, int baseh)
{
    createTLExtra();
    LTLWExtra* x = topData();
    if (x->basew == basew && x->baseh == baseh){
        return;
    }
    x->basew = basew;
    x->baseh = baseh;
    if (isWindow()){
        setConstraintsSys();
    }
}

void LWidget::resize(const LSize<int> &s)
{
    setAttribute(LWidgetAttribute::Resized);
    if (testAttribute(LWidgetAttribute::WStateCreated)) {
        fixPosIncludesFrame();
        setGeometrySys(m_widgetData.crect.m_x,
                       m_widgetData.crect.m_y,
                       s.width(), s.height(), false);
        setDirtyOpaqueRegion();
    } else {
        const auto oldRect = m_widgetData.crect;
        m_widgetData.crect.setSize(s.boundedTo(maximumSize()).expandedTo(minimumSize()));
        if (oldRect != m_widgetData.crect){
            if(m_layout != nullptr){
                m_layout->invalidate();
            }
        }
    }
}

const LSizePolicy& LWidget::sizePolicy() const
{
    return m_sizePolicy;
}

void LWidget::setSizePolicy(LSizePolicy sp)
{
    m_sizePolicy = sp;
}

WId LWidget::winId() const
{
    if (!m_widgetData.inDestructor
        && (!testAttribute(LWidgetAttribute::WStateCreated) || !internalWinId()))
    {
        LWidget *that = const_cast<LWidget*>(this);
        that->setAttribute(LWidgetAttribute::NativeWindow);
        that->createWinId();
        return that->m_widgetData.winId;
    }
    return m_widgetData.winId;
}

WId LWidget::effectiveWinId() const
{
    const WId id = internalWinId();
    if (id || !testAttribute(LWidgetAttribute::WStateCreated)){
        return id;
    }
    if (const std::shared_ptr<LWidget> realParent = nativeParentWidget()){
        return realParent->internalWinId();
    }
    return 0;
}


void LWidget::createWinId()
{
    const bool forceNativeWindow = testAttribute(LWidgetAttribute::NativeWindow);
    if (!testAttribute(LWidgetAttribute::WStateCreated) || (forceNativeWindow && !internalWinId())) {
        if (!isWindow()) {
            std::shared_ptr<LWidget> parent = parentT<LWidget>();
            if(parent != nullptr){
                if (forceNativeWindow && !testAttribute(LWidgetAttribute::DontCreateNativeAncestors)){
                    parent->setAttribute(LWidgetAttribute::NativeWindow);
                }
                if (!parent->internalWinId()) {
                    parent->createWinId();
                }

                for(std::shared_ptr<LTreeObj>& ch: parent->m_children){
                    std::shared_ptr<LWidget> w = std::dynamic_pointer_cast<LWidget>(ch);
                    if(w != nullptr){
                        if(!w->isWindow() && (!w->testAttribute(LWidgetAttribute::WStateCreated)
                                               || (!w->internalWinId() && w->testAttribute(LWidgetAttribute::NativeWindow)))){
                            w->create();
                        }
                    }
                }
            }
        } else {
            create();
        }
    }
}

void LWidget::setWindowIcon(std::shared_ptr<LTexture> icon)
{
    setAttribute(LWidgetAttribute::SetWindowIcon, icon != nullptr);
    createTLExtra();

    m_pExtraData->topExtra->icon = icon;

    setWindowIconSys();
    setWindowIconHelper();
}

std::shared_ptr<LTexture> LWidget::windowIcon() const
{
    std::shared_ptr<LWidget> w = getPtrT<LWidget>();
    while (w != nullptr) {
        if(m_pExtraData != nullptr){
            if(m_pExtraData->topExtra != nullptr){
                if(m_pExtraData->topExtra->icon != nullptr){
                    return m_pExtraData->topExtra->icon;
                }
            }
        }
        w = w->parentT<LWidget>();
    }
    return LApplication::windowIcon();
}

LMargins<int> LWidget::frameStrut() const
{
    if (!isWindow() || (windowType() == LWindowType::Desktop) || testAttribute(LWidgetAttribute::DontShowOnScreen)) {
        return LMargins<int>();
    }

    if (m_widgetData.fstrutDirty
        && isVisible()
        && testAttribute(LWidgetAttribute::WStateCreated)){
        const_cast<LWidget*>(this)->updateFrameStrut();
    }

    if( m_pExtraData != nullptr){
        if(m_pExtraData->topExtra != nullptr){
            return m_pExtraData->topExtra->frameStrut;
        }
    }
    return LMargins<int>();
}

void LWidget::updateFrameStrut()
{
    if (m_widgetData.fstrutDirty) {
        if (LTLWExtra *te = maybeTopData()) {
            if ((te->window != nullptr)
                && (te->window->handle() != nullptr)) {
                const LMargins<int> margins = te->window->frameMargins();
                if (!margins.isNull()) {
                    te->frameStrut = margins;
                    m_widgetData.fstrutDirty = false;
                }
            }
        }
    }
}

void LWidget::setGeometrySys(int x, int y, int w, int h, bool isMove)
{
    if (m_pExtraData != nullptr) {                                // any size restrictions?
        w = std::min(w,m_pExtraData->maxw);
        h = std::min(h,m_pExtraData->maxh);
        w = std::max(w,m_pExtraData->minw);
        h = std::max(h,m_pExtraData->minh);
    }

    std::shared_ptr<LWindow>  wh = windowHandle();
    if(wh != nullptr){
        if (isWindow()) {
            LPlatformIntegration *integration = LGuiApplication::platformIntegration();
            if (!integration->hasCapability(LPlatformCapability::NonFullScreenWindows)) {
                x = 0;
                y = 0;
                w = wh->width();
                h = wh->height();
            }
        }
    }

    LPoint<int> oldp = geometry().topLeft();
    LSize<int> olds = size();
    LRect<int> r(x, y, w, h, false);

    bool isResize = olds != r.size();
    if (!isMove){
        isMove = oldp != r.topLeft();
    }


    // We only care about stuff that changes the geometry, or may
    // cause the window manager to change its state
    if (r.size() == olds && oldp == r.topLeft()){
        return;
    }

    if (!m_widgetData.inSetWindowState) {
        m_widgetData.windowState &= ~uint32_t(LWindowState::Maximized);
        m_widgetData.windowState &= ~uint32_t(LWindowState::FullScreen);
    }

    LPoint<int> oldPos = pos();
    m_widgetData.crect = r;

    bool needsShow = false;

    if (isWindow() || wh != nullptr) {
        if (!(m_widgetData.windowState & uint32_t(LWindowState::FullScreen)) && (w == 0 || h == 0)) {
            setAttribute(LWidgetAttribute::OutsideWSRange, true);
            if (isVisible()){
                hideSys();
            }
            m_widgetData.crect = LRect(x, y, w, h, false);
        } else if (testAttribute(LWidgetAttribute::OutsideWSRange)) {
            setAttribute(LWidgetAttribute::OutsideWSRange, false);
            needsShow = true;
        }
    }

    if (isVisible()) {
        if (!testAttribute(LWidgetAttribute::DontShowOnScreen) && !testAttribute(LWidgetAttribute::OutsideWSRange)) {
            std::shared_ptr<LWindow> win = wh;
            if (win != nullptr) {
                if (isWindow()) {
                    if (isResize && !isMove){
                        win->resize(LSize<int>(w, h));
                    }
                    else if (isMove && !isResize){
                        win->setPosition(x, y);
                    }
                    else{
                        win->setGeometry(geometry());
                    }
                } else {
                    std::shared_ptr<LWidget> npw = nativeParentWidget();
                    LPoint<int> posInNativeParent =  mapTo(npw,LPoint<int>());
                    win->setGeometry(LRect<int>(posInNativeParent,r.size()));
                }

                if (needsShow){
                    showSys();
                }
            }

            if (!isWindow()) {
                if (m_renderToTexture) {

                    std::shared_ptr<LWidget> pw = parentT<LWidget>();
                    if(pw != nullptr){
                        std::shared_ptr<LRegion> updateRegion
                            = std::make_shared<LRegion>(geometry());
                        updateRegion->unite(LRect<int>(oldPos, olds));
                        pw->invalidateBackingStore(updateRegion);
                    }
                } else if (isMove && !isResize) {
                    moveRect(LRect<int>(oldPos, olds), x - oldPos.m_x, y - oldPos.m_y);
                } else {
                    invalidateBackingStoreResizeHelper(oldPos, olds);
                }
            }
        }

        if (isMove) {
            std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();
            std::shared_ptr<LMoveEvent> e
                = std::make_shared<LMoveEvent>(pos(), oldPos);
            LCoreApplication::sendEvent(tThis, e);
        }
        if (isResize) {
            std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();
            std::shared_ptr<LResizeEvent> e
                = std::make_shared<LResizeEvent>(r.size(), olds);
            LCoreApplication::sendEvent(tThis, e);
            if (wh != nullptr){
                update();
            }
        }
    } else { // not visible
        if (isResize){
            if(m_layout != nullptr){
                m_layout->invalidate();
            }
        }
    }
}

void LWidget::setDirtyOpaqueRegion()
{
    m_dirtyOpaqueChildren = true;

    if (isWindow()){
        return;
    }

    std::shared_ptr<LWidget> parent = parentT<LWidget>();
    if (parent == nullptr){
        return;
    }

    if(parent->m_dirtyOpaqueChildren == false){
        parent->setDirtyOpaqueRegion();
    }
}

void LWidget::moveRect(const LRect<int> &rect, int dx, int dy)
{
    if (!isVisible() || (dx == 0 && dy == 0)){
        return;
    }

    std::shared_ptr<LWidget> tlw = window();
    LTLWExtra* x = tlw->topData();

    static const bool accelEnv = true;

    std::shared_ptr<LWidget> pw = parentT<LWidget>();
    LPoint<int> toplevelOffset = pw->mapTo(tlw, LPoint<int>());

    LRect<int> clipR(pw->clipRect());
    const LRect<int> newRect(rect.translated(dx, dy));
    LRect<int> destRect = rect.intersected(clipR);
    if (destRect.isValid()){
        destRect = destRect.translated(dx, dy).intersected(clipR);
    }

    std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();
    const LRect<int> sourceRect(destRect.translated(-dx, -dy));
    const LRect<int> parentRect(rect & clipR);
    const bool nativeWithTextureChild = m_textureChildSeen && hasPlatformWindow(tThis);

    const bool accelerateMove = accelEnv && m_isOpaque && !nativeWithTextureChild;

    if (!accelerateMove) {
        std::shared_ptr<LRegion> parentR
            = std::make_shared<LRegion>(parentRect);
        if ((m_pExtraData == nullptr) || (m_pExtraData->mask == nullptr)) {
            parentR->subtract(newRect);
        } else {
            parentR->unite(newRect & clipR);
        }
        pw->invalidateBackingStore(parentR);
        invalidateBackingStore((newRect & clipR).translated(-m_widgetData.crect.topLeft()));
    } else {

        LWidgetRepaintMan *repaintManager = x->repaintMan.get();
        std::shared_ptr<LRegion> childExpose = std::make_shared<LRegion>(newRect & clipR);
        std::shared_ptr<LRegion> overlappedExpose;

        if (sourceRect.isValid()) {
            std::shared_ptr<LRegion> overlappedExpose = overlappedRegion(sourceRect);
            overlappedExpose->unite(overlappedRegion(destRect));
            overlappedExpose->intersect(clipR);

            if(overlappedExpose != nullptr){
                std::shared_ptr<LWindow> wh = windowHandle();

                const double factor = LHighDpiScaling::factor(wh.get());
                if (overlappedExpose->isEmpty() || floor(factor) == factor) {

                    std::shared_ptr<LRegion> t = std::make_shared<LRegion>(sourceRect);
                    t->subtract(overlappedExpose);

                    const std::vector<LRect<int> > rectsToScroll = getSortedRectsToScroll(t, dx, dy);
                    for (const LRect<int>& rect : rectsToScroll) {
                        if (repaintManager->bltRect(rect, dx, dy, pw)) {
                            childExpose->subtract(rect.translated(dx, dy));
                        }
                    }
                }

                childExpose->subtract(overlappedExpose);
            }
        }

        if (!pw->updatesEnabled()){
            return;
        }

        const bool childUpdatesEnabled = updatesEnabled();
        if (childUpdatesEnabled) {

            if(overlappedExpose != nullptr){
                if (!overlappedExpose->isEmpty()) {
                    overlappedExpose->translate(-m_widgetData.crect.topLeft());
                    invalidateBackingStore(overlappedExpose);
                }
            }
            if (!childExpose->isEmpty()) {              
                childExpose->translate(-m_widgetData.crect.topLeft());
                repaintManager->markDirty(childExpose, tThis);
                m_isMoved = true;
            }
        }

        std::shared_ptr<LRegion> parentExpose = std::make_shared<LRegion>(parentRect);
        parentExpose->subtract(newRect);
        if(m_pExtraData != nullptr){
            if (m_pExtraData->mask != nullptr){
                std::shared_ptr<LRegion> t = std::make_shared<LRegion>(newRect);
                std::shared_ptr<LRegion> tt = m_pExtraData->mask->translated(m_widgetData.crect.topLeft());
                t->subtract(tt);
                parentExpose->unite(t);
            }
        }

        if (!parentExpose->isEmpty()) {
            repaintManager->markDirty(parentExpose, pw);
            pw->m_isMoved = true;
        }

        if (childUpdatesEnabled) {
            std::shared_ptr<LRegion> needsFlush= std::make_shared<LRegion>(sourceRect);
            needsFlush->unite(destRect);
            repaintManager->markNeedsFlush(pw, needsFlush, toplevelOffset);
        }
    }
}

std::shared_ptr<LRegion> LWidget::overlappedRegion(const LRect<int> &rect, bool breakAfterFirst) const
{
    std::shared_ptr<LWidget> w = getPtrT<LWidget>();
    LRect<int> r = rect;
    LPoint<int> p;
    std::shared_ptr<LRegion> region = std::make_shared<LRegion>();
    while (w) {
        if (w->isWindow()){
            break;
        }
        std::shared_ptr<LWidget> pw = w->parentT<LWidget>();
        bool above = false;
        for (std::shared_ptr<LTreeObj>& ch : pw->m_children) {
            std::shared_ptr<LWidget> sibling = std::dynamic_pointer_cast<LWidget>(ch);
            if (sibling==nullptr || !sibling->isVisible() || sibling->isWindow()){
                continue;
            }
            if (!above) {
                above = (sibling == w);
                continue;
            }

            const LRect<int> siblingRect = sibling->m_widgetData.crect;
            if (siblingRect.isIntersects(r)) {
                const auto &siblingExtra = sibling->m_pExtraData;
                if(siblingExtra != nullptr){
                    if(siblingExtra->mask != nullptr){
                        std::shared_ptr<LRegion> t =
                            siblingExtra->mask->translated(sibling->m_widgetData.crect.topLeft());
                        if(!t->isIntersects(r)){
                            continue;
                        }
                    }
                }


                region->unite( siblingRect.translated(-p));
                if (breakAfterFirst){
                    break;
                }
            }
        }
        w = w->parentT<LWidget>();
        r.translate(pw->m_widgetData.crect.topLeft());
        p += pw->m_widgetData.crect.topLeft();
    }
    return region;
}


void LWidget::showSys()
{
    std::shared_ptr<LWindow> window = windowHandle();

    if (testAttribute(LWidgetAttribute::DontShowOnScreen)) {
        invalidateBackingStore(rect());
        setAttribute(LWidgetAttribute::Mapped);
        // add our window the modal window list (native dialogs)
        if(window != nullptr){
            if (isWindow() && isModal()) {
                LGuiApplication::showModalWindow(window);
            }
        }
        return;
    }

    if (m_renderToTexture && !isWindow()){
        std::shared_ptr<LWidget> p = parentT<LWidget>();
        if(p != nullptr){
            std::shared_ptr<LUpdateLaterEvent> e= std::make_shared<LUpdateLaterEvent>(geometry());
            LCoreApplication::postEvent(p, e);
        }
    }else{
        std::shared_ptr<LWidget> p = getPtrT<LWidget>();
        std::shared_ptr<LUpdateLaterEvent> e= std::make_shared<LUpdateLaterEvent>(rect());
        LCoreApplication::postEvent(p, e);
    }

    if ((!isWindow() && !testAttribute(LWidgetAttribute::NativeWindow))
        || testAttribute(LWidgetAttribute::OutsideWSRange)) {
        return;
    }

    if (window != nullptr) {
        if (isWindow()){
            fixPosIncludesFrame();
        }
        LRect<int> geomRect = geometry();
        if (!isWindow()) {
            LPoint<int> topLeftOfWindow = mapTo(nativeParentWidget(),LPoint<int>());
            geomRect.moveTopLeft(topLeftOfWindow);
        }
        const LRect<int> windowRect = window->geometry();
        if (windowRect != geomRect) {
            if (testAttribute(LWidgetAttribute::Moved)
                || !LGuiApplication::platformIntegration()->hasCapability(LPlatformCapability::WindowManagement)){
                window->setGeometry(geomRect);
            }
            else{
                window->resize(geomRect.size());
            }
        }

        setCursorSys(false);
        invalidateBackingStore(rect());
        window->setVisible(true);
        // Was the window moved by the Window system or LPlatformWindow::initialGeometry() ?
        if (window->isTopLevel()) {
            const LPoint<int> crectTopLeft = m_widgetData.crect.topLeft();
            const LPoint<int> windowTopLeft = window->geometry().topLeft();
            if (crectTopLeft == LPoint<int>(0, 0) && windowTopLeft != crectTopLeft){
                m_widgetData.crect.moveTopLeft(windowTopLeft);
            }
        }
    }
}


void LWidget::showHelper()
{
    m_widgetData.inShow = true;
    // make sure we receive pending move and resize events
    sendPendingMoveAndResizeEvents();

    // become visible before showing all children
    setAttribute(LWidgetAttribute::WStateVisible);

    // finally show all children recursively
    showChildren(false);

    const bool isWin = isWindow();
    bool isEmbedded = false;
    std::shared_ptr<LWidget> q = getPtrT<LWidget>();

    LWindowType winType = windowType();
    // popup handling: new popups and tools need to be raised, and
    // existing popups must be closed. Also propagate the current
    // windows's KeyboardFocusChange status.
    if (isWin && !isEmbedded) {

        if ((winType == LWindowType::Tool)
            || (winType == LWindowType::Popup)
            || (winType == LWindowType::ToolTip)) {
            raise();

            std::shared_ptr<LWidget> p = parentT<LWidget>();
            if(p != nullptr){
                std::shared_ptr<LWidget> pWin = p->window();
                if(pWin != nullptr){
                    if(pWin->testAttribute(LWidgetAttribute::KeyboardFocusChange)){
                        setAttribute(LWidgetAttribute::KeyboardFocusChange);
                    }
                }
            }
        } else {
            std::shared_ptr<LWidget> actPop = LApplication::activePopupWidget();
            while (actPop != nullptr) {
                if (!actPop->close()){ //Todo close
                    break;
                }
            }
        }
    }

    // send the show event before showing the window
    std::shared_ptr<LEvent> showEvent = std::make_shared<LEvent>(LEvent::Show);
    LCoreApplication::sendEvent(q, showEvent);

    showSys();

    if (!isEmbedded && winType == LWindowType::Popup){
        ((LApplication*)lApp)->openPopup(q);
    }

    if (LApplication::m_hiddenFocusWidget.lock() == q) {
        LApplication::m_hiddenFocusWidget.reset();
        setFocus(LFocusReason::Other);
    }

    // Process events when showing a SplashScreen widget before the event loop
    // is spinnning; otherwise it might not show up on particular platforms.
    // This makes SplashScreen behave the same on all platforms.
    if (!lApp->inExec() && winType == LWindowType::SplashScreen){
        LCoreApplication::processEvents();
    }

    m_widgetData.inShow = false;
}

void LWidget::hideSys()
{
    std::shared_ptr<LWindow> window = windowHandle();

    if (testAttribute(LWidgetAttribute::DontShowOnScreen)) {
        setAttribute(LWidgetAttribute::Mapped, false);
        // remove our window from the modal window list (native dialogs)
        if(window != nullptr){
            if (isWindow() && isModal()) {
                LGuiApplication::hideModalWindow(window);
            }
        }
    }

    deactivateWidgetCleanup();

    if (!isWindow()) {
        std::shared_ptr<LWidget> p= parentT<LWidget>();
        if(p != nullptr){
            if (p->isVisible()) {
                if (m_renderToTexture){
                    p->invalidateBackingStore(geometry());
                }
                else{
                    invalidateBackingStore(rect());
                }
            }
        }
    } else {
        invalidateBackingStore(rect());
    }

    if (window){
        window->setVisible(false);
    }
}

void LWidget::hideHelper()
{
    std::shared_ptr<LWidget> q= getPtrT<LWidget>();
    if (windowType() == LWindowType::Popup){
        ((LApplication*)lApp)->closePopup(q);
    }

    setAttribute(LWidgetAttribute::Mapped, false);
    hideSys();

    bool wasVisible = testAttribute(LWidgetAttribute::WStateVisible);

    if (wasVisible) {
        setAttribute(LWidgetAttribute::WStateVisible, false);
    }


    std::shared_ptr<LEvent> hideEvent = std::make_shared<LEvent>(LEvent::Hide);
    LCoreApplication::sendEvent(q, hideEvent);
    hideChildren(false);

    // next bit tries to move the focus if the focus widget is now
    // hidden.
    if (wasVisible) {
        ((LApplication*)lApp)->sendSyntheticEnterLeave(q);
        std::shared_ptr<LWidget> fw = LApplication::focusWidget();
        while ((fw != nullptr) &&  !fw->isWindow()) {
            if (fw == q) {
                q->focusNextPrevChild(true);
                break;
            }
            fw = fw->parentT<LWidget>();
        }
    }

    if (LWidgetRepaintMan *repaintManager = maybeRepaintMan()){
        repaintManager->removeDirtyWidget(q);
    }
}

void LWidget::updateGeometryHelper(bool forceUpdate)
{
    if (forceUpdate || (m_pExtraData == nullptr)
        || (m_pExtraData->minw != m_pExtraData->maxw)
        || (m_pExtraData->minh != m_pExtraData->maxh)) {
        const int isHide = isHidden() && !m_sizePolicy.m_retainSizeWhenHidden && !m_retainSizeWhenHiddenChanged;

        std::shared_ptr<LWidget> parent = parentT<LWidget>();
        if(parent != nullptr){
            if (!isWindow() && !isHide) {
                if (parent->m_layout != nullptr){
                    parent->m_layout->invalidate();
                }
                else if (parent->isVisible()){
                    std::shared_ptr<LEvent> e = std::make_shared<LEvent>(LEvent::LayoutRequest);
                    LCoreApplication::postEvent(parent, e);
                }
            }
        }
    }
}

void LWidget::setEnabledHelper(bool enable)
{
    std::shared_ptr<LWidget> parent = parentT<LWidget>();
    if (enable && !isWindow() && parent && !parent->isEnabled()){
        return; // nothing we can do
    }

    if (enable != testAttribute(LWidgetAttribute::Disabled)){
        return; // nothing to do
    }

    setAttribute(LWidgetAttribute::Disabled, !enable);
    std::shared_ptr<LWidget> win = window();
    std::shared_ptr<LWidget> q = getPtrT<LWidget>();

    if (!enable && win->focusWidget() == q) {
        bool parentIsEnabled = (parent == nullptr || parent->isEnabled());
        if (!parentIsEnabled || !q->focusNextChild()){
            q->clearFocus();
        }
    }

    LWidgetAttribute attribute = enable ? LWidgetAttribute::ForceDisabled : LWidgetAttribute::Disabled;
    for (std::shared_ptr<LTreeObj>& o : m_children) {
        std::shared_ptr<LWidget> w = std::dynamic_pointer_cast<LWidget>(o);
        if (w!=nullptr && !w->testAttribute(attribute)){
            w->setEnabledHelper(enable);
        }
    }

    if (testAttribute(LWidgetAttribute::SetCursor) || isWindow()) {
        // enforce the windows behavior of clearing the cursor on
        // disabled widgets
        setCursorSys(false);
    }

    std::shared_ptr<LEvent> e = std::make_shared<LEvent>(LEvent::EnabledChange);
    LCoreApplication::sendEvent(q, e);
}

void LWidget::setUpdatesEnabledHelper(bool enable)
{

}

bool LWidget::setMinimumSizeHelper(int &minw, int &minh)
{
    int mw = minw, mh = minh;
    if (mw == LWIDGETSIZE_MAX){
        mw = 0;
    }
    if (mh == LWIDGETSIZE_MAX){
        mh = 0;
    }
    if (L_UNLIKELY(minw > LWIDGETSIZE_MAX || minh > LWIDGETSIZE_MAX)) {
        minw = mw = std::min(minw, LWIDGETSIZE_MAX);
        minh = mh = std::min(minh, LWIDGETSIZE_MAX);
    }
    if (L_UNLIKELY(minw < 0 || minh < 0)) {
        minw = mw = std::max(minw, 0);
        minh = mh = std::max(minh, 0);
    }
    createExtra();
    if (m_pExtraData->minw == mw && m_pExtraData->minh == mh){
        return false;
    }
    m_pExtraData->minw = mw;
    m_pExtraData->minh = mh;
    m_pExtraData->explicitMinSize = (mw ? uint32_t(LOrientation::Horizontal) : 0)
                                    | (mh ? uint32_t(LOrientation::Vertical) : 0);
    return true;
}

bool LWidget::setMaximumSizeHelper(int &maxw, int &maxh)
{
    if (L_UNLIKELY(maxw > LWIDGETSIZE_MAX || maxh > LWIDGETSIZE_MAX)) {
        maxw = std::min(maxw, LWIDGETSIZE_MAX);
        maxh = std::min(maxh, LWIDGETSIZE_MAX);
    }
    if (L_UNLIKELY(maxw < 0 || maxh < 0)) {
        maxw = std::max(maxw, 0);
        maxh = std::max(maxh, 0);
    }
    createExtra();
    if (m_pExtraData->maxw == maxw && m_pExtraData->maxh == maxh){
        return false;
    }
    m_pExtraData->maxw = maxw;
    m_pExtraData->maxh = maxh;
    m_pExtraData->explicitMaxSize = (maxw != LWIDGETSIZE_MAX ? uint32_t(LOrientation::Horizontal) : 0)
                                    | (maxh != LWIDGETSIZE_MAX ? uint32_t(LOrientation::Vertical) : 0);
    return true;

}

void LWidget::invalidateBackingStoreResizeHelper(const LPoint<int> &oldPos, const LSize<int> &oldSize)
{
    const bool staticContents = testAttribute(LWidgetAttribute::StaticContents);
    const bool sizeDecreased = (m_widgetData.crect.width() < oldSize.width())
                               || (m_widgetData.crect.height() < oldSize.height());

    const LPoint<int> offset(m_widgetData.crect.m_x - oldPos.m_x, m_widgetData.crect.m_y - oldPos.m_y);
    const bool parentAreaExposed = !offset.isNull() || sizeDecreased;
    const LRect<int> newWidgetRect(rect());
    const LRect<int> oldWidgetRect(0, 0, oldSize.width(), oldSize.height());

    if (!staticContents) {
        std::shared_ptr<LRegion> staticChildren;
        LWidgetRepaintMan *bs = nullptr;
        if (offset.isNull() && (bs = maybeRepaintManager())){
            std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();
            staticChildren = bs->staticContents(tThis, oldWidgetRect);
        }

        bool hasStaticChildren = false;
        if(staticChildren != nullptr){
            hasStaticChildren = !staticChildren->isEmpty();
        }

        if (hasStaticChildren) {
            std::shared_ptr<LRegion> dirty = std::make_shared<LRegion>(newWidgetRect);
            dirty->subtract(staticChildren);
            invalidateBackingStore(dirty);
        } else {
            // Entire widget needs repaint.
            invalidateBackingStore(newWidgetRect);
        }

        if (!parentAreaExposed){
            return;
        }

        std::shared_ptr<LWidget> pw = parentT<LWidget>();

        if(pw != nullptr){
            // Invalidate newly exposed area of the parent.
            if (m_pExtraData != nullptr && m_pExtraData->mask != nullptr) {
                std::shared_ptr<LRegion> parentExpose = m_pExtraData->mask->translated(oldPos);
                parentExpose->intersect(LRect<int>(oldPos, oldSize));
                if (hasStaticChildren){
                    parentExpose->subtract(m_widgetData.crect); // Offset is unchanged, safe to do this.
                }
                pw->invalidateBackingStore(parentExpose);
            } else {
                if (hasStaticChildren) {
                    std::shared_ptr<LRegion> parentExpose
                        = std::make_shared<LRegion>(LRect<int>(oldPos, oldSize));
                    parentExpose->subtract(m_widgetData.crect); // Offset is unchanged, safe to do this.
                    pw->invalidateBackingStore(parentExpose);
                } else {
                    pw->invalidateBackingStore(LRect<int>(oldPos, oldSize));
                }
            }
        }
        return;
    }

    // Move static content to its new position.
    if (!offset.isNull()) {
        if (sizeDecreased) {
            const LSize<int> minSize(std::min(oldSize.width(), m_widgetData.crect.width()),
                                     std::min(oldSize.height(), m_widgetData.crect.height()));
            moveRect(LRect<int>(oldPos, minSize), offset.m_x, offset.m_y);
        } else {
            moveRect(LRect<int>(oldPos, oldSize), offset.m_x, offset.m_y);
        }
    }

    // Invalidate newly visible area of the widget.
    if (!sizeDecreased || !oldWidgetRect.contains(newWidgetRect)) {
        std::shared_ptr<LRegion> newVisible
            = std::make_shared<LRegion>(newWidgetRect);
        newVisible->subtract(oldWidgetRect);
        invalidateBackingStore(newVisible);
    }

    if (!parentAreaExposed){
        return;
    }

    std::shared_ptr<LWidget> pw = parentT<LWidget>();
    // Invalidate newly exposed area of the parent.
    const LRect<int> oldRect(oldPos, oldSize);
    if (m_pExtraData != nullptr && m_pExtraData->mask != nullptr) {
        std::shared_ptr<LRegion> parentExpose = std::make_shared<LRegion>(oldRect);
        parentExpose->intersect(m_pExtraData->mask->translated(oldPos));
        std::shared_ptr<LRegion> t = m_pExtraData->mask->translated(m_widgetData.crect.topLeft());
        t->intersect(m_widgetData.crect);
        parentExpose->subtract(t);
        pw->invalidateBackingStore(parentExpose);
    } else {
        std::shared_ptr<LRegion> parentExpose = std::make_shared<LRegion>(oldRect);
        parentExpose->subtract(m_widgetData.crect);
        pw->invalidateBackingStore(parentExpose);
    }
}

void LWidget::setLayoutDirectionHelper(LLayoutDirection direction)
{
    if ( (direction == LLayoutDirection::RightToLeft) == testAttribute(LWidgetAttribute::RightToLeft)){
        return;
    }
    setAttribute(LWidgetAttribute::RightToLeft, (direction == LLayoutDirection::RightToLeft));
    if (!m_children.empty()) {
        for (std::shared_ptr<LTreeObj>& obj : m_children) {
            std::shared_ptr<LWidget> w = std::dynamic_pointer_cast<LWidget>(obj);
            if(w != nullptr){
                if (!w->isWindow() && !w->testAttribute(LWidgetAttribute::SetLayoutDirection)){
                    w->setLayoutDirectionHelper(direction);
                }
            }
        }
    }
    std::shared_ptr<LEvent> e = std::make_shared<LEvent>(LEvent::LayoutDirectionChange);
    std::shared_ptr<LWidget> q = getPtrT<LWidget>();
    LCoreApplication::sendEvent(q, e);
}

void LWidget::setConstraintsSys()
{
    if(m_pExtraData != nullptr){
        std::shared_ptr<LWindow> win = windowHandle();
        if(win != nullptr){
            win->setMinimumSize(LSize<int>(m_pExtraData->minw, m_pExtraData->minh));
            win->setMaximumSize(LSize<int>(m_pExtraData->maxw, m_pExtraData->maxh));

            if (m_pExtraData->topExtra != nullptr) {
                win->setBaseSize(LSize<int>(m_pExtraData->topExtra->basew, m_pExtraData->topExtra->baseh));
                win->setSizeIncrement(LSize<int>(m_pExtraData->topExtra->incw, m_pExtraData->topExtra->inch));
            }

            std::shared_ptr<LPlatformWindow> p = win->platformWindow();
            if (p != nullptr) {
                fixPosIncludesFrame();
                p->propagateSizeHints();
            }
        }
    }
}

void LWidget::fixPosIncludesFrame()
{
    if (LTLWExtra *te = maybeTopData()) {
        if (te->posIncludesFrame) {
            // For LWidgetAttribute::DontShowOnScreen, assume a frame of 0.
            if (testAttribute(LWidgetAttribute::DontShowOnScreen)) {
                te->posIncludesFrame = 0;
            } else {
                std::shared_ptr<LWindow> winH = windowHandle();
                if(winH != nullptr){
                    if(winH->handle()){
                        updateFrameStrut();
                        if (!m_widgetData.fstrutDirty) {
                            m_widgetData.crect.translate(te->frameStrut.m_left, te->frameStrut.m_top);
                            te->posIncludesFrame = 0;
                        }
                    }
                }
            }
        }
    }

}

void LWidget::setMaskSys(std::shared_ptr<LRegion> mask)
{
    std::shared_ptr<LWindow> win = windowHandle();
    if (win != nullptr){
        win->setMask(mask);
    }
}

void LWidget::setWindowModifiedHelper()
{
    std::shared_ptr<LWindow> window = windowHandle();
    if (window == nullptr){
        return;
    }
    std::shared_ptr<LPlatformWindow> platformWindow = window->handle();
    if (platformWindow == nullptr){
        return;
    }
    bool on = testAttribute(LWidgetAttribute::WindowModified);
    if (!platformWindow->setWindowModified(on)) {
        setWindowTitleHelper(windowTitle());
    }
}

void LWidget::setWinId(WId id)
{
    std::shared_ptr<LDesktopWidget> desktop = LApplication::desktop();
    std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();
    bool userDesktopWidget = desktop != nullptr
                             && desktop != tThis && tThis->windowType() == LWindowType::Desktop;

    if (m_widgetData.winId && !userDesktopWidget) {
        LApplication::m_mapper.erase(m_widgetData.winId);
    }

    const WId oldWinId = m_widgetData.winId;

    m_widgetData.winId = id;
    if (id && !userDesktopWidget) {
        LApplication::m_mapper[id] = tThis;
    }

    if(oldWinId != id) {
        std::shared_ptr<LEvent> e = std::make_shared<LEvent>(LEvent::WinIdChange);
        LCoreApplication::sendEvent(tThis, e);
    }
}

void LWidget::resolveFont()
{

}

void LWidget::resolvePalette()
{

}

void LWidget::resolveLayoutDirection()
{
    if (!testAttribute(LWidgetAttribute::SetLayoutDirection)){
        if(isWindow()){
            setLayoutDirectionHelper(LGuiApplication::layoutDirection());
        }else{
            std::shared_ptr<LWidget> p =  parentT<LWidget>();
            if(p != nullptr){
                setLayoutDirectionHelper(p->layoutDirection());
            }else{
                setLayoutDirectionHelper(LGuiApplication::layoutDirection());
            }
        }
    }
}

void LWidget::resolveLocale()
{

}

void LWidget::inheritStyle()
{

}

bool LWidget::shouldDiscardSyncRequest() const
{
    return (maybeTopData()== nullptr) || !testAttribute(LWidgetAttribute::Mapped) || !isVisible();
}

bool LWidget::shouldPaintOnScreen() const
{
    if (testAttribute(LWidgetAttribute::PaintOnScreen)){
        return true;
    }

    if(!isWindow()) {
        std::shared_ptr<LWidget> w = window();
        if(w->testAttribute(LWidgetAttribute::PaintOnScreen)){
            return true;
        }
    }
    return false;
}

void LWidget::paintOnScreen(const std::shared_ptr<LRegion> rgn)
{
    if (m_widgetData.inDestructor){
        return;
    }

    if (shouldDiscardSyncRequest()){
        return;
    }

    std::shared_ptr<LPaintDevice> dev = paintScreenDevice();
    if(dev == nullptr){
        return;
    }

    std::shared_ptr<LPaintEngine> engine = dev->paintEngine();
    if (testAttribute(LWidgetAttribute::StaticContents)) {
        if (m_pExtraData == nullptr){
            createExtra();
        }
        m_pExtraData->staticContentsSize = m_widgetData.crect.size();
    }


    rgn->intersect(clipRect());
    clipToEffectiveMask(rgn);
    if (rgn->isEmpty()){
        return; // Nothing to repaint.
    }

    drawWidget(dev,
               rgn,
               LPoint<int>(),
               LDrawWidgetOpt::DrawAsRoot | LDrawWidgetOpt::DrawPaintOnScreen,
               nullptr);

    if (L_UNLIKELY(dev->paintingActive())){
        lWarn() << "LWidget::repaint: It is dangerous to leave painters active on a widget outside of the PaintEvent";
    }

}

std::shared_ptr<LScreen> LWidget::screen() const
{
    std::shared_ptr<LScreen> s;
    std::shared_ptr<LWindow> w = windowHandle(WinHandleMode::Closest);
    if(w !=nullptr){
        s = w->screen();
        if(s!= nullptr){
            return s;
        }
    }

    std::shared_ptr<LWidget> topLevel = window();
    if(topLevel != nullptr) {
        s = LGuiApplication::screenAt(topLevel->geometry().center());
        if (s != nullptr){
            return s;
        }
    }
    return LGuiApplication::primaryScreen();
}

bool LWidget::setScreen(std::shared_ptr<LScreen> s)
{
    if ((s == nullptr) || !isWindow()){
        return false;
    }
    const std::shared_ptr<LScreen> currentScreen = windowHandle() ? windowHandle()->screen() : nullptr;
    if (currentScreen != s) {
        std::shared_ptr<LWindow> w = windowHandle();
        if (w != nullptr){
            w->setScreen(s);
        }
        return true;
    }
    return false;
}

bool LWidget::setScreenForPoint(const LPoint<int> &pos)
{
    if (!isWindow()){
        return false;
    }
    // Find the screen for pos and make the widget understand it is on that screen.
    return setScreen(LGuiApplication::screenAt(pos));
}

LVariant LWidget::inputMethodQuery(LInputMethodQuery query, const LVariant& args) const
{
    switch(query) {
    case LInputMethodQuery::ImCursorRectangle:{
        return LRect<int>(width()/2, 0, 1, height()).toCbor();
    }
    case LInputMethodQuery::ImFont:{
        std::shared_ptr<LIVar> var = font();
        return var;
    }
    case LInputMethodQuery::ImAnchorPosition:{
        return inputMethodQuery(LInputMethodQuery::ImCursorPosition);
    }
    case LInputMethodQuery::ImHints:{
        return (int)inputMethodHints();
    }
    case LInputMethodQuery::ImInputItemClipRectangle:{
        return clipRect().toCbor();
    }
    default:
        return LVariant();
    }
}

LFlags<LInputMethodHint> LWidget::inputMethodHints() const
{
    std::shared_ptr<LWidget> q = getPtrT<LWidget>();
    while (q->m_inheritsInputMethodHints) {
        std::shared_ptr<LWidget> t = q->parentT<LWidget>();
        if(t != nullptr){
            q = t;
        }else{
            break;
        }
    }
    return q->m_imHints;
}

void LWidget::setInputMethodHints(LFlags<LInputMethodHint> hints)
{
    if (m_imHints == hints){
        return;
    }
    m_imHints = hints;
    if (this == LGuiApplication::focusObject().get()){
        LGuiApplication::inputMethod()->update(LInputMethodQuery::ImHints);
    }
}

void LWidget::setPaintLayout(bool isPaint)
{
    if((bool)(m_paintLayout) != isPaint){
        m_paintLayout = isPaint ? 1 : 0;
        if(m_layout != nullptr && isVisible()){
            update();
        }
    }
}

LRect<int> LWidget::childrenRect() const
{
    LRect<int> r(0, 0, 0, 0);
    for (const std::shared_ptr<LTreeObj>& c : m_children) {
        std::shared_ptr<LWidget> w = std::dynamic_pointer_cast<LWidget>(c);
        if(w != nullptr){
            if (!w->isWindow() && !w->isHidden()){
                r |= w->geometry();
            }
        }
    }
    return r;
}

std::shared_ptr<LRegion> LWidget::childrenRegion() const
{
    std::shared_ptr<LRegion> r;
    for (const std::shared_ptr<LTreeObj>& c : m_children) {
        std::shared_ptr<LWidget> w = std::dynamic_pointer_cast<LWidget>(c);
        if(w != nullptr){
            if (!w->isWindow() && !w->isHidden()) {
                std::shared_ptr<LRegion> mask = w->mask();
                if (mask== nullptr || mask->isEmpty()){
                    r->unite(w->geometry());
                }else{
                    r->unite(mask->translated(w->pos()));
                }
            }
        }
    }
    return r;
}

void LWidget::setMask(std::shared_ptr<LRegion> newMask)
{

    createExtra();
    if (newMask == m_pExtraData->mask){
        return;
    }

    std::shared_ptr<LRegion> oldMask = m_pExtraData->mask;
    m_pExtraData->mask = newMask;
    if (!testAttribute(LWidgetAttribute::WStateCreated)){
        return;
    }

    setMaskSys(newMask);
    if (!isVisible()){
        return;
    }

    if (m_pExtraData->mask == nullptr) {
        // Mask was cleared; update newly exposed area.
        std::shared_ptr<LRegion> expose = std::make_shared<LRegion>(rect());
        expose->subtract(oldMask);
        if (!expose->isEmpty()) {
            setDirtyOpaqueRegion();
            update(expose);
        }
        return;
    }

    if (!isWindow()) {
        // Update newly exposed area on the parent widget.
        std::shared_ptr<LRegion> parentExpose = std::make_shared<LRegion>(rect());
        parentExpose->subtract(newMask);
        if (!parentExpose->isEmpty()) {
            setDirtyOpaqueRegion();
            std::shared_ptr<LWidget> p =parentT<LWidget>();
            if(p != nullptr){
                parentExpose->translate(m_widgetData.crect.topLeft());
                p->update(parentExpose);
            }
        }
        // Update newly exposed area on this widget
        if(oldMask != nullptr && newMask != nullptr){
            if (!oldMask->isEmpty()){
                std::shared_ptr<LRegion> t = newMask->subtracted(oldMask);
                update(t);
            }
        }
    }
}

std::shared_ptr<LRegion> LWidget::mask() const
{
    if (m_pExtraData!=nullptr){
        return m_pExtraData->mask;
    }
    return nullptr;
}

void LWidget::clearMask()
{
    if (m_pExtraData==nullptr || m_pExtraData->mask == nullptr){
        return;
    }
    setMask(nullptr);
}

void LWidget::updateWidgetTransform(std::shared_ptr<LEvent> event)
{
    std::shared_ptr<LObject> focus = LGuiApplication::focusObject();
    if (this == focus.get() || event->type() == LEvent::FocusIn) {

        std::shared_ptr<LWidget> w = window();
        LTransform2D t;
        LPoint<int> p = mapTo(w, LPoint<int>(0,0));
        t.translate(p.m_x, p.m_y);

        std::shared_ptr<LInputMethod> input = LGuiApplication::inputMethod();
        input->setInputItemTransform(t);
        input->setInputItemRectangle(rectF());
        input->update(LInputMethodQuery::ImInputItemClipRectangle);
    }
}

void LWidget::clipToEffectiveMask(std::shared_ptr<LRegion> region) const
{
    std::shared_ptr<LWidget> q = getPtrT<LWidget>();
    std::shared_ptr<LWidget> w = q;

    LPoint<int> offset;

    while (w!= nullptr) {
        if(w->m_pExtraData != nullptr){
            if(w->m_pExtraData->mask != nullptr){
                if(w!= q){
                    region->intersect(w->m_pExtraData->mask->translated(offset));
                }else{
                    region->intersect(w->m_pExtraData->mask);
                }
            }
        }

        if (w->isWindow()){
            return;
        }
        offset -= w->m_widgetData.crect.topLeft();
        w = w->parentT<LWidget>();
    }
}

void LWidget::syncBackingStore()
{
    if (shouldPaintOnScreen()) {
        paintOnScreen(m_dirty);
        m_dirty = nullptr;
    } else if (LWidgetRepaintMan *repaintManager = maybeRepaintMan()) {
        repaintManager->sync(); //正常界面更新走这边
    }
}

void LWidget::syncBackingStore(const std::shared_ptr<LRegion> region)
{
    if (shouldPaintOnScreen()){
        paintOnScreen(region);
    }
    else if (LWidgetRepaintMan *repaintManager = maybeRepaintMan()) {
        std::shared_ptr<LWidget> q= getPtrT<LWidget>();
        repaintManager->sync(q, region); //正常界面更新走这边
    }
}

std::shared_ptr<LBackingStore> LWidget::backingStore() const
{
    LTLWExtra *extra = maybeTopData();
    if (extra){
        if(extra->backingStore != nullptr){
            return extra->backingStore;
        }
    }

    LWidgetRepaintMan *repaintManager = maybeRepaintMan();
    if (repaintManager == nullptr){
        return nullptr;
    }
    return repaintManager->backingStore();
}

void LWidget::setBackingStore(std::shared_ptr<LBackingStore> store)
{
    if (!isWindow()){
        return;
    }

    LTLWExtra *extra = topData();
    if (extra->backingStore == store){
        return;
    }

    extra->backingStore = store;
    LWidgetRepaintMan *repaintManager = maybeRepaintMan();
    if (repaintManager == nullptr){
        return;
    }
    repaintManager->setBackingStore(store);
}

LRect<int> LWidget::clipRect() const
{
    if (!isVisible()){
        return LRect<int>();
    }

    LRect<int> r = rect();
    int ox = 0;
    int oy = 0;

    std::shared_ptr<LWidget> w = getPtrT<LWidget>();
    while (w->isVisible()
           && !w->isWindow()) {
        std::shared_ptr<LWidget> p = w->parentT<LWidget>();
        if(p!= nullptr){
            ox -= w->x();
            oy -= w->y();
            w = p;
            r &= LRect<int>(ox, oy, w->width(), w->height(), false);
        }else{
            break;
        }
    }
    return r;
}

/*
  Returns the widget's clipping region (without siblings).
*/
std::shared_ptr<LRegion> LWidget::clipRegion() const
{
    if (!isVisible()){
        return nullptr;
    }

    std::shared_ptr<LRegion> r = std::make_shared<LRegion>(rect());
    std::shared_ptr<LWidget> w = getPtrT<LWidget>();
    std::shared_ptr<LWidget> ignoreUpTo;
    int ox = 0;
    int oy = 0;
    while (w->isVisible()
           && !w->isWindow()) {

        std::shared_ptr<LWidget> p = w->parentT<LWidget>();
        if(p!= nullptr){
            ox -= w->x();
            oy -= w->y();
            ignoreUpTo = w;
            w = p;
            r->intersect(LRect<int>(ox, oy, w->width(), w->height(), false));

            auto iter = w->m_children.begin();
            for(; iter != w->m_children.end(); ){
                if((*iter).get() == ignoreUpTo.get()){
                    ++iter;
                    break;
                }else{
                    ++iter;
                }
            }

            for(; iter !=w->m_children.end(); ++iter){
                std::shared_ptr<LWidget> sibling = std::dynamic_pointer_cast<LWidget>(*iter);
                if(sibling != nullptr){
                    if(sibling->isVisible() && !sibling->isWindow()){
                        LRect<int> siblingRect(ox+sibling->x(),
                                               oy+sibling->y(),
                                               sibling->width(),
                                               sibling->height(),
                                               false);
                        if (siblingRect.isIntersects(rect())){
                            r->subtract(siblingRect);
                        }
                    }
                }
            }
        }else{
            break;
        }
    }
    return r;
}

std::shared_ptr<LRegion> LWidget::getOpaqueChildren()
{
    if (!m_dirtyOpaqueChildren){
        return m_opaqueChildren;
    }

    m_opaqueChildren = std::make_shared<LRegion>();
    for(std::shared_ptr<LTreeObj>& tc : m_children){
        std::shared_ptr<LWidget> child = std::dynamic_pointer_cast<LWidget>(tc);
        if(child == nullptr){
            continue;
        }

        if((!child->isVisible()) || child->isWindow()){
            continue;
        }

        const LPoint<int> offset = child->geometry().topLeft();


        std::shared_ptr<LRegion> r = nullptr;
        if(child->m_isOpaque){
            r = std::make_shared<LRegion>(child->rect());
        }else{
            r = std::make_shared<LRegion>(child->getOpaqueChildren());
        }

        if(child->m_pExtraData != nullptr){
            if(child->m_pExtraData->mask != nullptr){
                r->intersect(child->m_pExtraData->mask);
            }
        }

        if(r->isEmpty()){
            continue;
        }
        r->translate(offset);
        m_opaqueChildren->unite(r);
    }

    m_opaqueChildren->intersect(rect());
    m_dirtyOpaqueChildren = false;

    return m_opaqueChildren;
}

void LWidget::sendPaintEvent(const LRect<int> &toBePainted)
{
    std::shared_ptr<LObject> q =objPtr();
    std::shared_ptr<LPaintEvent> e= std::make_shared<LPaintEvent>(toBePainted);
    LApplication::sendSpontaneousEvent(q, e);
}

void LWidget::sendPaintEvent(const std::shared_ptr<LRegion> toBePainted)
{
    std::shared_ptr<LObject> q =objPtr();
    std::shared_ptr<LPaintEvent> e= std::make_shared<LPaintEvent>(toBePainted);
    LApplication::sendSpontaneousEvent(q, e);
}

std::shared_ptr<LPaintDevice> LWidget::paintScreenDevice()
{
    return nullptr;
}

void LWidget::paintSiblingsRecursive(std::shared_ptr<LPaintDevice> pdev,
                                     const std::list<std::shared_ptr<LTreeObj> > &siblings,
                                     int index,
                                     const std::shared_ptr<LRegion> rgn,
                                     const LPoint<int> &offset,
                                     LFlags<LDrawWidgetOpt> flags,
                                     LPainter *sharedPainter,
                                     LWidgetRepaintMan *repaintManager)
{
    std::shared_ptr<LWidget> w = nullptr;
    LRect<int> boundingRect;
    bool dirtyBoundingRect = true;
    const bool exludeOpaqueChildren = (flags & LDrawWidgetOpt::DontDrawOpaqueChildren);
    const bool excludeNativeChildren = (flags & LDrawWidgetOpt::DontDrawNativeChildren);

    do {
        auto it = siblings.begin();
        std::advance(it, index);
        std::shared_ptr<LWidget> x =  std::dynamic_pointer_cast<LWidget>(*it);
        if (x && !(exludeOpaqueChildren && x->m_isOpaque) && !x->isHidden() && !x->isWindow()
            && !(excludeNativeChildren && x->internalWinId())) {
            if (dirtyBoundingRect) {
                if(rgn != nullptr){
                    boundingRect = rgn->boundingRect();
                }
                dirtyBoundingRect = false;
            }

            if (boundingRect.isIntersects(x->m_widgetData.crect)) {
                w = x;
                break;
            }
        }
        --index;
    } while (index >= 0);

    if (w == nullptr){
        return;
    }

    const LPoint<int> widgetPos(w->m_widgetData.crect.topLeft());
    bool hasMask = false;
    if(w->m_pExtraData != nullptr){
        hasMask = (w->m_pExtraData->mask != nullptr); //&& !w->m_graphicsEffect;
    }
    if (index > 0) {
        std::shared_ptr<LRegion> wr = std::make_shared<LRegion>(*rgn);
        if (w->m_isOpaque){
            if(hasMask){
                wr->subtract(w->m_pExtraData->mask->translated(widgetPos));
            }else{
                wr->subtract(w->m_widgetData.crect);
            }
        }
        paintSiblingsRecursive(pdev, siblings, --index, wr, offset, flags,
                               sharedPainter, repaintManager);
    }

    if (w->updatesEnabled()) {
        std::shared_ptr<LRegion> wRegion = std::make_shared<LRegion>(*rgn);
        wRegion->intersect(w->m_widgetData.crect);
        wRegion->translate(-widgetPos);
        if (hasMask){
            wRegion->intersect(w->m_pExtraData->mask);
        }
        w->drawWidget(pdev, wRegion, offset + widgetPos, flags, sharedPainter, repaintManager);
    }
}

void LWidget::drawWidget(std::shared_ptr<LPaintDevice> pdev,
                         std::shared_ptr<LRegion> rgn,
                         const LPoint<int> &offset,
                         LFlags<LDrawWidgetOpt> flags,
                         LPainter *sharedPainter,
                         LWidgetRepaintMan *repaintManager)
{
    if (rgn->isEmpty()){
        return;
    }

    std::shared_ptr<LWidget> q = getPtrT<LWidget>();

    const bool asRoot = flags & LDrawWidgetOpt::DrawAsRoot;
    bool onScreen = shouldPaintOnScreen();

    const bool alsoOnScreen = flags & LDrawWidgetOpt::DrawPaintOnScreen;
    const bool recursive = flags & LDrawWidgetOpt::DrawRecursive;
    const bool alsoInvisible = flags & LDrawWidgetOpt::DrawInvisible;

    std::shared_ptr<LRegion> toBePainted = std::make_shared<LRegion>(*rgn);
    if (asRoot && !alsoInvisible){
        toBePainted->intersect(clipRect()); //(rgn & visibleRegion());
    }
    if (!(flags & LDrawWidgetOpt::DontSubtractOpaqueChildren)){
        subtractOpaqueChildren(toBePainted, rect());
    }

    if (!toBePainted->isEmpty()) {
        if (!onScreen || alsoOnScreen) {
            //update the "in paint event" flag
            if (L_UNLIKELY(testAttribute(LWidgetAttribute::WStateInPaintEvent))){
                lWarn() << "LWidget::repaint: Recursive repaint detected";
            }
            setAttribute(LWidgetAttribute::WStateInPaintEvent);

            //clip away the new area
            std::shared_ptr<LPaintEngine> paintEngine = pdev->paintEngine();
            if (paintEngine != nullptr) {
                setRedirected(pdev, -offset);

                if (sharedPainter){
                    setSystemClip(paintEngine, pdev->devicePixelRatioF(), toBePainted);
                }else{
                    paintEngine->m_systemRect = m_widgetData.crect;
                }

                //paint the background
                if ((asRoot || autoFillBackground() || onScreen || testAttribute(LWidgetAttribute::StyledBackground))
                    && !testAttribute(LWidgetAttribute::OpaquePaintEvent) && !testAttribute(LWidgetAttribute::NoSystemBackground)) {

                        beginBackingStorePainting();
                        LPainter p(pdev);
                        paintBackground(&p, toBePainted, (asRoot || onScreen) ? (flags | LDrawWidgetOpt::DrawAsRoot) : LFlags<LDrawWidgetOpt>());
                        endBackingStorePainting();
                }

                if (!sharedPainter){
                    setSystemClip(pdev->paintEngine(), pdev->devicePixelRatioF(), toBePainted->translated(offset));
                }

                if (!onScreen && !asRoot && !m_isOpaque && testAttribute(LWidgetAttribute::TintedBackground)) {
                    beginBackingStorePainting();
                    LPainter p(pdev);
                    LColor tint = palette()->color(colorGroup(), LPalette::Window);
                    tint.m_alpha = 153;
                    p.fillRect(toBePainted->boundingRect().toRectF(), tint);
                    endBackingStorePainting();
                }

                sendPaintEvent(toBePainted);
                if (repaintManager){
                    repaintManager->markNeedsFlush(q, toBePainted, offset);
                }

                restoreRedirected();
                if (sharedPainter == nullptr){
                    paintEngine->m_systemRect = LRect<int>();
                }
                setSystemClip(pdev->paintEngine(), 1, nullptr);
            }

            setAttribute(LWidgetAttribute::WStateInPaintEvent, false);
        } else if (isWindow()) {
            std::shared_ptr<LPaintEngine> engine = pdev->paintEngine();
            if (engine != nullptr) {
                LPainter p(pdev);
                p.setClipRegion(toBePainted);
                const std::shared_ptr<LBrush> bg = palette()->brush(colorGroup(), LPalette::Window);
                if (bg->m_style == LBrush::TexturePattern){
                    p.drawTiledPixmap(rect().toRectF(), bg->texture());
                }
                else{
                    p.fillRect(rect().toRectF(), bg);
                }
            }
        }
    }


    if (recursive && !m_children.empty()) {
        flags &= (~uint8_t(LDrawWidgetOpt::DrawAsRoot));
        paintSiblingsRecursive(pdev,
                               m_children,
                               (int)m_children.size() - 1,
                               rgn,
                               offset,
                               flags,
                               sharedPainter,
                               repaintManager);
    }

    if(m_paintLayout && m_layout != nullptr){
        setRedirected(pdev, -offset);
        LPainter p(pdev);
        m_layout->paint(&p);
        restoreRedirected();
    }
}

void LWidget::paintBackground(LPainter *painter, std::shared_ptr<LRegion> rgn, LFlags<LDrawWidgetOpt> flags) const
{
    bool brushOriginSet = false;
    std::shared_ptr<LPalette> pal = palette();
    const std::shared_ptr<LBrush> autoFillBrush = pal->brush(backgroundRole());

    if ((flags & LDrawWidgetOpt::DrawAsRoot) && !(autoFillBackground() && autoFillBrush->isOpaque())) {
        const std::shared_ptr<LBrush> bg = pal->brush(LPalette::Window);

        brushOriginSet = updateBrushOrigin(painter, bg);

        if (!(flags & LDrawWidgetOpt::DontSetCompositionMode)) {
            //copy alpha straight in
            LCompositionMode oldMode = painter->compositionMode();
            painter->setCompositionMode(LCompositionMode::Source);
            fillRegion(painter, rgn, bg);
            painter->setCompositionMode(oldMode);
        } else {
            fillRegion(painter, rgn, bg);
        }
    }

    if (autoFillBackground()) {
        if (!brushOriginSet){
            updateBrushOrigin(painter, autoFillBrush);
        }
        fillRegion(painter, rgn, autoFillBrush);
    }


   if (testAttribute(LWidgetAttribute::StyledBackground)) {
       painter->setClipRegion(rgn);
       uint16_t state = uiElementState(LUiElementType::None);
       std::shared_ptr<LRenderRule> rule = m_elementStyle.getRenderRule(LUiElementType::None, state);
       if(rule != nullptr){
           if (rule->hasBackground()) {
               rule->drawBackground(painter, rect());
           }
       }
   }
}

void LWidget::subtractOpaqueChildren(std::shared_ptr<LRegion> rgn, const LRect<int> &clipRect) const
{
    if (m_children.empty() || clipRect.isEmpty()){
        return;
    }

    const std::shared_ptr<LRegion> r =
        std::make_shared<LRegion>(const_cast<LWidget*>(this)->getOpaqueChildren());
    if (r != nullptr){
        r->intersect(clipRect);
        if(!r->isEmpty()){
            rgn->subtract(r);
        }
    }
}

void LWidget::subtractOpaqueSiblings(std::shared_ptr<LRegion> sourceRegion,
                                     bool *hasDirtySiblingsAbove,
                                     bool alsoNonOpaque) const
{
    if (isWindow()){
        return;
    }

    LRect<int> clipBoundingRect;
    bool dirtyClipBoundingRect = true;

    std::shared_ptr<LRegion> parentClip = std::make_shared<LRegion>();
    bool dirtyParentClip = true;

    LPoint<int> parentOffset = m_widgetData.crect.topLeft();

    std::shared_ptr<LWidget> w = getPtrT<LWidget>();

    while (w != nullptr) {
        if (w->isWindow()){
            break;
        }
        std::shared_ptr<LWidget> pd = w->parentT<LWidget>();
        auto isWidget = [w](std::shared_ptr<LTreeObj> ww) {
            std::shared_ptr<LWidget> t = std::dynamic_pointer_cast<LWidget>(ww);
            return t == w;
        };

        const LRect<int> widgetGeometry = w->m_widgetData.crect;
        auto iter = std::find_if(pd->m_children.begin(), pd->m_children.end(), isWidget);
        iter ++;
        for (; iter != pd->m_children.end(); ++iter) {
            std::shared_ptr<LWidget> sibling =
                std::dynamic_pointer_cast<LWidget>(*iter);
            if (!sibling || !sibling->isVisible() || sibling->isWindow()){
                continue;
            }

            const LRect<int> siblingGeometry = sibling->m_widgetData.crect;
            if (!siblingGeometry.isIntersects(widgetGeometry)){
                continue;
            }

            if (dirtyClipBoundingRect) {
                clipBoundingRect = sourceRegion->boundingRect();
                dirtyClipBoundingRect = false;
            }

            if (!siblingGeometry.isIntersects(clipBoundingRect.translated(parentOffset)))
                continue;

            if (dirtyParentClip) {
                parentClip = sourceRegion->translated(parentOffset);
                dirtyParentClip = false;
            }

            const LPoint<int> siblingPos(sibling->m_widgetData.crect.topLeft());
            const LRect<int> siblingClipRect(sibling->clipRect());
            std::shared_ptr<LRegion> siblingDirty=
                std::make_shared<LRegion>(*parentClip);
            siblingDirty->intersect(siblingClipRect.translated(siblingPos));

            bool hasMask = false;
            if(sibling->m_pExtraData != nullptr){
                hasMask = sibling->m_pExtraData->mask != nullptr;
            }
            if (hasMask){
                siblingDirty->intersect(sibling->m_pExtraData->mask->translated(siblingPos));
            }
            if (siblingDirty->isEmpty()){
                continue;
            }

            if (sibling->m_isOpaque || alsoNonOpaque) {
                if (hasMask) {
                    siblingDirty->translate(-parentOffset);
                    sourceRegion->subtract(siblingDirty);
                } else {
                    sourceRegion->subtract(siblingGeometry.translated(-parentOffset));
                }
            } else {
                if (hasDirtySiblingsAbove){
                    *hasDirtySiblingsAbove = true;
                }
                if (sibling->m_children.empty()){
                    continue;
                }
                std::shared_ptr<LRegion> opaqueSiblingChildren =
                    std::make_shared<LRegion>(sibling->getOpaqueChildren());
                opaqueSiblingChildren->translate(-parentOffset + siblingPos);
                sourceRegion->subtract(opaqueSiblingChildren);
            }
            if (sourceRegion->isEmpty()){
                return;
            }
            dirtyClipBoundingRect = true;
            dirtyParentClip = true;
        }

        w = w->parentT<LWidget>();
        parentOffset += pd->m_widgetData.crect.topLeft();
        dirtyParentClip = true;
    }
}

void LWidget::deactivateWidgetCleanup()
{
    if (LApplication::activeWindow().get() == this){
        LApplication::setActiveWindow(nullptr);
    }
    // If the is the active mouse press widget, reset it
    if (this == LWidgetWindow::m_buttonDown.lock().get()){
        LWidgetWindow::m_buttonDown.reset();
    }
}

void LWidget::updateContentsRect()
{
    if (m_layout){
        m_layout->update(); //force activate; will do updateGeometry
    }else{
        updateGeometry();
    }

    if (isVisible()) {
        update();
    }

    std::shared_ptr<LEvent> e= std::make_shared<LEvent>(LEvent::ContentsRectChange);
    LCoreApplication::sendEvent(getPtrT<LWidget>(), e);
}

bool LWidget::pointInsideRectAndMask(const LPoint<int> &p) const
{
    if(rect().contains(p)){
        if(m_pExtraData == nullptr){
            return true;
        }

        if(m_pExtraData->mask == nullptr){
            return true;
        }

        if(testAttribute(LWidgetAttribute::MouseNoMask)){
            return true;
        }

        return m_pExtraData->mask->contains(p);
    }
    return false;
}

std::shared_ptr<LWidget> LWidget::childAtRecursiveHelper(const LPoint<int> &p) const
{
    for (const std::shared_ptr<LTreeObj>& tChild : m_children) {
        std::shared_ptr<LWidget> child = std::dynamic_pointer_cast<LWidget>(tChild);
        if (!child || child->isWindow() || child->isHidden() || child->testAttribute(LWidgetAttribute::TransparentForMouseEvents)) {
            continue;
        }

        // Map the point 'p' from parent coordinates to child coordinates.
        LPoint<int> childPoint = p;
        childPoint -= child->m_widgetData.crect.topLeft();

        // Check if the point hits the child.
        if (!child->pointInsideRectAndMask(childPoint)){
            continue;
        }

        // Do the same for the child's descendants.
        if (std::shared_ptr<LWidget> w = child->childAtRecursiveHelper(childPoint)){
            return w;
        }
        // We have found our target; namely the child at position 'p'.
        return child;
    }
    return nullptr;
}

void LWidget::adjustFlags(LFlags<LWindowType>& flags, std::shared_ptr<LWidget> w)
{
    bool customize =  (flags & (LWindowType::CustomizeWindowHint
                               | LWindowType::FramelessWindowHint
                               | LWindowType::WindowTitleHint
                               | LWindowType::WindowSystemMenuHint
                               | LWindowType::WindowMinimizeButtonHint
                               | LWindowType::WindowMaximizeButtonHint
                               | LWindowType::WindowCloseButtonHint
                               | LWindowType::WindowContextHelpButtonHint));

    uint32_t type = (flags & LWindowType::WindowTypeMask);

    if(w != nullptr){
        if(w->parent() == nullptr){
            if (type == uint32_t(LWindowType::Widget) || type == uint32_t(LWindowType::SubWindow)) {
                type = uint32_t(LWindowType::Window);
                flags |= LWindowType::Window;
            }
        }
    }


    if (flags & LWindowType::CustomizeWindowHint) {
        // modify window flags to make them consistent.
        // Only enable this on non-Mac platforms. Since the old way of doing this would
        // interpret WindowSystemMenuHint as a close button and we can't change that behavior
        // we can't just add this in.
        if (flags & (LWindowType::WindowMinMaxButtonsHint
                      | LWindowType::WindowCloseButtonHint
                      | LWindowType::WindowContextHelpButtonHint)) {
            flags |= LWindowType::WindowSystemMenuHint;
            flags |= LWindowType::WindowTitleHint;
            flags &= ~uint32_t(LWindowType::FramelessWindowHint);
        }
    } else if (customize && !(flags & LWindowType::FramelessWindowHint)) {
        // if any of the window hints that affect the titlebar are set
        // and the window is supposed to have frame, we add a titlebar
        // and system menu by default.
        flags |= LWindowType::WindowSystemMenuHint;
        flags |= LWindowType::WindowTitleHint;
    }
    if (customize){
        ; // don't modify window flags if the user explicitly set them.
    }
    else if (type == uint32_t(LWindowType::Dialog) || type == uint32_t(LWindowType::Sheet)) {
        flags |= LWindowType::WindowTitleHint
                 | LWindowType::WindowSystemMenuHint
                 | LWindowType::WindowCloseButtonHint;
    } else if (type == uint32_t(LWindowType::Tool)){
        flags |= LWindowType::WindowTitleHint
                 | LWindowType::WindowSystemMenuHint
                 | LWindowType::WindowCloseButtonHint;
    }
    else{
        flags |= LWindowType::WindowTitleHint
                 | LWindowType::WindowSystemMenuHint
                 | LWindowType::WindowMinimizeButtonHint
                 | LWindowType::WindowMaximizeButtonHint
                 | LWindowType::WindowCloseButtonHint
                 | LWindowType::WindowFullscreenButtonHint;
    }

    if (w->testAttribute(LWidgetAttribute::TransparentForMouseEvents)){
        flags |= LWindowType::WindowTransparentForInput;
    }
}

bool LWidget::focusNextPrevChild(bool next)
{
    std::shared_ptr<LWidget> p = parentT<LWidget>();
    bool isSubWindow = (windowType() == LWindowType::SubWindow);
    if (!isWindow() && !isSubWindow && (p != nullptr)){
        return p->focusNextPrevChild(next);
    }

    bool wrappingOccurred = false;
    std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();
    std::shared_ptr<LWidget> w = LApplication::focusNextPrevChildHelper(tThis, next, &wrappingOccurred);
    if (w == nullptr){
        return false;
    }

    LFocusReason reason = next ? LFocusReason::Tab : LFocusReason::Backtab;

    /* If we are about to wrap the focus chain, give the platform
     * implementation a chance to alter the wrapping behavior.  This is
     * especially needed when the window is embedded in a window created by
     * another process.
     */
    if (wrappingOccurred) {
        std::shared_ptr<LWindow> window = windowHandle();
        if (window != nullptr) {
            std::shared_ptr<LPlatformWindow> pw = window->platformWindow();

            if (pw != nullptr) {
                std::shared_ptr<LFocusEvent> event = std::make_shared<LFocusEvent>(LEvent::FocusIn, reason);
                event->ignore();
                pw->windowEvent(event);
                if (event->isAccepted()){
                    return true;
                }
            }
        }
    }

    w->setFocus(reason);
    return true;
}

void LWidget::mousePressEvent(std::shared_ptr<LMouseEvent> event)
{

}

void LWidget::mouseReleaseEvent(std::shared_ptr<LMouseEvent> event)
{

}

void LWidget::mouseDoubleClickEvent(std::shared_ptr<LMouseEvent> event)
{

}

void LWidget::mouseMoveEvent(std::shared_ptr<LMouseEvent> event)
{

}

void LWidget::wheelEvent(std::shared_ptr<LWheelEvent> event)
{

}

void LWidget::keyPressEvent(std::shared_ptr<LKeyEvent> event)
{

}

void LWidget::keyReleaseEvent(std::shared_ptr<LKeyEvent> event)
{

}

void LWidget::focusInEvent(std::shared_ptr<LFocusEvent> event)
{

}

void LWidget::focusOutEvent(std::shared_ptr<LFocusEvent> event)
{

}

void LWidget::enterEvent(std::shared_ptr<LEvent> event)
{
    if(testAttribute(LWidgetAttribute::Hover)){
        update();
    }
}

void LWidget::leaveEvent(std::shared_ptr<LEvent> event)
{
    if(testAttribute(LWidgetAttribute::Hover)){
        update();
    }
}

void LWidget::paintEvent(std::shared_ptr<LPaintEvent> event)
{

}

void LWidget::moveEvent(std::shared_ptr<LMoveEvent> event)
{

}

void LWidget::resizeEvent(std::shared_ptr<LResizeEvent> event)
{

}

void LWidget::closeEvent(std::shared_ptr<LEvent> event)
{
    event->accept();
}

void LWidget::contextMenuEvent(std::shared_ptr<LContextMenuEvent> event)
{
    event->ignore();
}

void LWidget::showEvent(std::shared_ptr<LEvent> event)
{

}

void LWidget::hideEvent(std::shared_ptr<LEvent> event)
{

}

void LWidget::changeEvent(std::shared_ptr<LEvent> event)
{

}

void LWidget::inputMethodEvent(std::shared_ptr<LInputMethodEvent> event)
{

}

void LWidget::updateElementStyle(const LStyleOpt &opt)
{

}

bool LWidget::enterSubItem(const LPoint<int> &)
{
    return true;
}

void LWidget::initStyleOpt(LStyleOpt &opt) const
{
    opt.state = uiElementState(LUiElementType::None);
    opt.direction = layoutDirection();
    opt.rect = contentsRect();
    opt.palette = palette();
    opt.foregroundRole = foregroundRole();
    opt.backgroundRole = backgroundRole();
    opt.elementStyle = &m_elementStyle;
}

uint16_t LWidget::uiElementState(LUiElementType type) const
{
    if(isEnabled()){
        if(underMouse() && testAttribute(LWidgetAttribute::Hover)){
            return uint16_t(LUiState::Hover);
        }else{
            return uint16_t(LUiState::Enabled);
        }
    }else{
        return uint16_t(LUiState::Disabled);
    }
}

void LWidget::setLayoutDirection(LLayoutDirection direction)
{
    if (direction == LLayoutDirection::Auto) {
        unsetLayoutDirection();
        return;
    }

    setAttribute(LWidgetAttribute::SetLayoutDirection);
    setLayoutDirectionHelper(direction);
}

LLayoutDirection LWidget::layoutDirection() const
{
    return testAttribute(LWidgetAttribute::RightToLeft) ? LLayoutDirection::RightToLeft : LLayoutDirection::LeftToRight;
}

void LWidget::unsetLayoutDirection()
{
    setAttribute(LWidgetAttribute::SetLayoutDirection, false);
    resolveLayoutDirection();
}

std::shared_ptr<LStyle> LWidget::style() const
{
    return LApplication::style();
}


const std::shared_ptr<LWidgetStyleSheet> LWidget::styleSheet()
{
    return m_elementStyle.m_styleSheet;
}

std::list<std::shared_ptr<LWidgetStyleSheet> > LWidget::effectStyleSheets()
{
    if((m_elementStyle.m_styleSheet != nullptr)
        && (m_elementStyle.m_shieldParentStyleSheet == true)){
            std::list<std::shared_ptr<LWidgetStyleSheet> > ret;
            ret.push_back(m_elementStyle.m_styleSheet);
            return ret;
    }else{
        std::shared_ptr<LWidget> p = parentT<LWidget>();
        if(p != nullptr){
            std::list<std::shared_ptr<LWidgetStyleSheet> > ret = p->effectStyleSheets();
            if(m_elementStyle.m_styleSheet != nullptr){
                ret.push_back(m_elementStyle.m_styleSheet);
            }
            return ret;
        }else if(m_elementStyle.m_styleSheet != nullptr){
            std::list<std::shared_ptr<LWidgetStyleSheet> > ret;
            ret.push_back(m_elementStyle.m_styleSheet);
            return ret;
        }
    }
    return std::list<std::shared_ptr<LWidgetStyleSheet> >();
}

bool LWidget::clearCachedStyle()
{
    bool hasClear = m_elementStyle.clearCachedStyle();
    for(const std::shared_ptr<LTreeObj>& child : m_children){
        std::shared_ptr<LWidget> w = std::dynamic_pointer_cast<LWidget>(child);
        if(w != nullptr){
            // 样式表变化后整棵子树的渲染规则都应失效，这里无条件递归；
            // 注意必须先调用再合并结果，否则短路求值会跳过后续子控件
            const bool childClear = w->clearCachedStyle();
            hasClear = hasClear || childClear;
        }
    }
    return hasClear;
}

void LWidget::handleSoftwareInputPanel(bool clickCausedFocus)
{
    if (LApplication::m_autoSipEnabled) {
        std::shared_ptr<LStyle> pStyle = style();
        if (!clickCausedFocus || pStyle->getBit(LStyleBit::InputPanelNeedFocus) == false) {
            LGuiApplication::inputMethod()->show();
        }
    }
}

int LWidget::boundWidth(LDirection dir, int flags) const
{
    std::shared_ptr<LStyle> pStyle = style();
    double width = pStyle->boundWidth(m_sizePolicy.controlType(), dir, flags, &m_elementStyle);
    if(flags & LRenderRule::CodeMargin){
        switch (dir) {
        case LDirection::ToTop:{
            width += m_topMargin;
            break;
        }
        case LDirection::ToRight:{
            width += m_rightMargin;
            break;
        }
        case LDirection::ToBottom:{
            width += m_bottomMargin;
            break;
        }
        case LDirection::ToLeft:{
            width += m_leftMargin;
            break;
        }
        default:
            break;
        }
    }
    return std::lround(width);
}

LMargins<int> LWidget::boundWidths(int flags) const
{
    std::shared_ptr<LStyle> pStyle = style();
    LSizePolicy::ControlType ctrlType = m_sizePolicy.controlType();
    LMargins<double> ret = pStyle->boundWidths(ctrlType, flags, &m_elementStyle);
    if(flags & LRenderRule::CodeMargin){
        ret.m_top += m_topMargin;
        ret.m_right += m_rightMargin;
        ret.m_bottom += m_bottomMargin;
        ret.m_left += m_leftMargin;
    }
    return ret.toMargin();
}


void LWidget::setStyleSheet(const std::string &styleSheet, bool isRaw, bool shieldParent)
{
    // 1. 先清除旧样式：内容为空时直接把样式表置空，等价于“恢复默认外观”，
    //    这样调用方只需 setStyleSheet(新样式) / setStyleSheet("") 即可切换主题，
    //    不必自己先 clear 再设置。
    if (styleSheet.empty()) {
        m_elementStyle.m_styleSheet = nullptr;
        m_elementStyle.m_shieldParentStyleSheet = false;
        clearCachedStyle();
        update();
        return;
    }

    if(m_elementStyle.m_styleSheet != nullptr){
        m_elementStyle.m_styleSheet->clear();
    }else{
        m_elementStyle.m_styleSheet = std::make_shared<LWidgetStyleSheet>();
    }

    if(isRaw){
        LCssParser::parseStyleSheet(styleSheet, m_elementStyle.m_styleSheet);
    }else{
        LCssParser::parseFile(styleSheet, m_elementStyle.m_styleSheet);
    }
    m_elementStyle.m_shieldParentStyleSheet = shieldParent;

    // 2. 再让整棵控件树的渲染规则缓存失效，重新按新样式计算
    clearCachedStyle();
    update();
}

void LWidget::setStyleSheet(const std::shared_ptr<LWidgetStyleSheet> styleSheet)
{
    if(m_elementStyle.m_styleSheet != styleSheet){
        m_elementStyle.m_styleSheet = styleSheet;
        m_elementStyle.m_shieldParentStyleSheet = (styleSheet != nullptr);//共享stylesheet，屏蔽传递，以免重复
        if(clearCachedStyle()){
            update();
        }
    }
}


std::shared_ptr<LFont> LWidget::font() const
{
    if(m_widgetData.font != nullptr){
        return m_widgetData.font;
    }else{
        return LGuiApplication::font();
    }
}

void LWidget::setFont(std::shared_ptr<LFont> font)
{
    m_widgetData.font = font;
}

void LWidget::grabMouse(const LCursor& c)
{
    std::shared_ptr<LWidget> mouseGrab = m_mouseGrab.lock();
    if (mouseGrab != nullptr){
        mouseGrab->releaseMouse();
    }

    std::shared_ptr<LWidget> w = getPtrT<LWidget>();
    std::shared_ptr<LWindow> window = grabberWindow(w);
    if (window != nullptr) {
        if(c.isValid()){
            m_mouseGrabWithCursor = true;
            m_mouseGrabBackCursor = LGuiApplication::overrideCursor();
            LGuiApplication::setOverrideCursor(c);
        }
        window->setMouseGrabEnabled(true);
        m_mouseGrab = w;
    }
}

void LWidget::releaseMouse()
{
    if(m_mouseGrabWithCursor){
        m_mouseGrabWithCursor = false;
        LGuiApplication::restoreOverrideCursor(m_mouseGrabBackCursor);
    }

    std::shared_ptr<LWidget> mouseGrab = m_mouseGrab.lock();
    if (mouseGrab.get() == this) {
        std::shared_ptr<LWindow> window = grabberWindow(mouseGrab);
        if (window != nullptr) {
            window->setMouseGrabEnabled(false);
        }
        m_mouseGrab.reset();
    }
}

std::shared_ptr<LWidget> LWidget::mouseGrabber()
{
    return m_mouseGrab.lock();
}

std::shared_ptr<LWidget> LWidget::keyboardGrabber()
{
    return m_keyboardGrab.lock();
}

LCursor LWidget::cursor() const
{
    if (testAttribute(LWidgetAttribute::SetCursor)){
        return (m_pExtraData != nullptr) ? m_pExtraData->cursor : LCursor();
    }

    std::shared_ptr<LWidget> parent = parentT<LWidget>();
    if (isWindow() || parent == nullptr){
        return LCursor();
    }
    return parent->cursor();
}

void LWidget::setCursor(const LCursor& cursor)
{
    if (cursor.isValid())
    {
        createExtra();
        m_pExtraData->cursor = cursor;
    }
    setAttribute(LWidgetAttribute::SetCursor);
    setCursorSys(false);
}

void LWidget::unsetCursor()
{
    if (m_pExtraData != nullptr){
        m_pExtraData->cursor.reset();
    }
    if (!isWindow()){
        setAttribute(LWidgetAttribute::SetCursor, false);
    }
    setCursorSys(false);
}

bool LWidget::acceptDrops() const
{
    return testAttribute(LWidgetAttribute::AcceptDrops);
}

void LWidget::setAcceptDrops(bool on)
{
    setAttribute(LWidgetAttribute::AcceptDrops, on);
}

double LWidget::dpi() const
{
    if (LCoreApplication::testAttribute(LAppAttribute::Use96Dpi)){
        return 96;
    }
    return font()->dpi();
}

double LWidget::dpiScaled(double value) const
{
    return lDpiScaled(value, dpi());
}

void LWidget::enforceNativeChildren()
{
    if (m_pExtraData == nullptr){
        createExtra();
    }

    if (m_pExtraData->nativeChildrenForced){
        return;
    }
    m_pExtraData->nativeChildrenForced = 1;

    for(std::shared_ptr<LTreeObj>& tObj : m_children){
        std::shared_ptr<LWidget> child = std::dynamic_pointer_cast<LWidget>(tObj);
        if(child != nullptr){
            child->setAttribute(LWidgetAttribute::NativeWindow);
        }
    }
}

void LWidget::create(WId wId){
    if (testAttribute(LWidgetAttribute::WStateCreated) && wId == 0 && internalWinId()){
        return;
    }

    if (m_widgetData.inDestructor){
        return;
    }

    LWindowType type = windowType();
    LFlags<LWindowType> &flags = m_widgetData.windowFlags;

    std::shared_ptr<LWidget> pw = parentT<LWidget>();
    if(pw == nullptr){
        if (type == LWindowType::Widget || type == LWindowType::SubWindow) {
            type = LWindowType::Window;
            flags |= LWindowType::Window;
        }
    }

    if (pw != nullptr) {
        if (uint32_t(type) & uint32_t(LWindowType::Window)) {
            if (!pw->testAttribute(LWidgetAttribute::WStateCreated)){
                pw->createWinId();
            }
        } else if (testAttribute(LWidgetAttribute::NativeWindow) && !pw->internalWinId()
                   && !testAttribute(LWidgetAttribute::DontCreateNativeAncestors)) {
            // We're about to create a native child widget that doesn't have a native parent;
            // enforce a native handle for the parent unless the Qt::WA_DontCreateNativeAncestors
            // attribute is set.
            createWinId();
            return;
        }
    }

    if (LApplication::testAttribute(LAppAttribute::NativeWindows)){
        setAttribute(LWidgetAttribute::NativeWindow);
    }

    updateIsOpaque();

    setAttribute(LWidgetAttribute::WStateCreated);                        // set created flag
    createInner();


    // A real toplevel window needs a paint manager
    if (isWindow() && windowType() != LWindowType::Desktop){
        LTLWExtra *pTopData = topData();
        std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();
        pTopData->repaintMan.reset(new LWidgetRepaintMan(tThis));
    }

    setModalSys();

    if(pw != nullptr){
        if (!isWindow() && pw->testAttribute(LWidgetAttribute::DropSiteRegistered)){
            setAttribute(LWidgetAttribute::DropSiteRegistered, true);
        }
    }

    // need to force the resting of the icon after changing parents
    if (testAttribute(LWidgetAttribute::SetWindowIcon)){
        setWindowIconSys();
    }

    if(isWindow()){
        LTLWExtra *pTopData = topData();
        if (!pTopData->caption.empty()){
            setWindowTitleHelper(pTopData->caption);
        }
    }

    if (windowType() != LWindowType::Desktop) {
        //updateSystemBackground();
        if (isWindow() && !testAttribute(LWidgetAttribute::SetWindowIcon)){
            setWindowIconSys();
        }
    }

    // Frame strut update needed in cases where there are native widgets,
    // as those force native window creation on their ancestors before they are shown.
    // If the strut is not updated, any subsequent move of the top level window before show
    // will cause window frame to be ignored when positioning the window.
    // Note that this only helps on platforms that handle window creation synchronously.
    updateFrameStrut();

}

void LWidget::createRecursively() {
    create(0);
    for(std::shared_ptr<LTreeObj>& obj : m_children){
        std::shared_ptr<LWidget> child = std::dynamic_pointer_cast<LWidget>(obj);
        if(child != nullptr){
            if(!child->isHidden() && !child->isWindow() && !child->testAttribute(LWidgetAttribute::WStateCreated)){
                child->createRecursively();
            }
        }
    }
}

void createNativeChildrenAndSetParent(const std::shared_ptr<LWidget> parentWidget)
{
    std::list<std::shared_ptr<LTreeObj> > children = parentWidget->children();
    for (const std::shared_ptr<LTreeObj>& obj : children) {
        if (obj->isWidgetType()) {
            std::shared_ptr<LWidget> childWidget = std::dynamic_pointer_cast<LWidget>(obj);
            if (childWidget != nullptr) {
                if (childWidget->testAttribute(LWidgetAttribute::NativeWindow)) {
                    if (!childWidget->internalWinId()){
                        childWidget->winId();
                    }
                    if (childWidget->windowHandle()) {
                        if (childWidget->isWindow()) {
                            childWidget->windowHandle()->setTransientParent(parentWidget->window()->windowHandle());
                        } else {
                            childWidget->windowHandle()->setParent(childWidget->nativeParentWidget()->windowHandle());
                        }
                    }
                } else {
                    createNativeChildrenAndSetParent(childWidget);
                }
            }
        }
    }

}

void LWidget::createInner()
{
    if (!testAttribute(LWidgetAttribute::NativeWindow) && !isWindow()){
        return; // we only care about real toplevels
    }
    std::shared_ptr<LWidget> q = getPtrT<LWidget>();
    LTLWExtra *pTlwExtra = topData();
    std::shared_ptr<LWidgetWindow> win = pTlwExtra->window;
    // topData() ensures the extra is created but does not ensure 'window' is non-null
    // in case the extra was already valid.
    if (win == nullptr) {
        createTLSysExtra();
        win = pTlwExtra->window;
    }

    LFlags<LWindowType> &flags = m_widgetData.windowFlags;
    win->setFlags(flags);
    fixPosIncludesFrame();
    if (testAttribute(LWidgetAttribute::Moved)
        || !LGuiApplication::platformIntegration()->hasCapability(LPlatformCapability::WindowManagement)){
        win->setGeometry(geometry());
    }
    else{
       win->resize(size());
    }


    if (win->isTopLevel()) {
        int screenNumber = pTlwExtra->initialScreenIndex;
        pTlwExtra->initialScreenIndex = -1;
        if (screenNumber < 0) {
            screenNumber = windowType() != LWindowType::Desktop
                               ? LDesktopWidget::screenNumber(q) : 0;
        }
        win->setScreen(LGuiApplication::screenAt(screenNumber, 0));
    }

    LSurfaceFormat format = win->requestedFormat();
    if ((flags & LWindowType::Window) && win->surfaceType() != LSurface::OpenGLSurface
        && testAttribute(LWidgetAttribute::TranslucentBackground)) {
        format.m_alphaBufferSize = 8;
    }
    win->setFormat(format);

    if (std::shared_ptr<LWidget> nativeParent = q->nativeParentWidget()) {
        std::shared_ptr<LWindow> pWin = nativeParent->windowHandle();
        if (pWin != nullptr) {
            if (flags & LWindowType::Window) {
                win->setTransientParent(nativeParent->window()->windowHandle());
                win->setParent(nullptr);
            } else {
                win->setTransientParent(nullptr);
                win->setParent(pWin);
            }
        }
    }

    if(pTlwExtra->posIncludesFrame){
        win->m_positionPolicy = LWindow::WindowFrameInclusive;
    }else{
        win->m_positionPolicy = LWindow::WindowFrameExclusive;
    }


   if (q->windowType() != LWindowType::Desktop || q->testAttribute(LWidgetAttribute::NativeWindow)) {
        win->create();
        std::shared_ptr<LPlatformWindow> platformWindow = win->handle();
        if (platformWindow != nullptr){
            platformWindow->setFrameStrutEventsEnabled(true);
        }
    }

    m_widgetData.windowFlags = win->flags();
    if (!win->isTopLevel()){ // In a Widget world foreign windows can only be top level
        m_widgetData.windowFlags &= ~uint32_t(LWindowType::ForeignWindow);
    }


    std::shared_ptr<LBackingStore> store = backingStore();
    if (store == nullptr) {
        if (windowType() != LWindowType::Desktop) {
            if (isWindow()){
                store = std::make_shared<LBackingStore>(win);
                setBackingStore(store);
            }
        } else {
            setAttribute(LWidgetAttribute::PaintOnScreen, true);
        }
    }
    setWindowModifiedHelper();

    if (win->handle()) {
        WId id = win->winId();
        setWinId(id);
    }

    // Check children and create windows for them if necessary
    createNativeChildrenAndSetParent(q);

    if(m_pExtraData != nullptr){
        if (m_pExtraData->mask != nullptr){
            setMaskSys(m_pExtraData->mask);
        }
    }

    if (m_widgetData.crect.width() == 0 || m_widgetData.crect.height() == 0) {
        setAttribute(LWidgetAttribute::OutsideWSRange, true);
    } else if (q->isVisible()) {
        // If widget is already shown, set window visible, too
        win->setVisible(true);
    }
}

void LWidget::updateIsOpaque()
{
    // hw: todo: only needed if opacity actually changed
    setDirtyOpaqueRegion();

    if (testAttribute(LWidgetAttribute::OpaquePaintEvent) || testAttribute(LWidgetAttribute::PaintOnScreen)) {
        setOpaque(true);
        return;
    }

    std::shared_ptr<LPalette> pal = palette();
    if (autoFillBackground()) {
        const std::shared_ptr<LBrush> autoFillBrush = pal->brush(backgroundRole());
        if (autoFillBrush->m_style != LBrush::NoPattern && autoFillBrush->isOpaque()) {
            setOpaque(true);
            return;
        }
    }

    if (isWindow() && !testAttribute(LWidgetAttribute::NoSystemBackground)) {
        const std::shared_ptr<LBrush> windowBrush = pal->brush(LPalette::Window);
        if (windowBrush->m_style != LBrush::NoPattern && windowBrush->isOpaque()) {
            setOpaque(true);
            return;
        }
    }
    setOpaque(false);
}

void LWidget::setOpaque(bool opaque)
{
    if (m_isOpaque != opaque) {
        m_isOpaque = opaque;
        updateIsTranslucent();
    }
}

void LWidget::updateIsTranslucent()
{
    std::shared_ptr<LWindow> window = windowHandle();
    if (window != nullptr) {
        LSurfaceFormat format = window->format();
        const int oldAlpha = format.m_alphaBufferSize;
        const int newAlpha = testAttribute(LWidgetAttribute::TranslucentBackground)? 8 : 0;
        if (oldAlpha != newAlpha) {
            format.m_alphaBufferSize = newAlpha;
            window->setFormat(format);
        }
    }
}

std::shared_ptr<LWidget> LWidget::deepestFocusProxy() const
{
    std::shared_ptr<LWidget> pFocusProxy = focusProxy();
    if (pFocusProxy == nullptr){
        return nullptr;
    }

    while (std::shared_ptr<LWidget> nextFocusProxy = pFocusProxy->focusProxy()){
        pFocusProxy = nextFocusProxy;
    }

    return pFocusProxy;
}

void LWidget::updateFocusChild()
{
    std::shared_ptr<LWidget> q= getPtrT<LWidget>();
    std::shared_ptr<LWidget> w = q;
    if (q->isHidden()) {
        while ((w!=nullptr) && w->isHidden()) {
            w->m_focusChild = q;
            w = w->isWindow() ? nullptr : w->parentT<LWidget>();
        }
    } else {
        while (w != nullptr) {
            w->m_focusChild = q;
            w = w->isWindow() ? nullptr : w->parentT<LWidget>();
        }
    }

    if (LTLWExtra *extra = q->window()->maybeTopData()) {
        if (extra->window != nullptr){
            lGuiApp->updateFocusObject(q);
        }
    }
}

bool LWidget::updateBrushOrigin(LPainter *painter, const std::shared_ptr<LBrush> brush) const
{
    if (brush->m_style == LBrush::NoPattern || brush->m_style == LBrush::SolidPattern){
        return false;
    }
    std::shared_ptr<LAbstractScrollArea> scrollArea = parentT<LAbstractScrollArea>();
    if (scrollArea != nullptr){
        if(scrollArea->viewport().get() == this) {
            painter->setBrushOrigin(-scrollArea->contentsOffset().toPointF());
        }
    }
    return true;
}

void LWidget::setWindowIconSys()
{
    std::shared_ptr<LWindow>  w = windowHandle();
    if (w != nullptr){
        w->setIcon(windowIcon());
    }
}

void LWidget::setWindowIconHelper()
{
    std::shared_ptr<LEvent> e = std::make_shared<LEvent>(LEvent::WindowIconChange);

    // Do not send the event if the widget is a top level.
    // In that case, setWindowIcon_sys does it, and event propagation from
    // LWidgetWindow to the top level LWidget ensures that the event reaches
    // the top level anyhow
    std::shared_ptr<LWindow>  wh = windowHandle();
    if (wh != nullptr){
        std::shared_ptr<LWidget> w = getPtrT<LWidget>();
        LCoreApplication::sendEvent(w, e);
    }

    for(std::shared_ptr<LTreeObj>& ch: m_children){
        std::shared_ptr<LWidget> w = std::dynamic_pointer_cast<LWidget>(ch);
        if(w != nullptr){
            if(!w->isWindow()){
                LCoreApplication::sendEvent(w, e);
            }
        }
    }
}


void LWidget::setWindowTitleHelper(const std::wstring &t)
{
    if (testAttribute(LWidgetAttribute::WStateCreated)){
        setWindowTitleSys(t);
    }
}

void LWidget::setCursorSys(bool force)
{
    std::shared_ptr<LWidget> w= getPtrT<LWidget>();
    if (!w->testAttribute(LWidgetAttribute::WStateCreated)){
        return;
    }

    static std::weak_ptr<LWidget> lastUnderMouse;
    std::shared_ptr<LWidget> lastUnderMouseLock = lastUnderMouse.lock();
    if (force) {
        lastUnderMouse = w;
    } else if (lastUnderMouseLock != nullptr) {
        const WId lastWinId = lastUnderMouseLock->effectiveWinId();
        const WId winId = w->effectiveWinId();
        if (lastWinId && lastWinId == winId){
            w = lastUnderMouseLock;
        }
    } else if (!w->internalWinId()) {
        return; // The mouse is not under this widget, and it's not native, so don't change it.
    }

    while (!w->internalWinId() && !w->isWindow() && !w->testAttribute(LWidgetAttribute::SetCursor)){
        std::shared_ptr<LWidget> p = w->parentT<LWidget>();
        if(p != nullptr){
            w = p;
        }else{
            break;
        }
    }

    std::shared_ptr<LWidget> nativeParent = w;
    if (!w->internalWinId()){
        nativeParent = w->nativeParentWidget();
    }
    if (nativeParent==nullptr || !nativeParent->internalWinId()){
        return;
    }

    std::shared_ptr<LWindow> win = nativeParent->windowHandle();
    if (win != nullptr){
        if (w->isWindow() || w->testAttribute(LWidgetAttribute::SetCursor)) {
            if (w->isEnabled()){
                win->setCursor(w->cursor());
                return;
            }
        }
        win->unsetCursor();
    }
}

void LWidget::setWindowTitleSys(const std::wstring &caption)
{
    if (!isWindow()){
        return;
    }

    std::shared_ptr<LWindow>  wh = windowHandle();
    if(wh != nullptr){
        wh->setTitle(caption);
    }
}

LWindowModality LWidget::windowModality() const
{
    return static_cast<LWindowModality>(m_widgetData.windowModality);
}

void LWidget::setWindowModality(LWindowModality windowModality)
{
    m_widgetData.windowModality = (uint32_t)windowModality;
    // setModal_sys() will be called by setAttribute()
    setAttribute(LWidgetAttribute::ShowModal, (m_widgetData.windowModality != (uint32_t)LWindowModality::NonModal));
    setAttribute(LWidgetAttribute::SetWindowModality, true);
}

void LWidget::setParent(std::shared_ptr<LTreeObj> parent)
{
    std::shared_ptr<LWidget> p = std::dynamic_pointer_cast<LWidget>(parent);
    if(parent!=nullptr && p== nullptr){
        lWarn() << "Cannot set a widget with a non widget parent!";
        return;
    }

    std::shared_ptr<LWidget> pw = parentT<LWidget>();
    if (p == pw){
        return;
    }
    setParent(p, uint32_t(m_widgetData.windowFlags) & ~uint32_t(LWindowType::WindowTypeMask));
}


void LWidget::setParent(std::shared_ptr<LWidget> parentIn, LFlags<LWindowType> f)
{
    bool resized = testAttribute(LWidgetAttribute::Resized);
    bool wasCreated = testAttribute(LWidgetAttribute::WStateCreated);
    std::shared_ptr<LWidget>  oldtlw = window();

    if (f & LWindowType::Window){ // Frame geometry likely changes, refresh.
        m_widgetData.fstrutDirty = true;
    }

    std::shared_ptr<LWidget> desktopWidget = nullptr;
    if(parentIn != nullptr){
        if (parentIn->windowType() == LWindowType::Desktop){
            desktopWidget = parentIn;
        }
    }

    std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();
    std::shared_ptr<LWidget> pw = parentT<LWidget>();
    bool newParent = (parentIn != pw) || !wasCreated || (desktopWidget != nullptr);

    if(parentIn != nullptr && (desktopWidget == nullptr)){
        if (newParent) {
            if (testAttribute(LWidgetAttribute::NativeWindow)
                && !LCoreApplication::testAttribute(LAppAttribute::DontCreateNativeWidgetSiblings)){
                parentIn->enforceNativeChildren();
            }
            else if (parentIn->nativeChildrenForced()
                     || parentIn->testAttribute(LWidgetAttribute::PaintOnScreen)){
                setAttribute(LWidgetAttribute::NativeWindow);
            }
        }
    }

    if (wasCreated) {
        if (!testAttribute(LWidgetAttribute::WStateHidden)) {
            setVisible(false);
            setAttribute(LWidgetAttribute::WStateExplicitShowHide, false);
        }
        if (newParent) {
            std::shared_ptr<LEvent> e = std::make_shared<LEvent>(LEvent::ParentAboutToChange);
            LCoreApplication::sendEvent(tThis, e);
        }
    }

    std::shared_ptr<LWidget> tFocusChild = m_focusChild.lock();
    if (newParent && isAncestorOf(tFocusChild)){
        tFocusChild->clearFocus();
    }

    setParentSys(parentIn, f);

    if (desktopWidget != nullptr){
        parentIn = nullptr;
    }
    clearCachedStyle();

    if (LWidgetRepaintMan *oldPaintManager = oldtlw->maybeRepaintMan()) {
        if (newParent){
            oldPaintManager->removeDirtyWidget(tThis);
        }
        // Move the widget and all its static children from
        // the old backing store to the new one.
        oldPaintManager->moveStaticWidgets(tThis);
    }

    reparentFocusWidgets(oldtlw);
    setAttribute(LWidgetAttribute::Resized, resized);

    const bool useStyleSheetPropagationInWidgetStyles =
        LCoreApplication::testAttribute(LAppAttribute::UseStyleSheetPropagationInWidgetStyles);

    if (!useStyleSheetPropagationInWidgetStyles && !testAttribute(LWidgetAttribute::StyleSheet)){
        if (parentIn != nullptr) {
            if(parentIn->testAttribute(LWidgetAttribute::StyleSheet)){
//                const auto pd = parentIn->d_fun_dic();
                m_inheritedFontResolveMask = parentIn->m_directFontResolveMask | parentIn->m_inheritedFontResolveMask;
                m_inheritedPaletteResolveMask = parentIn->m_directPaletteResolveMask | parentIn->m_inheritedPaletteResolveMask;
                resolveFont();
                resolvePalette();
            }
        }else{
            resolveFont();
            resolvePalette();
        }
    }
    resolveLayoutDirection();
    resolveLocale();


    // platforms).
    if (newParent) {
        // propagate enabled updates enabled state to non-windows
        if (!isWindow()) {
            if (!testAttribute(LWidgetAttribute::ForceDisabled)){
                setEnabledHelper(parentIn ? parentIn->isEnabled() : true);
            }
            if (!testAttribute(LWidgetAttribute::ForceUpdatesDisabled)){
                setUpdatesEnabledHelper(parentIn ? parentIn->updatesEnabled() : true);
            }
        }
        inheritStyle();

        // send and post remaining QObject events
        if (parentIn != nullptr && m_sendChildEvents) {
            std::shared_ptr<LChildEvent> e = std::make_shared<LChildEvent>(LEvent::ChildAdded, tThis);
            LCoreApplication::sendEvent(parentIn, e);
        }

        std::shared_ptr<LEvent> e = std::make_shared<LEvent>(LEvent::ParentAboutToChange);
        LCoreApplication::sendEvent(tThis, e);
    }

    if (!wasCreated) {
        if (isWindow()){
            std::shared_ptr<LWidget> p =parentT<LWidget>();
            if(p != nullptr){
                if(p->isVisible()){
                    setAttribute(LWidgetAttribute::WStateHidden, true);
                }
            }
        }
        else if (!testAttribute(LWidgetAttribute::WStateExplicitShowHide)){
            setAttribute(LWidgetAttribute::WStateHidden, false);
        }
    }

    updateIsOpaque();

    if(m_pExtraData != nullptr){
        if(m_pExtraData->hasWindowContainer){
            LWindowContainer::parentWasChanged(tThis);
        }
    }
}

std::shared_ptr<LDialog> LWidget::dialogParent() const
{
    std::shared_ptr<LWidget> p = parentT<LWidget>();
    while (p!= nullptr) {
        std::shared_ptr<LDialog> d = std::dynamic_pointer_cast<LDialog>(p);
        if (d != nullptr){
            return d;
        }

        if(p->isWindow()){
            break;
        }else{
            p = p->parentT<LWidget>();
        }
    }
    return nullptr;
}


bool LWidget::isAncestorOf(std::shared_ptr<LWidget> child) const
{
    while (child != nullptr) {
        if (child.get() == this){
            return true;
        }
        if (child->isWindow()){
            return false;
        }
        child = child->parentT<LWidget>();
    }
    return false;
}

void LWidget::clearFocus()
{
    std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();
    if (hasFocus()) {
        if (testAttribute(LWidgetAttribute::InputMethodEnabled)){
            LGuiApplication::inputMethod()->commit();
        }

        std::shared_ptr<LFocusEvent> focusAboutToChange
            = std::make_shared<LFocusEvent>(LEvent::FocusAboutToChange);
        LCoreApplication::sendEvent(tThis, focusAboutToChange);
    }

    std::shared_ptr<LWidget> w = tThis;
    while (w) {
        // Just like setFocus(), we update (clear) the focus_child of our parents
        std::shared_ptr<LWidget> wFocusChild = w->m_focusChild.lock();
        if (wFocusChild.get() == this){
            w->m_focusChild.reset();
        }
        w = w->parentT<LWidget>();
    }

    if (LTLWExtra *extra = window()->maybeTopData()) {
        if (extra->window != nullptr){
            lGuiApp->updateFocusObject(extra->window->focusObject());
        }
    }

    if (hasFocus()) {
        // Update proxy state
        LApplication::setFocusWidget(nullptr, LFocusReason::Other);
    }
}

bool LWidget::hasFocus() const
{
    std::shared_ptr<LWidget> w = getPtrT<LWidget>();
    while (w->m_pExtraData != nullptr){
        std::shared_ptr<LWidget> p = w->m_pExtraData->focusProxy.lock();
        if(p == nullptr){
            break;
        }
        w = p;
    }

    return (LApplication::m_focusWidget.lock() == w);
}

void LWidget::setFocus(LFocusReason reason)
{
    if (!isEnabled()){
        return;
    }

    std::shared_ptr<LWidget> f = deepestFocusProxy();
    if (f== nullptr){
        f = getPtrT<LWidget>();
    }

    if (LApplication::focusWidget() == f){
        return;
    }


    if (f->isActiveWindow()) {
        std::shared_ptr<LWidget> prev = LApplication::m_focusWidget.lock();
        if (prev != nullptr) {
            if (reason != LFocusReason::Popup && reason != LFocusReason::MenuBar
                && prev->testAttribute(LWidgetAttribute::InputMethodEnabled)) {
                LGuiApplication::inputMethod()->commit();
            }

            if (reason != LFocusReason::No) {
                std::shared_ptr<LFocusEvent> focusAboutToChange
                    = std::make_shared<LFocusEvent>(LEvent::FocusAboutToChange, reason);
                LCoreApplication::sendEvent(prev, focusAboutToChange);
            }
        }

        f->updateFocusChild();

        LApplication::setFocusWidget(f, reason);
    } else {
        f->updateFocusChild();
    }
}

std::shared_ptr<LWidget> LWidget::focusWidget() const
{
    return m_focusChild.lock();
}

std::shared_ptr<LWidget> LWidget::nextInFocusChain() const
{
    return m_focusNext.lock();
}

std::shared_ptr<LWidget> LWidget::previousInFocusChain() const
{
    return m_focusPrev.lock();
}

std::shared_ptr<LObject> LWidget::focusObject()
{
    std::shared_ptr<LWidget>  proxy = deepestFocusProxy();
    return proxy!=nullptr ? proxy : getPtrT<LObject>();
}

std::shared_ptr<LWidget> LWidget::childAt(const LPoint<int> &pos) const
{
    if (m_children.empty()){
        return nullptr;
    }

    if (!pointInsideRectAndMask(pos)){
        return nullptr;
    }
    return childAtRecursiveHelper(pos);
}

LFocusPolicy LWidget::focusPolicy() const
{
    return (LFocusPolicy)m_widgetData.focusPolicy;
}

void LWidget::setFocusPolicy(LFocusPolicy policy)
{
    m_widgetData.focusPolicy = (uint8_t) policy;
    if(m_pExtraData != nullptr){
        std::shared_ptr<LWidget> p = m_pExtraData->focusProxy.lock();
        if(p != nullptr){
            p->setFocusPolicy(policy);
        }
    }
}

std::shared_ptr<LWidget> LWidget::find(WId id)
{
    std::shared_ptr<LWidget> ret = nullptr;
    auto iter = LApplication::m_mapper.find(id);
    if(iter != LApplication::m_mapper.end()){
        ret = iter->second.lock();
    }
    return ret;
}

void LWidget::setWindowTitle(const std::wstring &title)
{
    if (windowTitle() == title && !title.empty()){
        return;
    }

    topData()->caption = title;
    setWindowTitleHelper(title);

    std::shared_ptr<LEvent> e = std::make_shared<LEvent>(LEvent::WindowTitleChange);
    std::shared_ptr<LWidget> q = getPtrT<LWidget>();
    LCoreApplication::sendEvent(q, e);

}

std::wstring LWidget::windowTitle() const
{
    if(m_pExtraData != nullptr){
        if (m_pExtraData->topExtra != nullptr) {
            return m_pExtraData->topExtra->caption;
        }
    }
    return L"";
}

void LWidget::setFocusProxy(std::shared_ptr<LWidget> w)
{
    if (w== nullptr && m_pExtraData == nullptr){
        return;
    }

    std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();
    for (std::shared_ptr<LWidget> fp  = w; fp; fp = fp->focusProxy()) {
        if (L_UNLIKELY(fp == tThis)) {
            lWarn()<< "LWidget already in focus proxy chain";
            return;
        }
    }

    const bool moveFocusToProxy = (LApplication::m_focusWidget.lock() == tThis);

    createExtra();
    m_pExtraData->focusProxy = w;

    if (moveFocusToProxy){
        setFocus(LFocusReason::Other);
    }
}

std::shared_ptr<LWidget> LWidget::focusProxy() const
{
    if(m_pExtraData != nullptr){
        return m_pExtraData->focusProxy.lock();
    }
    return nullptr;
}

void LWidget::update()
{
    update(rect());
}

void LWidget::update(const LRect<int> &r)
{
    if (!isVisible() || !updatesEnabled()){
        return;
    }

    LRect<int> clipped = r & rect();
    if (clipped.isEmpty()){
        return;
    }

    std::shared_ptr<LWidget> q =getPtrT<LWidget>();

    if (testAttribute(LWidgetAttribute::WStateInPaintEvent)) {
        std::shared_ptr<LUpdateLaterEvent> e = std::make_shared<LUpdateLaterEvent>(clipped);
        LCoreApplication::postEvent(q, e);
        return;
    }

    LTLWExtra *tlwExtra = window()->maybeTopData();
    if (tlwExtra !=nullptr){
        if(tlwExtra->backingStore != nullptr){
            tlwExtra->repaintMan->markDirty(clipped, q);
        }
    }
}

void LWidget::update(const std::shared_ptr<LRegion> r)
{
    if (!isVisible() || !updatesEnabled() || r== nullptr || r->isEmpty()){
        return;
    }

    std::shared_ptr<LRegion> clipped = r->intersected(rect());
    if (clipped->isEmpty()){
        return;
    }

    std::shared_ptr<LWidget> q =getPtrT<LWidget>();

    if (testAttribute(LWidgetAttribute::WStateInPaintEvent)) {
        std::shared_ptr<LUpdateLaterEvent> e = std::make_shared<LUpdateLaterEvent>(clipped);
        LCoreApplication::postEvent(q, e);
        return;
    }

    LTLWExtra *tlwExtra = window()->maybeTopData();
    if (tlwExtra !=nullptr){
        if(tlwExtra->backingStore != nullptr){
            tlwExtra->repaintMan->markDirty(clipped, q);
        }
    }
}

void LWidget::repaint()
{
    repaint(rect());
}

void LWidget::repaint(const LRect<int> &r)
{
    if (!isVisible() || !updatesEnabled() || r.isEmpty()){
        return;
    }

    LTLWExtra *tlwExtra = window()->maybeTopData();
    if (tlwExtra !=nullptr){
        if(tlwExtra->backingStore != nullptr){
            std::shared_ptr<LWidget> q =getPtrT<LWidget>();
            tlwExtra->repaintMan->markDirty(r, q, LWidgetRepaintMan::UpdateNow);
        }
    }
}

void LWidget::repaint(const std::shared_ptr<LRegion> r)
{
    if (!isVisible() || !updatesEnabled() || r == nullptr || r->isEmpty()){
        return;
    }

    LTLWExtra *tlwExtra = window()->maybeTopData();
    if (tlwExtra !=nullptr){
        if(tlwExtra->backingStore != nullptr){
            std::shared_ptr<LWidget> q =getPtrT<LWidget>();
            tlwExtra->repaintMan->markDirty(r, q, LWidgetRepaintMan::UpdateNow);
        }
    }
}

bool LWidget::autoFillBackground() const
{
    if(m_pExtraData != nullptr){
        return m_pExtraData->autoFillBackground;
    }
    return false;
}

void LWidget::setAutoFillBackground(bool enabled)
{
    if (m_pExtraData == nullptr){
        createExtra();
    }
    if (m_pExtraData->autoFillBackground == enabled){
        return;
    }

    m_pExtraData->autoFillBackground = enabled;
    updateIsOpaque();
    update();
    updateIsOpaque();
}

void LWidget::setBackgroundRole(LPalette::ColorRole role)
{
    m_bgRole = role;
    //Todo
//    updateSystemBackground();
//    propagatePaletteChange();
    updateIsOpaque();
}

LPalette::ColorRole LWidget::backgroundRole() const
{
    std::shared_ptr<LWidget> w = getPtrT<LWidget>();
    do {
        LPalette::ColorRole role = w->m_bgRole;
        if (role != LPalette::NoRole){
            return role;
        }
        if (w->isWindow() || w->windowType() == LWindowType::SubWindow){
            break;
        }
        w = w->parentT<LWidget>();
    } while (w);
    return LPalette::Window;
}

void LWidget::setForegroundRole(LPalette::ColorRole role)
{
    m_fgRole = role;
    //Todo
//    updateSystemBackground();
//    propagatePaletteChange();
}

LPalette::ColorRole LWidget::foregroundRole() const
{
    if (m_fgRole != LPalette::NoRole){
        return m_fgRole;
    }
    LPalette::ColorRole role = LPalette::WindowText;
    switch (backgroundRole()) {
    case LPalette::Button:{
        role = LPalette::ButtonText;
        break;
    }
    case LPalette::Base:{
        role = LPalette::Text;
        break;
    }
    case LPalette::Dark:
    case LPalette::Shadow:{
        role = LPalette::Light;
        break;
    }
    case LPalette::Highlight:{
        role = LPalette::HighlightedText;
        break;
    }
    case LPalette::ToolTipBase:{
        role = LPalette::ToolTipText;
        break;
    }
    default:{
        break;
    }
    }
    return role;
}

int LWidget::x() const
{
    if (isWindow() && ! (windowType() == LWindowType::Popup)){
        return m_widgetData.crect.m_x - frameStrut().m_left;
    }
    return m_widgetData.crect.m_x;
}

int LWidget::y() const
{
    if (isWindow() && ! (windowType() == LWindowType::Popup)){
        return m_widgetData.crect.m_y - frameStrut().m_top;
    }
    return m_widgetData.crect.m_y;
}

int LWidget::heightForWidth(int w) const
{
    if(m_layout != nullptr){
        if(m_layout->hasHeightForWidth()){
            return m_layout->totalHeightForWidth(w);
        }
    }
    return -1;
}

bool LWidget::hasHeightForWidth() const
{
    if(m_layout != nullptr){
        return m_layout->hasHeightForWidth();
    }else{
        return m_sizePolicy.hasHeightForWidth();
    }
}

LSize<int> LWidget::sizeHint() const
{
    if (m_layout != nullptr){
        return m_layout->totalSizeHint();
    }
    return LSize<int>(-1, -1);
}

LSize<int> LWidget::minimumSizeHint() const
{
    if (m_layout != nullptr){
        return m_layout->totalMinimumSize();
    }
    return LSize<int>(-1, -1);
}

std::shared_ptr<LPalette> LWidget::palette() const
{
    std::shared_ptr<LPalette> ret = LGuiApplication::palette();
    if (!isEnabled()) {
        ret->setCurrentColorGroup(LPalette::Disabled);
    } else if (!isVisible() || isActiveWindow()){
        ret->setCurrentColorGroup(LPalette::Active);
    } else {
        ret->setCurrentColorGroup(LPalette::Inactive);
    }
    return ret;
}

LPalette::ColorGroup LWidget::colorGroup()
{
    if (!isEnabled()) {
        return LPalette::Disabled;
    } else if ((!isVisible()) || isActiveWindow()){
        return LPalette::Active;
    } else {
        return LPalette::Inactive;
    }
}

void LWidget::setLayout(std::shared_ptr<LLayout> l)
{
    if (l == nullptr) {
        lWarn() << "LWidget::setLayout: Cannot set layout to nullptr!";
        return;
    }
    if (m_layout != nullptr) {
        if (m_layout != l){
            lWarn() << "LWidget::setLayout: Cannot set layout for already has layout!";
        }
        return;
    }

    std::shared_ptr<LObject> oldParent = l->parent();
    if (oldParent != nullptr && oldParent.get() != this) {
        if (oldParent->isWidgetType()) {
            // Steal the layout off a widget parent. Takes effect when
            // morphing laid-out container widgets in Designer.
            std::shared_ptr<LWidget> oldParentWidget = std::dynamic_pointer_cast<LWidget>(oldParent);
            oldParentWidget->takeLayout();
        } else {
            lWarn() << "LWidget::setLayout: Cannot set layout which has a layout parent already!";
            return;
        }
    }

    m_layout = l;
    std::shared_ptr<LWidget> q  =getPtrT<LWidget>();
    if (oldParent != q) {
        l->setParentWidget(q);
        l->invalidate();
    }

    if (isWindow() ){
        LTLWExtra * t = maybeTopData();
        if(t != nullptr){
            t->sizeAdjusted = false;
        }
    }
}

void LWidget::updateGeometry()
{
    updateGeometryHelper(false);
}

void LWidget::initPainter(LPainter *painter) const
{
    std::shared_ptr<LPainterState> s = painter->state();
    if(s!= nullptr){
        std::shared_ptr<LPalette> p= palette();
        //复制一份，以免绘制过程中发生改变
        std::shared_ptr<LBrush> foreground = p->brush(foregroundRole());
        std::shared_ptr<LBrush> background = p->brush(backgroundRole());

        std::shared_ptr<LPen> t = std::make_shared<LPen>(foreground, 1);
        painter->setPen(t, false);
        painter->setBackground(background);
        painter->setFont(font());
    }
}


void LWidget::setModalSys()
{
    std::shared_ptr<LWindow>  wh = windowHandle();
    if (wh != nullptr){
        wh->setModality((LWindowModality)m_widgetData.windowModality);
    }
}

static inline bool isEmbedded(const std::shared_ptr<LWindow> w)
{
    const std::shared_ptr<LPlatformWindow> platformWindow = w->handle();
    if(platformWindow != nullptr){
        return platformWindow->isEmbedded();
    }
    return false;
}

void LWidget::setFocusSys()
{
    if (std::shared_ptr<LWindow> nativeWindow =
        testAttribute(LWidgetAttribute::WStateCreated) ? window()->windowHandle() : nullptr) {
        if (nativeWindow->type() != LWindowType::Popup && nativeWindow != LGuiApplication::focusWindow()
            && (LGuiApplication::applicationState() == LApplicationState::Active
                || LCoreApplication::testAttribute(LAppAttribute::PluginApplication)
                || isEmbedded(nativeWindow))) {
            nativeWindow->requestActivate();
        }
    }
}

void LWidget::setParentSys(std::shared_ptr<LWidget> newParent, LFlags<LWindowType> f)
{
    LFlags<LWindowType> oldFlags = m_widgetData.windowFlags;
    bool wasCreated = testAttribute(LWidgetAttribute::WStateCreated);

    int targetScreen = -1;
    // Handle a request to move the widget to a particular screen
    if (newParent != nullptr){
        if(newParent->windowType() == LWindowType::Desktop) {
            // make sure the widget is created on the same screen as the
            // programmer specified desktop widget
            const std::shared_ptr<LDesktopScreenWidget> sw = std::dynamic_pointer_cast<LDesktopScreenWidget>(newParent);
            targetScreen = (sw!=nullptr) ? sw->screenNumber() : 0;
            newParent = nullptr;
        }
    }

    setWinId(0);

    std::shared_ptr<LWidget> parent = parentT<LWidget>();
    if (parent != newParent) {
        LTreeObj::setParent(newParent);
        std::shared_ptr<LWindow> win = windowHandle();
        if (win != nullptr) {
            win->setFlags(f);
            std::shared_ptr<LWidget> parentWithWindow = nullptr;
            if(newParent!=nullptr){
                if(newParent->windowHandle()!= nullptr){
                    parentWithWindow = newParent;
                }else{
                    parentWithWindow = newParent->nativeParentWidget();
                }
            }
            if (parentWithWindow != nullptr) {
                std::shared_ptr<LWidget> topLevel = parentWithWindow->window();
                if ((f & LWindowType::Window) && (topLevel!=nullptr) && topLevel->windowHandle()) {
                    win->setTransientParent(topLevel->windowHandle());
                    win->setParent(nullptr);
                } else {
                    win->setTransientParent(nullptr);
                    win->setParent(parentWithWindow->windowHandle());
                }
            } else {
                win->setTransientParent(nullptr);
                win->setParent(nullptr);
            }
        }
    }

    if (newParent == nullptr) {
        f |= LWindowType::Window;
        if (targetScreen <0) {
            if (parent != nullptr){
                targetScreen = LDesktopWidget::screenNumber(parent->window());
            }
        }
    }


    std::shared_ptr<LWidget> tThis =  getPtrT<LWidget>();
    bool explicitlyHidden = testAttribute(LWidgetAttribute::WStateHidden)
                            && testAttribute(LWidgetAttribute::WStateExplicitShowHide);

    // Reparenting toplevel to child
    if (wasCreated
        && !(f & LWindowType::Window)
        && (oldFlags & LWindowType::Window)
        && !testAttribute(LWidgetAttribute::NativeWindow)) {

        if(m_pExtraData != nullptr){
            if (m_pExtraData->hasWindowContainer){
                LWindowContainer::toplevelAboutToBeDestroyed(tThis);
            }
        }

        std::shared_ptr<LWindow> newParentWindow = newParent->windowHandle();
        if (newParentWindow == nullptr){
            if (std::shared_ptr<LWidget> npw = newParent->nativeParentWidget()){
                newParentWindow = npw->windowHandle();
            }
        }

        std::vector<std::weak_ptr<LWindow> >& winChildren = windowHandle()->children();
        for(const std::weak_ptr<LWindow>& tChild: winChildren){
            std::shared_ptr<LWindow> winChild = tChild.lock();
            if(winChild != nullptr){
                std::shared_ptr<LWidgetWindow> childWW
                    = std::dynamic_pointer_cast<LWidgetWindow>(winChild);

                if(childWW != nullptr){
                    std::shared_ptr<LWidget> childWidget = childWW->widget();
                    if(childWidget != nullptr){
                        if(childWidget->testAttribute(LWidgetAttribute::NativeWindow)){
                            winChild->setParent(newParentWindow);
                        }
                    }
                }else{
                    winChild->setParent(newParentWindow);
                }
            }
        }
        destroy();
    }


    adjustFlags(f, tThis);
    m_widgetData.windowFlags = f;
    setAttribute(LWidgetAttribute::WStateCreated, false);
    setAttribute(LWidgetAttribute::WStateVisible, false);
    setAttribute(LWidgetAttribute::WStateHidden, false);

    if (newParent && wasCreated && (testAttribute(LWidgetAttribute::NativeWindow) || (f & LWindowType::Window))){
        createWinId();
    }

    if (isWindow() || (!newParent || newParent->isVisible()) || explicitlyHidden){
        setAttribute(LWidgetAttribute::WStateHidden);
    }
    setAttribute(LWidgetAttribute::WStateExplicitShowHide, explicitlyHidden);

    // move the window to the selected screen
    if (!newParent && (targetScreen>=0)) {
        // only if it is already created
        if (testAttribute(LWidgetAttribute::WStateCreated)){
            windowHandle()->setScreen(LGuiApplication::screenAt(targetScreen, 0));
        }
        else{
            topData()->initialScreenIndex = targetScreen;
        }
    }
}

void LWidget::raiseSys()
{
    if (isWindow() || testAttribute(LWidgetAttribute::NativeWindow)) {
        windowHandle()->raise();
    } else if (m_renderToTexture) {
        std::shared_ptr<LWidget> parent = parentT<LWidget>();
        if (parent != nullptr) {
            setDirtyOpaqueRegion();
            parent->invalidateBackingStore(geometry());
        }
    }
}

void LWidget::lowerSys()
{
    if (isWindow() || testAttribute(LWidgetAttribute::NativeWindow)) {
        windowHandle()->lower();
    } else{
        std::shared_ptr<LWidget> parent = parentT<LWidget>();
        if (parent != nullptr) {
            setDirtyOpaqueRegion();
            parent->invalidateBackingStore(geometry());
        }
    }
}

void LWidget::reparentFocusWidgets(std::shared_ptr<LWidget> oldtlw)
{
    std::shared_ptr<LWidget> win = window();
    if (oldtlw == win){
        return; // nothing to do
    }

    std::shared_ptr<LWidget> tFocusChild = m_focusChild.lock();
    if(tFocusChild != nullptr){
        tFocusChild->clearFocus();
    }

    // separate the focus chain into new (children of myself) and old (the rest)
    std::shared_ptr<LWidget> firstOld = nullptr;
    //QWidget *firstNew = q; //invariant
    std::shared_ptr<LWidget> o = nullptr; // last in the old list

    std::shared_ptr<LWidget> q = getPtrT<LWidget>();
    std::shared_ptr<LWidget> n = q; // last in the new list

    bool prevWasNew = true;
    std::shared_ptr<LWidget> w = m_focusNext.lock();

    //Note: for efficiency, we do not maintain the list invariant inside the loop
    //we append items to the relevant list, and we optimize by not changing pointers
    //when subsequent items are going into the same list.
    while (w  != q) {
        if(w == nullptr){
            break;
        }

        bool currentIsNew =  q->isAncestorOf(w);
        if (currentIsNew) {
            if (!prevWasNew) {
                //prev was old -- append to new list
                n->m_focusNext = w;
                w->m_focusPrev = n;
            }
            n = w;
        } else {
            if (prevWasNew) {
                //prev was new -- append to old list, if there is one
                if (o) {
                    o->m_focusNext = w;
                    w->m_focusPrev = o;
                } else {
                    // "create" the old list
                    firstOld = w;
                }
            }
            o = w;
        }
        w = w->m_focusNext.lock();
        prevWasNew = currentIsNew;
    }

    //repair the old list:
    if (firstOld != nullptr && o != nullptr) {
        o->m_focusNext = firstOld;
        firstOld->m_focusPrev = o;
    }

    if (!isWindow()) {
        std::shared_ptr<LWidget> prev = win->m_focusPrev.lock();

        win->m_focusPrev = n;
        if(prev != nullptr){
            prev->m_focusNext = q;
            m_focusPrev = prev;
        }else{
            m_focusPrev.reset();
        }
        n->m_focusNext = win;
    } else {
        //repair the new list
        n->m_focusNext = q;
        m_focusPrev = n;
    }
}

void LWidget::sendPendingMoveAndResizeEvents(bool recursive, bool disableUpdates)
{

}

void LWidget::showChildren(bool spontaneous)
{
    for(std::shared_ptr<LTreeObj>& obj : m_children){
        std::shared_ptr<LWidget> widget = std::dynamic_pointer_cast<LWidget>(obj);
        if(widget != nullptr){
            if(!widget->testAttribute(LWidgetAttribute::WStateExplicitShowHide)){
                std::shared_ptr<LWindow> win = widget->windowHandle();
                if(win != nullptr){
                    widget->setAttribute(LWidgetAttribute::WStateHidden, false);
                }
            }

            if(widget->isWindow() || widget->testAttribute(LWidgetAttribute::WStateHidden)){
                continue;
            }

            if(spontaneous){
                widget->setAttribute(LWidgetAttribute::Mapped);
                widget->showChildren(true);
                std::shared_ptr<LEvent> e = std::make_shared<LEvent>(LEvent::Show);
                LApplication::sendSpontaneousEvent(widget, e);
            }else{
                if (widget->testAttribute(LWidgetAttribute::WStateExplicitShowHide))
                    widget->showRecursive();
                else
                    widget->show();
            }
        }
    }
}

void LWidget::hideChildren(bool spontaneous)
{
    for(std::shared_ptr<LTreeObj>& obj : m_children){
        std::shared_ptr<LWidget> widget = std::dynamic_pointer_cast<LWidget>(obj);
        if(widget != nullptr){
            if (widget->isWindow() || widget->testAttribute(LWidgetAttribute::WStateHidden)){
                continue;
            }

            if (spontaneous){
                widget->setAttribute(LWidgetAttribute::Mapped, false);
            }
            else{
                widget->setAttribute(LWidgetAttribute::WStateVisible, false);
            }
            widget->hideChildren(spontaneous);
            std::shared_ptr<LEvent> e = std::make_shared<LEvent>(LEvent::Hide);
            if (spontaneous) {
                LApplication::sendSpontaneousEvent(widget, e);
            } else {
                LCoreApplication::sendEvent(widget, e);
                if (widget->internalWinId()
                    && widget->testAttribute(LWidgetAttribute::DontCreateNativeAncestors)) {
                    // hide_sys() on an ancestor won't have any affect on this
                    // widget, so it needs an explicit hide_sys() of its own
                    widget->hideSys();
                }
            }
            ((LApplication*)lApp)->sendSyntheticEnterLeave(widget);
        }
    }
}

void LWidget::showRecursive()
{
    if (!testAttribute(LWidgetAttribute::WStateCreated)){
        createRecursively();
    }
    //ensurePolished();
    if(!isWindow()){
        std::shared_ptr<LWidget> p = parentT<LWidget>();
        if(p!= nullptr){
            if(p->m_layout != nullptr && !p->m_widgetData.inShow ){
                p->m_layout->activate();
            }
        }
    }

    if (m_layout != nullptr){
        m_layout->activate();
    }
    showHelper();
}

void LWidget::createExtra(){
    if (m_pExtraData == nullptr) {                                // if not exists
        m_pExtraData = std::make_unique<LWidgetExtra>();
        //m_pExtraData->glContext = nullptr;

        m_pExtraData->minw = 0;
        m_pExtraData->minh = 0;
        m_pExtraData->maxw = LWIDGETSIZE_MAX;
        m_pExtraData->maxh = LWIDGETSIZE_MAX;
        m_pExtraData->explicitMinSize = 0;
        m_pExtraData->explicitMaxSize = 0;
        m_pExtraData->autoFillBackground = 0;
        m_pExtraData->nativeChildrenForced = 0;
        m_pExtraData->inRenderWithPainter = 0;
        m_pExtraData->hasWindowContainer = false;
        createUserExtra();
    }

}

void LWidget::LWidget::createTLExtra(){
    if (m_pExtraData == nullptr){
        createExtra();
    }
    if (m_pExtraData->topExtra == nullptr) {
        m_pExtraData->topExtra = std::make_unique<LTLWExtra>();
        LTLWExtra* x = m_pExtraData->topExtra.get();
        x->backingStore = nullptr;
//        x->sharedPainter = nullptr;
        x->incw = x->inch = 0;
        x->basew = x->baseh = 0;
        x->frameStrut.reset();
//        x->normalGeometry.reset();
        x->savedFlags = LWindowType::Widget;
        x->opacity = 255;
        x->posIncludesFrame = 0;
        x->sizeAdjusted = false;
        x->embedded = 0;
        x->window = nullptr;
        x->initialScreenIndex = -1;
    }
}

void LWidget::createTLSysExtra()
{
    if (!m_pExtraData->topExtra->window && (testAttribute(LWidgetAttribute::NativeWindow) || isWindow())) {
        std::shared_ptr<LWidget> q = getPtrT<LWidget>();
        m_pExtraData->topExtra->window = makeInit<LWidgetWindow>(q);
        if (m_pExtraData->minw || m_pExtraData->minh){
            m_pExtraData->topExtra->window->setMinimumSize(LSize<int>(m_pExtraData->minw, m_pExtraData->minh));
        }
        if (m_pExtraData->maxw != LWIDGETSIZE_MAX || m_pExtraData->maxh != LWIDGETSIZE_MAX){
            m_pExtraData->topExtra->window->setMaximumSize(LSize<int>(m_pExtraData->maxw, m_pExtraData->maxh));
        }
        if (m_pExtraData->topExtra->opacity != 255 && isWindow()){
            m_pExtraData->topExtra->window->setOpacity(double(m_pExtraData->topExtra->opacity) / double(255.0));
        }
    }
}

void LWidget::destroy(bool destroyWindow, bool destroySubWindows)
{
    //Todo
}

std::shared_ptr<LLayout>  LWidget::takeLayout()
{
    std::shared_ptr<LLayout> l =  m_layout;
    if(l == nullptr){
        return nullptr;
    }
    m_layout = nullptr;
    l->setParentWidget(nullptr);
    return l;
}

LK_NAMESPACE_END


