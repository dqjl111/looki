#include "lscrollbar.h"
#include "lkwidgets/draw/lstyle.h"

LK_NAMESPACE_BEGIN

LScrollBar::LScrollBar(std::shared_ptr<LWidget> parent, LOrientation orient) : LAbstractSlider(parent, orient)
{
    if (orient == LOrientation::Horizontal){
        m_sizePolicy = (LSizePolicy(LSizePolicy::Expanding, LSizePolicy::Fixed, LSizePolicy::Slider));
    }else{
        m_sizePolicy = (LSizePolicy(LSizePolicy::Fixed, LSizePolicy::Expanding, LSizePolicy::Slider));
    }
}

LSize<int> LScrollBar::sizeHint() const
{
    std::shared_ptr<LStyle> pStyle = style();
    int scrollBarExtent = pStyle->get(&LStylePixelMetric::scrollBarExtent, &m_elementStyle);
    int scrollBarSliderMin = std::lround(dpiScaled(pStyle->get(&LStylePixelMetric::scrollBarSliderMin, &m_elementStyle)));
    if (m_orientation == LOrientation::Horizontal){
        return LSize<int>(scrollBarExtent * 2 + scrollBarSliderMin, scrollBarExtent);
    }else{
        return LSize<int>(scrollBarExtent, scrollBarExtent * 2 + scrollBarSliderMin);
    }
}

void LScrollBar::blink()
{
    std::shared_ptr<LWidget> p= parentT<LWidget>();
    if(p != nullptr){
        p->blink();
    }
}

void LScrollBar::paintEvent(std::shared_ptr<LPaintEvent> ev)
{
    LPainter painter(paintDevice());
    LStyleOptComplex opt;
    initStyleOpt(opt);

    std::shared_ptr<LStyle> pStyle = style();
    opt.subCtrls.set();
    if (m_pressedCtrl.any()) {
        opt.activeSubCtrls = m_pressedCtrl;
        if (!m_pointerOutsidePressedCtrl){
            opt.state |= LUiState::Sunken;
        }
    } else {
        opt.activeSubCtrls = m_hoverCtrl;
    }
    std::shared_ptr<LWidget> q= getPtrT<LWidget>();
    pStyle->drawScroll(q, *this, &painter, opt);
}

void LScrollBar::mousePressEvent(std::shared_ptr<LMouseEvent> e)
{
    if(m_repeatTimer != nullptr){
        if (m_repeatTimer->isActive()){
            stopRepeatAction();
        }
    }

    std::shared_ptr<LStyle> pStyle = style();
    LStyleOptComplex opt;
    initStyleOpt(opt);
    // opt.keyboardModifiers = e->modifiers();

    if (m_maximum == m_minimum // no range
        || (e->buttons() & (~uint32_t(e->button()))) // another button was clicked before
        || !(e->button() == LMouseButton::Left)) {
        e->ignore();
        return;
    }

    m_pressedCtrl = pStyle->hitTestScroll(*this, opt, e->pos());
    m_pointerOutsidePressedCtrl = false;

    LRect<int> sr = pStyle->scrollSubCtrlRect(*this,
                                              opt,
                                              1<<(uint8_t)LUiSubId::ScrollBar::Slider,
                                              -1,
                                              -1,
                                              -1,
                                              -1);
    LPoint<int> click = e->pos();
    if (m_pressedCtrl.test((uint8_t)LUiSubId::ScrollBar::Slider)) {
        if(m_orientation == LOrientation::Horizontal){
            m_clickOffset = click.m_x-sr.m_x;
        }else{
            m_clickOffset = click.m_y-sr.m_y;
        }
    }

    if (m_pressedCtrl.test((uint8_t)LUiSubId::ScrollBar::AddPage)
         || m_pressedCtrl.test((uint8_t)LUiSubId::ScrollBar::SubPage)) {
        int sliderLength = m_orientation == LOrientation::Horizontal ? sr.width() : sr.height();
        setSliderPosition(pixelPosToRangeValue((m_orientation == LOrientation::Horizontal ? click.m_x : click.m_y) - sliderLength / 2));
        m_pressedCtrl = 1<<(uint8_t)LUiSubId::ScrollBar::Slider;
        m_clickOffset = sliderLength / 2;
    }

    activateControl(m_pressedCtrl);
    repaint(pStyle->scrollSubCtrlRect(*this,
                                      opt,
                                      m_pressedCtrl,
                                      -1,
                                      -1,
                                      -1,
                                      -1));
    if (m_pressedCtrl.test((uint8_t)LUiSubId::ScrollBar::Slider)){
        setSliderDown(true);
    }
}

void LScrollBar::mouseReleaseEvent(std::shared_ptr<LMouseEvent> ev)
{
    if (m_pressedCtrl.none()){
        return;
    }

    if (ev->buttons() & (~uint32_t(ev->button()))){ // some other button is still pressed
        return;
    }

    stopRepeatAction();
}

void LScrollBar::mouseMoveEvent(std::shared_ptr<LMouseEvent> e)
{
    if (m_pressedCtrl.none()){
        return;
    }

    std::shared_ptr<LStyle> pStyle = style();
    LStyleOptComplex opt;
    initStyleOpt(opt);

    if (m_pressedCtrl.test((uint8_t)LUiSubId::ScrollBar::Slider)) {
        LPoint<int>  click = e->pos();
        int newPosition = pixelPosToRangeValue((m_orientation == LOrientation::Horizontal ? click.m_x : click.m_y) -m_clickOffset);
        setSliderPosition(newPosition);
    } else{
        // stop scrolling when the mouse pointer leaves a control
        // similar to push buttons
        LRect<int> pr = pStyle->scrollSubCtrlRect(*this,
                                                  opt,
                                                  m_pressedCtrl,
                                                  -1,
                                                  -1,
                                                  -1,
                                                  -1);
        if (pr.contains(e->pos())) {
            if(m_pointerOutsidePressedCtrl == true){
                activateControl(m_pressedCtrl);
            }
        }else{
            if(m_pointerOutsidePressedCtrl == false){
                m_pointerOutsidePressedCtrl = true;
                setRepeatAction(SliderNoAction);
                repaint(pr);
            }
        }
    }
}

void LScrollBar::activateControl(LUiSubCtrl control, int threshold)
{
    LAbstractSlider::SliderAction action = LAbstractSlider::SliderNoAction;
    switch (control.to_ulong()) {
    case 1<<(uint8_t)LUiSubId::ScrollBar::AddPage:{
        action = LAbstractSlider::SliderPageStepAdd;
        break;
    }
    case 1<<(uint8_t)LUiSubId::ScrollBar::SubPage:{
        action = LAbstractSlider::SliderPageStepSub;
        break;
    }
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

void LScrollBar::initSpecific()
{
    setFocusPolicy(LFocusPolicy::No);
    setAttribute(LWidgetAttribute::WStateOwnSizePolicy, false);
    setAttribute(LWidgetAttribute::OpaquePaintEvent);
}

void LScrollBar::stopRepeatAction()
{
    LUiSubCtrl tmp = m_pressedCtrl;
    setRepeatAction(SliderNoAction);
    m_pressedCtrl.reset();

    if (tmp.test((uint8_t)LUiSubId::ScrollBar::Slider)){
        setSliderDown(false);
    }

    LStyleOptComplex opt;
    initStyleOpt(opt);
    repaint(style()->scrollSubCtrlRect(*this, opt, tmp, -1,-1,-1,-1));
}

int LScrollBar::pixelPosToRangeValue(int pos) const
{
    std::shared_ptr<LStyle> pStyle = style();
    LStyleOptComplex opt;
    initStyleOpt(opt);
    LRect<int> gr = pStyle->scrollSubCtrlRect(*this, opt, 1<<(uint8_t)LUiSubId::ScrollBar::Groove,-1,-1,-1,-1);
    LRect<int> sr = pStyle->scrollSubCtrlRect(*this, opt, 1<<(uint8_t)LUiSubId::ScrollBar::Slider,-1,-1,-1,-1);
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

    return  LStyle::sliderValueFromPosition(m_minimum, m_maximum, pos - sliderMin,
                                           sliderMax - sliderMin, upsideDown(opt.direction));
}

LK_NAMESPACE_END
