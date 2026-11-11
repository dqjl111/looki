#include "lcomboedit.h"

LK_NAMESPACE_BEGIN

LComboEdit::LComboEdit(std::shared_ptr<LWidget> parent, LFlags<LWindowType> f)  : LAbstractLineEdit(parent, f){

    m_sizePolicy = LSizePolicy(LSizePolicy::Preferred, LSizePolicy::Preferred, LSizePolicy::ComboBox);
    m_editor = std::make_shared<LLineEditor>();
    m_editor->setMaxLength(65535);

    m_menu = makeInit<LMenu>();
    m_menu->setUseScrollBar(true);
    m_menu->setBackgroundRole(LPalette::Base);
}

LSize<int> LComboEdit::sizeHint() const
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
    w += pStyle->get(&LStylePixelMetric::editButtonLength, &m_elementStyle);
    double lw = lineWidth();
    return LSize<double>(w+2*lw, h+2*lw).toSize();
}

LSize<int> LComboEdit::minimumSizeHint() const
{
    LFontMetrics fm(font());
    std::shared_ptr<LEditBase> base = m_editor->editBase();
    const LMargins<double>& tm = base->margins();
    const LMargins<int>& bounds = boundWidths();

    double h = fm.height()  + tm.m_top + tm.m_bottom + bounds.m_top + bounds.m_bottom;
    double w = fm.maxWidth() + tm.m_left + tm.m_right + bounds.m_right+bounds.m_left;
    std::shared_ptr<LStyle> pStyle = style();
    w += pStyle->get(&LStylePixelMetric::editButtonLength, &m_elementStyle);
    double lw = lineWidth();
    return LSize<double>(w+2*lw , h+2*lw).toSize();
}

void LComboEdit::setEditText(const std::wstring &s)
{
    m_editor->setContent(s);
    if(m_undoStack != nullptr){
        m_undoStack->clear();
    }
}

std::shared_ptr<LAbstractMenu> LComboEdit::menu() const
{
    return m_menu;
}

void LComboEdit::currrentItemUpdate()
{
    if(m_currentItem != nullptr){
        m_editor->setContent(m_currentItem->text());
        if(m_undoStack != nullptr){
            m_undoStack->clear();
        }
    }
}

std::shared_ptr<LCombBoxBase> LComboEdit::initItem(std::shared_ptr<LComboBoxItem>)
{
    return getPtrT<LComboEdit>();
}

void LComboEdit::paintEvent(std::shared_ptr<LPaintEvent> e)
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
        uint8_t subId = (uint8_t)LUiSubId::ComboBox::Arrow;

        LStyleOptComplex opt;
        initStyleOpt(opt);
        LRect<int> rect = pStyle->comboEditSubCtrlRect(opt, 1<<subId, btnLen);

        if(rect.isValid()){
            LUiElementType eleType = LStyle::comboEditSubCtrlUiElement(subId);
            LFlags<LUiState>  state =uiElementState(eleType);
            std::shared_ptr<LRenderRule> rule = m_elementStyle.getRenderRule(eleType, state);

            if(rule != nullptr && rule->hasDrawable()){
                rule->drawRule(&painter, rect);
            }else{
                std::shared_ptr<LBrush> btnBrush = opt.palette->brush(LPalette::Button);
                if(state & LUiState::Sunken) {
                    painter.setPen(opt.palette->dark()->m_color);
                    painter.setBrush(btnBrush);
                    painter.drawRect(rect.adjusted(0, 0, -1, -1));
                } else {
                    LDrawUtil::drawPlainRect(&painter, rect,
                                             btnBrush->m_color,
                                             0,
                                             btnBrush);
                }

                painter.setPen(opt.palette->brush(opt.foregroundRole)->m_color, LPenStyle::Solid, 1);
                pStyle->drawMark(&painter, rect.toRectF(), LDrawMark::Down, &m_elementStyle, state);
            }
        }
    }

    paintEditLine(painter, pal);
}

LRect<int> LComboEdit::editRect() const
{
    LStyleOptComplex opt;
    initStyleOpt(opt);
    std::shared_ptr<LStyle> pStyle = style();
    int btnLen = pStyle->get(&LStylePixelMetric::editButtonLength, &m_elementStyle);
    return pStyle->comboEditSubCtrlRect(opt, (1<<(uint8_t)LUiSubId::ComboBox::EditField), btnLen);
}

uint16_t LComboEdit::uiElementState(LUiElementType type) const
{
    uint16_t ret = 0;
    if(type == LUiElementType::DownButton){
        if(isEnabled()){
            if(m_menu->isVisible()){
                ret = uint16_t(LUiState::Sunken);
            }else{
                if(m_hoverCtrl.test((uint8_t)LUiSubId::ComboBox::Arrow)){
                    ret = uint16_t(LUiState::Hover);
                }else{
                    ret = uint16_t(LUiState::Enabled);
                }
            }
        }else{
            ret= uint16_t(LUiState::Disabled);
        }
    }else{
        ret = LWidget::uiElementState(type);
    }
    return ret;
}

void LComboEdit::swichMenu()
{
    if(m_menu != nullptr){
        if(!m_menu->isVisible()){
            if(m_menu->isScrollable()){
                m_menu->setMinimumWidth(width());
                m_menu->setMaximumWidth(width());
            }else{
                m_menu->setMaximumWidth(LWIDGETSIZE_MAX);
            }
        }

        if(m_menu->isVisible()){
            m_menu->setAttribute(LWidgetAttribute::NoMouseReplay);
            m_menu->close();
        }else{
            std::shared_ptr<LWidget> w = getPtrT<LWidget>();
            m_menu->setMenuCause(w);
            m_menu->popup(m_menu->adjustedMenuPosition(w));
        }
    }
    update();
}


void LComboEdit::mousePressEvent(std::shared_ptr<LMouseEvent> ev)
{
    if (ev->button() != LMouseButton::Left) {
        ev->ignore();
        return;
    }

    std::shared_ptr<LStyle> pStyle = style();
    LStyleOptComplex opt;
    initStyleOpt(opt);

    m_pressedCtrl = pStyle->hitTestComboEdit(opt, ev->pos());
    if(m_pressedCtrl.test((uint8_t)LUiSubId::ComboBox::Arrow)){
        swichMenu();
    }
    else if(m_pressedCtrl.test((uint8_t)LUiSubId::ComboBox::EditField) ){
        LAbstractLineEdit::mousePressEvent(ev);
    }
}

void LComboEdit::mouseMoveEvent(std::shared_ptr<LMouseEvent> ev)
{
    std::shared_ptr<LStyle> pStyle = style();
    LStyleOptComplex opt;
    initStyleOpt(opt);
    m_hoverCtrl = pStyle->hitTestComboEdit(opt, ev->pos());
    if(m_hoverCtrl.test((uint8_t)LUiSubId::ComboBox::EditField)){
        LAbstractLineEdit::mouseMoveEvent(ev);
    }
}

void LComboEdit::mouseReleaseEvent(std::shared_ptr<LMouseEvent> ev)
{
    if (ev->button() != LMouseButton::Left) {
        ev->ignore();
        return;
    }

    LAbstractLineEdit::mouseReleaseEvent(ev);
    m_pressedCtrl.reset();
    update();
}

void LComboEdit::initSpecific()
{
    setAttribute(LWidgetAttribute::CanEnterSubItem);
    LAbstractLineEdit::initSpecific();
}

bool LComboEdit::enterSubItem(const LPoint<int> &p)
{
    if(m_pExtraData != nullptr){
        std::shared_ptr<LStyle> pStyle = style();
        LStyleOptComplex opt;
        initStyleOpt(opt);
        LUiSubCtrl subCtrl = pStyle->hitTestComboEdit(opt, p);
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


LK_NAMESPACE_END
