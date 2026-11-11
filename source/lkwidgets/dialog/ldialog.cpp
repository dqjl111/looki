#include "ldialog.h"
#include "lkcore/leventloop.h"
#include "lkgui/lplatformdialoghelper.h"
#include "lkwidgets/core/lapplication.h"
#include "lkwidgets/widget/lbutton.h"

LK_NAMESPACE_BEGIN

LDialog::LDialog(std::shared_ptr<LWidget> parent, LFlags<LWindowType> f)
    : LWidget(parent, f | ((f & LWindowType::WindowTypeMask) == LWindowType(0) ? LWindowType::Dialog : LWindowType(0)))
{

}


int LDialog::exec()
{
    if(m_eventLoop != nullptr){
        if(m_eventLoop->isRunning()){
            lWarn() << "Alread run dialog!";
            return -1;
        }
    }

    resetModalitySetByOpen();

    bool wasShowModal = testAttribute(LWidgetAttribute::ShowModal);
    setAttribute(LWidgetAttribute::ShowModal, true);
    setResult(0);

    show();
    m_eventLoop = std::make_shared<LEventLoop>();
    m_eventLoop->exec();
    m_eventLoop = nullptr;

    setAttribute(LWidgetAttribute::ShowModal, wasShowModal);
    return m_rescode;
}

void LDialog::open()
{
    LWindowModality modality = windowModality();
    if (modality != LWindowModality::WindowModal) {
        m_resetModalityTo = (int)modality;
        m_wasModalitySet = testAttribute(LWidgetAttribute::SetWindowModality);
        setWindowModality(LWindowModality::WindowModal);
        setAttribute(LWidgetAttribute::SetWindowModality, false);
    }
    setResult(0);
    show();
}

void LDialog::done(int r)
{
    lWarn() << "Dialog done: " << r;
    hide(r);
    finalize(r, r);
}

void LDialog::accept()
{
    done(Accepted);
}

void LDialog::reject()
{
    done(Rejected);
}

void LDialog::setVisible(bool visible)
{
    // We should not block windows by the invisible modal dialog
    // if a platform-specific dialog is implemented as an in-process
    // window, because in this case it will also be blocked.
    const bool dontBlockWindows = testAttribute(LWidgetAttribute::DontShowOnScreen);
    LWindowModality oldModality;
    bool wasModalitySet;

    if (dontBlockWindows) {
        oldModality = windowModality();
        wasModalitySet = testAttribute(LWidgetAttribute::SetWindowModality);
        setWindowModality(LWindowModality::NonModal);
    }

    if (visible) {
        if (testAttribute(LWidgetAttribute::WStateExplicitShowHide)
            && !testAttribute(LWidgetAttribute::WStateHidden)){
            return;
        }

        LWidget::setVisible(visible);

        std::shared_ptr<LWidget> win = window();
        std::shared_ptr<LWidget> fw = win->focusWidget();
        if (fw == nullptr){
            fw = getPtrT<LWidget>();
        }

        std::shared_ptr<LButton> mainDef = m_mainDef.lock();
        if(mainDef == nullptr){
            if(isWindow()){
                std::shared_ptr<LButton> pb = std::dynamic_pointer_cast<LButton>(fw->nextInFocusChain());
                while(pb != nullptr){
                    if(pb->autoDefault() && pb->focusPolicy() != LFocusPolicy::No){
                        pb->setDefault(true);
                        break;
                    }else{
                        pb = std::dynamic_pointer_cast<LButton>(pb->nextInFocusChain());
                    }
                }
            }
        }else{
            if(fw->focusPolicy() == LFocusPolicy::No){
                std::shared_ptr<LWidget> first = fw->nextInFocusChain();
                while(first != nullptr && first != fw){
                    if(first->focusPolicy() == LFocusPolicy::No){
                        first = first->nextInFocusChain();
                    }else{
                        break;
                    }
                }

                if(first != mainDef){
                    std::shared_ptr<LButton> pb = std::dynamic_pointer_cast<LButton>(first);
                    if(pb != nullptr){
                         mainDef->setFocus();
                    }
                }
            }
        }


        if (fw != nullptr){
            if(!fw->hasFocus()) {
                std::shared_ptr<LFocusEvent> e =
                    std::make_shared<LFocusEvent>(LEvent::FocusIn, LFocusReason::Tab);
                LCoreApplication::sendEvent(fw, e);
            }
        }
    } else {
        if (testAttribute(LWidgetAttribute::WStateExplicitShowHide)
            && testAttribute(LWidgetAttribute::WStateHidden)){
            return;
        }

        // Reimplemented to exit a modal event loop when the dialog is hidden.
        LWidget::setVisible(visible);
        if (m_eventLoop != nullptr){
            m_eventLoop->quit();
        }
    }

    if (dontBlockWindows) {
        setWindowModality(oldModality);
        setAttribute(LWidgetAttribute::SetWindowModality, wasModalitySet);
    }
}

void LDialog::setMainDefault(std::shared_ptr<LButton> b)
{
    m_mainDef.reset();
    std::shared_ptr<LWidget> q = getPtrT<LWidget>();
    std::list<std::shared_ptr<LButton> >  list = findChildren<LButton>();
    for (std::shared_ptr<LButton> pb : list) {
        if (pb->window() == q) {
            if (pb == b){
                m_mainDef = b;
            }else{
                pb->setDefault(false);
            }
        }
    }
}

void LDialog::hideDefault()
{
    std::list<std::shared_ptr<LButton> >  list = findChildren<LButton>();
    for (std::shared_ptr<LButton> pb : list) {
        pb->setDefault(false);
    }
}

void LDialog::keyPressEvent(std::shared_ptr<LKeyEvent> e)
{
    if (e->matches(LKeySequence::Cancel)) {
        reject();
    } else{
        if (!e->modifiers() || ((e->modifiers() & LKeyModifier::Keypad) && (e->key() == (uint32_t)LKey::Enter))) {
            switch (e->key()) {
            case (uint32_t)LKey::Enter:
            case (uint32_t)LKey::Return: {
                std::shared_ptr<LButton> pb = m_mainDef.lock();
                if(pb != nullptr){
                    if (pb->isDefault() && pb->isVisible() && pb->isEnabled()) {
                        std::shared_ptr<LWidget> q = getPtrT<LWidget>();
                        if (pb->window() == q) {
                            pb->click();
                            return;
                        }
                    }
                }
                break;
            }
            default:{
                e->ignore();
                return;
            }
            }
        } else {
            e->ignore();
        }
    }
}

void LDialog::closeEvent(std::shared_ptr<LEvent> e)
{
    if (isVisible()) {
        reject();
        if (isVisible()){
            e->ignore();
        }
    } else {
        e->accept();
    }
}

void LDialog::showEvent(std::shared_ptr<LEvent> event)
{
    if (!event->spontaneous() && !testAttribute(LWidgetAttribute::Moved)) {
        LFlags<LWindowState>  state = windowState();
        std::shared_ptr<LWidget> p = parentT<LWidget>();
        adjustPosition(p);
        setAttribute(LWidgetAttribute::Moved, false); // not really an explicit position
        if (state != windowState()){
            setWindowState(state);
        }
    }
}

// void LDialog::contextMenuEvent(std::shared_ptr<LContextMenuEvent> e)
// {

// }

std::shared_ptr<LWindow> LDialog::transientParentWindow() const
{
    const std::shared_ptr<LWidget> parent = nativeParentWidget();
    if (parent != nullptr){
        return parent->windowHandle();
    }
    std::shared_ptr<LWindow> win = windowHandle();
    if (win != nullptr){
        return win->transientParent();
    }
    return nullptr;
}


void LDialog::hide(int resultCode)
{
    setResult(resultCode);
    setVisible(false);
    closeHelper(LCloseMode::NoEvent);
    resetModalitySetByOpen();
}

void LDialog::finalize(int resultCode, int dialogCode)
{
    if (dialogCode == LDialog::Accepted){
        sigAccepted.emt();
    }
    else if (dialogCode == LDialog::Rejected){
        sigRejected.emt();
    }
    sigFinished.emt(resultCode);
}

void LDialog::adjustPosition(std::shared_ptr<LWidget> w)
{
    if(LGuiApplication::platformIntegration()
            ->styleHint(LPlatformTheme::Hint::WindowAutoPlacement).toBool()){
        return;
    }

    LPoint<int> p(0, 0);
    int extraw = 0, extrah = 0, scrn = 0;
    if (w != nullptr){
        w = w->window();
    }
    LRect<int> desk;
    if (w != nullptr) {
        scrn = LDesktopWidget::screenNumber(w);
    }
    else{
        scrn = LDesktopWidget::screenNumber(getPtrT<LWidget>());
    }
    desk = LDesktopWidget::availableGeometry(scrn);

    std::list<std::shared_ptr<LWidget> >  list = LApplication::topLevelWidgets();
    for (const std::shared_ptr<LWidget>& current: list) {
        if((extraw == 0) || (extrah == 0)){
            break;
        }

        if (current->isVisible()) {
            int framew = current->geometry().m_x - current->x();
            int frameh = current->geometry().m_y - current->y();

            extraw = std::max<int>(extraw, framew);
            extrah = std::max<int>(extrah, frameh);
        }
    }

    // sanity check for decoration frames. With embedding, we
    // might get extraordinary values
    if (extraw == 0 || extrah == 0 || extraw >= 10 || extrah >= 40) {
        extrah = 40;
        extraw = 10;
    }


    if (w) {
        // Use pos() if the widget is embedded into a native window
        LPoint<int> pp;
        std::shared_ptr<LWindow> window = windowHandle();
        if (window != nullptr){
            pp = w->pos();
        }else{
            pp = w->mapToGlobal(LPoint<int>(0,0));
        }
        p = LPoint<int>(pp.m_x + w->width()/2, pp.m_y + w->height()/ 2);
    } else {
        // p = middle of the desktop
        p = LPoint<int>(desk.m_x + desk.width()/2, desk.m_y + desk.height()/2);
    }

    // p = origin of this
    p = LPoint<int>(p.m_x-width()/2 - extraw, p.m_y-height()/2 - extrah);
    if (p.m_x + extraw + width() > desk.x2()){
        p.m_x = desk.x2() - width() - extraw;
    }

    if (p.m_x < desk.m_x){
        p.m_x = desk.m_x;
    }

    if (p.m_y + extrah + height() > desk.y2()){
        p.m_y = desk.y2() - height() - extrah;
    }

    if (p.m_y < desk.m_y){
        p.m_y = desk.m_y;
    }

    if (scrn >= 0) {
        std::shared_ptr<LWindow> window = windowHandle();
        if (window != nullptr){
            window->setScreen(LGuiApplication::screenAt(scrn));
        }
    }
    move(p);
}

void LDialog::resetModalitySetByOpen()
{
    if (m_resetModalityTo != -1 && !testAttribute(LWidgetAttribute::SetWindowModality)) {
        setWindowModality(LWindowModality(m_resetModalityTo));
        setAttribute(LWidgetAttribute::SetWindowModality, m_wasModalitySet);
    }
    m_resetModalityTo = -1;
}


LK_NAMESPACE_END
