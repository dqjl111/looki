#include "lcssparser.h"
#include "lkcore/lstringutil.h"
#include "lkcore/ldebug.h"
#include <fstream>
#include <algorithm>
#include "lkcore/io/lresource.h"

LK_NAMESPACE_BEGIN

namespace lcss {


constexpr std::string_view propertyIdName(const StylePropertyID id)
{
    switch (id) {
    case StylePropertyID::Width:                    return "width";
    case StylePropertyID::Height:                   return "height";
    case StylePropertyID::MinWidth:                 return "min_width";
    case StylePropertyID::MinHeight:                return "min_height";
    case StylePropertyID::MaxWidth:                 return "max_width";
    case StylePropertyID::MaxHeight:                return "max_height";

    case StylePropertyID::MarginTop:                return "margin_top";
    case StylePropertyID::MarginRight:              return "margin_right";
    case StylePropertyID::MarginBottom:             return "margin_bottom";
    case StylePropertyID::MarginLeft:               return "margin_left";

    case StylePropertyID::PaddingTop:               return "padding_top";
    case StylePropertyID::PaddingRight:             return "padding_right";
    case StylePropertyID::PaddingBottom:            return "padding_bottom";
    case StylePropertyID::PaddingLeft:              return "padding_left";

    case StylePropertyID::BorderTopColor:           return "border_top_color";
    case StylePropertyID::BorderRightColor:         return "border_right_color";
    case StylePropertyID::BorderBottomColor:        return "border_bottom_color";
    case StylePropertyID::BorderLeftColor:          return "border_left_color";
    case StylePropertyID::BorderTopLeftRadius:      return "border_top_left_radius";
    case StylePropertyID::BorderTopRightRadius:     return "border_top_right_radius";
    case StylePropertyID::BorderBottomRightRadius:  return "border_bottom_right_radius";
    case StylePropertyID::BorderBottomLeftRadius:   return "border_bottom_left_radius";
    case StylePropertyID::BorderTopWidth:           return "border_top_width";
    case StylePropertyID::BorderRightWidth:         return "border_right_width";
    case StylePropertyID::BorderBottomWidth:        return "border_bottom_width";
    case StylePropertyID::BorderLeftWidth:          return "border_left_width";
    case StylePropertyID::BorderTopStyle:           return "border_top_style";
    case StylePropertyID::BorderRightStyle:         return "border_right_style";
    case StylePropertyID::BorderBottomStyle:        return "border_bottom_style";
    case StylePropertyID::BorderLeftStyle:          return "border_left_style";
    case StylePropertyID::BorderImageSource:        return "border_image_source";
    case StylePropertyID::BorderImageTopSlice:      return "border_image_top_slice";
    case StylePropertyID::BorderImageRightSlice:    return "border_image_right_slice";
    case StylePropertyID::BorderImageBottomSlice:   return "border_image_bottom_slice";
    case StylePropertyID::BorderImageLeftSlice:     return "border_image_left_slice";
    case StylePropertyID::BorderImageHorizRepeat:   return "border_image_horiz_repeat";
    case StylePropertyID::BorderImageVertRepeat:    return "border_image_vert_repeat";

    case StylePropertyID::FlexGrow:                 return "flex_grow";
    case StylePropertyID::FlexShrink:               return "flex_shrink";
    case StylePropertyID::FlexBasis:                return "flex_basis";
    case StylePropertyID::FlexDirection:            return "flex_direction";
    case StylePropertyID::FlexWrap:                 return "flex_wrap";

    case StylePropertyID::AlignSelf:                return "align_self";
    case StylePropertyID::AlignItems:               return "align_items";
    case StylePropertyID::AlignContent:             return "align_content";
    case StylePropertyID::JustifyContent:           return "justify_content";

    case StylePropertyID::Position:                 return "position";

    case StylePropertyID::Top:                      return "top";
    case StylePropertyID::Right:                    return "right";
    case StylePropertyID::Bottom:                   return "bottom";
    case StylePropertyID::Left:                     return "left";

    case StylePropertyID::Color:                    return "color";
    case StylePropertyID::BackgroundColor:          return "background_color";
    case StylePropertyID::BackgroundImage:          return "background_image";
    case StylePropertyID::BackgroundRepeat:         return "background_repeat";
    case StylePropertyID::BackgroundOrigin:         return "background_origin";
    case StylePropertyID::BackgroundClip:           return "background_clip";
    case StylePropertyID::BackgroundAttachment:     return "background_attachment";
    case StylePropertyID::BackgroundPosition:       return "background_position";

    case StylePropertyID::Overflow:                 return "overflow";
    case StylePropertyID::Display:                  return "display";
    case StylePropertyID::Image:                    return "image";
    case StylePropertyID::ImagePosition:            return "image_position";

    case StylePropertyID::TextAlign:                return "text_align";
    case StylePropertyID::FontFamily:               return "font_family";
    case StylePropertyID::FontSize:                 return "font_size";
    case StylePropertyID::WhiteSpace:               return "white_space";

    case StylePropertyID::Cursor:                   return "cursor";
    default:      return "";
    }

}

/// Advances pos until there is no more whitespace
inline void consumeWhiteSpace(const StringSection& input, size_t& pos) {
    while (pos < input.length() && LStringUtil::isSpace(input[pos])){
        pos++;
    }
}

inline bool isPropertyNameChar(const char c) {
    return (c == '-') || (c >= 'a' && c <= 'z');
}


bool SelectorPart::operator <(const SelectorPart &other) const
{
    if(identifier.type == other.identifier.type){
        if(identifier.value == other.identifier.value){
            if(prevRel == other.prevRel){
                return pseudos < other.pseudos;
            }else{
                return prevRel < other.prevRel;
            }
        }else{
            if(identifier.type == SelectorIdentifierType::Tag){
                if(identifier.tag == nullptr && other.identifier.tag == nullptr){
                    return identifier.value < other.identifier.value;
                }
                return identifier.tag->isMatchBefore(other.identifier.tag);
            }else{
                if(identifier.type == SelectorIdentifierType::PseudoElement){
                    if(identifier.pseudoElement == other.identifier.pseudoElement){
                        return identifier.value < other.identifier.value;
                    }else{
                        return identifier.pseudoElement < other.identifier.pseudoElement;
                    }
                }
                return identifier.value < other.identifier.value;
            }
        }
    }else{
        return identifier.type < other.identifier.type;
    }
}

bool SelectorPart::operator >(const SelectorPart &other) const
{
    if(identifier.type == other.identifier.type){
        if(identifier.value == other.identifier.value){
            if(prevRel == other.prevRel){
                return pseudos > other.pseudos;
            }else{
                return prevRel > other.prevRel;
            }
        }else{
            if(identifier.type == SelectorIdentifierType::Tag){
                if(identifier.tag == nullptr && other.identifier.tag == nullptr){
                    return identifier.value > other.identifier.value;
                }
                return identifier.tag->isMatchAfter(other.identifier.tag);
            }else{
                if(identifier.type == SelectorIdentifierType::PseudoElement){
                    if(identifier.pseudoElement == other.identifier.pseudoElement){
                        return identifier.value > other.identifier.value;
                    }else{
                        return identifier.pseudoElement > other.identifier.pseudoElement;
                    }
                }
                return identifier.value > other.identifier.value;
            }
        }
    }else{
        return identifier.type > other.identifier.type;
    }
}

bool SelectorPart::operator !=(const SelectorPart &other) const
{
    if(pseudos != other.pseudos){
        return true;
    }

    if(identifier.type != other.identifier.type){
        return true;
    }

    if(prevRel != other.prevRel){
        return true;
    }

    return identifier.value != other.identifier.value;
}



bool Parser::parseNumber(const StringSection& input, size_t& pos, float& output) {
    bool valid = false;
    bool negative = false;
    bool mantissa = false;
    int mantissaPlace = 1;
    float value = 0;

    consumeWhiteSpace(input, pos);

    while (pos < input.length()) {
        char chr = input[pos];
        if (chr == '-') {
            if (pos == 0) {
                negative = true;
            }
            else {
                lWarn() << "Unexpected '-'";
                return false; // unexpected -
            }
        }
        else if (chr == '.') {
            if (mantissa) {
                lWarn() << "Unexpected '.'";
                return false; // unexpected .
            }
            mantissa = true;
        }
        else if (LStringUtil::isDigit(chr)) {
            int num = chr - '0';
            if (!mantissa) {
                // whole part
                value = value * 10 + num;
            }
            else {
                // decimal part
                value = value + (float)num / powf(10, (float)mantissaPlace++);
            }
            valid = true;
        }
        else {
            break; // unexpected character (probably a unit)
        }
        pos++;
    }

    output = negative ? -value : value;

    return valid;
}

bool Parser::parseLength(const StringSection& input, StyleLength& output) {
    size_t pos = 0;
    if (parseNumber(input, pos, output.number)) {
        // number parsed
        if (pos < input.length()) {
            // more to read, must be a unit

            // set as AUTO, if continues to be AUTO after parsing, the unit was invalid
            output.unit = StyleLengthUnit::Auto;

            if (input[pos] == '%'){
                output.unit = StyleLengthUnit::Percent;
            }
            else if (pos + 1 < input.length()) { // enough for two char unit
                if (input[pos] == 'p' && input[pos + 1] == 'x'){
                    output.unit = StyleLengthUnit::px;
                }else if(input[pos] == 'i' && input[pos + 1] == 'n'){
                    output.unit = StyleLengthUnit::px;
                }else if(input[pos] == 'c' && input[pos + 1] == 'm'){
                    output.unit = StyleLengthUnit::px;
                }else if(input[pos] == 'm' && input[pos + 1] == 'm'){
                    output.unit = StyleLengthUnit::px;
                }else if(input[pos] == 'e' && input[pos + 1] == 'm'){
                    output.unit = StyleLengthUnit::px;
                }else if(input[pos] == 'v' && input[pos + 1] == 'w'){
                    output.unit = StyleLengthUnit::px;
                }else if(input[pos] == 'v' && input[pos + 1] == 'h'){
                    output.unit = StyleLengthUnit::px;
                }
            }

            if(output.unit == StyleLengthUnit::Auto){
                // invalid unit
                lWarn() << "Unexpected character '" << std::string(1, input[pos]) << "' parsing length";
                return false; // unexpected character
            }
        }
        else {
            // by default pixels
            output.unit = StyleLengthUnit::px;
            // annoying if using something like: margin: 0;
            // pr.warnings.emplace_back("Defaulting to pixels due absence of length unit");
        }

        return true;
    }
    else if (input.length() >= 4) {
        // check if auto
        if (input[0] == 'a' && input[1] == 'u' && input[2] == 't' && input[3] == 'o') {
            output.unit = StyleLengthUnit::Auto;
            return true;
        }
    }

    return false;
}



bool Parser::parseColor(const StringSection& input, LColor& output) {
    if (input.length() < 2)
        return false;

    if (input[0] == '#') { // hex color
        // #RGB
        if (input.length() == 4) {
            if (LStringUtil::isHex(input[1]) &&
                LStringUtil::isHex(input[2]) &&
                LStringUtil::isHex(input[3])) {
                output = LColor(LStringUtil::hexToDec(input[1], input[1]),
                                LStringUtil::hexToDec(input[2], input[2]),
                                LStringUtil::hexToDec(input[3], input[3]));
                return true;
            }
            else {
                lWarn() << "Invalid #RGB hex value";
                return false; // invalid hex
            }
        }
        // #RRGGBB
        else if (input.length() == 7) {
            if (LStringUtil::isHex(input[1]) &&
                LStringUtil::isHex(input[2]) &&
                LStringUtil::isHex(input[3]) &&
                LStringUtil::isHex(input[4]) &&
                LStringUtil::isHex(input[5]) &&
                LStringUtil::isHex(input[6])) {
                output = LColor(LStringUtil::hexToDec(input[1], input[2]),
                                LStringUtil::hexToDec(input[3], input[4]),
                                LStringUtil::hexToDec(input[5], input[6]));
                return true;
            }
            else {
                lWarn() << "Invalid #RRGGBB hex value";
                return false; // invalid hex
            }
        }
        else {
            lWarn() << "Invalid size for hex number (" + std::to_string(input.length()) + ")";
            return false; // invalid size for hex number
        }
    }
    else if (input.length() > 5 && input[0] == 'r' && input[1] == 'g' && input[2] == 'b') { // rgb/a color
        //std::string k = input.str();
        bool hasAlpha = input[3] == 'a';
        int numComponents = hasAlpha ? 4 : 3;
        size_t pos = hasAlpha ? 5 : 4;
        float components[4];

        for (int i = 0; i < numComponents; i++) {
            if (parseNumber(input, pos, components[i])) {
                if (pos < input.length()) {
                    if (input[pos] == '%') {
                        pos++; // skip %
                    }
                    if (input[pos] == ',') {
                        pos++; // skip ,

                        // next component
                        if (i + 1 >= numComponents) {
                            lWarn() << "Too many components for rgb/a";
                            return false; // too many components
                        }
                    }
                    else if (input[pos] == ')') {
                        // check if matched all components
                        if (i + 1 < numComponents) {
                            lWarn() << "Too few components for rgb/a";
                            return false; // too few components
                        }

                        output = LColor((uint8_t)components[0],
                                        (uint8_t)components[1],
                                        (uint8_t)components[2],
                                        hasAlpha ? (uint8_t)((float)components[3] * 255.0f) : 0xFF);
                        return true;
                    }
                }
                else {
                    lWarn() << "Incomplete rgb/a color";
                    return false;
                }
            }
            else {
                lWarn() << "Could not parse component number " + std::to_string(i) + " of rgb/a color";
                return false;
            }
        }

        return false;
    }
    else {
        const std::vector<LNamedVar<uint32_t, LIDynNamedVar> > & colors = LColor::innerColors();
        const std::string inputName = LStringUtil::toLower(LStringUtil::trim(input.str()));

        for(const LNamedVar<uint32_t, LIDynNamedVar>& color : colors){
            if(color.name == inputName){
                output.fromAbgr(color.value);
                return true;
            }
        }
    }

    lWarn() << "Expected color definition";
    return false;
}

bool Parser::parseString(const StringSection& input, std::string& str)
{
    if (input.length() < 2){ // must have at least two quotes
        str.clear();
        return false;
    }

    bool _single = input[0] == '"' || input[input.length() - 1] == '"';
    bool _double = input[0] == '\'' || input[input.length() - 1] == '\'';

    if (!_single && !_double){
        str.clear();
        return false;
    }

    const size_t start = 1;
    const size_t end = input.length() - 1;

    str = input.section(start, end).str();
    return true;
}

bool Parser::parsePosition(const StringSection &input, Position &pos)
{
    pos = 0;;

    if(input.find("top")){
        pos |= LAlignment::Top;
    }else{
        if(input.find("bottom")){
            pos |= LAlignment::Bottom;
        }
    }

    if(input.find("right")){
        pos |= LAlignment::Right;
    }else{
        if(input.find("left")){
            pos |= LAlignment::Left;
        }
    }

    if(input.find("center")){
        if(pos & LAlignment::LeftRightMask){
            if(pos & LAlignment::TopBottomMask){
            }else{
                pos |= LAlignment::VCenter;
            }
        }else{
            if(pos & LAlignment::TopBottomMask){
                pos |= LAlignment::HCenter;
            }else{
                pos |= LAlignment::Center;
            }
        }
    }
    return true;
}

StylePropertyID  parsePropertyId(const StringSection& sec){
    return StylePropertyID::LastPropertyInvalid;
}

uint32_t  parsePropertyValue(const StylePropertyID& id, const StringSection& sec){
    return 0;
}


bool Parser::parseStylePropertyLine(const StringSection& line, std::shared_ptr<StyleProperties> properties, StyleSheet::ValueMode mode)
{
    // Example of valid property lines:
    //
    // margin: 5px
    // margin-left: 5px
    // color: #ff00ff
    // background-color: rgba(255, 0, 255, 0.5)
    // font-family: "default"
    // justify-content: flex-start

    size_t pos = 0;
    consumeWhiteSpace(line, pos);

    if (pos >= line.length())
        return false;

    // consume a property name
    size_t propertyNameStart = pos;
    while (pos < line.length() && isPropertyNameChar(line[pos])){
        pos++;
    }
    size_t propertyNameEnd = pos;

    if (propertyNameStart == propertyNameEnd) {
        lWarn() << "Missing property name";
        return false;
    }

    consumeWhiteSpace(line, pos);

    // next char must be :
    if (pos >= line.length() || line[pos] != ':') {
        lWarn() << "Missing :";
        return false;
    }

    pos++; // consume :
    consumeWhiteSpace(line, pos);

    // consume whitespace backwards
    size_t tail = line.length() - 1;
    while (tail > pos && LStringUtil::isSpace(line[tail])){
        tail--;
    }

    StringSection propertyName = line.section(propertyNameStart, propertyNameEnd);
    StringSection rawValue = line.section(pos, tail + 1);
    if(mode == StyleSheet::OnlyRaw){
        properties->emplace_back(propertyName.str(), rawValue.str());
        return true;
    }

    StyleValue value;
    StylePropertyID id = StylePropertyID::LastPropertyInvalid; // for enums
    size_t dummy = 0; // for parseNumber

   switch (propertyName.hash()) {
#define LENGTH_PROPERTY(propName, propId) \
   case hashCase(propName):{ \
       StyleLength len; \
       if (parseLength(rawValue, len)) { \
           value.setStyleLength(len); \
           properties->emplace_back(propId, value); \
           return true; \
       } \
       break;\
   }

#define COLOR_PROPERTY(propName, propId) \
    case hashCase(propName):{ \
        LColor color; \
        if (parseColor(rawValue, color)) { \
            value.setColor(color); \
            properties->emplace_back(propId, value); \
            return true; \
        } \
        break;\
    }

#define STRING_PROPERTY(propName, propId) \
    case hashCase(propName):{ \
        std::string s; \
        if (parseString(rawValue, s)) { \
            value.setStr(s); \
            properties->emplace_back(propId, value); \
            return true; \
        } \
        break;\
    }

#define NUMBER_PROPERTY(propName, propId) \
    case hashCase(propName):{ \
        float number;\
        if (parseNumber(rawValue, dummy, number)) { \
            value.setNumber(number); \
            properties->emplace_back(propId, value); \
            return true; \
        }\
        break;\
    }

#define POSITION_PROPERTY(propName, propId) \
    case hashCase(propName):{ \
           Position pos;\
           if (parsePosition(rawValue, pos)) { \
               value.setPosition(pos); \
               properties->emplace_back(propId, value); \
               return true; \
        }\
        break;\
    }

#define PARSE_ENUM_START(propName, propId) \
    case hashCase(propName):{ \
        id = propId; \
        switch (rawValue.hash()) {

#define PARSE_ENUM_ENTRY(Key, a, b) \
        case hashCase(a):{ \
            value.set##Key((Key)b); \
            break;\
        }

#define PARSE_ENUM_END() \
        default:{ \
            return false; /* no match */ \
        }\
        }\
        properties->emplace_back(id, value); \
        return true;\
   }


    LENGTH_PROPERTY("width",      StylePropertyID::Width);
    LENGTH_PROPERTY("height",     StylePropertyID::Height);
    LENGTH_PROPERTY("min-width",  StylePropertyID::MinWidth);
    LENGTH_PROPERTY("min-height", StylePropertyID::MinHeight);
    LENGTH_PROPERTY("max-width",  StylePropertyID::MaxWidth);
    LENGTH_PROPERTY("max-height", StylePropertyID::MaxHeight);

    LENGTH_PROPERTY("margin-top",    StylePropertyID::MarginTop);
    LENGTH_PROPERTY("margin-right",  StylePropertyID::MarginRight);
    LENGTH_PROPERTY("margin-bottom", StylePropertyID::MarginBottom);
    LENGTH_PROPERTY("margin-left",   StylePropertyID::MarginLeft);

    LENGTH_PROPERTY("padding-top",    StylePropertyID::PaddingTop);
    LENGTH_PROPERTY("padding-right",  StylePropertyID::PaddingRight);
    LENGTH_PROPERTY("padding-bottom", StylePropertyID::PaddingBottom);
    LENGTH_PROPERTY("padding-left",   StylePropertyID::PaddingLeft);

    COLOR_PROPERTY("border-top-color", StylePropertyID::BorderTopColor);
    COLOR_PROPERTY("border-right-color", StylePropertyID::BorderRightColor);
    COLOR_PROPERTY("border-bottom-color", StylePropertyID::BorderBottomColor);
    COLOR_PROPERTY("border-left-color", StylePropertyID::BorderLeftColor);

    LENGTH_PROPERTY("border-top-left-radius",     StylePropertyID::BorderTopLeftRadius);
    LENGTH_PROPERTY("border-top-right-radius",    StylePropertyID::BorderTopRightRadius);
    LENGTH_PROPERTY("border-bottom-left-radius",  StylePropertyID::BorderBottomLeftRadius);
    LENGTH_PROPERTY("border-bottom-right-radius", StylePropertyID::BorderBottomRightRadius);

    LENGTH_PROPERTY("border-top-width",    StylePropertyID::BorderTopWidth);
    LENGTH_PROPERTY("border-right-width",  StylePropertyID::BorderRightWidth);
    LENGTH_PROPERTY("border-bottom-width", StylePropertyID::BorderBottomWidth);
    LENGTH_PROPERTY("border-left-width",   StylePropertyID::BorderLeftWidth);

#define BORDER_STYLE_ENUM_ENTRIES() \
            PARSE_ENUM_ENTRY(BorderStyle, "none", BorderStyle::None); \
            PARSE_ENUM_ENTRY(BorderStyle, "dotted", BorderStyle::Dotted); \
            PARSE_ENUM_ENTRY(BorderStyle, "dashed", BorderStyle::Dashed); \
            PARSE_ENUM_ENTRY(BorderStyle, "solid", BorderStyle::Solid); \
            PARSE_ENUM_ENTRY(BorderStyle, "double", BorderStyle::Double); \
            PARSE_ENUM_ENTRY(BorderStyle, "dotdash", BorderStyle::DotDash); \
            PARSE_ENUM_ENTRY(BorderStyle, "dotdotdash", BorderStyle::DotDotDash); \
            PARSE_ENUM_ENTRY(BorderStyle, "groove", BorderStyle::Groove); \
            PARSE_ENUM_ENTRY(BorderStyle, "ridge", BorderStyle::Ridge); \
            PARSE_ENUM_ENTRY(BorderStyle, "inset", BorderStyle::Inset); \
            PARSE_ENUM_ENTRY(BorderStyle, "outset", BorderStyle::Outset); \
            PARSE_ENUM_ENTRY(BorderStyle, "native", BorderStyle::Native);

#define BORDER_IMAGE_REPEAT_ENUM_ENTRIES() \
        PARSE_ENUM_ENTRY(BorderImageRepeat, "strech", BorderImageRepeat::Stretch); \
        PARSE_ENUM_ENTRY(BorderImageRepeat, "repeat", BorderImageRepeat::Repeat); \
        PARSE_ENUM_ENTRY(BorderImageRepeat, "round", BorderImageRepeat::Round); \
        PARSE_ENUM_ENTRY(BorderImageRepeat, "space", BorderImageRepeat::Space);


    PARSE_ENUM_START("border-top-style", StylePropertyID::BorderTopStyle);
    BORDER_STYLE_ENUM_ENTRIES();
    PARSE_ENUM_END();
    PARSE_ENUM_START("border-right-style", StylePropertyID::BorderRightStyle);
    BORDER_STYLE_ENUM_ENTRIES();
    PARSE_ENUM_END();
    PARSE_ENUM_START("border-bottom-style", StylePropertyID::BorderBottomStyle);
    BORDER_STYLE_ENUM_ENTRIES();
    PARSE_ENUM_END();
    PARSE_ENUM_START("border-left-style", StylePropertyID::BorderLeftStyle);
    BORDER_STYLE_ENUM_ENTRIES();
    PARSE_ENUM_END();


    STRING_PROPERTY("border-image-source", StylePropertyID::BorderImageSource);

    NUMBER_PROPERTY("flex-grow",   StylePropertyID::FlexGrow);
    NUMBER_PROPERTY("flex-shrink", StylePropertyID::FlexShrink);
    LENGTH_PROPERTY("flex-basis",  StylePropertyID::FlexBasis);

    PARSE_ENUM_START("flex-direction", StylePropertyID::FlexDirection);
    PARSE_ENUM_ENTRY(FlexDirection, "row",            FlexDirection::Row);
    PARSE_ENUM_ENTRY(FlexDirection, "column",         FlexDirection::Column);
    PARSE_ENUM_ENTRY(FlexDirection, "row-reverse",    FlexDirection::RowReverse);
    PARSE_ENUM_ENTRY(FlexDirection, "column-reverse", FlexDirection::ColumnReverse);
    PARSE_ENUM_END();
    PARSE_ENUM_START("flex-wrap", StylePropertyID::FlexWrap);
    PARSE_ENUM_ENTRY(FlexWrap, "nowrap",       FlexWrap::NoWrap);
    PARSE_ENUM_ENTRY(FlexWrap, "wrap",         FlexWrap::Wrap);
    PARSE_ENUM_ENTRY(FlexWrap, "wrap-reverse", FlexWrap::WrapReverse);
    PARSE_ENUM_END();

#define ALIGN_ENUM_ENTRIES() \
            PARSE_ENUM_ENTRY(Align, "auto",          Align::Auto); \
            PARSE_ENUM_ENTRY(Align, "flex-start",    Align::FlexStart); \
            PARSE_ENUM_ENTRY(Align, "center",        Align::Center); \
            PARSE_ENUM_ENTRY(Align, "flex-end",      Align::FlexEnd); \
            PARSE_ENUM_ENTRY(Align, "stretch",       Align::Stretch); \
            PARSE_ENUM_ENTRY(Align, "baseline",      Align::Baseline); \
            PARSE_ENUM_ENTRY(Align, "space-between", Align::SpaceBetween); \
            PARSE_ENUM_ENTRY(Align, "space-around",  Align::SpaceAround);

    PARSE_ENUM_START("align-self", StylePropertyID::AlignSelf);
    ALIGN_ENUM_ENTRIES();
    PARSE_ENUM_END();
    PARSE_ENUM_START("align-items", StylePropertyID::AlignItems);
    ALIGN_ENUM_ENTRIES();
    PARSE_ENUM_END();
    PARSE_ENUM_START("align-content", StylePropertyID::AlignContent);
    ALIGN_ENUM_ENTRIES();
    PARSE_ENUM_END();
    PARSE_ENUM_START("justify-content", StylePropertyID::JustifyContent);
    PARSE_ENUM_ENTRY(Justify, "flex-start",    Justify::FlexStart);
    PARSE_ENUM_ENTRY(Justify, "center",        Justify::Center);
    PARSE_ENUM_ENTRY(Justify, "flex-end",      Justify::FlexEnd);
    PARSE_ENUM_ENTRY(Justify, "space-between", Justify::SpaceBetween);
    PARSE_ENUM_ENTRY(Justify, "space-around",  Justify::SpaceAround);
    PARSE_ENUM_ENTRY(Justify, "space-evenly",  Justify::SpaceEvenly);
    PARSE_ENUM_END();

    PARSE_ENUM_START("position", StylePropertyID::Position);
    PARSE_ENUM_ENTRY(PositionType, "relative", PositionType::Relative);
    PARSE_ENUM_ENTRY(PositionType, "absolute", PositionType::Absolute);
    PARSE_ENUM_END();
    LENGTH_PROPERTY("top",    StylePropertyID::Top);
    LENGTH_PROPERTY("right",  StylePropertyID::Right);
    LENGTH_PROPERTY("bottom", StylePropertyID::Bottom);
    LENGTH_PROPERTY("left",   StylePropertyID::Left);

    COLOR_PROPERTY("color", StylePropertyID::Color);
    COLOR_PROPERTY("background-color", StylePropertyID::BackgroundColor);
    STRING_PROPERTY("background-image", StylePropertyID::BackgroundImage);
    POSITION_PROPERTY("background-position", StylePropertyID::BackgroundPosition);

    PARSE_ENUM_START("background-repeat", StylePropertyID::BackgroundRepeat);
    PARSE_ENUM_ENTRY(Repeat, "no-repeat", Repeat::None);
    PARSE_ENUM_ENTRY(Repeat, "repeat-x", Repeat::X);
    PARSE_ENUM_ENTRY(Repeat, "repeat-y", Repeat::Y);
    PARSE_ENUM_ENTRY(Repeat, "repeat-xy", Repeat::XY);
    PARSE_ENUM_END();

    PARSE_ENUM_START("background-origin", StylePropertyID::BackgroundOrigin);
    PARSE_ENUM_ENTRY(Origin, "border", Origin::Border);
    PARSE_ENUM_ENTRY(Origin, "content", Origin::Content);
    PARSE_ENUM_ENTRY(Origin, "margin", Origin::Margin);
    PARSE_ENUM_ENTRY(Origin, "padding", Origin::Padding);
    PARSE_ENUM_END();

    PARSE_ENUM_START("background-clip", StylePropertyID::BackgroundClip);
    PARSE_ENUM_ENTRY(Origin, "border", Origin::Border);
    PARSE_ENUM_ENTRY(Origin, "content", Origin::Content);
    PARSE_ENUM_ENTRY(Origin, "margin", Origin::Margin);
    PARSE_ENUM_ENTRY(Origin, "padding", Origin::Padding);
    PARSE_ENUM_END();

    PARSE_ENUM_START("background-attachment", StylePropertyID::BackgroundAttachment);
    PARSE_ENUM_ENTRY(Attachment, "scroll", Attachment::Scroll);
    PARSE_ENUM_ENTRY(Attachment, "fixed", Attachment::Fixed);
    PARSE_ENUM_ENTRY(Attachment, "local", Attachment::Local);
    PARSE_ENUM_END();

    PARSE_ENUM_START("overflow", StylePropertyID::Overflow);
    PARSE_ENUM_ENTRY(Overflow, "visible", Overflow::Visible);
    PARSE_ENUM_ENTRY(Overflow, "hidden",  Overflow::Hidden);
    PARSE_ENUM_ENTRY(Overflow, "scrol",  Overflow::Scroll);
    PARSE_ENUM_END();
    PARSE_ENUM_START("display", StylePropertyID::Display);
    PARSE_ENUM_ENTRY(Display, "flex", Display::Flex);
    PARSE_ENUM_ENTRY(Display, "none", Display::None);
    PARSE_ENUM_END();

    STRING_PROPERTY("image", StylePropertyID::Image);
    POSITION_PROPERTY("image-position", StylePropertyID::ImagePosition);

    PARSE_ENUM_START("text-align", StylePropertyID::TextAlign);
    PARSE_ENUM_ENTRY(TextAlign, "left", TextAlign::Left);
    PARSE_ENUM_ENTRY(TextAlign, "center",  TextAlign::Center);
    PARSE_ENUM_ENTRY(TextAlign, "right",  TextAlign::Right);
    PARSE_ENUM_ENTRY(TextAlign, "justify",  TextAlign::Justify);
    PARSE_ENUM_END();

    STRING_PROPERTY("font-family", StylePropertyID::FontFamily);
    LENGTH_PROPERTY("font-size", StylePropertyID::FontSize);
    PARSE_ENUM_START("white-space", StylePropertyID::WhiteSpace);
    PARSE_ENUM_ENTRY(WhiteSpace, "norma", WhiteSpace::Normal);
    PARSE_ENUM_ENTRY(WhiteSpace, "nowrap", WhiteSpace::NoWrap);
    PARSE_ENUM_END();

    PARSE_ENUM_START("cursor", StylePropertyID::Cursor);
    PARSE_ENUM_ENTRY(Cursor, "auto",        Cursor::Auto);
    PARSE_ENUM_ENTRY(Cursor, "default",     Cursor::Default);
    PARSE_ENUM_ENTRY(Cursor, "none",        Cursor::None);
    PARSE_ENUM_ENTRY(Cursor, "help",        Cursor::Help);
    PARSE_ENUM_ENTRY(Cursor, "pointer",     Cursor::Pointer);
    PARSE_ENUM_ENTRY(Cursor, "progress",    Cursor::Progress);
    PARSE_ENUM_ENTRY(Cursor, "wait",        Cursor::Wait);
    PARSE_ENUM_ENTRY(Cursor, "crosshair",   Cursor::CrossHair);
    PARSE_ENUM_ENTRY(Cursor, "text",        Cursor::Text);
    PARSE_ENUM_ENTRY(Cursor, "move",        Cursor::Move);
    PARSE_ENUM_ENTRY(Cursor, "not-allowed", Cursor::NotAllowed);
    PARSE_ENUM_ENTRY(Cursor, "all-scrol",  Cursor::AllScroll);
    PARSE_ENUM_ENTRY(Cursor, "col-resize",  Cursor::ColResize);
    PARSE_ENUM_ENTRY(Cursor, "row-resize",  Cursor::RowResize);
    PARSE_ENUM_END();

    // shorthands
#define FOUR_LENGTH_SHORTHAND(propName, a, b, c, d) \
    case hashCase(propName):{ \
        StyleLength len; \
        if (parseLength(rawValue, len)) { \
            value.setStyleLength(len); \
            properties->emplace_back(a, value); \
            properties->emplace_back(b, value); \
            properties->emplace_back(c, value); \
            properties->emplace_back(d, value); \
            return true; \
        } \
        break; \
    }

    FOUR_LENGTH_SHORTHAND(
        "border-radius",
        StylePropertyID::BorderTopLeftRadius,
        StylePropertyID::BorderTopRightRadius,
        StylePropertyID::BorderBottomRightRadius,
        StylePropertyID::BorderBottomLeftRadius
        )
    FOUR_LENGTH_SHORTHAND(
        "border-width",
        StylePropertyID::BorderTopWidth,
        StylePropertyID::BorderRightWidth,
        StylePropertyID::BorderBottomWidth,
        StylePropertyID::BorderLeftWidth
        )
    FOUR_LENGTH_SHORTHAND(
        "margin",
        StylePropertyID::MarginTop,
        StylePropertyID::MarginRight,
        StylePropertyID::MarginBottom,
        StylePropertyID::MarginLeft
        )
    FOUR_LENGTH_SHORTHAND(
        "padding",        
        StylePropertyID::PaddingTop,
        StylePropertyID::PaddingRight,
        StylePropertyID::PaddingBottom,
        StylePropertyID::PaddingLeft
        )
    case hashCase("border-color"):{
        LColor color;
        if (parseColor(rawValue, color)) {
            value.setColor(color);
            properties->emplace_back(StylePropertyID::BorderTopColor, value);
            properties->emplace_back(StylePropertyID::BorderRightColor, value);
            properties->emplace_back(StylePropertyID::BorderBottomColor, value);
            properties->emplace_back(StylePropertyID::BorderLeftColor, value);
            return true;
        }
        break;
    }
    case hashCase("border-style"):{
        switch (rawValue.hash()) {
            BORDER_STYLE_ENUM_ENTRIES();
        default:{
            return false;
        }
        }
        properties->emplace_back(StylePropertyID::BorderTopStyle, value);
        properties->emplace_back(StylePropertyID::BorderRightStyle, value);
        properties->emplace_back(StylePropertyID::BorderBottomStyle, value);
        properties->emplace_back(StylePropertyID::BorderLeftStyle, value);
        return true;
    }
    case hashCase("border"):{
        size_t subPos = 0;
        consumeWhiteSpace(rawValue, subPos);
        StringSection subValue = rawValue.sectionUntil(' ',subPos);
        StyleLength len;
        StyleValue widthValue;
        if (parseLength(subValue, len)) {
            widthValue.setStyleLength(len);
            consumeWhiteSpace(rawValue, subPos);
            subValue = rawValue.sectionUntil(' ',subPos);
        }

        switch (subValue.hash()) { //必需字段
            BORDER_STYLE_ENUM_ENTRIES();
        default:{
            return false;
        }
        }
        if(!widthValue.isNull()){
            properties->emplace_back(StylePropertyID::BorderTopWidth, widthValue);
            properties->emplace_back(StylePropertyID::BorderRightWidth, widthValue);
            properties->emplace_back(StylePropertyID::BorderBottomWidth, widthValue);
            properties->emplace_back(StylePropertyID::BorderLeftWidth, widthValue);
        }

        properties->emplace_back(StylePropertyID::BorderTopStyle, value);
        properties->emplace_back(StylePropertyID::BorderRightStyle, value);
        properties->emplace_back(StylePropertyID::BorderBottomStyle, value);
        properties->emplace_back(StylePropertyID::BorderLeftStyle, value);

        consumeWhiteSpace(rawValue, subPos);
        subValue = rawValue.sectionUntil(';',subPos);
        StyleValue colorValue;
        LColor color;
        if (parseColor(subValue, color)) {
            colorValue.setColor(color);
            properties->emplace_back(StylePropertyID::BorderTopColor, colorValue);
            properties->emplace_back(StylePropertyID::BorderRightColor, colorValue);
            properties->emplace_back(StylePropertyID::BorderBottomColor, colorValue);
            properties->emplace_back(StylePropertyID::BorderLeftColor, colorValue);
        }
        return true;
    }
    case hashCase("border-image"):{
        if(rawValue.find("none")>=0){
            return true;
        }
        size_t subPos = 0;
        consumeWhiteSpace(rawValue, subPos);
        StringSection subValue = rawValue.sectionUntil('(',subPos);
        if(subValue.length()>0){
            subPos = subValue.end();
            subValue = rawValue.sectionUntil(')',subPos);
            if(subValue.length()>0){
                subPos = subValue.end();
                std::string s;
                if (parseString(subValue, s)) {
                    value.setStr(s);
                    properties->emplace_back(StylePropertyID::BorderImageSource, value);
                }
            }else{
                return false;
            }
        }else{
            return false;
        }

        consumeWhiteSpace(rawValue, subPos);
        subValue = rawValue.section(' ', subPos);
        StyleLength len;
        if (parseLength(subValue, len)) {
            StyleValue slice;
            slice.setStyleLength(len);
            properties->emplace_back(StylePropertyID::BorderImageTopSlice, slice);
            properties->emplace_back(StylePropertyID::BorderImageRightSlice, slice);
            properties->emplace_back(StylePropertyID::BorderImageBottomSlice, slice);
            properties->emplace_back(StylePropertyID::BorderImageLeftSlice, slice);

            consumeWhiteSpace(rawValue, subPos);
            subValue = rawValue.section(';', subPos);
        }

        switch (subValue.hash()) {
            BORDER_IMAGE_REPEAT_ENUM_ENTRIES();
        default:{
            return true;
        }
        }
        properties->emplace_back(StylePropertyID::BorderImageHorizRepeat, value);
        properties->emplace_back(StylePropertyID::BorderImageVertRepeat, value);
        return true;
    }
    case hashCase("border-image-slice"):{
        size_t subPos = 0;
        consumeWhiteSpace(rawValue, subPos);
        StringSection subValue = rawValue.sectionUntil(' ',subPos);
        StyleLength len;
        StyleValue topValue;
        if (parseLength(subValue, len)) {
            topValue.setStyleLength(len);
            properties->emplace_back(StylePropertyID::BorderImageTopSlice, topValue);
            consumeWhiteSpace(rawValue, subPos);
            subValue = rawValue.sectionUntil(' ',subPos);
        }else{
            return false;
        }

        StyleValue rightValue;
        if (parseLength(subValue, len)) {
            rightValue.setStyleLength(len);
            properties->emplace_back(StylePropertyID::BorderImageRightSlice, rightValue);
            consumeWhiteSpace(rawValue, subPos);
            subValue = rawValue.sectionUntil(' ',subPos);
        }else{
            properties->emplace_back(StylePropertyID::BorderImageRightSlice, topValue);
            properties->emplace_back(StylePropertyID::BorderImageBottomSlice, topValue);
            properties->emplace_back(StylePropertyID::BorderImageLeftSlice, topValue);
            return true;
        }

        StyleValue bottomValue;
        if (parseLength(subValue, len)) {
            bottomValue.setStyleLength(len);
            properties->emplace_back(StylePropertyID::BorderImageBottomSlice, bottomValue);
            consumeWhiteSpace(rawValue, subPos);
            subValue = rawValue.sectionUntil(';',subPos);
        }else{
            properties->emplace_back(StylePropertyID::BorderImageBottomSlice, topValue);
            properties->emplace_back(StylePropertyID::BorderImageLeftSlice, rightValue);
            return true;
        }

        StyleValue leftValue;
        if (parseLength(subValue, len)) {
            leftValue.setStyleLength(len);
            properties->emplace_back(StylePropertyID::BorderImageLeftSlice, leftValue);
        }else{
            properties->emplace_back(StylePropertyID::BorderImageLeftSlice, rightValue);
        }
        return true;
    }
    case hashCase("border-image-repeat"):{
        size_t subPos = 0;
        consumeWhiteSpace(rawValue, subPos);
        StringSection subValue = rawValue.sectionUntil(' ',subPos);
        switch (subValue.hash()) { //必需字段
            BORDER_IMAGE_REPEAT_ENUM_ENTRIES();
        default:{
            return false;
        }
        }
        properties->emplace_back(StylePropertyID::BorderImageHorizRepeat, value);

        consumeWhiteSpace(rawValue, subPos);
        subValue = rawValue.sectionUntil(';',subPos);
        switch (subValue.hash()) { //必需字段
            BORDER_IMAGE_REPEAT_ENUM_ENTRIES();
        default:{
            break;
        }
        }
        properties->emplace_back(StylePropertyID::BorderImageVertRepeat, value);
        return true;
    }
    default:{
        if(mode == StyleSheet::AllowRaw){
            properties->emplace_back(propertyName.str(), rawValue.str());
            return true;
        }else{
            lWarn() << "Property '" + propertyName.str() + "' is not supported";
        }
        break;
    }
    }
#undef LENGTH_PROPERTY
#undef COLOR_PROPERTY
#undef STRING_PROPERTY
#undef NUMBER_PROPERTY
#undef POSITION_PROPERTY
#undef PARSE_ENUM_START
#undef PARSE_ENUM_ENTRY
#undef PARSE_ENUM_END
#undef ALIGN_ENUM_ENTRIES
#undef FOUR_LENGTH_SHORTHAND
#undef BORDER_STYLE_ENUM_ENTRIES
#undef BORDER_IMAGE_REPEAT_ENUM_ENTRIES
    return true;
}

bool Parser::parseStylePropertiesBlock(const StringSection& block, std::shared_ptr<StyleProperties> properties, StyleSheet::ValueMode mode)
{
    bool any = false;

    size_t pos = 0;
    while (pos < block.length()) {
        any |= parseStylePropertyLine(block.sectionUntil(';', pos), properties, mode);
        pos++; // skip ,
    }

    return any;
}

std::string sanitizeSource(const std::string& input) {
    std::string result;
    result.reserve(input.size());

    bool before_whitespace = false;
    bool insideComment = false;
    for (int i = 0; i < input.size(); i++) {
        char chr = input[i];
        // check if next is closing comment
        if (chr == '*' && i + 1 < input.size() && input[i + 1] == '/') {
            i += 2;
            insideComment = false;
        }
        else if (insideComment) {
            // ignore character
        }
        else if (LStringUtil::isSpace(chr)) {
            if (!before_whitespace) {
                result.push_back(' ');
                before_whitespace = true;
            }
        }
        // check if next is opening comment
        else if (chr == '/' && i + 1 < input.size() && input[i + 1] == '*') {
            i += 2;
            insideComment = true;
        }
        else {
            result.push_back(chr);
            before_whitespace = false;
        }
    }

    if (insideComment) {
        lWarn() << "Expected comment ending but found EOF";
    }
    return result;
}

/// Check if the character is a CSS selector marker
inline bool isSelectorMarker(char c) {
    return c == '*' || c == '#' || c == '.' || c == ':';
}

/// Check if the character is a CSS nesting operator
inline bool isNestingOperator(char c) {
    return c == '>' || c == '+' || c == '~';
}

/// Check if the character is a valid start for a CSS identifier
inline bool isIdentStart(char c) {
    return
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c == '-') || (c == '_');
}

/// Check if the character is valid for a CSS identifier
inline bool isIdent(char c) {
    return isIdentStart(c) || (c >= '0' && c <= '9');
}

/// Parses a single CSS identifier from pos
std::string Parser::parseIdentifier(const StringSection& input, size_t& pos) {
    size_t startAbs = pos;
    while (pos < input.length() && isIdent(input[pos])){
        pos++;
    }
    return input.section(startAbs, pos).str();
}

bool  Parser::parseSingleSelector(const StringSection& inputSelector, Selector& selector)
{
    selector.clear();

    size_t pos = 0;
    SelectorRelationship nextRel = SelectorRelationship::None;
    bool hasSelect = false, hasPseduoEle = false;

    while (pos < inputSelector.length()) {
        consumeWhiteSpace(inputSelector, pos);
        if (pos >= inputSelector.length()){
            break; // no more
        }

        char chr = inputSelector[pos];
        bool marker = isSelectorMarker(chr);

        if (isIdentStart(chr) || marker) {
            if (marker){
                pos++; // skip marker
            }

            SelectorIdentifierType type;
            bool isWildcard = false;

            switch (chr) {
            case '#':{
                type = SelectorIdentifierType::Id;
                break;
            }
            default:{
                type = SelectorIdentifierType::Tag;
                break;
            }
            case '*':{
                type = SelectorIdentifierType::Tag;
                isWildcard = true;
                break;
            }
            case '.':{
                type = SelectorIdentifierType::Class;
                break;
            }
            case ':':{
                // check for unsupported pseudo-elements (::)
                if (pos < inputSelector.length() && inputSelector[pos] == ':') {
                    if(hasSelect == false){
                        lWarn() << "Error get pseudo-element without main selector";
                        return false;
                    }

                    if(hasPseduoEle){
                        lWarn() << "Error get multiple pseudo-elements in same selector";
                        return false;
                    }

                    pos++; // skip marker
                    if(pos < inputSelector.length() && inputSelector[pos] == ':'){
                        lWarn() << "Error get selector with :::";
                        return false;
                    }else{
                        type = SelectorIdentifierType::PseudoElement;
                        break;
                    }
                }

                LHashedString pseudo = parseIdentifier(inputSelector, pos);

                PseudoState state = PseudoState::None;
                switch (pseudo.hash()) {
                case hashCase("hover"):{
                    state = PseudoState::Hover;
                    break;
                }
                case hashCase("disabled"):{
                    state = PseudoState::Disabled;
                    break;
                }
                case hashCase("enabled"):{
                    state = PseudoState::Enabled;
                    break;
                }
                case hashCase("checked"):{
                    state = PseudoState::Checked;
                    break;
                }
                case hashCase("active"):{
                    state = PseudoState::Active;
                    break;
                }
                case hashCase("focus"):{
                    state = PseudoState::Focus;
                    break;
                }
                case hashCase("selected"):{
                    state = PseudoState::Selected;
                    break;
                }
                case hashCase("raised"):{
                    state = PseudoState::Raised;
                    break;
                }
                case hashCase("sunken"):{
                    state = PseudoState::Sunken;
                    break;
                }
                case hashCase("horizontal"):{
                    state = PseudoState::Horizontal;
                    break;
                }
                default:{
                    lWarn() << "Unsupported pseudo state '" + pseudo.str() + "'";
                    return false;
                }
                }

                // elem:pseudo
                if (nextRel == SelectorRelationship::None && selector.size() > 0) {
                    selector.back().pseudos |= state;
                }
                // :pseudo
                // elem > :pseudo
                else {
                    // create wildcard tag
                    SelectorPart part = { };
                    part.identifier.type = SelectorIdentifierType::Tag;
                    part.identifier.value = "*";
                    part.prevRel = SelectorRelationship::None;
                    part.pseudos = state;

                    selector.emplace_back(part);
                }

                if (pos < inputSelector.length() && inputSelector[pos] == ' '){ // check for space
                    nextRel = SelectorRelationship::Descendant;
                }
                else{
                    nextRel = SelectorRelationship::None;
                }
                continue; // skip the rest
            }
            }

            SelectorPart part = { };
            part.identifier.type = type;
            part.identifier.value = isWildcard ? "*" : parseIdentifier(inputSelector, pos);
            part.prevRel = nextRel;
            part.pseudos = PseudoState::None;

            selector.emplace_back(part);

            if(type == SelectorIdentifierType::PseudoElement){
                hasPseduoEle = true;
                nextRel = SelectorRelationship::None; //一个选择器中只能使用一个伪元素。伪元素必须紧跟在语句中的简单选择器/基础选择器之后
            }else{
                hasSelect = true;
                if (pos < inputSelector.length() && inputSelector[pos] == ' '){ // check for space
                    nextRel = SelectorRelationship::Descendant;
                }
                else{
                    nextRel = SelectorRelationship::None;
                }
            }
        }
        else if (isNestingOperator(chr)) {
            pos++; // skip operator

            switch (chr) {
            case '>':{
                nextRel = SelectorRelationship::Child;
                break;
            }
            case '+':{
                nextRel = SelectorRelationship::AdjacentSibling;
                break;
            }
            case '~':{
                nextRel = SelectorRelationship::GeneralSibling;
                break;
            }
            }
        }
        else if (chr == '[') {
            lWarn() << "Attribute selectors are not supported";
            return false;
        }
        else {
            lWarn() << "Unexpected character '" <<  std::string(1, chr) <<  "'";
            return false;
        }
    }

    return selector.size();
}

bool Parser::parseSelectors(const StringSection& inputSelectors, std::vector<Selector>& selectors)
{
    // we have to track if we add a new selector
    // we can't use selectors.size() because the caller
    // may have passed a non empty vector
    bool any = false;

    size_t pos = 0;
    while (pos < inputSelectors.length()) {
        StringSection singleSelector = inputSelectors.sectionUntil(',', pos);

        Selector selector;
        if (parseSingleSelector(singleSelector, selector)) {
            selectors.emplace_back(selector);
            any = true;
        }

        pos++; // skip ,
    }

    return any;
}


StringSection::StringSection(const std::string &input):
    m_string(std::make_shared<std::string>(input)),
    m_start(0),
    m_end(input.size())
{

}

StringSection::StringSection(const StringSection &input, const size_t start, const size_t end):
    m_string(input.m_string),
    m_start(input.m_start + start),
    m_end(input.m_start + end)
{

}




StringSection StringSection::section(const size_t start, const size_t end) const
{
    return StringSection(*this, start, end);
}

StringSection StringSection::sectionUntil(const char c, size_t &pos) const
{
    size_t startAbs = m_start + pos;
    size_t endAbs = m_string->find(c, startAbs);
    endAbs = endAbs > m_end ? m_end : endAbs;
    pos = endAbs - m_start;
    return section(startAbs - m_start, pos);
}

uint32_t StringSection::hash() const
{
    return hashCase(std::string_view(m_string->c_str() + m_start, length()));
}

std::string StringSection::str() const
{
    return m_string->substr(m_start, length());
}

char StringSection::operator[](const size_t i) const
{
    if((m_start+i)< m_end){
        return m_string->at(m_start + i);
    }
    return char(0);
}

size_t StringSection::length() const
{
    return m_end - m_start;
}

size_t StringSection::find(const char c, const size_t pos) const
{
    return m_string->find(c, m_start + pos);
}

size_t StringSection::find(const std::string &s, const size_t pos) const
{
    return m_string->find(s, m_start + pos);
}

const StyleLength StyleValue::styleLength() const
{
    if(var.type() == LVariant::VarByteArray){
        StyleLength* d = (StyleLength*)var.getBinData();
        if(d != nullptr){
            if(var.getByteSize() == sizeof(StyleLength)){
                return *d;
            }
        }
    }
    return StyleLength();
}

const float StyleValue::length() const
{
    if(var.type() == LVariant::VarByteArray){
        StyleLength* d = (StyleLength*)var.getBinData();
        if(d != nullptr){
            if(var.getByteSize() == sizeof(StyleLength)){
                return d->number;
            }
        }
    }
    return 0;
}

const StyleLengthUnit StyleValue::lengthUnit() const
{
    if(var.type() == LVariant::VarByteArray){
        StyleLength* d = (StyleLength*)var.getBinData();
        if(d != nullptr){
            if(var.getByteSize() == sizeof(StyleLength)){
                return d->unit;
            }
        }
    }
    return StyleLengthUnit::Auto;
}


StyleProperty::StyleProperty(const StylePropertyID id, const StyleValue &value):
    m_id(id),
    m_value(value)
{

}

StyleProperty::StyleProperty(const std::string& idName, const StyleValue &value):
    m_id(StylePropertyID::RawProperty),
    m_value(value),
    m_rawIDName(idName)
{

}

StyleProperty::StyleProperty() :
    m_id(StylePropertyID::LastPropertyInvalid)
{

}

std::string StyleProperty::getIDName() const
{
    if(m_id == StylePropertyID::RawProperty){
        return m_rawIDName;
    }
    return std::string(propertyIdName(m_id));
}

StyleSheet::StyleSheet(ValueMode m) :m_valueMode(m)
{

}

StyleSheet::~StyleSheet()
{
}

void StyleSheet::addRule(const std::shared_ptr<StyleRule> &rule)
{
    m_rules.emplace_back(rule);
}

void StyleSheet::buildSelector()
{
    for(std::shared_ptr<StyleRule>& rule: m_rules){
        for(SelectorPart& part : rule->selector){
            if(part.identifier.type == SelectorIdentifierType::Tag){
                part.identifier.tag = getRegistTag(part.identifier.value, true);
            }else{
                part.identifier.tag = nullptr;

                if(part.identifier.type == SelectorIdentifierType::PseudoElement){

                    LHashedString pseudo = part.identifier.value;
                    switch(pseudo.hash()){
                    case hashCase("add-page"):{
                        part.identifier.pseudoElement=PseudoElementType::AddPage;
                        break;
                    }
                    case hashCase("branch"):{
                        part.identifier.pseudoElement=PseudoElementType::Branch;
                        break;
                    }
                    case hashCase("chunk"):{
                        part.identifier.pseudoElement=PseudoElementType::Chunk;
                        break;
                    }
                    case hashCase("close-button"):{
                        part.identifier.pseudoElement=PseudoElementType::CloseButton;
                        break;
                    }
                    case hashCase("corner"):{
                        part.identifier.pseudoElement=PseudoElementType::Corner;
                        break;
                    }
                    case hashCase("down-arrow"):{
                        part.identifier.pseudoElement=PseudoElementType::DownArrow;
                        break;
                    }
                    case hashCase("down-button"):{
                        part.identifier.pseudoElement=PseudoElementType::DownButton;
                        break;
                    }
                    case hashCase("drop-down"):{
                        part.identifier.pseudoElement=PseudoElementType::DropDown;
                        break;
                    }
                    case hashCase("float-button"):{
                        part.identifier.pseudoElement=PseudoElementType::FloatButton;
                        break;
                    }
                    case hashCase("groove"):{
                        part.identifier.pseudoElement=PseudoElementType::Groove;
                        break;
                    }
                    case hashCase("indicator"):{
                        part.identifier.pseudoElement=PseudoElementType::Indicator;
                        break;
                    }
                    case hashCase("handle"):{
                        part.identifier.pseudoElement=PseudoElementType::Handle;
                        break;
                    }
                    case hashCase("icon"):{
                        part.identifier.pseudoElement=PseudoElementType::Icon;
                        break;
                    }
                    case hashCase("item"):{
                        part.identifier.pseudoElement=PseudoElementType::Item;
                        break;
                    }
                    case hashCase("left-arrow"):{
                        part.identifier.pseudoElement=PseudoElementType::LeftArrow;
                        break;
                    }
                    case hashCase("left-corner"):{
                        part.identifier.pseudoElement=PseudoElementType::LeftCorner;
                        break;
                    }
                    case hashCase("menu-arrow"):{
                        part.identifier.pseudoElement=PseudoElementType::MenuArrow;
                        break;
                    }
                    case hashCase("menu-button"):{
                        part.identifier.pseudoElement=PseudoElementType::MenuButton;
                        break;
                    }
                    case hashCase("menu-indicator"):{
                        part.identifier.pseudoElement=PseudoElementType::MenuIndicator;
                        break;
                    }
                    case hashCase("right-arrow"):{
                        part.identifier.pseudoElement=PseudoElementType::RightArrow;
                        break;
                    }
                    case hashCase("pane"):{
                        part.identifier.pseudoElement=PseudoElementType::Pane;
                        break;
                    }
                    case hashCase("right-corner"):{
                        part.identifier.pseudoElement=PseudoElementType::RightCorner;
                        break;
                    }
                    case hashCase("scroller"):{
                        part.identifier.pseudoElement=PseudoElementType::Scroller;
                        break;
                    }
                    case hashCase("section"):{
                        part.identifier.pseudoElement=PseudoElementType::Section;
                        break;
                    }
                    case hashCase("separator"):{
                        part.identifier.pseudoElement=PseudoElementType::Separator;
                        break;
                    }
                    case hashCase("sub-line"):{
                        part.identifier.pseudoElement=PseudoElementType::SubLine;
                        break;
                    }
                    case hashCase("sub-page"):{
                        part.identifier.pseudoElement=PseudoElementType::SubPage;
                        break;
                    }
                    case hashCase("tab"):{
                        part.identifier.pseudoElement=PseudoElementType::Tab;
                        break;
                    }
                    case hashCase("tab-bar"):{
                        part.identifier.pseudoElement=PseudoElementType::TabBar;
                        break;
                    }
                    case hashCase("tear"):{
                        part.identifier.pseudoElement=PseudoElementType::Tear;
                        break;
                    }
                    case hashCase("tearoff"):{
                        part.identifier.pseudoElement=PseudoElementType::Tearoff;
                        break;
                    }
                    case hashCase("text"):{
                        part.identifier.pseudoElement=PseudoElementType::Text;
                        break;
                    }
                    case hashCase("title"):{
                        part.identifier.pseudoElement=PseudoElementType::Title;
                        break;
                    }
                    case hashCase("up-arrow"):{
                        part.identifier.pseudoElement=PseudoElementType::UpArrow;
                        break;
                    }
                    case hashCase("up-button"):{
                        part.identifier.pseudoElement=PseudoElementType::UpButton;
                        break;
                    }
                    default:{
                        part.identifier.pseudoElement = PseudoElementType::None;
                        break;
                    }
                    }
                }
            }
        }
    }
}

static const ElementTag* innerRegistTag(const std::string &tagName, bool create){
    if(tagName.empty()){
        return nullptr;
    }

    static std::unordered_map<std::string, ElementTag> tags;
     std::unordered_map<std::string, ElementTag>::const_iterator iter = tags.find(tagName);
    if(iter != tags.cend()){
         return &(iter->second);
    }
    if(create){
        tags[tagName] = ElementTag();
        iter = tags.find(tagName);
        if (iter != tags.cend()) {
            return &iter->second;
        }
    }
    return nullptr;
}

const ElementTag* StyleSheet::getRegistTag(const std::string &tagName,  bool create) const
{
    return innerRegistTag(tagName, create);
}

void StyleSheet::sortRules()
{
    std::sort(m_rules.begin(), m_rules.end(), [](const std::shared_ptr<StyleRule>& a, const std::shared_ptr<StyleRule>& b) {
        return a->selector < b->selector;
    });
}

bool StyleSheet::isSelectorMatch(const Selector &selector, const ElementTag* tag, const uint16_t states,
                                 const std::string &objName, const PseudoElementType element) const
{
    if(selector.empty()){ //无selector，对所有都有效
        return true;
    }

    if(element == PseudoElementType::None){
        int partIndex = (int)selector.size() - 1;
        if (partIndex >= 0)
        {
            const lcss::SelectorPart& part = selector[partIndex];
            if((part.pseudos & states) != part.pseudos){
                return false;
            }

            if(part.identifier.type == lcss::SelectorIdentifierType::Tag){
                if(part.identifier.tag != nullptr){
                    return part.identifier.tag->isMatch(tag);
                }else{
                    return false;
                }
            }else if(part.identifier.type == lcss::SelectorIdentifierType::Id){
                if(part.identifier.value == objName){
                    partIndex --;
                    if(partIndex >=0){
                        const lcss::SelectorPart& part1 = selector[partIndex];
                        if(part1.identifier.type == lcss::SelectorIdentifierType::Tag){
                            if(part.identifier.tag != nullptr){
                                return part.identifier.tag->isMatch(tag);
                            }else{
                                return false;
                            }
                        }else{
                            return false;
                        }
                    }else{
                        return false;
                    }
                }else{
                    return false;
                }
            }
        }
        return false;
    }else{
        int partIndex = (int)selector.size() - 1;
        if (partIndex >= 0)
        {
            const lcss::SelectorPart& part = selector[partIndex];
            if((part.pseudos & states) != part.pseudos){
                return false;
            }


            if(part.identifier.type == lcss::SelectorIdentifierType::PseudoElement){
                if(part.identifier.pseudoElement == element){
                    partIndex --;
                    if(partIndex >=0){
                        const lcss::SelectorPart& part1 = selector[partIndex];
                        if(part1.identifier.type == lcss::SelectorIdentifierType::Tag){
                            if(part1.identifier.tag != nullptr){
                                return part1.identifier.tag->isMatch(tag);
                            }else{
                                return false;
                            }
                        }else if(part1.identifier.type == lcss::SelectorIdentifierType::Id){
                            if(part1.identifier.value == objName){
                                partIndex --;
                                if(partIndex >=0){
                                    const lcss::SelectorPart& part2 = selector[partIndex];
                                    if(part2.identifier.type == lcss::SelectorIdentifierType::Tag){
                                        if(part2.identifier.tag != nullptr){
                                            return part2.identifier.tag->isMatch(tag);
                                        }else{
                                            return false;
                                        }
                                    }else{
                                        return false;
                                    }
                                }else{
                                    return false;
                                }
                            }else{
                                return false;
                            }
                        }
                    }else{
                        return false;
                    }
                }
            }
        }
        return false;
    }
}

bool StyleSheet::computeStyle(std::shared_ptr<StyleComputed> style, const ElementTag* tag, const uint16_t states,
                              const std::string &objName, const PseudoElementType element) const
{
    bool hasMatch = false;
    for(const std::shared_ptr<lcss::StyleRule>& rule : m_rules){
        if(isSelectorMatch(rule->selector, tag, states, objName, element)){
            hasMatch = true;
            style->applyRule(rule);
        }
    }
    return hasMatch;
}

bool StyleSheet::isSelectorMatch(const Selector &selector, const std::string &tagName, const uint16_t states,
                                 const std::string &objName, const PseudoElementType element) const
{
    const ElementTag* tag = getRegistTag(tagName, false);
    if(tag != nullptr){
        return isSelectorMatch(selector, tag, states, objName, element);
    }
    return false;
}

bool StyleSheet::computeStyle(std::shared_ptr<StyleComputed> style, const std::string &tagName, const uint16_t states,
                              const std::string &objName, const PseudoElementType element) const
{
    const ElementTag* tag = getRegistTag(tagName, false);
    if(tag != nullptr){
        return computeStyle(style, tag, states, objName, element);
    }
    return false;
}



std::shared_ptr<StyleSheet> Parser::parseStyleSheet(const std::string& rawCss){
    std::shared_ptr<StyleSheet> stylesheet = std::make_shared<StyleSheet>();
    if(parseStyleSheet(rawCss, stylesheet)){
        return stylesheet;
    }
    return nullptr;
}

std::shared_ptr<StyleSheet> Parser::parseFile(const std::string &filePath)
{
    std::shared_ptr<StyleSheet> stylesheet = std::make_shared<StyleSheet>();
    if(parseFile(filePath, stylesheet)){
        return stylesheet;
    }
    return nullptr;
}

bool Parser::parseStyleSheet(const std::string &rawCss, std::shared_ptr<StyleSheet> styleSheet)
{
    StringSection css = sanitizeSource(rawCss);
    bool any = false;
    bool hasSelector = false;
    size_t pos = 0;
    while (pos < css.length()) {
        size_t startAbs = css.start() + pos;
        size_t endAbs = css.find('{', startAbs);
        if((endAbs == std::string::npos) && (hasSelector == false)){ //无selector
            StringSection block = css.sectionUntil('}', pos);
            pos++; // skip }
            if (block.length() == 0) {
                lWarn() << "Expected properties block";
                break;
            }

            std::shared_ptr<StyleProperties> tProperties = std::make_shared<StyleProperties>();
            if(parseStylePropertiesBlock(block, tProperties, styleSheet->valueMode())) {
                std::shared_ptr<StyleRule> rule = std::make_shared<StyleRule>();
                rule->properties = tProperties;
                styleSheet->addRule(rule);
                any = true;
            }
        }else{
            hasSelector = true;
            endAbs = endAbs > css.end() ? css.end() : endAbs;
            pos = endAbs - css.start();
            StringSection selectors = css.section(startAbs - css.start(), pos);
            pos++; // skip {

            StringSection block = css.sectionUntil('}', pos);
            pos++; // skip }

            if (selectors.length() == 0) {
                lWarn() << "Expected selector";
                break;
            }
            if (block.length() == 0) {
                lWarn() << "Expected properties block";
                break;
            }

            std::vector<Selector> tSelectors;
            std::shared_ptr<StyleProperties> tProperties = std::make_shared<StyleProperties>();
            if (parseSelectors(selectors, tSelectors) &&
                parseStylePropertiesBlock(block, tProperties, styleSheet->valueMode())) {
                for(const Selector& selector : tSelectors){
                    std::shared_ptr<StyleRule> rule = std::make_shared<StyleRule>();
                    rule->properties = tProperties;
                    rule->selector = selector;
                    styleSheet->addRule(rule);
                    any = true;
                }
            }
        }
    }
    styleSheet->buildSelector();
    styleSheet->sortRules();
    return any;
}

bool Parser::parseFile(const std::string &filePath, std::shared_ptr<StyleSheet> styleSheet)
{
    if(filePath.starts_with(":")){
        LResource resource(filePath);
        std::vector<uint8_t> data = resource.uncompressedData();
        if(!data.empty()){
            std::string fileContent((char*)(data.data()), data.size());
            return parseStyleSheet(fileContent, styleSheet);
        }else{
            lWarn() << "Failed to open resource: " << filePath;
            return false;
        }
    }else{
        std::ifstream file(filePath);

        if (!file.is_open()) {
            lWarn() << "Failed to open file: " << filePath;
            return false;
        }

        std::string fileContent((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        return parseStyleSheet(fileContent, styleSheet);
    }
}

Parser::Parser() {
}

void StyleComputed::applyProperty(const StyleProperty &property)
{
    switch (property.getID()) {
#define SAVE_PROP(id, name, fun) \
    case id:{ \
        name.populated = true; \
        name.inherited = false; \
        name.value = property.getValue().fun(); \
        break;\
    }

    SAVE_PROP(StylePropertyID::Width,      width, styleLength);
    SAVE_PROP(StylePropertyID::Height,     height, styleLength);
    SAVE_PROP(StylePropertyID::MinWidth,  minWidth, styleLength);
    SAVE_PROP(StylePropertyID::MinHeight, minHeight, styleLength);
    SAVE_PROP(StylePropertyID::MaxWidth,  maxWidth, styleLength);
    SAVE_PROP(StylePropertyID::MaxHeight, maxHeight, styleLength);

    SAVE_PROP(StylePropertyID::MarginTop,    marginTop, styleLength);
    SAVE_PROP(StylePropertyID::MarginRight,  marginRight, styleLength);
    SAVE_PROP(StylePropertyID::MarginBottom, marginBottom, styleLength);
    SAVE_PROP(StylePropertyID::MarginLeft,   marginLeft, styleLength);

    SAVE_PROP(StylePropertyID::PaddingTop,    paddingTop, styleLength);
    SAVE_PROP(StylePropertyID::PaddingRight,  paddingRight, styleLength);
    SAVE_PROP(StylePropertyID::PaddingBottom, paddingBottom, styleLength);
    SAVE_PROP(StylePropertyID::PaddingLeft,   paddingLeft, styleLength);

    SAVE_PROP(StylePropertyID::BorderTopColor, borderTopColor, color);
    SAVE_PROP(StylePropertyID::BorderRightColor, borderRightColor, color);
    SAVE_PROP(StylePropertyID::BorderBottomColor, borderBottomColor, color);
    SAVE_PROP(StylePropertyID::BorderLeftColor, borderLeftColor, color);

    SAVE_PROP(StylePropertyID::BorderTopLeftRadius,     borderTopLeftRadius, styleLength);
    SAVE_PROP(StylePropertyID::BorderTopRightRadius,    borderTopRightRadius, styleLength);
    SAVE_PROP(StylePropertyID::BorderBottomLeftRadius,  borderBottomLeftRadius, styleLength);
    SAVE_PROP(StylePropertyID::BorderBottomRightRadius, borderBottomRightRadius, styleLength);


    SAVE_PROP(StylePropertyID::BorderTopWidth,    borderTopWidth, styleLength);
    SAVE_PROP(StylePropertyID::BorderRightWidth,  borderRightWidth, styleLength);
    SAVE_PROP(StylePropertyID::BorderBottomWidth, borderBottomWidth, styleLength);
    SAVE_PROP(StylePropertyID::BorderLeftWidth,   borderLeftWidth, styleLength);


    SAVE_PROP(StylePropertyID::BorderTopStyle, borderTopStyle, borderStyle);
    SAVE_PROP(StylePropertyID::BorderRightStyle, borderRightStyle, borderStyle);
    SAVE_PROP(StylePropertyID::BorderBottomStyle, borderBottomStyle, borderStyle);
    SAVE_PROP(StylePropertyID::BorderLeftStyle, borderLeftStyle, borderStyle);

    SAVE_PROP(StylePropertyID::BorderImageSource, borderImage, str);
    SAVE_PROP(StylePropertyID::BorderImageTopSlice, borderImageTopSlice, styleLength);
    SAVE_PROP(StylePropertyID::BorderImageRightSlice, borderImageRightSlice, styleLength);
    SAVE_PROP(StylePropertyID::BorderImageBottomSlice, borderImageBottomSlice, styleLength);
    SAVE_PROP(StylePropertyID::BorderImageLeftSlice, borderImageLeftSlice, styleLength);
    SAVE_PROP(StylePropertyID::BorderImageHorizRepeat, borderImageHorizRepeat, borderImageRepeat);
    SAVE_PROP(StylePropertyID::BorderImageVertRepeat, borderImageVertRepeat, borderImageRepeat);

    SAVE_PROP(StylePropertyID::FlexGrow,      flexGrow, number);
    SAVE_PROP(StylePropertyID::FlexShrink,    flexShrink, number);
    SAVE_PROP(StylePropertyID::FlexBasis,     flexBasis, styleLength);
    SAVE_PROP(StylePropertyID::FlexDirection, flexDirection, flexDirection);
    SAVE_PROP(StylePropertyID::FlexWrap,      flexWrap, flexWrap);

    SAVE_PROP(StylePropertyID::AlignSelf,      alignSelf, align);
    SAVE_PROP(StylePropertyID::AlignItems,     alignItems, align);
    SAVE_PROP(StylePropertyID::AlignContent,   alignContent, align);
    SAVE_PROP(StylePropertyID::JustifyContent, justifyContent, justify);

    SAVE_PROP(StylePropertyID::Position, positionType, positionType);

    SAVE_PROP(StylePropertyID::Top,    top, styleLength);
    SAVE_PROP(StylePropertyID::Right,  right, styleLength);
    SAVE_PROP(StylePropertyID::Bottom, bottom, styleLength);
    SAVE_PROP(StylePropertyID::Left,   left, styleLength);

    SAVE_PROP(StylePropertyID::Color, color, color);
    SAVE_PROP(StylePropertyID::BackgroundColor, backgroundColor, color);
    SAVE_PROP(StylePropertyID::BackgroundImage, backgroundImage, str);
    SAVE_PROP(StylePropertyID::BackgroundRepeat, backgroundRepeat, repeat);
    SAVE_PROP(StylePropertyID::BackgroundOrigin, backgroundOrigin, origin);
    SAVE_PROP(StylePropertyID::BackgroundClip, backgroundClip, origin);
    SAVE_PROP(StylePropertyID::BackgroundAttachment, backgroundAttachment, attachment);
    SAVE_PROP(StylePropertyID::BackgroundPosition, backgroundPosition, position);

    SAVE_PROP(StylePropertyID::Overflow, overflow, overflow);
    SAVE_PROP(StylePropertyID::Display, display, display);
    SAVE_PROP(StylePropertyID::Image, image, str);
    SAVE_PROP(StylePropertyID::ImagePosition, imagePosition, position);

    SAVE_PROP(StylePropertyID::TextAlign, textAlign, textAlign);
    SAVE_PROP(StylePropertyID::FontFamily, fontFamily, str);
    SAVE_PROP(StylePropertyID::FontSize, fontSize, styleLength);
    SAVE_PROP(StylePropertyID::WhiteSpace, whiteSpace, whiteSpace);

    SAVE_PROP(StylePropertyID::Cursor, cursor, cursor);
    case StylePropertyID::RawProperty:{
        rawProperties[property.getIDName()] = property.getValue().str();
    }
    case StylePropertyID::LastPropertyInvalid:{
        break;
    }
    }
#undef SAVE_PROP
}

void StyleComputed::applyRule(const std::shared_ptr<StyleRule> rule)
{
    if(rule->properties != nullptr){
        const StyleProperties& props = *rule->properties;
        for (const StyleProperty& prop : props){
            applyProperty(prop);
        }
    }
}

void StyleComputed::inherit(const std::shared_ptr<StyleComputed> source)
{
    if(source == nullptr){
        return;
    }

    if(source.get() == this){
        return;
    }

#define INHERIT(propName) \
    if (!propName.populated || propName.inherited) { \
            propName.inherited = true; \
            propName.populated = true; \
            propName.value = source->propName.value; \
    }

    // only some properties are inherited

    INHERIT(color);
    INHERIT(fontSize);
    INHERIT(whiteSpace);
    INHERIT(cursor);
#undef INHERIT
}

#define SET_VALUE(propName, _value) \
    propName.populated = true; \
    propName.inherited = true; \
    propName.value = _value;

#define SET_LENGTH(propName, _value, _unit) \
    propName.populated = true; \
    propName.inherited = true; \
    propName.value.number = _value; \
    propName.value.unit = _unit;

#define SET_AUTO(propName) \
    propName.populated = true; \
    propName.inherited = true; \
    propName.value.unit = StyleLengthUnit::Auto;

void StyleComputed::buildDefaultStyleValues()
{
    rawProperties.clear();
    SET_AUTO(width);
    SET_AUTO(height);
    SET_AUTO(minWidth);
    SET_AUTO(minHeight);
    SET_AUTO(maxWidth);
    SET_AUTO(maxHeight);


    SET_VALUE(borderTopColor, LColorID::Transparent);
    SET_VALUE(borderRightColor, LColorID::Transparent);
    SET_VALUE(borderBottomColor, LColorID::Transparent);
    SET_VALUE(borderLeftColor, LColorID::Transparent);

    SET_LENGTH(borderTopLeftRadius, 0, StyleLengthUnit::px);
    SET_LENGTH(borderTopRightRadius, 0, StyleLengthUnit::px);
    SET_LENGTH(borderBottomLeftRadius, 0, StyleLengthUnit::px);
    SET_LENGTH(borderBottomRightRadius, 0, StyleLengthUnit::px);


    SET_LENGTH(borderTopWidth, 0, StyleLengthUnit::px);
    SET_LENGTH(borderRightWidth, 0, StyleLengthUnit::px);
    SET_LENGTH(borderBottomWidth, 0, StyleLengthUnit::px);
    SET_LENGTH(borderLeftWidth, 0, StyleLengthUnit::px);


    SET_VALUE(borderTopStyle, BorderStyle::None);
    SET_VALUE(borderRightStyle, BorderStyle::None);
    SET_VALUE(borderBottomStyle, BorderStyle::None);
    SET_VALUE(borderLeftStyle, BorderStyle::None);

    SET_VALUE(borderImage, "");
    SET_LENGTH(borderImageTopSlice, 0, StyleLengthUnit::px);
    SET_LENGTH(borderImageRightSlice, 0, StyleLengthUnit::px);
    SET_LENGTH(borderImageBottomSlice, 0, StyleLengthUnit::px);
    SET_LENGTH(borderImageLeftSlice, 0, StyleLengthUnit::px);
    SET_VALUE(borderImageHorizRepeat, BorderImageRepeat::Stretch);
    SET_VALUE(borderImageVertRepeat, BorderImageRepeat::Stretch);


    SET_LENGTH(marginTop, 0, StyleLengthUnit::px);
    SET_LENGTH(marginRight, 0, StyleLengthUnit::px);
    SET_LENGTH(marginBottom, 0, StyleLengthUnit::px);
    SET_LENGTH(marginLeft, 0, StyleLengthUnit::px);


    SET_LENGTH(paddingTop, 0, StyleLengthUnit::px);
    SET_LENGTH(paddingRight, 0, StyleLengthUnit::px);
    SET_LENGTH(paddingBottom, 0, StyleLengthUnit::px);
    SET_LENGTH(paddingLeft, 0, StyleLengthUnit::px);

    SET_VALUE(flexGrow, 0);
    SET_VALUE(flexShrink, 1);
    SET_AUTO(flexBasis);

    SET_VALUE(flexDirection, FlexDirection::Row);
    SET_VALUE(flexWrap, FlexWrap::NoWrap);
    SET_VALUE(alignSelf, Align::Auto);
    SET_VALUE(alignItems, Align::Stretch);
    SET_VALUE(alignContent, Align::FlexStart);
    SET_VALUE(justifyContent, Justify::FlexStart);

    SET_VALUE(positionType, (PositionType)0); // Position::RELATIVE

    SET_LENGTH(top, 0, StyleLengthUnit::px);
    SET_LENGTH(right, 0, StyleLengthUnit::px);
    SET_LENGTH(bottom, 0, StyleLengthUnit::px);
    SET_LENGTH(left, 0, StyleLengthUnit::px);

    SET_VALUE(color, LColorID::White); // white
    SET_VALUE(backgroundColor, LColorID::Transparent); // transparent
    SET_VALUE(backgroundImage, "");
    SET_VALUE(backgroundRepeat, Repeat::None);
    SET_VALUE(backgroundOrigin, Origin::Padding);
    SET_VALUE(backgroundClip, Origin::Border);
    SET_VALUE(backgroundAttachment, Attachment::Scroll);
    SET_VALUE(backgroundPosition, LAlignment::Left | LAlignment::Top);

    SET_VALUE(overflow, Overflow::Visible);
    SET_VALUE(display, Display::Flex);
    SET_VALUE(image, "");
    SET_VALUE(imagePosition, LAlignment::Left | LAlignment::Top);

    SET_VALUE(fontFamily, "");
    SET_LENGTH(fontSize, 14, StyleLengthUnit::px);
    SET_VALUE(whiteSpace, WhiteSpace::Normal);

    SET_VALUE(cursor, Cursor::Auto);
}

#undef SET_VALUE
#undef SET_LENGTH
#undef SET_AUTO


const StyleValueComputed<LColor> &StyleComputed::borderColor(LDirection dir) const
{
    switch(dir){
    case LDirection::ToTop:{
        return borderTopColor;
    }
    case LDirection::ToRight:{
        return borderRightColor;
    }
    case LDirection::ToBottom:{
        return borderBottomColor;
    }
    case LDirection::ToLeft:{
        return borderLeftColor;
    }
    default:{
        return borderTopColor;
    }
    }
}

const StyleValueComputed<StyleLength> &StyleComputed::borderRadius(LCorner dir) const
{
    switch(dir){
    case LCorner::TopLeft:{
        return borderTopLeftRadius;
    }
    case LCorner::TopRight:{
        return borderTopRightRadius;
    }
    case LCorner::BottomRight:{
        return borderBottomRightRadius;
    }
    case LCorner::BottomLeft:{
        return borderBottomLeftRadius;
    }
    default:{
        return borderTopLeftRadius;
    }
    }
}

const StyleValueComputed<StyleLength> &StyleComputed::borderWidth(LDirection dir) const
{
    switch(dir){
    case LDirection::ToTop:{
         return borderTopWidth;
    }
    case LDirection::ToRight:{
        return borderRightWidth;
    }
    case LDirection::ToBottom:{
        return borderBottomWidth;
    }
    case LDirection::ToLeft:{
        return borderLeftWidth;
    }
    default:{
        return borderTopWidth;
    }
    }
}

const StyleValueComputed<BorderStyle> &StyleComputed::borderStyle(LDirection dir) const
{
    switch(dir){
    case LDirection::ToTop:{
        return borderTopStyle;
    }
    case LDirection::ToRight:{
        return borderRightStyle;
    }
    case LDirection::ToBottom:{
        return borderBottomStyle;
    }
    case LDirection::ToLeft:{
        return borderLeftStyle;
    }
    default:{
        return borderTopStyle;
    }
    }
}

const StyleValueComputed<StyleLength> &StyleComputed::borderImageSlice(LDirection dir) const
{
    switch(dir){
    case LDirection::ToTop:{
        return borderImageTopSlice;
    }
    case LDirection::ToRight:{
        return borderImageRightSlice;
    }
    case LDirection::ToBottom:{
        return borderImageBottomSlice;
    }
    case LDirection::ToLeft:{
        return borderImageLeftSlice;
    }
    default:{
        return borderImageTopSlice;
    }
    }
}

const StyleValueComputed<StyleLength> &StyleComputed::margin(LDirection dir) const
{
    switch(dir){
    case LDirection::ToTop:{
        return marginTop;
    }
    case LDirection::ToRight:{
        return marginRight;
    }
    case LDirection::ToLeft:{
        return marginLeft;
    }
    case LDirection::ToBottom:{
        return marginBottom;
    }
    default:{
        return marginTop;
    }
    }
}

const StyleValueComputed<StyleLength> &StyleComputed::padding(LDirection dir) const
{
    switch(dir){
    case LDirection::ToTop:{
        return paddingTop;
    }
    case LDirection::ToRight:{
        return paddingRight;
    }
    case LDirection::ToLeft:{
        return paddingLeft;
    }
    case LDirection::ToBottom:{
        return paddingBottom;
    }
    default:{
        return paddingTop;
    }
    }
}

const StyleValueComputed<StyleLength> &StyleComputed::position(LDirection dir) const
{
    switch(dir){
    case LDirection::ToTop:{
        return top;
    }
    case LDirection::ToRight:{
        return right;
    }
    case LDirection::ToBottom:{
        return bottom;
    }
    case LDirection::ToLeft:{
        return left;
    }
    default:{
        return top;
    }
    }
}

ElementTag::ElementTag()
{

}

ElementTag::~ElementTag()
{

}

bool ElementTag::isMatch(const ElementTag* other) const
{
    return this == other;
}

bool ElementTag::isMatchBefore(const ElementTag *other) const
{
    return false;
}

bool ElementTag::isMatchAfter(const ElementTag *other) const
{
    return false;
}

}

LK_NAMESPACE_END
