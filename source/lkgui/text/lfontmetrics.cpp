#include "lfontmetrics.h"
#include "lkcore/text/lchar.h"
#include "lkcore/lstringutil.h"
#include "lkgui/text/lfontengine.h"
#include "lkgui/paint/lpainter.h"
#include "lkgui/text/ltextengine.h"
LK_NAMESPACE_BEGIN

LFontMetrics::LFontMetrics(const std::shared_ptr<LFont> font) :m_font(font)
{

}

double LFontMetrics::horizontalAdvance(const std::wstring &text, int len) const
{
    int pos = text.find(wchar_t(0x9c));
    if (pos != -1) {
        len = (len < 0) ? pos : std::min<int>(pos, len);
    } else if (len < 0) {
        len = text.length();
    }
    if (len == 0){
        return 0;
    }

    LStackTextEngine layout(text, m_font);
    return layout.width(0, len);
}

double LFontMetrics::horizontalAdvance(wchar_t ch) const
{
    if (LStringUtil::category(ch) == lchar::Mark_NonSpacing){
        return 0;
    }

    std::shared_ptr<LFontEngine> engine = m_font->engine();
    m_font->alterCharForCapitalization(ch);
    uint32_t glyph = engine->glyphIndex(ch);
    double advance = 0;

    LGlyphLayout glyphs;
    glyphs.numGlyphs = 1;
    glyphs.glyphs = &glyph;
    glyphs.advances = &advance;
    engine->recalcAdvances(&glyphs, 0);
    return advance;
}

double LFontMetrics::averageCharWidth() const
{
    std::shared_ptr<LFontEngine> engine = m_font->engine();
    return engine->averageCharWidth();
}

double LFontMetrics::ascent() const
{
    std::shared_ptr<LFontEngine> engine = m_font->engine();
    return engine->ascent();
}

double LFontMetrics::capHeight() const
{
    std::shared_ptr<LFontEngine> engine = m_font->engine();
    return engine->capHeight();
}

double LFontMetrics::descent() const
{
    std::shared_ptr<LFontEngine> engine = m_font->engine();
    return engine->descent();
}

double LFontMetrics::height() const
{
    std::shared_ptr<LFontEngine> engine = m_font->engine();
    return (engine->ascent() + engine->descent());
}

double LFontMetrics::leading() const
{
    std::shared_ptr<LFontEngine> engine = m_font->engine();
    return engine->leading();
}

double LFontMetrics::lineSpacing() const
{
    std::shared_ptr<LFontEngine> engine = m_font->engine();
    return (engine->leading() + engine->ascent() + engine->descent());
}

double LFontMetrics::minLeftBearing() const
{
    std::shared_ptr<LFontEngine> engine = m_font->engine();
    return engine->minLeftBearing();
}

double LFontMetrics::minRightBearing() const
{
    std::shared_ptr<LFontEngine> engine = m_font->engine();
    return engine->minRightBearing();
}

double LFontMetrics::maxWidth() const
{
    std::shared_ptr<LFontEngine> engine = m_font->engine();
    return engine->maxCharWidth();
}

LRect<int> LFontMetrics::boundingRect(const LRect<int> &r, uint32_t flags,
                                      const std::wstring &text, int tabstops, int *tabarray) const
{
    int tabArrayLen = 0;
    if (tabarray){
        while (tabarray[tabArrayLen]){
            tabArrayLen++;
        }
    }

    LRect<double> rb;
    LRect<double> rr = r.toRectF();
    LPainter::formatText(m_font, rr, flags | (uint32_t)LTextFlag::DontPrint, text, &rb, tabstops, tabarray,
                   tabArrayLen, nullptr);

    return rb.toAlignedRect();
}

LRect<int> LFontMetrics::boundingRect(wchar_t ch) const
{
    std::shared_ptr<LFontEngine> engine = m_font->engine();

    uint32_t glyph = engine->glyphIndex(ch);

    LGlyphMetrics<double>  gm = engine->boundingBox(glyph);
    return LRect<double>(gm.m_x, gm.m_y, gm.m_width, gm.m_height, false).toAlignedRect();
}

LSize<int> LFontMetrics::size(int flags, const std::wstring &str, int tabstops, int *tabarray) const
{
    return boundingRect(LRect<int>(),
                        flags | (int)LTextFlag::LongestVariant,
                        str,
                        tabstops,
                        tabarray).size();
}

std::wstring LFontMetrics::elidedText(const std::wstring &text, LTextElide mode, double width, int flags) const
{
    std::wstring _text = text;
    if (!(flags & (int)LTextFlag::LongestVariant)) {
        int posA = 0;
        int posB = _text.find('\x9c');
        while (posB >= 0) {
            std::wstring portion = _text.substr(posA, posB - posA);
            if (size(flags, portion).width() <= width){
                return portion;
            }
            posA = posB + 1;
            posB = _text.find(wchar_t('\x9c'), posA);
        }
        _text = _text.substr(posA);
    }
    LStackTextEngine engine(_text, m_font);
    return engine.elidedText(mode, width, flags);
}



LK_NAMESPACE_END
