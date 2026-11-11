#ifndef LCSSPARSER_H
#define LCSSPARSER_H

#include "lkcore/lflags.h"
#include "lkcore/lvariant.h"
#include <vector>
#include <unordered_set>
#include "lkgui/paint/lcolor.h"


LK_NAMESPACE_BEGIN
namespace lcss {
class StringSection {
public:
    StringSection(const std::string& input);

    /// Generate a subsection of this string
    StringSection section(const size_t start, const size_t end) const;
    /// Generate a subsection until the next occurrence of char
    StringSection sectionUntil(const char c, size_t& pos) const;
    uint32_t hash() const;

    std::string str() const; // warning: string allocation
    char operator[](const size_t i) const;
    size_t length() const;
    size_t find(const char c, const size_t pos = 0) const;
    size_t find(const std::string& s, const size_t pos = 0) const;

    inline const size_t start() const{
        return m_start;
    }

    inline const size_t end() const{
        return m_end;
    }
private:
    StringSection(const StringSection& input, const size_t start, const size_t end);

    std::shared_ptr<std::string> m_string;
    size_t m_start, m_end; // [ )
};


/// Text alignment
enum class TextAlign { Left, Center, Right, Justify };

/// Text wrapping
enum class TextWrap { None, Char, Word };


enum class PseudoState : uint16_t {
    None =          0,
    Hover =    1 << 0,
    Disabled = 1 << 1,
    Enabled =  1 << 2,
    Checked =  1 << 3,
    Active =   1 << 4,
    Focus =    1 << 5,
    Selected = 1 << 6,
    Raised =   1 << 7,
    Sunken =   1 << 8,
    Horizontal = 1 << 9
};



enum class SelectorIdentifierType : uint8_t {
    Tag   = 0, //可标记c++类
    Id    = 1, //可标记c++类对象
    Class = 2,  //css的类不是c++的类, 当然也可以看成多个父类, 更像是java类是以.分包的
    PseudoElement = 3
};

enum class SelectorRelationship : uint8_t {
    None, // same target (for example tag.class:pseudo)
    Descendant, // default (any in subtree)
    Child, // >
    AdjacentSibling, // +
    GeneralSibling // ~
};

enum class Repeat : uint8_t{
    None,
    X,
    Y,
    XY
};

enum class Origin : uint8_t{
    Border,
    Content,
    Margin,
    Padding
};

enum class Attachment : uint8_t{
    Scroll,
    Fixed,
    Local
};

enum class PseudoElementType : uint8_t {
    None,
    AddPage,
    Branch,
    Chunk,
    CloseButton,
    Corner,
    DownArrow,
    DownButton,
    DropDown,
    FloatButton,
    Groove,
    Indicator,
    Handle,
    Icon,
    Item,
    LeftArrow,
    LeftCorner,
    MenuArrow,
    MenuButton,
    MenuIndicator,
    RightArrow,
    Pane,
    RightCorner,
    Scroller,
    Section,
    Separator,
    SubLine,
    SubPage,
    Tab,
    TabBar,
    Tear,
    Tearoff,
    Text,
    Title,
    UpArrow,
    UpButton
};

class LKGUIDEF ElementTag{
public:
    ElementTag();
    virtual ~ElementTag();
    virtual bool isMatch(const ElementTag* other) const;
    virtual bool isMatchBefore(const ElementTag* other )const;
    virtual bool isMatchAfter(const ElementTag* other )const;
};

struct SelectorIdentifier {
    SelectorIdentifierType type;
    PseudoElementType pseudoElement;
    const ElementTag* tag;
    std::string value;

    inline SelectorIdentifier(){
        type = SelectorIdentifierType::Tag;
        tag = nullptr;
    }
};

struct SelectorPart {
    SelectorIdentifier identifier;
    SelectorRelationship prevRel = SelectorRelationship::None;
    LFlags<PseudoState> pseudos;

    bool operator <(const SelectorPart &other) const;
    bool operator > (const SelectorPart &other) const;
    bool operator != (const SelectorPart &other) const;
    inline bool operator == (const SelectorPart &other) const{
        return *this != other;
    }
    inline bool operator <=(const SelectorPart &other) const{
        return !(*this > other);
    }
    inline bool operator >= (const SelectorPart &other) const{
        return !(*this < other);
    }
};

typedef std::vector<SelectorPart> Selector;

enum class StylePropertyID : uint8_t {
    Width,
    Height,
    MinWidth,
    MinHeight,
    MaxWidth,
    MaxHeight,

    MarginTop,
    MarginRight,
    MarginBottom,
    MarginLeft,

    PaddingTop,
    PaddingRight,
    PaddingBottom,
    PaddingLeft,

    BorderTopColor,
    BorderRightColor,
    BorderBottomColor,
    BorderLeftColor,
    BorderTopLeftRadius,
    BorderTopRightRadius,
    BorderBottomRightRadius,
    BorderBottomLeftRadius,
    BorderTopWidth,
    BorderRightWidth,
    BorderBottomWidth,
    BorderLeftWidth,    
    BorderTopStyle,
    BorderRightStyle,
    BorderBottomStyle,
    BorderLeftStyle,
    BorderImageSource,
    BorderImageTopSlice,
    BorderImageRightSlice,
    BorderImageBottomSlice,
    BorderImageLeftSlice,
    BorderImageHorizRepeat,
    BorderImageVertRepeat,

    FlexGrow,
    FlexShrink,
    FlexBasis,
    FlexDirection,
    FlexWrap,

    AlignSelf,
    AlignItems,
    AlignContent,
    JustifyContent,

    Position,

    Top,
    Right,
    Bottom,
    Left,

    Color,
    BackgroundColor,
    BackgroundImage,
    BackgroundRepeat,
    BackgroundOrigin,
    BackgroundClip,
    BackgroundAttachment,
    BackgroundPosition,

    Overflow,
    Display,
    Image,
    ImagePosition,

    TextAlign,
    FontFamily,
    FontSize,
    WhiteSpace,

    Cursor,
    // always keep it last
    RawProperty,
    LastPropertyInvalid
};

enum class PositionType {
    Relative,
    Absolute
};

typedef LFlags<LAlignment> Position;

enum class FlexDirection {
    Column,
    ColumnReverse,
    Row,
    RowReverse
};

enum class FlexWrap {
    NoWrap,
    Wrap,
    WrapReverse,
};

enum class Align {
    Auto,
    FlexStart,
    Center,
    FlexEnd,
    Stretch,
    Baseline,
    SpaceBetween,
    SpaceAround
};

enum class Justify {
    FlexStart,
    Center,
    FlexEnd,
    SpaceBetween,
    SpaceAround,
    SpaceEvenly
};

enum class Display {
    Flex,
    None
};

enum class Overflow {
    Visible,
    Hidden,
    Scroll
};

enum class WhiteSpace {
    Normal = 0,
    NoWrap,
};

enum class Cursor {
    Auto,
    Default,
    None,
    Help,
    Pointer,
    Progress,
    Wait,
    CrossHair,
    Text,
    Move,
    NotAllowed,
    AllScroll,
    ColResize,
    RowResize
};

enum class StyleLengthUnit {
    Auto,
    Percent,
    px, //像素(Pixel),相对于设备的长度单位，像素是相对于显示器屏幕分辨率而言的。譬如，WONDOWS的用户所使用的分辨率一般是96像素/英寸。而MAC的用户所使用的分辨率一般是72像素/英寸。
    in, //英寸(Inch)，绝对长度单位。1in = 96px = 2.54cm
    cm, // 1cm = 37.8px
    mm, // 1mm = 0.1cm = 3.78px
    em, //相对长度单位。相对于元素的字体大小（font-size），如果当前行内文本的字体尺寸未被人为设置，则相对于浏览器的默认字体尺寸
    vw, //相对长度单位。相对于视口*宽度的 1%；
    vh  //相对长度单位。相对于视口*高度的 1%；
};

enum class BorderStyle {
    None,
    Dotted,
    Dashed,
    Solid,
    Double,
    DotDash,
    DotDotDash,
    Groove,
    Ridge,
    Inset,
    Outset,
    Native,
    NumKnownBorderStyles
};

typedef LTileRule BorderImageRepeat;


struct StyleLength{
    StyleLengthUnit unit = StyleLengthUnit::Auto;
    float number = 0;
};

class LKGUIDEF StyleValue{

public:
    StyleValue(){}
    StyleValue(const StyleValue& s): var(s.var){}
    StyleValue(const std::string&  s): var(s){}
    StyleValue(float  f):var(f){}
    StyleValue(const StyleLength&  l): var((void*)&l, sizeof(StyleLength)) {}
    StyleValue(const LColor&  c): var(c.toUInt()) {}
    StyleValue(PositionType  p): var((uint32_t)p) {}
    StyleValue(FlexDirection  d): var((uint32_t)d) {}
    StyleValue(FlexWrap  w): var((uint32_t)w) {}
    StyleValue(Align  a): var((uint32_t)a) {}
    StyleValue(Justify  j): var((uint32_t)j) {}
    StyleValue(TextAlign  t): var((uint32_t)t) {}
    StyleValue(WhiteSpace  s): var((uint32_t)s) {}
    StyleValue(Display  d): var((uint32_t)d) {}
    StyleValue(Overflow  o): var((uint32_t)o) {}
    StyleValue(Cursor  c): var((uint32_t)c) {}
    StyleValue(BorderStyle  d): var((uint32_t)d) {}
    StyleValue(Repeat  d): var((uint32_t)d) {}
    StyleValue(Origin  d): var((uint32_t)d) {}
    StyleValue(Attachment  d): var((uint32_t)d) {}
    StyleValue(Position  d): var((uint32_t)d) {}
    StyleValue(BorderImageRepeat  d): var((uint32_t)d) {}


    const bool isNull() const{
        return var.isNull();
    }

    void clear(){
        var.clear();
    }

    inline const std::string str() const{
        return var.toString();
    }

    inline void setStr(const std::string& v){
        var.setString(v);
    }

    const StyleLength styleLength() const;
    inline void setStyleLength(const StyleLength& v){
        var.setByteArray((char*)&v, sizeof(StyleLength));
    }

    inline const float number() const{
        return var.toFloat();
    }
    inline void setNumber(float v){
        var.setFloat(v);
    }


    const float length() const;
    const StyleLengthUnit lengthUnit() const;
    inline const LColor color() const{
        return LColor::uintToColor(var.toUInt());
    }
    inline void setColor(const LColor v){
        var.setUInt32(v.toUInt());
    }

    inline const PositionType positionType() const{
        return (PositionType)var.toUInt();
    }
    inline void setPositionType(const PositionType v){
        var.setUInt32((uint32_t)v);
    }

    inline const FlexDirection flexDirection() const{
        return (FlexDirection)var.toUInt();
    }
    inline void setFlexDirection(const FlexDirection v){
        var.setUInt32((uint32_t)v);
    }

    inline const FlexWrap flexWrap() const{
        return (FlexWrap)var.toUInt();
    }
    inline void setFlexWrap(const FlexWrap v){
        var.setUInt32((uint32_t)v);
    }

    inline const Align align() const{
        return (Align)var.toUInt();
    }
    inline void setAlign(const Align v){
        var.setUInt32((uint32_t)v);
    }

    inline const Justify justify() const{
        return (Justify)var.toUInt();
    }
    inline void setJustify(const Justify v){
        var.setUInt32((uint32_t)v);
    }

    inline const TextAlign textAlign() const{
        return (TextAlign)var.toUInt();
    }
    inline void setTextAlign(const TextAlign v){
        var.setUInt32((uint32_t)v);
    }


    inline const WhiteSpace whiteSpace() const{
        return (WhiteSpace)var.toUInt();
    }
    inline void setWhiteSpace(const WhiteSpace v){
        var.setUInt32((uint32_t)v);
    }


    inline const Display display() const{
        return (Display)var.toUInt();
    }
    inline void setDisplay(const Display v){
        var.setUInt32((uint32_t)v);
    }


    inline const Overflow overflow() const{
        return (Overflow)var.toUInt();
    }
    inline void setOverflow(const Overflow v){
        var.setUInt32((uint32_t)v);
    }


    inline const Cursor cursor() const{
        return (Cursor)var.toUInt();
    }
    inline void setCursor(const Cursor v){
        var.setUInt32((uint32_t)v);
    }

    inline const BorderStyle borderStyle() const{
        return (BorderStyle)var.toUInt();
    }
    inline void setBorderStyle(const BorderStyle v){
        var.setUInt32((uint32_t)v);
    }

    inline const Repeat repeat() const{
        return (Repeat)var.toUInt();
    }
    inline void setRepeat(const Repeat v){
        var.setUInt32((uint32_t)v);
    }

    inline const Origin origin() const{
        return (Origin)var.toUInt();
    }
    inline void setOrigin(const Origin v){
        var.setUInt32((uint32_t)v);
    }

    inline const Attachment attachment() const{
        return (Attachment)var.toUInt();
    }
    inline void setAttachment(const Attachment v){
        var.setUInt32((uint32_t)v);
    }

    inline const Position position() const{
        return (Position)var.toUInt();
    }
    inline void setPosition(const Position v){
        var.setUInt32((uint32_t)v);
    }

    inline const BorderImageRepeat borderImageRepeat() const{
        return (BorderImageRepeat)var.toUInt();
    }
    inline void setBorderImageRepeat(const BorderImageRepeat v){
        var.setUInt32((uint32_t)v);
    }


    inline StyleValue &operator = (const StyleValue &obj) {
        var = obj.var;
        return *this;
    }
    inline StyleValue &operator = (StyleValue&& obj) noexcept {
        var = std::move(obj.var);
        return *this;
    }
private:
    LVariant var;
};


/// A single style property
class LKGUIDEF StyleProperty {
public:
    // The ownership of any valid pointer in the StyleValue parameter will
    // be transfered to this StyleProperty (for deletion)
    StyleProperty(const StylePropertyID id, const StyleValue& value);
    StyleProperty(const std::string& idName, const StyleValue& value);
    StyleProperty();

    // disable copying
    StyleProperty(const StyleProperty&) = default;
    StyleProperty& operator = (const StyleProperty&) = default;
    // enable move
    StyleProperty(StyleProperty&&) = default;
    StyleProperty& operator = (StyleProperty&&) = default;

    bool isValid() const { return m_id != StylePropertyID::LastPropertyInvalid;  }
    StylePropertyID getID() const { return m_id; }
    const StyleValue& getValue() const { return m_value; }

    std::string getIDName() const;
private:
    StylePropertyID m_id;
    StyleValue m_value;
    std::string m_rawIDName;
};

typedef std::vector<StyleProperty>  StyleProperties;
struct StyleRule {
    Selector selector;
    std::shared_ptr<StyleProperties> properties;
};


template<typename T>
struct StyleValueComputed {
    bool populated = false; // value is ready
    bool inherited = false; // value is being inherited
    T value;
};


struct LKGUIDEF StyleComputed {
    StyleValueComputed<StyleLength> marginTop;
    StyleValueComputed<StyleLength> marginRight;
    StyleValueComputed<StyleLength> marginBottom;
    StyleValueComputed<StyleLength> marginLeft;

    StyleValueComputed<StyleLength> paddingTop;
    StyleValueComputed<StyleLength> paddingRight;
    StyleValueComputed<StyleLength> paddingBottom;
    StyleValueComputed<StyleLength> paddingLeft;

    StyleValueComputed<StyleLength> width;
    StyleValueComputed<StyleLength> height;
    StyleValueComputed<StyleLength> minWidth;
    StyleValueComputed<StyleLength> minHeight;
    StyleValueComputed<StyleLength> maxWidth;
    StyleValueComputed<StyleLength> maxHeight;

    StyleValueComputed<LColor> borderTopColor;
    StyleValueComputed<LColor> borderRightColor;
    StyleValueComputed<LColor> borderBottomColor;
    StyleValueComputed<LColor> borderLeftColor;

    StyleValueComputed<StyleLength> borderTopLeftRadius;
    StyleValueComputed<StyleLength> borderTopRightRadius;
    StyleValueComputed<StyleLength> borderBottomRightRadius;
    StyleValueComputed<StyleLength> borderBottomLeftRadius;


    StyleValueComputed<StyleLength> borderTopWidth;
    StyleValueComputed<StyleLength> borderRightWidth;
    StyleValueComputed<StyleLength> borderBottomWidth;
    StyleValueComputed<StyleLength> borderLeftWidth;


    StyleValueComputed<BorderStyle> borderTopStyle;
    StyleValueComputed<BorderStyle> borderRightStyle;
    StyleValueComputed<BorderStyle> borderBottomStyle;
    StyleValueComputed<BorderStyle> borderLeftStyle;

    StyleValueComputed<std::string> borderImage;
    StyleValueComputed<StyleLength> borderImageTopSlice;
    StyleValueComputed<StyleLength> borderImageRightSlice;
    StyleValueComputed<StyleLength> borderImageBottomSlice;
    StyleValueComputed<StyleLength> borderImageLeftSlice;
    StyleValueComputed<BorderImageRepeat> borderImageHorizRepeat;
    StyleValueComputed<BorderImageRepeat> borderImageVertRepeat;

    StyleValueComputed<float> flexGrow;
    StyleValueComputed<float> flexShrink;
    StyleValueComputed<StyleLength> flexBasis;
    StyleValueComputed<FlexDirection> flexDirection;
    StyleValueComputed<FlexWrap> flexWrap;

    StyleValueComputed<Align> alignSelf;
    StyleValueComputed<Align> alignItems;
    StyleValueComputed<Align> alignContent;
    StyleValueComputed<Justify> justifyContent;

    StyleValueComputed<PositionType> positionType;

    StyleValueComputed<StyleLength> top;
    StyleValueComputed<StyleLength> right;
    StyleValueComputed<StyleLength> bottom;
    StyleValueComputed<StyleLength> left;


    StyleValueComputed<LColor> color;
    StyleValueComputed<LColor> backgroundColor;
    StyleValueComputed<std::string> backgroundImage;
    StyleValueComputed<Repeat> backgroundRepeat;
    StyleValueComputed<Origin> backgroundOrigin;
    StyleValueComputed<Origin> backgroundClip;
    StyleValueComputed<Attachment> backgroundAttachment;
    StyleValueComputed<Position> backgroundPosition;

    StyleValueComputed<Overflow> overflow;
    StyleValueComputed<Display> display;
    StyleValueComputed<std::string> image;
    StyleValueComputed<Position> imagePosition;

    StyleValueComputed<TextAlign> textAlign;
    StyleValueComputed<std::string> fontFamily;
    StyleValueComputed<StyleLength> fontSize;
    StyleValueComputed<WhiteSpace> whiteSpace;

    StyleValueComputed<Cursor> cursor;

    std::unordered_map<std::string, std::string> rawProperties;

    void applyProperty(const StyleProperty& property);
    void applyRule(const std::shared_ptr<StyleRule> rule);
    void inherit(const std::shared_ptr<StyleComputed> source);

    void buildDefaultStyleValues();

    const StyleValueComputed<StyleLength>& margin(LDirection dir) const;
    const StyleValueComputed<StyleLength>& padding(LDirection dir) const;

    const StyleValueComputed<LColor>& borderColor(LDirection dir) const;
    const StyleValueComputed<StyleLength>& borderRadius(LCorner dir) const;
    const StyleValueComputed<StyleLength>& borderWidth(LDirection dir) const;
    const StyleValueComputed<BorderStyle>& borderStyle(LDirection dir) const;
    const StyleValueComputed<StyleLength>& borderImageSlice(LDirection dir) const;

    const StyleValueComputed<StyleLength>& position(LDirection dir) const;
};

class LKGUIDEF StyleSheet {
public:
    enum ValueMode{
        NoRaw,
        AllowRaw,
        OnlyRaw
    };

    StyleSheet(ValueMode m= NoRaw);
    virtual ~StyleSheet();

    void addRule(const std::shared_ptr<StyleRule>& rule);
    const std::vector<std::shared_ptr<StyleRule>>& getRules() const { return m_rules; }
    virtual void buildSelector();
    virtual const ElementTag* getRegistTag(const std::string& tagName, bool create) const;
    void sortRules();
    const inline ValueMode valueMode() const{
        return m_valueMode;
    }

    bool isSelectorMatch(const Selector& selector,
                         const ElementTag* tag,
                         const uint16_t states,
                         const std::string& objName = LStringUtil::empty,
                         const PseudoElementType element = PseudoElementType::None) const;

    bool computeStyle(std::shared_ptr<StyleComputed> style,
                      const ElementTag* tag,
                      const uint16_t states,
                      const std::string& objName = LStringUtil::empty,
                      const PseudoElementType element = PseudoElementType::None) const;

    bool isSelectorMatch(const Selector& selector,
                         const std::string& tagName,
                         const uint16_t states,
                         const std::string& objName = LStringUtil::empty,
                         const PseudoElementType element = PseudoElementType::None) const;

    bool computeStyle(std::shared_ptr<StyleComputed> style,
                      const std::string& tagName,
                      const uint16_t states,
                      const std::string& objName = LStringUtil::empty,
                      const PseudoElementType element = PseudoElementType::None) const;
protected:
    std::vector<std::shared_ptr<StyleRule>> m_rules;
    ValueMode m_valueMode;
};


class LKGUIDEF Parser
{
protected:
    static bool parseNumber(const StringSection& input, size_t& pos, float& output);
    static bool parseLength(const StringSection& input, StyleLength& output);
    static bool parseString(const StringSection& input, std::string& str);
    static bool parsePosition(const StringSection& input, Position& pos);

    static bool parseStylePropertyLine(const StringSection& line, std::shared_ptr<StyleProperties> properties, StyleSheet::ValueMode mode);
    static bool parseStylePropertiesBlock(const StringSection& block, std::shared_ptr<StyleProperties> properties, StyleSheet::ValueMode mode);
    static std::string parseIdentifier(const StringSection& input, size_t& pos);
    static bool parseSingleSelector(const StringSection& inputSelector, Selector& selector);
    static bool parseSelectors(const StringSection& inputSelectors, std::vector<Selector>& selectors);


public:
    Parser();

    static bool parseColor(const StringSection& input, LColor& output);
    static std::shared_ptr<StyleSheet> parseStyleSheet(const std::string& rawCss);
    static std::shared_ptr<StyleSheet> parseFile(const std::string& filePath);

    static bool parseStyleSheet(const std::string& rawCss, std::shared_ptr<StyleSheet> styleSheet);
    static bool parseFile(const std::string& filePath, std::shared_ptr<StyleSheet> styleSheet);
};

}

L_EN_FLAGS(lcss::PseudoState)
typedef lcss::Parser LCssParser;
typedef lcss::Parser LCssStyleSheet;
LK_NAMESPACE_END

#endif // LCSSPARSER_H
