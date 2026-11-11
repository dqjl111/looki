#ifndef LWIDGETSTYLE_H
#define LWIDGETSTYLE_H

#include "lkgui/image/ltexture.h"
#include "lkwidgets/draw/ldrawutil.h"
#include <unordered_map>


LK_NAMESPACE_BEGIN
enum class LUiComplexCtrl : uint8_t{
    Default,
    SpinBox,
    ComboBox,
    ScrollBar,
    Slider,
    Button,
    TitleBar,
    Dial,
    GroupBox,
    MdiControls
};


namespace LUiSubId {
static const uint8_t None =0;

enum class ScrollBar : uint8_t{
    AddLine,
    SubLine,
    AddPage,
    SubPage,
    Slider,
    Groove,
};

enum class SpinBox : uint8_t{
    Up,
    Down,
    EditField,
    Frame,
};

enum class ComboBox : uint8_t{
    Arrow,
    EditField,
    ListBoxPopup,
    Frame,
};

enum class Slider : uint8_t{
    Groove,
    Handle,
    Tickmarks,
};

enum class Button : uint8_t{
    MenuArrow,
    MenuButton
};

enum class TitleBar : uint8_t{
    SysMenu,
    MinButton,
    MaxButton,
    CloseButton,
    NormalButton,
    ShadeButton,
    UnshadeButton,
    ContextHelpButton,
    Label,
};

enum class Dial : uint8_t{
    Groove,
    Handle,
    Tickmarks,
};

enum class GroupBox : uint8_t{
    CheckBox,
    Label,
    Contents,
    Frame,
};

enum class Mdi : uint8_t{
    MinButton,
    NormalButton,
    CloseButton,
};
}


typedef std::bitset<16> LUiSubCtrl;
typedef lcss::PseudoState LUiState;
typedef lcss::PseudoElementType LUiElementType;

enum class LMathMLLineBreakStyle :uint8_t{
    Before,
    After,
    Duplicate
};


enum class LMathMLUnit :uint8_t{
    em, //an em (font-relative unit traditionally used for horizontal lengths)
    ex, //an ex (font-relative unit traditionally used for vertical lengths)
    px, //pixels, or size of a pixel in the current display
    in, //inches (1 inch = 2.54 centimeters)
    cm, //centimeters
    mm, //millimeters
    pt, //points (1 point = 1/72 inch)
    pc, //picas (1 pica = 12 points)
    percent //% percentage of the default value
};

enum class LMathMLSpacing : int8_t{
    Nonespace = 0,
    Veryverythinmathspace=1, //1/18em
    Verythinmathspace=2,     //2/18em
    Thinmathspace=3,         //3/18em
    Mediummathspace=4,       //4/18em
    Thickmathspace=5,        //5/18em
    Verythickmathspace=6,    //6/18em
    Veryverythickmathspace=7, //7/18em
    NegativeVeryverythinmathspace=-1, //-1/18em
    NegativeVerythinmathspace=-2,     //-2/18em
    NegativeThinmathspace=-3,         //-3/18em
    NegativeMediummathspace=-4,       //-4/18em
    NegativeThickmathspace=-5,        //-5/18em
    NegativeVerythickmathspace=-6,    //-6/18em
    NegativeVeryverythickmathspace=-7, //-7/18em
};

enum class LDrawMark{
    None,
    Left,
    Right,
    Up,
    Down,
    RightBottom,
    LeftBottom,
    LeftUp,
    RightUp,
    Checked,
    Crossed,
    CheckedBox,
    UncheckedBox,
    CrossedBox,
    CheckedRadio,
    UncheckedRadio
};

//修改对齐方式为1个字节
#pragma pack(push)
#pragma pack(1)
struct LStylePixelMetric
{
    uint16_t toolBarIconSize = 16;
    uint16_t tabBarIconSize = 16;
    uint16_t listViewIconSize =24;
    uint16_t iconViewIconSize =32;
    uint16_t smallIconSize =16;
    uint16_t largeIconSize =32;
    uint16_t iconSpan = 1;
    uint16_t menuDesktopFrameWidth = 1;
    uint16_t menuScrollerHeight = 14;
    uint16_t menuScrollerWidth = 18;
    uint16_t menuItemAddWidth = 12;
    uint16_t menuBarItemMargin = 8;
    uint16_t subMenuOverlap = 1;
    uint16_t checkItemAddWidth = 7;
    uint16_t itemMarkSpan = 7;
    uint16_t sliderLength = 11;
    uint16_t sliderThickness = 11;
    uint16_t scrollBarExtent = 18;
    uint16_t scrollBarSliderMin = 9;
    uint16_t editButtonLength = 18;
    uint16_t tabBarItemMargin = 8;
    uint16_t controlRadius = 4;
};

struct LStyleBoundMetric
{
    uint16_t menuHMargin = 3;
    uint16_t menuVMargin = 3;
    uint16_t menuBorderWidth = 1;
    uint16_t menuBarHMargin = 3;
    uint16_t menuBarVMargin = 3;
    uint16_t menuBarBorderWidth = 0;
    uint16_t editHMargin =2;
    uint16_t editVMargin =1;
    uint16_t groupBoxVMargin = 3;
    uint16_t groupBoxHMargin = 3;
    uint16_t groupBoxTopPadding = 11;
    uint16_t groupBoxLeftPadding = 8;
    uint16_t groupBoxRightPadding = 3;
    uint16_t groupBoxBottomPadding = 3;
};


struct LMathMLStyle
{
    uint16_t sriptLevel = 0;
    bool displaystyle = false;
    float scriptsizeMultiplier = 1;
    uint16_t scriptMinSize = 8;
    uint8_t scriptMinSizeUnit = uint8_t(LMathMLUnit::pt);
    uint8_t infixLinebreakStyle;
    char decimalpoint = '.';
};
#pragma pack(pop)


enum class LStyleBit{
    InputPanelNeedFocus,
    Reserved,
    Num
};


class LKWIDGETSDEF LWidgetStyleSheet : public lcss::StyleSheet{
public:
    LWidgetStyleSheet();
    virtual ~LWidgetStyleSheet();
    virtual const lcss::ElementTag* getRegistTag(const std::string& tagName, bool create) const override;

    void clear();

    bool isSelectorMatch(const lcss::Selector& selector,
                         const LIVarTag* tag,
                         const uint16_t states,
                         const std::string& objName = LStringUtil::empty,
                         const LUiElementType element = LUiElementType::None) const;

    bool computeStyle(std::shared_ptr<lcss::StyleComputed> style,
                      const LIVarTag* tag,
                      const uint16_t states,
                      const std::string& objName = LStringUtil::empty,
                      const LUiElementType element = LUiElementType::None) const;


    inline void setCachedRender(const LIVarTag* tag, uint32_t key, std::shared_ptr<LRenderRule> style);
    std::shared_ptr<LRenderRule> getCachedRender(const LIVarTag* tag, uint32_t key) const;
    inline bool isObjectStyle(const std::string& objName) const{
        return m_ids.contains(objName);
    }
    void clearCachedRender();

    virtual void buildSelector() override;
protected:
    std::unordered_set<std::string> m_ids;
    std::map<uint32_t, std::unordered_map<const LIVarTag*, std::shared_ptr<LRenderRule> > > m_cachedRender;
};


class LWidget;
class LKWIDGETSDEF LWidgetStyle{
public:
    LWidgetStyle();

    void set(uint16_t LStylePixelMetric::* k, uint16_t v){
        uint16_t key = (uint16_t)lMemberAddr(k);
        m_pixmetric[key] = v;
    }

    void erase(uint16_t LStylePixelMetric::* k){
        uint16_t key = (uint16_t)lMemberAddr(k);
        m_pixmetric.erase(key);
    }

    uint16_t get(uint16_t LStylePixelMetric::* k, bool* find= nullptr) const{
        uint16_t key = (uint16_t)lMemberAddr(k);
        std::map<uint16_t, uint16_t>::const_iterator iter = m_pixmetric.find(key);
        if(iter != m_pixmetric.cend()){
            if(find!= nullptr){
                *find = true;
            }
            return iter->second;
        }
        if(find!= nullptr){
            *find = false;
        }
        return 0;
    }
    inline const std::shared_ptr<LWidget> widget() const{
        return m_widget.lock();
    }

    inline static uint32_t cachedStyleKey(LUiElementType element = LUiElementType::None, uint16_t state=0)
    {
        return (uint32_t)(element) << 16 | (uint32_t)(state);
    }

    bool clearCachedStyle();
    union{
        uint8_t m_styleFlags = 0;
        struct{
            uint8_t m_shieldParentStyleSheet : 1;
            uint8_t m_hasCachedWithCssId : 1;
        };
    };

    std::shared_ptr<LWidgetStyleSheet> m_styleSheet = nullptr;
    std::shared_ptr<LRenderRule> getRenderRule(LUiElementType element = LUiElementType::None, uint16_t state =0) const;
protected:
    std::map<uint32_t, std::shared_ptr<LRenderRule> > m_cachedRender;
    std::map<uint16_t, uint16_t> m_pixmetric;
    std::weak_ptr<LWidget> m_widget;
    friend class LWidget;
};

LK_NAMESPACE_END
#endif // LWIDGETSTYLE_H
