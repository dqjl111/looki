#include "ltextformat.h"
#include "lkgui/paint/lpen.h"
#include "lkgui/lguiapplication.h"

LK_NAMESPACE_BEGIN

class LTextFormatData
{
public:
    LTextFormatData() : m_hashDirty(true), m_fontDirty(true) {}

    inline const std::shared_ptr<LFont> font() const {
        if (m_fontDirty){
            const_cast<LTextFormatData*>(this)->recalcFont();
        }
        return m_fnt;
    }

    bool hasProperty(uint32_t propertyId) const;
    LVariant property(uint32_t propertyId) const;
    void resolveFont(const std::shared_ptr<LFont> defaultFont);

    inline void insertProperty(uint32_t key, const LVariant &value)
    {
        m_hashDirty = true;
        if ((key >= LTextFormat::FirstFontProperty && key <= LTextFormat::LastFontProperty)
            || key == LTextFormat::FontLetterSpacingType) {
            m_fontDirty = true;
        }

        m_props[key] = value;
    }

    inline void clearProperty(uint32_t key)
    {
        if(m_props.find(key) != m_props.end()){
            m_hashDirty = true;
            if ((key >= LTextFormat::FirstFontProperty && key <= LTextFormat::LastFontProperty)
                || key == LTextFormat::FontLetterSpacingType) {
                m_fontDirty = true;
            }
            m_props.erase(key);
        }
    }

    inline uint64_t hash() const
    {
        if (!m_hashDirty){
            return m_hashValue;
        }
        return const_cast<LTextFormatData*>(this)->recalcHash();
    }

    std::map<uint32_t, LVariant> m_props;
private:
    uint64_t recalcHash();
    void recalcFont();
    mutable bool m_hashDirty = false;
    mutable bool m_fontDirty = false;
    uint64_t m_hashValue;
    mutable std::shared_ptr<LFont> m_fnt = nullptr;
};


bool LTextFormatData::hasProperty(uint32_t propertyId) const
{
    std::map<uint32_t, LVariant>::const_iterator iter = m_props.find(propertyId);
    if(iter != m_props.cend()){
        return true;
    }
}

LVariant LTextFormatData::property(uint32_t propertyId) const{
    std::map<uint32_t, LVariant>::const_iterator iter = m_props.find(propertyId);
    if(iter != m_props.cend()){
        return iter->second;
    }
    return LVariant();
}

void LTextFormatData::resolveFont(const std::shared_ptr<LFont> defaultFont){
    recalcFont();
    const uint32_t oldMask = m_fnt->getResolve();
    std::shared_ptr<LFont> t = m_fnt->resolve(defaultFont);
    if(t != m_fnt){
        m_fnt = t;
    }

    if (hasProperty(LTextFormat::FontSizeAdjustment)) {
        const double scaleFactors[7] = {double(0.7), double(0.8), double(1.0), double(1.2), double(1.5), double(2), double(2.4)};

        const int htmlFontSize = lBound<int>(0, property(LTextFormat::FontSizeAdjustment).toInt64() + 3 - 1, 6);


        if (defaultFont->pointSize() <= 0) {
            double pixelSize = scaleFactors[htmlFontSize] * defaultFont->pixelSize();
            m_fnt->setPixelSize(std::lround(pixelSize));
        } else {
            double pointSize = scaleFactors[htmlFontSize] * defaultFont->pointSizeF();
            m_fnt->setPointSizeF(pointSize);
        }
    }

    m_fnt->setResolve(oldMask);
}

void LTextFormatData::recalcFont(){
    // update cached font as well
    std::shared_ptr<LFont> f = std::make_shared<LFont>();

    bool hasSpacingInformation = false;
    LFont::SpacingType spacingType = LFont::PercentageSpacing;
    double letterSpacing = 0.0;

    std::map<uint32_t, LVariant>::const_iterator iter = m_props.cbegin();
    for (; iter != m_props.cend(); ++iter) {
        const LVariant& prop = iter->second;
        uint32_t key = iter->first;
        switch (key) {
        case LTextFormat::FontFamily:{
            f->setFamily(prop.toWString());
            break;
        }
//        case LTextFormat::FontFamilies:
//            f->setFamilies(prop.toStringList());
//            break;
        case LTextFormat::FontStyleName:{
            f->setStyleName(prop.toWString());
            break;
        }
        case LTextFormat::FontPointSize:{
            f->setPointSizeF(prop.toDouble());
            break;
        }
        case  LTextFormat::FontPixelSize:{
            f->setPixelSize(prop.toInt64());
            break;
        }
        case LTextFormat::FontWeight: {
            int weight = prop.toInt64();
            if (weight >= 0 && (!prop.isNull())){
                f->setWeight(weight);
            }
            break;
        }
        case LTextFormat::FontItalic:{
            f->setItalic(prop.toBool());
            break;
        }
        case LTextFormat::FontUnderline:{
            if (! hasProperty(LTextFormat::TextUnderlineStyle)) // don't use the old one if the new one is there.
                f->setUnderline(prop.toBool());
            break;
        }
        case LTextFormat::TextUnderlineStyle:{
            f->setUnderline(static_cast<LTextCharFormat::UnderlineStyle>(prop.toInt64()) == LTextCharFormat::SingleUnderline);
            break;
        }
        case LTextFormat::FontOverline:{
            f->setOverline(prop.toBool());
            break;
        }
        case LTextFormat::FontStrikeOut:{
            f->setStrikeOut(prop.toBool());
            break;
        }
        case LTextFormat::FontLetterSpacingType:{
            spacingType = static_cast<LFont::SpacingType>(prop.toInt64());
            hasSpacingInformation = true;
            break;
        }
        case LTextFormat::FontLetterSpacing:{
            letterSpacing = prop.toDouble();
            hasSpacingInformation = true;
            break;
        }
        case LTextFormat::FontWordSpacing:{
            f->setWordSpacing(prop.toDouble());
            break;
        }
        case LTextFormat::FontCapitalization:{
            f->setCapitalization(static_cast<LFont::Capitalization> (prop.toInt64()));
            break;
        }
        case LTextFormat::FontFixedPitch: {
            const bool value = prop.toBool();
            if (f->fixedPitch() != value){
                f->setFixedPitch(value);
            }
            break;
        }
        case LTextFormat::FontStretch:{
            f->setStretch(prop.toInt64());
            break;
        }
        case LTextFormat::FontStyleHint:{
            f->setStyleHint(static_cast<LFont::StyleHint>(prop.toInt64()), f->styleStrategy());
            break;
        }
        case LTextFormat::FontHintingPreference:{
            f->setHintingPreference(static_cast<LFont::HintingPreference>(prop.toInt64()));
            break;
        }
        case LTextFormat::FontStyleStrategy:{
            f->setStyleStrategy(static_cast<LFont::StyleStrategy>(prop.toInt64()));
            break;
        }
        case LTextFormat::FontKerning:{
            f->setKerning(prop.toBool());
            break;
        }
        default:{
            break;
        }
        }
    }

    if (hasSpacingInformation){
        f->setLetterSpacing(spacingType, letterSpacing);
    }

    m_fnt = f;
    m_fontDirty = false;
}

static inline uint32_t variantHash(const LVariant &variant)
{
    // simple and fast hash functions to differentiate between type and value

}

uint64_t LTextFormatData::recalcHash()
{
    m_hashValue = 0;
    std::map<uint32_t, LVariant>::const_iterator iter = m_props.cbegin();
    for (; iter != m_props.cend(); ++iter) {
        m_hashValue += (uint64_t(iter->first) << 48) + iter->second.hashValue();
    }
    m_hashDirty = false;
    return m_hashValue;
}

static inline uint64_t getHash(const std::shared_ptr<LTextFormatData> d, int format)
{
    return (d!= nullptr ? d->hash() : 0) + format;
}

LTextFormat::LTextFormat()
{

}

LTextFormat::LTextFormat(const LTextFormat &rhs)
 : m_data(rhs.m_data), m_formatType(rhs.m_formatType)
{

}

LTextFormat &LTextFormat::operator=(const LTextFormat &rhs)
{
    m_data = rhs.m_data;
    m_formatType = rhs.m_formatType;
    return *this;
}

LVariant LTextFormat::property(uint32_t propertyId) const
{
    if(m_data != nullptr){
        return m_data->property(propertyId);
    }
    return LVariant();
}

void LTextFormat::setProperty(uint32_t propertyId, const LVariant &value)
{
    if(m_data == nullptr){
        m_data = std::make_shared<LTextFormatData>();
    }
    m_data->m_props[propertyId] = value;
}

void LTextFormat::clearProperty(uint32_t propertyId)
{
    if(m_data != nullptr){
        m_data->m_props.erase(propertyId);
    }
}

bool LTextFormat::hasProperty(uint32_t propertyId) const
{
    if(m_data != nullptr){
        return m_data->hasProperty(propertyId);
    }
    return false;
}

bool LTextFormat::boolProperty(int propertyId) const
{
    if(m_data != nullptr){
        std::map<uint32_t, LVariant>::const_iterator iter = m_data->m_props.find(propertyId);
        if(iter != m_data->m_props.cend()){
            return iter->second.toBool();
        }
    }
    return false;
}

int LTextFormat::intProperty(int propertyId) const
{
    if(m_data != nullptr){
        std::map<uint32_t, LVariant>::const_iterator iter = m_data->m_props.find(propertyId);
        if(iter != m_data->m_props.cend()){
            return iter->second.toInt64();
        }
    }
    return 0;
}

double LTextFormat::doubleProperty(int propertyId) const
{
    if(m_data != nullptr){
        std::map<uint32_t, LVariant>::const_iterator iter = m_data->m_props.find(propertyId);
        if(iter != m_data->m_props.cend()){
            return iter->second.toDouble();
        }
    }
    return 0;
}

std::string LTextFormat::stringProperty(int propertyId) const
{
    if(m_data != nullptr){
        std::map<uint32_t, LVariant>::const_iterator iter = m_data->m_props.find(propertyId);
        if(iter != m_data->m_props.cend()){
            return iter->second.toString();
        }
    }
    return "";
}

std::wstring LTextFormat::wstringProperty(int propertyId) const
{
    if(m_data != nullptr){
        std::map<uint32_t, LVariant>::const_iterator iter = m_data->m_props.find(propertyId);
        if(iter != m_data->m_props.cend()){
            return iter->second.toWString();
        }
    }
    return L"";
}

std::shared_ptr<LBrush> LTextFormat::brushProperty(int propertyId) const
{
    std::shared_ptr<LBrush> ret  = nullptr;
    if(m_data != nullptr){
        std::map<uint32_t, LVariant>::const_iterator iter = m_data->m_props.find(propertyId);
        if(iter != m_data->m_props.cend()){
            const LVariant& var = iter->second;
            ret = var.getIVarT<LBrush>();
            if(ret != nullptr){
                return ret;
            }
        }
    }
    ret = std::make_shared<LBrush>();
    return ret;
}

std::shared_ptr<LPen> LTextFormat::penProperty(int propertyId) const
{
    std::shared_ptr<LPen> ret  = nullptr;
    if(m_data != nullptr){
        std::map<uint32_t, LVariant>::const_iterator iter = m_data->m_props.find(propertyId);
        if(iter != m_data->m_props.cend()){
            const LVariant& var = iter->second;
            ret = var.getIVarT<LPen>();
            if(ret != nullptr){
                return ret;
            }
        }
    }

    ret = std::make_shared<LPen>();
    ret->m_style = LPenStyle::No;
    return ret;
}

void LTextFormat::setBackground(const std::shared_ptr<LBrush> brush)
{
    setProperty(BackgroundBrush, LVariant(brush));
}

void LTextFormat::setForeground(const std::shared_ptr<LBrush> brush)
{
    setProperty(ForegroundBrush, LVariant(brush));
}

bool LTextFormat::operator==(const LTextFormat &rhs) const
{
    if(m_data == rhs.m_data){
        return true;
    }


    if (m_formatType != rhs.m_formatType){
        return false;
    }

    if(m_data == nullptr && rhs.m_data->m_props.empty()){
        return true;
    }

    if(rhs.m_data == nullptr && m_data->m_props.empty()){
        return true;
    }

    if (m_data->m_props.empty() && rhs.m_data->m_props.empty()){
        return true;
    }


    std::map<uint32_t, LVariant>::const_iterator iter = m_data->m_props.begin();
    for(; iter != m_data->m_props.end(); ++iter){
        std::map<uint32_t, LVariant>::const_iterator fIter = rhs.m_data->m_props.find(iter->first);
        if(fIter != rhs.m_data->m_props.end()){
            if(iter->second != fIter->second){
                return false;
            }
        }else{
            if(!iter->second.isNull()){
                return false;
            }
        }
    }

    iter = rhs.m_data->m_props.begin();
    for(; iter != rhs.m_data->m_props.end(); ++iter){
        std::map<uint32_t, LVariant>::const_iterator fIter = m_data->m_props.find(iter->first);
        if(fIter != rhs.m_data->m_props.end()){
            if(!iter->second.isNull()){
                return false;
            }
        }
    }
    return true;
}

void LTextFormat::merge(const LTextFormat &other)
{
    if (m_formatType != other.m_formatType){
        return;
    }

    if (m_data == nullptr) {
        m_data = other.m_data;
        return;
    }

    if (other.m_data == nullptr){
        return;
    }


    const std::map<uint32_t, LVariant> &otherProps = other.m_data->m_props;
    std::map<uint32_t, LVariant>::const_iterator iter = otherProps.cbegin();
    for (; iter != otherProps.cend(); ++iter) {
        m_data->m_props[iter->first] = iter->second;
    }
}

LTextCharFormat LTextFormat::toCharFormat() const
{
    return LTextCharFormat(*this);
}

LTextCharFormat::LTextCharFormat()
{
    m_formatType = CharFormat;
}

void LTextCharFormat::setFont(const std::shared_ptr<LFont> font,
                              FontPropertiesInheritanceBehavior behavior)
{
    if(font == nullptr){
        return;
    }

    const uint32_t mask = behavior == FontPropertiesAll ? uint32_t(LFont::AllPropertiesResolved)
                                                        : font->getResolve();

    if (mask & LFont::FamilyResolved){
        setFontFamily(font->family());
    }
    if (mask & LFont::StyleNameResolved){
        setFontStyleName(font->styleName());
    }

    if (mask & LFont::SizeResolved) {
        const double pointSize = font->pointSizeF();
        if (pointSize > 0) {
            setFontPointSize(pointSize);
        } else {
            const int pixelSize = font->pixelSize();
            if (pixelSize > 0){
                setProperty(LTextFormat::FontPixelSize, pixelSize);
            }
        }
    }

    if (mask & LFont::WeightResolved){
        setFontWeight(font->weight());
    }
    if (mask & LFont::StyleResolved){
        setFontItalic(font->style() != LFont::StyleNormal);
    }
    if (mask & LFont::UnderlineResolved){
        setUnderlineStyle(font->underline() ? SingleUnderline : NoUnderline);
    }
    if (mask & LFont::OverlineResolved){
        setFontOverline(font->overline());
    }
    if (mask & LFont::StrikeOutResolved){
        setFontStrikeOut(font->strikeOut());
    }
    if (mask & LFont::FixedPitchResolved){
        setFontFixedPitch(font->fixedPitch());
    }
    if (mask & LFont::CapitalizationResolved){
        setFontCapitalization(font->capitalization());
    }
    if (mask & LFont::WordSpacingResolved){
        setFontWordSpacing(font->wordSpacing());
    }
    if (mask & LFont::LetterSpacingResolved) {
        setFontLetterSpacingType(font->letterSpacingType());
        setFontLetterSpacing(font->letterSpacing());
    }
    if (mask & LFont::StretchResolved){
        setFontStretch(font->stretch());
    }
    if (mask & LFont::StyleHintResolved){
        setFontStyleHint(font->styleHint());
    }
    if (mask & LFont::StyleStrategyResolved){
        setFontStyleStrategy(font->styleStrategy());
    }
    if (mask & LFont::HintingPreferenceResolved){
        setFontHintingPreference(font->hintingPreference());
    }
    if (mask & LFont::KerningResolved){
        setFontKerning(font->kerning());
    }
}

std::shared_ptr<LFont> LTextCharFormat::font() const
{
    if(m_data != nullptr){
        std::shared_ptr<LFont> ret = m_data->font();
        if(ret != nullptr){
            return ret;
        }
    }
    return LGuiApplication::font();
}

bool LTextCharFormat::fontUnderline() const
{
    if (hasProperty(TextUnderlineStyle)){
        return underlineStyle() == LPenStyle::Solid;
    }
    return boolProperty(FontUnderline);
}

void LTextCharFormat::setUnderlineStyle(UnderlineStyle style)
{
    setProperty(TextUnderlineStyle, style);
    // for compatibility
    setProperty(FontUnderline, style == SingleUnderline);
}

void LTextCharFormat::setTextOutline(const std::shared_ptr<LPen> pen)
{
    setProperty(TextOutline, LVariant(pen));
}

LTextCharFormat::LTextCharFormat(const LTextFormat &fmt)
    : LTextFormat(fmt)
{

}

LTextFormatCollection::~LTextFormatCollection()
{

}

void LTextFormatCollection::clear()
{
    m_formats.clear();
    m_objFormats.clear();
    m_hashes.clear();

}

int LTextFormatCollection::objectFormatIndex(int objectIndex) const
{
    if (objectIndex == -1){
        return -1;
    }
    return m_objFormats.at(objectIndex);
}

void LTextFormatCollection::setObjectFormatIndex(int objectIndex, int formatIndex)
{
     m_objFormats[objectIndex] = formatIndex;
}

int LTextFormatCollection::createObjectIndex(const LTextFormat &f)
{
     const int objectIndex = m_objFormats.size();
     m_objFormats.push_back(indexForFormat(f));
     return objectIndex;
}

int LTextFormatCollection::indexForFormat(const LTextFormat &format)
{
    uint64_t hash = getHash(format.m_data, format.m_formatType);
    std::multimap<uint64_t,int>::iterator i = m_hashes.find(hash);
    while (i != m_hashes.cend() && i->first == hash) {
        if (m_formats.at(i->second) == format) {
            return i->second;
        }
        ++i;
    }

    int idx = m_formats.size();
    m_formats.push_back(format);
    LTextFormat &f = m_formats.back();
    if (f.m_data == nullptr){
        f.m_data = std::shared_ptr<LTextFormatData>();
    }
    f.m_data->resolveFont(m_defaultFnt);

    m_hashes.insert(std::pair<uint64_t, int>(hash, idx));
    return idx;
}

bool LTextFormatCollection::hasFormatCached(const LTextFormat &format) const
{
    uint64_t hash = getHash(format.m_data, format.m_formatType);
    std::multimap<uint64_t, int>::const_iterator i = m_hashes.find(hash);
    while (i != m_hashes.end() && i->first == hash) {
        if (m_formats.at(i->second) == format) {
            return true;
        }
        ++i;
    }
    return false;
}

LTextFormat LTextFormatCollection::format(int idx) const
{
    if (idx < 0 || idx >= m_formats.size()){
        return LTextFormat();
    }

    return m_formats[idx];
}

void LTextFormatCollection::setDefaultFont(const std::shared_ptr<LFont> f)
{
    if(f != nullptr){
        m_defaultFnt = f;
        for(LTextFormat& f : m_formats){
            f.m_data->resolveFont(m_defaultFnt);
        }
    }
}


LK_NAMESPACE_END
