#include "ltextitem.h"
#include "ltextengine.h"

LK_NAMESPACE_BEGIN


void LGlyphLayout::grow(char *address, int totalGlyphs)
{
    LGlyphLayout oldLayout(address, numGlyphs);
    LGlyphLayout newLayout(address, totalGlyphs);

    if (numGlyphs) {
        // move the existing data
        memmove(newLayout.attributes, oldLayout.attributes, numGlyphs * sizeof(LGlyphAttributes));
        memmove(newLayout.justifications, oldLayout.justifications, numGlyphs * sizeof(LGlyphJustification));
        memmove(newLayout.advances, oldLayout.advances, numGlyphs * sizeof(double));
        memmove(newLayout.glyphs, oldLayout.glyphs, numGlyphs * sizeof(uint32_t));
    }
    // clear the new data
    newLayout.clear(numGlyphs);
    *this = newLayout;
}

LTextItem::LTextItem()
{

}

LTextItem::LTextItem(const LScriptItem *si, std::shared_ptr<LFont> _font, const LTextCharFormat &format)
    : charFormat(format),
    font(_font)
{
    if(font != nullptr){
        fontEngine = font->engine();
    }
    if(si != nullptr){
        initWithScriptItem(si);
    }
}

LTextItem::LTextItem(const LGlyphLayout &g,
                     std::shared_ptr<LFont> _font,
                     const wchar_t *_chars,
                     int _numChars,
                     std::shared_ptr<LFontEngine> fe,
                     const LTextCharFormat &format)
    : charFormat(format),
    numChars(_numChars),
    chars(_chars),
    font(_font),
    glyphs(g),
    fontEngine(fe)
{

}

void LTextItem::initWithScriptItem(const LScriptItem *si)
{
    flags = 0;
    if (si->analysis.bidiLevel %2){
        flags |= LTextRenderHint::RightToLeft;
    }
    ascent = si->ascent;
    descent = si->descent;

    if (charFormat.hasProperty(LTextFormat::TextUnderlineStyle)) {
        underlineStyle = charFormat.underlineStyle();
    } else if (charFormat.boolProperty(LTextFormat::FontUnderline)
               || font->underline()) {
        underlineStyle = LPenStyle::Solid;
    }

    // compat
    if (underlineStyle == LPenStyle::Solid){
        flags |= LTextRenderHint::Underline;
    }

    if (font->overline() || charFormat.fontOverline()){
        flags |= LTextRenderHint::Overline;
    }
    if (font->strikeOut() || charFormat.fontStrikeOut()){
        flags |= LTextRenderHint::StrikeOut;
    }
}


LK_NAMESPACE_END
