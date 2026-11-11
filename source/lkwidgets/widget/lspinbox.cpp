#include "lspinbox.h"
#include <format>

LK_NAMESPACE_BEGIN

class LSpinBoxEditor : public LMultiTextLineEditor
{
public:
    LSpinBoxEditor(){}
    LSpinBoxData* data = 0;
protected:
    virtual bool isInputable(const wchar_t c) const override{
        const std::wstring &t = m_textLayout.text();

        int dotPos = (int)t.find(wchar_t('.'), 0);
        if(dotPos >=0){
            int decNum = (int)t.length() - dotPos-1;
            if(m_activePos > dotPos){
                if(decNum >= data->m_decimals){
                    return false;
                }
            }
        }

        if(t.starts_with('-')){
            if(isAtStart()){
                return c == wchar_t('-');
            }
        }

        if(c <= wchar_t('9') && c >= wchar_t('0')){
            return true;
        }

        if(data->m_decimals>0){
            if(c == wchar_t('.')){
                return t.contains(c) == false;
            }
        }

        if(c == wchar_t('-')){
            return isAtStart() && data->m_minimum<0;
        }
        return false;

    }
};



LSpinBoxData::LSpinBoxData()
{
    m_sameSideSpinButton = true;
}

LSpinBox::LSpinBox(std::shared_ptr<LWidget> parent,  LFlags<LWindowType> f) : LAbstractLineEdit(parent, f) {
    m_sizePolicy = LSizePolicy(LSizePolicy::Preferred, LSizePolicy::Preferred, LSizePolicy::SpinBox);
    std::shared_ptr<LSpinBoxEditor> e = std::make_shared<LSpinBoxEditor>();
    e->data = this;
    m_editor = e;
}

LSize<int> LSpinBox::sizeHint() const
{
    const int iconSize = style()->get(&LStylePixelMetric::smallIconSize, &m_elementStyle);
    LFontMetrics fm(font());
    std::shared_ptr<LEditBase> base = m_editor->editBase();
    const LMargins<double>& tm = base->margins();
    const LMargins<int>& bounds = boundWidths();
    double x = fm.horizontalAdvance('0') * 11;

    double h = std::max<double>(fm.height(), std::max<double>(14, iconSize - 2))
               + tm.m_top + tm.m_bottom + bounds.m_top+bounds.m_bottom;
    double w = x + tm.m_left + tm.m_right + bounds.m_right+bounds.m_left;
    std::shared_ptr<LStyle> pStyle = style();
    if(m_sameSideSpinButton){
        w += pStyle->get(&LStylePixelMetric::editButtonLength, &m_elementStyle);
    }else{
        w += 2*pStyle->get(&LStylePixelMetric::editButtonLength, &m_elementStyle);
    }
    double lw = lineWidth();
    return LSize<double>(w+2*lw, h+2*lw).toSize();
}

LSize<int> LSpinBox::minimumSizeHint() const
{
    LFontMetrics fm(font());
    std::shared_ptr<LEditBase> base = m_editor->editBase();
    const LMargins<double>& tm = base->margins();
    const LMargins<int>& bounds = boundWidths();

    double h = fm.height()  + tm.m_top + tm.m_bottom + bounds.m_top + bounds.m_bottom;
    double w = fm.maxWidth() + tm.m_left + tm.m_right + bounds.m_right+bounds.m_left;
    std::shared_ptr<LStyle> pStyle = style();
    if(m_sameSideSpinButton){
        w += pStyle->get(&LStylePixelMetric::editButtonLength, &m_elementStyle);
    }else{
        w += 2*pStyle->get(&LStylePixelMetric::editButtonLength, &m_elementStyle);
    }
    double lw = lineWidth();
    return LSize<double>(w+2*lw , h+2*lw).toSize();
}

double LSpinBox::value() const
{
    return m_value;
}

void LSpinBox::setValue(double value)
{
    m_value = lBound(m_minimum, value, m_maximum);
    std::wstring s = std::format(L"{:.{}f}", m_value, m_decimals);
    if(m_editor->content() != s){
        m_editor->setContent(s);
        if(m_undoStack != nullptr){
            m_undoStack->clear();
        }
        sigValueChanged.emt(m_value);
    }
}

void LSpinBox::setDecimals(uint16_t dec)
{
    m_decimals = lBound(uint16_t(0), dec, uint16_t(DBL_MAX_10_EXP + DBL_DIG));
    setRange(m_minimum, m_maximum); // make sure values are rounded
    setValue(value());
}

void LSpinBox::setRange(double min, double max)
{
    double oldMin = m_minimum;
    double oldMax = m_maximum;
    m_minimum = min;
    m_maximum = std::max<double>(min, max);
    if (oldMin != m_minimum || oldMax != m_maximum) {
        setValue(m_value);
        sigRangeChanged.emt(m_minimum, m_maximum);
    }
}

void LSpinBox::setSingleStep(double step)
{
    m_step = std::abs(step);
    update();
}

void LSpinBox::paintEvent(std::shared_ptr<LPaintEvent> e)
{
    LPainter painter(paintDevice());

    std::shared_ptr<LStyle> pStyle = style();
    std::shared_ptr<LWidget> f = getPtrT<LWidget>();
    std::shared_ptr<LPalette> pal = palette();

    uint16_t state = uiElementState(LUiElementType::Text);
    std::shared_ptr<LRenderRule> rule = m_elementStyle.getRenderRule(LUiElementType::Text, state);
    bool hasRuleBackground = false;
    if(rule != nullptr){
        if (rule->hasBackground()) {
            rule->drawBackground(&painter, rule->borderRect(editRect().toRectF()).toRect());
            hasRuleBackground = rule->hasBackground();
        }
    }

    if(!hasRuleBackground && isEnabled()){
        painter.fillRect(editRect().toRectF().adjusted(m_lineWidth, m_lineWidth, -m_lineWidth, -m_lineWidth), pal->brush(LPalette::Base));
    }
    pStyle->drawFrame(f, &painter, *this);
    int btnLen = pStyle->get(&LStylePixelMetric::editButtonLength, &m_elementStyle);
    if(btnLen >0){
        const static uint8_t drawOrder[2] = {
            (uint8_t)LUiSubId::SpinBox::Up,
            (uint8_t)LUiSubId::SpinBox::Down,
        };

        for(uint8_t subId: drawOrder){
            LStyleOptComplex opt;
            initStyleOpt(opt);
            LRect<int> rect = pStyle->spinBoxSubCtrlRect(opt, 1<<subId, btnLen, m_sameSideSpinButton);

            if(rect.isValid()){
                LUiElementType eleType = LStyle::spinBoxSubCtrlUiElement(subId);
                LFlags<LUiState>  state =uiElementState(eleType);
                std::shared_ptr<LRenderRule> rule = m_elementStyle.getRenderRule(eleType, state);

                if(rule != nullptr && rule->hasDrawable()){
                    rule->drawRule(&painter, rect);
                }else{
                    // Windows 11 风格：透明背景，仅绘制箭头符号
                    switch(subId){
                    case (uint8_t)LUiSubId::SpinBox::Up:{
                        pStyle->drawMark(&painter, rect.toRectF(), m_sameSideSpinButton? LDrawMark::Up: LDrawMark::Left, &m_elementStyle, state);
                        break;
                    }
                    case (uint8_t)LUiSubId::SpinBox::Down:{
                        pStyle->drawMark(&painter, rect.toRectF(), m_sameSideSpinButton? LDrawMark::Down: LDrawMark::Right, &m_elementStyle, state);
                        break;
                    }
                    default:{
                        break;
                    }
                    }
                }
            }
        }
    }

    paintEditLine(painter, pal);
}

void LSpinBox::mousePressEvent(std::shared_ptr<LMouseEvent> ev)
{
    if (ev->button() != LMouseButton::Left) {
        ev->ignore();
        return;
    }

    std::shared_ptr<LStyle> pStyle = style();
    LStyleOptComplex opt;
    initStyleOpt(opt);

    m_pressedCtrl = pStyle->hitTestSpinBox(opt, m_sameSideSpinButton, ev->pos());
    activateControl(m_pressedCtrl);

    if(m_pressedCtrl.test((uint8_t)LUiSubId::SpinBox::EditField) ){
        LAbstractLineEdit::mousePressEvent(ev);
    }
}

void LSpinBox::mouseMoveEvent(std::shared_ptr<LMouseEvent> ev)
{
    std::shared_ptr<LStyle> pStyle = style();
    LStyleOptComplex opt;
    initStyleOpt(opt);
    LUiSubCtrl hoverCtrl = pStyle->hitTestSpinBox(opt, m_sameSideSpinButton, ev->pos());

    if(m_pressedCtrl.test((uint8_t)LUiSubId::SpinBox::Up)){
        if(!hoverCtrl.test((uint8_t)LUiSubId::SpinBox::Up)){
            setRepeatAction(LAbstractSlider::SliderNoAction);
        }
    }else if(m_pressedCtrl.test((uint8_t)LUiSubId::SpinBox::Down)){
        if(!hoverCtrl.test((uint8_t)LUiSubId::SpinBox::Down)){
            setRepeatAction(LAbstractSlider::SliderNoAction);
        }
    }else{
        if(hoverCtrl.test((uint8_t)LUiSubId::SpinBox::Up)){
            m_hoverCtrl = 1<<(uint8_t)LUiSubId::SpinBox::Up;
        }else if(hoverCtrl.test((uint8_t)LUiSubId::SpinBox::Down)){
            m_hoverCtrl = 1<<(uint8_t)LUiSubId::SpinBox::Down;
        }else if(hoverCtrl.test((uint8_t)LUiSubId::SpinBox::EditField)){
            m_hoverCtrl = 1<<(uint8_t)LUiSubId::SpinBox::EditField;
            LAbstractLineEdit::mouseMoveEvent(ev);
        }else{
            m_hoverCtrl.reset();
        }
    }
}

void LSpinBox::mouseReleaseEvent(std::shared_ptr<LMouseEvent> ev)
{
    if (ev->button() != LMouseButton::Left) {
        ev->ignore();
        return;
    }

    LAbstractLineEdit::mouseReleaseEvent(ev);

    if(m_pressedCtrl.test((uint8_t)LUiSubId::SpinBox::Up)
        || m_pressedCtrl.test((uint8_t)LUiSubId::SpinBox::Down)
        || m_pressedCtrl.test((uint8_t)LUiSubId::SpinBox::EditField)){
        stopRepeatAction();
        ev->accept();
        return;
    }
    stopRepeatAction();
}


void LSpinBox::keyPressEvent(std::shared_ptr<LKeyEvent> ev)
{
    if(!isEnabled()){
        return;
    }

    switch (ev->key()) {
    case (uint32_t)LKey::Up:{
        doUp();
        ev->accept();
        break;
    }
    case (uint32_t)LKey::Down: {
        doDown();
        ev->accept();
        break;
    }
    case (uint32_t)LKey::Return:
    case (uint32_t)LKey::Enter:{
        endInput();
        selectAll();
        ev->accept();
        break;
    }
    default:{
        LAbstractLineEdit::keyPressEvent(ev);
    }
    }
}

void LSpinBox::focusOutEvent(std::shared_ptr<LFocusEvent> ev)
{
    endInput();
    LAbstractLineEdit::focusOutEvent(ev);
}

void LSpinBox::wheelEvent(std::shared_ptr<LWheelEvent> e)
{
    if (rect().contains(e->position().toPoint())){
        if(e->angleDelta().m_y > 0 ){
            doUp();
        }else{
            doDown();
        }
    }
}

bool LSpinBox::enterSubItem(const LPoint<int> &p)
{
    if(m_pExtraData != nullptr){
        std::shared_ptr<LStyle> pStyle = style();
        LStyleOptComplex opt;
        initStyleOpt(opt);
        LUiSubCtrl subCtrl =  pStyle->hitTestSpinBox(opt, m_sameSideSpinButton, p);
        if(subCtrl != m_hoverCtrl){
            m_hoverCtrl = subCtrl;
            if(m_hoverCtrl.test((uint8_t)LUiSubId::ComboBox::EditField)){
                m_pExtraData->cursor = m_isReadOnly ? LCursorShape::Arrow : LCursorShape::IBeam;
                setAttribute(LWidgetAttribute::SetCursor);
                update();
                return true;
            }else{
                unsetCursor();
                update();
            }
        }
    }
    return false;
}

void LSpinBox::initSpecific()
{
    setAttribute(LWidgetAttribute::CanEnterSubItem);
    LAbstractLineEdit::initSpecific();
    m_editor->setContent(LStringUtil::utf8ToWide(std::format("{:.{}f}", m_value, m_decimals)));
}

uint16_t LSpinBox::uiElementState(LUiElementType type) const
{
    uint16_t ret = 0;
    if(type == LUiElementType::UpButton){
        if(((m_value+ m_step) <= m_maximum) && isEnabled()){
            if(m_pressedCtrl.test((uint8_t)LUiSubId::SpinBox::Up)){
                ret = uint16_t(LUiState::Sunken);
            }else if(m_hoverCtrl.test((uint8_t)LUiSubId::SpinBox::Up)){
                ret = uint16_t(LUiState::Hover);
            }else{
                ret=  uint16_t(LUiState::Enabled);
            }
        }else{
            ret= uint16_t(LUiState::Disabled);
        }
    }else if(type == LUiElementType::DownButton){
        if(((m_value- m_step) >= m_minimum) && isEnabled()){
            if(m_pressedCtrl.test((uint8_t)LUiSubId::SpinBox::Down)){
                ret = uint16_t(LUiState::Sunken);
            }else if(m_hoverCtrl.test((uint8_t)LUiSubId::SpinBox::Down)){
                ret = uint16_t(LUiState::Hover);
            }else{
                ret = uint16_t(LUiState::Enabled);
            }
        }else{
            ret= uint16_t(LUiState::Disabled);
        }
    }else{
        ret = LWidget::uiElementState(type);
    }
    return ret;
}

LRect<int> LSpinBox::editRect() const
{
    LStyleOptComplex opt;
    initStyleOpt(opt);
    std::shared_ptr<LStyle> pStyle = style();
    int btnLen = pStyle->get(&LStylePixelMetric::editButtonLength, &m_elementStyle);
    return pStyle->spinBoxSubCtrlRect(opt, (1<<(uint8_t)LUiSubId::SpinBox::EditField), btnLen, m_sameSideSpinButton);
}

bool LSpinBox::checkOpResult()
{
    std::wstring c = m_editor->content();
    if(c.empty()){
        return true;
    }

    if(c == L"."){
        if(m_decimals>0){
            return true;
        }else{
            return false;
        }
    }

    if(c == L"-"){
        if(m_minimum <0){
            return true;
        }else{
            return false;
        }
    }

    if(c == L"-."){
        if((m_minimum <0) && (m_decimals>0)){
            return true;
        }else{
            return false;
        }
    }

    double value = LStringUtil::stod(c);
    if(lBound(m_minimum, value, m_maximum) == value){
        if(m_value != value){
            m_value = value;
            sigValueChanged.emt(m_value);
        }
        return true;
    }
    return false;
}

void LSpinBox::endInput()
{
    std::wstring s = LStringUtil::utf8ToWide(std::format("{:.{}f}", m_value, m_decimals));
    if(m_editor->content() != s){
        m_editor->setContent(s);
        if(m_undoStack != nullptr){
            m_undoStack->clear();
        }
        sigValueChanged.emt(m_value);
    }
}

void LSpinBox::activateControl(LUiSubCtrl control, int threshold)
{
    LAbstractSlider::SliderAction action = LAbstractSlider::SliderNoAction;
    switch (control.to_ulong()) {
    case 1<<(uint8_t)LUiSubId::SpinBox::Up:{
        action = LAbstractSlider::SliderSingleStepAdd;
        break;
    }
    case 1<<(uint8_t)LUiSubId::SpinBox::Down:{
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

LVariant LSpinBox::inputMethodQuery(LInputMethodQuery property, const LVariant &args) const
{
    switch(property) {
    case LInputMethodQuery::ImEnabled:{
        return false;
    }
    default:{
        return LAbstractLineEdit::inputMethodQuery(property);
    }
    }
}


void LSpinBox::setRepeatAction(LAbstractSlider::SliderAction action, int thresholdTime, int repeatTime)
{
    if(m_repeatTimer == nullptr){
        m_repeatTimer = std::make_shared<LTimer>();
        m_repeatTimer->sigTimeout.connect(this, &LSpinBox::doRepeat, ConnectionType::Queued);
    }

    if ((m_repeatAction = action) == LAbstractSlider::SliderNoAction) {
        m_repeatTimer->stop();
    } else {
        m_repeatActionTime = repeatTime;
        m_repeatTimer->start(std::chrono::milliseconds(thresholdTime), false);
    }
}

void LSpinBox::doRepeat()
{
    if (m_repeatActionTime != 0) { // was threshold time, use repeat time next time
        m_repeatTimer->start(std::chrono::milliseconds(m_repeatActionTime), true);
        m_repeatActionTime = 0;
    }
    triggerAction(m_repeatAction);
}

void LSpinBox::triggerAction(LAbstractSlider::SliderAction action)
{
    switch (action) {
    case LAbstractSlider::SliderSingleStepAdd:{
        doUp();
        break;
    }
    case LAbstractSlider::SliderSingleStepSub:{
        doDown();
        break;
    }
    default:{
        break;
    }
    };
}

void LSpinBox::stopRepeatAction()
{
    setRepeatAction(LAbstractSlider::SliderNoAction);
    m_pressedCtrl.reset();
}


LK_NAMESPACE_END
