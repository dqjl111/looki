#include "laction.h"
#include "lkwidgets/core/lapplication.h"
#include "lkwidgets/core/lwidgetwindow.h"
#include "lkwidgets/widget/lmenu.h"
LK_NAMESPACE_BEGIN
static bool correctWidgetContext(LShortcutCtxType context,
                                 std::shared_ptr<LWidget> w,
                                 std::shared_ptr<LWidget> activeWindow)
{
    bool visible = w->isVisible();
    if (!visible || !w->isEnabled())
        return false;

    if (context == LShortcutCtxType::Application){
        return LApplication::tryModalHelper(w) == nullptr;
    }

    if (context == LShortcutCtxType::Widget){
        return w == LApplication::focusWidget();
    }

    if (context == LShortcutCtxType::WidgetWithChildren) {
        std::shared_ptr<LWidget> tw = LApplication::focusWidget();
        while (tw != nullptr && tw != w){
            LWindowType winType = tw->windowType();
            if (winType == LWindowType::Widget
               || winType == LWindowType::Popup
               || winType == LWindowType::SubWindow){
                tw = tw->parentT<LWidget>();
            }else{
                break;
            }
        }
        return tw == w;
    }

    std::shared_ptr<LWidget> tlw = w->window();
    if ((activeWindow!=nullptr) && (activeWindow != tlw)) {
        /* if a floating tool window is active, keep shortcuts on the parent working.
         * and if a popup window is active (f.ex a completer), keep shortcuts on the
         * focus proxy working */
        LWindowType winType = activeWindow->windowType();
        std::shared_ptr<LWidget> p = nullptr;
        if(winType == LWindowType::Tool){
            p = activeWindow->parentT<LWidget>();

        }else if(winType == LWindowType::Popup){
            p = activeWindow->focusProxy();
        }

        if(p != nullptr){
            activeWindow = p->window();
        }
    }

    if (activeWindow != tlw) {
        return false;
    }

    /* if we live in a MDI subwindow, ignore the event if we are
       not the active document window */
    std::shared_ptr<LWidget> sw = w;
    while (sw != nullptr && (sw->windowType() != LWindowType::SubWindow) && !sw->isWindow()){
        sw = sw->parentT<LWidget>();
    }

    if ((sw!=nullptr) && (sw->windowType() == LWindowType::SubWindow)) {
        std::shared_ptr<LWidget> focusWidget = LApplication::focusWidget();
        while (focusWidget!=nullptr && focusWidget != sw){
            focusWidget = focusWidget->parentT<LWidget>();
        }
        return sw == focusWidget;
    }
    return LApplication::tryModalHelper(w) == nullptr;
}


static bool correctActionContext(LShortcutCtxType context,
                                 std::shared_ptr<LAction> a,
                                 std::shared_ptr<LWidget> activeWindow)
{
    if(a == nullptr){
        return false;
    }

    const std::list<std::weak_ptr<LWidget> >& widgets = a->associatedWidgets();
    for (const std::weak_ptr<LWidget>& ww : widgets) {
        std::shared_ptr<LWidget> w = ww.lock();
        if(w!= nullptr){
            std::shared_ptr<LMenu> menu = std::dynamic_pointer_cast<LMenu>(w);
            if (menu != nullptr) {
                a = std::dynamic_pointer_cast<LAction>(menu->menuAction());
                if(a == nullptr){
                    return false;
                }

                if (correctActionContext(context, a, activeWindow)){
                    return true;
                }
            } else{
                if (correctWidgetContext(context, w, activeWindow)){
                    return true;
                }
            }
        }
    }
    return false;
}


bool LAction::widgetShortcutContextMatcher(std::shared_ptr<LObject> object, LShortcutCtxType context)
{
    if(object == nullptr){
        return false;
    }

    std::shared_ptr<LWidget> actWindow = LApplication::activePopupWidget();
    if(actWindow == nullptr){
        actWindow = LApplication::activeWindow();
    }

    if (actWindow == nullptr) {
        std::shared_ptr<LWindow> tWin = LGuiApplication::focusWindow();
        if (tWin !=nullptr) {
            if(tWin->isActive()){
                while (tWin) {
                    std::shared_ptr<LWidgetWindow> widgetWindow = std::dynamic_pointer_cast<LWidgetWindow>(tWin);
                    if (widgetWindow != nullptr) {
                        actWindow = widgetWindow->widget();
                        break;
                    }
                    tWin = tWin->parent();
                }
            }

        }
    }

    if (actWindow == nullptr){
        return false;
    }


    std::shared_ptr<LAction> a = std::dynamic_pointer_cast<LAction>(object);
    if (a != nullptr){
        return correctActionContext(context, a, actWindow);
    }

    std::shared_ptr<LWidget> w = std::dynamic_pointer_cast<LWidget>(object);
    if (w == nullptr) {
        std::shared_ptr<LWindow> tWin = std::dynamic_pointer_cast<LWindow>(object);
        while (tWin != nullptr) {
            std::shared_ptr<LWidgetWindow> widgetWindow = std::dynamic_pointer_cast<LWidgetWindow>(tWin);
            if (widgetWindow != nullptr) {
                w = widgetWindow->widget();
                break;
            }
            tWin = tWin->parent();
        }
    }

    if (w == nullptr){
        return false;
    }

    return correctWidgetContext(context, w, actWindow);
}


LActionBase::LActionBase(bool isSep)
{
    m_isSeparator = isSep;
    m_isEnabled = true;
    m_isVisible = true;
}

LActionBase::LActionBase(const std::wstring& text,
                         std::shared_ptr<LTexture> icon,
                         const LVariant& userData)
    : m_text(text)
    , m_icon(icon)
    , m_userData(userData)
{
    m_isSeparator = false;
    m_isEnabled = true;
    m_isVisible = true;
}

LFlags<LUiState> LActionBase::uiState() const
{
    if (m_isEnabled) {
        if (m_isSelected) {
            return LUiState::Hover;
        } else {
            return LUiState::Enabled;
        }
    } else {
        return LUiState::Disabled;
    }
}

void LActionBase::setSeparator(bool v)
{
    m_isSeparator = v;
}

void LActionBase::setVisible(bool v)
{
    m_isVisible = v;
}

void LActionBase::setEabled(bool v)
{
    m_isEnabled = v;
}

void LActionBase::setCheckable(bool v)
{
    m_isCheckable = v;
}

void LActionBase::setChecked(bool v)
{
    m_isCheckable = true;
    m_isChecked = v;
}

void LActionBase::setSelected(bool v)
{
    m_isSelected = v;
}

void LActionBase::setText(const std::wstring &s)
{
    m_text = s;
}

void LActionBase::setIcon(const std::shared_ptr<LTexture> t)
{
    m_icon = t;
}

void LActionBase::setUserData(const LVariant &data)
{
    m_userData = data;
}

const LPoint<int> LActionBase::actPos() const
{
    return LPoint<int>();
}

void LActionBase::setActPos(const LPoint<int> &p)
{

}


void LActionBase::activate(ActionEvent event)
{
    if(event == Trigger) {
        if(m_isCheckable) {
            setChecked(!m_isChecked);
        }
    }
}

const LKeySequence LActionBase::shortcut() const
{
    return LKeySequence();
}

void LActionBase::close(ActionEvent events)
{

}

const std::shared_ptr<LWidget> LActionBase::widget() const
{
    return nullptr;
}

void LActionBase::setWidget(const std::shared_ptr<LWidget> w, bool isPopup)
{
    m_isPopup = true;
}

LAction::LAction(const std::wstring &text, std::shared_ptr<LTexture> icon)
 : LActionBase(text, icon)
{
    m_isSeparator = false;
}

LAction::~LAction()
{
    if(m_shortCut != nullptr){
        lGuiApp->m_shortcutMan.removeShortcut(m_shortCut);
        m_shortCut = nullptr;
    }
}

const LKeySequence LAction::shortcut() const
{
    if(m_shortCut != nullptr){
        return m_shortCut->m_keyseq;
    }
    return LKeySequence();
}

void LAction::setShortcut(const LKeySequence& seq,
                          LShortcutCtxType ctxType,
                          uint32_t funID,
                          LShortcutEntry::ContextMatcher matcher)
{
    if(m_shortCut != nullptr){
        if(seq.isEmpty()){
            lGuiApp->m_shortcutMan.removeShortcut(m_shortCut);
            m_shortCut = nullptr;
        }else{
            bool needSort = m_shortCut->m_keyseq != seq;
            m_shortCut->m_keyseq = seq;
            m_shortCut->m_context = ctxType;
            m_shortCut->m_id = funID;
            m_shortCut->m_contextMatcher = matcher;
            lGuiApp->m_shortcutMan.setSorted(needSort);
        }
    }else{
        std::shared_ptr<LObject> q = getPtrT<LObject>();
        m_shortCut = lGuiApp->m_shortcutMan.addShortcut(q, seq, ctxType, funID, matcher);
    }
}

void LAction::dissociate(std::shared_ptr<LWidget> w)
{
    auto isWidget = [w](std::weak_ptr<LWidget> ww) { return ww.lock() == w; };
    m_associateWidgets.remove_if(isWidget);
}

void LAction::associate(std::shared_ptr<LWidget> widget)
{
    if (widget == nullptr){
        return;
    }

    auto it = std::find_if(m_associateWidgets.begin(),
                           m_associateWidgets.end(),
                           [widget](std::weak_ptr<LWidget> w) { return w.lock() == widget; });
    if (it == m_associateWidgets.end())
    {
        m_associateWidgets.push_back(widget);
    }
}

const std::shared_ptr<LWidget> LAction::widget() const
{
    return m_widget;
}

void LAction::setWidget(const std::shared_ptr<LWidget> w, bool isPopup)
{
    m_widget = w;
    m_isPopup = isPopup;
}

LSignalAction::LSignalAction(const std::wstring &text, std::shared_ptr<LTexture> texture):LAction(text, texture)
{

}

void LSignalAction::activate(ActionEvent event)
{
    LAction::activate(event);
    if(event == Trigger){
        sigTrigger.emt();
    }
}

LCheckAction::LCheckAction(const std::wstring &text, std::shared_ptr<LTexture> texture):LAction(text, texture)
{

}

void LCheckAction::activate(ActionEvent event)
{
    LAction::activate(event);
    if(event == Trigger){
        sigTrigger.emt(m_isChecked);
    }
}


LK_NAMESPACE_END
