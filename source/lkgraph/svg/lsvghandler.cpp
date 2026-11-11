#include "lsvgdocument.h"
#include "lsvghandler.h"
#include "lsvgstructure.h"
#include "lkcore/io/lxml.h"
#include "lsvgfont.h"
#include "lsvganimatenode.h"
#include "lsvgfilter.h"
#include "lkgui/text/lcssparser.h"
#include "lkgui/image/limagereader.h"
#include "lsvganimator.h"
#include "lkcore/io/lurl.h"
#include <set>
#include <filesystem>

LK_NAMESPACE_BEGIN

static const char *lk_inherit_text = "inherit";
#define LK_INHERIT std::string(lk_inherit_text)

double convertToPixels(double len, bool , LSvgLengthType type)
{

    switch (type) {
    case LSvgLengthType::percent:
        break;
    case LSvgLengthType::px:
        break;
    case LSvgLengthType::pc:
        break;
    case LSvgLengthType::pt:
        return len * 1.25;
        break;
    case LSvgLengthType::mm:
        return len * 3.543307;
        break;
    case LSvgLengthType::cm:
        return len * 35.43307;
        break;
    case LSvgLengthType::in:
        return len * 90;
        break;
    case LSvgLengthType::other:
        break;
    default:
        break;
    }
    return len;
}

// ======== duplicated from LColor_p

static inline int LSvg_h2i(char hex, bool *ok = nullptr)
{
    if (hex >= '0' && hex <= '9')
        return hex - '0';
    if (hex >= 'a' && hex <= 'f')
        return hex - 'a' + 10;
    if (hex >= 'A' && hex <= 'F')
        return hex - 'A' + 10;
    if (ok)
        *ok = false;
    return -1;
}

static inline int LSvg_hex2int(const char *s, bool *ok = nullptr)
{
    return (LSvg_h2i(s[0], ok) * 16) | LSvg_h2i(s[1], ok);
}

static inline int LSvg_hex2int(char s, bool *ok = nullptr)
{
    int h = LSvg_h2i(s, ok);
    return (h * 16) | h;
}

bool LSvg_get_hex_rgb(const char *name, LColor *rgb)
{
    if(name[0] != '#'){
        return false;
    }
    name++;
    const size_t len = strlen(name);
    int r, g, b;
    bool ok = true;
    if (len == 12) {
        r = LSvg_hex2int(name, &ok);
        g = LSvg_hex2int(name + 4, &ok);
        b = LSvg_hex2int(name + 8, &ok);
    } else if (len == 9) {
        r = LSvg_hex2int(name, &ok);
        g = LSvg_hex2int(name + 3, &ok);
        b = LSvg_hex2int(name + 6, &ok);
    } else if (len == 6) {
        r = LSvg_hex2int(name, &ok);
        g = LSvg_hex2int(name + 2, &ok);
        b = LSvg_hex2int(name + 4, &ok);
    } else if (len == 3) {
        r = LSvg_hex2int(name[0], &ok);
        g = LSvg_hex2int(name[1], &ok);
        b = LSvg_hex2int(name[2], &ok);
    } else {
        r = g = b = -1;
    }
    if ((uint32_t)r > 255 || (uint32_t)g > 255 || (uint32_t)b > 255 || !ok) {
        *rgb = LColor();
        return false;
    }
    *rgb = LColor(r, g ,b);
    return true;
}

bool LSvg_get_hex_rgb(const char *str, int len, LColor *rgb)
{
    if (len > 13){
        return false;
    }
    char tmp[16];
    for(int i = 0; i < len; ++i){
        tmp[i] = str[i];
    }
    tmp[len] = 0;
    return LSvg_get_hex_rgb(tmp, rgb);
}

// ======== end of LColor_p duplicate

static bool parsePathDataFast(std::string_view data, std::shared_ptr<LPainterPath> path, bool limitLength = true);

static inline std::string someId(LXmlDoc::const_iterator &xmlIter)
{
    std::string_view id = xmlIter.attribute("id");
    if(id.empty()){
        id = xmlIter.attribute("xml:id");
    }
    return std::string(id);
}

struct LSvgAttributes
{
    LSvgAttributes(const LXmlNode &xmlNode, LSvgHandler *handler);

    std::string id;

    std::string_view color;
    std::string_view colorOpacity;
    std::string_view fill;
    std::string_view fillRule;
    std::string_view fillOpacity;
    std::string_view stroke;
    std::string_view strokeDashArray;
    std::string_view strokeDashOffset;
    std::string_view strokeLineCap;
    std::string_view strokeLineJoin;
    std::string_view strokeMiterLimit;
    std::string_view strokeOpacity;
    std::string_view strokeWidth;
    std::string_view vectorEffect;
    std::string_view fontFamily;
    std::string_view fontSize;
    std::string_view fontStyle;
    std::string_view fontWeight;
    std::string_view fontVariant;
    std::string_view textAnchor;
    std::string_view transform;
    std::string_view visibility;
    std::string_view opacity;
    std::string_view compOp;
    std::string_view display;
    std::string_view offset;
    std::string_view stopColor;
    std::string_view stopOpacity;
    std::string_view imageRendering;
    std::string_view mask;
    std::string_view markerStart;
    std::string_view markerMid;
    std::string_view markerEnd;
    std::string_view filter;
    std::string_view animationName;
    std::string_view animationDuration;
    std::string_view animationDelay;
    std::string_view animationIterationCount;
    std::string_view animationDirection;
    std::string_view animationTimingFunction;
    std::string_view animationFillMode;
    std::string_view animation;


    std::list<LSvgCssAttribute> m_cssAttributes;
};

LSvgAttributes::LSvgAttributes(const LXmlNode &xmlNode, LSvgHandler *handler)
{
    //xmlAttributes.
    std::map<std::string_view, std::string_view> attrs = xmlNode.attributes();
    std::map<std::string_view, std::string_view>::const_iterator iter = attrs.cbegin();
    for (; iter != attrs.cend(); ++iter) {
        const std::string_view& name = iter->first;
        if (name.empty()){
            continue;
        }
        const std::string_view& value = iter->second;
        switch (name.at(0)) {
        case 'a':{
            if (name == std::string("animation-name")){
                animationName = value;
            }else if (name == std::string("animation-duration")){
                animationDuration = value;
            }else if (name == std::string("animation-delay")){
                animationDelay = value;
            }else if (name == std::string("animation-iteration-count")){
                animationIterationCount = value;
            }else if (name == std::string("animation-direction")){
                animationDirection = value;
            }else if (name == std::string("animation-timing-function")){
                animationTimingFunction = value;
            }else if (name == std::string("animation-fill-mode")){
                animationFillMode = value;
            }else if (name == std::string("animation")){
                animation = value;
            }
            break;
        }
        case 'c':{
            if (name == std::string("color")){
                color = value;
            }else if (name == std::string("color-opacity")){
                colorOpacity = value;
            }else if (name == std::string("comp-op")){
                compOp = value;
            }
            break;
        }
        case 'd':{
            if (name == std::string("display")){
                display = value;
            }
            break;
        }
        case 'f':{
            if (name == std::string("fill")){
                fill = value;
            }else if (name == std::string("fill-rule")){
                fillRule = value;
            }else if (name == std::string("fill-opacity")){
                fillOpacity = value;
            }else if (name == std::string("font-family")){
                fontFamily = value;
            }else if (name == std::string("font-size")){
                fontSize = value;
            }else if (name == std::string("font-style")){
                fontStyle = value;
            }else if (name == std::string("font-weight")){
                fontWeight = value;
            }else if (name == std::string("font-variant")){
                fontVariant = value;
            }else if (name == std::string("filter") &&
                       !handler->options().testFlag(LSvgOption::Tiny12FeaturesOnly)){
                filter = value;
            }
            break;
        }
        case 'i':{
            if (name == std::string("id")){
                id = value;
            }else if (name == std::string("image-rendering")){
                imageRendering = value;
            }
            break;
        }
        case 'm':{
            if(!handler->options().testFlag(LSvgOption::Tiny12FeaturesOnly)){
                if (name == std::string("mask")){
                    mask = value;
                }else if (name == std::string("marker-start")){
                    markerStart = value;
                }else if (name == std::string("marker-mid")){
                    markerMid = value;
                }else if (name == std::string("marker-end")){
                    markerEnd = value;
                }
            }
            break;
        }
        case 'o':{
            if (name == std::string("opacity")){
                opacity = value;
            }else if (name == std::string("offset")){
                offset = value;
            }
            break;
        }
        case 's':{
            if (name.size() > 5 && name.substr(1, 5) == std::string("troke")) {
                std::string_view strokeRef = name.substr(6, name.size() - 6);
                if (strokeRef.empty()){
                    stroke = value;
                }else if (strokeRef == std::string("-dasharray")){
                    strokeDashArray = value;
                }else if (strokeRef == std::string("-dashoffset")){
                    strokeDashOffset = value;
                }else if (strokeRef == std::string("-linecap")){
                    strokeLineCap = value;
                }else if (strokeRef == std::string("-linejoin")){
                    strokeLineJoin = value;
                }else if (strokeRef == std::string("-miterlimit")){
                    strokeMiterLimit = value;
                }else if (strokeRef == std::string("-opacity")){
                    strokeOpacity = value;
                }else if (strokeRef == std::string("-width")){
                    strokeWidth = value;
                }
            } else if (name == std::string("stop-color")){
                stopColor = value;
            }
            else if (name == std::string("stop-opacity")){
                stopOpacity = value;
            }
            break;
        }
        case 't':{
            if (name == std::string("text-anchor")){
                textAnchor = value;
            }else if (name == std::string("transform")){
                transform = value;
            }
            break;
        }
        case 'v':{
            if (name == std::string("vector-effect")){
                vectorEffect = value;
            }else if (name == std::string("visibility")){
                visibility = value;
            }
            break;
        }
        case 'x':{
            if (name == std::string("xml:id") && id.empty()){
                id = std::string(value);
            }
            break;
        }
        default:{
            break;
        }
        }
    }


    std::string_view style = xmlNode.attribute("style");
    if (!style.empty()) {
        handler->parseCSStoXMLAttrs(std::string(style), &m_cssAttributes);
        for (const LSvgCssAttribute &attribute : m_cssAttributes) {
            std::string_view name = attribute.name;
            std::string_view value = attribute.value;
            if (name.empty()){
                continue;
            }

            switch (name.at(0)) {
            case 'c':{
                if (name == std::string("color")){
                    color = value;
                }else if (name == std::string("color-opacity")){
                    colorOpacity = value;
                }else if (name == std::string("comp-op")){
                    compOp = value;
                }
                break;
            }
            case 'd':{
                if (name == std::string("display")){
                    display = value;
                }
                break;
            }
            case 'f':{
                if (name == std::string("fill")){
                    fill = value;
                }else if (name == std::string("fill-rule")){
                    fillRule = value;
                }else if (name == std::string("fill-opacity")){
                    fillOpacity = value;
                }else if (name == std::string("font-family")){
                    fontFamily = value;
                }else if (name == std::string("font-size")){
                    fontSize = value;
                }else if (name == std::string("font-style")){
                    fontStyle = value;
                }else if (name == std::string("font-weight")){
                    fontWeight = value;
                }else if (name == std::string("font-variant")){
                    fontVariant = value;
                }else if (name == std::string("filter") &&
                           !handler->options().testFlag(LSvgOption::Tiny12FeaturesOnly)){
                    filter = value;
                }
                break;
            }
            case 'i':{
                if (name == std::string("image-rendering")){
                    imageRendering = value;
                }
                break;
            }
            case 'm':{
                if (name == std::string("mask") &&
                    !handler->options().testFlag(LSvgOption::Tiny12FeaturesOnly)){
                    mask = value;
                }
                if (name == std::string("marker-start") &&
                    !handler->options().testFlag(LSvgOption::Tiny12FeaturesOnly)){
                    markerStart = value;
                }
                if (name == std::string("marker-mid") &&
                    !handler->options().testFlag(LSvgOption::Tiny12FeaturesOnly)){
                    markerMid = value;
                }
                if (name == std::string("marker-end") &&
                    !handler->options().testFlag(LSvgOption::Tiny12FeaturesOnly)){
                    markerEnd = value;
                }
                break;
            }
            case 'o':{
                if (name == std::string("opacity")){
                    opacity = value;
                }else if (name == std::string("offset")){
                    offset = value;
                }
                break;
            }
            case 's':{
                if (name.size() > 5 && name.substr(1, 5) == std::string("troke")) {
                    std::string_view strokeRef = name.substr(6, name.size() - 6);
                    if (strokeRef.empty()){
                        stroke = value;
                    }
                    else if (strokeRef == std::string("-dasharray")){
                        strokeDashArray = value;
                    }
                    else if (strokeRef == std::string("-dashoffset")){
                        strokeDashOffset = value;
                    }
                    else if (strokeRef == std::string("-linecap")){
                        strokeLineCap = value;
                    }
                    else if (strokeRef == std::string("-linejoin")){
                        strokeLineJoin = value;
                    }
                    else if (strokeRef == std::string("-miterlimit")){
                        strokeMiterLimit = value;
                    }
                    else if (strokeRef == std::string("-opacity")){
                        strokeOpacity = value;
                    }
                    else if (strokeRef == std::string("-width")){
                        strokeWidth = value;
                    }
                } else if (name == std::string("stop-color")){
                    stopColor = value;
                }else if (name == std::string("stop-opacity")){
                    stopOpacity = value;
                }
                break;
            }
            case 't':{
                if (name == std::string("text-anchor")){
                    textAnchor = value;
                }
                else if (name == std::string("transform")){
                    transform = value;
                }
                break;
            }
            case 'v':{
                if (name == std::string("vector-effect")){
                    vectorEffect = value;
                }
                else if (name == std::string("visibility")){
                    visibility = value;
                }
                break;
            }
            default:{
                break;
            }
            }
        }
    }
}


class LSvgStyleElementTag : public lcss::ElementTag{
    std::shared_ptr<LSvgNode> node;
    std::string xmlClass;
public:
    LSvgStyleElementTag(std::shared_ptr<LSvgNode> _node = nullptr, const std::string& _xmlClass = "") : node(_node), xmlClass(_xmlClass){}

    virtual bool isMatch(const lcss::ElementTag* other) const override{
        if(this == other){
            return true;
        }

        if(node != nullptr){
            const LSvgStyleElementTag* wTag = dynamic_cast<const LSvgStyleElementTag*>(other);
            if(wTag != nullptr){
                if(wTag->node == nullptr){
                    return false;
                }

                if(wTag->node != node){
                    if(!wTag->node->isDescendantOf(node)){
                        return false;
                    }
                }
            }else{
                return false;
            }
        }


        const LSvgStyleElementTag* wTag = dynamic_cast<const LSvgStyleElementTag*>(other);
        if(wTag != nullptr){
            if(wTag->node != nullptr){
                return wTag->node->xmlClass() == xmlClass;
            }
        }
        return false;
    }

    virtual bool isMatchBefore(const ElementTag* other )const override{
        if(this == other){
            return false;
        }

        if(!xmlClass.empty()){
            const LSvgStyleElementTag* wTag = dynamic_cast<const LSvgStyleElementTag*>(other);
            if(wTag != nullptr){
                return xmlClass < wTag->xmlClass;
            }
        }
        return false;
    }

    virtual bool isMatchAfter(const ElementTag* other )const override{
        if(this == other){
            return false;
        }

        if(!xmlClass.empty()){
            const LSvgStyleElementTag* wTag = dynamic_cast<const LSvgStyleElementTag*>(other);
            if(wTag != nullptr){
                return xmlClass > wTag->xmlClass;
            }
            return false;
        }else{
            if(other != nullptr){
                return true;
            }
            return false;
        }
    }
};

class LSvgStyleSheet : public lcss::StyleSheet
{
    std::shared_ptr<LSvgNode> node;
    std::unordered_map<std::string, LSvgStyleElementTag> tags;
public:
    LSvgStyleSheet(std::shared_ptr<LSvgNode> _node) : lcss::StyleSheet(lcss::StyleSheet::OnlyRaw), node(_node){
    }

    virtual const lcss::ElementTag* getRegistTag(const std::string& tagName, bool create) const override{
        if(tagName.empty()){
            return nullptr;
        }

        std::unordered_map<std::string, LSvgStyleElementTag>::const_iterator iter = tags.find(tagName);
        if(iter != tags.cend()){
            return &(iter->second);
        }
        if(create){
            const_cast<LSvgStyleSheet*>(this)->tags[tagName] = LSvgStyleElementTag(node, tagName);
            iter = tags.find(tagName);
            if (iter != tags.cend()) {
                return &(iter->second);
            }
        }
        return nullptr;
    }

    bool isSelectorMatch(const lcss::Selector& selector, const std::shared_ptr<LSvgNode> snode, const uint16_t states) const{
        if(snode == nullptr){
            if(node == nullptr){
                return true;
            }
            return false;
        }
        if(node == nullptr || node == snode  || snode->isDescendantOf(node)){
            std::string id = snode->nodeId();
            const lcss::ElementTag* tag = getRegistTag(snode->xmlClass(), false);
            if(tag != nullptr){
                return lcss::StyleSheet::isSelectorMatch(selector, tag, states, id);
            }
        }
        return false;
    }

    bool computeStyle(std::shared_ptr<lcss::StyleComputed> style, const std::shared_ptr<LSvgNode> snode) const
    {
        bool hasMatch = false;
        for(const std::shared_ptr<lcss::StyleRule>& rule : m_rules){
            if(isSelectorMatch(rule->selector, snode, (uint16_t)lcss::PseudoState::None)){
                hasMatch = true;
                style->applyRule(rule);
            }
        }
        return hasMatch;
    }
};

class LSvgStyleSelector
{
public:
   std::list<std::shared_ptr<LSvgStyleSheet> > m_styleSheets;

    LSvgStyleSelector()
    {
    }
    virtual ~LSvgStyleSelector()
    {
    }

    std::shared_ptr<lcss::StyleComputed> getStyleComputed(const std::shared_ptr<LSvgNode> snode){
        std::shared_ptr<lcss::StyleComputed> computed = std::make_shared<lcss::StyleComputed>();
        bool hasMatch = false;
        for(const std::shared_ptr<LSvgStyleSheet>& sheet : m_styleSheets){
            if(sheet->computeStyle(computed, snode)){
                hasMatch = true;
            }
        }
        if(hasMatch){
            return computed;
        }else{
            return nullptr;
        }
    }
};

double toDouble(const char *&str)
{
    const int maxLen = 255;//technically doubles can go til 308+ but whatever
    char temp[maxLen+1];
    int pos = 0;

    if (*str == ('-')) {
        temp[pos++] = '-';
        ++str;
    } else if (*str == ('+')) {
        ++str;
    }
    while (LStringUtil::isDigit(*str) && pos < maxLen) {
        temp[pos++] = *str;
        ++str;
    }
    if (*str == ('.') && pos < maxLen) {
        temp[pos++] = '.';
        ++str;
    }
    while (LStringUtil::isDigit(*str) && pos < maxLen) {
        temp[pos++] = *str;
        ++str;
    }
    bool exponent = false;
    if ((*str == ('e') || *str == ('E')) && pos < maxLen) {
        exponent = true;
        temp[pos++] = 'e';
        ++str;
        if ((*str == ('-') || *str == ('+')) && pos < maxLen) {
            temp[pos++] = *str;
            ++str;
        }
        while (LStringUtil::isDigit(*str) && pos < maxLen) {
            temp[pos++] = *str;
            ++str;
        }
    }

    temp[pos] = '\0';

    double val;
    if (!exponent && pos < 10) {
        int ival = 0;
        const char *t = temp;
        bool neg = false;
        if (*t == '-') {
            neg = true;
            ++t;
        }
        while (*t && *t != '.') {
            ival *= 10;
            ival += (*t) - '0';
            ++t;
        }
        if (*t == '.') {
            ++t;
            int div = 1;
            while (*t) {
                ival *= 10;
                ival += (*t) - '0';
                div *= 10;
                ++t;
            }
            val = ((double)ival)/((double)div);
        } else {
            val = ival;
        }
        if (neg)
            val = -val;
    } else {
        bool isOk = false;
        std::string t(temp, pos);
        val = LStringUtil::stod(t, &isOk);
        if(isOk == false){
            val = 0;
        }
    }
    return val;

}

double toDouble(std::string_view str, bool *ok = nullptr)
{
    const char *c = str.data();
    double res = (c == nullptr ? 0 : toDouble(c));
    if (ok){
        *ok = (c == (str.data() + str.size()));
    }
    return res;
}

static std::vector<double> parseNumbersVec(const char*& str)
{
    std::vector<double> points;
    if (!str){
        return points;
    }

    while (LStringUtil::isSpace(*str)){
        ++str;
    }
    while (LStringUtil::isDigit(*str) ||
           *str == ('-') || *str == ('+') ||
           *str == ('.')) {

        points.push_back(toDouble(str));

        while (LStringUtil::isSpace(*str)){
            ++str;
        }
        if (*str == (',')){
            ++str;
        }

        //eat the rest of space
        while (LStringUtil::isSpace(*str)){
            ++str;
        }
    }
    return points;
}

static inline void parseNumbersArray(const char *&str,
                                     std::vector<double> &points,
                                     const char *pattern = nullptr)
{
    const size_t patternLen = (pattern == nullptr) ? 0 : std::strlen(pattern);
    while (LStringUtil::isSpace(*str)){
        ++str;
    }
    while (LStringUtil::isDigit(*str) ||
           *str == ('-') || *str == ('+') ||
           *str == ('.')) {

        if (patternLen && pattern[points.size() % patternLen] == 'f') {
            // flag expected, may only be 0 or 1
            if (*str != ('0') && *str != ('1')){
                return;
            }
            points.push_back(*str == ('0') ? 0.0 : 1.0);
            ++str;
        } else {
            points.push_back(toDouble(str));
        }

        while (LStringUtil::isSpace(*str)){
            ++str;
        }
        if (*str == (',')){
            ++str;
        }

        //eat the rest of space
        while (LStringUtil::isSpace(*str)){
            ++str;
        }
    }
}

static std::vector<double> parsePercentageVec(const char *&str)
{
    std::vector<double> points;
    if (!str){
        return points;
    }

    while (LStringUtil::isSpace(*str)){
        ++str;
    }
    while ((*str >= ('0') && *str <= ('9')) ||
           *str == ('-') || *str == ('+') ||
           *str == ('.')) {

        points.push_back(toDouble(str));

        while (LStringUtil::isSpace(*str)){
            ++str;
        }
        if (*str == ('%')){
            ++str;
        }
        while (LStringUtil::isSpace(*str)){
            ++str;
        }
        if (*str == (',')){
            ++str;
        }
        //eat the rest of space
        while (LStringUtil::isSpace(*str)){
            ++str;
        }
    }

    return points;
}

static std::string idFromUrl(const std::string &url)
{
    // The form is url(<IRI>), where IRI can be
    // just an ID on #<id> form.
    std::string::const_iterator itr = url.cbegin();
    std::string::const_iterator end = url.cend();
    std::string id;
    while (itr != end && LStringUtil::isSpace(*itr)){
        ++itr;
    }
    if (itr != end && (*itr) == ('(')){
        ++itr;
    }else{
        return std::string();
    }
    while (itr != end && LStringUtil::isSpace(*itr)){
        ++itr;
    }
    if (itr != end && (*itr) == ('#')) {
        id += *itr;
        ++itr;
    } else {
        return std::string();
    }
    while (itr != end && (*itr) != (')')) {
        id += *itr;
        ++itr;
    }
    if (itr == end || (*itr) != (')'))
        return std::string();
    return id;
}

/**
 * returns true when successfully set the color. false signifies
 * that the color should be inherited
 */
static bool resolveColor(std::string_view colorStr, LColor &color, LSvgHandler *handler)
{
    std::string_view colorStrTr = LStringUtil::trim(colorStr);
    if (colorStrTr.empty()){
        return false;
    }

    switch(colorStrTr.at(0)) {

    case '#':
    {
        // #rrggbb is very very common, so let's tackle it here
        // rather than falling back to LColor
        LColor rgb;
        bool ok = LSvg_get_hex_rgb(colorStrTr.data(), colorStrTr.size(), &rgb);
        if (ok){
            color =rgb;
        }
        return ok;
    }
    break;

    case 'r':
    {
        // starts with "rgb(", ends with ")" and consists of at least 7 characters "rgb(,,)"
        if (colorStrTr.size() >= 7 && colorStrTr.at(colorStrTr.size() - 1) == (')')
            && colorStrTr.substr(0, 4) == std::string("rgb(")) {
            const char *s = colorStrTr.data() + 4;
            std::vector<double> compo = parseNumbersVec(s);
            //1 means that it failed after reaching non-parsable
            //character which is going to be "%"
            if (compo.size() == 1) {
                s = colorStrTr.data() + 4;
                compo = parsePercentageVec(s);
                std::vector<double>::iterator iter = compo.begin();
                for(; iter != compo.end(); ++iter){
                    *iter *= (double)2.55;
                }
            }

            if (compo.size() == 3) {
                color = LColor(int(compo[0]), int(compo[1]), int(compo[2]));
                return true;
            }
            return false;
        }
    }
    break;

    case 'c':
        if (colorStrTr == std::string("currentColor")) {
            color = handler->currentColor();
            return true;
        }
        break;
    case 'i':
        if (colorStrTr == LK_INHERIT){
            return false;
        }
        break;
    default:
        break;
    }

    bool isValid;
    color = LColor::fromColorName(std::string(colorStrTr), &isValid);
    return isValid;
}

static bool constructColor(std::string_view colorStr, std::string_view opacity, LColor &color, LSvgHandler *handler)
{
    if (!resolveColor(colorStr, color, handler)){
        return false;
    }
    if (!opacity.empty()) {
        bool ok = true;
        double op = std::min<double>(double(1.0), std::max<double>(double(0.0), toDouble(opacity, &ok)));
        if (!ok){
            op = 1.0;
        }
        color.m_alpha = op*255;
    }
    return true;
}

double parseLength(std::string_view str, LSvgLengthType *type, bool *ok = nullptr)
{
    std::string_view numStr = LStringUtil::trim(str);

    if (numStr.empty()) {
        if (ok){
            *ok = false;
        }
        *type = LSvgLengthType::other;
        return false;
    }
    if (numStr.ends_with(('%'))) {
        numStr = numStr.substr(0, numStr.size()-1);
        *type = LSvgLengthType::percent;
    } else if (numStr.ends_with(("px"))) {
        numStr = numStr.substr(0, numStr.size()-2);
        *type = LSvgLengthType::px;
    } else if (numStr.ends_with(("pc"))) {
        numStr = numStr.substr(0, numStr.size()-2);
        *type = LSvgLengthType::pc;
    } else if (numStr.ends_with(("pt"))) {
        numStr = numStr.substr(0, numStr.size()-2);
        *type = LSvgLengthType::pt;
    } else if (numStr.ends_with(("mm"))) {
        numStr = numStr.substr(0, numStr.size()-2);
        *type = LSvgLengthType::mm;
    } else if (numStr.ends_with(("cm"))) {
        numStr = numStr.substr(0, numStr.size()-2);
        *type = LSvgLengthType::cm;
    } else if (numStr.ends_with(("in"))) {
        numStr = numStr.substr(0, numStr.size()-2);
        *type = LSvgLengthType::in;
    } else {
        // default coordinate system
        *type = LSvgLengthType::px;
    }
    double len = toDouble(numStr, ok);
    return len;
}

static inline double convertToNumber(std::string_view str, bool *ok = NULL)
{
    LSvgLengthType type;
    double num = parseLength(str, &type, ok);
    if (type == LSvgLengthType::percent) {
        num = num/100.0;
    }
    return num;
}

static bool createSvgGlyph(std::shared_ptr<LSvgFont> font, LXmlDoc::const_iterator &attributes)
{
    std::string_view uncStr = attributes.attribute("unicode");
    std::string_view havStr = attributes.attribute("horiz-adv-x");
    std::string_view pathStr = attributes.attribute("d");

    char unicode = (uncStr.empty()) ? '\0' : uncStr.at(0);
    double havx = (havStr.empty()) ? -1 : toDouble(havStr);
    std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
    path->setFillRule(LFillRule::Winding);
    parsePathDataFast(pathStr, path);

    font->addGlyph(unicode, path, havx);
    return true;
}

static void parseColor(std::shared_ptr<LSvgNode> ,
                       const LSvgAttributes &attributes,
                       LSvgHandler *handler)
{
    LColor color;
    if (constructColor(attributes.color, attributes.colorOpacity, color, handler)) {
        handler->popColor();
        handler->pushColor(color);
    }
}

static std::shared_ptr<LSvgStyleProperty> styleFromUrl(std::shared_ptr<LSvgNode> node, const std::string &url)
{
    return node!=nullptr ? node->styleProperty(idFromUrl(url)) : nullptr;
}

static void parseBrush(std::shared_ptr<LSvgNode> node,
                       const LSvgAttributes &attributes,
                       LSvgHandler *handler)
{
    if (!attributes.fill.empty() || !attributes.fillRule.empty() || !attributes.fillOpacity.empty()) {
        std::shared_ptr<LSvgFillStyle> prop = std::make_shared<LSvgFillStyle>();

        //fill-rule attribute handling
        if (!attributes.fillRule.empty() && attributes.fillRule != LK_INHERIT) {
            if (attributes.fillRule == std::string("evenodd")){
                prop->setFillRule(LFillRule::OddEven);
            }
            else if (attributes.fillRule == std::string("nonzero")){
                prop->setFillRule(LFillRule::Winding);
            }
        }

        //fill-opacity attribute handling
        if (!attributes.fillOpacity.empty() && attributes.fillOpacity != LK_INHERIT) {
            prop->setFillOpacity(std::min<double>(double(1.0), std::max<double>(double(0.0), toDouble(attributes.fillOpacity))));
        }

        //fill attribute handling
        if ((!attributes.fill.empty()) && (attributes.fill != LK_INHERIT) ) {
            if (attributes.fill.size() > 3 && attributes.fill.substr(0, 3) == std::string("url")) {
                std::string value = std::string(attributes.fill.substr(3, attributes.fill.size() - 3));
                std::shared_ptr<LSvgStyleProperty> style = styleFromUrl(node, value);
                if (style!= nullptr) {
                    if (style->type() == LSvgStyleProperty::SOLID_COLOR || style->type() == LSvgStyleProperty::GRADIENT
                        || style->type() == LSvgStyleProperty::PATTERN){
                        std::shared_ptr<LSvgPaintStyleProperty> t= std::dynamic_pointer_cast<LSvgPaintStyleProperty>(style);
                        prop->setFillStyle(t);
                    }
                } else {
                    std::string id = idFromUrl(value);
                    prop->setPaintStyleId(id);
                    prop->setPaintStyleResolved(false);
                }
            } else if (attributes.fill != std::string("none")) {
                LColor color;
                if (resolveColor(attributes.fill, color, handler)){
                    prop->setBrush(color);
                }
            } else {
                prop->setBrush(nullptr);
            }
        }
        node->appendStyleProperty(prop, attributes.id);
    }
}



static LTransform2D parseTransformationMatrix(std::string_view value)
{
    if (value.empty()){
        return LTransform2D();
    }

    LTransform2D matrix;
    const char *str = value.data();
    const char *end = str + value.size();

    while (str < end) {
        if (LStringUtil::isSpace(*str) || *str == (',')) {
            ++str;
            continue;
        }
        enum State {
            Matrix,
            Translate,
            Rotate,
            Scale,
            SkewX,
            SkewY
        };
        State state = Matrix;
        if (*str == ('m')) {  //matrix
            const char *ident = "atrix";
            for (int i = 0; i < 5; ++i){
                if (*(++str) != (ident[i])){
                    goto error;
                }
            }
            ++str;
            state = Matrix;
        } else if (*str == ('t')) { //translate
            const char *ident = "ranslate";
            for (int i = 0; i < 8; ++i){
                if (*(++str) != (ident[i])){
                    goto error;
                }
            }
            ++str;
            state = Translate;
        } else if (*str == ('r')) { //rotate
            const char *ident = "otate";
            for (int i = 0; i < 5; ++i){
                if (*(++str) != (ident[i])){
                    goto error;
                }
            }
            ++str;
            state = Rotate;
        } else if (*str == ('s')) { //scale, skewX, skewY
            ++str;
            if (*str == ('c')) {
                const char *ident = "ale";
                for (int i = 0; i < 3; ++i){
                    if (*(++str) != (ident[i])){
                        goto error;
                    }
                }
                ++str;
                state = Scale;
            } else if (*str == ('k')) {
                if (*(++str) != ('e')){
                    goto error;
                }
                if (*(++str) != ('w')){
                    goto error;
                }
                ++str;
                if (*str == ('X')){
                    state = SkewX;
                }else if (*str == ('Y')){
                    state = SkewY;
                }else{
                    goto error;
                }
                ++str;
            } else {
                goto error;
            }
        } else {
            goto error;
        }


        while (str < end && LStringUtil::isSpace(*str)){
            ++str;
        }
        if (*str != ('(')){
            goto error;
        }
        ++str;
        std::vector<double> points;
        parseNumbersArray(str, points);
        if (*str != (')')){
            goto error;
        }
        ++str;

        if(state == Matrix) {
            if(points.size() != 6){
                goto error;
            }
            matrix = LTransform2D(points[0], points[1],
                                points[2], points[3],
                                points[4], points[5]) * matrix;
        } else if (state == Translate) {
            if (points.size() == 1){
                matrix.translate(points[0], 0);
            }else if (points.size() == 2){
                matrix.translate(points[0], points[1]);
            }else{
                goto error;
            }
        } else if (state == Rotate) {
            if(points.size() == 1) {
                matrix.rotate(points[0]);
            } else if (points.size() == 3) {
                matrix.translate(points[1], points[2]);
                matrix.rotate(points[0]);
                matrix.translate(-points[1], -points[2]);
            } else {
                goto error;
            }
        } else if (state == Scale) {
            if (points.size() < 1 || points.size() > 2){
                goto error;
            }
            double sx = points[0];
            double sy = sx;
            if(points.size() == 2)
                sy = points[1];
            matrix.scale(sx, sy);
        } else if (state == SkewX) {
            if (points.size() != 1){
                goto error;
            }
            matrix.shear(std::tan(lDegreesToRadians(points[0])), 0);
        } else if (state == SkewY) {
            if (points.size() != 1)
                goto error;
            matrix.shear(0, std::tan(lDegreesToRadians(points[0])));
        }
    }
error:
    return matrix;
}

static void parsePen(std::shared_ptr<LSvgNode> node,
                     const LSvgAttributes &attributes,
                     LSvgHandler *handler)
{

    if (!attributes.stroke.empty() || !attributes.strokeDashArray.empty() || !attributes.strokeDashOffset.empty() || !attributes.strokeLineCap.empty()
        || !attributes.strokeLineJoin.empty() || !attributes.strokeMiterLimit.empty() || !attributes.strokeOpacity.empty() || !attributes.strokeWidth.empty()
        || !attributes.vectorEffect.empty()) {

        std::shared_ptr<LSvgStrokeStyle> prop = std::make_shared<LSvgStrokeStyle>();

        //stroke attribute handling
        if ((!attributes.stroke.empty()) && (attributes.stroke != LK_INHERIT) ) {
            if (attributes.stroke.size() > 3 && attributes.stroke.substr(0, 3) == std::string("url")) {
                std::string value = std::string(attributes.stroke.substr(3, attributes.stroke.size() - 3));
                std::shared_ptr<LSvgStyleProperty> style = styleFromUrl(node, value);
                if (style!= nullptr) {
                    if (style->type() == LSvgStyleProperty::SOLID_COLOR || style->type() == LSvgStyleProperty::GRADIENT
                        || style->type() == LSvgStyleProperty::PATTERN){
                        std::shared_ptr<LSvgPaintStyleProperty> t = std::dynamic_pointer_cast<LSvgPaintStyleProperty>(style);
                        prop->setStyle(t);
                    }
                } else {
                    std::string id = idFromUrl(value);
                    prop->setPaintStyleId(id);
                    prop->setPaintStyleResolved(false);
                }
            } else if (attributes.stroke != std::string("none")) {
                LColor color;
                if (resolveColor(attributes.stroke, color, handler)){
                    std::shared_ptr<LBrush> b = std::make_shared<LBrush>(color);
                    prop->setStroke(b);
                }
            } else {
                prop->setStroke(nullptr);
            }
        }

        //stroke-width handling
        if (!attributes.strokeWidth.empty() && attributes.strokeWidth != LK_INHERIT) {
            LSvgLengthType lt;
            prop->setWidth(parseLength(attributes.strokeWidth, &lt));
        }

        //stroke-dasharray
        if (!attributes.strokeDashArray.empty() && attributes.strokeDashArray != LK_INHERIT) {
            if (attributes.strokeDashArray == std::string("none")) {
                prop->setDashArrayNone();
            } else {
                std::string dashArray  = std::string(attributes.strokeDashArray);
                const char *s = dashArray.data();
                std::vector<double> dashes = parseNumbersVec(s);
                bool allZeroes = true;
                for (double dash : dashes) {
                    if (dash != 0.0) {
                        allZeroes = false;
                        break;
                    }
                }

                // if the stroke dash array contains only zeros,
                // force drawing of solid line.
                if (allZeroes == false) {
                    // if the dash count is odd the dashes should be duplicated
                    if ((dashes.size() & 1) != 0){
                        std::vector<double> t = dashes;
                        dashes.insert(dashes.end(), t.begin(), t.end());
                    }
                    prop->setDashArray(dashes);
                } else {
                    prop->setDashArrayNone();
                }
            }
        }

        //stroke-linejoin attribute handling
        if (!attributes.strokeLineJoin.empty()) {
            if (attributes.strokeLineJoin == std::string("miter")){
                prop->setLineJoin(LPenJoinStyle::SvgMiter);
            }else if (attributes.strokeLineJoin == std::string("round")){
                prop->setLineJoin(LPenJoinStyle::Round);
            }else if (attributes.strokeLineJoin == std::string("bevel")){
                prop->setLineJoin(LPenJoinStyle::Bevel);
            }
        }

        //stroke-linecap attribute handling
        if (!attributes.strokeLineCap.empty()) {
            if (attributes.strokeLineCap == std::string("butt")){
                prop->setLineCap(LPenCapStyle::Flat);
            }else if (attributes.strokeLineCap == std::string("round")){
                prop->setLineCap(LPenCapStyle::Round);
            }else if (attributes.strokeLineCap == std::string("square")){
                prop->setLineCap(LPenCapStyle::Square);
            }
        }

        //stroke-dashoffset attribute handling
        if (!attributes.strokeDashOffset.empty() && attributes.strokeDashOffset != LK_INHERIT){
            prop->setDashOffset(toDouble(attributes.strokeDashOffset));
        }

        //vector-effect attribute handling
        if (!attributes.vectorEffect.empty()) {
            if (attributes.vectorEffect == std::string("non-scaling-stroke")){
                prop->setVectorEffect(true);
            }else if (attributes.vectorEffect == std::string("none")){
                prop->setVectorEffect(false);
            }
        }

        //stroke-miterlimit
        if (!attributes.strokeMiterLimit.empty() && attributes.strokeMiterLimit != LK_INHERIT){
            prop->setMiterLimit(toDouble(attributes.strokeMiterLimit));
        }
        //stroke-opacity atttribute handling
        if (!attributes.strokeOpacity.empty() && attributes.strokeOpacity != LK_INHERIT){
            prop->setOpacity(std::min<double>(double(1.0), std::max<double>(double(0.0), toDouble(attributes.strokeOpacity))));
        }
        node->appendStyleProperty(prop, attributes.id);
    }
}

enum FontSizeSpec { XXSmall, XSmall, Small, Medium, Large, XLarge, XXLarge,
                    FontSizeNone, FontSizeValue };

static const double sizeTable[] =
    { double(6.9), double(8.3), double(10.0), double(12.0), double(14.4), double(17.3), double(20.7) };

static FontSizeSpec fontSizeSpec(std::string_view spec)
{
    switch (spec.at(0)) {
    case 'x':{
        if (spec == std::string("xx-small")){
            return XXSmall;
        }
        if (spec == std::string("x-small")){
            return XSmall;
        }
        if (spec == std::string("x-large")){
            return XLarge;
        }
        if (spec == std::string("xx-large")){
            return XXLarge;
        }
        break;
    }
    case 's':{
        if (spec == std::string("small")){
            return Small;
        }
        break;
    }
    case 'm':{
        if (spec == std::string("medium")){
            return Medium;
        }
        break;
    }
    case 'l':{
        if (spec == std::string("large")){
            return Large;
        }
        break;
    }
    case 'n':{
        if (spec == std::string("none")){
            return FontSizeNone;
        }
        break;
    }
    default:
        break;
    }
    return FontSizeValue;
}

static void parseFont(std::shared_ptr<LSvgNode> node,
                      const LSvgAttributes &attributes,
                      LSvgHandler *)
{
    if (attributes.fontFamily.empty() && attributes.fontSize.empty() && attributes.fontStyle.empty() &&
        attributes.fontWeight.empty() && attributes.fontVariant.empty() && attributes.textAnchor.empty()){
        return;
    }

    std::shared_ptr<LSvgFontStyle> fontStyle = nullptr;
    if (!attributes.fontFamily.empty()) {
        std::shared_ptr<LSvgDocument>  doc = node->document();
        if (doc!= nullptr) {
            std::shared_ptr<LSvgFont> svgFont = doc->svgFont(std::string(attributes.fontFamily));
            if (svgFont != nullptr){
                fontStyle = std::make_shared<LSvgFontStyle>(svgFont, doc);
            }
        }
    }
    if (fontStyle == nullptr){
        fontStyle = std::make_shared<LSvgFontStyle>();
    }
    if (!attributes.fontFamily.empty() && attributes.fontFamily != LK_INHERIT) {
        std::string family = std::string(LStringUtil::trim(attributes.fontFamily));
        if (!family.empty() && (family.at(0) == ('\'') || family.at(0) == ('\"'))){
            family = family.substr(1, family.size() - 2);
        }
        fontStyle->setFamily(family);
    }

    if (!attributes.fontSize.empty() && attributes.fontSize != LK_INHERIT) {
        // TODO: Support relative sizes 'larger' and 'smaller'.
        const FontSizeSpec spec = fontSizeSpec(attributes.fontSize);
        switch (spec) {
        case FontSizeNone:{
            break;
        }
        case FontSizeValue: {
            LSvgLengthType type;
            double fs = parseLength(attributes.fontSize, &type);
            fs = convertToPixels(fs, true, type);
            fontStyle->setSize(std::min<double>(fs, double(0xffff)));
            break;
        }
        default:{
            fontStyle->setSize(sizeTable[spec]);
            break;
        }
        }
    }

    if (!attributes.fontStyle.empty() && attributes.fontStyle != LK_INHERIT) {
        if (attributes.fontStyle == std::string("normal")) {
            fontStyle->setStyle(LFont::StyleNormal);
        } else if (attributes.fontStyle == std::string("italic")) {
            fontStyle->setStyle(LFont::StyleItalic);
        } else if (attributes.fontStyle == std::string("oblique")) {
            fontStyle->setStyle(LFont::StyleOblique);
        }
    }

    if (!attributes.fontWeight.empty() && attributes.fontWeight != LK_INHERIT) {
        bool ok = false;
        const int weightNum = LStringUtil::stoi(std::string(attributes.fontWeight), &ok);
        if (ok) {
            fontStyle->setWeight(weightNum);
        } else {
            if (attributes.fontWeight == std::string("normal")) {
                fontStyle->setWeight(LFont::Normal);
            } else if (attributes.fontWeight == std::string("bold")) {
                fontStyle->setWeight(LFont::Bold);
            } else if (attributes.fontWeight == std::string("bolder")) {
                fontStyle->setWeight(LSvgFontStyle::BOLDER);
            } else if (attributes.fontWeight == std::string("lighter")) {
                fontStyle->setWeight(LSvgFontStyle::LIGHTER);
            }
        }
    }

    if (!attributes.fontVariant.empty() && attributes.fontVariant != LK_INHERIT) {
        if (attributes.fontVariant == std::string("normal"))
            fontStyle->setVariant(LFont::MixedCase);
        else if (attributes.fontVariant == std::string("small-caps"))
            fontStyle->setVariant(LFont::SmallCaps);
    }

    if (!attributes.textAnchor.empty() && attributes.textAnchor != LK_INHERIT) {
        if (attributes.textAnchor == std::string("start")){
            fontStyle->setTextAnchor(LAlignment::Left);
        }else if (attributes.textAnchor == std::string("middle")){
            fontStyle->setTextAnchor(LAlignment::HCenter);
        }else if (attributes.textAnchor == std::string("end")){
            fontStyle->setTextAnchor(LAlignment::Right);
        }
    }

    node->appendStyleProperty(fontStyle, attributes.id);
}

static void parseTransform(std::shared_ptr<LSvgNode> node,
                           const LSvgAttributes &attributes,
                           LSvgHandler *)
{
    std::string_view trans = LStringUtil::trim(attributes.transform);
    if (trans.empty()){
        return;
    }
    LTransform2D matrix = parseTransformationMatrix(trans);

    if (!matrix.isIdentity()) {
        std::shared_ptr<LSvgTransformStyle> style = std::make_shared<LSvgTransformStyle>(LTransform2D(matrix));
        node->appendStyleProperty(style, attributes.id);
    }

}

static void parseVisibility(std::shared_ptr<LSvgNode> node,
                            const LSvgAttributes &attributes,
                            LSvgHandler *)
{
    std::shared_ptr<LSvgNode> parent = node->parent();

    if (parent && (attributes.visibility.empty() || attributes.visibility == LK_INHERIT))
        node->setVisible(parent->isVisible());
    else if (attributes.visibility == std::string("hidden") || attributes.visibility == std::string("collapse")) {
        node->setVisible(false);
    } else
        node->setVisible(true);
}

static void pathArcSegment(std::shared_ptr<LPainterPath> path,
                           double xc, double yc,
                           double th0, double th1,
                           double rx, double ry, double xAxisRotation)
{
    double sinTh, cosTh;
    double a00, a01, a10, a11;
    double x1, y1, x2, y2, x3, y3;
    double t;
    double thHalf;

    sinTh = std::sin(xAxisRotation * (std::numbers::pi / 180.0));
    cosTh = std::cos(xAxisRotation * (std::numbers::pi / 180.0));

    a00 =  cosTh * rx;
    a01 = -sinTh * ry;
    a10 =  sinTh * rx;
    a11 =  cosTh * ry;

    thHalf = 0.5 * (th1 - th0);
    t = (8.0 / 3.0) * std::sin(thHalf * 0.5) * std::sin(thHalf * 0.5) / std::sin(thHalf);
    x1 = xc + std::cos(th0) - t * std::sin(th0);
    y1 = yc + std::sin(th0) + t * std::cos(th0);
    x3 = xc + std::cos(th1);
    y3 = yc + std::sin(th1);
    x2 = x3 + t * std::sin(th1);
    y2 = y3 - t * std::cos(th1);

    path->cubicTo(LPoint<double>(a00 * x1 + a01 * y1, a10 * x1 + a11 * y1),
                 LPoint<double>(a00 * x2 + a01 * y2, a10 * x2 + a11 * y2),
                 LPoint<double>(a00 * x3 + a01 * y3, a10 * x3 + a11 * y3));
}

// the arc handling code underneath is from XSVG (BSD license)
/*
 * Copyright  2002 USC/Information Sciences Institute
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies
 * and that both that copyright notice and this permission notice
 * appear in supporting documentation, and that the name of
 * Information Sciences Institute not be used in advertising or
 * publicity pertaining to distribution of the software without
 * specific, written prior permission.  Information Sciences Institute
 * makes no representations about the suitability of this software for
 * any purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 * INFORMATION SCIENCES INSTITUTE DISCLAIMS ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL INFORMATION SCIENCES
 * INSTITUTE BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 */
static void pathArc(std::shared_ptr<LPainterPath>path,
                    double               rx,
                    double               ry,
                    double               x_axis_rotation,
                    int         large_arc_flag,
                    int         sweep_flag,
                    double               x,
                    double               y,
                    double curx, double cury)
{
    const double Pr1 = rx * rx;
    const double Pr2 = ry * ry;

    if (!Pr1 || !Pr2)
        return;

    double sin_th, cos_th;
    double a00, a01, a10, a11;
    double x0, y0, x1, y1, xc, yc;
    double d, sfactor, sfactor_sq;
    double th0, th1, th_arc;
    int i, n_segs;
    double dx, dy, dx1, dy1, Px, Py, check;

    rx = std::abs(rx);
    ry = std::abs(ry);

    sin_th = std::sin(x_axis_rotation * (std::numbers::pi / 180.0));
    cos_th = std::cos(x_axis_rotation * (std::numbers::pi / 180.0));

    dx = (curx - x) / 2.0;
    dy = (cury - y) / 2.0;
    dx1 =  cos_th * dx + sin_th * dy;
    dy1 = -sin_th * dx + cos_th * dy;
    Px = dx1 * dx1;
    Py = dy1 * dy1;
    /* Spec : check if radii are large enough */
    check = Px / Pr1 + Py / Pr2;
    if (check > 1) {
        rx = rx * std::sqrt(check);
        ry = ry * std::sqrt(check);
    }

    a00 =  cos_th / rx;
    a01 =  sin_th / rx;
    a10 = -sin_th / ry;
    a11 =  cos_th / ry;
    x0 = a00 * curx + a01 * cury;
    y0 = a10 * curx + a11 * cury;
    x1 = a00 * x + a01 * y;
    y1 = a10 * x + a11 * y;
    /* (x0, y0) is current point in transformed coordinate space.
       (x1, y1) is new point in transformed coordinate space.

       The arc fits a unit-radius circle in this space.
    */
    d = (x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0);
    if (!d)
        return;
    sfactor_sq = 1.0 / d - 0.25;
    if (sfactor_sq < 0) sfactor_sq = 0;
    sfactor = std::sqrt(sfactor_sq);
    if (sweep_flag == large_arc_flag) sfactor = -sfactor;
    xc = 0.5 * (x0 + x1) - sfactor * (y1 - y0);
    yc = 0.5 * (y0 + y1) + sfactor * (x1 - x0);
    /* (xc, yc) is center of the circle. */

    th0 = std::atan2(y0 - yc, x0 - xc);
    th1 = std::atan2(y1 - yc, x1 - xc);

    th_arc = th1 - th0;
    if (th_arc < 0 && sweep_flag)
        th_arc += 2 * std::numbers::pi;
    else if (th_arc > 0 && !sweep_flag)
        th_arc -= 2 * std::numbers::pi;

    n_segs = std::ceil(std::abs(th_arc / (std::numbers::pi * 0.5 + 0.001)));

    for (i = 0; i < n_segs; i++) {
        pathArcSegment(path, xc, yc,
                       th0 + i * th_arc / n_segs,
                       th0 + (i + 1) * th_arc / n_segs,
                       rx, ry, x_axis_rotation);
    }
}

static bool parsePathDataFast(std::string_view dataStr, std::shared_ptr<LPainterPath> path, bool limitLength)
{
    const int maxElementCount = 0x7fff; // Assume file corruption if more path elements than this
    double x0 = 0, y0 = 0;              // starting point
    double x = 0, y = 0;                // current point
    char lastMode = 0;
    LPoint<double> ctrlPt;
    const char *str = dataStr.data();
    const char *end = str + dataStr.size();

    bool ok = true;
    while (ok && str != end) {
        while (LStringUtil::isSpace(*str) && (str + 1) != end){
            ++str;
        }
        char pathElem = *str;
        ++str;
        char endc = *end;
        *const_cast<char *>(end) = '\0'; // parseNumbersArray requires 0-termination that std::string_view cannot guarantee
        const char *pattern = nullptr;
        if (pathElem == ('a') || pathElem == ('A'))
            pattern = "rrrffrr";
        std::vector<double> arg;
        parseNumbersArray(str, arg, pattern);
        *const_cast<char *>(end) = endc;
        if (pathElem == ('z') || pathElem == ('Z')){
            arg.push_back(0);//dummy
        }
        const double *num = arg.data();
        int count = arg.size();
        while (ok && count > 0) {
            double offsetX = x;        // correction offsets
            double offsetY = y;        // for relative commands
            switch (pathElem) {
            case 'm': {
                if (count < 2) {
                    ok = false;
                    break;
                }
                x = x0 = num[0] + offsetX;
                y = y0 = num[1] + offsetY;
                num += 2;
                count -= 2;
                path->moveTo(x0, y0);

                // As per 1.2  spec 8.3.2 The "moveto" commands
                // If a 'moveto' is followed by multiple pairs of coordinates without explicit commands,
                // the subsequent pairs shall be treated as implicit 'lineto' commands.
                pathElem = ('l');
            }
            break;
            case 'M': {
                if (count < 2) {
                    ok = false;
                    break;
                }
                x = x0 = num[0];
                y = y0 = num[1];
                num += 2;
                count -= 2;
                path->moveTo(x0, y0);

                // As per 1.2  spec 8.3.2 The "moveto" commands
                // If a 'moveto' is followed by multiple pairs of coordinates without explicit commands,
                // the subsequent pairs shall be treated as implicit 'lineto' commands.
                pathElem = ('L');
            }
            break;
            case 'z':
            case 'Z': {
                x = x0;
                y = y0;
                count--; // skip dummy
                num++;
                path->closeSubpath();
            }
            break;
            case 'l': {
                if (count < 2) {
                    ok = false;
                    break;
                }
                x = num[0] + offsetX;
                y = num[1] + offsetY;
                num += 2;
                count -= 2;
                path->lineTo(x, y);

            }
            break;
            case 'L': {
                if (count < 2) {
                    ok = false;
                    break;
                }
                x = num[0];
                y = num[1];
                num += 2;
                count -= 2;
                path->lineTo(x, y);
            }
            break;
            case 'h': {
                x = num[0] + offsetX;
                num++;
                count--;
                path->lineTo(x, y);
            }
            break;
            case 'H': {
                x = num[0];
                num++;
                count--;
                path->lineTo(x, y);
            }
            break;
            case 'v': {
                y = num[0] + offsetY;
                num++;
                count--;
                path->lineTo(x, y);
            }
            break;
            case 'V': {
                y = num[0];
                num++;
                count--;
                path->lineTo(x, y);
            }
            break;
            case 'c': {
                if (count < 6) {
                    ok = false;
                    break;
                }
                LPoint<double> c1(num[0] + offsetX, num[1] + offsetY);
                LPoint<double> c2(num[2] + offsetX, num[3] + offsetY);
                LPoint<double> e(num[4] + offsetX, num[5] + offsetY);
                num += 6;
                count -= 6;
                path->cubicTo(c1, c2, e);
                ctrlPt = c2;
                x = e.m_x;
                y = e.m_y;
                break;
            }
            case 'C': {
                if (count < 6) {
                    ok = false;
                    break;
                }
                LPoint<double> c1(num[0], num[1]);
                LPoint<double> c2(num[2], num[3]);
                LPoint<double> e(num[4], num[5]);
                num += 6;
                count -= 6;
                path->cubicTo(c1, c2, e);
                ctrlPt = c2;
                x = e.m_x;
                y = e.m_y;
                break;
            }
            case 's': {
                if (count < 4) {
                    ok = false;
                    break;
                }
                LPoint<double> c1;
                if (lastMode == 'c' || lastMode == 'C' ||
                    lastMode == 's' || lastMode == 'S')
                    c1 = LPoint<double>(2*x-ctrlPt.m_x, 2*y-ctrlPt.m_y);
                else
                    c1 = LPoint<double>(x, y);
                LPoint<double> c2(num[0] + offsetX, num[1] + offsetY);
                LPoint<double> e(num[2] + offsetX, num[3] + offsetY);
                num += 4;
                count -= 4;
                path->cubicTo(c1, c2, e);
                ctrlPt = c2;
                x = e.m_x;
                y = e.m_y;
                break;
            }
            case 'S': {
                if (count < 4) {
                    ok = false;
                    break;
                }
                LPoint<double> c1;
                if (lastMode == 'c' || lastMode == 'C' ||
                    lastMode == 's' || lastMode == 'S')
                    c1 = LPoint<double>(2*x-ctrlPt.m_x, 2*y-ctrlPt.m_y);
                else
                    c1 = LPoint<double>(x, y);
                LPoint<double> c2(num[0], num[1]);
                LPoint<double> e(num[2], num[3]);
                num += 4;
                count -= 4;
                path->cubicTo(c1, c2, e);
                ctrlPt = c2;
                x = e.m_x;
                y = e.m_y;
                break;
            }
            case 'q': {
                if (count < 4) {
                    ok = false;
                    break;
                }
                LPoint<double> c(num[0] + offsetX, num[1] + offsetY);
                LPoint<double> e(num[2] + offsetX, num[3] + offsetY);
                num += 4;
                count -= 4;
                path->quadTo(c, e);
                ctrlPt = c;
                x = e.m_x;
                y = e.m_y;
                break;
            }
            case 'Q': {
                if (count < 4) {
                    ok = false;
                    break;
                }
                LPoint<double> c(num[0], num[1]);
                LPoint<double> e(num[2], num[3]);
                num += 4;
                count -= 4;
                path->quadTo(c, e);
                ctrlPt = c;
                x = e.m_x;
                y = e.m_y;
                break;
            }
            case 't': {
                if (count < 2) {
                    ok = false;
                    break;
                }
                LPoint<double> e(num[0] + offsetX, num[1] + offsetY);
                num += 2;
                count -= 2;
                LPoint<double> c;
                if (lastMode == 'q' || lastMode == 'Q' ||
                    lastMode == 't' || lastMode == 'T')
                    c = LPoint<double>(2*x-ctrlPt.m_x, 2*y-ctrlPt.m_y);
                else
                    c = LPoint<double>(x, y);
                path->quadTo(c, e);
                ctrlPt = c;
                x = e.m_x;
                y = e.m_y;
                break;
            }
            case 'T': {
                if (count < 2) {
                    ok = false;
                    break;
                }
                LPoint<double> e(num[0], num[1]);
                num += 2;
                count -= 2;
                LPoint<double> c;
                if (lastMode == 'q' || lastMode == 'Q' ||
                    lastMode == 't' || lastMode == 'T')
                    c = LPoint<double>(2*x-ctrlPt.m_x, 2*y-ctrlPt.m_y);
                else
                    c = LPoint<double>(x, y);
                path->quadTo(c, e);
                ctrlPt = c;
                x = e.m_x;
                y = e.m_y;
                break;
            }
            case 'a': {
                if (count < 7) {
                    ok = false;
                    break;
                }
                double rx = (*num++);
                double ry = (*num++);
                double xAxisRotation = (*num++);
                double largeArcFlag  = (*num++);
                double sweepFlag = (*num++);
                double ex = (*num++) + offsetX;
                double ey = (*num++) + offsetY;
                count -= 7;
                double curx = x;
                double cury = y;
                pathArc(path, rx, ry, xAxisRotation, int(largeArcFlag),
                        int(sweepFlag), ex, ey, curx, cury);

                x = ex;
                y = ey;
            }
            break;
            case 'A': {
                if (count < 7) {
                    ok = false;
                    break;
                }
                double rx = (*num++);
                double ry = (*num++);
                double xAxisRotation = (*num++);
                double largeArcFlag  = (*num++);
                double sweepFlag = (*num++);
                double ex = (*num++);
                double ey = (*num++);
                count -= 7;
                double curx = x;
                double cury = y;
                pathArc(path, rx, ry, xAxisRotation, int(largeArcFlag),
                        int(sweepFlag), ex, ey, curx, cury);

                x = ex;
                y = ey;
            }
            break;
            default:
                ok = false;
                break;
            }
            lastMode = pathElem;
            if (limitLength && path->elementCount() > maxElementCount)
                ok = false;
        }
    }
    return ok;
}

static bool parseStyle(std::shared_ptr<LSvgNode> node,
                       const LXmlNode &xmlNode,
                       LSvgHandler *);

static bool parseStyle(std::shared_ptr<LSvgNode> node,
                       const LSvgAttributes &attributes,
                       LSvgHandler *);

static int parseClockValue(std::string_view str, bool *ok)
{
    int res = 0;
    int ms = 1000;
    str = LStringUtil::trim(str);
    if (str.ends_with(std::string("ms"))) {
        str = str.substr(0, str.size()-2);
        ms = 1;
    } else if (str.ends_with(std::string("s"))) {
        str = str.substr(0, str.size()-1);
    }
    double val = ms * toDouble(str, ok);
    if (ok) {
        if (val > std::numeric_limits<int>::min() && val < std::numeric_limits<int>::max())
            res = static_cast<int>(val);
        else
            *ok = false;
    }
    return res;
}

static void parseCSStoXMLAttrs(const std::shared_ptr<lcss::StyleComputed> computed, LXmlNode &xmlNode){

    if(computed != nullptr){
        std::unordered_map<std::string, std::string>::const_iterator iter = computed->rawProperties.cbegin();
        for(;iter != computed->rawProperties.cend(); ++iter){
            const std::string& id = iter->first;
            const std::string& value = iter->second;
            xmlNode.attribute(id, value);
        }
    }
}

void LSvgHandler::parseCSStoXMLAttrs(const std::string &css, std::list<LSvgCssAttribute> *attributes)
{
    std::shared_ptr<LSvgStyleSheet> sheet = std::make_shared<LSvgStyleSheet>(nullptr);
    LCssParser::parseStyleSheet(css, sheet);

    std::shared_ptr<lcss::StyleComputed> computed = std::make_shared<lcss::StyleComputed>();
    if(sheet->computeStyle(computed, nullptr)){

        std::unordered_map<std::string, std::string>::const_iterator iter = computed->rawProperties.cbegin();
        for(;iter != computed->rawProperties.cend(); ++iter){
            LSvgCssAttribute attribute;
            attribute.name = iter->first;
            attribute.value = iter->second;
            attributes->push_back(attribute);
        }
    }
}

static void cssStyleLookup(std::shared_ptr<LSvgNode> node,
                           LSvgHandler *handler,
                           std::shared_ptr<LSvgStyleSelector> selector,
                           LXmlNode &xmlNode)
{
    std::shared_ptr<lcss::StyleComputed> computed = selector->getStyleComputed(node);
    if(computed != nullptr){
        parseCSStoXMLAttrs(computed, xmlNode);
    }
}

static void cssStyleLookup(std::shared_ptr<LSvgNode> node,
                           LSvgHandler *handler,
                           std::shared_ptr<LSvgStyleSelector> selector)
{
    LXmlNode xmlNode;
    cssStyleLookup(node, handler, selector, xmlNode);
}

static void parseCssAnimations(std::shared_ptr<LSvgNode> node,
                               const LSvgAttributes &attributes,
                               LSvgHandler *handler)
{
    if (attributes.animationName.empty() || attributes.animationDuration.empty())
        return;

    bool ok;
    int duration = parseClockValue(attributes.animationDuration, &ok);
    int delay = parseClockValue(attributes.animationDelay, &ok);

    double repeatCount = (attributes.animationIterationCount == std::string("infinite")) ? -1 :
                            std::max<double>(1.0, toDouble(attributes.animationIterationCount));

    std::shared_ptr<LSvgCssAnimation> anim = handler->cssHandler().createAnimation(std::string(attributes.animationName));
    if (anim) {
        anim->setRunningTime(delay, duration);
        anim->setIterationCount(repeatCount);
        handler->document()->animator()->appendAnimation(node, anim);
        handler->setAnimPeriod(delay, delay + duration);
        handler->document()->setAnimated(true);
    }
}

LFlags<LSvgOption> LSvgHandler::options() const
{
    return m_options;
}

LSvgAnimatorType LSvgHandler::animatorType() const
{
    return m_animatorType;
}

bool LSvgHandler::trustedSourceMode() const
{
    return m_options.testFlag(LSvgOption::AssumeTrustedSource);
}

static inline std::list<std::string> stringToList(const std::string &str)
{
    std::list<std::string> lst = LStringUtil::split(str, ",", true);
    return lst;
}

static bool parseCoreNode(std::shared_ptr<LSvgNode> node,
                          LXmlDoc::const_iterator &xmlIter)
{
    std::list<std::string> features;
    std::list<std::string> extensions;
    std::list<std::string> languages;
    std::list<std::string> formats;
    std::list<std::string> fonts;
    std::string xmlClassStr;

    std::map<std::string_view, std::string_view> attrs = xmlIter.attributes();
    std::map<std::string_view, std::string_view>::const_iterator iter = attrs.cbegin();
    for (; iter != attrs.cend(); ++iter) {
        const std::string_view& name = iter->first;
        if (name.empty()){
            continue;
        }
        const std::string_view& value = iter->second;

        switch (name.at(0)) {
        case 'c':{
            if (name == std::string("class")){
                xmlClassStr = std::string(value);
            }
            break;
        }
        case 'r':
            if (name == std::string("requiredFeatures")){
                features = stringToList(std::string(value));
            }else if (name == std::string("requiredExtensions")){
                extensions = stringToList(std::string(value));
            }else if (name == std::string("requiredFormats")){
                formats = stringToList(std::string(value));
            }else if (name == std::string("requiredFonts")){
                fonts = stringToList(std::string(value));
            }
            break;
        case 's':{
            if (name == std::string("systemLanguage")){
                languages = stringToList(std::string(value));
            }
            break;
        }
        default:
            break;
        }
    }

    node->setRequiredFeatures(features);
    node->setRequiredExtensions(extensions);
    node->setRequiredLanguages(languages);
    node->setRequiredFormats(formats);
    node->setRequiredFonts(fonts);
    node->setNodeId(someId(xmlIter));
    node->setXmlClass(xmlClassStr);

    return true;
}

static void parseOpacity(std::shared_ptr<LSvgNode> node,
                         const LSvgAttributes &attributes,
                         LSvgHandler *)
{
    const std::string_view value = LStringUtil::trim(attributes.opacity);
    if (value.empty()){
        return;
    }

    bool ok = false;
    double op = toDouble(value, &ok);

    if (ok) {
        std::shared_ptr<LSvgOpacityStyle> opacity = std::make_shared<LSvgOpacityStyle>(lBound(double(0.0), op, double(1.0)));
        node->appendStyleProperty(opacity, attributes.id);
    }
}

static LCompositionMode svgToQtCompositionMode(const std::string &op)
{
    if (op == std::string("clear")) {
        return LCompositionMode::Clear;
    } else if (op == std::string("src")) {
        return LCompositionMode::Source;
    } else if (op == std::string("dst")) {
        return LCompositionMode::Destination;
    } else if (op == std::string("src-over")) {
        return LCompositionMode::SourceOver;
    } else if (op == std::string("dst-over")) {
        return LCompositionMode::DestinationOver;
    } else if (op == std::string("src-in")) {
        return LCompositionMode::SourceIn;
    } else if (op == std::string("dst-in")) {
        return LCompositionMode::DestinationIn;
    } else if (op == std::string("src-out")) {
        return LCompositionMode::SourceOut;
    } else if (op == std::string("dst-out")) {
        return LCompositionMode::DestinationOut;
    } else if (op == std::string("src-atop")) {
        return LCompositionMode::SourceAtop;
    } else if (op == std::string("dst-atop")) {
        return LCompositionMode::DestinationAtop;
    } else if (op == std::string("xor")) {
        return LCompositionMode::Xor;
    } else if (op == std::string("plus")) {
        return LCompositionMode::SvgPlus;
    } else if (op == std::string("multiply")) {
        return LCompositionMode::SvgMultiply;
    } else if (op == std::string("screen")) {
        return LCompositionMode::SvgScreen;
    } else if (op == std::string("overlay")) {
        return LCompositionMode::SvgOverlay;
    } else if (op == std::string("darken")) {
        return LCompositionMode::SvgDarken;
    } else if (op == std::string("lighten")) {
        return LCompositionMode::SvgLighten;
    } else if (op == std::string("color-dodge")) {
        return LCompositionMode::SvgColorDodge;
    } else if (op == std::string("color-burn")) {
        return LCompositionMode::SvgColorBurn;
    } else if (op == std::string("hard-light")) {
        return LCompositionMode::SvgHardLight;
    } else if (op == std::string("soft-light")) {
        return LCompositionMode::SvgSoftLight;
    } else if (op == std::string("difference")) {
        return LCompositionMode::SvgDifference;
    } else if (op == std::string("exclusion")) {
        return LCompositionMode::SvgExclusion;
    } else {
        lWarn()<<"Operation: "<<op<<" is not implemented";
    }

    return LCompositionMode::SourceOver;
}

static void parseCompOp(std::shared_ptr<LSvgNode> node,
                        const LSvgAttributes &attributes,
                        LSvgHandler *)
{
    const std::string_view value = LStringUtil::trim(attributes.compOp);
    if (value.empty()){
        return;
    }

    if (!value.empty()) {
        std::shared_ptr<LSvgCompOpStyle> compop = std::make_shared<LSvgCompOpStyle>(svgToQtCompositionMode(std::string(value)));
        node->appendStyleProperty(compop, attributes.id);
    }
}

static inline LSvgNode::DisplayMode displayStringToEnum(const std::string &str)
{
    if (str == std::string("inline")) {
        return LSvgNode::InlineMode;
    } else if (str == std::string("block")) {
        return LSvgNode::BlockMode;
    } else if (str == std::string("list-item")) {
        return LSvgNode::ListItemMode;
    } else if (str == std::string("run-in")) {
        return LSvgNode::RunInMode;
    } else if (str == std::string("compact")) {
        return LSvgNode::CompactMode;
    } else if (str == std::string("marker")) {
        return LSvgNode::MarkerMode;
    } else if (str == std::string("table")) {
        return LSvgNode::TableMode;
    } else if (str == std::string("inline-table")) {
        return LSvgNode::InlineTableMode;
    } else if (str == std::string("table-row-group")) {
        return LSvgNode::TableRowGroupMode;
    } else if (str == std::string("table-header-group")) {
        return LSvgNode::TableHeaderGroupMode;
    } else if (str == std::string("table-footer-group")) {
        return LSvgNode::TableFooterGroupMode;
    } else if (str == std::string("table-row")) {
        return LSvgNode::TableRowMode;
    } else if (str == std::string("table-column-group")) {
        return LSvgNode::TableColumnGroupMode;
    } else if (str == std::string("table-column")) {
        return LSvgNode::TableColumnMode;
    } else if (str == std::string("table-cell")) {
        return LSvgNode::TableCellMode;
    } else if (str == std::string("table-caption")) {
        return LSvgNode::TableCaptionMode;
    } else if (str == std::string("none")) {
        return LSvgNode::NoneMode;
    } else if (str == LK_INHERIT) {
        return LSvgNode::InheritMode;
    }
    return LSvgNode::BlockMode;
}

static void parseOthers(std::shared_ptr<LSvgNode> node,
                        const LSvgAttributes &attributes,
                        LSvgHandler *)
{
    const std::string_view value = LStringUtil::trim(attributes.display);
    if (value.empty()){
        return;
    }

    node->setDisplayMode(displayStringToEnum(std::string(value)));
}

static void parseExtendedAttributes(std::shared_ptr<LSvgNode> node,
                                    const LSvgAttributes &attributes,
                                    LSvgHandler *handler)
{
    if (handler->options().testFlag(LSvgOption::Tiny12FeaturesOnly))
        return;

    if (!attributes.mask.empty()) {
        std::string maskStr = std::string(LStringUtil::trim(attributes.mask));
        if (maskStr.size() > 3 && maskStr.substr(0, 3) == std::string("url")){
            maskStr = maskStr.substr(3, maskStr.size() - 3);
        }
        std::string maskId = idFromUrl(maskStr);
        if (maskId.starts_with(('#'))){
            maskId = maskId.substr(1);
        }
        node->setMaskId(maskId);
    }

    if (!attributes.markerStart.empty() &&
        !handler->options().testFlag(LSvgOption::Tiny12FeaturesOnly)) {
        std::string markerStr = std::string(LStringUtil::trim(attributes.markerStart));
        if (markerStr.size() > 3 && markerStr.substr(0, 3) == std::string("url")){
            markerStr = markerStr.substr(3, markerStr.size() - 3);
        }
        std::string markerId = idFromUrl(markerStr);
        if (markerId.starts_with(('#'))){
            markerId = markerId.substr(1);
        }
        node->setMarkerStartId(markerId);
    }
    if (!attributes.markerMid.empty() &&
        !handler->options().testFlag(LSvgOption::Tiny12FeaturesOnly)) {
        std::string markerStr = std::string(LStringUtil::trim(attributes.markerMid));
        if (markerStr.size() > 3 && markerStr.substr(0, 3) == std::string("url")){
            markerStr = markerStr.substr(3, markerStr.size() - 3);
        }
        std::string markerId = idFromUrl(markerStr);
        if (markerId.starts_with(('#'))){
            markerId = markerId.substr(1);
        }
        node->setMarkerMidId(markerId);
    }
    if (!attributes.markerEnd.empty() &&
        !handler->options().testFlag(LSvgOption::Tiny12FeaturesOnly)) {
        std::string markerStr = std::string(LStringUtil::trim(attributes.markerEnd));
        if (markerStr.size() > 3 && markerStr.substr(0, 3) == std::string("url")){
            markerStr = markerStr.substr(3, markerStr.size() - 3);
        }
        std::string markerId = idFromUrl(markerStr);
        if (markerId.starts_with(('#'))){
            markerId = markerId.substr(1);
        }
        node->setMarkerEndId(markerId);
    }

    if (!attributes.filter.empty() &&
        !handler->options().testFlag(LSvgOption::Tiny12FeaturesOnly)) {
        std::string filterStr = std::string(LStringUtil::trim(attributes.filter));

        if (filterStr.size() > 3 && filterStr.substr(0, 3) == std::string("url")){
            filterStr = filterStr.substr(3, filterStr.size() - 3);
        }
        std::string filterId = idFromUrl(filterStr);
        if (filterId.starts_with(('#'))){
            filterId = filterId.substr(1);
        }
        node->setFilterId(filterId);
    }

}

static void parseRenderingHints(std::shared_ptr<LSvgNode> node,
                                const LSvgAttributes &attributes,
                                LSvgHandler *)
{
    const std::string_view value = LStringUtil::trim(attributes.imageRendering);
    if (value.empty()){
        return;
    }

    std::string ir = std::string(value);
    std::shared_ptr<LSvgQualityStyle> p = std::make_shared<LSvgQualityStyle>();
    if (ir == std::string("auto")){
        p->setImageRendering(LSvgQualityStyle::ImageRenderingAuto);
    }else if (ir == std::string("optimizeSpeed")){
        p->setImageRendering(LSvgQualityStyle::ImageRenderingOptimizeSpeed);
    }else if (ir == std::string("optimizeQuality")){
        p->setImageRendering(LSvgQualityStyle::ImageRenderingOptimizeQuality);
    }
    node->appendStyleProperty(p, attributes.id);
}

static bool parseStyle(std::shared_ptr<LSvgNode> node,
                       const LSvgAttributes &attributes,
                       LSvgHandler *handler)
{
    parseColor(node, attributes, handler);
    parseBrush(node, attributes, handler);
    parsePen(node, attributes, handler);
    parseFont(node, attributes, handler);
    parseTransform(node, attributes, handler);
    parseVisibility(node, attributes, handler);
    parseOpacity(node, attributes, handler);
    parseCompOp(node, attributes, handler);
    parseRenderingHints(node, attributes, handler);
    parseOthers(node, attributes, handler);
    parseExtendedAttributes(node, attributes, handler);

    if (!handler->options().testFlag(LSvgOption::DisableCSSAnimations)){
        parseCssAnimations(node, attributes, handler);
    }
    return true;
}

static bool parseStyle(std::shared_ptr<LSvgNode> node,
                       const LXmlNode &xmlNode,
                       LSvgHandler *handler)
{
    return parseStyle(node, LSvgAttributes(xmlNode, handler), handler);
}

static bool parseAnchorNode(std::shared_ptr<LSvgNode> parent,
                            LXmlDoc::const_iterator &attributes,
                            LSvgHandler *)
{
    return true;
}

static bool parseBaseAnimate(std::shared_ptr<LSvgNode> parent,
                             LXmlDoc::const_iterator &attributes,
                             std::shared_ptr<LSvgAnimateNode> anim,
                             LSvgHandler *handler)
{
    std::string beginStr   = std::string(attributes.attribute("begin"));
    std::string durStr     = std::string(attributes.attribute("dur"));
    std::string endStr = std::string(attributes.attribute("end"));
    std::string repeatStr  = std::string(attributes.attribute("repeatCount"));
    std::string fillStr    = std::string(attributes.attribute("fill"));
    std::string addtv      = std::string(attributes.attribute("additive"));
    std::string linkId = std::string(attributes.attribute("xlink:href")).substr(1);

    if (linkId.empty()){
        linkId = std::string(attributes.attribute("href")).substr(0, 1);
    }

    bool ok = true;
    int begin = parseClockValue(beginStr, &ok);
    if (!ok)
        return false;
    int dur = parseClockValue(durStr, &ok);
    if (!ok)
        return false;
    int end = parseClockValue(endStr, &ok);
    if (!ok)
        return false;
    double repeatCount = (repeatStr == std::string("indefinite")) ? -1 :
                            std::max<double>(1.0, toDouble(repeatStr));

    LSvgAnimateNode::Fill fill = (fillStr == std::string("freeze")) ? LSvgAnimateNode::Freeze :
                                     LSvgAnimateNode::Remove;

    LSvgAnimateNode::Additive additive = (addtv == std::string("sum")) ? LSvgAnimateNode::Sum :
                                             LSvgAnimateNode::Replace;

    anim->setRunningTime(begin, dur, end, 0);
    anim->setRepeatCount(repeatCount);
    anim->setFill(fill);
    anim->setAdditiveType(additive);
    anim->setLinkId(linkId);

    parent->document()->setAnimated(true);

    handler->setAnimPeriod(begin, begin + dur);
    return true;
}

static void generateKeyFrames(std::list<double> &keyFrames, uint32_t count)
{
    if (count < 2)
        return;

    double spacing = 1.0f / (count - 1);
    for (uint32_t i = 0; i < count; i++) {
        keyFrames.push_back(i * spacing);
    }
}

static std::shared_ptr<LSvgNode> createAnimateColorNode(std::shared_ptr<LSvgNode> parent,
                                        LXmlDoc::const_iterator &attributes,
                                        LSvgHandler *handler)
{
    std::string_view fromStr    = attributes.attribute("from");
    std::string_view toStr      = attributes.attribute("to");
    std::string valuesStr  = std::string(attributes.attribute("values"));
    std::string targetStr  = std::string(attributes.attribute("attributeName"));

    if (targetStr != std::string("fill") && targetStr != std::string("stroke")){
        return nullptr;
    }

    std::list<LColor> colors;
    if (valuesStr.empty()) {
        LColor startColor, endColor;
        resolveColor(fromStr, startColor, handler);
        resolveColor(toStr, endColor, handler);
        colors.push_back(startColor);
        colors.push_back(endColor);
    } else {
        std::vector<std::string> str = LStringUtil::splitToVec(valuesStr, ";");
        std::vector<std::string>::const_iterator itr;
        for (itr = str.cbegin(); itr != str.cend(); ++itr) {
            LColor color;
            resolveColor(*itr, color, handler);
            colors.push_back(color);
        }
    }

    std::shared_ptr<LSvgAnimatedPropertyColor> prop = std::dynamic_pointer_cast<LSvgAnimatedPropertyColor>
        (LSvgAbstractAnimatedProperty::createAnimatedProperty(targetStr));
    if (!prop){
        return nullptr;
    }

    prop->setColors(colors);

    std::list<double> keyFrames;
    generateKeyFrames(keyFrames, colors.size());
    prop->setKeyFrames(keyFrames);

    std::shared_ptr<LSvgAnimateColor> anim = std::make_shared<LSvgAnimateColor>(parent);
    anim->appendProperty(prop);

    parseBaseAnimate(parent, attributes, anim, handler);

    return anim;
}

static std::shared_ptr<LSvgNode> createAimateMotionNode(std::shared_ptr<LSvgNode> parent,
                                        LXmlDoc::const_iterator &attributes,
                                        LSvgHandler *)
{
    return nullptr;
}

static void parseNumberTriplet(std::vector<double> &values, const char *&s)
{
    std::vector<double> vec = parseNumbersVec(s);
    for(const double& v: vec){
        values.push_back(v);
    }
    for (int i = 3 - vec.size(); i > 0; --i){
        values.push_back(0.0);
    }
}

static std::shared_ptr<LSvgNode> createAnimateTransformNode(std::shared_ptr<LSvgNode> parent,
                                            LXmlDoc::const_iterator &attributes,
                                            LSvgHandler *handler)
{
    std::string typeStr    = std::string(attributes.attribute("type"));
    std::string values     = std::string(attributes.attribute("values"));
    std::string fromStr    = std::string(attributes.attribute("from"));
    std::string toStr      = std::string(attributes.attribute("to"));
    std::string byStr      = std::string(attributes.attribute("by"));

    std::vector<double> vals;
    if (values.empty()) {
        const char *s;
        if (fromStr.empty()) {
            if (!byStr.empty()) {
                vals.push_back(0.0);
                vals.push_back(0.0);
                vals.push_back(0.0);
                parseNumberTriplet(vals, s = byStr.data());
            } else {
                // To-animation not defined.
                return nullptr;
            }
        } else {
            if (!toStr.empty()) {
                // From-to-animation.
                parseNumberTriplet(vals, s = fromStr.data());
                parseNumberTriplet(vals, s = toStr.data());
            } else if (!byStr.empty()) {
                // From-by-animation.
                parseNumberTriplet(vals, s = fromStr.data());
                parseNumberTriplet(vals, s = byStr.data());
                for (int i = vals.size() - 3; i < vals.size(); ++i){
                    vals[i] += vals[i - 3];
                }
            } else {
                return nullptr;
            }
        }
    } else {
        const char *s = values.data();
        while (s && *s != (0)) {
            parseNumberTriplet(vals, s);
            if (*s == (0))
                break;
            ++s;
        }
    }
    if (vals.size() % 3 != 0)
        return nullptr;

    std::list<LPoint<double>> translations;
    std::list<LPoint<double>> scales;
    std::list<double> rotations;
    std::list<LPoint<double>> coRotations;
    std::list<LPoint<double>> skews;

    for (int i = 0; i < vals.size(); i+=3) {
        if (typeStr == std::string("translate")) {
            translations.push_back(LPoint<double>(vals.at(i), vals.at(i + 1)));
        } else if (typeStr == std::string("scale")) {
            scales.push_back(LPoint<double>(vals.at(i), vals.at(i + 1)));
        } else if (typeStr == std::string("rotate")) {
            rotations.push_back(vals.at(i));
            coRotations.push_back(LPoint<double>(vals.at(i + 1), vals.at(i + 2)));
        } else if (typeStr == std::string("skewX")) {
            skews.push_back(LPoint<double>(vals.at(i), 0));
        } else if (typeStr == std::string("skewY")) {
            skews.push_back(LPoint<double>(0, vals.at(i)));
        } else {
            return nullptr;
        }
    }

    std::shared_ptr<LSvgAnimatedPropertyTransform> prop = std::dynamic_pointer_cast<LSvgAnimatedPropertyTransform>
        (LSvgAbstractAnimatedProperty::createAnimatedProperty(std::string("transform")));
    if (prop == nullptr){
        return nullptr;
    }

    prop->setTranslations(translations);
    prop->setScales(scales);
    prop->setRotations(rotations);
    prop->setCentersOfRotation(coRotations);
    prop->setSkews(skews);

    std::list<double> keyFrames;
    generateKeyFrames(keyFrames, vals.size() / 3);
    prop->setKeyFrames(keyFrames);

    std::shared_ptr<LSvgAnimateTransform> anim = std::make_shared<LSvgAnimateTransform>(parent);
    anim->appendProperty(prop);

    parseBaseAnimate(parent, attributes, anim, handler);

    return anim;
}

static std::shared_ptr<LSvgNode> createAnimateNode(std::shared_ptr<LSvgNode> parent,
                                   LXmlDoc::const_iterator &attributes,
                                   LSvgHandler *)
{
    return nullptr;
}

static bool parseAudioNode(std::shared_ptr<LSvgNode> parent,
                           LXmlDoc::const_iterator &attributes,
                           LSvgHandler *)
{
    return true;
}

static std::shared_ptr<LSvgNode> createCircleNode(std::shared_ptr<LSvgNode> parent,
                                  LXmlDoc::const_iterator &attributes,
                                  LSvgHandler *)
{
    const std::string_view cx = attributes.attribute("cx");
    const std::string_view cy = attributes.attribute("cy");
    const std::string_view r = attributes.attribute("r");
    double ncx = toDouble(cx);
    double ncy = toDouble(cy);
    double nr  = toDouble(r);
    if (nr < 0.0)
        return nullptr;

    LRect<double> rect(ncx-nr, ncy-nr, nr*2, nr*2);
    std::shared_ptr<LSvgNode> circle = std::make_shared<LSvgCircle>(parent, rect);
    return circle;
}

static std::shared_ptr<LSvgNode> createDefsNode(std::shared_ptr<LSvgNode> parent,
                                LXmlDoc::const_iterator &attributes,
                                LSvgHandler *)
{
    std::shared_ptr<LSvgDefs> defs = std::make_shared<LSvgDefs>(parent);
    return defs;
}

static bool parseDiscardNode(std::shared_ptr<LSvgNode> parent,
                             LXmlDoc::const_iterator &attributes,
                             LSvgHandler *)
{
    return true;
}

static std::shared_ptr<LSvgNode> createEllipseNode(std::shared_ptr<LSvgNode> parent,
                                   LXmlDoc::const_iterator &attributes,
                                   LSvgHandler *)
{
    const std::string_view cx = attributes.attribute("cx");
    const std::string_view cy = attributes.attribute("cy");
    const std::string_view rx = attributes.attribute("rx");
    const std::string_view ry = attributes.attribute("ry");
    double ncx = toDouble(cx);
    double ncy = toDouble(cy);
    double nrx = toDouble(rx);
    double nry = toDouble(ry);

    LRect<double> rect(ncx-nrx, ncy-nry, nrx*2, nry*2);
    std::shared_ptr<LSvgNode> ellipse = std::make_shared<LSvgEllipse>(parent, rect);
    return ellipse;
}

static std::shared_ptr<LSvgStyleProperty> createFontNode(std::shared_ptr<LSvgNode> parent,
                                         LXmlDoc::const_iterator &attributes,
                                         LSvgHandler *)
{
    const std::string_view hax = attributes.attribute("horiz-adv-x");
    std::string myId     = someId(attributes);

    double horizAdvX = toDouble(hax);

    while (parent && parent->type() != LSvgNode::Doc) {
        parent = parent->parent();
    }

    if (parent && !myId.empty()) {
        std::shared_ptr<LSvgDocument> doc = std::dynamic_pointer_cast<LSvgDocument>(parent);
        std::shared_ptr<LSvgFont> font = doc->svgFont(myId);
        if (!font) {
            font = std::make_shared<LSvgFont>(horizAdvX);
            font->setFamilyName(myId);
            doc->addSvgFont(font);
        }
        return std::make_shared<LSvgFontStyle>(font, doc);
    }
    return nullptr;
}

static bool parseFontFaceNode(std::shared_ptr<LSvgStyleProperty> parent,
                              LXmlDoc::const_iterator &attributes,
                              LSvgHandler *)
{
    if (parent->type() != LSvgStyleProperty::FONT) {
        return false;
    }

    std::shared_ptr<LSvgFontStyle> style = std::dynamic_pointer_cast<LSvgFontStyle>(parent);
    std::shared_ptr<LSvgFont> font = style->svgFont();
    std::string name   = std::string(attributes.attribute("font-family"));
    const std::string_view unitsPerEmStr = attributes.attribute("units-per-em");

    double unitsPerEm = toDouble(unitsPerEmStr);
    if (!unitsPerEm){
        unitsPerEm = LSvgFont::DEFAULT_UNITS_PER_EM;
    }

    if (!name.empty()){
        font->setFamilyName(name);
    }
    font->setUnitsPerEm(unitsPerEm);

    if (!font->familyName().empty()){
        if (!style->doc()->svgFont(font->familyName())){
            style->doc()->addSvgFont(font);
        }
    }

    return true;
}

static bool parseFontFaceNameNode(std::shared_ptr<LSvgStyleProperty> parent,
                                  LXmlDoc::const_iterator &attributes,
                                  LSvgHandler *)
{
    if (parent->type() != LSvgStyleProperty::FONT) {
        return false;
    }

    std::shared_ptr<LSvgFontStyle> style = std::dynamic_pointer_cast<LSvgFontStyle>(parent);
    std::shared_ptr<LSvgFont> font = style->svgFont();
    std::string name   = std::string(attributes.attribute("name"));

    if (!name.empty())
        font->setFamilyName(name);

    if (!font->familyName().empty())
        if (!style->doc()->svgFont(font->familyName()))
            style->doc()->addSvgFont(font);

    return true;
}

static bool parseFontFaceSrcNode(std::shared_ptr<LSvgStyleProperty> parent,
                                 LXmlDoc::const_iterator &attributes,
                                 LSvgHandler *)
{
    return true;
}

static bool parseFontFaceUriNode(std::shared_ptr<LSvgStyleProperty> parent,
                                 LXmlDoc::const_iterator &attributes,
                                 LSvgHandler *)
{
    return true;
}

static bool parseForeignObjectNode(std::shared_ptr<LSvgNode> parent,
                                   LXmlDoc::const_iterator &attributes,
                                   LSvgHandler *)
{
    return true;
}

static std::shared_ptr<LSvgNode> createGNode(std::shared_ptr<LSvgNode> parent,
                             LXmlDoc::const_iterator &attributes,
                             LSvgHandler *)
{
    std::shared_ptr<LSvgG> node = std::make_shared<LSvgG>(parent);
    return node;
}

static bool parseGlyphNode(std::shared_ptr<LSvgStyleProperty> parent,
                           LXmlDoc::const_iterator &attributes,
                           LSvgHandler *)
{
    if (parent->type() != LSvgStyleProperty::FONT) {
        return false;
    }

    std::shared_ptr<LSvgFontStyle> style = std::dynamic_pointer_cast<LSvgFontStyle>(parent);
    std::shared_ptr<LSvgFont> font = style->svgFont();
    createSvgGlyph(font, attributes);
    return true;
}

static bool parseHandlerNode(std::shared_ptr<LSvgNode> parent,
                             LXmlDoc::const_iterator &attributes,
                             LSvgHandler *)
{
    return true;
}

static bool parseHkernNode(std::shared_ptr<LSvgNode> parent,
                           LXmlDoc::const_iterator &attributes,
                           LSvgHandler *)
{
    return true;
}

static std::shared_ptr<LSvgNode> createImageNode(std::shared_ptr<LSvgNode> parent,
                                 LXmlDoc::const_iterator &attributes,
                                 LSvgHandler *handler)
{
    const std::string_view x = attributes.attribute("x");
    const std::string_view y = attributes.attribute("y");
    const std::string_view width  = attributes.attribute("width");
    const std::string_view height = attributes.attribute("height");
    std::string filename = std::string(attributes.attribute("xlink:href"));
    if (filename.empty() && !handler->options().testFlag(LSvgOption::Tiny12FeaturesOnly)){
        filename = std::string(attributes.attribute("href"));
}
    double nx = toDouble(x);
    double ny = toDouble(y);
    LSvgLengthType type;
    double nwidth = parseLength(width, &type);
    nwidth = convertToPixels(nwidth, true, type);

    double nheight = parseLength(height, &type);
    nheight = convertToPixels(nheight, false, type);

    filename = LStringUtil::trim(filename);
    if (filename.empty()) {
        lWarn() << "LSvgHandler: Image filename is empty";
        return 0;
    }
    if (nwidth <= 0 || nheight <= 0) {
        lWarn() << "LSvgHandler: Width or height for" << filename << "image was not greater than 0";
        return 0;
    }

    std::shared_ptr<LImage> image;
    enum {
        NotLoaded,
        LoadedFromData,
        LoadedFromFile
    } filenameType = NotLoaded;

    if (filename.starts_with(std::string("data"))) {
        std::string mimeType;
        std::vector<uint8_t> data;
        LUrl url(filename);
        if (url.decodeDataUrl(mimeType, data)) {
            image = LImageReader::loadFromMemory(data.data(), data.size());
            filenameType = LoadedFromData;
        }
    }

    if (image == nullptr) {
        std::filesystem::path path(filename);
        if (path.is_relative()) {
            filename = std::filesystem::absolute(filename).string();
        }

        if (handler->trustedSourceMode()) {
            if(!filename.ends_with(".svg")){
                image = LImageReader::loadFromFile(filename);
            }
            if(image != nullptr){
                filenameType = LoadedFromFile;
            }
        }
    }

    if (image == nullptr) {
        lWarn() << "Could not create image from" << filename;
        return nullptr;
    }

    if (image->format() == LImage::Format::ARGB32){
        image = image->convertToFormat(LImage::Format::ARGB32Premultiplied);
    }

    std::shared_ptr<LSvgNode> img = std::make_shared<LSvgImage>(parent,
                                  image,
                                  filenameType == LoadedFromFile ? filename : std::string{},
                                  LRect<double>(nx, ny, nwidth, nheight));
    return img;
}

static std::shared_ptr<LSvgNode> createLineNode(std::shared_ptr<LSvgNode> parent,
                                LXmlDoc::const_iterator &attributes,
                                LSvgHandler *)
{
    const std::string_view x1 = attributes.attribute("x1");
    const std::string_view y1 = attributes.attribute("y1");
    const std::string_view x2 = attributes.attribute("x2");
    const std::string_view y2 = attributes.attribute("y2");
    double nx1 = toDouble(x1);
    double ny1 = toDouble(y1);
    double nx2 = toDouble(x2);
    double ny2 = toDouble(y2);

    LLine<double> lineBounds(nx1, ny1, nx2, ny2);
    std::shared_ptr<LSvgNode> line = std::make_shared<LSvgLine>(parent, lineBounds);
    return line;
}


static void parseBaseGradient(std::shared_ptr<LSvgNode> node,
                              LXmlDoc::const_iterator &attributes,
                              std::shared_ptr<LSvgGradientStyle> gradProp,
                              LSvgHandler *handler)
{
    std::string link   = std::string(attributes.attribute("xlink:href"));
    std::string_view trans  = attributes.attribute("gradientTransform");
    std::string spread = std::string(attributes.attribute("spreadMethod"));
    std::string units = std::string(attributes.attribute("gradientUnits"));
    std::string_view colorStr = attributes.attribute("color");
    std::string_view colorOpacityStr = attributes.attribute("color-opacity");

    LColor color;
    if (constructColor(colorStr, colorOpacityStr, color, handler)) {
        handler->popColor();
        handler->pushColor(color);
    }

    LTransform2D matrix;
    std::shared_ptr<LGradient> grad = gradProp->gradient();
    if (node && !link.empty()) {
        std::shared_ptr<LSvgStyleProperty> prop = node->styleProperty(link);
        //qDebug()<<"inherited "<<prop<<" ("<<link<<")";
        if (prop && prop->type() == LSvgStyleProperty::GRADIENT) {
            std::shared_ptr<LSvgGradientStyle> inherited = std::dynamic_pointer_cast<LSvgGradientStyle>(prop);
            if (!inherited->stopLink().empty()) {
                gradProp->setStopLink(inherited->stopLink(), handler->document());
            } else {
                grad->m_stops = inherited->gradient()->m_stops;
                gradProp->setGradientStopsSet(inherited->gradientStopsSet());
            }

            matrix = inherited->transform();
        } else {
            gradProp->setStopLink(link, handler->document());
        }
    }

    if (!trans.empty()) {
        matrix = parseTransformationMatrix(trans);
        gradProp->setTransform(matrix);
    } else if (!matrix.isIdentity()) {
        gradProp->setTransform(matrix);
    }

    if (!spread.empty()) {
        if (spread == std::string("pad")) {
            grad->m_spread = LGradient::Spread::Pad;
        } else if (spread == std::string("reflect")) {
            grad->m_spread = LGradient::Spread::Reflect;
        } else if (spread == std::string("repeat")) {
            grad->m_spread = LGradient::Spread::Repeat;
        }
    }

    if (units.empty() || units == std::string("objectBoundingBox")) {
        grad->m_coordinate = LGradient::Mode::Object;
    }
}

static std::shared_ptr<LSvgStyleProperty> createLinearGradientNode(std::shared_ptr<LSvgNode> node,
                                                   LXmlDoc::const_iterator &attributes,
                                                   LSvgHandler *handler)
{
    const std::string_view x1 = attributes.attribute("x1");
    const std::string_view y1 = attributes.attribute("y1");
    const std::string_view x2 = attributes.attribute("x2");
    const std::string_view y2 = attributes.attribute("y2");

    double nx1 = 0.0;
    double ny1 = 0.0;
    double nx2 = 1.0;
    double ny2 = 0.0;

    if (!x1.empty()){
        nx1 =  convertToNumber(x1);
    }
    if (!y1.empty()){
        ny1 =  convertToNumber(y1);
    }
    if (!x2.empty()){
        nx2 =  convertToNumber(x2);
    }
    if (!y2.empty()){
        ny2 =  convertToNumber(y2);
    }

    std::shared_ptr<LSvgNode> itr = node;
    while (itr && itr->type() != LSvgNode::Doc) {
        itr = itr->parent();
    }

    std::shared_ptr<LGradient> grad = std::make_shared<LGradient>();
    grad->m_type = LGradient::Type::Linear;
    grad->m_linear.origin.x = nx1;
    grad->m_linear.origin.y = ny1;
    grad->m_linear.end.x = nx2;
    grad->m_linear.end.y = ny2;

    grad->m_interpolation = LGradient::Interpolation::Component;

    std::shared_ptr<LSvgGradientStyle> prop = std::make_shared<LSvgGradientStyle>(grad);
    parseBaseGradient(node, attributes, prop, handler);

    return prop;
}

static bool parseMetadataNode(std::shared_ptr<LSvgNode> parent,
                              LXmlDoc::const_iterator &attributes,
                              LSvgHandler *)
{
    return true;
}

static bool parseMissingGlyphNode(std::shared_ptr<LSvgStyleProperty> parent,
                                  LXmlDoc::const_iterator &attributes,
                                  LSvgHandler *)
{
    if (parent->type() != LSvgStyleProperty::FONT) {
        return false;
    }

    std::shared_ptr<LSvgFontStyle> style = std::dynamic_pointer_cast<LSvgFontStyle>(parent);
    std::shared_ptr<LSvgFont> font = style->svgFont();
    createSvgGlyph(font, attributes);
    return true;
}

static bool parseMpathNode(std::shared_ptr<LSvgNode> parent,
                           LXmlDoc::const_iterator &attributes,
                           LSvgHandler *)
{
    return true;
}

static bool parseMaskNode(std::shared_ptr<LSvgNode> parent,
                          LXmlDoc::const_iterator &attributes,
                          LSvgHandler *)
{
    return true;
}

static bool parseMarkerNode(std::shared_ptr<LSvgNode> ,
                            LXmlDoc::const_iterator &,
                            LSvgHandler *)
{
    return true;
}

static std::shared_ptr<LSvgNode> createMaskNode(std::shared_ptr<LSvgNode> parent,
                                LXmlDoc::const_iterator &attributes,
                                LSvgHandler *)
{
    const std::string_view x      = attributes.attribute("x");
    const std::string_view y      = attributes.attribute("y");
    const std::string_view width  = attributes.attribute("width");
    const std::string_view height = attributes.attribute("height");
    const std::string_view mU     = attributes.attribute("maskUnits");
    const std::string_view mCU    = attributes.attribute("maskContentUnits");

    LSvgUnitTypes nmU = mU.contains(std::string("userSpaceOnUse")) ?
                               LSvgUnitTypes::userSpaceOnUse : LSvgUnitTypes::objectBoundingBox;

    LSvgUnitTypes nmCU = mCU.contains(std::string("objectBoundingBox")) ?
                                LSvgUnitTypes::objectBoundingBox : LSvgUnitTypes::userSpaceOnUse;

    bool ok;
    LSvgLengthType type;

    LSvgUnitTypes nmUx = nmU;
    LSvgUnitTypes nmUy = nmU;
    LSvgUnitTypes nmUw = nmU;
    LSvgUnitTypes nmUh = nmU;
    double nx = parseLength(x, &type, &ok);
    nx = convertToPixels(nx, true, type);
    if (x.empty() || !ok) {
        nx = -0.1;
        nmUx = LSvgUnitTypes::objectBoundingBox;
    } else if (type == LSvgLengthType::percent && nmU == LSvgUnitTypes::userSpaceOnUse) {
        nx = nx / 100. * parent->document()->viewBox().width();
    } else if (type == LSvgLengthType::percent) {
        nx = nx / 100.;
    }

    double ny = parseLength(y, &type, &ok);
    ny = convertToPixels(ny, true, type);
    if (y.empty() || !ok) {
        ny = -0.1;
        nmUy = LSvgUnitTypes::objectBoundingBox;
    } else if (type == LSvgLengthType::percent && nmU == LSvgUnitTypes::userSpaceOnUse) {
        ny = ny / 100. * parent->document()->viewBox().height();
    } else if (type == LSvgLengthType::percent) {
        ny = ny / 100.;
    }

    double nwidth = parseLength(width, &type, &ok);
    nwidth = convertToPixels(nwidth, true, type);
    if (width.empty() || !ok) {
        nwidth = 1.2;
        nmUw = LSvgUnitTypes::objectBoundingBox;
    } else if (type == LSvgLengthType::percent && nmU == LSvgUnitTypes::userSpaceOnUse) {
        nwidth = nwidth / 100. * parent->document()->viewBox().width();
    } else if (type == LSvgLengthType::percent) {
        nwidth = nwidth / 100.;
    }

    double nheight = parseLength(height, &type, &ok);
    nheight = convertToPixels(nheight, true, type);
    if (height.empty() || !ok) {
        nheight = 1.2;
        nmUh = LSvgUnitTypes::objectBoundingBox;
    } else if (type == LSvgLengthType::percent && nmU == LSvgUnitTypes::userSpaceOnUse) {
        nheight = nheight / 100. * parent->document()->viewBox().height();
    } else if (type == LSvgLengthType::percent) {
        nheight = nheight / 100.;
    }

    LRect<double> bounds(nx, ny, nwidth, nheight);
    if (bounds.isEmpty()){
        return nullptr;
    }

    std::shared_ptr<LSvgNode> mask = std::make_shared<LSvgMask>(parent, LSvgRectInfo(bounds, nmUx, nmUy, nmUw, nmUh), nmCU);

    return mask;
}

static void parseFilterBounds(std::shared_ptr<LSvgNode> , LXmlDoc::const_iterator &attributes,
                              LSvgHandler *, LSvgRectInfo *rect)
{
    const std::string_view xStr        = attributes.attribute("x");
    const std::string_view yStr        = attributes.attribute("y");
    const std::string_view widthStr    = attributes.attribute("width");
    const std::string_view heightStr   = attributes.attribute("height");

    double x = 0;
    if (!xStr.empty()) {
        LSvgLengthType type;
        x = parseLength(xStr, &type);
        if (type != LSvgLengthType::pt) {
            x = convertToPixels(x, true, type);
            rect->m_unitX = (LSvgUnitTypes::userSpaceOnUse);
        }
        if (type == LSvgLengthType::percent) {
            x /= 100.;
            rect->m_unitX = (LSvgUnitTypes::objectBoundingBox);
        }
        rect->m_rect.m_x = (x);
    }
    double y = 0;
    if (!yStr.empty()) {
        LSvgLengthType type;
        y = parseLength(yStr, &type);
        if (type != LSvgLengthType::pt) {
            y = convertToPixels(y, false, type);
            rect->m_unitY = (LSvgUnitTypes::userSpaceOnUse);
        }
        if (type == LSvgLengthType::percent) {
            y /= 100.;
            rect->m_unitX = (LSvgUnitTypes::objectBoundingBox);
        }
        rect->m_rect.m_y = (y);
    }
    double width = 0;
    if (!widthStr.empty()) {
        LSvgLengthType type;
        width = parseLength(widthStr, &type);
        if (type != LSvgLengthType::pt) {
            width = convertToPixels(width, true, type);
            rect->m_unitW = (LSvgUnitTypes::userSpaceOnUse);
        }
        if (type == LSvgLengthType::percent) {
            width /= 100.;
            rect->m_unitX = (LSvgUnitTypes::objectBoundingBox);
        }
        rect->m_rect.m_width = (width);
    }
    double height = 0;
    if (!heightStr.empty()) {
        LSvgLengthType type;
        height = parseLength(heightStr, &type);
        if (type != LSvgLengthType::pt) {
            height = convertToPixels(height, false, type);
            rect->m_unitH = (LSvgUnitTypes::userSpaceOnUse);
        }
        if (type == LSvgLengthType::percent) {
            height /= 100.;
            rect->m_unitX = (LSvgUnitTypes::objectBoundingBox);
        }
        rect->m_rect.m_height = (height);
    }
}

static std::shared_ptr<LSvgNode> createFilterNode(std::shared_ptr<LSvgNode> parent,
                                  LXmlDoc::const_iterator &attributes,
                                  LSvgHandler *handler)
{
    std::string fU = std::string(attributes.attribute("filterUnits"));
    std::string pU = std::string(attributes.attribute("primitiveUnits"));

    LSvgUnitTypes filterUnits = fU.contains(std::string("userSpaceOnUse")) ?
                                       LSvgUnitTypes::userSpaceOnUse : LSvgUnitTypes::objectBoundingBox;

    LSvgUnitTypes primitiveUnits = pU.contains(std::string("objectBoundingBox")) ?
                                          LSvgUnitTypes::objectBoundingBox : LSvgUnitTypes::userSpaceOnUse;

    // https://www.w3.org/TR/SVG11/filters.html#FilterEffectsRegion
    // If ‘x’ or ‘y’ is not specified, the effect is as if a value of -10% were specified.
    // If ‘width’ or ‘height’ is not specified, the effect is as if a value of 120% were specified.
    LSvgRectInfo rect;
    if (filterUnits == LSvgUnitTypes::userSpaceOnUse) {
        double width = parent->document()->viewBox().width();
        double height = parent->document()->viewBox().height();
        rect = LSvgRectInfo(LRect<double>(-0.1 * width, -0.1 * height, 1.2 * width, 1.2 * height),
                         LSvgUnitTypes::userSpaceOnUse, LSvgUnitTypes::userSpaceOnUse,
                         LSvgUnitTypes::userSpaceOnUse, LSvgUnitTypes::userSpaceOnUse);
    } else {
        rect = LSvgRectInfo(LRect<double>(-0.1, -0.1, 1.2, 1.2),
                         LSvgUnitTypes::objectBoundingBox, LSvgUnitTypes::objectBoundingBox,
                         LSvgUnitTypes::objectBoundingBox, LSvgUnitTypes::objectBoundingBox);
    }

    parseFilterBounds(parent, attributes, handler, &rect);

    std::shared_ptr<LSvgNode> filter = std::make_shared<LSvgFilterContainer>(parent, rect, filterUnits, primitiveUnits);
    return filter;
}

static void parseFilterAttributes(std::shared_ptr<LSvgNode> parent, LXmlDoc::const_iterator &attributes,
                                  LSvgHandler *handler, std::string *inString, std::string *outString,
                                  LSvgRectInfo *rect)
{
    *inString = std::string(attributes.attribute("in"));
    *outString = std::string(attributes.attribute("result"));

    // https://www.w3.org/TR/SVG11/filters.html#FilterPrimitiveSubRegion
    // the default subregion is 0%,0%,100%,100%, where as a special-case the percentages are
    // relative to the dimensions of the filter region, thus making the the default filter primitive
    // subregion equal to the filter region.
    *rect = LSvgRectInfo(LRect<double>(0, 0, 1.0, 1.0),
                      LSvgUnitTypes::unknown, LSvgUnitTypes::unknown,
                      LSvgUnitTypes::unknown, LSvgUnitTypes::unknown);
    // if we recognize unit == unknown we use the filter as a reference instead of the item, see
    // LSvgFeFilterPrimitive::localSubRegion

    parseFilterBounds(parent, attributes, handler, rect);
}

static std::shared_ptr<LSvgNode> createFeColorMatrixNode(std::shared_ptr<LSvgNode> parent,
                                         LXmlDoc::const_iterator &attributes,
                                         LSvgHandler *handler)
{
    const std::string typeString = std::string(attributes.attribute("type"));
    std::string valuesString = std::string(attributes.attribute("values"));

    std::string inputString;
    std::string outputString;
    LSvgRectInfo rect;

    LSvgFeColorMatrix::ColorShiftType type;
    LSvgFeColorMatrix::Matrix values;
    LSvgFeColorMatrix::fill(values, 0);

    parseFilterAttributes(parent, attributes, handler,
                          &inputString, &outputString, &rect);

    if (typeString.starts_with(std::string("saturate"))){
        type = LSvgFeColorMatrix::ColorShiftType::Saturate;
    }else if (typeString.starts_with(std::string("hueRotate"))){
        type = LSvgFeColorMatrix::ColorShiftType::HueRotate;
    }else if (typeString.starts_with(std::string("luminanceToAlpha"))){
        type = LSvgFeColorMatrix::ColorShiftType::LuminanceToAlpha;
    }else{
        type = LSvgFeColorMatrix::ColorShiftType::Matrix;
    }

    if (!valuesString.empty()) {
        const static std::string delimiterRE("[,\\s]");
        const std::vector<std::string> valueStrings = LStringUtil::splitToVec(valuesString, delimiterRE, true);
        int k=0;
        bool ok;
        for(int i=0; i<5; ++i){
            if(k >= valueStrings.size()){
                break;
            }
            for(int j=0; j<5; ++j){
                if(k >= valueStrings.size()){
                    break;
                }
                double v = toDouble(valueStrings[k], &ok);
                if(ok){
                    values[i][j] = v;
                }
                ++k;
            }
        }
    } else {
        LSvgFeColorMatrix::setIdentity(values);
    }

    std::shared_ptr<LSvgNode> filter = std::make_shared<LSvgFeColorMatrix>(parent, inputString, outputString, rect, type, values);
    return filter;
}

static std::shared_ptr<LSvgNode> createFeGaussianBlurNode(std::shared_ptr<LSvgNode> parent,
                                          LXmlDoc::const_iterator &attributes,
                                          LSvgHandler *handler)
{
    const std::string edgeModeString    = std::string(attributes.attribute("edgeMode"));
    std::string stdDeviationString  = std::string(attributes.attribute("stdDeviation"));

    std::string inputString;
    std::string outputString;
    LSvgRectInfo rect;

    LSvgFeGaussianBlur::EdgeMode edgemode = LSvgFeGaussianBlur::EdgeMode::Duplicate;

    parseFilterAttributes(parent, attributes, handler,
                          &inputString, &outputString, &rect);
    double stdDeviationX = 0;
    double stdDeviationY = 0;
    if (stdDeviationString.contains(" ")){
        std::vector<std::string> ss = LStringUtil::splitToVec(stdDeviationString, " ");
        if(ss.size() >= 2){
            stdDeviationX = std::max<double>(0., toDouble(ss[0]));
            stdDeviationY = std::max<double>(0., toDouble(ss[ss.size()-1]));
        }else if(ss.size() >=1){
            stdDeviationY = stdDeviationX = std::max<double>(0., toDouble(ss[0]));
        }
    } else {
        stdDeviationY = stdDeviationX = std::max<double>(0., toDouble(stdDeviationString));
    }

    if (edgeModeString.starts_with(std::string("wrap"))){
        edgemode = LSvgFeGaussianBlur::EdgeMode::Wrap;
    }else if (edgeModeString.starts_with(std::string("none"))){
        edgemode = LSvgFeGaussianBlur::EdgeMode::None;
    }

    std::shared_ptr<LSvgNode> filter = std::make_shared<LSvgFeGaussianBlur>(parent, inputString, outputString, rect, stdDeviationX, stdDeviationY, edgemode);
    return filter;
}

static std::shared_ptr<LSvgNode> createFeOffsetNode(std::shared_ptr<LSvgNode> parent,
                                    LXmlDoc::const_iterator &attributes,
                                    LSvgHandler *handler)
{
    std::string_view dxString = attributes.attribute("dx");
    std::string_view dyString = attributes.attribute("dy");

    std::string inputString;
    std::string outputString;
    LSvgRectInfo rect;

    parseFilterAttributes(parent, attributes, handler,
                          &inputString, &outputString, &rect);

    double dx = 0;
    if (!dxString.empty()) {
        LSvgLengthType type;
        dx = parseLength(dxString, &type);
        if (type != LSvgLengthType::pt){
            dx = convertToPixels(dx, true, type);
        }
    }

    double dy = 0;
    if (!dyString.empty()) {
        LSvgLengthType type;
        dy = parseLength(dyString, &type);
        if (type != LSvgLengthType::pt){
            dy = convertToPixels(dy, true, type);
        }
    }

    std::shared_ptr<LSvgNode> filter = std::make_shared<LSvgFeOffset>(parent, inputString, outputString, rect, dx, dy);
    return filter;
}

static std::shared_ptr<LSvgNode> createFeCompositeNode(std::shared_ptr<LSvgNode> parent,
                                       LXmlDoc::const_iterator &attributes,
                                       LSvgHandler *handler)
{
    std::string in2String        = std::string(attributes.attribute("in2"));
    std::string operatorString   = std::string(attributes.attribute("operator"));
    std::string k1String         = std::string(attributes.attribute("k1"));
    std::string k2String         = std::string(attributes.attribute("k2"));
    std::string k3String         = std::string(attributes.attribute("k3"));
    std::string k4String         = std::string(attributes.attribute("k4"));

    std::string inputString;
    std::string outputString;
    LSvgRectInfo rect;

    parseFilterAttributes(parent, attributes, handler,
                          &inputString, &outputString, &rect);

    LSvgFeComposite::Operator op = LSvgFeComposite::Operator::Over;
    if (operatorString.starts_with(("in"))){
        op = LSvgFeComposite::Operator::In;
    }else if (operatorString.starts_with(("out"))){
        op = LSvgFeComposite::Operator::Out;
    }else if (operatorString.starts_with(("atop"))){
        op = LSvgFeComposite::Operator::Atop;
    }else if (operatorString.starts_with(("xor"))){
        op = LSvgFeComposite::Operator::Xor;
    }else if (operatorString.starts_with(("lighter"))){
        op = LSvgFeComposite::Operator::Lighter;
    }else if (operatorString.starts_with(("arithmetic"))){
        op = LSvgFeComposite::Operator::Arithmetic;
    }

    std::array<double, 4> k;

    if (op == LSvgFeComposite::Operator::Arithmetic) {
        bool ok;
        double v = toDouble(k1String, &ok);
        k[0] = ok ? v : 0;
        v = toDouble(k2String, &ok);
        k[1] = ok ? v : 0;
        v = toDouble(k3String, &ok);
        k[2] = ok ? v : 0;
        v = toDouble(k4String, &ok);
        k[3] = ok ? v : 0;
    }else{
        for(int i=0; i<k.size(); ++i){
            k[i] = 0;
        }
    }

    std::shared_ptr<LSvgNode> filter = std::make_shared<LSvgFeComposite>(parent, inputString, outputString, rect, in2String, op, k);
    return filter;
}


static std::shared_ptr<LSvgNode> createFeMergeNode(std::shared_ptr<LSvgNode> parent,
                                   LXmlDoc::const_iterator &attributes,
                                   LSvgHandler *handler)
{
    std::string inputString;
    std::string outputString;
    LSvgRectInfo rect;

    parseFilterAttributes(parent, attributes, handler,
                          &inputString, &outputString, &rect);

    std::shared_ptr<LSvgNode> filter = std::make_shared<LSvgFeMerge>(parent, inputString, outputString, rect);
    return filter;
}

static std::shared_ptr<LSvgNode> createFeFloodNode(std::shared_ptr<LSvgNode> parent,
                                   LXmlDoc::const_iterator &attributes,
                                   LSvgHandler *handler)
{
    std::string_view colorStr          = attributes.attribute("flood-color");
    const std::string_view opacityStr  = attributes.attribute("flood-opacity");

    LColor color;
    if (!constructColor(colorStr, opacityStr, color, handler)) {
        color = LColor(LColorID::Black);
        bool ok;
        double op = std::min<double>(double(1.0), std::max<double>(double(0.0), toDouble(opacityStr, &ok)));
        if (ok){
            color.m_alpha = op*255;
        }
    }

    std::string inputString;
    std::string outputString;
    LSvgRectInfo rect;

    parseFilterAttributes(parent, attributes, handler,
                          &inputString, &outputString, &rect);

    std::shared_ptr<LSvgNode> filter = std::make_shared<LSvgFeFlood>(parent, inputString, outputString, rect, color);
    return filter;
}

static std::shared_ptr<LSvgNode> createFeMergeNodeNode(std::shared_ptr<LSvgNode> parent,
                                       LXmlDoc::const_iterator &attributes,
                                       LSvgHandler *handler)
{
    std::string inputString;
    std::string outputString;
    LSvgRectInfo rect;

    parseFilterAttributes(parent, attributes, handler,
                          &inputString, &outputString, &rect);

    std::shared_ptr<LSvgNode> filter = std::make_shared<LSvgFeMergeNode>(parent, inputString, outputString, rect);
    return filter;
}

static std::shared_ptr<LSvgNode> createFeBlendNode(std::shared_ptr<LSvgNode> parent,
                                   LXmlDoc::const_iterator &attributes,
                                   LSvgHandler *handler)
{
    std::string in2String = std::string(attributes.attribute("in2"));
    std::string modeString = std::string(attributes.attribute("mode"));

    std::string inputString;
    std::string outputString;
    LSvgRectInfo rect;

    parseFilterAttributes(parent, attributes, handler,
                          &inputString, &outputString, &rect);

    LSvgFeBlend::Mode mode = LSvgFeBlend::Mode::Normal;
    if (modeString.starts_with(("multiply"))){
        mode = LSvgFeBlend::Mode::Multiply;
    }else if (modeString.starts_with(("screen"))){
        mode = LSvgFeBlend::Mode::Screen;
    }else if (modeString.starts_with(("darken"))){
        mode = LSvgFeBlend::Mode::Darken;
    }else if (modeString.starts_with(("lighten"))){
        mode = LSvgFeBlend::Mode::Lighten;
    }

    std::shared_ptr<LSvgNode> filter = std::make_shared<LSvgFeBlend>(parent, inputString, outputString, rect, in2String, mode);
    return filter;
}

static std::shared_ptr<LSvgNode> createFeUnsupportedNode(std::shared_ptr<LSvgNode> parent,
                                         LXmlDoc::const_iterator &attributes,
                                         LSvgHandler *handler)
{
    std::string inputString;
    std::string outputString;
    LSvgRectInfo rect;

    parseFilterAttributes(parent, attributes, handler,
                          &inputString, &outputString, &rect);

    std::shared_ptr<LSvgNode> filter = std::make_shared<LSvgFeUnsupported>(parent, inputString, outputString, rect);
    return filter;
}

static bool parseSymbolLikeAttributes(LXmlDoc::const_iterator &attributes, LSvgHandler *handler,
                                      LRect<double> *rect, LRect<double> *viewBox, LPoint<double> *refPoint,
                                      LFlags<LSvgPreserveAspectRatio> *aspect,
                                      LSvgSymbolLike::Overflow *overflow,
                                      bool marker = false)
{
    const std::string_view xStr        = attributes.attribute("x");
    const std::string_view yStr        = attributes.attribute("y");
    const std::string_view refXStr     = attributes.attribute("refX");
    const std::string_view refYStr     = attributes.attribute("refY");
    const std::string_view widthStr    = attributes.attribute(marker ? "markerWidth":"width");
    const std::string_view heightStr   = attributes.attribute(marker ? "markerHeight":"height");
    const std::string pAspectRStr     = std::string(attributes.attribute("preserveAspectRatio"));
    const std::string_view overflowStr = attributes.attribute("overflow");

    std::string viewBoxStr = std::string(attributes.attribute("viewBox"));


    double x = 0;
    if (!xStr.empty()) {
        LSvgLengthType type;
        x = parseLength(xStr, &type);
        if (type != LSvgLengthType::pt)
            x = convertToPixels(x, true, type);
    }
    double y = 0;
    if (!yStr.empty()) {
        LSvgLengthType type;
        y = parseLength(yStr, &type);
        if (type != LSvgLengthType::pt)
            y = convertToPixels(y, false, type);
    }
    double width = 0;
    if (!widthStr.empty()) {
        LSvgLengthType type;
        width = parseLength(widthStr, &type);
        if (type != LSvgLengthType::pt)
            width = convertToPixels(width, true, type);
    }
    double height = 0;
    if (!heightStr.empty()) {
        LSvgLengthType type;
        height = parseLength(heightStr, &type);
        if (type != LSvgLengthType::pt)
            height = convertToPixels(height, false, type);
    }

    *rect = LRect<double>(x, y, width, height);

    x = 0;
    if (!refXStr.empty()) {
        LSvgLengthType type;
        x = parseLength(refXStr, &type);
        if (type != LSvgLengthType::pt)
            x = convertToPixels(x, true, type);
    }
    y = 0;
    if (!refYStr.empty()) {
        LSvgLengthType type;
        y = parseLength(refYStr, &type);
        if (type != LSvgLengthType::pt)
            y = convertToPixels(y, false, type);
    }
    *refPoint = LPoint<double>(x,y);

    std::vector<std::string> viewBoxValues;
    if (!viewBoxStr.empty()) {
        viewBoxValues = LStringUtil::splitToVec(viewBoxStr, "[ ,\t\r\n]+", true);
    }
    if (viewBoxValues.size() == 4) {
        std::string xStr      = LStringUtil::trim(viewBoxValues[0]);
        std::string yStr      = LStringUtil::trim(viewBoxValues[1]);
        std::string widthStr  = LStringUtil::trim(viewBoxValues[2]);
        std::string heightStr = LStringUtil::trim(viewBoxValues[3]);

        LSvgLengthType lt;
        double x = parseLength(xStr, &lt);
        double y = parseLength(yStr, &lt);
        double w = parseLength(widthStr, &lt);
        double h = parseLength(heightStr, &lt);

        *viewBox = LRect<double>(x, y, w, h);

    } else if (width > 0 && height > 0) {
        *viewBox = LRect<double>(0, 0, width, height);
    } else {
        *viewBox = handler->document()->viewBox();
    }

    if (viewBox->isNull())
        return false;

    std::list<std::string> pAspectRStrs = LStringUtil::split(pAspectRStr, " ");
    LSvgPreserveAspectRatio aspectX = LSvgPreserveAspectRatio::xMid;
    LSvgPreserveAspectRatio aspectY = LSvgPreserveAspectRatio::yMid;
    LSvgPreserveAspectRatio aspectMS = LSvgPreserveAspectRatio::meet;

    for (auto &pAStr : std::as_const(pAspectRStrs)) {
        if (pAStr.starts_with(std::string("none"))) {
            aspectX = LSvgPreserveAspectRatio::None;
            aspectY = LSvgPreserveAspectRatio::None;
        }else {
            if (pAStr.starts_with(std::string("xMin"))){
                aspectX = LSvgPreserveAspectRatio::xMin;
            }else if (pAStr.starts_with(std::string("xMax"))){
                aspectX = LSvgPreserveAspectRatio::xMax;
            }
            if (pAStr.ends_with(std::string("YMin"))){
                aspectY = LSvgPreserveAspectRatio::yMin;
            }else if (pAStr.ends_with(std::string("YMax"))){
                aspectY = LSvgPreserveAspectRatio::yMax;
            }
        }

        if (pAStr.ends_with(std::string("slice")))
            aspectMS = LSvgPreserveAspectRatio::slice;
    }
    *aspect = aspectX | aspectY | aspectMS;

    // overflow is not limited to the symbol element but it is often found with the symbol element.
    // the symbol element makes little sense without the overflow attribute so it is added here.
    // if we decide to remove this from LSvgSymbol, the default value should be set to visible.

    // The default value is visible but chrome uses default value hidden.
    *overflow = LSvgSymbolLike::Overflow::Hidden;

    if (overflowStr.ends_with(std::string("auto"))){
        *overflow = LSvgSymbolLike::Overflow::Auto;
    }else if (overflowStr.ends_with(std::string("visible"))){
        *overflow = LSvgSymbolLike::Overflow::Visible;
    }else if (overflowStr.ends_with(std::string("hidden"))){
        *overflow = LSvgSymbolLike::Overflow::Hidden;
    }else if (overflowStr.ends_with(std::string("scroll"))){
        *overflow = LSvgSymbolLike::Overflow::Scroll;
    }
    return true;
}

static std::shared_ptr<LSvgNode> createSymbolNode(std::shared_ptr<LSvgNode> parent,
                                  LXmlDoc::const_iterator &attributes,
                                  LSvgHandler *handler)
{
    LRect<double> rect, viewBox;
    LPoint<double> refP;
    LFlags<LSvgPreserveAspectRatio> aspect;
    LSvgSymbolLike::Overflow overflow;

    if (!parseSymbolLikeAttributes(attributes, handler, &rect, &viewBox, &refP, &aspect, &overflow)){
        return nullptr;
    }

    refP = LPoint<double>(0, 0); //refX, refY is ignored in Symbol in Firefox and Chrome.
    std::shared_ptr<LSvgNode> symbol = std::make_shared<LSvgSymbol>(parent, rect, viewBox, refP, aspect, overflow);
    return symbol;
}

static std::shared_ptr<LSvgNode> createMarkerNode(std::shared_ptr<LSvgNode> parent,
                                  LXmlDoc::const_iterator &attributes,
                                  LSvgHandler *handler)
{
    LRect<double> rect, viewBox;
    LPoint<double> refP;
    LFlags<LSvgPreserveAspectRatio> aspect;
    LSvgSymbolLike::Overflow overflow;

    const std::string orientStr      = std::string(attributes.attribute("orient"));
    const std::string markerUnitsStr = std::string(attributes.attribute("markerUnits"));

    double orientationAngle = 0;
    LSvgMarker::Orientation orientation;
    if (orientStr.starts_with(std::string("auto-start-reverse")))
        orientation = LSvgMarker::Orientation::AutoStartReverse;
    else if (orientStr.starts_with(std::string("auto")))
        orientation = LSvgMarker::Orientation::Auto;
    else {
        orientation = LSvgMarker::Orientation::Value;
        bool ok;
        double a;
        if (orientStr.ends_with(("turn")))
            a = 360. * toDouble(orientStr.substr(0, orientStr.length()-4), &ok);
        else if (orientStr.ends_with(("grad")))
            a = toDouble(orientStr.substr(0, orientStr.length()-4), &ok);
        else if (orientStr.ends_with(("rad")))
            a = 180. / std::numbers::pi * toDouble(orientStr.substr(0, orientStr.length()-3), &ok);
        else
            a = toDouble(orientStr, &ok);
        if (ok)
            orientationAngle = a;
    }

    LSvgMarker::MarkerUnits markerUnits = LSvgMarker::MarkerUnits::StrokeWidth;
    if (markerUnitsStr.starts_with(std::string("userSpaceOnUse")))
        markerUnits = LSvgMarker::MarkerUnits::UserSpaceOnUse;

    if (!parseSymbolLikeAttributes(attributes, handler, &rect, &viewBox, &refP, &aspect, &overflow, true))
        return nullptr;

    std::shared_ptr<LSvgNode> marker = std::make_shared<LSvgMarker>(parent, rect, viewBox, refP, aspect, overflow,
                                                                    orientation, orientationAngle, markerUnits);
    return marker;
}

static std::shared_ptr<LSvgNode> createPathNode(std::shared_ptr<LSvgNode> parent,
                                LXmlDoc::const_iterator &attributes,
                                LSvgHandler *handler)
{
    std::string_view data = attributes.attribute("d");

    std::shared_ptr<LPainterPath> lPath = std::make_shared<LPainterPath>();
    lPath->setFillRule(LFillRule::Winding);
    if (!parsePathDataFast(data, lPath, !handler->trustedSourceMode())){
        lWarn() << "Invalid path data; path truncated.";
    }

    std::shared_ptr<LSvgNode> path = std::make_shared<LSvgPath>(parent, lPath);
    return path;
}

static std::shared_ptr<LSvgNode> createPolygonNode(std::shared_ptr<LSvgNode> parent,
                                   LXmlDoc::const_iterator &attributes,
                                   LSvgHandler *)
{
    std::string pointsStr  = std::string(attributes.attribute("points"));

    //same QPolygon parsing is in createPolylineNode
    const char *s = pointsStr.data();
    std::vector<double> points = parseNumbersVec(s);
    LPolygon<double> poly(points.size()/2);
    if (poly.size() < 2){
        return nullptr;
    }

    for (int i = 0; i < poly.size(); ++i){
        poly[i] = LPoint<double>(points.at(2 * i), points.at(2 * i + 1));
    }
    std::shared_ptr<LSvgNode> polygon = std::make_shared<LSvgPolygon>(parent, poly);
    return polygon;
}

static std::shared_ptr<LSvgNode> createPolylineNode(std::shared_ptr<LSvgNode> parent,
                                    LXmlDoc::const_iterator &attributes,
                                    LSvgHandler *)
{
    std::string pointsStr  = std::string(attributes.attribute("points"));

    //same QPolygon parsing is in createPolygonNode
    const char *s = pointsStr.data();
    std::vector<double> points = parseNumbersVec(s);
    LPolygon<double> poly(points.size()/2);
    if (poly.size() < 2){
        return nullptr;
    }

    for (int i = 0; i < poly.size(); ++i)
        poly[i] = LPoint<double>(points.at(2 * i), points.at(2 * i + 1));

    std::shared_ptr<LSvgNode> line = std::make_shared<LSvgPolyline>(parent, poly);
    return line;
}

static bool parsePrefetchNode(std::shared_ptr<LSvgNode> parent,
                              LXmlDoc::const_iterator &attributes,
                              LSvgHandler *)
{
    return true;
}

static std::shared_ptr<LSvgStyleProperty> createRadialGradientNode(std::shared_ptr<LSvgNode> node,
                                                   LXmlDoc::const_iterator &attributes,
                                                   LSvgHandler *handler)
{
    const std::string_view cx = attributes.attribute("cx");
    const std::string_view cy = attributes.attribute("cy");
    const std::string_view r  = attributes.attribute("r");
    const std::string_view fx = attributes.attribute("fx");
    const std::string_view fy = attributes.attribute("fy");

    double ncx = 0.5;
    double ncy = 0.5;
    if (!cx.empty()){
        ncx = toDouble(cx);
    }
    if (!cy.empty()){
        ncy = toDouble(cy);
    }

    double nr = 0.5;
    if (!r.empty()){
        nr = toDouble(r);
    }
    if (nr <= 0.0){
        return nullptr;
    }

    double nfx = ncx;
    if (!fx.empty()){
        nfx = toDouble(fx);
    }
    double nfy = ncy;
    if (!fy.empty()){
        nfy = toDouble(fy);
    }

    std::shared_ptr<LGradient> grad = std::make_shared<LGradient>();
    grad->m_type = LGradient::Type::Radial;
    grad->m_radial.center.x = ncx;
    grad->m_radial.center.y = ncx;
    grad->m_radial.center.radius = nr;
    grad->m_radial.focal.x = nfx;
    grad->m_radial.focal.y = nfy;
    grad->m_radial.focal.radius = 0;
    grad->m_interpolation = LGradient::Interpolation::Component;

    std::shared_ptr<LSvgGradientStyle>prop = std::make_shared<LSvgGradientStyle>(grad);
    parseBaseGradient(node, attributes, prop, handler);
    return prop;
}

static std::shared_ptr<LSvgNode> createRectNode(std::shared_ptr<LSvgNode> parent,
                                LXmlDoc::const_iterator &attributes,
                                LSvgHandler *)
{
    const std::string_view x      = attributes.attribute("x");
    const std::string_view y      = attributes.attribute("y");
    const std::string_view width  = attributes.attribute("width");
    const std::string_view height = attributes.attribute("height");
    const std::string_view rx      = attributes.attribute("rx");
    const std::string_view ry      = attributes.attribute("ry");

    bool ok = true;
    LSvgLengthType type;
    double nwidth = parseLength(width, &type, &ok);
    if (!ok)
        return nullptr;
    nwidth = convertToPixels(nwidth, true, type);
    double nheight = parseLength(height, &type, &ok);
    if (!ok)
        return nullptr;
    nheight = convertToPixels(nheight, true, type);
    double nrx = toDouble(rx);
    double nry = toDouble(ry);

    LRect<double> bounds(toDouble(x), toDouble(y), nwidth, nheight, false);
    if (bounds.isEmpty()){
        return nullptr;
    }

    if (!rx.empty() && ry.empty()){
        nry = nrx;
    }else if (!ry.empty() && rx.empty()){
        nrx = nry;
    }

    //9.2 The 'rect'  element clearly specifies it
    // but the case might in fact be handled because
    // we draw rounded rectangles differently
    if (nrx > bounds.width()/2){
        nrx = bounds.width()/2;
    }
    if (nry > bounds.height()/2){
        nry = bounds.height()/2;
    }

    //we draw rounded rect from 0...99
    //svg from 0...bounds.width()/2 so we're adjusting the
    //coordinates
    nrx *= (100/(bounds.width()/2));
    nry *= (100/(bounds.height()/2));

    std::shared_ptr<LSvgNode> rect = std::make_shared<LSvgRect>(parent, bounds, nrx, nry);
    return rect;
}

static bool parseScriptNode(std::shared_ptr<LSvgNode> parent,
                            LXmlDoc::const_iterator &attributes,
                            LSvgHandler *)
{
    return true;
}

static bool parseSetNode(std::shared_ptr<LSvgNode> parent,
                         LXmlDoc::const_iterator &attributes,
                         LSvgHandler *)
{
    return true;
}

static std::shared_ptr<LSvgStyleProperty> createSolidColorNode(std::shared_ptr<LSvgNode> parent,
                                               LXmlDoc::const_iterator &attributes,
                                               LSvgHandler *handler)
{
    std::string_view solidColorStr = attributes.attribute("solid-color");
    std::string_view solidOpacityStr = attributes.attribute("solid-opacity");

    if (solidOpacityStr.empty()){
        solidOpacityStr = attributes.attribute("opacity");
    }

    LColor color;
    if (!constructColor(solidColorStr, solidOpacityStr, color, handler)){
        return 0;
    }
    std::shared_ptr<LSvgSolidColorStyle> style = std::make_shared<LSvgSolidColorStyle>(color);
    return style;
}

static bool parseStopNode(std::shared_ptr<LSvgStyleProperty> parent,
                          LXmlDoc::const_iterator &attributes,
                          LSvgHandler *handler)
{
    if (parent->type() != LSvgStyleProperty::GRADIENT){
        return false;
    }
    std::string nodeIdStr     = someId(attributes);
    std::string xmlClassStr   = std::string(attributes.attribute("class"));

    //### nasty hack because stop gradients are not in the rendering tree
    //    we force a dummy node with the same id and class into a rendering
    //    tree to figure out whether the selector has a style for it
    //    LSvgStyleSelector should be coded in a way that could avoid it
    std::shared_ptr<LSvgDummyNode> dummy = std::make_shared<LSvgDummyNode>();
    dummy->setNodeId(nodeIdStr);
    dummy->setXmlClass(xmlClassStr);

    LXmlNode node = *attributes;

    cssStyleLookup(dummy, handler, handler->selector(), node);
    parseStyle(dummy, node, handler);

    LSvgAttributes attrs(node, handler);

    std::shared_ptr<LSvgGradientStyle> style = std::dynamic_pointer_cast<LSvgGradientStyle>(parent);
    std::string_view colorStr    = attrs.stopColor;
    LColor color;

    bool ok = true;
    double offset = convertToNumber(attrs.offset, &ok);
    if (!ok){
        offset = 0.0;
    }
    if (colorStr.empty()) {
        colorStr = ("#000000");
    }

    constructColor(colorStr, attrs.stopOpacity, color, handler);

    std::shared_ptr<LGradient>  grad = style->gradient();

    offset = std::min<double>(double(1), std::max<double>(double(0), offset)); // Clamp to range [0, 1]
    LGradientStops stops;
    if (style->gradientStopsSet()) {
        stops = grad->m_stops;
        // If the stop offset equals the one previously added, add an epsilon to make it greater.
        if(!stops.empty()){
            if (offset <= stops.back().first){
                offset = stops.back().first + FLT_EPSILON;
            }
        }

    }

    // If offset is greater than one, it must be clamped to one.
    if (offset > 1.0) {
        if ((stops.size() == 1)) {
            stops.back().first = 1.0 - FLT_EPSILON;
            grad->m_stops = stops;
        }else if(stops.size()>=2){
            auto it = stops.cbegin();
            std::advance(it, stops.size() - 2);
            if(it->first < 1.0 - FLT_EPSILON){
                stops.back().first = 1.0 - FLT_EPSILON;
                grad->m_stops = stops;
            }
        }
        offset = 1.0;
    }

    grad->setColorAt(offset, color);
    style->setGradientStopsSet(true);
    return true;
}

static bool parseStyleNode(std::shared_ptr<LSvgNode> parent,
                           LXmlDoc::const_iterator &attributes,
                           LSvgHandler *handler)
{
    const std::string_view type = attributes.attribute("type");
    if (LStringUtil::compareIgnoreCase(std::string(type), std::string("text/css")) == 0 || type.empty()){
        handler->setInStyle(true);
    }
    return true;
}

static std::shared_ptr<LSvgNode> createSvgNode(std::shared_ptr<LSvgNode> parent,
                               LXmlDoc::const_iterator &attributes,
                               LSvgHandler *handler)
{
    std::shared_ptr<LSvgDocument> node = std::make_shared<LSvgDocument>(handler->options(), handler->animatorType());
    const std::string_view widthStr  = attributes.attribute("width");
    const std::string_view heightStr = attributes.attribute("height");
    std::string viewBoxStr = std::string(attributes.attribute("viewBox"));

    LSvgLengthType type = LSvgLengthType::px;
    double width = 0;
    if (!widthStr.empty()) {
        width = parseLength(widthStr, &type);
        if (type != LSvgLengthType::pt){
            width = convertToPixels(width, true, type);
        }
        node->setWidth(int(width), type == LSvgLengthType::percent);
    }
    double height = 0;
    if (!heightStr.empty()) {
        height = parseLength(heightStr, &type);
        if (type != LSvgLengthType::pt){
            height = convertToPixels(height, false, type);
        }
        node->setHeight(int(height), type == LSvgLengthType::percent);
    }

    std::vector<std::string> viewBoxValues;
    if (!viewBoxStr.empty()) {
        viewBoxValues = LStringUtil::splitToVec(viewBoxStr, "[ ,\t\r\n]+", true);
    }
    if (viewBoxValues.size() == 4) {
        std::string xStr      = LStringUtil::trim(viewBoxValues[0]);
        std::string yStr      = LStringUtil::trim(viewBoxValues[1]);
        std::string widthStr  = LStringUtil::trim(viewBoxValues[2]);
        std::string heightStr = LStringUtil::trim(viewBoxValues[3]);

        LSvgLengthType lt;
        double x = parseLength(xStr, &lt);
        double y = parseLength(yStr, &lt);
        double w = parseLength(widthStr, &lt);
        double h = parseLength(heightStr, &lt);

        node->setViewBox(LRect<double>(x, y, w, h));

    } else if (width && height) {
        if (type == LSvgLengthType::pt) {
            width = convertToPixels(width, false, type);
            height = convertToPixels(height, false, type);
        }
        node->setViewBox(LRect<double>(0, 0, width, height));
    }
    handler->setDefaultCoordinateSystem(LSvgLengthType::px);
    return node;
}

static std::shared_ptr<LSvgNode> createSwitchNode(std::shared_ptr<LSvgNode> parent,
                                  LXmlDoc::const_iterator &attributes,
                                  LSvgHandler *)
{
    return std::make_shared<LSvgSwitch>(parent);
}

static std::shared_ptr<LSvgNode> createPatternNode(std::shared_ptr<LSvgNode> parent,
                                   LXmlDoc::const_iterator &attributes,
                                   LSvgHandler *handler)
{
    const std::string_view x      = attributes.attribute("x");
    const std::string_view y      = attributes.attribute("y");
    const std::string_view width  = attributes.attribute("width");
    const std::string_view height = attributes.attribute("height");
    const std::string_view patternUnits     = attributes.attribute("patternUnits");
    const std::string_view patternContentUnits    = attributes.attribute("patternContentUnits");
    const std::string_view patternTransform = attributes.attribute("patternTransform");

    LSvgUnitTypes nPatternUnits = patternUnits.contains(std::string("userSpaceOnUse")) ?
                                         LSvgUnitTypes::userSpaceOnUse : LSvgUnitTypes::objectBoundingBox;

    LSvgUnitTypes nPatternContentUnits = patternContentUnits.contains(std::string("objectBoundingBox")) ?
                                                LSvgUnitTypes::objectBoundingBox : LSvgUnitTypes::userSpaceOnUse;

    std::string viewBoxStr = std::string(attributes.attribute("viewBox"));

    bool ok = false;
    LSvgLengthType type;

    double nx = parseLength(x, &type, &ok);
    nx = convertToPixels(nx, true, type);
    if (!ok){
        nx = 0.0;
    }else if (type == LSvgLengthType::percent && nPatternUnits == LSvgUnitTypes::userSpaceOnUse){
        nx = (nx / 100.) * handler->document()->viewBox().width();
    }else if (type == LSvgLengthType::percent){
        nx = nx / 100.;
    }

    double ny = parseLength(y, &type, &ok);
    ny = convertToPixels(ny, true, type);
    if (!ok){
        ny = 0.0;
    }else if (type == LSvgLengthType::percent && nPatternUnits == LSvgUnitTypes::userSpaceOnUse){
        ny = (ny / 100.) * handler->document()->viewBox().height();
    }else if (type == LSvgLengthType::percent){
        ny = ny / 100.;
    }

    double nwidth = parseLength(width, &type, &ok);
    nwidth = convertToPixels(nwidth, true, type);
    if (!ok){
        nwidth = 0.0;
    }else if (type == LSvgLengthType::percent && nPatternUnits == LSvgUnitTypes::userSpaceOnUse){
        nwidth = (nwidth / 100.) * handler->document()->viewBox().width();
    }else if (type == LSvgLengthType::percent){
        nwidth = nwidth / 100.;
    }

    double nheight = parseLength(height, &type, &ok);
    nheight = convertToPixels(nheight, true, type);
    if (!ok){
        nheight = 0.0;
    }else if (type == LSvgLengthType::percent && nPatternUnits == LSvgUnitTypes::userSpaceOnUse){
        nheight = (nheight / 100.) * handler->document()->viewBox().height();
    }else if (type == LSvgLengthType::percent){
        nheight = nheight / 100.;
    }


    std::vector<std::string> viewBoxValues;
    LRect<double> viewBox;
    if (!viewBoxStr.empty()) {
        viewBoxValues = LStringUtil::splitToVec(viewBoxStr, "[ ,\t\r\n]+", true);
    }
    if (viewBoxValues.size() == 4) {
        std::string xStr      = LStringUtil::trim(viewBoxValues[0]);
        std::string yStr      = LStringUtil::trim(viewBoxValues[1]);
        std::string widthStr  = LStringUtil::trim(viewBoxValues[2]);
        std::string heightStr = LStringUtil::trim(viewBoxValues[3]);

        double x = convertToNumber(xStr);
        double y = convertToNumber(yStr);
        double w = convertToNumber(widthStr);
        double h = convertToNumber(heightStr);

        if (w > 0 && h > 0){
            viewBox.setRect(x, y, w, h);
        }
    }

    LTransform2D matrix;
    if (!patternTransform.empty()){
        matrix = parseTransformationMatrix(patternTransform);
    }

    LRect<double> bounds(nx, ny, nwidth, nheight);
    if (bounds.isEmpty()){
        return nullptr;
    }

    LSvgRectInfo patternRectF(bounds, nPatternUnits, nPatternUnits, nPatternUnits, nPatternUnits);
    std::shared_ptr<LSvgPattern> node = std::make_shared<LSvgPattern>(parent, patternRectF, viewBox, nPatternContentUnits, matrix);

    // Create a style node for the Pattern.
    std::shared_ptr<LSvgPatternStyle> prop = std::make_shared<LSvgPatternStyle>(node);
    node->appendStyleProperty(prop, someId(attributes));

    return node;
}

static bool parseTbreakNode(std::shared_ptr<LSvgNode> parent,
                            LXmlDoc::const_iterator &,
                            LSvgHandler *)
{
    if (parent->type() != LSvgNode::Textarea){
        return false;
    }
    std::shared_ptr<LSvgText> t= std::dynamic_pointer_cast<LSvgText>(parent);
    t->addLineBreak();
    return true;
}

static std::shared_ptr<LSvgNode> createTextNode(std::shared_ptr<LSvgNode> parent,
                                LXmlDoc::const_iterator &attributes,
                                LSvgHandler *)
{
    const std::string_view x = attributes.attribute("x");
    const std::string_view y = attributes.attribute("y");
    //### editable and rotate not handled
    LSvgLengthType type;
    double nx = parseLength(x, &type);
    nx = convertToPixels(nx, true, type);
    double ny = parseLength(y, &type);
    ny = convertToPixels(ny, true, type);

    return std::make_shared<LSvgText>(parent, LPoint<double>(nx, ny));
}

static std::shared_ptr<LSvgNode> createTextAreaNode(std::shared_ptr<LSvgNode> parent,
                                    LXmlDoc::const_iterator &attributes,
                                    LSvgHandler *handler)
{
     std::shared_ptr<LSvgText> node = std::dynamic_pointer_cast<LSvgText>(createTextNode(parent, attributes, handler));
    if (node != nullptr) {
        LSvgLengthType type;
        double width = parseLength(attributes.attribute("width"), &type);
        double height = parseLength(attributes.attribute("height"), &type);
        node->setTextArea(LSize<double>(width, height));
    }
    return node;
}

static std::shared_ptr<LSvgNode> createTspanNode(std::shared_ptr<LSvgNode> parent,
                                 LXmlDoc::const_iterator &,
                                 LSvgHandler *)
{
    return std::make_shared<LSvgTspan>(parent);
}

static std::shared_ptr<LSvgNode> createUseNode(std::shared_ptr<LSvgNode> parent,
                               LXmlDoc::const_iterator &attributes,
                               LSvgHandler *)
{
    std::string linkId = std::string(attributes.attribute("xlink:href")).substr(1);
    const std::string_view xStr = attributes.attribute("x");
    const std::string_view yStr = attributes.attribute("y");
    std::shared_ptr<LSvgStructure> group = nullptr;

    if (linkId.empty()){
        linkId = std::string(attributes.attribute("href")).substr(0, 1);
    }
    switch (parent->type()) {
    case LSvgNode::Doc:
    case LSvgNode::Defs:
    case LSvgNode::Group:
    case LSvgNode::Switch:
    case LSvgNode::Mask:{
        group = std::dynamic_pointer_cast<LSvgStructure>(parent);
        break;
    }
    default:{
        break;
    }
    }

    if (group != nullptr) {
        LPoint<double> pt;
        if (!xStr.empty() || !yStr.empty()) {
            LSvgLengthType type;
            double nx = parseLength(xStr, &type);
            nx = convertToPixels(nx, true, type);

            double ny = parseLength(yStr, &type);
            ny = convertToPixels(ny, true, type);
            pt = LPoint<double>(nx, ny);
        }

        std::shared_ptr<LSvgNode> link = group->scopeNode(linkId);
        if (link) {
            if (parent->isDescendantOf(link)){
                lWarn() << "link "<< linkId << " is recursive!";
            }

            return std::make_shared<LSvgUse>(pt, parent, link);
        }

        //delay link resolving, link might have not been created yet
        return std::make_shared<LSvgUse>(pt, parent, linkId);
    }
    return nullptr;
}

static std::shared_ptr<LSvgNode> createVideoNode(std::shared_ptr<LSvgNode> parent,
                                 LXmlDoc::const_iterator &attributes,
                                 LSvgHandler *)
{
    return nullptr;
}

typedef std::shared_ptr<LSvgNode> (*FactoryMethod)(std::shared_ptr<LSvgNode> , LXmlDoc::const_iterator &, LSvgHandler *);

static FactoryMethod findGroupFactory(const std::string &name, LFlags<LSvgOption> options)
{
    if (name.empty())
        return 0;

    std::string_view ref = std::string_view{name}.substr(1, name.size() - 1);
    switch (name.at(0)) {
    case 'd':
        if (ref == std::string("efs")) return createDefsNode;
        break;
    case 'f':
        if (ref == std::string("ilter") && !options.testFlag(LSvgOption::Tiny12FeaturesOnly)) return createFilterNode;
        break;
    case 'g':
        if (ref.empty()) return createGNode;
        break;
    case 'm':
        if (ref == std::string("ask") && !options.testFlag(LSvgOption::Tiny12FeaturesOnly)) return createMaskNode;
        if (ref == std::string("arker") && !options.testFlag(LSvgOption::Tiny12FeaturesOnly)) return createMarkerNode;
        break;
    case 's':
        if (ref == std::string("vg")) return createSvgNode;
        if (ref == std::string("witch")) return createSwitchNode;
        if (ref == std::string("ymbol") && !options.testFlag(LSvgOption::Tiny12FeaturesOnly)) return createSymbolNode;
        break;
    case 'p':
        if (ref == std::string("attern") && !options.testFlag(LSvgOption::Tiny12FeaturesOnly)) return createPatternNode;
        break;
    default:
        break;
    }
    return 0;
}

static FactoryMethod findGraphicsFactory(const std::string &name, LFlags<LSvgOption> options)
{
    if (name.empty()){
        return 0;
    }

    std::string_view ref = std::string_view{name}.substr(1, name.size() - 1);
    switch (name.at(0)) {
    case 'c':
        if (ref == std::string("ircle")) return createCircleNode;
        break;
    case 'e':
        if (ref == std::string("llipse")) return createEllipseNode;
        break;
    case 'i':
        if (ref == std::string("mage")) return createImageNode;
        break;
    case 'l':
        if (ref == std::string("ine")) return createLineNode;
        break;
    case 'p':
        if (ref == std::string("ath")) return createPathNode;
        if (ref == std::string("olygon")) return createPolygonNode;
        if (ref == std::string("olyline")) return createPolylineNode;
        break;
    case 'r':
        if (ref == std::string("ect")) return createRectNode;
        break;
    case 't':
        if (ref == std::string("ext")) return createTextNode;
        if (ref == std::string("extArea")) return createTextAreaNode;
        if (ref == std::string("span")) return createTspanNode;
        break;
    case 'u':
        if (ref == std::string("se")) return createUseNode;
        break;
    case 'v':
        if (ref == std::string("ideo")) return createVideoNode;
        break;
    default:
        break;
    }
    return 0;
}

static FactoryMethod findFilterFactory(const std::string &name, LFlags<LSvgOption> options)
{
    if (options.testFlag(LSvgOption::Tiny12FeaturesOnly))
        return 0;

    if (name.empty())
        return 0;

    if (!name.starts_with(std::string("fe")))
        return 0;

    if (name == std::string("feMerge")) return createFeMergeNode;
    if (name == std::string("feColorMatrix")) return createFeColorMatrixNode;
    if (name == std::string("feGaussianBlur")) return createFeGaussianBlurNode;
    if (name == std::string("feOffset")) return createFeOffsetNode;
    if (name == std::string("feMergeNode")) return createFeMergeNodeNode;
    if (name == std::string("feComposite")) return createFeCompositeNode;
    if (name == std::string("feFlood")) return createFeFloodNode;
    if (name == std::string("feBlend")) return createFeBlendNode;

    static const std::set<std::string> unsupportedFilters = {
        ("feComponentTransfer"),
        ("feConvolveMatrix"),
        ("feDiffuseLighting"),
        ("feDisplacementMap"),
        ("feDropShadow"),
        ("feFuncA"),
        ("feFuncB"),
        ("feFuncG"),
        ("feFuncR"),
        ("feImage"),
        ("feMorphology"),
        ("feSpecularLighting"),
        ("feTile"),
        ("feTurbulence")
    };

    if (unsupportedFilters.contains(name)){
        return createFeUnsupportedNode;
    }
    return 0;
}

typedef std::shared_ptr<LSvgNode> (*AnimationMethod)(std::shared_ptr<LSvgNode> , LXmlDoc::const_iterator &, LSvgHandler *);

static AnimationMethod findAnimationFactory(const std::string &name, LFlags<LSvgOption> options)
{
    if (name.empty() || options.testFlag(LSvgOption::DisableSMILAnimations)){
        return nullptr;
    }

    std::string_view ref = std::string_view{name}.substr(1, name.size() - 1);

    switch (name.at(0)) {
    case 'a':{
        if (ref == std::string("nimate")) return createAnimateNode;
        if (ref == std::string("nimateColor")) return createAnimateColorNode;
        if (ref == std::string("nimateMotion")) return createAimateMotionNode;
        if (ref == std::string("nimateTransform")) return createAnimateTransformNode;
        break;
    }
    default:{
        break;
    }
    }

    return nullptr;
}

typedef bool (*ParseMethod)(std::shared_ptr<LSvgNode> , LXmlDoc::const_iterator &, LSvgHandler *);

static ParseMethod findUtilFactory(const std::string &name, LFlags<LSvgOption> options)
{
    if (name.empty()){
        return nullptr;
    }

    std::string_view ref = std::string_view{name}.substr(1, name.size() - 1);
    switch (name.at(0)) {
    case 'a':
        if (ref.empty()) return parseAnchorNode;
        if (ref == std::string("udio")) return parseAudioNode;
        break;
    case 'd':
        if (ref == std::string("iscard")) return parseDiscardNode;
        break;
    case 'f':
        if (ref == std::string("oreignObject")) return parseForeignObjectNode;
        break;
    case 'h':
        if (ref == std::string("andler")) return parseHandlerNode;
        if (ref == std::string("kern")) return parseHkernNode;
        break;
    case 'm':
        if (ref == std::string("etadata")) return parseMetadataNode;
        if (ref == std::string("path")) return parseMpathNode;
        if (ref == std::string("ask") && !options.testFlag(LSvgOption::Tiny12FeaturesOnly)) return parseMaskNode;
        if (ref == std::string("arker") && !options.testFlag(LSvgOption::Tiny12FeaturesOnly)) return parseMarkerNode;
        break;
    case 'p':
        if (ref == std::string("refetch")) return parsePrefetchNode;
        break;
    case 's':
        if (ref == std::string("cript")) return parseScriptNode;
        if (ref == std::string("et")) return parseSetNode;
        if (ref == std::string("tyle")) return parseStyleNode;
        break;
    case 't':
        if (ref == std::string("break")) return parseTbreakNode;
        break;
    default:
        break;
    }
    return nullptr;
}

typedef std::shared_ptr<LSvgStyleProperty> (*StyleFactoryMethod)(std::shared_ptr<LSvgNode> ,
                                                 LXmlDoc::const_iterator &,
                                                 LSvgHandler *);

static StyleFactoryMethod findStyleFactoryMethod(const std::string &name)
{
    if (name.empty()){
        return nullptr;
    }

    std::string_view ref = std::string_view{name}.substr(1, name.size() - 1);
    switch (name.at(0)) {
    case 'f':
        if (ref == std::string("ont")) return createFontNode;
        break;
    case 'l':
        if (ref == std::string("inearGradient")) return createLinearGradientNode;
        break;
    case 'r':
        if (ref == std::string("adialGradient")) return createRadialGradientNode;
        break;
    case 's':
        if (ref == std::string("olidColor")) return createSolidColorNode;
        break;
    default:
        break;
    }
    return 0;
}

typedef bool (*StyleParseMethod)(std::shared_ptr<LSvgStyleProperty> ,
                                 LXmlDoc::const_iterator &,
                                 LSvgHandler *);

static StyleParseMethod findStyleUtilFactoryMethod(const std::string &name)
{
    if (name.empty()){
        return nullptr;
    }

    std::string_view ref = std::string_view{name}.substr(1, name.size() - 1);
    switch (name.at(0)) {
    case 'f':{
        if (ref == std::string("ont-face")) return parseFontFaceNode;
        if (ref == std::string("ont-face-name")) return parseFontFaceNameNode;
        if (ref == std::string("ont-face-src")) return parseFontFaceSrcNode;
        if (ref == std::string("ont-face-uri")) return parseFontFaceUriNode;
        break;
    }
    case 'g':{
        if (ref == std::string("lyph")) return parseGlyphNode;
        break;
    }
    case 'm':{
        if (ref == std::string("issing-glyph")) return parseMissingGlyphNode;
        break;
    }
    case 's':{
        if (ref == std::string("top")) return parseStopNode;
        break;
    }
    default:{
        break;
    }
    }
    return nullptr;
}

LSvgHandler::LSvgHandler(const std::string &content, LFlags<LSvgOption> options, LSvgAnimatorType type)
    : m_options(options)
    , m_animatorType(type)
{

    LXmlParser parser(content);
    m_xmlRetSegment = parser.to_tree(m_xmlTree, true);
    if(m_xmlRetSegment){
    }else{
        lWarn() << "Svg xml parse failed:" << m_xmlRetSegment.error();
    }
    init();
}


void LSvgHandler::init()
{
    m_doc = nullptr;
    m_style = 0;
    m_animEnd = 0;
    m_defaultCoords = LSvgLengthType::px;
    m_defaultPen = std::make_shared<LPen>(LColorID::Black, 1, LPenStyle::Solid, LPenCapStyle::Flat, LPenJoinStyle::SvgMiter);
    m_defaultPen->m_miterLimit = 4;
    parse();
}

static bool detectPatternCycles(const std::shared_ptr<LSvgNode> node, std::list<std::shared_ptr<LSvgNode> > active = {})
{
    std::shared_ptr<LSvgFillStyle> fillStyle = std::dynamic_pointer_cast<LSvgFillStyle>(node->styleProperty(LSvgStyleProperty::FILL));
    if ((fillStyle!= nullptr) && fillStyle->style() && fillStyle->style()->type() == LSvgStyleProperty::PATTERN) {
        std::shared_ptr<LSvgPatternStyle> patternStyle = std::dynamic_pointer_cast<LSvgPatternStyle>(fillStyle->style());
        auto it = std::find (active.begin (), active.end (), patternStyle->patternNode());
        if (it != active.end ()){
            return true;
        }
    }

    std::shared_ptr<LSvgStrokeStyle> strokeStyle = std::dynamic_pointer_cast<LSvgStrokeStyle>(node->styleProperty(LSvgStyleProperty::STROKE));
    if (strokeStyle && strokeStyle->style() && strokeStyle->style()->type() == LSvgStyleProperty::PATTERN) {
        std::shared_ptr<LSvgPatternStyle> patternStyle = std::dynamic_pointer_cast<LSvgPatternStyle>(strokeStyle->style());
        auto it = std::find (active.begin (), active.end (), patternStyle->patternNode());
        if (it != active.end ()){
            return true;
        }
    }
    return false;
}

static bool detectCycles(const std::shared_ptr<LSvgNode> node, std::list<std::shared_ptr<LSvgNode> > active = {})
{
    if (L_UNLIKELY(!node)){
        return false;
    }
    switch (node->type()) {
    case LSvgNode::Doc:
    case LSvgNode::Group:
    case LSvgNode::Defs:
    case LSvgNode::Pattern:
    {
        if (node->type() == LSvgNode::Pattern){
            active.push_back(node);
        }

        std::shared_ptr<LSvgStructure> g = std::dynamic_pointer_cast<LSvgStructure>(node);
        for (const std::shared_ptr<LSvgNode>& r : g->renderers()) {
            if (detectCycles(r, active)){
                return true;
            }
        }
    }
    break;
    case LSvgNode::Use:
    {
        auto it = std::find (active.begin (), active.end (), node);
        if (it != active.end ()){
            return true;
        }

        std::shared_ptr<LSvgUse> u = std::dynamic_pointer_cast<LSvgUse>(node);
        std::shared_ptr<LSvgNode> target = u->link();
        if (target) {
            active.push_back(u);
            if (detectCycles(target, active)){
                return true;
            }
        }
    }
    break;
    case LSvgNode::Rect:
    case LSvgNode::Ellipse:
    case LSvgNode::Circle:
    case LSvgNode::Line:
    case LSvgNode::Path:
    case LSvgNode::Polygon:
    case LSvgNode::Polyline:
    case LSvgNode::Tspan:
        if (detectPatternCycles(node, active))
            return true;
        break;
    default:
        break;
    }
    return false;
}

static bool detectCyclesAndWarn(const std::shared_ptr<LSvgNode> node) {
    const bool cycleFound = detectCycles(node);
    if (cycleFound){
        lWarn() << "Cycles detected in SVG, document discarded.";
    }
    return cycleFound;
}

void LSvgHandler::parse()
{
    m_selector = std::make_shared<LSvgStyleSelector>();
    m_inStyle = false;

    LXmlDoc::const_iterator it = m_xmlTree.cbegin();
    int level = 0;
    bool done = false;
    while (!it.atEnd() && !done) {
        switch(it->category()){
        case xxmlxx::tree_node_category::element:{
            if (startElement(std::string(it->name()), it)
                && !detectCyclesAndWarn(m_doc)) {
            }else{
                m_doc = nullptr;
                return;
            }
            break;
        }
        case xxmlxx::tree_node_category::text:{
            characters(it->text());
            break;
        }
        case xxmlxx::tree_node_category::instruction:{
            processingInstruction(std::string(it->name()), it);
            break;
        }
        default:{
            break;
        }
        }

        LXmlDoc::const_iterator ch = it.dive();
        if(!ch.atEnd()){
            it = ch;
            level ++;
            continue;
        }

        while(true){
            if(it->category() == xxmlxx::tree_node_category::element){
                done = endElement(it->name());
            }
            LXmlDoc::const_iterator ch = it.next();
            if(!ch.atEnd()){
                it = ch;
                break;
            }else{
                if(level > 0){
                    it = it.rise();
                    level--;
                }else{
                    it = ch;
                    break;
                }
            }
        }
    }

    resolvePaintServers(m_doc);
    resolveNodes();
    if (detectCyclesAndWarn(m_doc)) {
        m_doc = nullptr;
    }
}

bool LSvgHandler::startElement(const std::string &localName, LXmlDoc::const_iterator attributes)
{
    std::shared_ptr<LSvgNode> node = nullptr;

    pushColorCopy();

    /* The xml:space attribute may appear on any element. We do
     * a lookup by the qualified name here, but this is namespace aware, since
     * the XML namespace can only be bound to prefix "xml." */
    //xmlNode.


    const std::string_view xmlSpace(attributes.attribute("xml:space"));
    if (xmlSpace.empty()) {
        // This element has no xml:space attribute.
        m_whitespaceMode.push(m_whitespaceMode.empty() ? LSvgText::Default : m_whitespaceMode.top());
    } else if (xmlSpace == std::string("preserve")) {
        m_whitespaceMode.push(LSvgText::Preserve);
    } else if (xmlSpace == std::string("default")) {
        m_whitespaceMode.push(LSvgText::Default);
    } else {
        lWarn() <<  '"' + std::string(xmlSpace) +  "\" is an invalid value for attribute xml:space. " + "Valid values are \"preserve\" and \"default\".";
        m_whitespaceMode.push(LSvgText::Default);
    }

    if ((m_doc == nullptr) && localName != std::string("svg")){
        return false;
    }

    if ((m_doc!=nullptr) && localName == std::string("svg")) {
        m_skipNodes.push(Doc);
        lWarn() << "Skipping a nested svg element, because SVG Document must not contain nested svg elements in Svg Tiny 1.2";
    }

    if (!m_skipNodes.empty() && m_skipNodes.top() == Doc){
        return true;
    }

    if (FactoryMethod method = findGroupFactory(localName, options())) {
        //group
        if (m_doc == nullptr) {
            node = method(nullptr, attributes, this);
            if (node != nullptr) {
                assert(node->type() == LSvgNode::Doc);
                m_doc = std::dynamic_pointer_cast<LSvgDocument>(node);
            }
        } else {
            switch (m_nodes.top()->type()) {
            case LSvgNode::Doc:
            case LSvgNode::Group:
            case LSvgNode::Defs:
            case LSvgNode::Switch:
            case LSvgNode::Mask:
            case LSvgNode::Symbol:
            case LSvgNode::Marker:
            case LSvgNode::Pattern:
            {
                node = method(m_nodes.top(), attributes, this);
                if(node != nullptr){
                    std::shared_ptr<LSvgStructure> group = std::dynamic_pointer_cast<LSvgStructure>(m_nodes.top());
                    group->addChild(node, someId(attributes));
                }
                break;
            }
            default:{
                lWarn() << "Could not add child element to parent element because the types are incorrect.";
                node = nullptr;
                break;
            }
            }
        }

        if (node) {
            parseCoreNode(node, attributes);
            cssStyleLookup(node, this, m_selector);
            parseStyle(node, *attributes, this);
            if (node->type() == LSvgNode::Filter){
                m_toBeResolved.push_back(node);
            }
        }
    } else if (FactoryMethod method = findGraphicsFactory(localName, options())) {
        //rendering element
        if(m_nodes.empty()){
            lWarn() << "No stack node when parse findGraphicsFactory node:" << localName;
            return false;
        }
        node = method(m_nodes.top(), attributes, this);
        if (node != nullptr) {
            switch (m_nodes.top()->type()) {
            case LSvgNode::Doc:
            case LSvgNode::Group:
            case LSvgNode::Defs:
            case LSvgNode::Switch:
            case LSvgNode::Mask:
            case LSvgNode::Symbol:
            case LSvgNode::Marker:
            case LSvgNode::Pattern:
            {
                if (node->type() == LSvgNode::Tspan) {
                    lWarn() << std::string("\'tspan\' element in wrong context.");
                    node = nullptr;
                    break;
                }
                std::shared_ptr<LSvgStructure> group = std::dynamic_pointer_cast<LSvgStructure>(m_nodes.top());
                if(group != nullptr){
                    group->addChild(node, someId(attributes));
                }
                break;
            }

            case LSvgNode::Text:
            case LSvgNode::Textarea:{
                if (node->type() == LSvgNode::Tspan) {
                    std::shared_ptr<LSvgText> t = std::dynamic_pointer_cast<LSvgText>(m_nodes.top());
                    if(t != nullptr){
                        std::shared_ptr<LSvgTspan> ts = std::dynamic_pointer_cast<LSvgTspan>(node);
                        if(ts != nullptr){
                            t->addTspan(ts);
                        }
                    }
                } else {
                    lWarn() << std::string("\'text\' or \'textArea\' element contains invalid element type.");
                    node = nullptr;
                }
                break;
            }
            default:{
                lWarn() << std::string("Could not add child element to parent element because the types are incorrect.");
                node = nullptr;
                break;
            }
            }

            if (node) {
                parseCoreNode(node, attributes);
                cssStyleLookup(node, this, m_selector);
                parseStyle(node, *attributes, this);
                if (node->type() == LSvgNode::Text || node->type() == LSvgNode::Textarea) {
                    std::shared_ptr<LSvgText> t = std::dynamic_pointer_cast<LSvgText>(node);
                    if(t != nullptr){
                        t->setWhitespaceMode(m_whitespaceMode.top());
                    }
                } else if (node->type() == LSvgNode::Tspan) {
                    std::shared_ptr<LSvgTspan> t = std::dynamic_pointer_cast<LSvgTspan>(node);
                    if(t != nullptr){
                        t->setWhitespaceMode(m_whitespaceMode.top());
                    }
                } else if (node->type() == LSvgNode::Use) {
                    auto useNode = std::dynamic_pointer_cast<LSvgUse>(node);
                    if (!useNode->isResolved()){
                        m_toBeResolved.push_back(useNode);
                    }
                }
            }
        }
    } else if (FactoryMethod method = findFilterFactory(localName, options())) {
        //filter nodes to be aded to be filtercontainer
        if(m_nodes.empty()){
            lWarn() << "No stack node when parse findFilterFactory node:" << localName;
            return false;
        }
        node = method(m_nodes.top(), attributes, this);
        if (node) {
            if (m_nodes.top()->type() == LSvgNode::Filter ||
                (m_nodes.top()->type() == LSvgNode::FeMerge && node->type() == LSvgNode::FeMergenode)) {
                std::shared_ptr<LSvgStructure> container = std::dynamic_pointer_cast<LSvgStructure>(m_nodes.top());
                if(container != nullptr){
                    container->addChild(node, someId(attributes));
                }
            } else {
                lWarn() << std::string("Could not add child element to parent element because the types are incorrect.");
                node = nullptr;
            }
        }
    } else if (AnimationMethod method = findAnimationFactory(localName, options())) {
        if(m_nodes.empty()){
            lWarn() << "No stack node when parse findAnimationFactory node:" << localName;
            return false;
        }
        node = method(m_nodes.top(), attributes, this);
        if (node) {
            std::shared_ptr<LSvgAnimateNode> anim = std::dynamic_pointer_cast<LSvgAnimateNode>(node);
            if(anim != nullptr){
                if (anim->linkId().empty()){
                    m_doc->animator()->appendAnimation(m_nodes.top(), anim);
                }else if (m_doc->namedNode(anim->linkId())){
                    m_doc->animator()->appendAnimation(m_doc->namedNode(anim->linkId()), anim);
                }else{
                    m_toBeResolved.push_back(anim);
                }
            }
        }
    } else if (ParseMethod method = findUtilFactory(localName, options())) {
        //assert(!m_nodes.empty());
        if(m_nodes.empty()){
            lWarn() << "No stack node when parse findUtilFactory node:" << localName;
            return false;
        }
        if (!method(m_nodes.top(), attributes, this)){
            lWarn() << "Could not parse node:" << localName;
        }
    } else if (StyleFactoryMethod method = findStyleFactoryMethod(localName)) {
        std::shared_ptr<LSvgStyleProperty> prop = method(m_nodes.top(), attributes, this);
        if (prop) {
            m_style = prop;
            m_nodes.top()->appendStyleProperty(prop, someId(attributes));
        } else {
            lWarn() << "Could not parse node:" << localName;
        }
    } else if (StyleParseMethod method = findStyleUtilFactoryMethod(localName)) {
        if (m_style) {
            if (!method(m_style, attributes, this)){
                lWarn() << "Could not parse node:" << localName;
            }
        }
    } else {
        lWarn() << "Skipping unknown element" << localName;
        m_skipNodes.push(Unknown);
        return true;
    }

    if (node) {
        m_nodes.push(node);
        m_skipNodes.push(Graphics);
    } else {
        m_skipNodes.push(Style);
    }
    return true;
}

bool LSvgHandler::endElement(const std::string_view localName)
{
    CurrentNode node = m_skipNodes.top();

    if (node == Doc && localName != std::string("svg")){
        return false;
    }

    m_skipNodes.pop();
    m_whitespaceMode.pop();

    popColor();

    if (node == Unknown){
        return false;
    }


    if (m_inStyle && localName == std::string("style")){
        m_inStyle = false;
    }

    if (node == Graphics){
        m_nodes.pop();
    }else if(node == Style){
    }else if (m_style && !m_skipNodes.empty() && m_skipNodes.top() != Style){
        m_style = 0;
    }

    return ((localName == std::string("svg")) && (node != Doc));
}

void LSvgHandler::resolvePaintServers(std::shared_ptr<LSvgNode> node, int nestedDepth)
{
    if (!node || (node->type() != LSvgNode::Doc && node->type() != LSvgNode::Group
                  && node->type() != LSvgNode::Defs && node->type() != LSvgNode::Switch)) {
        return;
    }

    std::shared_ptr<LSvgStructure> structureNode = std::dynamic_pointer_cast<LSvgStructure>(node);

    const std::list<std::shared_ptr<LSvgNode> > ren = structureNode->renderers();
    for (auto it = ren.begin(); it != ren.end(); ++it) {
        std::shared_ptr<LSvgFillStyle> fill = std::dynamic_pointer_cast<LSvgFillStyle>((*it)->styleProperty(LSvgStyleProperty::FILL));
        if (fill && !fill->isPaintStyleResolved()) {
            std::string id = fill->paintStyleId();
            std::shared_ptr<LSvgPaintStyleProperty> style = structureNode->styleProperty(id);
            if (style != nullptr) {
                fill->setFillStyle(style);
            } else {
                lWarn() << "Could not resolve:" << id;
                fill->setBrush(nullptr);
            }
        }

        std::shared_ptr<LSvgStrokeStyle> stroke = std::dynamic_pointer_cast<LSvgStrokeStyle>((*it)->styleProperty(LSvgStyleProperty::STROKE));
        if (stroke && !stroke->isPaintStyleResolved()) {
            std::string id = stroke->paintStyleId();
            std::shared_ptr<LSvgPaintStyleProperty> style = structureNode->styleProperty(id);
            if (style) {
                stroke->setStyle(style);
            } else {
                lWarn() << "Could not resolve:" << id;
                stroke->setStroke(nullptr);
            }
        }

        if (nestedDepth < 2048)
            resolvePaintServers(*it, nestedDepth + 1);
    }
}

void LSvgHandler::resolveNodes()
{
    for (const std::shared_ptr<LSvgNode>& node : std::as_const(m_toBeResolved)) {
        if (node->type() == LSvgNode::Use) {
            std::shared_ptr<LSvgUse> useNode = std::dynamic_pointer_cast<LSvgUse>(node);
            if(useNode == nullptr){
                continue;
            }
            const auto parent = useNode->parent();
            if (parent == nullptr){
                continue;
            }

            LSvgNode::Type t = parent->type();
            if (t != LSvgNode::Doc && t != LSvgNode::Defs && t != LSvgNode::Group && t != LSvgNode::Switch){
                continue;
            }

            std::shared_ptr<LSvgStructure> group = std::dynamic_pointer_cast<LSvgStructure>(parent);
            std::shared_ptr<LSvgNode> link = group->scopeNode(useNode->linkId());
            if (link == nullptr) {
                lWarn() <<  "LSvgHandler link "<< useNode->linkId() << " is undefined!";
                continue;
            }

            if (useNode->parent()->isDescendantOf(link)){
                lWarn() <<  "LSvgHandler link "<< useNode->linkId() << " is recursive!";
            }
            useNode->setLink(link);
        } else if (node->type() == LSvgNode::Filter) {
            std::shared_ptr<LSvgFilterContainer> filter = std::dynamic_pointer_cast<LSvgFilterContainer>(node);
            for (const std::shared_ptr<LSvgNode>& renderer : filter->renderers()) {
                std::shared_ptr<LSvgFeFilterPrimitive> primitive = LSvgFeFilterPrimitive::castToFilterPrimitive(renderer);
                if (!primitive || primitive->type() == LSvgNode::FeUnsupported) {
                    filter->setSupported(false);
                    break;
                }
            }
        } else if (node->type() == LSvgNode::AnimateTransform || node->type() == LSvgNode::AnimateColor) {
            std::shared_ptr<LSvgAnimateNode> anim = std::dynamic_pointer_cast<LSvgAnimateNode>(node);
            if(anim != nullptr){
                std::shared_ptr<LSvgNode> targetNode = m_doc->namedNode(anim->linkId());
                if (targetNode != nullptr){
                    m_doc->animator()->appendAnimation(targetNode, anim);
                }
            }
        }
    }
    m_toBeResolved.clear();
}

bool LSvgHandler::characters(const std::string_view str)
{
    if (m_inStyle) {
        std::shared_ptr<LSvgNode> top = m_nodes.empty()? nullptr : m_nodes.top();
        std::string css = std::string(str);
        std::shared_ptr<LSvgStyleSheet> sheet = std::make_shared<LSvgStyleSheet>(top);
        LCssParser::parseStyleSheet(css, sheet);
        m_selector->m_styleSheets.push_back(sheet);
        m_cssHandler.collectAnimations(sheet);
        return true;
    }

    if (m_skipNodes.empty() || m_skipNodes.top() == Unknown || m_nodes.empty()){
        return true;
    }

    std::shared_ptr<LSvgNode> top = m_nodes.empty()? nullptr : m_nodes.top();
    if(top != nullptr){
        if (top->type() == LSvgNode::Text || top->type() == LSvgNode::Textarea) {
            std::shared_ptr<LSvgText> t = std::dynamic_pointer_cast<LSvgText>(top);
            if(t!= nullptr){
                t->addText(std::string(str));
            }
        } else if (m_nodes.top()->type() == LSvgNode::Tspan) {
            std::shared_ptr<LSvgTspan> t = std::dynamic_pointer_cast<LSvgTspan>(top);
            if(t!= nullptr){
                t->addText(std::string(str));
            }
        }
    }
    return true;
}


std::shared_ptr<LSvgDocument>  LSvgHandler::document() const
{
    return m_doc;
}

LSvgLengthType LSvgHandler::defaultCoordinateSystem() const
{
    return m_defaultCoords;
}

void LSvgHandler::setDefaultCoordinateSystem(LSvgLengthType type)
{
    m_defaultCoords = type;
}

void LSvgHandler::pushColor(const LColor &color)
{
    m_colorStack.push(color);
    m_colorTagCount.push(1);
}

void LSvgHandler::pushColorCopy()
{
    if (m_colorTagCount.size()){
        ++m_colorTagCount.top();
    }else{
        pushColor(LColorID::Black);
    }
}

void LSvgHandler::popColor()
{
    if (m_colorTagCount.size()) {
        if (!--m_colorTagCount.top()) {
            m_colorStack.pop();
            m_colorTagCount.pop();
        }
    }
}

LColor LSvgHandler::currentColor() const
{
    if (!m_colorStack.empty()){
        return m_colorStack.top();
    }else{
        return LColor(0, 0, 0);
    }
}

void LSvgHandler::setInStyle(bool b)
{
    m_inStyle = b;
}

bool LSvgHandler::inStyle() const
{
    return m_inStyle;
}

std::shared_ptr<LSvgStyleSelector> LSvgHandler::selector() const
{
    return m_selector;
}

LSvgCssHandler &LSvgHandler::cssHandler()
{
    return m_cssHandler;
}

bool LSvgHandler::processingInstruction(const std::string &target, LXmlDoc::const_iterator attributes)
{
    if (target == std::string("xml-stylesheet")) {
        bool isCss = false;
        std::string  t(LStringUtil::trim(attributes.attribute("text/css")));
        if(LStringUtil::toLower(t) == std::string("text/css")){
            isCss = true;
        }


        if (isCss) {
            std::string  addr(LStringUtil::trim(attributes.attribute("href")));
            if (!addr.empty()) {
                std::filesystem::path fi(addr);
                if (std::filesystem::exists(fi)) {
                    std::shared_ptr<LSvgNode> top = m_nodes.empty()? nullptr : m_nodes.top();
                    std::shared_ptr<LSvgStyleSheet> sheet = std::make_shared<LSvgStyleSheet>(top);
                    if(LCssParser::parseFile(addr, sheet)){
                        m_selector->m_styleSheets.push_back(sheet);
                        m_cssHandler.collectAnimations(sheet);
                    }
                }
            }
        }
    }
    return true;
}

void LSvgHandler::setAnimPeriod(int64_t start, int64_t end)
{
    m_animEnd   = std::max<int64_t>(end, m_animEnd);
}

int64_t LSvgHandler::animationDuration() const
{
    return m_animEnd;
}

LSvgHandler::~LSvgHandler()
{
}

LK_NAMESPACE_END
