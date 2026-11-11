#include "labstractmenu.h"
#include "lkwidgets/draw/lstyle.h"
#include "lkwidgets/core/ldesktopwidget.h"
#include "lkwidgets/core/lapplication.h"
#include "lkwidgets/widget/lframe.h"

LK_NAMESPACE_BEGIN

std::weak_ptr<LAbstractMenu> LAbstractMenu::m_mouseDown;

LAbstractMenu::LAbstractMenu(std::shared_ptr<LWidget> parent, LFlags<LWindowType> f)   : LWidget(parent, f)
{
    m_collapsibleSeparators = true;
    m_isShortcutVisible = true;
    m_itemsDirty = true;
}

void LAbstractMenu::initSpecific()
{
    setMouseTracking(true);
}

LSize<int> LAbstractMenu::rawSizeHint() const
{
    const_cast<LAbstractMenu*>(this)->updateActionRects();

    LSize<int> s;
    for(const LRect<int>& rect : m_actionRects) {
        if (rect.isNull()){
            continue;
        }
        if (rect.y2() > s.m_y){
            s.setHeight(rect.y2());
        }
        if (rect.x2() > s.m_x){
            s.setWidth(rect.x2());
        }
    }

    s.m_x += boundWidth(LDirection::ToRight);
    s.m_y += boundWidth(LDirection::ToBottom);
    return s;
}

uint16_t LAbstractMenu::uiElementState(LUiElementType type) const
{
    uint16_t ret = 0;
    if(type == LUiElementType::UpButton){
        if(m_scrollFlags& ScrollUp){
            if(m_pressedCtrl.test((uint8_t)LUiSubId::ScrollBar::SubLine)){
                ret = uint16_t(LUiState::Sunken);
            }else if(m_hoverCtrl.test((uint8_t)LUiSubId::ScrollBar::SubLine)){
                ret = uint16_t(LUiState::Hover);
            }else{
                ret=  uint16_t(LUiState::Enabled);
            }
        }else{
            ret= uint16_t(LUiState::Disabled);
        }
    }else if(type == LUiElementType::DownButton){
        if(m_scrollFlags& ScrollDown){
            if(m_pressedCtrl.test((uint8_t)LUiSubId::ScrollBar::AddLine)){
                ret = uint16_t(LUiState::Sunken);
            }else if(m_hoverCtrl.test((uint8_t)LUiSubId::ScrollBar::AddLine)){
                ret = uint16_t(LUiState::Hover);
            }else{
                ret = uint16_t(LUiState::Enabled);
            }
        }else{
            ret= uint16_t(LUiState::Disabled);
        }
    }else if(type == LUiElementType::Handle){
        if(m_pressedCtrl.test((uint8_t)LUiSubId::ScrollBar::Slider)){
            ret = uint16_t(LUiState::Sunken);
        }else if(m_hoverCtrl.test((uint8_t)LUiSubId::ScrollBar::Slider)){
            ret = uint16_t(LUiState::Hover);
        }else{
            ret = uint16_t(LUiState::Enabled);
        }
    }else{
        ret = LWidget::uiElementState(type);
    }
    return ret;
}

LSize<int> LAbstractMenu::sizeHint() const
{
    LSize<int> s = rawSizeHint();
    if(m_useScrollBar){
        std::shared_ptr<LStyle> pStyle = style();
        int scrollBarExtent = pStyle->get(&LStylePixelMetric::scrollBarExtent, &m_elementStyle);
        int scrollBarSliderMin = std::lround(dpiScaled(pStyle->get(&LStylePixelMetric::scrollBarSliderMin, &m_elementStyle)));

        if(m_scrollHorizon){
            s.m_x = std::max<int>(s.m_x, scrollBarExtent * 2 + scrollBarSliderMin);
            s.m_y += scrollBarExtent;
        }else{
            s.m_y = std::max<int>(s.m_y, scrollBarExtent * 2 + scrollBarSliderMin);
            s.m_x += scrollBarExtent;
        }
    }
    s.m_y = std::min<int>(maximumHeight(), s.m_y);
    s.m_x = std::min<int>(maximumWidth(), s.m_x);
    return s;
}

void LAbstractMenu::mouseReleaseEvent(std::shared_ptr<LMouseEvent> e)
{
    if(!isVisible() || mouseEventTaken(e)){
        e->accept();
        return;
    }

    if (m_mouseDown.lock() != getPtrT<LAbstractMenu>()){
        m_mouseDown.reset();
        e->accept();
        return;
    }

    m_mouseDown.reset();
    if(m_scrollable){
        if(m_scrollFlags &(ScrollUp|ScrollDown)){
            if(m_pressedCtrl.test((uint8_t)LUiSubId::ScrollBar::SubLine)
                || m_pressedCtrl.test((uint8_t)LUiSubId::ScrollBar::AddLine)
                || m_pressedCtrl.test((uint8_t)LUiSubId::ScrollBar::Slider)){
                stopRepeatAction();
                e->accept();
                return;
            }
            stopRepeatAction();
        }
    }


    setSyncAction();
    bool isAtClose = false;
    std::shared_ptr<LActionBase> action = actionAt(e->pos(), &isAtClose);

    if(action != nullptr && action == m_currentAction){
        if (!action->isPopup()){
            if(isAtClose){
                closeAction(action, LAction::Trigger);
            }else{
                activateAction(action, LAction::Trigger);
            }
        }
    }else{
        hideUpToMenuBar();
    }
    e->accept();
}

void LAbstractMenu::mousePressEvent(std::shared_ptr<LMouseEvent> e)
{
    if(!isVisible() || mouseEventTaken(e)){
        e->accept();
        return;
    }

    LPoint<int> pos = e->pos();
    if ((pos.isNull() && !e->screenPos().isNull()) || !rect().contains(pos)) {
        if (m_eventLoop != nullptr){ // synchronous operation
            m_syncAction = nullptr;
        }
        hideUpToMenuBar();
        e->accept();
        return;
    }
    m_mouseDown = getPtrT<LAbstractMenu>();

    if(m_scrollable){
        if(m_scrollFlags &(ScrollUp|ScrollDown)){
            std::shared_ptr<LStyle> pStyle = style();
            if(m_useScrollBar){
                if(isSliderDataValid()){
                    LStyleOptComplex opt;
                    initStyleOpt(opt);
                    opt.rect = scrollBarRect(pStyle);

                    LRect<int> sr = pStyle->scrollSubCtrlRect(*m_sliderData,
                                                              opt,
                                                              1<<(uint8_t)LUiSubId::ScrollBar::Slider,
                                                              -1,
                                                              -1,
                                                              -1,
                                                              -1);

                    m_sliderData->m_value = m_scrollHorizon ? (pos.m_x-sr.m_x) : (pos.m_y-sr.m_y);
                    m_pressedCtrl = pStyle->hitTestScroll(*m_sliderData, opt, pos);
                    activateControl(m_pressedCtrl);
                    if (m_pressedCtrl.test((uint8_t)LUiSubId::ScrollBar::Slider)){
                        setSliderDown(true);
                    }
                }
            }else{
                if(prevScrollerRect(pStyle).contains(pos)){
                    m_pressedCtrl = 1<<(uint8_t)LUiSubId::ScrollBar::SubLine;
                    activateControl(m_pressedCtrl);
                    e->accept();
                    return;
                }else{
                    if(nextScrollerRect(pStyle).contains(pos)){
                        m_pressedCtrl = 1<<(uint8_t)LUiSubId::ScrollBar::AddLine;
                        activateControl(m_pressedCtrl);
                        e->accept();
                        return;
                    }else{
                        m_pressedCtrl.reset();
                    }
                }
            }
        }
    }

    bool isAtClose = false;
    std::shared_ptr<LActionBase> action = actionAt(e->pos(), &isAtClose);
    if(action != nullptr){
        if(isAtClose){
            closeAction(action, LAction::Pressed);
        }else{
            m_hasHadMouseDown = true;
            setCurrentAction(action, action->isPopup());
        }
        update();
    }
    e->accept();
}

void LAbstractMenu::mouseMoveEvent(std::shared_ptr<LMouseEvent> e)
{
    if (!isVisible()){
        e->accept();
        return;
    }

    LPoint<int> pos = e->pos();

    if(m_scrollable){
        if(m_scrollFlags &(ScrollUp|ScrollDown)){
            if(m_useScrollBar){
                if(m_pressedCtrl.test((uint8_t)LUiSubId::ScrollBar::Slider)){
                    std::shared_ptr<LStyle> pStyle = style();
                    LStyleOptComplex opt;
                    initStyleOpt(opt);
                    opt.rect = scrollBarRect(pStyle);
                    int newPosition = pixelPosToRangeValue((m_scrollHorizon? pos.m_x : pos.m_y) -m_sliderData->m_value, opt, pStyle);
                    scrollMenu(ScrollNone, -newPosition);
                    e->accept();
                    return;
                }
            }
        }
    }

    if(mouseEventTaken(e)){
        e->accept();
        return;
    }

    if(m_scrollable){
        if(m_scrollFlags &(ScrollUp|ScrollDown)){
            std::shared_ptr<LStyle> pStyle = style();
            if(m_useScrollBar){
                LStyleOptComplex opt;
                initStyleOpt(opt);
                opt.rect = scrollBarRect(pStyle);
                LUiSubCtrl hoverCtrl = pStyle->hitTestScroll(*m_sliderData, opt, e->pos());

                if(m_pressedCtrl.test((uint8_t)LUiSubId::ScrollBar::SubLine)){
                    if(!hoverCtrl.test((uint8_t)LUiSubId::ScrollBar::SubLine)){
                        setRepeatAction(LAbstractSlider::SliderNoAction);
                    }
                }else if(m_pressedCtrl.test((uint8_t)LUiSubId::ScrollBar::AddLine)){
                    if(!hoverCtrl.test((uint8_t)LUiSubId::ScrollBar::AddLine)){
                        setRepeatAction(LAbstractSlider::SliderNoAction);
                    }
                }else{
                    if(hoverCtrl.test((uint8_t)LUiSubId::ScrollBar::SubLine)){
                        m_hoverCtrl = 1<<(uint8_t)LUiSubId::ScrollBar::SubLine;
                    }else if(hoverCtrl.test((uint8_t)LUiSubId::ScrollBar::AddLine)){
                        m_hoverCtrl = 1<<(uint8_t)LUiSubId::ScrollBar::AddLine;
                    }else if(hoverCtrl.test((uint8_t)LUiSubId::ScrollBar::Slider)){
                        m_hoverCtrl = 1<<(uint8_t)LUiSubId::ScrollBar::Slider;
                    }else{
                        m_hoverCtrl.reset();
                    }
                }
            }else{
                LRect<int> prevRect = prevScrollerRect(pStyle);
                LRect<int> nextRect = nextScrollerRect(pStyle);

                if(m_pressedCtrl.test((uint8_t)LUiSubId::ScrollBar::SubLine)){
                    if(!prevRect.contains(pos)){
                        setRepeatAction(LAbstractSlider::SliderNoAction);
                    }
                }else if(m_pressedCtrl.test((uint8_t)LUiSubId::ScrollBar::AddLine)){
                    if(!nextRect.contains(pos)){
                        setRepeatAction(LAbstractSlider::SliderNoAction);
                    }
                }else{
                    if(prevRect.contains(pos)){
                        m_hoverCtrl = 1<<(uint8_t)LUiSubId::ScrollBar::SubLine;
                    }else if(nextRect.contains(pos)){
                        m_hoverCtrl = 1<<(uint8_t)LUiSubId::ScrollBar::AddLine;
                    }else{
                        m_hoverCtrl.reset();
                    }
                }
            }
        }
    }


    bool isAtClose = false;
    std::shared_ptr<LActionBase> action = actionAt(pos, &isAtClose);
    if(isAtClose){
        closeAction(action, LAction::Hover);
    }else{
        if(action != m_currentAction){
            if (action==nullptr || action->isSeparator()) {
                e->accept();
                return;
            }

            if((m_widgetData.windowFlags & LWindowType::Popup) == LWindowType::Popup){
                setCurrentAction(action, action->isPopup());
            }else{
                if(m_hasHadMouseDown){
                    setCurrentAction(action, action->isPopup());
                }else{
                    setCurrentAction(action, false);
                }
            }
        }
    }

    if(m_hasHadMouseDown){
        if(action != nullptr){
            if (e->buttons()){
                m_mouseDown = getPtrT<LAbstractMenu>();
            }
        }
    }
    e->accept();

}

void LAbstractMenu::hideEvent(std::shared_ptr<LEvent> e)
{
    if (m_eventLoop != nullptr){
        m_eventLoop->quit();
    }
    setCurrentAction(nullptr);
    if (m_mouseDown.lock() == getPtrT<LAbstractMenu>()){
        m_mouseDown.reset();
    }

    m_hasHadMouseDown = false;
    m_causedPopup.widget.reset();
    m_causedPopup.action.reset();
    e->accept();
}

void LAbstractMenu::wheelEvent(std::shared_ptr<LWheelEvent> e)
{
    if (m_scrollable && rect().contains(e->position().toPoint())){
        scrollMenu(e->angleDelta().m_y > 0 ? LAbstractMenu::ScrollUp :LAbstractMenu::ScrollDown);
    }
}

void LAbstractMenu::resizeEvent(std::shared_ptr<LResizeEvent> event)
{
    updateSliderRange();
}


void LAbstractMenu::paintEvent(std::shared_ptr<LPaintEvent> e)
{
    LAbstractMenu::updateActionRects();
    LPainter p(paintDevice());
    std::shared_ptr<LStyle> pStyle = style();
    std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();


    LStyleOptComplex opt;
    initStyleOpt(opt);

    bool scrollAble = m_scrollable;
    uint16_t state = uiElementState(LUiElementType::None);
    std::shared_ptr<LRenderRule> rule = m_elementStyle.getRenderRule(LUiElementType::None, state);
    if(rule != nullptr && rule->hasBackground()){
        rule->drawBackground(&p, rect());
    }else{
        // 用 fillRect 铺满整个控件：drawRect 会带 1px 黑色描边（旧式黑边来源）
        // 弹出菜单使用更亮的底色，菜单栏沿用窗口底色
        const bool isPopup = (m_widgetData.windowFlags & LWindowType::Popup) == LWindowType::Popup;
        p.fillRect(rect().toRectF(),
                   opt.palette->brush(isPopup ? LPalette::Base : backgroundRole()));
    }


    int scrollerPrev = 0, scrollerNext = 0, scrollerS = 0;
    if (scrollAble) {
        if(m_scrollFlags & (ScrollUp|ScrollDown)){
            scrollerS = scrollerSize();
        }
    }

    if(m_scrollHorizon){
        scrollerPrev = scrollerS + boundWidth(LDirection::ToLeft);
        scrollerNext = width() - scrollerS - boundWidth(LDirection::ToRight);
    }else{
        scrollerPrev = scrollerS + boundWidth(LDirection::ToTop);
        scrollerNext = height() - scrollerS - boundWidth(LDirection::ToBottom);
    }
    //draw the items that need updating..
    int i=0;
    LRect<int> eventRect = e->rect();
    for (std::shared_ptr<LActionBase> action : m_actions) {
        LRect<int> actionRect = m_actionRects.at(i);
        if (!eventRect.isIntersects(actionRect)
            || (action->hasWidget() && (!action->isPopup()))){
            ++i;
            continue;
        }


        LRect<int> adjustedActionRect = actionRect;
        if(m_scrollHorizon){
            if((adjustedActionRect.x2() <= scrollerPrev) || (adjustedActionRect.m_x >=  scrollerNext)){
                ++i;
                continue;
            }

            if(adjustedActionRect.m_x < scrollerPrev){
                adjustedActionRect.m_width -= (scrollerPrev - adjustedActionRect.m_x);
                adjustedActionRect.m_x = scrollerPrev;

            }

            if(adjustedActionRect.x2() > scrollerNext){
                adjustedActionRect.m_width -= (adjustedActionRect.x2()-scrollerNext);
            }
        }else{
            if((adjustedActionRect.y2() <= scrollerPrev) || (adjustedActionRect.m_y >=  scrollerNext)){
                ++i;
                continue;
            }

            if(adjustedActionRect.m_y < scrollerPrev){
                adjustedActionRect.m_height -= (scrollerPrev - adjustedActionRect.m_y);
                adjustedActionRect.m_y = scrollerPrev;
            }

            if(adjustedActionRect.y2() > scrollerNext){
                adjustedActionRect.m_height -= (adjustedActionRect.y2()-scrollerNext);
            }
        }
        p.setClipRect(adjustedActionRect);

        action->setSelected(action == m_currentAction);
        opt.rect = actionRect;
        drawMenuItem(action, &p, opt, pStyle);
        ++i;
    }


    if (scrollAble) {
        if(m_scrollFlags & (ScrollUp|ScrollDown)){
            if(m_useScrollBar){
                opt.rect = scrollBarRect(pStyle);
                drawScrollBar(&p, opt);
            }else{
                opt.rect = prevScrollerRect(pStyle);
                drawScroller(&p, opt, ScrollUp);
                opt.rect = nextScrollerRect(pStyle);
                drawScroller(&p, opt, ScrollDown);
            }
        }
    }

    const LMargins<int> fw = boundWidths(LRenderRule::Border);
    if (!fw.isNull()) {
        const LMargins<int> bounds = boundWidths();
        std::shared_ptr<LRegion> borderReg = std::make_shared<LRegion>();
        borderReg->unite(LRect<int>(0, 0, bounds.m_left, height(), false)); //left
        borderReg->unite(LRect<int>(width()-bounds.m_right, 0, bounds.m_right, height(), false)); //right
        borderReg->unite(LRect<int>(0, 0, width(), bounds.m_top, false)); //top
        borderReg->unite(LRect<int>(0, height()-bounds.m_bottom, width(), bounds.m_bottom, false)); //bottom
        p.setClipRegion(borderReg);

        // 使用调色板的边框灰，而不是文字色（否则会出现 1px 黑边）
        pStyle->drawFrame(LRect<int>(0, 0, width(), height()),
                          &p, opt.palette, LPalette::Mid,
                          LFrameData::Panel, LFrameData::Plain, fw.m_left, 0);
    } else if ((m_widgetData.windowFlags & LWindowType::Popup) == LWindowType::Popup) {
        // Windows 11 风格弹出菜单：默认 1px 描边，避免菜单与背景混在一起
        p.setClipRect(rect());
        p.setBrush(LBrush::NoPattern);
        p.setPen(opt.palette->brush(LPalette::Mid)->m_color);
        p.drawRect(LRect<int>(0, 0, width() - 1, height() - 1, false));
    }
}

void LAbstractMenu::leaveEvent(std::shared_ptr<LEvent> event)
{
    if((m_widgetData.windowFlags & LWindowType::Popup) != LWindowType::Popup){
        if((m_hasHadMouseDown == false) && (m_currentAction != nullptr)){
            setCurrentAction(nullptr, false);
            event->accept();
        }
    }
}

void LAbstractMenu::setScrollable(bool isScrollable)
{
    if(m_scrollable != isScrollable){
        m_scrollable = isScrollable;
        if(m_scrollable == false){
            m_useScrollBar = false;
        }
        m_itemsDirty = true;
        update();
    }
}

void LAbstractMenu::setUseScrollBar(bool useScrollBar)
{
    if(m_useScrollBar != useScrollBar){
        m_useScrollBar = useScrollBar;
        if(m_useScrollBar){
            m_scrollable = true;
        }else{
            m_sliderData.reset();
        }
        m_itemsDirty = true;
        update();
    }
}


void LAbstractMenu::updateActionRects()
{
    if((m_widgetData.windowFlags & LWindowType::Popup) == LWindowType::Popup){
        updateActionRects(popupGeometry());
    }else{
        updateActionRects(geometry());
    }
}

int LAbstractMenu::scrollerSize() const
{
    if(m_useScrollBar){
        return 0;
    }
    std::shared_ptr<LStyle> pStyle = style();
    if(m_scrollHorizon){
        return std::lround(dpiScaled(pStyle->get(&LStylePixelMetric::menuScrollerWidth, &m_elementStyle)));
    }else{
        return std::lround(dpiScaled(pStyle->get(&LStylePixelMetric::menuScrollerHeight, &m_elementStyle)));
    }
}

LRect<int> LAbstractMenu::prevScrollerRect(std::shared_ptr<LStyle> pStyle) const
{
    if(m_useScrollBar){
        return LRect<int>();
    }


    const LMargins<int> margins = boundWidths(LRenderRule::CodeMargin);
    if(m_scrollHorizon){
        int scrollerWidth = std::lround(dpiScaled(pStyle->get(&LStylePixelMetric::menuScrollerWidth, &m_elementStyle)))
                            +boundWidth(LDirection::ToLeft, LRenderRule::StyleAll);
        return LRect<int>(margins.m_left, margins.m_top, scrollerWidth, height()-margins.m_top-margins.m_bottom, false);
    }else{
        int scrollerHeight = std::lround(dpiScaled(pStyle->get(&LStylePixelMetric::menuScrollerHeight, &m_elementStyle)))
                            +boundWidth(LDirection::ToTop, LRenderRule::StyleAll);
        return LRect<int>(margins.m_left, margins.m_top, width()-margins.m_left-margins.m_right, scrollerHeight, false);
    }
}

LRect<int> LAbstractMenu::nextScrollerRect(std::shared_ptr<LStyle> pStyle) const
{
    if(m_useScrollBar){
        return LRect<int>();
    }

    const LMargins<int> margins = boundWidths(LRenderRule::CodeMargin);
    if(m_scrollHorizon){
        int scrollerWidth = std::lround(dpiScaled(pStyle->get(&LStylePixelMetric::menuScrollerWidth, &m_elementStyle)))
                            +boundWidth(LDirection::ToRight, LRenderRule::StyleAll);
        return LRect<int>(width()-scrollerWidth-margins.m_right,
                          margins.m_top,
                          scrollerWidth,
                          height()-margins.m_top-margins.m_bottom, false);
    }else{
        int scrollerHeight = std::lround(dpiScaled(pStyle->get(&LStylePixelMetric::menuScrollerHeight, &m_elementStyle)))
                            +boundWidth(LDirection::ToBottom, LRenderRule::StyleAll);
        return LRect<int>(margins.m_left,
                          height() - scrollerHeight - margins.m_bottom,
                          width()-margins.m_left-margins.m_right,
                          scrollerHeight, false);
    }
}


LRect<int> LAbstractMenu::scrollBarRect(std::shared_ptr<LStyle> pStyle) const
{
    if(!m_useScrollBar){
        return LRect<int>();
    }

    const LMargins<int> margin = boundWidths(LRenderRule::CodeMargin);
    const uint16_t scrollBarExtent = pStyle->get(&LStylePixelMetric::scrollBarExtent, &m_elementStyle);
    if(m_scrollHorizon){
        return LRect<int>(margin.m_left,
                          height()-margin.m_bottom-scrollBarExtent,
                          width()-margin.m_left-margin.m_right,
                          scrollBarExtent, false);
    }else{
        return LRect<int>(width()-margin.m_right-scrollBarExtent,
                          margin.m_top,
                          scrollBarExtent,
                          height()-margin.m_top-margin.m_bottom, false);
    }
}

LRect<int> LAbstractMenu::actionCloseRect(std::shared_ptr<LActionBase> a, const LRect<int>& actRect) const{
    return LRect<int>();
}

void LAbstractMenu::updateSliderData()
{
    if(m_scrollable){
        if(m_useScrollBar){
            bool isDirty = m_itemsDirty;
            if(m_sliderData == nullptr){
                m_sliderData = std::make_unique<LSliderData>();
                m_sliderData->m_orientation = m_scrollHorizon ? LOrientation::Horizontal : LOrientation::Vertical;
                isDirty = true;
            }

            if(isDirty){
                m_sliderData->m_singleStep = 0;
                for(const LRect<int>& rect : m_actionRects){
                    if(m_scrollHorizon){
                        if(rect.m_width>0){
                            if(m_sliderData->m_singleStep == 0){
                                m_sliderData->m_singleStep = rect.m_width;
                            }
                            m_sliderData->m_singleStep = std::min<int>(m_sliderData->m_singleStep, rect.m_width);
                        }
                    }else{
                        if(rect.m_height>0){
                            if(m_sliderData->m_singleStep == 0){
                                m_sliderData->m_singleStep = rect.m_height;
                            }
                            m_sliderData->m_singleStep = std::min<int>(m_sliderData->m_singleStep, rect.m_height);
                        }
                    }
                }
                updateSliderRange();
            }

            m_sliderData->m_position = -m_scrollOffset;
        }else{
            m_sliderData.reset();
        }
    }
}

void LAbstractMenu::updateSliderRange()
{
    if(m_scrollable){
        if(m_actionRects.empty()){
            return;
        }

        int scrollerArea = 0;
        int scrollerS  = scrollerSize();
        if(m_scrollHorizon){
            scrollerArea = width() - boundWidth(LDirection::ToLeft) - boundWidth(LDirection::ToRight);
        }else{
            scrollerArea = height() - boundWidth(LDirection::ToTop) - boundWidth(LDirection::ToBottom);
        }

        bool needScroller = (m_viewSize > scrollerArea);
        if(m_viewSize > scrollerArea){
            scrollerArea -= 2*scrollerS;
        }

        uint32_t newScrollFlags = ScrollNone;
        if(m_scrollFlags){
            if(needScroller){
                if (m_scrollOffset < 0){ //easy and cheap one
                    newScrollFlags |= ScrollUp;
                }

                if(m_viewSize + m_scrollOffset> scrollerArea){
                    newScrollFlags |= ScrollDown;
                }

            }else{
                const int delta = -scrollerS-m_scrollOffset;
                //调整位置
                int i=0;
                for(const std::shared_ptr<LActionBase>& a : m_actions){
                    LRect<int> &current = m_actionRects[i];
                    if(m_scrollHorizon){
                        current.moveLeft(current.m_x + delta);
                    }else{
                        current.moveTop(current.m_y + delta);
                    }
                    //we need to update the widgets geometry
                    std::shared_ptr<LWidget> w = a->widget();
                    if (w != nullptr){
                        w->setGeometry(current);
                    }
                    ++i;
                }
                m_scrollOffset = 0;
            }
        }else{
            if(needScroller){
                m_scrollOffset = 0;
                //调整位置
                const int delta = scrollerS;
                int i=0;
                for(const std::shared_ptr<LActionBase>& a : m_actions){
                    LRect<int> &current = m_actionRects[i];
                    if(m_scrollHorizon){
                        current.moveLeft(current.m_x + delta);
                    }else{
                        current.moveTop(current.m_y + delta);
                    }
                    //we need to update the widgets geometry
                    std::shared_ptr<LWidget> w = a->widget();
                    if (w != nullptr){
                        w->setGeometry(current);
                    }
                    ++i;
                }
                newScrollFlags |= ScrollDown;
            }
        }
        m_scrollFlags = newScrollFlags;

        if(m_scrollFlags){
            if(m_sliderData != nullptr && m_useScrollBar){
                m_sliderData->m_maximum = m_viewSize-scrollerArea;
                if(m_sliderData->m_maximum >0){
                    m_sliderData->m_pageStep = std::max<int>(10, scrollerArea*scrollerArea/m_sliderData->m_maximum);
                }else{
                    m_sliderData->m_pageStep = 10;
                }
            }
        }
    }
}


void LAbstractMenu::setCurrentAction(std::shared_ptr<LActionBase> action, bool popup, SelectionReason reason, bool activateFirst)
{
    if (!considerAction(action)){
        action = nullptr;
    }

    if (reason != SelectedFromKeyboard) {
        std::shared_ptr<LAbstractMenu> menu = std::dynamic_pointer_cast<LAbstractMenu>(m_causedPopup.widget.lock());
        if (menu != nullptr) {
            std::shared_ptr<LActionBase> causedAct = m_causedPopup.action.lock();
            if(causedAct != nullptr){
                std::shared_ptr<LWidget> ap= causedAct->widget();
                if (ap.get() == this){
                    if (menu->m_currentAction != causedAct){
                        menu->setCurrentAction(causedAct, popup, reason, false);
                    }
                }
            }
        }
    }

    std::shared_ptr<LRegion> updateRegion = nullptr;
    if (m_currentAction != nullptr){
        updateRegion = std::make_shared<LRegion>(actionRect(m_currentAction));
    }

    if(popup == false){
        if(m_currentAction != nullptr &&  m_currentAction != action){
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
    }

    if (action != nullptr) {
        activateAction(action, LActionBase::Hover);
        if (popup) {
            if (isVisible()){
                popupAction(action, activateFirst);
            }else{
                m_currentAction = action;
            }
        }else{
            m_currentAction = action;
        }

        if(updateRegion != nullptr){
            updateRegion->unite(actionRect(action));
        }else{
            updateRegion = std::make_shared<LRegion>(actionRect(action));
        }

        if (reason == SelectedFromKeyboard) {
            std::shared_ptr<LWidget> widget = action->widget();
            if (widget) {
                if (widget->focusPolicy() != LFocusPolicy::No){
                    widget->setFocus(LFocusReason::Tab);
                }
            } else {
                if (!hasFocus()) {
                    setFocus(LFocusReason::Popup);
                }
            }
        }
    }else{
        m_hasHadMouseDown = false;
        m_currentAction = nullptr;
    }

    if(updateRegion != nullptr){
        update(updateRegion);
    }
}


void LAbstractMenu::scrollMenu(ScrollLocation location, bool active)
{
    updateActionRects();
    if (location == ScrollBottom) {
        std::list<std::shared_ptr<LActionBase> >::const_reverse_iterator iter = m_actions.crbegin();
        size_t i= m_actions.size()-1;
        bool isFind = false;
        for(; iter != m_actions.crend(); ++iter) {
            if (m_actionRects.at(i).isNull()){
                continue;
            }

            if(m_scrollHorizon){
                isFind = (m_actionRects[i].m_x - m_scrollOffset + m_actionRects[i].width()) >= m_viewSize;
            }else{
                isFind = (m_actionRects[i].m_y - m_scrollOffset + m_actionRects[i].height()) >= m_viewSize;
            }

            if (isFind) {
                std::shared_ptr<LActionBase> act = *iter;
                if (m_scrollFlags & ScrollDown){
                    scrollMenu(act, ScrollBottom, active);
                }
                else if (active){
                    setCurrentAction(act, false, SelectedFromKeyboard);
                }
                break;
            }
            --i;
        }
    } else if (location == ScrollTop) {
        int i=0;
        bool isFind = false;
        for(const std::shared_ptr<LActionBase>& act : m_actions) {
            if (m_actionRects[i].isNull()){
                continue;
            }
            if(m_scrollHorizon){
                isFind = m_actionRects[i].m_x <= m_scrollOffset;
            }else{
                isFind = m_actionRects[i].m_y <= m_scrollOffset;
            }

            if (isFind) {
                if (m_scrollFlags & ScrollUp){
                    scrollMenu(act, ScrollTop, active);
                }
                else if (active){
                    setCurrentAction(act, false, SelectedFromKeyboard);
                }
                break;
            }
            ++i;
        }
    }
}

void LAbstractMenu::scrollMenu(ScrollDirection direction, int destOffset, bool active)
{
    if (!m_scrollable){
        return;
    }

    int delta =0;
    if(direction == ScrollNone && destOffset<=0){
        delta = destOffset-m_scrollOffset;
        if (delta > 0) {
            direction = ScrollUp;
        }else if(delta < 0){
            direction = ScrollDown;
        }else{
            return;
        }
    }else{
        destOffset = 0xFF;
        if (!(m_scrollFlags & direction)){ //not really possible...
            return;
        }
    }
    LAbstractMenu::updateActionRects();
    if (direction == ScrollUp) {
        const int scrollerS = scrollerSize();

        int i=0;
        int lastPos1 = -m_viewSize;
        int pos1 = 0;
        std::shared_ptr<LActionBase> dest;

        if(m_scrollHorizon){
            const int scrollerLeftBound = boundWidth(LDirection::ToLeft)+scrollerS-delta;
            for(const std::shared_ptr<LActionBase>& a : m_actions){
                if(m_actionRects[i].isValid()){
                    pos1 =m_actionRects[i].m_x;
                    if (pos1 <scrollerLeftBound && pos1 >=lastPos1) {
                        lastPos1 = pos1;
                        dest = a;
                    }

                    if (m_actionRects[i].m_x >scrollerLeftBound) {
                        break;
                    }
                }
                ++i;
            }
        }else{
            const int scrollerUpBound = boundWidth(LDirection::ToTop)+scrollerS-delta;
            for(const std::shared_ptr<LActionBase>& a : m_actions){
                if(m_actionRects[i].isValid()){
                    pos1 =m_actionRects[i].m_y;
                    if (pos1 <scrollerUpBound && pos1 >=lastPos1) {
                        lastPos1 = pos1;
                        dest = a;
                    }

                    if (m_actionRects[i].m_y >scrollerUpBound) {
                        break;
                    }
                }
                ++i;
            }
        }

        if(dest!= nullptr){
            LAbstractMenu::scrollMenu(dest, ScrollTop, active);
        }else{
            if(destOffset<=0){
                std::list<std::shared_ptr<LActionBase> >::const_iterator iter = m_actions.cbegin();
                if(iter != m_actions.cend()){
                    LAbstractMenu::scrollMenu(*iter, ScrollTop, active);
                    return;
                }
            }

            m_scrollFlags &= ~ScrollUp;
            update();
        }
    } else if (direction == ScrollDown) {
        int i=0;
        int lastPos2 = m_viewSize;
        int pos2 = 0;
        std::shared_ptr<LActionBase> dest;

        const int scrollerS = scrollerSize();
        if(m_scrollHorizon){
            const int scrollerRightBound = width()-boundWidth(LDirection::ToRight)-scrollerS-delta;

            for(const std::shared_ptr<LActionBase>& a : m_actions){
                if(m_actionRects[i].isValid()){
                    pos2 =m_actionRects[i].x2();
                    if (pos2 > scrollerRightBound && pos2<=lastPos2) {
                        lastPos2 = pos2;
                        dest = a;
                    }

                    if (m_actionRects[i].m_x >= scrollerRightBound) {
                        break;
                    }
                }
                ++i;
            }
        }else{
            const int scrollerDownBound = height()-boundWidth(LDirection::ToBottom)-scrollerS-delta;
            for(const std::shared_ptr<LActionBase>& a : m_actions){
                if(m_actionRects[i].isValid()){
                    pos2 =m_actionRects[i].y2();
                    if (pos2 > scrollerDownBound && pos2<=lastPos2) {
                        lastPos2 = pos2;
                        dest = a;
                    }

                    if (m_actionRects[i].m_y >= scrollerDownBound) {
                        break;
                    }
                }
                ++i;
            }
        }

        if(dest!= nullptr){
            LAbstractMenu::scrollMenu(dest, ScrollBottom, active);
        }else{
            if(destOffset<=0){
                std::list<std::shared_ptr<LActionBase> >::const_reverse_iterator rIter = m_actions.crbegin();
                if(rIter != m_actions.crend()){
                    LAbstractMenu::scrollMenu(*rIter, ScrollBottom, active);
                    return;
                }
            }
            m_scrollFlags &= ~ScrollDown;
            update();
        }
    }
}


void LAbstractMenu::scrollMenu(std::shared_ptr<LActionBase> action, ScrollLocation location, bool active)
{
    if (!m_scrollable || !(m_scrollFlags)){ //not really possible...
        return;
    }

    updateActionRects();
    if(m_actionRects.empty()){
        return;
    }

    int newOffset = 0;
    int saccum = 0;
    int scrollerArea = 0;
    //figure out which scroll flags
    uint32_t newScrollFlags = ScrollNone;
    if(m_scrollHorizon){
        scrollerArea = width() - 2*scrollerSize() - boundWidth(LDirection::ToLeft) - boundWidth(LDirection::ToRight);
        const int startPos = m_actionRects[0].m_x;
        int i = actionIndex(action);

        if (location == ScrollTop) {
            saccum = m_actionRects[i].m_x - startPos;
            newOffset = -saccum;
        } else {
            saccum = m_actionRects[i].m_x - startPos + m_actionRects[i].m_width;
            if (location == ScrollCenter){
                newOffset = scrollerArea/2 - saccum;
                newScrollFlags |= ScrollDown;
            }else{
                newOffset = scrollerArea - saccum;
            }
        }
    }else{
        scrollerArea = height() - 2*scrollerSize() - boundWidth(LDirection::ToTop) - boundWidth(LDirection::ToBottom);
        const int startPos = m_actionRects[0].m_y;
        int i = actionIndex(action);

        if (location == ScrollTop) {
            saccum = m_actionRects[i].m_y - startPos;
            newOffset = -saccum;
        } else {
            saccum = m_actionRects[i].m_y - startPos + m_actionRects[i].m_height;
            if (location == ScrollCenter){
                newOffset = scrollerArea/2 - saccum;
                newScrollFlags |= ScrollDown;
            }else{
                newOffset = scrollerArea - saccum;
            }
        }
    }


    if (newOffset < 0){ //easy and cheap one
        newScrollFlags |= ScrollUp;
    }

    if(m_viewSize + newOffset> scrollerArea){
        newScrollFlags |= ScrollDown;
    }

    if (!(newScrollFlags & ScrollDown) && (m_scrollFlags & ScrollDown)) {
        newOffset = scrollerArea - m_viewSize;    //last item at bottom
    }

    if (!(newScrollFlags & ScrollUp) && (m_scrollFlags & ScrollUp)) {
        newOffset = 0;  //first item at top
    }

    //actually update flags
    const int delta = std::min<int>(0, newOffset) - m_scrollOffset; //make sure the new offset is always negative
    if ((m_itemsDirty==false) && delta) {
        //we've scrolled so we need to update the action rects
        int i=0;
        for(const std::shared_ptr<LActionBase>& a : m_actions){
            LRect<int> &current = m_actionRects[i];
            if(m_scrollHorizon){
                current.moveLeft(current.m_x + delta);
            }else{
                current.moveTop(current.m_y + delta);
            }
            //we need to update the widgets geometry
            std::shared_ptr<LWidget> w = a->widget();
            if (w != nullptr){
                w->setGeometry(current);
            }
            ++i;
        }
    }

    m_scrollFlags = newScrollFlags;
    m_scrollOffset += delta;
    updateSliderData();
    if (active){
        setCurrentAction(action);
    }
    update();     //issue an update so we see all the new state..
}

bool LAbstractMenu::mouseEventTaken(std::shared_ptr<LMouseEvent> e)
{
    if (frameGeometry().contains(e->globalPos())){
        return false; //otherwise if the event is in our rect we want it..
    }

    bool passOnEvent = false;
    std::shared_ptr<LWidget> nextWidget;
    std::shared_ptr<LWidget> popCause = m_causedPopup.widget.lock();
    while(popCause != nullptr) {
        nextWidget = nullptr;
        LPoint<int> cpos = popCause->mapFromGlobal(e->globalPos());
        passOnEvent = popCause->rect().contains(cpos);

        std::shared_ptr<LAbstractMenu> m = std::dynamic_pointer_cast<LAbstractMenu>(popCause);
        if(m != nullptr){
            nextWidget = m->m_causedPopup.widget.lock();
        }

        if (passOnEvent) {
            if (e->type() != LEvent::MouseButtonRelease  || m_mouseDown.lock() == popCause) {
                std::shared_ptr<LMouseEvent> newE
                    = std::make_shared<LMouseEvent>(e->type(),
                                                    cpos.toPointF(),
                                                    popCause->mapTo(popCause->window(), cpos).toPointF(),
                                                    e->screenPos(),
                                                    e->button(),
                                                    e->buttons(),
                                                    e->modifiers(),
                                                    e->source());
                LApplication::sendEvent(popCause, newE);
                return true;
            }
        }
        popCause = nextWidget;
    }
    return false;
}

void LAbstractMenu::drawScrollBar(LPainter *painter, LStyleOptComplex &opt)
{
    if(m_sliderData == nullptr){
        return;
    }

    if (opt.rect.isEmpty()){
        return;
    }


    if(m_sliderData->m_singleStep >0 && m_sliderData->m_maximum>0){
        painter->setClipRect(opt.rect);

        std::shared_ptr<LStyle> pStyle = style();
        opt.subCtrls.set();
        if (m_pressedCtrl.any()) {
            opt.activeSubCtrls = m_pressedCtrl;
        } else {
            opt.activeSubCtrls = m_hoverCtrl;
        }
        std::shared_ptr<LWidget> q= getPtrT<LWidget>();
        pStyle->drawScroll(q, *m_sliderData, painter, opt);
    }
}


void LAbstractMenu::drawScroller(LPainter *painter, const LStyleOpt& opt, ScrollDirection type)
{
    if (opt.rect.isEmpty()){
        return;
    }

    painter->setClipRect(opt.rect);
    std::shared_ptr<LRenderRule> rule;
    uint16_t state = 0;

    if(type == ScrollDirection::ScrollUp){
        state = uiElementState(LUiElementType::UpButton);
        rule = m_elementStyle.getRenderRule(LUiElementType::UpButton, state);
    }else{
        state = uiElementState(LUiElementType::DownButton);
        rule = m_elementStyle.getRenderRule(LUiElementType::DownButton, state);
    }

    if(rule != nullptr && rule->hasDrawable()){
        rule->drawRule(painter, opt.rect);
    }else{
        std::shared_ptr<LStyle> pStyle = style();
        // 先铺菜单底色，再画箭头，避免出现未绘制区域
        painter->fillRect(opt.rect.toRectF(), opt.palette->brush(opt.backgroundRole));
        if(m_scrollHorizon){
            pStyle->drawMark(painter, opt.rect.toRectF(), (type==ScrollUp)? LDrawMark::Left: LDrawMark::Right, &m_elementStyle, state);
        }else{
            pStyle->drawMark(painter, opt.rect.toRectF(), (type==ScrollUp)? LDrawMark::Up: LDrawMark::Down, &m_elementStyle, state);
        }
    }
}

void LAbstractMenu::addAction(std::shared_ptr<LActionBase> action)
{
    insertAction(nullptr, action);
}

int LAbstractMenu::removeAction(std::shared_ptr<LActionBase> action)
{
    int ret = m_actions.remove(action);
    if(ret >=0){
        m_itemsDirty = true;
        if(isVisible()){
            updateActionRects();
            update();
        }
    }
    return ret;
}

void LAbstractMenu::addActions(const std::list<std::shared_ptr<LActionBase> > &actions)
{
    insertActions(nullptr, actions);
}

void LAbstractMenu::insertAction(std::shared_ptr<LActionBase> before, std::shared_ptr<LActionBase> action)
{
    if (action == nullptr) {
        lWarn() << "LWidget::insertAction: Attempt to insert null action";
        return;
    }

    m_actions.remove(action);

    if(before != nullptr){
        std::list<std::shared_ptr<LActionBase> >::iterator iter = std::find(m_actions.begin(), m_actions.end(), before);
        if (iter != m_actions.end()) {
            m_actions.insert(iter, action);
        }else{
            m_actions.push_back(action);
        }
    }else{
        m_actions.push_back(action);
    }

    m_itemsDirty = true;
    if(isVisible()){
        updateActionRects();
        update();
    }
}

void LAbstractMenu::insertActions(std::shared_ptr<LActionBase> before, const std::list<std::shared_ptr<LActionBase> > &actions)
{
    std::list<std::shared_ptr<LActionBase> > t;
    for(const std::shared_ptr<LActionBase>& a : actions){
        if(a != nullptr){
            std::list<std::shared_ptr<LActionBase> >::iterator iter = std::find(m_actions.begin(), m_actions.end(), a);
            if(iter == m_actions.end()){
                t.push_back(a);
            }
        }
    }
    t.unique();

    if(t.size() != actions.size()){
        lWarn() << "Insert actions has duplicate removed!";
    }

    if(!t.empty()){
        if(before != nullptr){
            std::list<std::shared_ptr<LActionBase> >::iterator iter = std::find(m_actions.begin(), m_actions.end(), before);
            m_actions.insert(iter, t.begin(), t.end());
        }else{
            m_actions.insert(m_actions.end(), t.begin(), t.end());
        }

        m_itemsDirty = true;
        if(isVisible()){
            updateActionRects();
            update();
        }
    }
}

void LAbstractMenu::insertAction(int index, std::shared_ptr<LActionBase> action)
{
    if (action == nullptr) {
        lWarn() << "LWidget::insertAction: Attempt to insert null action";
        return;
    }

    m_actions.remove(action);

    if((int)m_actions.size()> index && index>=0){
        auto it = m_actions.begin();
        std::advance(it, index);
        m_actions.insert(it, action);
    }else{
        m_actions.push_back(action);
    }

    m_itemsDirty = true;
    if(isVisible()){
        updateActionRects();
        update();
    }
}

void LAbstractMenu::insertActions(int index, const std::list<std::shared_ptr<LActionBase> > &actions)
{
    insertActions(action(index), actions);
}

std::shared_ptr<LActionBase> LAbstractMenu::action(int index) const
{
    std::shared_ptr<LActionBase> t;
    if((int)m_actions.size()> index && index>=0){
        auto it = m_actions.begin();
        std::advance(it, index);
        t = *it;
    }
    return t;
}


std::shared_ptr<LActionBase> LAbstractMenu::actionAt(const LPoint<int> &p, bool* isAtClose) const
{
    if(isAtClose != nullptr){
        *isAtClose = false;
    }

    if (!rect().contains(p)){     //sanity check
        return nullptr;
    }

    int i=0;
    for(const LRect<int>& rect : m_actionRects) {
        if (rect.contains(p)){
            std::shared_ptr<LActionBase>  a = *std::next(m_actions.begin(), i);
            if(a != nullptr){
                if(a->isSeparator()){
                    return nullptr;
                }

                if(m_canCloseAction && isAtClose != nullptr){
                    *isAtClose = actionCloseRect(a, rect).contains(p);
                }
            }
            return a;
        }
        ++i;
    }
    return nullptr;
}

const std::shared_ptr<LWidget> LAbstractMenu::topCausedWidget() const
{
    std::shared_ptr<LWidget> top = m_causedPopup.widget.lock();
    std::shared_ptr<LAbstractMenu> m = std::dynamic_pointer_cast<LAbstractMenu>(top);
    while (m != nullptr){
        top = m->m_causedPopup.widget.lock();
        m = std::dynamic_pointer_cast<LAbstractMenu>(top);
    }
    return top;
}


void LAbstractMenu::setMenuCause(std::shared_ptr<LWidget> w, std::shared_ptr<LActionBase> at)
{
    m_causedPopup.widget = w;
    m_causedPopup.action = at;
}


std::shared_ptr<LActionBase> LAbstractMenu::insertMenu(std::shared_ptr<LActionBase> before, std::shared_ptr<LAbstractMenu> menu)
{
    if(menu != nullptr){
        std::shared_ptr<LActionBase> action = menu->m_menuAction.lock();
        if(action == nullptr){
            action = std::make_shared<LAction>(menu->wName());
            action->setWidget(menu, true);
        }
        insertAction(before, action);
        return action;
    }
    return nullptr;
}



void LAbstractMenu::setRepeatAction(LAbstractSlider::SliderAction action, int thresholdTime, int repeatTime)
{
    if(m_repeatTimer == nullptr){
        m_repeatTimer = std::make_shared<LTimer>();
        m_repeatTimer->sigTimeout.connect(this, &LAbstractMenu::doRepeat, ConnectionType::Queued);
    }

    if ((m_repeatAction = action) == LAbstractSlider::SliderNoAction) {
        m_repeatTimer->stop();
    } else {
        m_repeatActionTime = repeatTime;
        m_repeatTimer->start(std::chrono::milliseconds(thresholdTime), false);
    }
}

void LAbstractMenu::doRepeat()
{
    if (m_repeatActionTime != 0) { // was threshold time, use repeat time next time
        m_repeatTimer->start(std::chrono::milliseconds(m_repeatActionTime), true);
        m_repeatActionTime = 0;
    }
    triggerAction(m_repeatAction);
}

void LAbstractMenu::triggerAction(LAbstractSlider::SliderAction action)
{
    switch (action) {
    case LAbstractSlider::SliderSingleStepAdd:{
        scrollMenu(LAbstractMenu::ScrollDown);
        break;
    }
    case LAbstractSlider::SliderSingleStepSub:{
        scrollMenu(LAbstractMenu::ScrollUp);
        break;
    }
    default:{
        break;
    }
    };
}

void LAbstractMenu::stopRepeatAction()
{
    LUiSubCtrl tmp = m_pressedCtrl;
    setRepeatAction(LAbstractSlider::SliderNoAction);
    m_pressedCtrl.reset();

    if (tmp.test((uint8_t)LUiSubId::ScrollBar::Slider)){
        setSliderDown(false);
    }
}

void LAbstractMenu::setSliderDown(bool down)
{
    if(m_sliderData != nullptr){
        m_sliderData->m_pressed = down;
        if (!down && m_sliderData->m_position != -m_scrollOffset){
            triggerAction(LAbstractSlider::SliderMove);
        }
    }

}

void LAbstractMenu::activateControl(LUiSubCtrl control, int threshold)
{
    LAbstractSlider::SliderAction action = LAbstractSlider::SliderNoAction;
    switch (control.to_ulong()) {
    case 1<<(uint8_t)LUiSubId::ScrollBar::AddLine:{
        action = LAbstractSlider::SliderSingleStepAdd;
        break;
    }
    case 1<<(uint8_t)LUiSubId::ScrollBar::SubLine:{
        action = LAbstractSlider::SliderSingleStepSub;
        break;
    }
    default:
        break;
    }

    if (action) {
        setRepeatAction(action, threshold);
        triggerAction(action);
    }
}

bool LAbstractMenu::isSliderDataValid()
{
    if(m_useScrollBar && m_sliderData != nullptr){
        return m_sliderData->m_singleStep >0 && m_sliderData->m_maximum>m_sliderData->m_minimum;
    }
    return false;
}

int LAbstractMenu::pixelPosToRangeValue(int pos, const LStyleOptComplex &opt, std::shared_ptr<LStyle> pStyle) const
{
    if(m_sliderData != nullptr){
        LRect<int> gr = pStyle->scrollSubCtrlRect(*m_sliderData, opt, 1<<(uint8_t)LUiSubId::ScrollBar::Groove,-1,-1,-1,-1);
        LRect<int> sr = pStyle->scrollSubCtrlRect(*m_sliderData, opt, 1<<(uint8_t)LUiSubId::ScrollBar::Slider,-1,-1,-1,-1);
        int sliderMin, sliderMax, sliderLength;

        if(m_scrollHorizon){
            sliderLength = sr.width();
            sliderMin = gr.m_x;
            sliderMax = gr.x2() - sliderLength + 1;
        }else{
            sliderLength = sr.height();
            sliderMin = gr.m_y;
            sliderMax = gr.y2() - sliderLength + 1;
        }
        return  LStyle::sliderValueFromPosition(m_sliderData->m_minimum,
                                               m_sliderData->m_maximum,
                                               pos - sliderMin,
                                               sliderMax - sliderMin,
                                               m_sliderData->upsideDown(layoutDirection()));

    }
    return  0;
}

void LAbstractMenu::setSyncAction()
{
    std::shared_ptr<LActionBase> current = m_currentAction;
    if(current != nullptr){
        if ((!current->isEnabled() || current->isPopup() || current->isSeparator())){
            current = nullptr;
        }
    }

    std::shared_ptr<LAbstractMenu> caused = getPtrT<LAbstractMenu>();
    while(caused != nullptr) {
        if (caused->m_eventLoop != nullptr){
            caused->m_syncAction = current; //标志最后执行点击的action
        }
        caused = std::dynamic_pointer_cast<LAbstractMenu>(caused->m_causedPopup.widget.lock());
    }
}

void LAbstractMenu::hideUpToMenuBar()
{
    if((m_widgetData.windowFlags & LWindowType::Popup) == LWindowType::Popup){
        std::shared_ptr<LWidget> caused = m_causedPopup.widget.lock();
        hideMenu(); //hide after getting causedPopup
        while(caused != nullptr) {
            if(caused.get() == this){
                lWarn() << "Unexpect menu cause twisted!";
                break;
            }

            std::shared_ptr<LAbstractMenu> m = std::dynamic_pointer_cast<LAbstractMenu>(caused);
            if(m != nullptr){
                if((m->m_widgetData.windowFlags & LWindowType::Popup) == LWindowType::Popup){
                    caused = m->m_causedPopup.widget.lock();
                    m->hideMenu();
                }else{
                    caused = nullptr;
                }
                m->setCurrentAction(nullptr);
            }else{
                caused->update();
                caused = nullptr;
            }
        }
    }
    setCurrentAction(nullptr);
}

void LAbstractMenu::hideMenu()
{
    m_causedPopup.action.reset();
    close();
    m_causedPopup.widget.reset();
}

LPoint<int> LAbstractMenu::adjustedMenuPosition(std::shared_ptr<LWidget> q)
{
    if(q == nullptr){
        return LPoint<int>();
    }

    LRect<int> rect = q->rect();
    LPoint<int> globalPos = q->mapToGlobal(rect.topLeft());
    int x = globalPos.m_x;
    int y = globalPos.m_y;

    LSize<int> menuSize = sizeHint();
    const LRect<int> availableGeometry = LDesktopWidget::availableGeometry(q);

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

void LAbstractMenu::activateAction(std::shared_ptr<LActionBase> action, LActionBase::ActionEvent e)
{
    if (action==nullptr || !isEnabled()
        || (e == LActionBase::Trigger
            && (action->isSeparator() ||!action->isEnabled()))){
        return;
    }

    if (e == LActionBase::Trigger) {
        if(!action->isPopup()){
            action->activate(e);
            hideUpToMenuBar();
        }
    }else{
        action->activate(e);
    }
}

void LAbstractMenu::closeAction(std::shared_ptr<LActionBase> action, LActionBase::ActionEvent e)
{
    if (action== nullptr || action->isSeparator() || action->isPopup()){
        return;
    }
    action->close(e);
}


void LAbstractMenu::updateLayoutDirection()
{
    if (!testAttribute(LWidgetAttribute::SetLayoutDirection)) {
        std::shared_ptr<LWidget> w= m_causedPopup.widget.lock();
        if (w != nullptr){
            setLayoutDirectionHelper(w->layoutDirection());
            return;
        }

        w = parentT<LWidget>();
        if (w != nullptr){
            setLayoutDirectionHelper(w->layoutDirection());
        }else{
            setLayoutDirectionHelper(LGuiApplication::layoutDirection());
        }
    }
}

std::shared_ptr<LActionBase> LAbstractMenu::exec()
{
    return exec(pos());
}

std::shared_ptr<LActionBase> LAbstractMenu::exec(const LPoint<int> &pos, std::shared_ptr<LActionBase> at)
{
    if(m_eventLoop != nullptr){
        if(m_eventLoop->isRunning()){
            lWarn() << "Alread run menu!";
            return nullptr;
        }
    }

    createWinId();
    m_eventLoop = std::make_shared<LEventLoop>();
    popup(pos, at);

    m_eventLoop->exec();

    at = m_syncAction;  //反回最后执行点击的action
    m_syncAction = nullptr;
    m_eventLoop = nullptr;
    return at;
}

void LAbstractMenu::popup(const LPoint<int> &p, std::shared_ptr<LActionBase> atAction)
{
    std::shared_ptr<LScreen> popupScreen = LGuiApplication::screenAt(p);
    if(popupScreen == nullptr){
        return;
    }

    std::shared_ptr<LStyle> pStyle = style();
    bool scrollAble = m_scrollable;

    if (scrollAble) { // reset scroll state from last popup
        if (m_scrollOffset){
            m_itemsDirty = true; // sizeHint will be incorrect if there is previous scroll
        }
        m_scrollOffset = 0;
        m_scrollFlags = ScrollNone;
    }
    updateLayoutDirection();

    if (m_eventLoop == nullptr) {
        bool screenSet = false;
        std::shared_ptr<LAbstractMenu> parentMenu = std::dynamic_pointer_cast<LAbstractMenu>(m_parent.lock());
        if (parentMenu != nullptr) {
            if (setScreen(parentMenu->screen())){
                m_itemsDirty = true;
            }
            screenSet = true;
        }
        if (!screenSet && setScreenForPoint(p)){
            m_itemsDirty = true;
        }
    }

    const bool contextMenu = isContextMenu();
    if (m_lastContextMenu != contextMenu) {
        m_itemsDirty = true;
        m_lastContextMenu = contextMenu;
    }

    if (!windowHandle()) {
        createWinId();
    }

    const bool actionListChanged = m_itemsDirty;

    LRect<int> screen = popupGeometry(LGuiApplication::screenAt(p));
    updateActionRects(screen);

    std::shared_ptr<LWidget> causeWidget = m_causedPopup.widget.lock();
    LPoint<int> pos(p);
    if (actionListChanged && causeWidget != nullptr){
        LSizePolicy::ControlType ctrlType = causeWidget->sizePolicy().controlType();
        if(ctrlType == LSizePolicy::PushButton || ctrlType == LSizePolicy::ComboBox){
            pos = adjustedMenuPosition(causeWidget);
        }
    }

    popupScreen = LGuiApplication::screenAt(pos);

    const LSize<int> menuSizeHint(rawSizeHint());
    LSize<int> size = menuSizeHint;

    const int desktopFrame = pStyle->get(&LStylePixelMetric::menuDesktopFrameWidth, &m_elementStyle);
    bool adjustToDesktop = !window()->testAttribute(LWidgetAttribute::DontShowOnScreen);

    // if the screens have very different geometries and the menu is too big, we have to recalculate
    if (size.height() > screen.height() || size.width() > screen.width() || m_isReachBound ) {
        size.setWidth(std::min<int>(menuSizeHint.width(), screen.width() - desktopFrame * 2));
        size.setHeight(std::min<int>(menuSizeHint.height(), screen.height() - desktopFrame * 2));
        adjustToDesktop = true;
    }


    if (m_isReachBound) {
        if(m_scrollHorizon){
            pos.m_x = screen.m_x + desktopFrame;
        }else{
            pos.m_y = screen.m_y + desktopFrame;
        }
    } else if (atAction != nullptr) {
        if(!m_actions.empty()){
            const int startPos = m_scrollHorizon ? m_actionRects[0].m_x : m_actionRects[0].m_y;
            int i=0;
            for (const std::shared_ptr<LActionBase>& action : m_actions) {
                if (action == atAction) {
                    int newPos = (m_scrollHorizon ? (pos.m_x - m_actionRects[i].m_x) : (pos.m_y - m_actionRects[i].m_y)) - startPos;
                    if (scrollAble && newPos < desktopFrame) {
                        m_scrollFlags = m_scrollFlags | ScrollUp;
                        m_scrollOffset = newPos;
                        newPos = desktopFrame;
                    }

                    if(m_scrollHorizon){
                        pos.m_x = newPos;
                    }else{
                        pos.m_y = newPos;
                    }
                    break;
                }
                ++i;
            }
        }
    }

    // do nothing if we don't have a valid size, e.g. when all actions are invisible
    if (!size.isValid()) {
        m_eventLoop = nullptr;
        m_syncAction = nullptr;
        return;
    }

    const LPoint<int> mouse = LGuiApplication::m_lastCursorPosition.toPoint();
    bool snapToMouse = false;
    if(causeWidget == nullptr){
        snapToMouse = LRect<int>(p.m_x - 3, p.m_y - 3, 6, 6, false).contains(mouse);
    }

    if (adjustToDesktop) {
        // handle popup falling "off screen"
        size.setWidth(std::min<int>(size.width(), maximumWidth()));

        if (isRightToLeft()) {
            if (snapToMouse){
                pos.m_x = mouse.m_x - size.width();
            }

            if (pos.m_x < screen.m_x + desktopFrame){
                pos.m_x = std::max<int>(p.m_x, screen.m_x + desktopFrame);
            }

            if (pos.m_x + menuSizeHint.width()> screen.x2() - desktopFrame) {
                if (scrollAble && (m_scrollHorizon == false)) {
                    m_scrollFlags |= uint8_t(ScrollDown);
                    int x = std::max<int>(screen.m_x,pos.m_x);
                    size.setWidth(screen.x2() - (desktopFrame * 2) - x);
                }
            }

            if (pos.m_x + size.width() > screen.x2() - desktopFrame){
                pos.m_x = std::max<int>(p.m_x - size.width(), screen.x2() - desktopFrame - size.width());
            }
        } else {
            if (pos.m_x + menuSizeHint.width() > screen.x2() - desktopFrame) {
                if (scrollAble && (m_scrollHorizon == false)) {
                    m_scrollFlags |= uint8_t(ScrollDown);
                    int x = std::max<int>(screen.m_x,pos.m_x);
                    size.setWidth(screen.x2() - (desktopFrame * 2) - x);
                }
            }

            if (pos.m_x + size.width()> screen.x2() - desktopFrame){
                pos.m_x = screen.x2() - desktopFrame - size.width();
            }

            if (pos.m_x < screen.m_x + desktopFrame){
                pos.m_x = screen.m_x + desktopFrame;
            }
        }

        size.setHeight(std::min<int>(size.height(), maximumHeight()));

        if (pos.m_y + size.height()> screen.y2() - desktopFrame) {
            if (snapToMouse){
                pos.m_y = std::min<int>(mouse.m_y - (size.height() + desktopFrame), screen.y2()-desktopFrame-size.height());
            }
            else{
                pos.m_y = std::max<int>(p.m_y - (size.height() + desktopFrame), screen.y2()-desktopFrame-size.height());
            }
        }

        if (pos.m_y < screen.m_y + desktopFrame){
            pos.m_y = screen.m_y + desktopFrame;
        }

        if (pos.m_y + menuSizeHint.height()> screen.y2() - desktopFrame) {
            if (scrollAble && (m_scrollHorizon == false)) {
                m_scrollFlags |= uint8_t(ScrollDown);
                int y = std::max<int>(screen.m_y,pos.m_y);
                size.setHeight(screen.y2() - (desktopFrame * 2) - y);
            }
        }

        // Too big for screen, bias to see bottom of menu (for some reason)
        if (pos.m_y + size.height() > screen.y2() - desktopFrame) {
            pos.m_y = screen.y2() - size.height();
        }
    }

    popupScreen = LGuiApplication::screenAt(pos);
    setGeometry(LRect<int>(pos, size));
    updateSliderRange();
    show();

}

LRect<int> LAbstractMenu::popupGeometry(std::shared_ptr<LScreen> s) const
{
    if (s == nullptr) {
        s = isVisible() ? screen(): nullptr;
    }


    if (LDesktopWidget::useFullScreenForPopup()){
        if(s != nullptr){
            return s->m_geometry;
        }else{
            std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();
            return LDesktopWidget::screenGeometry(tThis);
        }
    }

    if(s != nullptr){
        return s->m_availableGeometry;
    }else{
        std::shared_ptr<LWidget> tThis = getPtrT<LWidget>();
        return LDesktopWidget::availableGeometry(tThis);
    }
}

LRect<int> LAbstractMenu::actionRect(std::shared_ptr<LActionBase> at) const
{
    int index = lItemIndex(m_actions, at);
    if (index < 0){
        return LRect<int>();
    }

    const_cast<LAbstractMenu*>(this)->LAbstractMenu::updateActionRects();
    if(index < (int)m_actionRects.size() ){
        return m_actionRects.at(index);
    }else{
        return LRect<int>();
    }
}

bool LAbstractMenu::considerAction(const std::shared_ptr<LActionBase> action)
{
    if (!action || action->isSeparator()){
        return false;
    }
    if (action->isEnabled()){
        return true;
    }
    return false;
}


void LAbstractMenu::setFirstActionActive()
{
    updateActionRects();
    uint32_t i=0;
    int scrollerVisibleBound =0;
    if (m_scrollable) {
        if(m_scrollHorizon){
            scrollerVisibleBound = boundWidth(LDirection::ToLeft)+scrollerSize();
        }else{
            scrollerVisibleBound = boundWidth(LDirection::ToTop)+scrollerSize();
        }
    }

    for(const std::shared_ptr<LActionBase>& act : m_actions) {
        if(i >= m_actionRects.size()){
            break;
        }

        const LRect<int> &rect = m_actionRects.at(i);
        if (rect.isNull()){
            ++i;
            continue;
        }
        if (m_scrollable) {
            if(m_scrollHorizon){
                if(rect.m_x < scrollerVisibleBound){
                    continue;
                }
            }else{
                if(rect.m_y < scrollerVisibleBound){
                    continue;
                }
            }
        }

        if(!act->isSeparator()){
            std::shared_ptr<LAction> a = std::dynamic_pointer_cast<LAction>(act);
            if(a != nullptr){
                if(act->isEnabled()){
                    setCurrentAction(a, false);
                    break;
                }
            }
        }
    }
}
LK_NAMESPACE_END
