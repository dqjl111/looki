#include "lstyle.h"
#include "lkgui/lguienums.h"
#include "lkgui/paint/lbrush.h"
#include "lkwidgets/draw/ldrawutil.h"
#include "lkwidgets/util/laction.h"
#include "lkwidgets/core/lwidget.h"
#include "lkwidgets/widget/lframe.h"
#include "lkwidgets/widget/llabel.h"
#include "lkwidgets/widget/lslider.h"
#include "lkwidgets/draw/lwidgetstyle.h"
LK_NAMESPACE_BEGIN


LStyle::LStyle() {}

void LStyle::drawFrame(std::shared_ptr<LWidget> w, LPainter *p, const LFrameData& f, bool fill) {

    if(w!= nullptr && p!= nullptr){
        uint16_t state = w->uiElementState(LUiElementType::None);
        std::shared_ptr<LRenderRule> rule = w->elementStyle().getRenderRule(LUiElementType::None, state);
        if(rule != nullptr){
            if(fill){
                if(rule->hasDrawable()){
                    rule->drawRule(p, w->rect());
                    return;
                }
            }else{
                if (rule->hasBorder()) {
                    rule->drawBorder(p, rule->borderRect(w->rectF()).toRect());
                    return;
                }
            }
        }

        std::shared_ptr<LBrush> brush = nullptr;
        if(fill){
            // Windows 11 风格状态填充：悬停轻微变灰，按下更暗
            const bool pressed = (f.m_shape == LFrameData::Box)
                                     ? (f.m_shadow == LFrameData::Raised)
                                     : (f.m_shadow == LFrameData::Sunken);
            if (state & (uint16_t)LUiState::Disabled) {
                brush = w->palette()->brush(w->backgroundRole());
            } else if (pressed) {
                brush = w->palette()->brush(LPalette::Dark);
            } else if (state & (uint16_t)LUiState::Hover) {
                brush = w->palette()->brush(LPalette::Midlight);
            } else {
                brush = w->palette()->brush(w->backgroundRole());
            }
        }
        drawFrame(w->rect(), p, w->palette(), w->foregroundRole(), f.m_shape, f.m_shadow, f.m_lineWidth, f.m_midLineWidth, brush);
    }
}

void LStyle::drawFrame(const LRect<int>& fRect,
                       LPainter *p,
                       std::shared_ptr<LPalette> palette,
                       LPalette::ColorRole foregroundRole,
                       int shape,
                       int shadow,
                       int lineWidth,
                       int midLineWidth,
                       std::shared_ptr<LBrush> fill)
{
    switch (shape) {
    // Windows 11 风格：扁平填充 + 1px 微妙描边 + 小圆角
    case LFrameData::Box:
    case LFrameData::StyledPanel:
    case LFrameData::Panel:
    case LFrameData::WinPanel: {
        if (shadow == LFrameData::Plain) {
            LDrawUtil::drawPlainRect(p, fRect, palette->color(foregroundRole), lineWidth, fill);
            break;
        }

        LColor borderColor = palette->brush(LPalette::Mid)->m_color;
        p->save();
        p->setRenderHints(LRenderHint::Antialiasing, true);
        const int radius = std::min<int>(get(&LStylePixelMetric::controlRadius),
                                         std::min<int>(fRect.m_width / 2, fRect.m_height / 2));
        if (fill != nullptr) {
            p->setBrush(fill);
        } else {
            p->setBrush(LBrush::NoPattern);
        }
        p->setPen(borderColor, LPenStyle::Solid, 1);
        p->drawRoundedRect(LRect<double>(fRect.m_x, fRect.m_y,
                                         fRect.m_width - 1, fRect.m_height - 1, false),
                           radius, radius, LSizeMode::Absolute);
        p->restore();
        break;
    }
    case LFrameData::HLine:
    case LFrameData::VLine: {
        LPoint<int> p1, p2;
        if (shape == LFrameData::HLine) {
            p1 = LPoint<int>(fRect.m_x, fRect.m_y + fRect.m_height / 2);
            p2 = LPoint<int>(fRect.m_x + fRect.m_width, p1.m_y);
        } else {
            p1 = LPoint<int>(fRect.m_x + fRect.m_width / 2, fRect.m_y);
            p2 = LPoint<int>(p1.m_x, p1.m_y + fRect.m_height);
        }
        if (shadow == LFrameData::Plain) {
            std::shared_ptr<LPen> pen = p->pen();
            LColor oldColor = pen->color();
            double oldWidth = pen->m_width;

            pen->m_width = lineWidth;
            pen->setColor(palette->brush(foregroundRole)->m_color);
            p->setPen(pen, true);
            p->drawLine(LLine(p1, p2));

            // restore pen
            pen->setColor(oldColor);
            pen->m_width = oldWidth;
            p->setPen(pen, true);
        } else {
            // Windows 11 风格分隔线：单色 1px
            std::shared_ptr<LPen> pen = p->pen();
            LColor oldColor = pen->color();
            double oldWidth = pen->m_width;

            pen->m_width = 1;
            pen->setColor(palette->brush(LPalette::Mid)->m_color);
            p->setPen(pen, true);
            p->drawLine(LLine(p1, p2));

            pen->setColor(oldColor);
            pen->m_width = oldWidth;
            p->setPen(pen, true);
        }
        break;
    }
    default: {
        break;
    }
    }
}

LRect<double> LStyle::labelIconRect(const LLabelData &f, LRect<int>& fRect,
                                    bool isImageVertical, const LWidgetStyle* ele) const
{
    LRect<double> iconRect;

    const std::shared_ptr<LTexture>& icon = f.icon();
    if(f.m_indentHor >0){
        if ((f.m_align & (uint16_t)LAlignment::Left)){
            if (icon != nullptr
                && (isImageVertical == false)
                && f.m_indentHor >= icon->width()/icon->devicePixelRatio()){

                iconRect.m_x = fRect.m_x;
                iconRect.m_y = fRect.m_y;
                iconRect.m_width = f.m_indentHor;
                iconRect.m_height = fRect.m_height;
            }

            int indentHor = f.m_indentHor + get(&LStylePixelMetric::iconSpan, ele);
            fRect.m_x += indentHor;
            fRect.m_width -= indentHor;
        }else if(f.m_align & (uint16_t)LAlignment::Right){
            if (icon != nullptr
                && (isImageVertical == false)
                && f.m_indentHor >= icon->width()/icon->devicePixelRatio()){

                iconRect.m_x = fRect.m_x +fRect.m_width- f.m_indentHor;
                iconRect.m_y = fRect.m_y;
                iconRect.m_width = f.m_indentHor;
                iconRect.m_height = fRect.m_height;
            }

            int indentHor = f.m_indentHor + get(&LStylePixelMetric::iconSpan, ele);
            fRect.m_width -= indentHor;
        }
    }

    if(f.m_indentVer >0){
        if ((f.m_align & (uint16_t)LAlignment::Top)){
            if (icon != nullptr
                && (isImageVertical)
                && f.m_indentVer >= icon->height()/icon->devicePixelRatio()){

                iconRect.m_x = fRect.m_x;
                iconRect.m_y = fRect.m_y;
                iconRect.m_width = fRect.width();
                iconRect.m_height = f.m_indentVer;
            }

            int indentVer = f.m_indentVer + get(&LStylePixelMetric::iconSpan, ele);
            fRect.m_y += indentVer;
            fRect.m_height -= indentVer;
        }else if(f.m_align & (uint16_t)LAlignment::Bottom){
            if (icon != nullptr
                && (isImageVertical)
                && f.m_indentVer >= icon->height()/icon->devicePixelRatio()){

                iconRect.m_x = fRect.m_x;
                iconRect.m_y = fRect.m_y+fRect.m_height-f.m_indentVer;
                iconRect.m_width = fRect.width();
                iconRect.m_height = f.m_indentVer;
            }

            int indentVer = f.m_indentVer + get(&LStylePixelMetric::iconSpan, ele);
            fRect.m_height -= indentVer;
        }
    }
    return iconRect;
}

LPoint<double> LStyle::labelIconPos(const LLabelData &f, const LRect<double>& iconRect) const
{
    const std::shared_ptr<LTexture>& icon = f.icon();
    LPoint<double> iconPos;
    if ((f.m_align & (uint16_t)LAlignment::Left)){
        iconPos.m_x = iconRect.m_x;
    }else if(f.m_align & (uint16_t)LAlignment::HCenter){
        iconPos.m_x = iconRect.m_x + (iconRect.width() - icon->width())/2/icon->devicePixelRatio();
    }else{
        iconPos.m_x = iconRect.x2() - icon->width()/icon->devicePixelRatio();
    }

    if(f.m_align & (uint16_t)LAlignment::Top){
        iconPos.m_y = iconRect.m_y;
    }else if(f.m_align & (uint16_t)LAlignment::VCenter){
        iconPos.m_y = iconRect.m_y + (iconRect.height() - icon->height())/2/icon->devicePixelRatio();
    }else{
        iconPos.m_y = iconRect.y2() - icon->height()/icon->devicePixelRatio();
    }
    return iconPos;
}

void LStyle::drawLabel(std::shared_ptr<LWidget> w, LPainter *p, const LLabelData &f, LDrawMark mark)
{
    if(w!= nullptr && p!= nullptr){
        drawLabel(f, w->contentsRect(), p, w->isEnabled(), mark, &(w->elementStyle()));
    }
}


void LStyle::drawLabel(const LLabelData& f,
                       LRect<int> fRect,
                       LPainter *p,
                       bool isEnable,
                       LDrawMark mark,
                       const LWidgetStyle* ele)
{
    const std::shared_ptr<LTexture>& icon = f.icon();

    if(f.m_isTextLabel){
        if(icon != nullptr){
            LRect<double> iconRect = labelIconRect(f, fRect, f.m_imageVertical);
            if(iconRect.isValid()){
                p->drawPixmap(labelIconPos(f, iconRect),
                              icon->getPixmap(isEnable? LTexture::Normal: LTexture::Disable),
                              icon->rect().toRectF());
            }
        }

        uint16_t state = 0;
        if(ele != nullptr){
            std::shared_ptr<LWidget> w = ele->widget();
            if(w != nullptr){
                state = w->uiElementState(LUiElementType::None);
            }
        }

        if(f.m_hasMark){
            int markSize = get(&LStylePixelMetric::smallIconSize, ele);
            int markCost = get(&LStylePixelMetric::itemMarkSpan, ele) + markSize;
            LRect<double> markRect = fRect.toRectF();

            if(f.m_isMarkLeft){
                markRect.setWidth(markSize);
                fRect.m_x += markCost;
                fRect.m_width -= markCost;
            }else{
                markRect.setWidth(markSize);
                markRect.m_x = fRect.x2() - markCost;
                fRect.m_width -= markCost;
            }

            drawMark(p, markRect, mark, ele, state);
        }
        if(!f.m_text.empty()){
            if(ele != nullptr){
                std::shared_ptr<LWidget> w = ele->widget();
                if(w != nullptr){
                    bool hasColor = false;
                    std::shared_ptr<LRenderRule> rule = w->elementStyle().getRenderRule(LUiElementType::None, state);
                    if(rule != nullptr){
                        hasColor = rule->applyForeground(p) & 0x01;
                    }

                    if(!hasColor){
                        std::shared_ptr<LPalette> pal = w->palette();
                        p->setPen(pal->brush(LPalette::Text)->m_color);
                    }
                }
            }
            p->drawText(fRect.toRectF(), f.m_text, f.m_align);
        }
    }else{
        if(icon != nullptr){
            p->drawPixmap(LPoint<double>(fRect.m_x, fRect.m_y),
                               icon->getPixmap(isEnable? LTexture::Normal: LTexture::Disable),
                               icon->rect().toRectF());
        }
    }
}

void LStyle::drawMenuItem(std::shared_ptr<LWidget> w,
                          std::shared_ptr<LActionBase> a,
                          LPainter *p,
                          const LStyleOpt& opt,
                          int maxIconWidth,
                          int tabWidth) {
    if (a->isVisible()) {
        const LRect<double>& rect = opt.rect.toRectF();
        const std::shared_ptr<LPalette>& palette = opt.palette;
        if (a->isSeparator()) {
            if(opt.elementStyle != nullptr && w!= nullptr){
                std::shared_ptr<LRenderRule> rule = opt.elementStyle->getRenderRule(LUiElementType::Separator, a->uiState());
                if(rule != nullptr){
                    rule->applyForeground(p);
                }
            }

            double yoff = rect.m_y + rect.m_height / 2;
            // Windows 11 风格分隔线：单色 1px
            p->setPen(palette->brush(LPalette::Mid)->m_color);
            p->drawLine(LLine<double>(rect.m_x, yoff, rect.x2(), yoff));

        } else {
            std::shared_ptr<LRenderRule> rule;
            if(opt.elementStyle != nullptr && w!= nullptr){
                rule = opt.elementStyle->getRenderRule(LUiElementType::Item, a->uiState());
            }

            LColor textColor;
            bool hasColor = false;
            if(rule != nullptr){
                if (rule->hasDrawable()) {
                    rule->drawRule(p, opt.rect);
                }
                hasColor = rule->applyForeground(p) & 0x01;
            }else{
                if (a->isSelected()) {
                    // Windows 11 风格：圆角强调色高亮
                    p->save();
                    p->setRenderHints(LRenderHint::Antialiasing, true);
                    p->setBrush(palette->brush(LPalette::Highlight));
                    p->setPen(LPenStyle::No);
                    p->drawRoundedRect(rect.adjusted(1, 1, -1, -1), 4, 4, LSizeMode::Absolute);
                    p->restore();
                }
                // 未选中项不填充，直接露出菜单自身背景，避免出现色块拼接
            }

            int checkcol = std::max<int>(maxIconWidth, get(&LStylePixelMetric::itemMarkSpan, opt.elementStyle));
            int textFlags = LTextFlag::VCenter | LTextFlag::ShowMnemonic |
                            LTextFlag::DontClip | LTextFlag::SingleLine |
                            LTextFlag::Left;


            if(!hasColor){
                if (a->isEnabled()) {
                    if (a->isSelected()) {
                        textColor = palette->highlightedText()->m_color;
                    } else {
                        textColor = palette->buttonText()->m_color;
                    }
                } else {
                    textColor = textColor = palette->brush(LPalette::Disabled, LPalette::Text)->m_color;
                }
                p->setPen(textColor);
            }

            std::shared_ptr<LTexture> icon = a->icon();
            if (icon != nullptr) {
                uint16_t size = get(&LStylePixelMetric::smallIconSize, opt.elementStyle);
                std::shared_ptr<LPixmap> pixmap = nullptr;

                if (a->isEnabled()) {
                    if (a->isSelected()) {
                        pixmap = icon->getPixmap(LSize<int>(size, size), LTexture::Select, true);
                    } else {
                        pixmap = icon->getPixmap(LSize<int>(size, size), LTexture::Normal, true);
                    }
                } else {
                    pixmap = icon->getPixmap(LSize<int>(size, size), LTexture::Disable, true);
                }

                const double pixw = pixmap->width() / pixmap->devicePixelRatio();
                const double pixh = pixmap->height() / pixmap->devicePixelRatio();
                LRect<double> iconRect(0, 0, pixw, pixh, false);
                iconRect.moveCenter(LRect<double>(rect.m_x, rect.m_y, checkcol, rect.m_height, false).center());
                LRect<double> vIconRect = LDrawUtil::LDrawUtil::visualRect(opt.direction, rect, iconRect);
                p->setPen(palette->text()->m_color);
                p->setBrush(LBrush::NoPattern);
                if (a->isChecked()) {
                    p->drawRect(vIconRect.adjusted(-1, -1, 0, 0));
                }
                p->drawPixmap(vIconRect.topLeft(), pixmap);
            } else if (a->isChecked()) {
                LRect<double> checkMarkRect = LRect<double>(rect.m_x + 2, rect.m_y + 2, checkcol, rect.height(), false);
                LRect<double> tRect = LDrawUtil::visualRect(opt.direction, rect, checkMarkRect);
                if(a->isEnabled()){
                    if(a->isSelected()){
                        drawMark(p, tRect, LDrawMark::Checked, opt.elementStyle, (uint16_t)LUiState::Hover);
                    }else{
                        drawMark(p, tRect, LDrawMark::Checked, opt.elementStyle, (uint16_t)LUiState::Enabled);
                    }
                }else{
                    drawMark(p, tRect, LDrawMark::Checked, opt.elementStyle, (uint16_t)LUiState::Disabled);
                }
            }

            double xm = checkcol + get(&LStylePixelMetric::checkItemAddWidth, opt.elementStyle);
            double addWidth = get(&LStylePixelMetric::menuItemAddWidth, opt.elementStyle);
            double xpos = rect.m_x + xm;
            LRect<double> textRect(xpos, rect.m_y,
                                   rect.m_width - xm - 2*addWidth - tabWidth + 1,
                                   rect.m_height, false);
            LRect<double> vTextRect = LDrawUtil::visualRect(opt.direction, rect, textRect);
            const std::wstring &s = a->text();
            if (!s.empty()) {
                int t = (int)s.find('\t');
                // draw tab text ----------------
                if (t >= 0) {
                    std::wstring tt = s.substr(0, t);

                    LRect<double> vShortcutRect =
                        LDrawUtil::visualRect(opt.direction, rect, LRect<double>(textRect.topRight(), rect.bottomRight()));

                    p->drawText(vShortcutRect.toRectF(), textFlags, s.substr(t + 1));
                    p->drawText(vTextRect, textFlags, tt);
                } else {
                    p->drawText(vTextRect, textFlags, s);
                }
            }

            // // draw sub menu arrow --------------------------------------------
            if (a->isPopup()) {
                double dim = rect.m_height - 2;
                double markSpan = get(&LStylePixelMetric::itemMarkSpan, opt.elementStyle);
                xpos = rect.m_x + rect.m_width - markSpan-addWidth;
                LRect<double>  vSubMenuRect =
                    LDrawUtil::visualRect(opt.direction, rect, LRect<double>(xpos, rect.m_y + rect.m_height / 2 - dim / 2, markSpan, dim, false));


                if(a->isEnabled()){
                    if(a->isSelected()){
                        drawMark(p, vSubMenuRect, LDrawMark::Right, opt.elementStyle, (uint16_t)LUiState::Hover);
                    }else{
                        drawMark(p, vSubMenuRect, LDrawMark::Right, opt.elementStyle, (uint16_t)LUiState::Enabled);
                    }
                }else{
                    drawMark(p, vSubMenuRect, LDrawMark::Right, opt.elementStyle, (uint16_t)LUiState::Disabled);
                }
            }
        }
    }
}


void LStyle::drawScroll(std::shared_ptr<LWidget> w, const LSliderData &f, LPainter *p, const LStyleOptComplex &opt)
{
    if(p != nullptr){
        int sbextent = get(&LStylePixelMetric::scrollBarExtent, opt.elementStyle);
        int maxLen = scrollMaxLen(f, opt, sbextent);
        int sliderLen = scrollSliderLen(f, opt, sbextent, maxLen);
        int sliderStart = scrollSliderStart(f, opt, sbextent, maxLen, sliderLen);

        const static uint8_t drawOrder[(uint8_t)(LUiSubId::ScrollBar::Groove)+1] = {
                                                                                        (uint8_t)LUiSubId::ScrollBar::AddLine,
                                                                                        (uint8_t)LUiSubId::ScrollBar::SubLine,
                                                                                        (uint8_t)LUiSubId::ScrollBar::AddPage,
                                                                                        (uint8_t)LUiSubId::ScrollBar::SubPage,
                                                                                        (uint8_t)LUiSubId::ScrollBar::Groove,
                                                                                        (uint8_t)LUiSubId::ScrollBar::Slider,
                                                                                    };

        std::shared_ptr<LRenderRule> rule = nullptr;
        for(uint8_t subId: drawOrder){
            if (opt.subCtrls.test(subId)) {

                LUiElementType element = scrollSubCtrlUiElement(subId, f.m_orientation);
                LRect<int> rect = scrollSubCtrlRect(f, opt, 1<<subId, sbextent, maxLen, sliderLen, sliderStart);

                if (rect.isValid()) {
                     LFlags<LUiState > state = opt.state;
                    if(w!= nullptr){
                        state = w->uiElementState(element);
                        rule = w->m_elementStyle.getRenderRule(element, state);
                    }

                    if(rule != nullptr && rule->hasDrawable()){
                        rule->drawRule(p, rect);
                    }else{

                        if (!(opt.activeSubCtrls.test((uint8_t)subId))){
                            state &= ~(LUiState::Sunken | LUiState::Hover);
                        }

                        if(subId == (uint8_t)(LUiSubId::ScrollBar::Slider)){
                            // Windows 11 风格滑块：圆角条，悬停/按下加深
                            LColor thumbColor = opt.palette->brush(LPalette::Shadow)->m_color;
                            if (state & (uint16_t)LUiState::Disabled) {
                                thumbColor = opt.palette->brush(LPalette::Mid)->m_color;
                            } else if (state & LUiState::Sunken) {
                                thumbColor = thumbColor.darker(135);
                            } else if (state & LUiState::Hover) {
                                thumbColor = thumbColor.darker(115);
                            }

                            p->fillRect(rect.toRectF(), opt.palette->brush(LPalette::AlternateBase));
                            p->save();
                            p->setRenderHints(LRenderHint::Antialiasing, true);
                            LRect<double> thumb = rect.toRectF().adjusted(2, 2, -2, -2);
                            const double r = std::min<double>(thumb.m_width, thumb.m_height) / 2;
                            p->setBrush(thumbColor, LBrush::SolidPattern);
                            p->setPen(LPenStyle::No);
                            p->drawRoundedRect(thumb, r, r, LSizeMode::Absolute);
                            p->restore();
                        }else{
                            // 轨道、翻页与箭头区域：浅色轨道（Windows 11 不显示箭头）
                            p->fillRect(rect.toRectF(), opt.palette->brush(LPalette::AlternateBase));
                        }
                    }
                }
            }
        }
    }
}

void LStyle::drawScroll(const LSliderData &f, LPainter *p, const LStyleOptComplex &opt)
{
    drawScroll(nullptr, f, p, opt);
}

void LStyle::drawPrimitive(std::shared_ptr<LWidget> w,
                           const LRect<int> &fRect,
                           LPainter *p,
                           LUiElementType type)
{
    if(p != nullptr){
        if(w != nullptr){
            uint16_t state = w->uiElementState(type);
            std::shared_ptr<LRenderRule> rule = w->m_elementStyle.getRenderRule(type, state);
            if(rule != nullptr){
                if (rule->hasDrawable()) {
                    rule->drawRule(p, fRect);
                } else {
                    drawPrimitive(fRect, p, type, state, w->palette());
                }
            }else{
                drawPrimitive(fRect, p, type, state, w->palette());
            }
        }else{
            drawPrimitive(fRect, p, type, (uint16_t)LUiState::None, nullptr);
        }
    }
}

void LStyle::drawPrimitive(const LRect<int> &fRect,
                           LPainter *p,
                           LUiElementType type,
                           uint16_t state,
                           std::shared_ptr<LPalette> palette)
{
    if(p != nullptr){
        switch(type){
        case LUiElementType::Corner:{
            if(palette == nullptr){
                palette = LGuiApplication::palette();
            }
            std::shared_ptr<LBrush> brush = palette->brush(LPalette::Window);
            p->fillRect(fRect.toRectF(), brush);
            break;
        }
        default:{
            break;
        }
        }
    }
}


LUiSubCtrl LStyle::hitTestSlider(LSliderData &f, const LStyleOptComplex &opt, const LPoint<int> &pt, uint8_t tickPosition, const LWidgetStyle* ele) const
{
    LUiSubCtrl ret;
    int thickness = sliderCtrlThickness(f.m_orientation, opt.rect, tickPosition);
    int tickOffset = sliderTickOffset(f.m_orientation, opt.rect, tickPosition, thickness);
    LRect<int> r = sliderHandleRect(f, opt, tickPosition, thickness, tickOffset);
    if (r.isValid() && r.contains(pt)) {
        ret.set((uint8_t)LUiSubId::Slider::Handle, true);
    } else {
        r = sliderGrooveRect(f.m_orientation,
                             opt,
                             tickPosition,
                             thickness,
                             tickOffset);
        if (r.isValid() && r.contains(pt)){
            ret.set((uint8_t)LUiSubId::Slider::Groove, true);
        }
    }
    return ret;
}

int LStyle::sliderCtrlThickness(const LOrientation orient,
                                const LRect<int> &rect,
                                const uint8_t tickPosition,
                                const LWidgetStyle* ele) const
{
    int space = (orient == LOrientation::Horizontal) ? rect.height() : rect.width();
    int ticks = tickPosition;
    int n = 0;
    if (ticks & LSlider::TicksAbove){
        ++n;
    }
    if (ticks & LSlider::TicksBelow){
        ++n;
    }
    if (!n) {
        return space;
    }

    int thick = 6;        // Magic constant to get 5 + 16 + 5
    if (ticks != LSlider::TicksBothSides && ticks != LSlider::NoTicks){
        thick += get(&LStylePixelMetric::sliderLength, ele) / 4;
    }

    space -= thick;
    if (space > 0){
        thick += (space * 2) / (n + 2);
    }
    return thick;
}

int LStyle::sliderTickOffset(const LOrientation orient,
                             const LRect<int>& rect,
                             const uint8_t tickPosition,
                             int ctrlThickness,
                             const LWidgetStyle* ele) const
{
    if(ctrlThickness <0){
        ctrlThickness = sliderCtrlThickness(orient, rect, tickPosition, ele);
    }

    int space = (orient == LOrientation::Horizontal) ? rect.height() : rect.width();

    if (tickPosition == LSlider::TicksBothSides){
        return (space - ctrlThickness) / 2;
    }
    else if (tickPosition == LSlider::TicksAbove)
        return space - ctrlThickness;
    else{
        return 0;
    }
}

int LStyle::sliderSpaceAvailable(const LOrientation orient, const LRect<int>& rect, const LWidgetStyle* ele) const
{
    if (orient == LOrientation::Horizontal){
        return rect.width() - get(&LStylePixelMetric::sliderLength, ele);
    }else{
        return rect.height() - get(&LStylePixelMetric::sliderLength, ele);
    }
}

LRect<int> LStyle::sliderGrooveRect(const LOrientation orient,
                                    const LStyleOptComplex& opt,
                                    const uint8_t tickPosition,
                                    int ctrlThickness,
                                    int tickOffset) const
{
    if(ctrlThickness <0){
        ctrlThickness = sliderCtrlThickness(orient, opt.rect, tickPosition, opt.elementStyle);
    }
    if(tickOffset < 0){
        tickOffset = sliderTickOffset(orient, opt.rect, tickPosition, ctrlThickness, opt.elementStyle);
    }

    LRect<int> ret;
    if (orient == LOrientation::Horizontal){
        ret = LRect<int>(opt.rect.m_x, opt.rect.m_y + tickOffset, opt.rect.width(), ctrlThickness, false);
    }else{
        ret = LRect<int>(opt.rect.m_x + tickOffset, opt.rect.m_y, ctrlThickness, opt.rect.height(), false);
    }
    return LDrawUtil::visualRect(opt.direction, opt.rect, ret);
}

LRect<int> LStyle::sliderHandleRect(const LSliderData& f,
                                    const LStyleOptComplex& opt,
                                    const uint8_t tickPosition,
                                    int ctrlThickness,
                                    int tickOffset) const
{
    if(ctrlThickness <0){
        ctrlThickness = sliderCtrlThickness(f.m_orientation, opt.rect, tickPosition, opt.elementStyle);
    }
    if(tickOffset < 0){
        tickOffset = sliderTickOffset(f.m_orientation, opt.rect, tickPosition, ctrlThickness, opt.elementStyle);
    }

    int sliderPos = 0;
    int len = get(&LStylePixelMetric::sliderLength, opt.elementStyle);
    bool horizontal = f.m_orientation == LOrientation::Horizontal;
    sliderPos = sliderPositionFromValue(f.m_minimum,
                                        f.m_maximum,
                                        f.m_position,
                                        (horizontal ? opt.rect.width()
                                                    : opt.rect.height()) - len,
                                        f.upsideDown(opt.direction));
    LRect<int> ret;
    if (horizontal){
        ret = LRect<int>(opt.rect.m_x + sliderPos, opt.rect.m_y + tickOffset, len, ctrlThickness, false);
    }else{
        ret = LRect<int>(opt.rect.m_x + tickOffset, opt.rect.m_y + sliderPos, ctrlThickness, len, false);
    }
    return LDrawUtil::visualRect(opt.direction, opt.rect, ret);
}

LRect<int> LStyle::sliderSubCtrlRect(const LSliderData &f,
                                     const LStyleOptComplex &opt,
                                     LUiSubCtrl sc,
                                     const uint8_t tickPosition,
                                     int ctrlThickness,
                                     int tickOffset) const
{
    if(sc == 1<< (uint8_t)LUiSubId::Slider::Handle){
        return sliderHandleRect(f, opt, tickPosition, ctrlThickness, tickOffset);
    }else if(sc == 1<<((uint8_t)LUiSubId::Slider::Groove)){
        return sliderGrooveRect(f.m_orientation, opt, tickPosition, ctrlThickness, tickOffset);
    }else{
        return LDrawUtil::visualRect(opt.direction, opt.rect, LRect<int>());
    }
}

int LStyle::scrollMaxLen(const LSliderData &f, const LStyleOptComplex &opt, int sbextent) const
{
    if(sbextent < 0){
        sbextent = get(&LStylePixelMetric::scrollBarExtent, opt.elementStyle);
    }
    return ((f.m_orientation == LOrientation::Horizontal) ?
                opt.rect.width() : opt.rect.height()) - (sbextent * 2);
}

int LStyle::scrollSliderLen(const LSliderData &f, const LStyleOptComplex &opt, int sbextent, int maxLen) const
{
    if(sbextent < 0){
        sbextent = get(&LStylePixelMetric::scrollBarExtent, opt.elementStyle);
    }

    if(maxLen <0){
        maxLen = scrollMaxLen(f, opt, sbextent);
    }

    int sliderLen =0;
    // calculate slider length
    if (f.m_maximum != f.m_minimum) {
        int range = f.m_maximum - f.m_minimum;
        sliderLen = (int64_t(f.m_pageStep) * maxLen) / (range + f.m_pageStep);

        int slidermin = get(&LStylePixelMetric::scrollBarSliderMin);
        if ((sliderLen < slidermin) || (range > INT_MAX / 2)){
            sliderLen = slidermin;
        }
        if (sliderLen > maxLen){
            sliderLen = maxLen;
        }
    } else {
        sliderLen = maxLen;
    }
    return sliderLen;

}

int LStyle::scrollSliderStart(const LSliderData &f, const LStyleOptComplex &opt, int sbextent, int maxLen, int sliderLen) const
{
    if(sbextent < 0){
        sbextent = get(&LStylePixelMetric::scrollBarExtent, opt.elementStyle);
    }
    if(maxLen <0){
        maxLen = scrollMaxLen(f, opt, sbextent);
    }
    if(sliderLen < 0){
        sliderLen = scrollSliderLen(f, opt, sbextent, maxLen);
    }
    return sbextent + sliderPositionFromValue(f.m_minimum,
                                              f.m_maximum,
                                              f.m_position,
                                              maxLen - sliderLen,
                                              f.upsideDown(opt.direction));
}

LRect<int> LStyle::scrollSubCtrlRect(const LSliderData &f,
                                     const LStyleOptComplex &opt,
                                     LUiSubCtrl sc,
                                     int sbextent,
                                     int maxLen,
                                     int sliderLen,
                                     int sliderStart) const
{
    if(sbextent < 0){
        sbextent = get(&LStylePixelMetric::scrollBarExtent, opt.elementStyle);
    }
    if(maxLen <0){
        maxLen = scrollMaxLen(f, opt, sbextent);
    }
    if(sliderLen < 0){
        sliderLen = scrollSliderLen(f, opt, sbextent, maxLen);
    }
    if(sliderStart <0){
        sliderStart = scrollSliderStart(f, opt, sbextent, maxLen, sliderLen);
    }

    LRect<int> ret;
    switch (sc.to_ulong()) {
    case (1<<(uint8_t)LUiSubId::ScrollBar::SubLine):{            // top/left button
        if (f.m_orientation == LOrientation::Horizontal) {
            int buttonWidth = std::min<int>(opt.rect.width() / 2, sbextent);
            ret.setRect(0, 0, buttonWidth, opt.rect.height());
        } else {
            int buttonHeight = std::min<int>(opt.rect.height() / 2, sbextent);
            ret.setRect(0, 0, opt.rect.width(), buttonHeight);
        }
        break;
    }
    case (1<<(uint8_t)LUiSubId::ScrollBar::AddLine):{            // bottom/right button
        if (f.m_orientation == LOrientation::Horizontal) {
            int buttonWidth = std::min<int>(opt.rect.width()/2, sbextent);
            ret.setRect(opt.rect.width() - buttonWidth, 0, buttonWidth, opt.rect.height());
        } else {
            int buttonHeight = std::min<int>(opt.rect.height()/2, sbextent);
            ret.setRect(0, opt.rect.height() - buttonHeight, opt.rect.width(), buttonHeight);
        }
        break;
    }
    case (1<<(uint8_t)LUiSubId::ScrollBar::SubPage):{            // between top/left button and slider
        if (f.m_orientation == LOrientation::Horizontal){
            ret.setRect(sbextent, 0, sliderStart - sbextent, opt.rect.height());
        }else{
            ret.setRect(0, sbextent, opt.rect.width(), sliderStart - sbextent);
        }
        break;
    }
    case (1<<(uint8_t)LUiSubId::ScrollBar::AddPage):{            // between bottom/right button and slider
        if (f.m_orientation == LOrientation::Horizontal){
            ret.setRect(sliderStart + sliderLen, 0,
                        maxLen - sliderStart - sliderLen + sbextent, opt.rect.height());
        }else{
            ret.setRect(0, sliderStart + sliderLen, opt.rect.width(),
                        maxLen - sliderStart - sliderLen + sbextent);
        }
        break;
    }
    case (1<<(uint8_t)LUiSubId::ScrollBar::Slider):{
        if (f.m_orientation == LOrientation::Horizontal){
            ret.setRect(sliderStart, 0, sliderLen, opt.rect.height());
        }else{
            ret.setRect(0, sliderStart, opt.rect.width(), sliderLen);
        }
        break;
    }
    case (1<<(uint8_t)LUiSubId::ScrollBar::Groove):{
        if (f.m_orientation == LOrientation::Horizontal){
            ret.setRect(sbextent, 0, opt.rect.width() - sbextent * 2,
                        opt.rect.height());
        }else{
            ret.setRect(0, sbextent, opt.rect.width(),
                        opt.rect.height() - sbextent * 2);
        }
        break;
    }
    default:{
        break;
    }
    }
    ret.translate(opt.rect.m_x, opt.rect.m_y);
    return LDrawUtil::visualRect(opt.direction, opt.rect, ret);
}

LRect<int> LStyle::spinBoxSubCtrlRect(const LStyleOptComplex &opt, LUiSubCtrl sc, int editButtonLen, bool isSpinButtonSameSide)
{
    LRect<int> ret;
    switch (sc.to_ulong()) {
    case (1<<(uint8_t)LUiSubId::SpinBox::Up):{
        if(isSpinButtonSameSide){
            ret.setRect(opt.rect.width()-editButtonLen, 0, editButtonLen, opt.rect.height()/2);
        }else{
            ret.setRect(0, 0, editButtonLen, opt.rect.height());
        }
        break;
    }
    case (1<<(uint8_t)LUiSubId::SpinBox::Down):{
        if(isSpinButtonSameSide){
            ret.setRect(opt.rect.width()-editButtonLen, opt.rect.height()/2, editButtonLen, opt.rect.height()/2);
        }else{
            ret.setRect(opt.rect.width()-editButtonLen, 0, editButtonLen, opt.rect.height());
        }
        break;
    }
    case (1<<(uint8_t)LUiSubId::SpinBox::EditField):{
        if(isSpinButtonSameSide){
            ret.setRect(0, 0, opt.rect.width()-editButtonLen, opt.rect.height());
        }else{
            ret.setRect(editButtonLen, 0, opt.rect.width()-2*editButtonLen, opt.rect.height());
        }
        break;
    }
    default:{
        break;
    }
    }

    ret.translate(opt.rect.m_x, opt.rect.m_y);
    return LDrawUtil::visualRect(opt.direction, opt.rect, ret);
}

LRect<int> LStyle::comboEditSubCtrlRect(const LStyleOptComplex &opt, LUiSubCtrl sc, int editButtonLen)
{
    LRect<int> ret;
    switch (sc.to_ulong()) {
    case (1<<(uint8_t)LUiSubId::ComboBox::Arrow):{
        ret.setRect(opt.rect.width()-editButtonLen, 0, editButtonLen, opt.rect.height());
        break;
    }
    case (1<<(uint8_t)LUiSubId::ComboBox::EditField):{
        ret.setRect(0, 0, opt.rect.width()-editButtonLen, opt.rect.height());
        break;
    }
    case (1<<(uint8_t)LUiSubId::ComboBox::ListBoxPopup):{
        return LRect<int>();
    }
    default:{
        break;
    }
    }

    ret.translate(opt.rect.m_x, opt.rect.m_y);
    return LDrawUtil::visualRect(opt.direction, opt.rect, ret);
}

LRect<int> LStyle::groupBoxSubCtrlRect(const LStyleOptGroupBox &opt, LUiSubCtrl sc, const std::wstring& title)
{
    LRect<int> ret;
    switch (sc.to_ulong()) {
    case (1<<(uint8_t)LUiSubId::GroupBox::Frame):
    case (1<<(uint8_t)LUiSubId::GroupBox::Contents): {
        int topLine = 0;
        bool hasCheckBox = opt.subCtrls.test((uint8_t)LUiSubId::GroupBox::CheckBox);
        LFontMetrics fontMetrics(opt.font);
        int topHeight = fontMetrics.height();
        if(hasCheckBox){
            int checkBoxHeight = get(&LStylePixelMetric::smallIconSize, opt.elementStyle);
            topHeight = std::max<int>(topHeight, checkBoxHeight);
        }

        if (sc == (1<<(uint8_t)LUiSubId::GroupBox::Frame)) {
            if ((opt.titleAlignment & LAlignment::VCenter) == LAlignment::VCenter){
                topLine += topHeight/2;
            }
            else if ((opt.titleAlignment & LAlignment::Bottom) == LAlignment::Bottom){
                topLine += topHeight;
            }

            LRect<int> frameRect = opt.rect;
            frameRect.m_y += topLine;
            frameRect.m_height -= topLine;

            LMargins<double> bounds = boundWidths(LSizePolicy::GroupBox, LRenderRule::Margin, opt.elementStyle);
            ret = frameRect.marginsRemoved(bounds.toMargin());
        }else{
            LRect<int> frameRect = opt.rect;
            frameRect.m_y += topHeight;
            frameRect.m_height -= topHeight;

            LMargins<double> bounds = boundWidths(LSizePolicy::GroupBox, LRenderRule::StyleAll, opt.elementStyle);
            ret = frameRect.marginsRemoved(bounds.toMargin());
        }
        break;
    }
    case (1<<(uint8_t)LUiSubId::GroupBox::CheckBox):
    case (1<<(uint8_t)LUiSubId::GroupBox::Label): {
        LFontMetrics fontMetrics(opt.font);
        int th = fontMetrics.height();
        int tw = fontMetrics.size((int)LTextFlag::ShowMnemonic, title).width();
        ret = opt.rect;

        int indicatorWidth = get(&LStylePixelMetric::smallIconSize, opt.elementStyle);
        int indicatorHeight = indicatorWidth;
        int indicatorSpace = get(&LStylePixelMetric::itemMarkSpan, opt.elementStyle);
        bool hasCheckBox = opt.subCtrls.test((uint8_t)LUiSubId::GroupBox::CheckBox);
        int checkBoxWidth = hasCheckBox ? (indicatorWidth + indicatorSpace) : 0;
        int checkBoxHeight = hasCheckBox ? indicatorHeight : 0;

        int h = std::max<int>(th, checkBoxHeight);
        ret.setHeight(h);

        int left = boundWidth(LSizePolicy::GroupBox, LDirection::ToLeft, LRenderRule::StyleAll, opt.elementStyle);
        ret.m_x += left;
        ret.m_width -= left;

        int top = boundWidth(LSizePolicy::GroupBox, LDirection::ToTop, LRenderRule::Margin, opt.elementStyle);
        ret.m_y += top;
        ret.m_height -= top;

        // Adjusted rect for label + indicatorWidth + indicatorSpace
        LRect<int> totalRect = LDrawUtil::alignedRect(opt.direction, opt.titleAlignment, LSize<int>(tw + checkBoxWidth, h), ret);

        // Adjust totalRect if checkbox is set
        if (hasCheckBox) {
            bool ltr = opt.direction == LLayoutDirection::LeftToRight;
            int left = 0;
            // Adjust for check box
            if (sc == (1<<(uint8_t)LUiSubId::GroupBox::CheckBox)) {
                left = ltr ? totalRect.m_x : (totalRect.x2() - indicatorWidth);
                int top = totalRect.m_y + (h - checkBoxHeight) / 2;
                totalRect.setRect(left, top, indicatorWidth, indicatorHeight);
                // Adjust for label
            } else {
                left = ltr ? (totalRect.m_x + checkBoxWidth - 2) : totalRect.m_x;
                int top = totalRect.m_y + (h - th) / 2;
                totalRect.setRect(left, top, totalRect.width() - checkBoxWidth, th);
            }
        }
        ret = totalRect;
        break;
    }
    default:
        break;
    }

    return ret;
}

LUiSubCtrl LStyle::hitTestSpinBox(const LStyleOptComplex &opt, bool isButtonSameSide, const LPoint<int> &pt)
{
    int editButtonLen = get(&LStylePixelMetric::editButtonLength, opt.elementStyle);
    LUiSubCtrl sc;
    for(uint8_t subId=0; subId <= (uint8_t)(LUiSubId::SpinBox::EditField); ++subId){
        LRect<int> rect = spinBoxSubCtrlRect(opt, 1<<subId, editButtonLen, isButtonSameSide);
        if (rect.isValid() && rect.contains(pt)) {
            sc.set(subId, true);
            break;
        }
    }
    return sc;
}

LUiSubCtrl LStyle::hitTestComboEdit(const LStyleOptComplex &opt, const LPoint<int> &pt)
{
    int editButtonLen = get(&LStylePixelMetric::editButtonLength, opt.elementStyle);
    LUiSubCtrl sc;
    for(uint8_t subId=0; subId <= (uint8_t)(LUiSubId::ComboBox::EditField); ++subId){
        LRect<int> rect = comboEditSubCtrlRect(opt, 1<<subId, editButtonLen);
        if (rect.isValid() && rect.contains(pt)) {
            sc.set(subId, true);
            break;
        }
    }
    return sc;
}

LUiSubCtrl LStyle::hitTestGroupBox(const LStyleOptGroupBox &opt, const LPoint<int> &pt, const std::wstring& title)
{
    LUiSubCtrl sc;
    for(uint8_t subId=0; subId <= (uint8_t)(LUiSubId::GroupBox::Label); ++subId){
        LRect<int> rect = groupBoxSubCtrlRect(opt, 1<<subId, title);
        if (rect.isValid() && rect.contains(pt)) {
            sc.set(subId, true);
            break;
        }
    }
    return sc;
}

LUiSubCtrl LStyle::hitTestScroll(LSliderData &f, const LStyleOptComplex &opt, const LPoint<int> &pt) const
{
    LUiSubCtrl sc;

    int sbextent = get(&LStylePixelMetric::scrollBarExtent, opt.elementStyle);
    int maxLen = scrollMaxLen(f, opt, sbextent);
    int sliderLen = scrollSliderLen(f, opt, sbextent, maxLen);
    int sliderStart = scrollSliderStart(f, opt, sbextent, maxLen, sliderLen);

    for(uint8_t subId=0; subId <= (uint8_t)(LUiSubId::ScrollBar::Groove); ++subId){
        LRect<int> rect = scrollSubCtrlRect(f, opt, 1<<subId,
                                            sbextent, maxLen, sliderLen, sliderStart);
        if (rect.isValid() && rect.contains(pt)) {
            sc.set(subId, true);
            break;
        }
    }
    return sc;
}

LUiElementType LStyle::scrollSubCtrlUiElement(uint8_t scId, LOrientation orient)
{
    switch (scId) {
    case (uint8_t)LUiSubId::ScrollBar::SubLine:{            // top/left button
        if (orient == LOrientation::Horizontal) {
            return LUiElementType::LeftArrow;
        } else {
            return LUiElementType::UpArrow;
        }
    }
    case (uint8_t)LUiSubId::ScrollBar::AddLine:{            // bottom/right button
        if (orient == LOrientation::Horizontal) {
            return LUiElementType::RightArrow;
        } else {
            return LUiElementType::DownArrow;
        }
    }
    case (uint8_t)LUiSubId::ScrollBar::SubPage:{            // between top/left button and slider
        return LUiElementType::SubPage;
    }
    case (uint8_t)LUiSubId::ScrollBar::AddPage:{            // between bottom/right button and slider
        return LUiElementType::AddPage;
    }
    case (uint8_t)LUiSubId::ScrollBar::Slider:{
        return LUiElementType::Handle;
    }
    case (uint8_t)LUiSubId::ScrollBar::Groove:{
        return LUiElementType::Groove;
    }
    default:{
        return LUiElementType::None;
    }
    }
}

LUiElementType LStyle::spinBoxSubCtrlUiElement(uint8_t scId)
{
    switch (scId) {
    case (uint8_t)LUiSubId::SpinBox::Up:{            // between top/left button and slider
        return LUiElementType::UpButton;
    }
    case (uint8_t)LUiSubId::SpinBox::Down:{            // between bottom/right button and slider
        return LUiElementType::DownButton;
    }
    case (uint8_t)LUiSubId::SpinBox::EditField:{
        return LUiElementType::Text;
    }
    default:{
        return LUiElementType::None;
    }
    }
}

LUiElementType LStyle::comboEditSubCtrlUiElement(uint8_t scId)
{
    switch (scId) {
    case (uint8_t)LUiSubId::ComboBox::Arrow:{            // between bottom/right button and slider
        return LUiElementType::DownButton;
    }
    case (uint8_t)LUiSubId::ComboBox::EditField:{
        return LUiElementType::Text;
    }
    case (uint8_t)LUiSubId::ComboBox::ListBoxPopup:{
        return LUiElementType::DropDown;
    }
    default:{
        return LUiElementType::None;
    }
    }
}

int LStyle::sliderPositionFromValue(int min, int max, int logicalValue, int span, bool upsideDown)
{
    if (span <= 0 || logicalValue < min || max <= min){
        return 0;
    }
    if (logicalValue > max){
        return upsideDown ? span : min;
    }

    uint32_t range = max - min;
    uint32_t p = upsideDown ? max - logicalValue : logicalValue - min;

    if (range > (uint32_t)INT_MAX/4096) {
        double dpos = (double(p))/(double(range)/span);
        return int(dpos);
    } else if (range > (uint32_t)span) {
        return (2 * p * span + range) / (2*range);
    } else {
        uint32_t div = span / range;
        uint32_t mod = span % range;
        return p * div + (2 * p * mod + range) / (2 * range);
    }
}

int LStyle::sliderValueFromPosition(int min, int max, int pos, int span, bool upsideDown)
{
    if (span <= 0 || pos <= 0){
        return upsideDown ? max : min;
    }
    if (pos >= span){
        return upsideDown ? min : max;
    }

    uint32_t range = max - min;

    if ((uint32_t)span > range) {
        int tmp = (2 * pos * range + span) / (2 * span);
        return upsideDown ? max - tmp : tmp + min;
    } else {
        uint32_t div = range / span;
        uint32_t mod = range % span;
        int tmp = pos * div + (2 * pos * mod + span) / (2 * span);
        return upsideDown ? max - tmp : tmp + min;
    }
}

std::shared_ptr<LPainterPath> LStyle::markPath(const LRect<double> &rect, LDrawMark mark)
{
    std::shared_ptr<LPainterPath> ret = std::make_shared<LPainterPath>();
    switch (mark) {
    case LDrawMark::Left:{
        double w = rect.m_width*0.5;
        LRect<double> t(rect.m_x+(rect.m_width-w)/2, rect.m_y, w, rect.m_height, false);
        ret->moveTo(t.topRight());
        ret->lineTo(LPoint<double>(t.m_x, t.m_y + t.m_height/2));
        ret->lineTo(t.bottomRight());
        break;
    }
    case LDrawMark::Right:{
        double w = rect.m_width*0.5;
        LRect<double> t(rect.m_x+(rect.m_width-w)/2, rect.m_y, w, rect.m_height, false);
        ret->moveTo(t.topLeft());
        ret->lineTo(LPoint<double>(t.m_x+t.m_width, t.m_y + t.m_height/2));
        ret->lineTo(t.bottomLeft());
        break;
    }
    case LDrawMark::Up:{
        double h = rect.m_height*0.5;
        LRect<double> t(rect.m_x, rect.m_y+(rect.m_height-h)/2, rect.m_width, h, false);
        ret->moveTo(t.bottomLeft());
        ret->lineTo(LPoint<double>(t.m_x+t.m_width/2, t.m_y));
        ret->lineTo(t.bottomRight());
        break;
    }
    case LDrawMark::Down:{
        double h = rect.m_height*0.5;
        LRect<double> t(rect.m_x, rect.m_y+(rect.m_height-h)/2, rect.m_width, h, false);
        ret->moveTo(t.topLeft());
        ret->lineTo(LPoint<double>(t.m_x+t.m_width/2, t.m_y + t.m_height));
        ret->lineTo(t.topRight());
        break;
    }
    case LDrawMark::RightBottom:{
        ret->moveTo(rect.bottomLeft());
        ret->lineTo(rect.topRight());
        ret->lineTo(rect.bottomRight());
        ret->lineTo(rect.bottomLeft());
        break;
    }
    case LDrawMark::LeftBottom:{
        ret->moveTo(rect.topLeft());
        ret->lineTo(rect.bottomRight());
        ret->lineTo(rect.bottomLeft());
        ret->lineTo(rect.topLeft());
        break;
    }
    case LDrawMark::LeftUp:{
        ret->moveTo(rect.topLeft());
        ret->lineTo(rect.topRight());
        ret->lineTo(rect.bottomLeft());
        ret->lineTo(rect.topLeft());
        break;
    }
    case LDrawMark::RightUp:{
        ret->moveTo(rect.topLeft());
        ret->lineTo(rect.topRight());
        ret->lineTo(rect.bottomRight());
        ret->lineTo(rect.topLeft());
        break;
    }
    case LDrawMark::Checked:{
        ret->moveTo(LPoint<double>(rect.m_x, rect.m_y + rect.m_height/2));
        ret->lineTo(LPoint<double>(rect.m_x+rect.m_width/2, rect.m_y + rect.m_height));
        ret->lineTo(LPoint<double>(rect.m_x+rect.m_width, rect.m_y));
        break;
    }
    case LDrawMark::Crossed:{
        ret->moveTo(rect.topLeft());
        ret->lineTo(rect.bottomRight());
        ret->moveTo(rect.topRight());
        ret->lineTo(rect.bottomLeft());
        break;
    }
    case LDrawMark::CheckedBox:{
        ret->addRect(rect);
        ret->moveTo(LPoint<double>(rect.m_x, rect.m_y + rect.m_height/2));
        ret->lineTo(LPoint<double>(rect.m_x+rect.m_width/2, rect.m_y + rect.m_height));
        ret->lineTo(LPoint<double>(rect.m_x+rect.m_width, rect.m_y));
        break;
    }
    case LDrawMark::UncheckedBox:{
        ret->addRect(rect);
        break;
    }
    case LDrawMark::CrossedBox:{
        ret->addRect(rect);
        ret->moveTo(rect.topLeft());
        ret->lineTo(rect.bottomRight());
        ret->moveTo(rect.topRight());
        ret->lineTo(rect.bottomLeft());
        break;
    }
    case LDrawMark::CheckedRadio:{
        ret->addEllipse(rect);
        ret->addEllipse(rect.marginsRemoved(LMargins<double>(rect.m_width/4, rect.m_width/4, rect.m_height/4, rect.m_height/4)));
        break;
    }
    case LDrawMark::UncheckedRadio:{
        ret->addEllipse(rect);
        break;
    }
    default:{
        break;
    }
    }
    return ret;
}

void LStyle::drawMark(LPainter *p, const LRect<double> &rect, LDrawMark mark, const LWidgetStyle* ele, uint16_t state)
{
    if(ele != nullptr){
        if(state == (uint16_t)LUiState::None){
            state = markState(mark);
        }
        LUiElementType type = markElementType(mark);
        bool hasColor = false;
        std::shared_ptr<LRenderRule> rule = ele->getRenderRule(type, state);
        if(rule != nullptr){
            if(rule->hasImage()){
                rule->drawImage(p, rect.toRect());
                return;
            }else{
                hasColor = rule->applyForeground(p) & 0x01;
            }
        }


        if(!hasColor){
            std::shared_ptr<LWidget> w = ele->widget();
            if(w != nullptr){
                std::shared_ptr<LPalette> pal = w->palette();
                if(state & (uint16_t)LUiState::Disabled){
                    p->setPen(pal->brush(LPalette::Disabled, w->foregroundRole())->m_color);
                }else if(state & ((uint16_t)LUiState::Hover|(uint16_t)LUiState::Sunken)){
                    p->setPen(pal->brush(LPalette::Highlight)->m_color);
                }else{
                    p->setPen(pal->brush(w->foregroundRole())->m_color);
                }
            }
        }
    }

    // Windows 11 风格复选框 / 单选钮外观
    switch(mark){
    case LDrawMark::UncheckedBox:
    case LDrawMark::CheckedBox:
    case LDrawMark::UncheckedRadio:
    case LDrawMark::CheckedRadio:{
        std::shared_ptr<LWidget> w = (ele != nullptr)? ele->widget() : nullptr;
        if(w == nullptr){
            break; // 无调色板来源时退回通用绘制
        }
        std::shared_ptr<LPalette> pal = w->palette();
        const bool disabled = state & (uint16_t)LUiState::Disabled;
        const LColor accent = pal->brush(LPalette::Highlight)->m_color;
        LColor borderColor = disabled ? pal->brush(LPalette::Mid)->m_color
                                      : pal->brush(LPalette::Shadow)->m_color;
        LColor fillColor = disabled ? pal->brush(LPalette::Disabled, LPalette::Base)->m_color
                                    : pal->brush(LPalette::Base)->m_color;

        p->save();
        p->setRenderHints(LRenderHint::Antialiasing, true);

        // 标记框必须是正方形，否则会被拉成椭圆
        const double side = std::min<double>(rect.m_width, rect.m_height);
        LRect<double> box(rect.m_x + (rect.m_width - side) / 2,
                          rect.m_y + (rect.m_height - side) / 2,
                          side, side, false);
        const double r = std::min<double>(1.6, side * 0.12);
        const LColor markColor = disabled
                ? pal->brush(LPalette::Disabled, w->foregroundRole())->m_color
                : accent;

        switch(mark){
        case LDrawMark::UncheckedBox:{
            p->setBrush(fillColor, LBrush::SolidPattern);
            p->setPen(borderColor, LPenStyle::Solid, 1);
            p->drawRoundedRect(box, r, r, LSizeMode::Absolute);
            break;
        }
        case LDrawMark::CheckedBox:{
            p->setBrush(markColor, LBrush::SolidPattern);
            p->setPen(markColor, LPenStyle::Solid, 1);
            p->drawRoundedRect(box, r, r, LSizeMode::Absolute);
            // 白色对勾
            LRect<double> cr = box.marginsRemoved(LMargins<double>(side*0.2, side*0.2,
                                                                   side*0.2, side*0.2));
            p->setBrush(LBrush::NoPattern);
            p->setPen(LColor(255, 255, 255), LPenStyle::Solid,
                      std::max(1.5, side * 0.12));
            p->drawPath(markPath(cr, LDrawMark::Checked));
            break;
        }
        case LDrawMark::UncheckedRadio:{
            p->setBrush(fillColor, LBrush::SolidPattern);
            p->setPen(borderColor, LPenStyle::Solid, 1);
            p->drawEllipse(box);
            break;
        }
        case LDrawMark::CheckedRadio:{
            // 白底 + 强调色圆环 + 中心圆点
            p->setBrush(fillColor, LBrush::SolidPattern);
            p->setPen(markColor, LPenStyle::Solid, std::max(1.5, side * 0.12));
            p->drawEllipse(box);
            LRect<double> dot = box.marginsRemoved(LMargins<double>(side*0.3, side*0.3,
                                                                    side*0.3, side*0.3));
            p->setBrush(markColor, LBrush::SolidPattern);
            p->setPen(markColor, LPenStyle::Solid, 1);
            p->drawEllipse(dot);
            break;
        }
        default:{
            break;
        }
        }
        p->restore();
        return;
    }
    default:{
        break;
    }
    }

    // 方向箭头使用细线 chevron，比实心三角形更精致
    if(mark == LDrawMark::Up || mark == LDrawMark::Down
        || mark == LDrawMark::Left || mark == LDrawMark::Right){
        std::shared_ptr<LPen> curPen = p->pen();
        const LColor color = (curPen != nullptr) ? curPen->color() : LColor(90, 90, 90);

        // 矮小的 V 形箭头：上下箭头偏扁，左右箭头偏高，保证尖角接近直角
        const bool isLeftRight = (mark == LDrawMark::Left || mark == LDrawMark::Right);
        double s = isLeftRight ? std::min<double>(rect.m_height, 18.0)
                               : std::min<double>(rect.m_width, rect.m_height);
        s *= 0.72;

        const double cx = rect.m_x + rect.m_width * 0.5;
        const double cy = rect.m_y + rect.m_height * 0.5;
        // 水平/垂直半跨度
        const double hx = s * (isLeftRight ? 0.17 : 0.30);
        const double hy = s * (isLeftRight ? 0.30 : 0.13);

        std::shared_ptr<LPainterPath> chevron = std::make_shared<LPainterPath>();
        switch(mark){
        case LDrawMark::Down:{
            chevron->moveTo(cx - hx, cy - hy);
            chevron->lineTo(cx, cy + hy);
            chevron->lineTo(cx + hx, cy - hy);
            break;
        }
        case LDrawMark::Up:{
            chevron->moveTo(cx - hx, cy + hy);
            chevron->lineTo(cx, cy - hy);
            chevron->lineTo(cx + hx, cy + hy);
            break;
        }
        case LDrawMark::Right:{
            chevron->moveTo(cx - hx, cy - hy);
            chevron->lineTo(cx + hx, cy);
            chevron->lineTo(cx - hx, cy + hy);
            break;
        }
        default:{
            chevron->moveTo(cx + hx, cy - hy);
            chevron->lineTo(cx - hx, cy);
            chevron->lineTo(cx + hx, cy + hy);
            break;
        }
        }

        p->save();
        p->setRenderHints(LRenderHint::Antialiasing, true);
        p->setBrush(LBrush::NoPattern);
        p->setPen(std::make_shared<LPen>(color, std::max(1.3, s * 0.12),
                                         LPenStyle::Solid,
                                         LPenCapStyle::Round, LPenJoinStyle::Round));
        p->drawPath(chevron);
        p->restore();
        return;
    }

    if(mark != LDrawMark::None){
        double w = std::min<double>(rect.m_width, rect.m_height)*0.6;
        LRect<double> t(rect.m_x+(rect.m_width-w)/2, rect.m_y+(rect.m_height-w)/2, w, w, false);
        std::shared_ptr<LPainterPath> path = markPath(t, mark);
        p->drawPath(path);
    }
}


uint16_t LStyle::markState(LDrawMark mark){
    switch(mark){
    case LDrawMark::Checked:
    case LDrawMark::CheckedBox:
    case LDrawMark::CheckedRadio:{
        return uint16_t(LUiState::Checked);
    }
    default:{
        return uint16_t(LUiState::None);
    }
    }
}

LUiElementType LStyle::markElementType(LDrawMark mark)
{
    switch(mark){
    case LDrawMark::Left:{
        return LUiElementType::LeftArrow;
    }
    case LDrawMark::Right:{
        return LUiElementType::RightArrow;
    }
    case LDrawMark::Up:{
        return LUiElementType::UpArrow;
    }
    case LDrawMark::Down:{
        return LUiElementType::DownArrow;
    }
    case LDrawMark::Checked:
    case LDrawMark::Crossed:
    case LDrawMark::CheckedBox:
    case LDrawMark::UncheckedBox:
    case LDrawMark::CrossedBox:
    case LDrawMark::CheckedRadio:
    case LDrawMark::UncheckedRadio:{
        return LUiElementType::Indicator;
    }
    default:{
        return LUiElementType::None;
    }
    }
}

double LStyle::boundWidth(LSizePolicy::ControlType type,  LDirection dir, int flags, const LWidgetStyle* ele)
{
    double ret =0;
    if(ele != nullptr){
        std::shared_ptr<LRenderRule> rule = ele->getRenderRule();
        if(rule != nullptr){
            ret += rule->boundWidth(dir, flags& (LRenderRule::Margin|LRenderRule::Padding|LRenderRule::Border));
            if(rule->hasBox()){
                flags &= ~int(LRenderRule::Margin|LRenderRule::Padding);
            }

            if(rule->hasBorder()){
                flags &= ~int(LRenderRule::Border);
            }

            if(flags ==0){
                return ret;
            }
        }
    }

    switch(type){
    case LSizePolicy::Menu:{
        if(flags & LRenderRule::Margin){
            if(dir == LDirection::ToLeft || dir == LDirection::ToRight){
                ret+= m_boundMetric.menuHMargin;
            }else{
                ret+= m_boundMetric.menuVMargin;
            }
        }
        if(flags & LRenderRule::Border){
            ret+= m_boundMetric.menuBorderWidth;
        }
        break;
    }
    case LSizePolicy::MenuBar:{
        if(flags & LRenderRule::Margin){
            if(dir == LDirection::ToLeft || dir == LDirection::ToRight){
                ret+= m_boundMetric.menuBarHMargin;
            }else{
                ret+= m_boundMetric.menuBarVMargin;
            }
        }
        if(flags & LRenderRule::Border){
            ret+= m_boundMetric.menuBarBorderWidth;
        }
        break;
    }
    case LSizePolicy::Edit:
    case LSizePolicy::LineEdit:{
        if(flags & LRenderRule::Margin){
            if(dir == LDirection::ToLeft || dir == LDirection::ToRight){
                ret+= m_boundMetric.editHMargin;
            }else{
                ret+= m_boundMetric.editVMargin;
            }
        }
        break;
    }
    case LSizePolicy::GroupBox:{
        if(flags & LRenderRule::Margin){
            if(dir == LDirection::ToLeft || dir == LDirection::ToRight){
                ret+= m_boundMetric.groupBoxVMargin;
            }else{
                ret+= m_boundMetric.groupBoxHMargin;
            }
        }

        if(flags & LRenderRule::Padding){
            switch(dir){
            case LDirection::ToTop:{
                ret+= m_boundMetric.groupBoxTopPadding;
                break;
            }
            case LDirection::ToRight:{
                ret+= m_boundMetric.groupBoxRightPadding;
                break;
            }
            case LDirection::ToBottom:{
                ret+= m_boundMetric.groupBoxBottomPadding;
                break;
            }
            default:{
                ret+= m_boundMetric.groupBoxLeftPadding;
                break;
            }
            }
        }
        break;
    }
    default:{
        break;
    }
    }

    return ret;
}

LMargins<double> LStyle::boundWidths(LSizePolicy::ControlType type, int flags, const LWidgetStyle *ele)
{
    LMargins<double> ret;
    if(ele != nullptr){
        std::shared_ptr<LRenderRule> rule = ele->getRenderRule();
        if(rule != nullptr){
            ret += rule->boundWidths(flags& (LRenderRule::Margin|LRenderRule::Padding));
            if(rule->hasBox()){
                flags &= ~int(LRenderRule::Margin|LRenderRule::Padding);
            }

            if(rule->hasBorder()){
                flags &= ~int(LRenderRule::Border);
            }

            if(flags ==0){
                return ret;
            }
        }
    }

    switch(type){
    case LSizePolicy::Menu:{
        if(flags & LRenderRule::Margin){
            ret.m_top+=m_boundMetric.menuVMargin;
            ret.m_bottom+=m_boundMetric.menuVMargin;
            ret.m_left+=m_boundMetric.menuHMargin;
            ret.m_right+=m_boundMetric.menuHMargin;
        }
        if(flags & LRenderRule::Border){
            ret+= m_boundMetric.menuBorderWidth;
        }
        break;
    }
    case LSizePolicy::MenuBar:{
        if(flags & LRenderRule::Margin){
            ret.m_top+=m_boundMetric.menuBarVMargin;
            ret.m_bottom+=m_boundMetric.menuBarVMargin;
            ret.m_left+=m_boundMetric.menuBarHMargin;
            ret.m_right+=m_boundMetric.menuBarHMargin;
        }
        if(flags & LRenderRule::Border){
            ret+= m_boundMetric.menuBarBorderWidth;
        }
        break;
    }
    case LSizePolicy::Edit:
    case LSizePolicy::LineEdit:{
        if(flags & LRenderRule::Margin){
            ret.m_top+=m_boundMetric.editVMargin;
            ret.m_bottom+=m_boundMetric.editVMargin;
            ret.m_left+=m_boundMetric.editHMargin;
            ret.m_right+=m_boundMetric.editHMargin;
        }
        break;
    }
    case LSizePolicy::GroupBox:{
        if(flags & LRenderRule::Margin){
            ret.m_top+=m_boundMetric.groupBoxVMargin;
            ret.m_bottom+=m_boundMetric.groupBoxVMargin;
            ret.m_left+=m_boundMetric.groupBoxHMargin;
            ret.m_right+=m_boundMetric.groupBoxHMargin;
        }

        if(flags & LRenderRule::Padding){
            ret.m_top+= m_boundMetric.groupBoxTopPadding;
            ret.m_bottom+=m_boundMetric.groupBoxBottomPadding;
            ret.m_left+=m_boundMetric.groupBoxLeftPadding;
            ret.m_right+=m_boundMetric.groupBoxRightPadding;
        }
        break;
    }
    default:{
        break;
    }
    }

    return ret;
}



LK_NAMESPACE_END
