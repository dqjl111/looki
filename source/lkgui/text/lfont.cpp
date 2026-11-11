#include "lfont.h"
#include "lkgui/paint/lpaintdevice.h"
#include "lfontdatabase.h"
#include "lfontcache.h"
#include "lkcore/lstringutil.h"
#include "lkgui/lguiapplication.h"
#include "lkgui/lscreen.h"
#include  "lkcore/ldebug.h"

LK_NAMESPACE_BEGIN
bool LFont::Def::exactMatch(const LFont::Def &other) const
{
    if (pixelSize != -1 && other.pixelSize != -1) {
        if (pixelSize != other.pixelSize){
            return false;
        }
    } else if (pointSize != -1 && other.pointSize != -1) {
        if (pointSize != other.pointSize){
            return false;
        }
    } else {
        return false;
    }

    if (!ignorePitch && !other.ignorePitch && fixedPitch != other.fixedPitch){
        return false;
    }

    if (stretch != 0 && other.stretch != 0 && stretch != other.stretch){
        return false;
    }

    return (styleHint     == other.styleHint
            && styleStrategy == other.styleStrategy
            && weight        == other.weight
            && style        == other.style
            && family   == other.family
            && (styleName.empty() || other.styleName.empty() || styleName == other.styleName)
            );

    return false;
}



LFont::LFont()
    : m_engine(nullptr), m_dpi(defaultDpi()),
    m_underline(false), m_overline(false), m_strikeOut(false), m_kerning(true),
    m_capital(0), m_letterSpacingIsAbsolute(false),
    m_letterSpacing(0), m_wordSpacing(0)
{

}

LFont::LFont(const std::shared_ptr<LFont> font)
{
    if(font != nullptr){
        m_request = font->m_request;
        m_dpi = font->m_dpi;
        m_underline = font->m_underline;
        m_overline = font->m_overline;
        m_strikeOut = font->m_strikeOut;
        m_kerning = font->m_kerning;
        m_capital = font->m_capital;
        m_letterSpacingIsAbsolute = font->m_letterSpacingIsAbsolute;
        m_letterSpacing = font->m_letterSpacing;
        m_wordSpacing = font->m_wordSpacing;

        m_engine = font->m_engine;
    }else{
        m_engine = nullptr;
        m_dpi = defaultDpi();
        m_underline = false;
        m_overline = false;
        m_strikeOut = false;
        m_kerning = true;
        m_capital = 0;
        m_letterSpacingIsAbsolute = false;
        m_letterSpacing = 0;
        m_wordSpacing = 0;
    }
}

LFont::LFont(const LFont &font)
{
    m_request = font.m_request;
    m_dpi = font.m_dpi;
    m_underline = font.m_underline;
    m_overline = font.m_overline;
    m_strikeOut = font.m_strikeOut;
    m_kerning = font.m_kerning;
    m_capital = font.m_capital;
    m_letterSpacingIsAbsolute = font.m_letterSpacingIsAbsolute;
    m_letterSpacing = font.m_letterSpacing;
    m_wordSpacing = font.m_wordSpacing;

    m_engine = font.m_engine;
}

LFont::LFont(const std::wstring &family, int pointSize, int weight, bool italic)
    : m_engine(nullptr), m_dpi(defaultDpi()),
    m_underline(false), m_overline(false), m_strikeOut(false), m_kerning(true),
    m_capital(0), m_letterSpacingIsAbsolute(false),
    m_letterSpacing(0), m_wordSpacing(0)
{
    m_request.family = family;
    m_request.pointSize = pointSize;
    m_request.pixelSize = -1;
    m_request.weight = weight;
    m_request.style = italic ? LFont::StyleItalic : LFont::StyleNormal;
}

LFont::LFont(const std::string &family, int pointSize, int weight, bool italic)
    : m_engine(nullptr), m_dpi(defaultDpi()),
    m_underline(false), m_overline(false), m_strikeOut(false), m_kerning(true),
    m_capital(0), m_letterSpacingIsAbsolute(false),
    m_letterSpacing(0), m_wordSpacing(0)
{
    m_request.family = LStringUtil::utf8ToWide(family);
    m_request.pointSize = pointSize;
    m_request.pixelSize = -1;
    m_request.weight = weight;
    m_request.style = italic ? LFont::StyleItalic : LFont::StyleNormal;
}

std::shared_ptr<LFontEngine> LFont::engine()
{
    if(m_engine == nullptr){
        m_engine = LFontDatabase::load(this);
    }
    return m_engine;
}

void LFont::setStyle(Style style)
{
    if(m_request.style != style){
        m_engine = nullptr;
        m_request.style = style;
    }
    m_resolveMask |= StyleResolved;
}

LFont::Style LFont::style() const
{
    return (LFont::Style)m_request.style;
}

const std::wstring& LFont::family() const
{
    return m_request.family;
}

void LFont::setFamily(const std::wstring &family)
{
    if(m_request.family != family){
        m_engine = nullptr;
        m_request.family = family;
    }
    m_resolveMask |= FamilyResolved;
}

void LFont::setFamily(const std::string &s)
{
    std::wstring ws = LStringUtil::utf8ToWide(s);
    setFamily(ws);
}

const std::wstring &LFont::styleName() const
{
    return m_request.styleName;
}

void LFont::setStyleName(const std::wstring &style)
{
    if(m_request.styleName != style){
        m_engine = nullptr;
        m_request.styleName = style;
    }
    m_resolveMask |= StyleNameResolved;
}

void LFont::setStyleName(const std::string &s)
{
    std::wstring ws = LStringUtil::utf8ToWide(s);
    setStyleName(ws);
}

int LFont::weight() const
{
    return m_request.weight;
}

void LFont::setWeight(int weight)
{
    if(m_request.weight != weight){
        m_engine = nullptr;
        m_request.weight = weight;
    }
    m_resolveMask |= WeightResolved;
}

int LFont::pointSize() const
{
    return std::lround(m_request.pointSize);
}

void LFont::setPointSize(int pointSize)
{
    if(m_request.pointSize != pointSize){
        m_engine = nullptr;
        m_request.pointSize = pointSize;
        m_request.pixelSize = -1;
    }
    m_resolveMask |= SizeResolved;
}

double LFont::pointSizeF() const
{
    return m_request.pointSize;
}

void LFont::setPointSizeF(double pointSize)
{
    if(m_request.pointSize != pointSize){
        m_engine = nullptr;
        m_request.pointSize = pointSize;
        m_request.pixelSize = -1;
    }
    m_resolveMask |= SizeResolved;
}

int LFont::pixelSize() const
{
    return std::lround(m_request.pixelSize);
}

void LFont::setPixelSize(int pixelSize)
{
    if(m_request.pixelSize != pixelSize){
        m_engine = nullptr;
        m_request.pixelSize = pixelSize;
        m_request.pointSize = -1;
    }
    m_resolveMask |= SizeResolved;
}

double LFont::pixelSizeF() const
{
    return m_request.pixelSize;
}

void LFont::setPixelSizeF(double pixelSize)
{
    if(m_request.pixelSize != pixelSize){
        m_engine = nullptr;
        m_request.pixelSize = pixelSize;
        m_request.pointSize = -1;
    }
    m_resolveMask |= SizeResolved;
}

bool LFont::underline() const
{
    return m_underline;
}

void LFont::setUnderline(bool enable)
{
    if(m_underline != enable){
        m_underline = enable;
    }
    m_resolveMask |= UnderlineResolved;
}

bool LFont::overline() const
{
    return m_overline;
}

void LFont::setOverline(bool enable)
{
    if(m_overline != enable){
        m_overline = enable;
    }
    m_resolveMask |= OverlineResolved;

}

bool LFont::strikeOut() const
{
    return m_strikeOut;
}

void LFont::setStrikeOut(bool enable)
{
    if(m_strikeOut != enable){
        m_strikeOut = enable;
    }
    m_resolveMask |= StrikeOutResolved;
}

void LFont::alterCharForCapitalization(wchar_t &c) const
{
    switch (m_capital) {
    case LFont::AllUppercase:
    case LFont::SmallCaps:{
        c = LStringUtil::toUpper(c);
        break;
    }
    case LFont::AllLowercase:{
        c = LStringUtil::toLower(c);
        break;
    }
    default:{
        break;
    }
    }
}

void LFont::setCapitalization(Capitalization caps)
{
    if(m_capital != caps){
        m_capital = caps;
    }
    m_resolveMask |= CapitalizationResolved;
}

LFont::Capitalization LFont::capitalization() const
{
    return static_cast<LFont::Capitalization> (m_capital);
}

double LFont::letterSpacing() const
{
    return m_letterSpacing;
}

LFont::SpacingType LFont::letterSpacingType() const
{
    return m_letterSpacingIsAbsolute ? AbsoluteSpacing : PercentageSpacing;
}

void LFont::setLetterSpacing(SpacingType type, double spacing)
{
    const bool absoluteSpacing = type == AbsoluteSpacing;
    m_letterSpacing = spacing;
    m_letterSpacingIsAbsolute = absoluteSpacing;
    m_resolveMask |= LetterSpacingResolved;
}

double LFont::wordSpacing() const
{
    return m_wordSpacing;
}

void LFont::setWordSpacing(double spacing)
{
    m_wordSpacing = spacing;
    m_resolveMask |= WordSpacingResolved;
}

bool LFont::fixedPitch() const
{
    return m_request.fixedPitch;
}

void LFont::setFixedPitch(bool enable)
{
    if(m_request.fixedPitch != enable){
        m_request.fixedPitch = enable;
        m_request.ignorePitch = false;
        m_engine = nullptr;
    }
    m_resolveMask |= FixedPitchResolved;
}

bool LFont::kerning() const
{
    return m_kerning;
}

void LFont::setKerning(bool enable)
{
    m_kerning = enable;
    m_resolveMask |= KerningResolved;
}

LFont::StyleHint LFont::styleHint() const
{
    return (LFont::StyleHint)m_request.styleHint;
}

LFont::StyleStrategy LFont::styleStrategy() const
{
    return (LFont::StyleStrategy)m_request.styleStrategy;
}

void LFont::setStyleHint(StyleHint hint, StyleStrategy strategy)
{
    if(m_request.styleHint != hint || m_request.styleStrategy != strategy){
        m_engine = nullptr;
        m_request.styleHint = hint;
        m_request.styleStrategy = strategy;
    }
    m_resolveMask |= StyleHintResolved;
    m_resolveMask |= StyleStrategyResolved;
}

void LFont::setStyleStrategy(StyleStrategy s)
{
    if(m_request.styleStrategy != s){
        m_engine = nullptr;
        m_request.styleStrategy = s;
    }
    m_resolveMask |= StyleStrategyResolved;
}

void LFont::setHintingPreference(HintingPreference hintingPreference)
{
    if(m_request.hintingPreference != hintingPreference){
        m_engine = nullptr;
        m_request.hintingPreference = hintingPreference;
    }
    m_resolveMask |= HintingPreferenceResolved;
}

LFont::HintingPreference LFont::hintingPreference() const
{
    return (LFont::HintingPreference)m_request.hintingPreference;
}

int LFont::stretch() const
{
    return m_request.stretch;
}

void LFont::setStretch(int factor)
{
    if ((factor < 0) || (factor > 4000)) {
        lWarn() << "LFont::setStretch " << factor << " out of range ";
        return;
    }

    if ((m_resolveMask & LFont::StretchResolved) &&
        m_request.stretch == (uint32_t)factor){
        return;
    }

    if(m_request.stretch != factor){
        m_engine = nullptr;
        m_request.stretch = (uint32_t)factor;
    }
    m_resolveMask |= LFont::StretchResolved;
}

std::shared_ptr<LFont> LFont::resolve(const std::shared_ptr<LFont> other) const
{
    if(other != nullptr){
        if (m_resolveMask == 0 || (m_resolveMask == other->m_resolveMask && *this == *other)) {
            return getPtrT<LFont>();
        }
    }

    std::shared_ptr<LFont> font = std::make_shared<LFont>(*this);
    font->resolve(m_resolveMask, other);
    return font;
}

void LFont::resolve(uint32_t mask, const std::shared_ptr<LFont> other)
{
    if(other == nullptr){
        return;
    }
    m_dpi = other->m_dpi;

    if ((mask & LFont::AllPropertiesResolved) == LFont::AllPropertiesResolved){
        return;
    }

    // assign the unset-bits with the set-bits of the other font def
    if (! (mask & LFont::FamilyResolved)){
        m_request.family = other->m_request.family;
    }

    if (! (mask & LFont::StyleNameResolved)){
        m_request.styleName = other->m_request.styleName;
    }

    if (! (mask & LFont::SizeResolved)) {
        m_request.pointSize = other->m_request.pointSize;
        m_request.pixelSize = other->m_request.pixelSize;
    }

    if (! (mask & LFont::StyleHintResolved)){
        m_request.styleHint = other->m_request.styleHint;
    }

    if (! (mask & LFont::StyleStrategyResolved)){
        m_request.styleStrategy = other->m_request.styleStrategy;
    }

    if (! (mask & LFont::WeightResolved)){
        m_request.weight = other->m_request.weight;
    }

    if (! (mask & LFont::StyleResolved)){
        m_request.style = other->m_request.style;
    }

    if (! (mask & LFont::FixedPitchResolved)){
        m_request.fixedPitch = other->m_request.fixedPitch;
    }

    if (! (mask & LFont::StretchResolved)){
        m_request.stretch = other->m_request.stretch;
    }

    if (! (mask & LFont::HintingPreferenceResolved)){
        m_request.hintingPreference = other->m_request.hintingPreference;
    }

    if (! (mask & LFont::UnderlineResolved)){
        m_underline = other->m_underline;
    }

    if (! (mask & LFont::OverlineResolved)){
        m_overline = other->m_overline;
    }

    if (! (mask & LFont::StrikeOutResolved)){
        m_strikeOut = other->m_strikeOut;
    }

    if (! (mask & LFont::KerningResolved)){
        m_kerning = other->m_kerning;
    }

    if (! (mask & LFont::LetterSpacingResolved)) {
        m_letterSpacing = other->m_letterSpacing;
        m_letterSpacingIsAbsolute = other->m_letterSpacingIsAbsolute;
    }
    if (! (mask & LFont::WordSpacingResolved)){
        m_wordSpacing = other->m_wordSpacing;
    }
    if (! (mask & LFont::CapitalizationResolved)){
        m_capital = other->m_capital;
    }
}

std::shared_ptr<LFontEngine> LFont::engine() const
{
    if(m_engine == nullptr){
        const_cast<LFont*>(this)->m_engine = LFontDatabase::load(this);
    }
    return m_engine;
}

int LFont::defaultDpiX()
{
    if (LGuiApplication::instance()->testAttribute(LAppAttribute::Use96Dpi)){
        return 96;
    }

    std::shared_ptr<LScreen> s= LGuiApplication::primaryScreen();
    if (s != nullptr){
        return std::lround(s->logicalDotsPerInchX());
    }
    return 100;
}

int LFont::defaultDpiY()
{
    if (LGuiApplication::instance()->testAttribute(LAppAttribute::Use96Dpi)){
        return 96;
    }

    std::shared_ptr<LScreen> s= LGuiApplication::primaryScreen();
    if (s != nullptr){
        return std::lround(s->logicalDotsPerInchY());
    }
    return 100;
}

int LFont::defaultDpi()
{
    return defaultDpiY();
}

bool LFont::operator==(const LFont &f) const
{
    return (this == &f
            || (f.m_request   == m_request
                && f.m_request.pointSize == m_request.pointSize
                && f.m_underline == m_underline
                && f.m_overline  == m_overline
                && f.m_strikeOut == m_strikeOut
                && f.m_kerning == m_kerning
                && f.m_capital == m_capital
                && f.m_letterSpacingIsAbsolute == m_letterSpacingIsAbsolute
                && f.m_letterSpacing == m_letterSpacing
                && f.m_wordSpacing == m_wordSpacing
                ));
}

bool LFont::operator!=(const LFont &f) const
{
    return !(operator==(f));
}

bool LFont::operator<(const LFont &f) const
{
    if (this == &f){
        return false;
    }
    // the < operator for fontdefs ignores point sizes.
    const LFont::Def &r1 = f.m_request;
    const LFont::Def &r2 = m_request;
    if (r1.pointSize != r2.pointSize){
        return r1.pointSize < r2.pointSize;
    }
    if (r1.pixelSize != r2.pixelSize){
        return r1.pixelSize < r2.pixelSize;
    }
    if (r1.weight != r2.weight){
        return r1.weight < r2.weight;
    }
    if (r1.style != r2.style){
        return r1.style < r2.style;
    }
    if (r1.stretch != r2.stretch){
        return r1.stretch < r2.stretch;
    }
    if (r1.styleHint != r2.styleHint){
        return r1.styleHint < r2.styleHint;
    }
    if (r1.styleStrategy != r2.styleStrategy){
        return r1.styleStrategy < r2.styleStrategy;
    }
    //if (r1.families != r2.families) return r1.families < r2.families;
    if (r1.family != r2.family){
        return r1.family < r2.family;
    }
    if (f.m_capital != m_capital){
        return f.m_capital < m_capital;
    }

    if (f.m_letterSpacingIsAbsolute != m_letterSpacingIsAbsolute){
        return f.m_letterSpacingIsAbsolute < m_letterSpacingIsAbsolute;
    }
    if (f.m_letterSpacing != m_letterSpacing){
        return f.m_letterSpacing < m_letterSpacing;
    }
    if (f.m_wordSpacing != m_wordSpacing){
        return f.m_wordSpacing < m_wordSpacing;
    }

    int f1attrs = (f.m_underline << 3) + (f.m_overline << 2) + (f.m_strikeOut<<1) + f.m_kerning;
    int f2attrs = (m_underline << 3) + (m_overline << 2) + (m_strikeOut<<1) + m_kerning;
    return f1attrs < f2attrs;

}



LK_NAMESPACE_END
