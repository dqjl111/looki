#include "lmenu.h"
#include "lkcore/leventloop.h"
#include "lkgui/lguiapplication.h"
#include <iterator>
#include "lkwidgets/core/ldesktopwidget.h"
#include "lkwidgets/draw/lstyle.h"
#include "lkwidgets/core/lapplication.h"
#include "lkcore/ldebug.h"

LK_NAMESPACE_BEGIN

LMenu::LMenu(std::shared_ptr<LWidget> parent)  : LAbstractMenu(parent, LWindowType::Popup)
{
    m_sizePolicy = (LSizePolicy(LSizePolicy::Preferred, LSizePolicy::Preferred, LSizePolicy::Menu));
}

std::shared_ptr<LActionBase> LMenu::exec(const std::list<std::shared_ptr<LActionBase> > &actions,
                                     const LPoint<int> &pos,
                                     std::shared_ptr<LActionBase> at,
                                     std::shared_ptr<LWidget> parent)
{
    std::shared_ptr<LAbstractMenu> menu = makeInit<LMenu>(parent);
    menu->addActions(actions);
    return menu->exec(pos, at);
}


void LMenu::updateActionRects(const LRect<int> &container)
{
    if (!m_itemsDirty){
        return;
    }

    //let's reinitialize the buffer
    m_actionRects.resize(m_actions.size(), LRect<int>());
    std::shared_ptr<LStyle> pStyle = style();
    bool scrollAble = m_scrollable;

    const uint16_t icone = pStyle->get(&LStylePixelMetric::smallIconSize, &m_elementStyle);
    const uint16_t deskFw = pStyle->get(&LStylePixelMetric::menuDesktopFrameWidth, &m_elementStyle);
    const LMargins<int> bounds = boundWidths();

    int baseY = bounds.m_top + (scrollAble? m_scrollOffset : 0);
    int columnMaxY = container.height();
    if(scrollAble){
        columnMaxY = std::min<int>(maximumHeight(), columnMaxY);
    }
    columnMaxY -= (2 * deskFw + bounds.m_bottom);

    int maxColumnWidth = 0;
    int y = baseY;
    bool needScroll = false;


    m_tabWidth = 0;
    m_maxIconWidth = 0;
    m_hasCheckableItems = false;
    m_hasPopupItems = false;
    int ncols = 1;

    for (const std::shared_ptr<LActionBase>& action : m_actions) {
        if (action->isSeparator() || !action->isVisible() || (action->hasWidget()&& (!action->isPopup()) )){
            continue;
        }
        //..and some members
        m_hasCheckableItems |= action->isCheckable();
        m_hasPopupItems |= action->isPopup();

        if(m_maxIconWidth == 0){
            std::shared_ptr<LTexture> is = action->icon();
            if ((is != nullptr) && (!is->isEmpty())) {
                m_maxIconWidth = icone + 4;
            }
        }
    }

    int addWidth = pStyle->get(&LStylePixelMetric::checkItemAddWidth, &m_elementStyle) + 2*pStyle->get(&LStylePixelMetric::menuItemAddWidth, &m_elementStyle);
    if (m_maxIconWidth > 0){
        addWidth += std::max<uint16_t>(m_maxIconWidth, pStyle->get(&LStylePixelMetric::itemMarkSpan, &m_elementStyle)) +6;
    }else{
        if (m_hasCheckableItems){
            addWidth += pStyle->get(&LStylePixelMetric::checkItemAddWidth, &m_elementStyle) +6;
        }
    }
    if(m_hasPopupItems){
        addWidth += pStyle->get(&LStylePixelMetric::itemMarkSpan, &m_elementStyle) + 6;
    }
    //calculate size
    const LFontMetrics qfm = fontMetrics();
    bool previousWasSeparator = true; // this is true to allow removing the leading separators
    int i=0;
    for (const std::shared_ptr<LActionBase>& action : m_actions) {
        if (!action->isVisible()){
            ++i;
            continue;
        }

        const bool isSection = action->isSeparator() && (!action->isEmpty());
        const bool isPlainSeparator = action->isSeparator() && !isSection;
        if (m_collapsibleSeparators && previousWasSeparator && isPlainSeparator){
            ++i;
            continue;
        }

        previousWasSeparator = isPlainSeparator;

        LSize<int> sz;
        std::shared_ptr<LWidget > w = action->widget();
        if (w != nullptr && (!action->isPopup())) {
            sz = w->sizeHint().expandedTo(w->minimumSize()).expandedTo(w->minimumSizeHint()).boundedTo(w->maximumSize());
        } else {
            //calc what I think the size is..
            if (action->isSeparator()) {
                sz = LSize<int>(10, 2);
            } else {
                std::wstring s = action->text();
                int t = (int)s.find(wchar_t('\t'));
                if (t >=0) { //通过"\t"分隔快捷键
                    m_tabWidth = std::max<int>(int(m_tabWidth), std::lround(qfm.horizontalAdvance(s.substr(t+1))));
                    s = s.substr(0, t);
                    sz.m_x += 13;
                } else if (m_isShortcutVisible) { //无分隔快捷键
                    LKeySequence seq = action->shortcut();
                    if (seq.isValid()){
                        m_tabWidth = std::max<int>(int(m_tabWidth), std::lround(qfm.horizontalAdvance(seq.toString())));
                        sz.m_x += 13;
                    }
                }
                sz.m_x += std::lround(qfm.boundingRect(LRect<int>(),
                                                       (uint32_t)LTextFlag::SingleLine | (uint32_t)LTextFlag::ShowMnemonic,
                                                       s).width());
                sz.m_y = std::lround(qfm.height() + 8);

                std::shared_ptr<LTexture> is = action->icon();
                if (is != nullptr) {
                    if(!is->isEmpty()){
                        sz.m_y = std::max<int>(sz.height(), is->actualSize(LSize<int>(icone, icone)).height()+4);
                    }
                }
            }
            sz.m_x += addWidth;
        }


        if (!sz.isEmpty()) {
            maxColumnWidth = std::max<int>(maxColumnWidth, sz.width());
            if(scrollAble){
                y += sz.height();
                needScroll = (y + sz.height()) > columnMaxY;
            }else{
                //wrapping
                if((y + sz.height()) > columnMaxY){
                    ncols++;
                    y = baseY;
                }
            }
            //update the item
            m_actionRects[i] = LRect<int>(0, 0, sz.width(), sz.height(), false);
        }
        ++i;
    }

    maxColumnWidth += m_tabWidth; //finally add in the tab width
    const int minColumnWidth = minimumWidth() - bounds.m_left - bounds.m_right;
    maxColumnWidth = std::max<int>(minColumnWidth, maxColumnWidth);


    //calculate position
    int x = bounds.m_left;
    if(needScroll && (m_useScrollBar == false)){
        m_scrollFlags |= ScrollDown;
        baseY += scrollerSize();
    }
    y = baseY;
    i=0;
    int maxViewY =0;
    const uint16_t hPadding = boundWidth(LDirection::ToLeft, LRenderRule::Padding);
    for (const std::shared_ptr<LActionBase>& action : m_actions) {
        LRect<int> &rect = m_actionRects[i];
        if (rect.isNull()){
            ++i;
            continue;
        }

        if((scrollAble == false) && ((y + rect.height()) > columnMaxY)){
            x += maxColumnWidth + hPadding;
            y = baseY;
        }
        rect.translate(x, y);                        //move
        rect.setWidth(maxColumnWidth); //uniform width
        maxViewY = std::max<int>(maxViewY, rect.m_y+rect.m_height-baseY);

        //we need to update the widgets geometry
        std::shared_ptr<LWidget > w = action->widget();
        if (w != nullptr && (!action->isPopup())) {
            w->setGeometry(rect);
            w->setVisible(action->isVisible());
        }

        y += rect.height();
        ++i;
    }
    m_scrollOffset = 0;
    m_isReachBound = ncols >1;
    m_viewSize = maxViewY;
    updateSliderData();
    m_itemsDirty = false;
}


void LMenu::popupAction(std::shared_ptr<LActionBase> action, bool activateFirst)
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
                    LRect<int> screen = popupGeometry(LGuiApplication::screenAt(pos()));
                    std::shared_ptr<LStyle> pStyle = style();

                    int subMenuOffset = pStyle->get(&LStylePixelMetric::subMenuOverlap, &m_elementStyle);
                    const LRect<int> actRect = actionRect(action);
                    LPoint<int> subMenuPos(mapToGlobal(LPoint<int>(actRect.x2() - subMenuOffset + 1, actRect.m_y)));
                    if (subMenuPos.m_x >= screen.x2()){
                        subMenuPos.m_x = (LCursor::pos().m_x);
                    }

                    if(activeMenu != nullptr){
                        std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();                        
                        activeMenu->setMenuCause(tThis, action);

                        const std::list<std::shared_ptr<LActionBase> >& acts = activeMenu->actions();
                        if (!acts.empty()) {
                            // Offset by the submenu's 1st action position to align with the current action
                            const LRect<int> subMenuActionRect = activeMenu->actionRect(acts.front());
                            subMenuPos.m_y -= subMenuActionRect.m_y;
                        }

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

void LMenu::drawMenuItem(std::shared_ptr<LActionBase> a, LPainter *p, const LStyleOpt &opt, std::shared_ptr<LStyle> pStyle)
{
    std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();
    pStyle->drawMenuItem(tThis, a, p, opt, m_maxIconWidth, m_tabWidth);
}


LK_NAMESPACE_END
