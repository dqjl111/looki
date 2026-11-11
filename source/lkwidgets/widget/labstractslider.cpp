#include "lkwidgets/widget/labstractslider.h"
#include "lkwidgets/core/lsizepolicy.h"

LK_NAMESPACE_BEGIN

LSliderData::LSliderData(LOrientation orient)
{
    m_orientation = orient;
    m_tracking = true;
}

bool LSliderData::upsideDown(LLayoutDirection dir) const
{
    if(m_orientation == LOrientation::Horizontal){
        return m_invertedAppearance != (dir == LLayoutDirection::RightToLeft);
    }else{
        return m_invertedAppearance == true;
    }
}

LAbstractSlider::LAbstractSlider(std::shared_ptr<LWidget> parent, LOrientation orient)
    : LWidget(parent)
    , LSliderData(orient)
{
}

void LAbstractSlider::setValue(int value)
{
    value = lBound(m_minimum, value, m_maximum);
    if (m_value == value && m_position == value){
        return;
    }

    m_value = value;
    m_position = value;
    update();
    sigValueChanged.emt(value);
}

void LAbstractSlider::setOrientation(LOrientation orientation)
{
    if (m_orientation == orientation){
        return;
    }

    m_orientation = orientation;
    if (!testAttribute(LWidgetAttribute::WStateOwnSizePolicy)) {
        setSizePolicy(sizePolicy().transposed());
        setAttribute(LWidgetAttribute::WStateOwnSizePolicy, false);
    }
    update();
    updateGeometry();
}

void LAbstractSlider::setRange(int min, int max)
{
    int oldMin = m_minimum;
    int oldMax = m_maximum;
    m_minimum = min;
    m_maximum = std::max<int>(min, max);
    if (oldMin != m_minimum || oldMax != m_maximum) {
        m_value = lBound(m_minimum, m_value, m_maximum);
        m_position = m_value;
        update();
        sigRangeChanged.emt(m_minimum, m_maximum);
        sigValueChanged.emt(m_value);
    }
}

void LAbstractSlider::setMinimum(int min)
{
    setRange(min, std::max<int>(m_maximum, min));
}


void LAbstractSlider::setMaximum(int max)
{
    setRange(std::min<int>(m_minimum, max), max);
}

void LAbstractSlider::setSingleStep(int step)
{
    if (step != m_singleStep){
        m_singleStep = step;
        update();
    }
}

void LAbstractSlider::setPageStep(int step)
{
    if (step != m_pageStep){
        m_pageStep = step;
        update();
    }
}

void LAbstractSlider::setSteps(int single, int page)
{
    if ((single != m_singleStep) || (m_pageStep != page)){
        m_singleStep = single;
        m_pageStep = page;
        update();
    }
}

void LAbstractSlider::setSliderDown(bool down)
{
    m_pressed = down;
    if (!down && m_position != m_value){
        triggerAction(SliderMove);
    }
}

void LAbstractSlider::setSliderPosition(int position)
{
    position = lBound(m_minimum, position, m_maximum);
    if (position == m_position){
        return;
    }
    m_position = position;
    if (!m_tracking){
        update();
    }

    if (m_tracking && !m_blocktracking){
        triggerAction(SliderMove);
    }
}

void LAbstractSlider::triggerAction(SliderAction action)
{
    m_blocktracking = true;
    switch (action) {
    case SliderSingleStepAdd:{
        setSliderPosition(overflowSafeAdd(m_singleStep));
        break;
    }
    case SliderSingleStepSub:{
        setSliderPosition(overflowSafeAdd(-m_singleStep));
        break;
    }
    case SliderPageStepAdd:{
        setSliderPosition(overflowSafeAdd(m_pageStep));
        break;
    }
    case SliderPageStepSub:{
        setSliderPosition(overflowSafeAdd(-m_pageStep));
        break;
    }
    case SliderToMinimum:{
        setSliderPosition(m_minimum);
        break;
    }
    case SliderToMaximum:{
        setSliderPosition(m_maximum);
        break;
    }
    case SliderMove:
    case SliderNoAction:
        break;
    };

    m_blocktracking = false;
    setValue(m_position);
}

int LAbstractSlider::overflowSafeAdd(int add) const
{
    int newValue = m_value + add;
    if (add > 0 && newValue < m_value){
        newValue = m_maximum;
    }
    else if (add < 0 && newValue > m_value){
        newValue = m_minimum;
    }
    return newValue;
}


void LAbstractSlider::setRepeatAction(SliderAction action, int thresholdTime, int repeatTime)
{
    if(m_repeatTimer == nullptr){
        m_repeatTimer = std::make_shared<LTimer>();
        m_repeatTimer->sigTimeout.connect(this, &LAbstractSlider::doRepeat, ConnectionType::Queued);
    }

    if ((m_repeatAction = action) == SliderNoAction) {
        m_repeatTimer->stop();
    } else {
        m_repeatActionTime = repeatTime;
        m_repeatTimer->start(std::chrono::milliseconds(thresholdTime), false);
    }
}

void LAbstractSlider::doRepeat()
{
    if (m_repeatActionTime != 0) { // was threshold time, use repeat time next time
        m_repeatTimer->start(std::chrono::milliseconds(m_repeatActionTime), true);
        m_repeatActionTime = 0;
    }
    triggerAction(m_repeatAction);
}






LK_NAMESPACE_END
