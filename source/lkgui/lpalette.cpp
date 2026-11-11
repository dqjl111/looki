#include "lpalette.h"

LK_NAMESPACE_BEGIN

LPalette::LPalette()
{
    m_currentGroup = Active;
    m_resolveMask = 0;
}

const std::shared_ptr<LBrush> LPalette::brush(ColorGroup cg, ColorRole cr) const
{
    if(cg >= (int)NColorGroups) {
        if(cg == Current) {
            cg = (ColorGroup)m_currentGroup;
        } else {
            cg = Active;
        }
    }
    std::shared_ptr<LBrush> ret = m_brushes[cg][cr];
    if(ret == nullptr){
        if(cr == PlaceholderText){
            ret = std::make_shared<LBrush>();
        }else{
            ret = std::make_shared<LBrush>(LColorID::Black, LBrush::SolidPattern);
        }
        const_cast<LPalette*>(this)->m_brushes[cg][cr] = ret;
    }
    return ret;
}

void LPalette::setBrush(ColorGroup cg, ColorRole cr, const LBrush &b)
{
    if (cg == All) {
        for (uint32_t i = 0; i < NColorGroups; i++){
            setBrush(ColorGroup(i), cr, b);
        }
        return;
    }

    if (cg == Current) {
        cg = ColorGroup(m_currentGroup);
    } else if (cg >= NColorGroups) {
        cg = Active;
    }


    if(m_brushes[cg][cr] == nullptr){
        m_brushes[cg][cr] = std::make_shared<LBrush>();
    }

    if (*m_brushes[cg][cr] != b) {
        *m_brushes[cg][cr] = b;
    }
    m_resolveMask |= (1<<cr);
}


void LPalette::initPaletteFromColor(const LColor &color)
{
    LHsv hsv;
    color.toHsv(hsv);
    // inactive and active are the same..
    const LBrush whiteBrush = LBrush(LColorID::White);
    const LBrush blackBrush = LBrush(LColorID::Black);
    const LBrush baseBrush = hsv.m_value > 128 ? whiteBrush : blackBrush;
    const LBrush foregroundBrush = hsv.m_value > 128 ? blackBrush : whiteBrush;
    const LBrush buttonBrush = LBrush(color);
    const LBrush buttonBrushDark = LBrush(color.darker());
    const LBrush buttonBrushDark150 = LBrush(color.darker(150));
    const LBrush buttonBrushLight150 = LBrush(color.lighter(150));
    setColorGroup(LPalette::Active, foregroundBrush, buttonBrush,
                  buttonBrushLight150, buttonBrushDark, buttonBrushDark150,
                  foregroundBrush, whiteBrush, baseBrush,
                  buttonBrush);

    setColorGroup(LPalette::Inactive, foregroundBrush, buttonBrush,
                  buttonBrushLight150, buttonBrushDark, buttonBrushDark150,
                  foregroundBrush, whiteBrush, baseBrush,
                  buttonBrush);

    setColorGroup(LPalette::Disabled, buttonBrushDark, buttonBrush,
                  buttonBrushLight150, buttonBrushDark, buttonBrushDark150,
                  buttonBrushDark, whiteBrush, buttonBrush,
                  buttonBrush);
}

void LPalette::setColorGroup(ColorGroup cg, const LBrush &windowText, const LBrush &button,
                             const LBrush &light, const LBrush &dark, const LBrush &mid,
                             const LBrush &text, const LBrush &brightText, const LBrush &base,
                             const LBrush &window)
{
    LBrush altBase = LBrush(LColor::mixColors(base.m_color, button.m_color));
    LBrush midLight = LBrush(LColor::mixColors(button.m_color, light.m_color));
    LBrush toolTipBase(LColor(255, 255, 220));
    LBrush toolTipText(LColor(0, 0, 0));

    setColorGroup(cg, windowText, button, light, dark, mid, text, brightText, base,
                  altBase, window, midLight, text,
                  LBrush(LColorID::Black), LBrush(LColorID::DarkBlue), LBrush(LColorID::White),
                  LBrush(LColorID::Blue), LBrush(LColorID::Magenta), LBrush(toolTipBase),
                  LBrush(toolTipText));

    m_resolveMask &= ~(1 << Highlight);
    m_resolveMask &= ~(1 << HighlightedText);
    m_resolveMask &= ~(1 << LinkVisited);
    m_resolveMask &= ~(1 << Link);
}

void LPalette::setColorGroup(ColorGroup cg, const LBrush &windowText, const LBrush &button,
                             const LBrush &light, const LBrush &dark, const LBrush &mid,
                             const LBrush &text, const LBrush &brightText, const LBrush &base,
                             const LBrush &alternateBase, const LBrush &window, const LBrush &midlight,
                             const LBrush &buttonText, const LBrush &shadow, const LBrush &highlight,
                             const LBrush &highlightedText, const LBrush &link, const LBrush &linkVisited,
                             const LBrush &toolTipBase, const LBrush &toolTipText)
{
    setBrush(cg, WindowText, windowText);
    setBrush(cg, Button, button);
    setBrush(cg, Light, light);
    setBrush(cg, Dark, dark);
    setBrush(cg, Mid, mid);
    setBrush(cg, Text, text);
    setBrush(cg, BrightText, brightText);
    setBrush(cg, Base, base);
    setBrush(cg, AlternateBase, alternateBase);
    setBrush(cg, Window, window);
    setBrush(cg, Midlight, midlight);
    setBrush(cg, ButtonText, buttonText);
    setBrush(cg, Shadow, shadow);
    setBrush(cg, Highlight, highlight);
    setBrush(cg, HighlightedText, highlightedText);
    setBrush(cg, Link, link);
    setBrush(cg, LinkVisited, linkVisited);
    setBrush(cg, ToolTipBase, toolTipBase);
    setBrush(cg, ToolTipText, toolTipText);

    LColor col = brush(Text)->m_color;
    col.m_alpha = 128;
    setBrush(cg, PlaceholderText, LBrush(col, LBrush::NoPattern));
}

bool LPalette::isEqual(ColorGroup group1, ColorGroup group2) const
{
    if(group1 >= (int)NColorGroups) {
        if(group1 == Current) {
            group1 = (ColorGroup)m_currentGroup;
        } else {
            group1 = Active;
        }
    }
    if(group2 >= (int)NColorGroups) {
        if(group2 == Current) {
            group2 = (ColorGroup)m_currentGroup;
        } else {
            group2 = Active;
        }
    }
    if(group1 == group2)
        return true;
    for(int role = 0; role < (int)NColorRoles; role++) {
        if(m_brushes[group1][role] != m_brushes[group2][role]){
            return false;
        }
    }
    return true;
}

void LPalette::initDefaultPalette()
{
    // Windows 11 浅色 (light) 风格中性配色
    const LColor windowColor(243, 243, 243);        // #F3F3F3 窗口背景
    const LColor baseColor(255, 255, 255);          // #FFFFFF 输入类控件背景
    const LColor buttonColor(237, 237, 237);        // #EDEDED 按钮背景
    const LColor altBaseColor(247, 247, 247);       // #F7F7F7 交替行背景
    const LColor textColor(27, 27, 27);             // #1B1B1B 正文
    const LColor disabledColor(159, 159, 159);      // #9F9F9F 禁用文字
    const LColor lightColor = baseColor;
    const LColor midlightColor(227, 227, 227);      // #E3E3E3 悬停填充
    const LColor midColor(207, 207, 207);           // #CFCFCF 控件描边/分隔线
    const LColor darkColor(214, 214, 214);          // #D6D6D6 按下填充
    const LColor shadowColor(178, 178, 178);        // #B2B2B2 滚动条滑块/标记描边
    const LColor disabledBaseColor(252, 252, 252);  // #FCFCFC 禁用输入框背景
    const LColor highlightColor(0, 103, 192);       // #0067C0 系统强调色
    const LColor highlightInactiveColor(198, 198, 198);
    const LColor highlightDisabledColor(190, 190, 190);
    const LColor highlightedTextColor(255, 255, 255);
    const LColor linkColor(0, 103, 192);
    const LColor linkVisitedColor(107, 105, 214);   // #6B69D6

    const LBrush windowBrush(windowColor);
    const LBrush lightBrush(lightColor);

    setColorGroup(All, LBrush(textColor), LBrush(buttonColor), lightBrush,
                  LBrush(darkColor), LBrush(midColor), LBrush(textColor), lightBrush,
                  LBrush(baseColor), windowBrush);

    setBrush(LPalette::Midlight, midlightColor);
    setBrush(LPalette::Button, buttonColor);
    setBrush(LPalette::AlternateBase, altBaseColor);
    setBrush(LPalette::Shadow, shadowColor);
    setBrush(LPalette::HighlightedText, highlightedTextColor);
    setBrush(LPalette::Link, linkColor);
    setBrush(LPalette::LinkVisited, linkVisitedColor);
    setBrush(LPalette::ToolTipBase, LColor(249, 249, 249));
    setBrush(LPalette::ToolTipText, textColor);

    setBrush(LPalette::Disabled, LPalette::Text, disabledColor);
    setBrush(LPalette::Disabled, LPalette::WindowText, disabledColor);
    setBrush(LPalette::Disabled, LPalette::ButtonText, disabledColor);
    setBrush(LPalette::Disabled, LPalette::Base, disabledBaseColor);
    setBrush(LPalette::Disabled, LPalette::Window, windowColor);
    setBrush(LPalette::Disabled, LPalette::Button, LColor(243, 243, 243));
    setBrush(LPalette::Disabled, LPalette::Mid, midColor);
    setBrush(LPalette::Disabled, LPalette::Dark, darkColor);
    setBrush(LPalette::Disabled, LPalette::Shadow, shadowColor);
    setBrush(LPalette::Disabled, LPalette::PlaceholderText,
             LBrush(LColor(159, 159, 159, 128), LBrush::NoPattern));

    setBrush(LPalette::Active, LPalette::Highlight, highlightColor);
    setBrush(LPalette::Inactive, LPalette::Highlight, highlightInactiveColor);
    setBrush(LPalette::Disabled, LPalette::Highlight, highlightDisabledColor);
}


LK_NAMESPACE_END
