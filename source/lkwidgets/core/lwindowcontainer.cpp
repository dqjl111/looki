#include "lwindowcontainer.h"
#include "lapplication.h"
#include "lkwidgets/widget/labstractscrollarea.h"

LK_NAMESPACE_BEGIN

typedef void (*lwindowcontainerTraverseCallback)(std::shared_ptr<LWidget> parent);
static void lwindowcontainerTraverse(std::shared_ptr<LWidget> parent, lwindowcontainerTraverseCallback callback)
{
    const std::list<std::shared_ptr<LTreeObj> >& children = parent->children();
    for (const std::shared_ptr<LTreeObj>& obj: children) {
        std::shared_ptr<LWidget> w = std::dynamic_pointer_cast<LWidget>(obj);
        if (w != nullptr) {
            if (w->hasWindowContainer()){
                callback(w);
            }
        }
    }
}

LWindowContainer::LWindowContainer(std::shared_ptr<LWindow> embeddedWindow,
                                   std::shared_ptr<LWidget> parent,
                                   LFlags<LWindowType> f)
{

    if (embeddedWindow == nullptr) {
        lWarn() << "LWindowContainer: embedded window cannot be null";
        return;
    }

    if (embeddedWindow->surfaceType() == LSurface::RasterSurface
        && LGuiApplication::platformIntegration()->hasCapability(LPlatformCapability::RasterGLSurface)
        && !LCoreApplication::testAttribute(LAppAttribute::ForceRasterWidgets)){
        embeddedWindow->setSurfaceType(LSurface::RasterGLSurface);
    }

    m_fakeParent = makeInit<LWindow>();
    m_window = embeddedWindow;
    m_window->setParent(m_fakeParent);

    setAcceptDrops(true);
    LGuiApplication::guiInst()->sigFocusWindowChanged.connect(this, &LWindowContainer::focusWindowChanged);
}

std::shared_ptr<LWindow> LWindowContainer::containedWindow() const
{
    return m_window;
}

void LWindowContainer::toplevelAboutToBeDestroyed(std::shared_ptr<LWidget> parent)
{
    std::shared_ptr<LWindowContainer> c = std::dynamic_pointer_cast<LWindowContainer>(parent);
    if((c!= nullptr) && (c->m_window != nullptr)){
        c->m_window->setParent(c->m_fakeParent);
    }
    lwindowcontainerTraverse(parent, toplevelAboutToBeDestroyed);
}

void LWindowContainer::parentWasChanged(std::shared_ptr<LWidget> parent)
{
    std::shared_ptr<LWindowContainer> c = std::dynamic_pointer_cast<LWindowContainer>(parent);
    if((c!= nullptr) && (c->m_window != nullptr)){
        if (c->m_window->parent() != nullptr) {
            c->updateUsesNativeWidgets();
            c->markParentChain();
            std::shared_ptr<LWidget > toplevel = c->m_usesNativeWidgets ? parent : parent->window();
            if (toplevel->windowHandle() == nullptr) {
                toplevel->createTLExtra();
                toplevel->createTLSysExtra();
            }
            c->m_window->setParent(toplevel->windowHandle());
            c->m_fakeParent->destroy();
            c->updateContainGeometry();
        }
    }
    lwindowcontainerTraverse(parent, parentWasChanged);
}

void LWindowContainer::parentWasMoved(std::shared_ptr<LWidget> parent)
{
    std::shared_ptr<LWindowContainer> c = std::dynamic_pointer_cast<LWindowContainer>(parent);
    if((c!= nullptr) && (c->m_window != nullptr)){
        if (c->m_window->parent() != nullptr){
            c->updateContainGeometry();
        }
    }
    lwindowcontainerTraverse(parent, parentWasMoved);
}

void LWindowContainer::parentWasRaised(std::shared_ptr<LWidget> parent)
{
    std::shared_ptr<LWindowContainer> c = std::dynamic_pointer_cast<LWindowContainer>(parent);
    if((c!= nullptr) && (c->m_window != nullptr)){
        if (c->m_window->parent() != nullptr){
            c->m_window->raise();
        }
    }
    lwindowcontainerTraverse(parent, parentWasRaised);
}

void LWindowContainer::parentWasLowered(std::shared_ptr<LWidget> parent)
{
    std::shared_ptr<LWindowContainer> c = std::dynamic_pointer_cast<LWindowContainer>(parent);
    if((c!= nullptr) && (c->m_window != nullptr)){
        if (c->m_window->parent() != nullptr){
            c->m_window->lower();
        }
    }
    lwindowcontainerTraverse(parent, parentWasLowered);
}

bool LWindowContainer::handleEvent(std::shared_ptr<LEvent> e)
{
    if (m_window == nullptr){
        return LWidget::handleEvent(e);
    }

    LEvent::Type type = e->type();
    switch (type) {
    case LEvent::ChildRemoved: {
        std::shared_ptr<LChildEvent> ce = std::dynamic_pointer_cast<LChildEvent>(e);
        if (ce->child() == m_window){
            m_window = nullptr;
        }
        break;
    }
    // The only thing we are interested in is making sure our sizes stay
    // in sync, so do a catch-all case.
    case LEvent::Resize:{
        updateContainGeometry();
        break;
    }
    case LEvent::Move:{
        updateContainGeometry();
        break;
    }
    case LEvent::PolishRequest:{
        updateContainGeometry();
        break;
    }
    case LEvent::Show:{
        updateUsesNativeWidgets();
        if (isStillAnOrphan()) {
            m_window->setParent(m_usesNativeWidgets
                                     ? windowHandle()
                                     : window()->windowHandle());
            m_fakeParent->destroy();
        }
        if (m_window->parent() != nullptr) {
            markParentChain();
            m_window->setVisible(true);
        }
        break;
    }
    case LEvent::Hide:{
        if (m_window->parent()!=nullptr){
            m_window->setVisible(false);
        }
        break;
    }
    case LEvent::FocusIn:{
        if (m_window->parent() != nullptr) {
            if (m_oldFocusWindow.lock() != m_window) {
                m_window->requestActivate();
            } else {
                std::shared_ptr<LWidget> next = nextInFocusChain();
                if(next != nullptr){
                    next->setFocus();
                }
            }
        }
        break;
    }
    case LEvent::Drop:
    case LEvent::DragMove:
    case LEvent::DragLeave:{
        LCoreApplication::sendEvent(m_window, e);
        return e->isAccepted();
    }
    case LEvent::DragEnter:{
        // Don't reject drag events for the entire widget when one
        // item rejects the drag enter
        LCoreApplication::sendEvent(m_window, e);
        e->accept();
        return true;
    }
    case LEvent::Paint:
    {
        static bool needsPunch = !LGuiApplication::platformIntegration()->hasCapability(
            LPlatformCapability::TopStackedNativeChildWindows);
        if (needsPunch) {
            LPainter p(paintDevice());
            p.setCompositionMode(LCompositionMode::Source);
            p.fillRect(rectF(), LColorID::Transparent);
        }
        break;
    }

    default:
        break;
    }

    return LWidget::handleEvent(e);
}

void LWindowContainer::focusWindowChanged(std::shared_ptr<LWindow> focusWindow)
{
    m_oldFocusWindow = focusWindow;
    if (focusWindow == m_window) {
        std::shared_ptr<LWidget> widget = LApplication::focusWidget();
        if (widget != nullptr){
            widget->clearFocus();
        }
    }
}

void LWindowContainer::updateContainGeometry()
{
    if(m_window != nullptr){
        if (!isWindow() && (geometry().y2() <= 0 || geometry().x2() <= 0)){
            m_window->setGeometry(geometry());
        }
        else if (m_usesNativeWidgets){
            m_window->setGeometry(rect());
        }
        else{
            m_window->setGeometry(LRect<int>(mapTo(window(), LPoint<int>()), size()));
        }
    }
}

void LWindowContainer::markParentChain()
{
    std::shared_ptr<LWidget> p = getPtrT<LWidget>();
    while (p != nullptr) {
        p->createExtra();
        p->m_pExtraData->hasWindowContainer = true;
        p = p->parentT<LWidget>();
    }
}

void LWindowContainer::updateUsesNativeWidgets()
{
    if(m_window != nullptr){
        if (m_window->parent() == nullptr){
            return;
        }

        if (internalWinId()) {
            // Allow use native widgets if the window container is already a native widget
            m_usesNativeWidgets = true;
            return;
        }
        bool nativeWidgetSet = false;
        std::shared_ptr<LWidget> p = p->parentT<LWidget>();
        while (p != nullptr) {
            std::shared_ptr<LAbstractScrollArea> s = std::dynamic_pointer_cast<LAbstractScrollArea>(p);
            if (s != nullptr) {
                winId();
                nativeWidgetSet = true;
                break;
            }
            p = p->parentT<LWidget>();
        }
        m_usesNativeWidgets = nativeWidgetSet;
    }
}

LK_NAMESPACE_END
