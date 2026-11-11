#include "lgroupbox.h"
#include "lkwidgets/draw/lstyle.h"

LK_NAMESPACE_BEGIN

LGroupBox::LGroupBox(std::shared_ptr<LWidget> parent, LFlags<LWindowType> f):LFrame(parent, f) {
    m_sizePolicy = LSizePolicy(LSizePolicy::Preferred, LSizePolicy::Preferred, LSizePolicy::GroupBox);
    m_shape =LFrameData::Box;
    m_lineWidth = 1;
    m_shadow = LFrameData::Sunken;
}

LGroupBox::LGroupBox(const std::wstring &text, std::shared_ptr<LWidget> parent, LFlags<LWindowType> f)
    :LFrame(parent, f),
    m_title(text)
{
    m_sizePolicy = LSizePolicy(LSizePolicy::Preferred, LSizePolicy::Preferred, LSizePolicy::GroupBox);
    m_shape =LFrameData::Box;
    m_lineWidth = 1;
    m_shadow = LFrameData::Sunken;
}

void LGroupBox::setCheckable(bool checkable)
{
    if (m_checkable != checkable) {
        m_checkable = checkable;

        if (checkable) {
            setChecked(true);
            setFocusPolicy(LFocusPolicy::Strong);
            setChildrenEnabled(true);
            updateGeometry();
        } else {
            setFocusPolicy(LFocusPolicy::No);
            setChildrenEnabled(true);
            updateGeometry();
        }
        update();
    }
}

void LGroupBox::setChecked(bool checked)
{
    if (m_checkable && m_checked != checked) {
        m_checked = checked;
        setChildrenEnabled(m_checked);
        update();
    }
}

void LGroupBox::setTitle(const std::wstring &text)
{
    if (m_title != text) {
        m_title = text;
        update();
    }
}

LRect<int> LGroupBox::customRect() const
{
    LStyleOptGroupBox opt;
    initStyleOpt(opt);
    std::shared_ptr<LStyle> pStyle = style();
    return pStyle->groupBoxSubCtrlRect(opt, 1<<(uint8_t)LUiSubId::GroupBox::Contents, m_title);
}

int LGroupBox::heightForWidth(int w) const
{
    LFontMetrics metrics(fontMetrics());

    std::shared_ptr<LStyle> pStyle = style();
    int baseHeight = std::lround(metrics.height());
    if (m_checkable) {
        int iconSize = pStyle->get(&LStylePixelMetric::smallIconSize, &m_elementStyle);
        baseHeight = std::max<int>(baseHeight, iconSize);
    }
    baseHeight += boundWidth(LDirection::ToTop,LRenderRule::All)+boundWidth(LDirection::ToBottom, LRenderRule::All);
    if (m_layout != nullptr){
        if(m_layout->hasHeightForWidth()){
            return baseHeight + m_layout->totalHeightForWidth(w);
        }
    }
    return baseHeight;
}

LSize<int> LGroupBox::sizeHint() const
{
    LSize<int> s;
    if (m_layout != nullptr){
        s = m_layout->totalSizeHint();
    }
    s += frameRequireSize();
    return s;
}

LSize<int> LGroupBox::minimumSizeHint() const
{
    LSize<int> s;
    if (m_layout != nullptr){
        s = m_layout->totalMinimumSize();
    }
    s += frameRequireSize();
    return s.expandedTo(LWidget::minimumSizeHint());
}

void LGroupBox::paintEvent(std::shared_ptr<LPaintEvent> e)
{
    LPainter p(paintDevice());
    LStyleOptGroupBox opt;
    initStyleOpt(opt);

    std::shared_ptr<LStyle> pStyle = style();

    LRect<int> bRect =  pStyle->groupBoxSubCtrlRect(opt, 1<<(uint8_t)LUiSubId::GroupBox::Frame, m_title);
    std::shared_ptr<LRegion> borderReg = std::make_shared<LRegion>();
    borderReg->unite(bRect);

    LRect<int> contentRect =  pStyle->groupBoxSubCtrlRect(opt, 1<<(uint8_t)LUiSubId::GroupBox::Contents, m_title);
    borderReg->subtract(contentRect);


    if(m_checkable){
        LRect<int> markRect = pStyle->groupBoxSubCtrlRect(opt, 1<<(uint8_t)LUiSubId::GroupBox::CheckBox, m_title);
        borderReg->subtract(markRect);

        if(m_checked){
            pStyle->drawMark(&p, markRect.toRectF(), LDrawMark::CheckedBox, &m_elementStyle, opt.state);
        }else{
            pStyle->drawMark(&p, markRect.toRectF(), LDrawMark::UncheckedBox, &m_elementStyle, opt.state);
        }
    }

    if(!m_title.empty()){
        LRect<int> textRect = pStyle->groupBoxSubCtrlRect(opt, 1<<(uint8_t)LUiSubId::GroupBox::Label, m_title);
        uint16_t state = uiElementState(LUiElementType::Title);
        std::shared_ptr<LRenderRule> rule = m_elementStyle.getRenderRule(LUiElementType::Title, state);
        if(rule != nullptr){
            if (rule->hasDrawable()) {
                rule->drawRule(&p, textRect);
            }
        }
        borderReg->subtract(textRect);
        p.drawText(textRect.toRectF(), m_title, m_titleAlignment);
    }


    if(bRect.isValid()){
        p.setClipRegion(borderReg);

        uint16_t state = uiElementState(LUiElementType::None);
        std::shared_ptr<LRenderRule> rule = m_elementStyle.getRenderRule(LUiElementType::None, state);
        if(rule != nullptr){
            if (rule->hasBorder()) {
                rule->drawBorder(&p, bRect);
            } else {
                pStyle->drawFrame(bRect, &p, palette(),foregroundRole(), m_shape, m_shadow, m_lineWidth, m_midLineWidth);
            }
        }else{
            pStyle->drawFrame(bRect, &p, palette(), foregroundRole(), m_shape, m_shadow, m_lineWidth, m_midLineWidth);
        }
    }

}

void LGroupBox::mousePressEvent(std::shared_ptr<LMouseEvent> ev)
{
    if (ev->button() != LMouseButton::Left) {
        ev->ignore();
        return;
    }

    if(m_checkable ==false){
        return;
    }

    std::shared_ptr<LStyle> pStyle = style();
    LStyleOptGroupBox opt;
    initStyleOpt(opt);

    LUiSubCtrl ctrl = pStyle->hitTestGroupBox(opt, ev->pos(), m_title);
    if(ctrl.test((uint8_t)LUiSubId::GroupBox::CheckBox)
        || ctrl.test((uint8_t)LUiSubId::GroupBox::Label)){
        setChecked(!m_checked);
    }
}


void LGroupBox::setChildrenEnabled(bool b)
{
    for (const std::shared_ptr<LTreeObj>& o : m_children) {
        std::shared_ptr<LWidget> w = std::dynamic_pointer_cast<LWidget>(o);
        if (w != nullptr) {
            if (b) {
                w->setEnabled(true);
            } else {
                if (w->isEnabled()) {
                    w->setEnabled(false);
                }
            }
        }
    }
}

void LGroupBox::initStyleOpt(LStyleOptGroupBox &opt) const
{
    LFrame::initStyleOpt(opt);
    opt.font = font();
    opt.titleAlignment = m_titleAlignment;
    opt.subCtrls = 1<<(uint8_t)LUiSubId::GroupBox::Label
                   |1<<(uint8_t)LUiSubId::GroupBox::Contents
                   |1<<(uint8_t)LUiSubId::GroupBox::Frame;
    if(m_checkable){
        opt.subCtrls |= 1<<(uint8_t)LUiSubId::GroupBox::CheckBox;
    }
}

LSize<int> LGroupBox::frameRequireSize() const
{
    LFontMetrics metrics(fontMetrics());

    std::shared_ptr<LStyle> pStyle = style();
    int baseWidth = std::lround(metrics.horizontalAdvance(m_title) + metrics.horizontalAdvance(wchar_t(' ')));
    int baseHeight = std::lround(metrics.height());
    if (m_checkable) {
        int iconSize = pStyle->get(&LStylePixelMetric::smallIconSize, &m_elementStyle);
        baseWidth += iconSize;
        baseWidth += pStyle->get(&LStylePixelMetric::itemMarkSpan, &m_elementStyle);
        baseHeight = std::max<int>(baseHeight, iconSize);
    }

    LMargins<int> bounds = boundWidths(LRenderRule::StyleAll);
    return LSize<int>(baseWidth + bounds.m_left+bounds.m_right, baseHeight +bounds.m_bottom+bounds.m_top);
}


LK_NAMESPACE_END
