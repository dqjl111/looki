#include "lkwidgets/widget/lslider.h"
#include "lkwidgets/draw/lstyle.h"
#include "lkwidgets/core/lapplication.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

LK_NAMESPACE_BEGIN

LSlider::LSlider(std::shared_ptr<LWidget> parent, LOrientation orient) : LAbstractSlider(parent, orient)
{
    if (orient == LOrientation::Horizontal){
        m_sizePolicy = (LSizePolicy(LSizePolicy::Minimum, LSizePolicy::Fixed, LSizePolicy::Slider));
    }else{
        m_sizePolicy = (LSizePolicy(LSizePolicy::Fixed, LSizePolicy::Minimum, LSizePolicy::Slider));
    }
}

LSize<int> LSlider::sizeHint() const
{
    const int sliderLength = 84, tickSpace = 5;
    std::shared_ptr<LStyle> pStyle = style();
    int thick = pStyle->get(&LStylePixelMetric::sliderThickness, &m_elementStyle);
    if (m_tickPosition & TicksAbove){
        thick += tickSpace;
    }
    if (m_tickPosition & TicksBelow){
        thick += tickSpace;
    }
    int w = thick, h = sliderLength;
    if (m_orientation == LOrientation::Horizontal) {
        w = sliderLength;
        h = thick;
    }
    return LSize<int>(w, h);
}

LSize<int> LSlider::minimumSizeHint() const
{
    LSize<int> s = sizeHint();
    int length = style()->get(&LStylePixelMetric::sliderLength, &m_elementStyle);
    if (m_orientation == LOrientation::Horizontal){
        s.setWidth(length);
    }
    else{
        s.setHeight(length);
    }
    return s;
}

void LSlider::setTickPosition(TickPosition position)
{
    m_tickPosition = position;
    update();
    updateGeometry();
}

void LSlider::setTickInterval(uint32_t ti)
{
    m_tickInterval = ti;
    update();
}

void LSlider::paintEvent(std::shared_ptr<LPaintEvent> ev)
{
    // 兜底：如果按键已经松开（例如拖出控件后在外面松手，控件收不到 release 事件），
    // 这里清除按下态，避免手柄一直停留在"拖动中"的外观
    if (m_pressedCtrl.any()) {
        bool leftDown = !!(LGuiApplication::mouseButtons() & LMouseButton::Left);
#ifdef _WIN32
        if (leftDown && !(GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
            leftDown = false;
        }
#endif
        if (!leftDown) {
            m_pressedCtrl.reset();
            setSliderDown(false);
            update();
        }
    }

    // 同理：鼠标移出控件后 hover 状态可能残留，用光标位置校正
    if (m_hoverCtrl.any()) {
        bool inside = true;
#ifdef _WIN32
        POINT pt{};
        if (::GetCursorPos(&pt)) {
            inside = rect().contains(mapFromGlobal(LPoint<int>(int(pt.x), int(pt.y))));
        }
#endif
        if (!inside) {
            m_hoverCtrl.reset();
            m_hoverRect = LRect<int>();
            update();
        }
    }

    LPainter painter(paintDevice());

    LStyleOptComplex opt;
    initStyleOpt(opt);
    std::shared_ptr<LStyle> pStyle = style();

    opt.subCtrls = (1 <<(uint8_t)LUiSubId::Slider::Groove) | (1<<(uint8_t)LUiSubId::Slider::Handle);
    if (m_tickPosition != NoTicks){
        opt.subCtrls.set((uint8_t)LUiSubId::Slider::Tickmarks, true);
    }

    if (m_pressedCtrl.any()) {
        opt.activeSubCtrls = m_pressedCtrl;
        opt.state |= LUiState::Sunken;
    } else {
        opt.activeSubCtrls = m_hoverCtrl;
    }

    int thickness = pStyle->sliderCtrlThickness(m_orientation, opt.rect, m_tickPosition, opt.elementStyle);
    int tickOffset = pStyle->sliderTickOffset(m_orientation, opt.rect, m_tickPosition, thickness, opt.elementStyle);
    int len = pStyle->get(&LStylePixelMetric::sliderLength, opt.elementStyle);

    if (opt.subCtrls.test((uint8_t)LUiSubId::Slider::Groove)) {
        LRect<int> groove = pStyle->sliderGrooveRect(m_orientation,
                                             opt,
                                             m_tickPosition,
                                             thickness,
                                             tickOffset);
        if(groove.isValid()){
            LFlags<LUiState > state  = uiElementState(LUiElementType::Groove);
            std::shared_ptr<LRenderRule> rule = m_elementStyle.getRenderRule(LUiElementType::Groove, state);
            LRect<int> handleRect = pStyle->sliderHandleRect(*this, opt, m_tickPosition, thickness, tickOffset);

            if(rule != nullptr && rule->hasDrawable()){
                rule->drawRule(&painter, groove);
            }else{
                int mid = thickness / 2;

                if (m_tickPosition & LSlider::TicksAbove){
                    mid += len / 8;
                }
                if (m_tickPosition & LSlider::TicksBelow){
                    mid -= len / 8;
                }

                // Windows 11 风格：细圆角凹槽，已滑过部分使用强调色
                LRect<double> band = (m_orientation == LOrientation::Horizontal)
                    ? LRect<double>(groove.m_x, groove.m_y + mid - 2, groove.width(), 4, false)
                    : LRect<double>(groove.m_x + mid - 2, groove.m_y, 4, groove.height(), false);

                painter.save();
                painter.setRenderHints(LRenderHint::Antialiasing, true);
                painter.setPen(LPenStyle::No);
                painter.setBrush(opt.palette->brush(LPalette::Mid));
                painter.drawRoundedRect(band, 2, 2, LSizeMode::Absolute);

                if (handleRect.isValid() && isEnabled()) {
                    painter.setBrush(opt.palette->brush(LPalette::Highlight));
                    if (m_orientation == LOrientation::Horizontal) {
                        double w0 = handleRect.center().m_x - band.m_x;
                        if (w0 > 0) {
                            w0 = std::min<double>(w0, band.m_width);
                            painter.drawRoundedRect(LRect<double>(band.m_x, band.m_y, w0, band.m_height, false),
                                                    2, 2, LSizeMode::Absolute);
                        }
                    } else {
                        double h0 = handleRect.center().m_y - band.m_y;
                        if (h0 > 0) {
                            h0 = std::min<double>(h0, band.m_height);
                            painter.drawRoundedRect(LRect<double>(band.m_x, band.m_y, band.m_width, h0, false),
                                                    2, 2, LSizeMode::Absolute);
                        }
                    }
                }
                painter.restore();
            }
        }
    }


    if (opt.subCtrls.test((uint8_t)LUiSubId::Slider::Tickmarks)) {
        int available = pStyle->sliderSpaceAvailable(m_orientation, opt.rect);
        uint32_t interval = m_tickInterval;
        if (interval <= 0) {
            interval = m_singleStep;
            if (pStyle->sliderPositionFromValue(m_minimum, m_maximum, interval, available)
                    - pStyle->sliderPositionFromValue(m_minimum, m_maximum, 0, available) < 3){
                interval = m_pageStep;
            }
        }
        if (interval == 0){
            interval = 1;
        }
        int fudge = len / 2;
        int pos =0;
        // Since there is no subrect for tickmarks do a translation here.
        painter.translate(LPoint<double>(opt.rect.m_x, opt.rect.m_y));

        bool hasColor = false;
        std::shared_ptr<LRenderRule> rule = m_elementStyle.getRenderRule(LUiElementType::None, opt.state);
        if(rule != nullptr){
            hasColor = rule->applyForeground(&painter) & 0x01;
        }

        if(!hasColor){
            std::shared_ptr<LPalette> pal = palette();
            painter.setPen(pal->brush(LPalette::Text)->m_color);
        }

        int v = m_minimum;
        while (v <= m_maximum + 1) {
            if (v == m_maximum + 1 && interval == 1){
                break;
            }
            const int v_ = std::min<int>(v, m_maximum);
            pos = pStyle->sliderPositionFromValue(m_minimum, m_maximum, v_, available) + fudge;
            if (m_orientation == LOrientation::Horizontal) {
                if (m_tickPosition & LSlider::TicksAbove){
                    painter.drawLine(LLine<int>(pos, 0, pos, tickOffset - 1));
                }
                if (m_tickPosition & LSlider::TicksBelow){
                    painter.drawLine(LLine<int>(pos, tickOffset + thickness + 1, pos, opt.rect.height()));
                }
            } else {
                if (m_tickPosition & LSlider::TicksAbove){
                    painter.drawLine(LLine<int>(0, pos, tickOffset - 1, pos));
                }
                if (m_tickPosition & LSlider::TicksBelow)
                    painter.drawLine(LLine<int>(tickOffset + thickness + 1, pos, opt.rect.width(), pos));
            }
            // in the case where maximum is max int
            int nextInterval = v + interval;
            if (nextInterval < v){
                break;
            }
            v = nextInterval;
        }
    }

    if (opt.subCtrls.test((uint8_t)LUiSubId::Slider::Handle)) {
        LRect<int> handle = pStyle->sliderHandleRect(*this, opt, m_tickPosition, thickness, tickOffset);

        LFlags<LUiState > state  = uiElementState(LUiElementType::Handle);
        std::shared_ptr<LRenderRule> rule = m_elementStyle.getRenderRule(LUiElementType::Handle, state);

        if(rule != nullptr && rule->hasDrawable()){
            rule->drawRule(&painter, handle);
        }else{
            // Windows 11 风格圆形滑块（不使用白色底块）：
            //   常态   —— 强调色实心圆
            //   悬停   —— 略微放大、颜色更亮
            //   拖动中 —— 放大 + 中心白点
            const std::shared_ptr<LPalette>& pal = opt.palette;
            const LColor accent = pal->brush(LPalette::Highlight)->m_color;
            LColor fillColor = accent;

            const bool enabled = !!(opt.state & LUiState::Enabled);
            const bool pressed = enabled && !!(opt.state & LUiState::Sunken);
            // 悬停信息记录在 m_hoverCtrl 上（手柄子区域）
            const bool hovered = enabled && !pressed
                    && (m_hoverCtrl.test((uint8_t)LUiSubId::Slider::Handle)
                        || !!(opt.state & LUiState::Hover));

            if (!enabled) {
                fillColor = pal->brush(LPalette::Mid)->m_color;
            } else if (hovered) {
                fillColor = accent.lighter(112);
            }

            painter.save();
            painter.setRenderHints(LRenderHint::Antialiasing, true);

            // 手柄略大于标记区域，视觉更饱满；悬停/拖动时再放大一点作为反馈
            const double baseSize = std::min(handle.width(), handle.height()) * 1.35;
            const double maxSize = thickness > 0 ? double(thickness) : 24.0;
            const bool grow = hovered || pressed;
            const double d = std::min<double>(baseSize + (grow ? 2.0 : 0.0), maxSize);
            const LPoint<int> c = handle.center();
            LRect<double> hr(c.m_x - d / 2, c.m_y - d / 2, d, d, false);

            painter.setBrush(fillColor, LBrush::SolidPattern);
            painter.setPen(LPenStyle::No);
            painter.drawEllipse(hr);

            // 拖动时中心白点，抓取感更明确
            if (pressed) {
                const double id = d * 0.42;
                painter.setBrush(LColor(255, 255, 255), LBrush::SolidPattern);
                painter.setPen(LPenStyle::No);
                painter.drawEllipse(LRect<double>(c.m_x - id / 2, c.m_y - id / 2, id, id, false));
            }
            painter.restore();
        }
    }
}

void LSlider::mousePressEvent(std::shared_ptr<LMouseEvent> ev)
{
    if (m_maximum == m_minimum || (ev->buttons() ^ ev->button())) {
        ev->ignore();
        return;
    }

    ev->accept();
    LStyleOptComplex opt;
    initStyleOpt(opt);

    std::shared_ptr<LStyle> pStyle = style();
    if ((ev->buttons() & LMouseButton::Middle) == ev->button()) {
        LRect<int> sliderRect = pStyle->sliderHandleRect(*this, opt, m_tickPosition, -1, -1);
        const LPoint<int> center = sliderRect.center() - sliderRect.topLeft();
        // to take half of the slider off for the setSliderPosition call we use the center - topLeft

        setSliderPosition(pixelPosToRangeValue(pick(ev->pos() - center)));
        triggerAction(SliderMove);
        m_pressedCtrl = 1<<((uint8_t)LUiSubId::Slider::Handle);
        update();
    } else if ((ev->buttons() & LMouseButton::Left) == ev->button()) {
        m_pressedCtrl = pStyle->hitTestSlider(*this, opt, ev->pos(), m_tickPosition);
        SliderAction action = SliderNoAction;
        if (m_pressedCtrl.test((uint8_t)LUiSubId::Slider::Groove)) {
            LRect<int> sliderRect = pStyle->sliderHandleRect(*this, opt, m_tickPosition,-1, -1);
            int pressValue = pixelPosToRangeValue(pick(ev->pos() - sliderRect.center() + sliderRect.topLeft()));
            if (pressValue > m_value){
                action = SliderPageStepAdd;
            }
            else if (pressValue < m_value){
                action = SliderPageStepSub;
            }
            if (action) {
                triggerAction(action);
            }
        }
    } else {
        ev->ignore();
        return;
    }

    if (m_pressedCtrl.test((uint8_t)LUiSubId::Slider::Handle)) {
        LRect<int> sr = pStyle->sliderHandleRect(*this, opt, m_tickPosition, -1, -1);
        m_clickOffset = pick(ev->pos() - sr.topLeft());
        update(sr);
        setSliderDown(true);
    }

}

void LSlider::mouseReleaseEvent(std::shared_ptr<LMouseEvent> ev)
{
    // 注意：这里不再以 ev->buttons() 作为拦截条件——部分平台的释放事件
    // 仍带有按下的位，会导致按下态永远清不掉
    if (m_pressedCtrl.none()) {
        ev->ignore();
        return;
    }
    ev->accept();
    LUiSubCtrl oldPressed = m_pressedCtrl;
    m_pressedCtrl.reset();
    if (oldPressed.test((uint8_t)LUiSubId::Slider::Handle)){
        setSliderDown(false);
    }
    LStyleOptComplex opt;
    initStyleOpt(opt);
    opt.subCtrls = oldPressed;
    update(style()->sliderSubCtrlRect(*this, opt, oldPressed, m_tickPosition));
}

void LSlider::mouseMoveEvent(std::shared_ptr<LMouseEvent> ev)
{
    // 保险：若按键已经松开（例如在控件外松开导致漏掉 release），清除按下态
    if (m_pressedCtrl.any() && !(ev->buttons() & LMouseButton::Left)) {
        m_pressedCtrl.reset();
        releaseMouse();
        setSliderDown(false);
        update();
        ev->ignore();
        return;
    }

    if (m_pressedCtrl.test((uint8_t)LUiSubId::Slider::Handle) == false) {
        ev->ignore();
        return;
    }
    ev->accept();
    int newPosition = pixelPosToRangeValue(pick(ev->pos()) - m_clickOffset);
    setSliderPosition(newPosition);
}

void LSlider::leaveEvent(std::shared_ptr<LEvent> ev)
{
    // 鼠标移出控件（例如按下后拖到别处再松手）：结束拖动与悬停状态，恢复常态外观
    if (m_pressedCtrl.any() || m_hoverCtrl.any()) {
        m_pressedCtrl.reset();
        m_hoverCtrl.reset();
        setSliderDown(false);
        update();
    }
    LAbstractSlider::leaveEvent(ev);
}

bool LSlider::enterSubItem(const LPoint<int> &pos)
{
    LRect<int> lastHoverRect = m_hoverRect;
    LUiSubCtrl lastHoverControl = m_hoverCtrl;
    if (lastHoverControl != newHoverControl(pos)) {
        if(lastHoverRect.isEmpty()){
            if(!m_hoverRect.isEmpty()){
                update(m_hoverRect);
            }
        }else{
            if(m_hoverRect.isEmpty()){
                update(lastHoverRect);
            }else{
                std::shared_ptr<LRegion> region = std::make_shared<LRegion>(lastHoverRect);
                region->unite(m_hoverRect);
                update(region);
            }
        }
        return true;
    }
    return false;
}

void LSlider::initSpecific()
{
    setAttribute(LWidgetAttribute::WStateOwnSizePolicy, false);
    setAttribute(LWidgetAttribute::CanEnterSubItem);
}

int LSlider::pixelPosToRangeValue(int pos) const
{
    LStyleOptComplex opt;
    initStyleOpt(opt);
    std::shared_ptr<LStyle> pStyle = style();

    int thickness = pStyle->sliderCtrlThickness(m_orientation, opt.rect, m_tickPosition, opt.elementStyle);
    int tickOffset = pStyle->sliderTickOffset(m_orientation, opt.rect, m_tickPosition, thickness, opt.elementStyle);

    LRect<int> gr = pStyle->sliderGrooveRect(m_orientation,
                                            opt,
                                            m_tickPosition,
                                            thickness,
                                            tickOffset);

    LRect<int> sr = pStyle->sliderHandleRect(*this, opt, m_tickPosition, thickness, tickOffset);

    int sliderMin, sliderMax, sliderLength;

    if (m_orientation == LOrientation::Horizontal) {
        sliderLength = sr.width();
        sliderMin = gr.m_x;
        sliderMax = gr.x2() - sliderLength + 1;
    } else {
        sliderLength = sr.height();
        sliderMin = gr.m_y;
        sliderMax = gr.y2() - sliderLength + 1;
    }

    return LStyle::sliderValueFromPosition(m_minimum,
                                           m_maximum,
                                           pos - sliderMin,
                                           sliderMax - sliderMin,
                                           upsideDown(opt.direction));
}

LUiSubCtrl LSlider::newHoverControl(const LPoint<int> &pos)
{
    LStyleOptComplex opt;
    initStyleOpt(opt);
    std::shared_ptr<LStyle> pStyle = style();
    int thickness = pStyle->sliderCtrlThickness(m_orientation, opt.rect, m_tickPosition, opt.elementStyle);
    int tickOffset = pStyle->sliderTickOffset(m_orientation, opt.rect, m_tickPosition, thickness, opt.elementStyle);

    LRect<int> grooveRect = pStyle->sliderGrooveRect(m_orientation,
                                             opt,
                                             m_tickPosition,
                                             thickness,
                                             tickOffset);
    LRect<int> handleRect = pStyle->sliderHandleRect(*this, opt, m_tickPosition, thickness, tickOffset);
    if (handleRect.contains(pos)) {
        m_hoverRect = handleRect;
        m_hoverCtrl = 1<<(uint8_t)LUiSubId::Slider::Handle;
    } else if (grooveRect.contains(pos)) {
        m_hoverRect = grooveRect;
        m_hoverCtrl = 1<<(uint8_t)LUiSubId::Slider::Groove;
    } else if (opt.rect.contains(pos)) {
        m_hoverRect = opt.rect;
        m_hoverCtrl = 1<<(uint8_t)LUiSubId::Slider::Tickmarks;
    } else {
        m_hoverRect.reset();
        m_hoverCtrl.reset();
    }
    return m_hoverCtrl;
}


LK_NAMESPACE_END
