#ifndef LPALETTE_H
#define LPALETTE_H
#include "lkgui/paint/lbrush.h"

LK_NAMESPACE_BEGIN

class LKGUIDEF LPalette
{
public:
    LPalette();

    enum ColorGroup { Active, Disabled, Inactive, NColorGroups, Current, All, Normal = Active };
    enum ColorRole : uint8_t{
        WindowText, Button, Light, Midlight, Dark, Mid,
        Text, BrightText, ButtonText, Base, Window, Shadow,
        Highlight, HighlightedText,
        Link, LinkVisited,
        AlternateBase,
        NoRole,
        ToolTipBase, ToolTipText,
        PlaceholderText,
        NColorRoles = PlaceholderText + 1,
    };


    inline ColorGroup currentColorGroup() const { return static_cast<ColorGroup>(m_currentGroup); }
    inline void setCurrentColorGroup(ColorGroup cg) { m_currentGroup = cg; }

    inline const LColor &color(ColorGroup cg, ColorRole cr) const
    { return brush(cg, cr)->m_color; }
    const std::shared_ptr<LBrush> brush(ColorGroup cg, ColorRole cr) const;
    inline void setColor(ColorGroup cg, ColorRole cr, const LColor &color){
        setBrush(cg, cr, LBrush(color));
    }
    inline void setColor(ColorRole cr, const LColor &color){
        setColor(All, cr, color);
    }
    inline void setBrush(ColorRole cr, const LBrush &brush){
        setBrush(All, cr, brush);
    }
    bool isBrushSet(ColorGroup cg, ColorRole cr) const;
    void setBrush(ColorGroup cg, ColorRole cr, const LBrush &brush);
    void setColorGroup(ColorGroup cg, const LBrush &windowText, const LBrush &button,
                       const LBrush &light, const LBrush &dark, const LBrush &mid,
                       const LBrush &text, const LBrush &brightText, const LBrush &base,
                       const LBrush &window);
    bool isEqual(ColorGroup cr1, ColorGroup cr2) const;


    inline const LColor &color(ColorRole cr) const { return color(Current, cr); }
    inline const std::shared_ptr<LBrush> brush(ColorRole cr) const { return brush(Current, cr); }
    inline const std::shared_ptr<LBrush> windowText() const { return brush(WindowText); }
    inline const std::shared_ptr<LBrush> button() const { return brush(Button); }
    inline const std::shared_ptr<LBrush> light() const { return brush(Light); }
    inline const std::shared_ptr<LBrush> dark() const { return brush(Dark); }
    inline const std::shared_ptr<LBrush> mid() const { return brush(Mid); }
    inline const std::shared_ptr<LBrush> text() const { return brush(Text); }
    inline const std::shared_ptr<LBrush> base() const { return brush(Base); }
    inline const std::shared_ptr<LBrush> alternateBase() const { return brush(AlternateBase); }
    inline const std::shared_ptr<LBrush> toolTipBase() const { return brush(ToolTipBase); }
    inline const std::shared_ptr<LBrush> toolTipText() const { return brush(ToolTipText); }
    inline const std::shared_ptr<LBrush> window() const { return brush(Window); }
    inline const std::shared_ptr<LBrush> midlight() const { return brush(Midlight); }
    inline const std::shared_ptr<LBrush> brightText() const { return brush(BrightText); }
    inline const std::shared_ptr<LBrush> buttonText() const { return brush(ButtonText); }
    inline const std::shared_ptr<LBrush> shadow() const { return brush(Shadow); }
    inline const std::shared_ptr<LBrush> highlight() const { return brush(Highlight); }
    inline const std::shared_ptr<LBrush> highlightedText() const { return brush(HighlightedText); }
    inline const std::shared_ptr<LBrush> link() const { return brush(Link); }
    inline const std::shared_ptr<LBrush> linkVisited() const { return brush(LinkVisited); }
    inline const std::shared_ptr<LBrush> placeholderText() const { return brush(PlaceholderText); }

    void initDefaultPalette();
    void initPaletteFromColor(const LColor &color);
protected:
    std::shared_ptr<LBrush> m_brushes[NColorGroups][NColorRoles];
    uint32_t m_currentGroup : 4;
    uint32_t m_resolveMask : 28;

    void setColorGroup(ColorGroup cg, const LBrush &windowText, const LBrush &button,
                       const LBrush &light, const LBrush &dark, const LBrush &mid,
                       const LBrush &text, const LBrush &bright_text,
                       const LBrush &base, const LBrush &alternate_base,
                       const LBrush &window, const LBrush &midlight,
                       const LBrush &button_text, const LBrush &shadow,
                       const LBrush &highlight, const LBrush &highlighted_text,
                       const LBrush &link, const LBrush &link_visited,
                       const LBrush &toolTipBase, const LBrush &toolTipText);
};

LK_NAMESPACE_END

#endif // LPALETTE_H
