#include "ltabbar.h"
#include "lkwidgets/draw/lstyle.h"
#include "lkwidgets/core/ldesktopwidget.h"

LK_NAMESPACE_BEGIN

LTabBar::LTabBar(std::shared_ptr<LWidget> parent, LFlags<LWindowType> f)  : LAbstractMenu(parent, f){
    m_sizePolicy = (LSizePolicy(LSizePolicy::Expanding, LSizePolicy::Minimum, LSizePolicy::TabBar));
    m_scrollHorizon = true;
    m_scrollable = true;
}

std::shared_ptr<LAbstractMenu> LTabBar::menu() const
{
    return getPtrT<LAbstractMenu>();
}

void LTabBar::setCanCloseAction(bool isClosable)
{
    if(m_canCloseAction != isClosable){
        m_canCloseAction = isClosable;
        m_itemsDirty = true;
        update();
    }
}

std::shared_ptr<LCombBoxBase> LTabBar::initItem(std::shared_ptr<LComboBoxItem>)
{
    return getPtrT<LTabBar>();
}

void LTabBar::updateActionRects(const LRect<int> &containerRect)
{
    if (!m_itemsDirty){
        return;
    }

    //let's reinitialize the buffer
    m_actionRects.resize(m_actions.size(), LRect<int>());
    std::shared_ptr<LStyle> pStyle = style();

    const int itemMargin = pStyle->get(&LStylePixelMetric::tabBarItemMargin, &m_elementStyle);
    const int markSpan = pStyle->get(&LStylePixelMetric::itemMarkSpan, &m_elementStyle);
    int maxItemHeight = 0;


    //calculate size
    const LFontMetrics fm = fontMetrics();
    const int icone = pStyle->get(&LStylePixelMetric::smallIconSize, &m_elementStyle);
    int i = 0;
    int viewSize = 0;
    for (const std::shared_ptr<LActionBase>& action : m_actions) {
        if (!action->isVisible()){
            ++i;
            continue;
        }

        LSize<int> sz;
        if (action->isSeparator()) {
            ++i;
            continue; //we don't really position these!
        } else {
            const std::wstring& s = action->text();
            if (!s.empty()){
                sz = fm.size((int)LTextFlag::ShowMnemonic, s);
                sz.m_y+=8;
            }


            const std::shared_ptr<LTexture> icon = action->icon();
            // If an icon is set, only the icon is visible
            if (icon != nullptr){
                sz.m_x += icone;
                sz.m_y = std::max<int>(sz.m_y, icone+4);
            }

            if((action->isPopup()==false) && m_canCloseAction){
                sz.m_x += markSpan+icone;
            }
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

void LTabBar::drawMenuItem(std::shared_ptr<LActionBase> a, LPainter *p, const LStyleOpt &opt, std::shared_ptr<LStyle> pStyle)
{
    if(a->isSeparator()){
        return;
    }


    LFlags<LUiState> state;
    if(isEnabled()){
        if(a == m_currentItem){
            state = LUiState::Active;
        }else if(a == m_currentAction){
            state = LUiState::Hover;
        }
    }else{
        state = LUiState::Disabled;
    }


    if(a->isPopup()){

    }else{
        std::shared_ptr<LRenderRule> rule;
        if(opt.elementStyle != nullptr){
            rule = opt.elementStyle->getRenderRule(LUiElementType::Item, state);
        }

        if(rule != nullptr){
            if (rule->hasDrawable()) {
                rule->drawRule(p, opt.rect);
            }
            rule->applyForeground(p);
        }else{
            // Windows 11 风格标签：直角、无圆角；选中页使用浅强调色底 + 底部强调条
            const LColor accent = opt.palette->brush(LPalette::Highlight)->m_color;
            LRect<double> r = opt.rect.toRectF();
            if(state == LUiState::Active){
                // 强调色与白色按比例混合得到的浅色底
                const auto blend = [](const LColor& c, double k){
                    return LColor(uint8_t(std::lround(255.0 * (1.0 - k) + c.m_red * k)),
                                  uint8_t(std::lround(255.0 * (1.0 - k) + c.m_green * k)),
                                  uint8_t(std::lround(255.0 * (1.0 - k) + c.m_blue * k)));
                };
                p->fillRect(r, blend(accent, 0.14));

                // 底部强调色指示条
                p->fillRect(LRect<double>(r.m_x, r.y2() - 3, r.m_width, 3, false), accent);

                p->setPen(accent);
            }else if (state == LUiState::Hover) {
                p->fillRect(r, opt.palette->brush(LPalette::Midlight));
                p->setPen(opt.palette->brush(LPalette::Text)->m_color);
            } else if (a->isEnabled()) {
                // 未选中标签：灰字，无背景
                p->setPen(opt.palette->brush(LPalette::Text)->m_color.lighter(190));
            } else {
                p->setPen(opt.palette->brush(LPalette::Disabled, LPalette::WindowText)->m_color);
            }
        }
    }

    const int icone = pStyle->get(&LStylePixelMetric::smallIconSize, &m_elementStyle);
    const int space = pStyle->get(&LStylePixelMetric::tabBarItemMargin, &m_elementStyle);

    int pos = opt.rect.m_x+space;
    const std::shared_ptr<LTexture> icon = a->icon();
    if(icon != nullptr){
        LRect<int> iconRect(0, 0, icone, icone, false);
        iconRect.moveCenter(LRect<int>(pos, opt.rect.m_y, icone, opt.rect.m_height, false).center());
        iconRect = LDrawUtil::visualRect(layoutDirection(), opt.rect, iconRect);
        p->drawPixmap(iconRect.toRectF(),
                      icon->getPixmap(a->isEnabled()? LTexture::Normal: LTexture::Disable),
                      icon->rect().toRectF());

        pos+=icone;
    }

    const std::wstring& s = a->text();
    if (!s.empty()){
        int textFlags = LTextFlag::VCenter | LTextFlag::ShowMnemonic |
                        LTextFlag::DontClip | LTextFlag::SingleLine |
                        LTextFlag::Left;

        int width = opt.rect.x2()-pos-space;
        if(m_canCloseAction){
            if(!a->isPopup()){
                width -= (icone+space);
            }
        }

        LRect<int> textRect(pos, opt.rect.m_y, width, opt.rect.height(), false);
        textRect = LDrawUtil::visualRect(layoutDirection(), opt.rect, textRect);
        p->drawText(textRect.toRectF(), textFlags, s);
    }

    if(m_canCloseAction){
        LRect<int> iconRect(0, 0, icone, icone, false);
        iconRect.moveCenter(LRect<int>(opt.rect.x2()-space-icone, opt.rect.m_y, icone, opt.rect.m_height, false).center());
        iconRect = LDrawUtil::visualRect(layoutDirection(), opt.rect, iconRect);
        pStyle->drawMark(p, iconRect.toRectF(), LDrawMark::Crossed, opt.elementStyle);
    }
}

void LTabBar::popupAction(std::shared_ptr<LActionBase> a, bool activateFirst)
{
    lWarn() << "Unexpect popup action at tabbar!";
}

void LTabBar::currrentItemUpdate()
{
    update();
}

void LTabBar::paintEvent(std::shared_ptr<LPaintEvent> e)
{
    LAbstractMenu::paintEvent(e);

    // 底部分隔线：极浅的一条，只用于和内容面板做轻微分界
    LPainter p(paintDevice());
    p.setPen(LColor(228, 228, 228));
    p.drawLine(LLine<int>(0, height() - 1, width() - 1, height() - 1));
}

LRect<int> LTabBar::actionCloseRect(std::shared_ptr<LActionBase> a, const LRect<int> &actRect) const
{
    if(m_canCloseAction){
        if(a->isSeparator() || a->isPopup()){
            return LRect<int>();
        }
        std::shared_ptr<LStyle> pStyle = style();
        const int icone = pStyle->get(&LStylePixelMetric::smallIconSize, &m_elementStyle);
        const int space = pStyle->get(&LStylePixelMetric::tabBarItemMargin, &m_elementStyle);

        LRect<int> iconRect(0, 0, icone, icone, false);
        iconRect.moveCenter(LRect<int>(actRect.x2()-space-icone, actRect.m_y, icone, actRect.m_height, false).center());
        return LDrawUtil::visualRect(layoutDirection(), actRect, iconRect);
    }else{
        return LRect<int>();
    }
}



LK_NAMESPACE_END
