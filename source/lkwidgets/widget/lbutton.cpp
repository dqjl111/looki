#include "lbutton.h"
#include "lkwidgets/draw/lstyle.h"
#include "lmenu.h"
#include "lkwidgets/core/ldesktopwidget.h"
#include "lkwidgets/dialog/ldialog.h"


LK_NAMESPACE_BEGIN

LButton::LButton(std::shared_ptr<LWidget> parent, LFlags<LWindowType> f)
    : LAbstractButton(LSizePolicy::PushButton, parent, f)
{
    initButton();
}

LButton::LButton(const std::wstring &text, std::shared_ptr<LWidget> parent, LFlags<LWindowType> f)
    : LAbstractButton(LSizePolicy::PushButton, parent, f)
{
    initButton();
    m_text = text;
    m_isTextLabel = true;
    m_textDirty = true;
}


LButton::LButton(LSizePolicy::ControlType type, std::shared_ptr<LWidget> parent, LFlags<LWindowType> f)
    : LAbstractButton(type, parent, f)
{
    initButton();
}


LSize<int> LButton::sizeHint() const
{
    if (!m_validHints){
        minimumSizeHint();
    }
    return m_sizeHint;
}

LSize<int> LButton::minimumSizeHint() const
{
    return LLabelData::minimumSizeHint(*this);
}

int LButton::heightForWidth(int w) const
{
    if (m_isTextLabel){
        return sizeForWidth(w, *this).height();
    }
    return LWidget::heightForWidth(w);
}

void LButton::setSizePolicy(LSizePolicy sp)
{
    m_sizePolicy = sp;
    m_validHints = false;
}

void LButton::setDefault(bool enable)
{
    if (m_isDefault == enable){
        return;
    }
    m_isDefault = enable;
    if (m_isDefault) {
        std::shared_ptr<LDialog> dlg = dialogParent();
        if (dlg != nullptr){
            dlg->setMainDefault(getPtrT<LButton>());
        }
    }
    update();
}

bool LButton::isDefault() const
{
    return m_isDefault;
}

bool LButton::autoDefault() const
{
    if(m_autoDefault == LButton::Auto){
        return ( dialogParent() != nullptr );
    }
    return m_autoDefault;
}

void LButton::setAutoDefault(bool enable)
{
    uint16_t state = enable ? LButton::On : LButton::Off;
    if (m_autoDefault != LButton::Auto && m_autoDefault == state){
        return;
    }
    m_autoDefault = state;
    m_sizeHint = LSize<int>();
    update();
    updateGeometry();
}


void LButton::paintEvent(std::shared_ptr<LPaintEvent> e)
{
    if(m_shape == Box){
        if(m_down){
            m_shadow = Raised;
        }else{
            m_shadow = Sunken;
        }
    }else{
        if(m_down){
            m_shadow = Sunken;
        }else{
            m_shadow = Raised;
        }
    }

    LPainter painter(paintDevice());

    std::shared_ptr<LStyle> pStyle = style();
    std::shared_ptr<LWidget> f = getPtrT<LWidget>();
    pStyle->drawFrame(f, &painter, *this, true);
    pStyle->drawLabel(f, &painter, *this, LDrawMark::None);
}

void LButton::initButton()
{    
    m_leftMargin = m_rightMargin = 3;
    m_shape = StyledPanel;
    m_lineWidth = 1;
}

void LButton::updateLabel()
{
    m_validHints = false;
    if (m_isTextLabel) {
        LSizePolicy policy = sizePolicy();
        const bool wrap = m_align & (uint16_t)LTextFlag::WordWrap;
        policy.setHasHeightForWidth(wrap);
        if (policy != sizePolicy()){
            setSizePolicy(policy);
        }
        m_textLayoutDirty = true;
    }
    updateGeometry();
    update(contentsRect());
}

void LButton::initSpecific()
{
    setAttribute(LWidgetAttribute::Hover);
    if(m_textDirty){
        updateLabel();
    }
}

LMenuButton::LMenuButton(std::shared_ptr<LWidget> parent, LFlags<LWindowType> f)
    : LButton(parent, f)
{
    m_hasMark = 1;
}

LMenuButton::LMenuButton(const std::wstring &text, std::shared_ptr<LWidget> parent, LFlags<LWindowType> f)
    : LButton(text, parent, f)
{
    m_hasMark = 1;
}

void LMenuButton::setMenu(std::shared_ptr<LMenu> menu)
{
    if (m_menu == menu){
        return;
    }

    m_menu = menu;
    if(m_menu->isVisible()){
        m_menu->close();
    }
}

std::shared_ptr<LMenu> LMenuButton::menu() const
{
    return m_menu;
}

void LMenuButton::setShowArrow(bool isShowArrow)
{
    if(isShowArrow != (bool)m_hasMark){
        m_hasMark = isShowArrow ? 1 : 0;
        m_sizeHint = LSize<int>();
        update();
        updateGeometry();
    }
}

uint16_t LMenuButton::uiElementState(LUiElementType type) const
{
    if(m_menu->isVisible()){
        return uint16_t(LUiState::Sunken);
    }
    return LWidget::uiElementState(type);
}

void LMenuButton::paintEvent(std::shared_ptr<LPaintEvent> e)
{
    if(m_menu == nullptr){
        LButton::paintEvent(e);
        return;
    }

    if(m_shape == Box){
        if(m_menu->isVisible()){
            m_shadow = Raised;
        }else{
            m_shadow = Sunken;
        }
    }else{
        if(m_menu->isVisible()){
            m_shadow = Sunken;
        }else{
            m_shadow = Raised;
        }
    }

    LPainter painter(paintDevice());

    std::shared_ptr<LStyle> pStyle = style();
    std::shared_ptr<LWidget> f = getPtrT<LWidget>();
    pStyle->drawFrame(f, &painter, *this, true);
    pStyle->drawLabel(f, &painter, *this, m_menu->isScrollable()? LDrawMark::Down: LDrawMark::Right);
}

void LMenuButton::setDown(bool down)
{
    if(m_menu != nullptr){
        if(m_menu->isVisible()){
            if(down){
                //第一次触发是菜单转过来的，关闭菜单后还会触发一次，借此标记，避免再次打开
                m_menu->setAttribute(LWidgetAttribute::NoMouseReplay);
                m_menu->close();
            }
        }else{
            if(down){
                std::shared_ptr<LWidget> w = getPtrT<LWidget>();
                m_menu->setMenuCause(w);
                m_menu->popup(m_menu->adjustedMenuPosition(w));
            }
        }
    }
    LButton::setDown(down);
}



LCheckBox::LCheckBox(std::shared_ptr<LWidget> parent, LFlags<LWindowType> f) : LButton(LSizePolicy::CheckBox, parent, f)
{
    m_shape = NoFrame;
    m_checkable = true;
    m_checked = false;
    m_hasMark = true;
    m_isMarkLeft = true;
}

LCheckBox::LCheckBox(const std::wstring &text, std::shared_ptr<LWidget> parent, LFlags<LWindowType> f)
    : LButton(LSizePolicy::CheckBox, parent, f)
{
    m_shape = NoFrame;
    m_text = text;
    m_isTextLabel = true;
    m_textDirty = true;

    m_checkable = true;
    m_checked = false;
    m_hasMark = true;
    m_isMarkLeft = true;
}

void LCheckBox::paintEvent(std::shared_ptr<LPaintEvent> e)
{
    LPainter painter(paintDevice());

    std::shared_ptr<LStyle> pStyle = style();
    std::shared_ptr<LWidget> f = getPtrT<LWidget>();
    pStyle->drawFrame(f, &painter, *this);
    if(m_checked){
        pStyle->drawLabel(f, &painter, *this, LDrawMark::CheckedBox);
    }else{
        pStyle->drawLabel(f, &painter, *this, LDrawMark::UncheckedBox);
    }
}

LRadioButton::LRadioButton(std::shared_ptr<LWidget> parent, LFlags<LWindowType> f): LButton(LSizePolicy::RadioButton, parent, f)
{
    m_shape = NoFrame;
    m_checkable = true;
    m_checked = false;
    m_hasMark = true;
    m_isMarkLeft = true;
    m_autoExclusive = true;
}

LRadioButton::LRadioButton(const std::wstring &text, std::shared_ptr<LWidget> parent, LFlags<LWindowType> f)
 : LButton(LSizePolicy::CheckBox, parent, f)
{
    m_shape = NoFrame;
    m_text = text;
    m_isTextLabel = true;
    m_textDirty = true;

    m_checkable = true;
    m_checked = false;
    m_hasMark = true;
    m_isMarkLeft = true;
    m_autoExclusive = true;
}

void LRadioButton::paintEvent(std::shared_ptr<LPaintEvent> e)
{
    LPainter painter(paintDevice());

    std::shared_ptr<LStyle> pStyle = style();
    std::shared_ptr<LWidget> f = getPtrT<LWidget>();
    pStyle->drawFrame(f, &painter, *this);
    if(m_checked){
        pStyle->drawLabel(f, &painter, *this, LDrawMark::CheckedRadio);
    }else{
        pStyle->drawLabel(f, &painter, *this, LDrawMark::UncheckedRadio);
    }
}





LK_NAMESPACE_END


