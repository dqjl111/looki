#include "llineedit.h"
#include "lkwidgets/draw/lstyle.h"
#include "lkwidgets/core/lapplication.h"
#include "lkwidgets/widget/lmenu.h"
#include "lkcore/io/li18n.h"

LK_NAMESPACE_BEGIN

LLineEdit::LLineEdit(std::shared_ptr<LWidget> parent, LFlags<LWindowType> f, bool isRow): LAbstractLineEdit(parent, f)
{
    m_sizePolicy = LSizePolicy(LSizePolicy::Preferred, LSizePolicy::Preferred, LSizePolicy::LineEdit);
    if(isRow){
        m_editor = std::make_shared<LLineEditor>();
    }else{
        m_editor = std::make_shared<LTextLineEditor>();
    }
    m_editor->setMaxLength(65535);
}

LLineEdit::LLineEdit(std::shared_ptr<LTextLineEditor> editor,
                     std::shared_ptr<LWidget> parent,
                     LFlags<LWindowType> f): LAbstractLineEdit(parent, f)
{
    m_sizePolicy = LSizePolicy(LSizePolicy::Preferred, LSizePolicy::Preferred, LSizePolicy::LineEdit);
    if(editor != nullptr){
        m_editor = editor;
    }else{
        m_editor = std::make_shared<LTextLineEditor>();
        m_editor->setMaxLength(65535);
    }
}

LLineEdit::LLineEdit(std::shared_ptr<LLineEditor> editor, std::shared_ptr<LWidget> parent, LFlags<LWindowType> f)
    : LAbstractLineEdit(parent, f)
{
    m_sizePolicy = LSizePolicy(LSizePolicy::Preferred, LSizePolicy::Preferred, LSizePolicy::LineEdit);
    if(editor != nullptr){
        m_editor = editor;
    }else{
        m_editor = std::make_shared<LLineEditor>();
        m_editor->setMaxLength(65535);
    }
}

LSize<int> LLineEdit::sizeHint() const
{
    const int iconSize = style()->get(&LStylePixelMetric::smallIconSize, &m_elementStyle);
    LFontMetrics fm(font());
    std::shared_ptr<LEditBase> base = m_editor->editBase();
    const LMargins<double>& tm = base->margins();
    const LMargins<int>& bounds = boundWidths();
    double x = fm.horizontalAdvance('x') * 17;

    double h = std::max<double>(fm.height(), std::max<double>(14, iconSize - 2))
                 + tm.m_top + tm.m_bottom + bounds.m_top+bounds.m_bottom;
    double w = x + tm.m_left + tm.m_right + bounds.m_right+bounds.m_left;
    double lw = lineWidth();
    return LSize<double>(w+2*lw, h+2*lw).toSize();
}

LSize<int> LLineEdit::minimumSizeHint() const
{
    LFontMetrics fm(font());
    std::shared_ptr<LEditBase> base = m_editor->editBase();
    const LMargins<double>& tm = base->margins();
    const LMargins<int>& bounds = boundWidths();

    double h = fm.height()  + tm.m_top + tm.m_bottom + bounds.m_top + bounds.m_bottom;
    double w = fm.maxWidth() + tm.m_left + tm.m_right + bounds.m_right+bounds.m_left;
    double lw = lineWidth();

    return LSize<double>(w+2*lw , h+2*lw).toSize();
}

std::wstring LLineEdit::text() const
{
    return m_editor->content();
}

void LLineEdit::setText(const std::wstring &s)
{
    m_editor->setContent(s);
    if(m_undoStack != nullptr){
        m_undoStack->clear();
    }
}

std::wstring LLineEdit::selectText() const
{
    return m_editor->selectContent();
}

void LLineEdit::paintEvent(std::shared_ptr<LPaintEvent> ev)
{
    LPainter p(paintDevice());
    std::shared_ptr<LPalette> pal = palette();
    std::shared_ptr<LStyle> pStyle = style();
    uint16_t state = uiElementState(LUiElementType::None);
    std::shared_ptr<LRenderRule> rule = m_elementStyle.getRenderRule(LUiElementType::None, state);
    bool hasRuleDraw = false;
    bool hasRuleBackground = false;
    if(rule != nullptr){
        if (rule->hasDrawable()) {
            rule->drawRule(&p, rule->borderRect(rectF()).toRect());
            hasRuleDraw = true;
            hasRuleBackground = rule->hasBackground();
        }
    }

    if(!hasRuleBackground && isEnabled()){
        p.fillRect(contentsRect().toRectF().adjusted(m_lineWidth, m_lineWidth, -m_lineWidth, -m_lineWidth), pal->brush(LPalette::Base));
    }

    if(hasRuleDraw == false) {
        // Windows 11 风格：聚焦时使用强调色描边
        if (isEnabled() && hasFocus()) {
            p.save();
            p.setRenderHints(LRenderHint::Antialiasing, true);
            const int radius = std::min<int>(pStyle->get(&LStylePixelMetric::controlRadius),
                                             std::min<int>(rect().m_width / 2, rect().m_height / 2));
            p.setBrush(LBrush::NoPattern);
            p.setPen(pal->brush(LPalette::Highlight)->m_color, LPenStyle::Solid, 2);
            p.drawRoundedRect(rect().toRectF().adjusted(1, 1, -1, -1),
                              radius, radius, LSizeMode::Absolute);
            p.restore();
        } else {
            pStyle->drawFrame(rect(), &p, pal, foregroundRole(), m_shape, m_shadow, m_lineWidth, m_midLineWidth);
        }
    }
    paintEditLine(p, pal);
}

LRect<int> LLineEdit::editRect() const
{
    return rect().marginsRemoved(boundWidths());
}

LK_NAMESPACE_END
