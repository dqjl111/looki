#include "lmenubar.h"
#include "lkwidgets/draw/lstyle.h"
#include "lkwidgets/core/ldesktopwidget.h"

LK_NAMESPACE_BEGIN

LMenuBar::LMenuBar(std::shared_ptr<LWidget> parent, LFlags<LWindowType> f ) : LAbstractMenu(parent, f)
{
    m_sizePolicy = (LSizePolicy(LSizePolicy::Expanding, LSizePolicy::Minimum, LSizePolicy::MenuBar));
    m_isShortcutVisible = false;
    m_scrollHorizon = true;
    m_scrollable = true;
}

void LMenuBar::updateActionRects(const LRect<int> &containerRect)
{
    if (!m_itemsDirty){
        return;
    }

    //let's reinitialize the buffer
    m_actionRects.resize(m_actions.size(), LRect<int>());
    std::shared_ptr<LStyle> pStyle = style();

    const int itemMargin = pStyle->get(&LStylePixelMetric::menuBarItemMargin, &m_elementStyle);
    int maxItemHeight = 0;

    //calculate size
    const LFontMetrics fm = fontMetrics();
    const int icone = pStyle->get(&LStylePixelMetric::smallIconSize, &m_elementStyle);
    int i=0;
    int viewSize = 0;
    for (const std::shared_ptr<LActionBase>& action : m_actions) {
        if (!action->isVisible()){
            ++i;
            continue;
        }


        if (action->isSeparator()) {
            ++i;
            continue; //we don't really position these!
        }

        LSize<int> sz;
        const std::wstring& s = action->text();
        const std::shared_ptr<LTexture> icon = action->icon();
        // If an icon is set, only the icon is visible
        if (icon != nullptr){
            sz = LSize<int>(icone, icone);
        }else if (!s.empty()){
            sz = fm.size((int)LTextFlag::ShowMnemonic, s);
        }

        if (!sz.isEmpty()) {
            //maximum height
            maxItemHeight = std::max<int>(maxItemHeight, sz.height());
            //append
            m_actionRects[i] = LRect<int>(0, 0, sz.width()+2*itemMargin, sz.height(), false);
            viewSize += m_actionRects[i].width();
        }
        ++i;
    }

    //calculate position
    int x = containerRect.m_x + boundWidth(LDirection::ToLeft);
    int validArea = width() - boundWidth(LDirection::ToLeft) - boundWidth(LDirection::ToRight);
    if(validArea >= m_viewSize){
        m_scrollFlags = ScrollNone;
    }else{
        m_scrollFlags = ScrollDown;
        x += scrollerSize();
    }

    int y = boundWidth(LDirection::ToTop);
    for(LRect<int> &actionRect : m_actionRects) {
        if (actionRect.isNull()){
            continue;
        }
        //resize
        actionRect.setHeight(maxItemHeight);
        actionRect.moveLeft(x);
        actionRect.moveTop(y);

        //keep moving along..
        x += actionRect.width();
    }
    m_scrollOffset = 0;
    m_viewSize = viewSize;
    updateSliderData();
    m_itemsDirty = false;
}

void LMenuBar::popupAction(std::shared_ptr<LActionBase> action, bool activateFirst)
{
    if (action != nullptr) {
        if (action->isEnabled() && action->isPopup()) {
            if(m_currentAction != action)
            {
                if(m_currentAction != nullptr && m_currentAction->isPopup()){
                    std::shared_ptr<LWidget> hideWidget = m_currentAction->widget();
                    if(hideWidget != nullptr){
                        if(hideWidget->isVisible()){
                            std::shared_ptr<LAbstractMenu> pHideMenu = std::dynamic_pointer_cast<LAbstractMenu>(hideWidget);
                            if(pHideMenu!= nullptr){
                                pHideMenu->hideMenu();
                            }else{
                                hideWidget->close();
                            }
                        }
                    }
                }
                m_currentAction = action;
            }

            std::shared_ptr<LWidget> activeWidget = action->widget();
            if(activeWidget != nullptr){
                if(activeWidget->isEnabled()){

                    std::shared_ptr<LAbstractMenu> activeMenu = std::dynamic_pointer_cast<LAbstractMenu>(activeWidget);
                    LPoint<int> subMenuPos = adjustedSubPosition(action);

                    if(activeMenu != nullptr){
                        std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();
                        activeMenu->setMenuCause(tThis, action);
                        activeMenu->popup(subMenuPos);

                        if(activateFirst){
                            activeMenu->setFirstActionActive();
                        }
                    }else{
                        activeWidget->move(subMenuPos);
                        activeWidget->show();
                    }
                }
            }
        }
    }
}

void LMenuBar::drawMenuItem(std::shared_ptr<LActionBase> a, LPainter *p, const LStyleOpt &opt, std::shared_ptr<LStyle> pStyle)
{
    std::shared_ptr<LRenderRule> rule;
    if(opt.elementStyle != nullptr){
        rule = opt.elementStyle->getRenderRule(LUiElementType::Item, a->uiState());
    }

    LRect<double> rect = opt.rect.toRectF();
    bool hasColor = false;
    if(rule != nullptr){
        if (rule->hasDrawable()) {
            rule->drawRule(p, opt.rect);
        }
        hasColor = rule->applyForeground(p) & 0x01;
    }else{
        if (a->isSelected()) {
            // Windows 11 风格：圆角浅灰胶囊高亮，保持深色文字
            p->save();
            p->setRenderHints(LRenderHint::Antialiasing, true);
            p->setBrush(opt.palette->brush(LPalette::Midlight));
            p->setPen(LPenStyle::No);
            p->drawRoundedRect(rect.adjusted(1, 2, -1, -2), 4, 4, LSizeMode::Absolute);
            p->restore();
        } else {
            std::shared_ptr<LBrush> fill = opt.palette->brush(opt.backgroundRole);
            p->fillRect(rect, fill);
        }
    }

    LColor textColor;
    if(!hasColor){
        if (a->isEnabled()) {
            textColor = opt.palette->buttonText()->m_color;
        } else {
            textColor = opt.palette->brush(LPalette::Disabled, LPalette::Text)->m_color;
        }
        p->setPen(textColor);
    }


    const int itemMargin = pStyle->get(&LStylePixelMetric::menuBarItemMargin, &m_elementStyle);
    rect.m_x += itemMargin;
    rect.m_width-= 2*itemMargin;
    const std::wstring& s = a->text();
    const std::shared_ptr<LTexture> icon = a->icon();
    // If an icon is set, only the icon is visible
    if (icon != nullptr){
        p->drawPixmap(rect,
                      icon->getPixmap(a->isEnabled()? LTexture::Normal: LTexture::Disable),
                      icon->rect().toRectF());
    }else if (!s.empty()){
        p->drawText(rect, s);
    }
}

LPoint<int> LMenuBar::adjustedSubPosition(std::shared_ptr<LActionBase> causeAction)
{
    const LRect<int> rect = actionRect(causeAction);
    LPoint<int> globalPos = mapToGlobal(rect.topLeft());
    std::shared_ptr<LWidget> subWidget = causeAction->widget();
    if(subWidget == nullptr){
        return globalPos;
    }

    int x = globalPos.m_x;
    int y = globalPos.m_y;

    LSize<int> menuSize = subWidget->sizeHint();

    std::shared_ptr<LWidget> q =getPtrT<LWidget>();
    LRect<int> availableGeometry = popupGeometry(LGuiApplication::screenAt(pos()));

    if (globalPos.m_y + rect.height() + menuSize.height() <= availableGeometry.y2()) {
        y += rect.height();
    } else if (globalPos.m_y - menuSize.height() >= availableGeometry.m_y) {
        y -= menuSize.height();
    }
    if (layoutDirection() == LLayoutDirection::RightToLeft){
        x += rect.width() - menuSize.width();
    }
    if(x <availableGeometry.m_x){
        x = availableGeometry.m_x;
    }
    else if(x + menuSize.width() > availableGeometry.x2()){
        x = availableGeometry.x2() - menuSize.width();
    }
    return LPoint(x,y);
}


LK_NAMESPACE_END
