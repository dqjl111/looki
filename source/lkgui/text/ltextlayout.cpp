#include "ltextlayout.h"
#include "lkgui/text/ltextengine.h"
#include "lkcore/ldebug.h"
#include "lkgui/text/lfontengine.h"
#include "lkgui/paint/lpainter.h"
#include "lkgui/text/lfontmetrics.h"
#include "lkcore/lglobal.h"
#include "lkgui/lguiapplication.h"
LK_NAMESPACE_BEGIN
//INT_MAX>>8
namespace {

struct LLineBreakHelper
{
    LLineBreakHelper(){

    }


    LScriptLine tmpData;
    LScriptLine spaceData;

    LGlyphLayout glyphs;

    int glyphCount = 0;
    int maxGlyphs = 0;
    int currentPosition = 0;
    uint32_t previousGlyph = 0;
    std::shared_ptr<LFontEngine> previousGlyphFontEngine;

    double minw = 0;
    double currentSoftHyphenWidth = 0;
    double commitedSoftHyphenWidth = 0;
    double rightBearing = 0;
    double minimumRightBearing = 0;

    std::shared_ptr<LFontEngine> fontEngine;
    const unsigned short *logClusters = nullptr;

    bool manualWrap =false;
    bool whiteSpaceOrObject = true;

    bool checkFullOtherwiseExtend(LScriptLine &line);

    double calculateNewWidth(const LScriptLine &line) const {
        return line.textWidth + tmpData.textWidth + spaceData.textWidth
               + (line.textWidth > 0 ? currentSoftHyphenWidth : 0) + negativeRightBearing();
    }

    inline uint32_t currentGlyph() const
    {
        return glyphs.glyphs[logClusters[currentPosition - 1]];
    }

    inline void saveCurrentGlyph()
    {
        previousGlyph = 0;
        if (currentPosition > 0 &&
            logClusters[currentPosition - 1] < glyphs.numGlyphs) {
            previousGlyph = currentGlyph(); // needed to calculate right bearing later
            previousGlyphFontEngine = fontEngine;
        }
    }

    inline void calculateRightBearing(std::shared_ptr<LFontEngine> engine, uint32_t glyph)
    {
        LGlyphMetrics<double>  gi = engine->boundingBox(glyph);
        rightBearing = std::min<double>(gi.rightBearing(), 0);
    }

    inline void calculateRightBearing()
    {
        if (currentPosition <= 0){
            return;
        }
        calculateRightBearing(fontEngine, currentGlyph());
    }

    inline void calculateRightBearingForPreviousGlyph()
    {
        if (previousGlyph > 0){
            calculateRightBearing(previousGlyphFontEngine, previousGlyph);
        }
    }

    static const double rightBearingNotCalculated;

    inline void resetRightBearing()
    {
        rightBearing = rightBearingNotCalculated;
    }

    // We express the negative right bearing as an absolute number
    // so that it can be applied to the width using addition.
    inline double negativeRightBearing() const
    {
        if (lFuzzyIsZero(rightBearing - rightBearingNotCalculated)){
            return 0;
        }
        return std::abs(rightBearing);
    }
};

const double LLineBreakHelper::rightBearingNotCalculated = 1;

inline bool LLineBreakHelper::checkFullOtherwiseExtend(LScriptLine &line)
{
    double newWidth = calculateNewWidth(line);
    if (line.length && !manualWrap && (newWidth > line.width || glyphCount > maxGlyphs)){
        return true;
    }

    const double oldTextWidth = line.textWidth;
    line += tmpData;
    line.textWidth += spaceData.textWidth;

    line.length += spaceData.length;
    tmpData.textWidth = 0;
    tmpData.length = 0;
    spaceData.textWidth = 0;
    spaceData.length = 0;

    if (oldTextWidth != line.textWidth || currentSoftHyphenWidth > 0) {
        commitedSoftHyphenWidth = currentSoftHyphenWidth;
        currentSoftHyphenWidth = 0;
    }
    return false;
}

} // anonymous namespace

static void addSelectedRegionsToPath(std::shared_ptr<LTextEngine> eng,
                                     int lineNumber,
                                     const LPoint<double> &pos,
                                     LTextLayout::FormatRange *selection,
                                     std::shared_ptr<LPainterPath> region,
                                     const LRect<double> &boundingRect)
{
    const LScriptLine &line = eng->m_lines[lineNumber];

    LTextLineItemIterator iterator(eng, lineNumber, pos, selection);

    const double selectionY = pos.m_y + line.y;
    const double lineHeight = line.height();

    double lastSelectionX = iterator.x;
    double lastSelectionWidth = 0;

    while (!iterator.atEnd()) {
        iterator.next();

        double selectionX = 0, selectionWidth = 0;
        if (iterator.getSelectionBounds(&selectionX, &selectionWidth)) {
            if (selectionX == lastSelectionX + lastSelectionWidth) {
                lastSelectionWidth += selectionWidth;
                continue;
            }

            if (lastSelectionWidth > 0) {
                const LRect<double> rect = boundingRect & LRect<double>(lastSelectionX, selectionY, lastSelectionWidth, lineHeight, false);
                region->addRect(rect.toAlignedRect().toRectF());
            }

            lastSelectionX = selectionX;
            lastSelectionWidth = selectionWidth;
        }
    }
    if (lastSelectionWidth > 0) {
        const LRect<double> rect = boundingRect & LRect<double>(lastSelectionX, selectionY, lastSelectionWidth, lineHeight, false);
        region->addRect(rect.toAlignedRect().toRectF());
    }
}

static inline LRect<double> clipIfValid(const LRect<double> &rect, const LRect<double> &clip)
{
    return clip.isValid() ? (rect & clip) : rect;
}


static inline void addNextCluster(int &pos, int end, LScriptLine &line, int &glyphCount,
                                  const LScriptItem &current, const unsigned short *logClusters,
                                  const LGlyphLayout &glyphs, double *clusterWidth = nullptr)
{
    int glyphPosition = logClusters[pos];
    do { // got to the first next cluster
        ++pos;
        ++line.length;
    } while (pos < end && logClusters[pos] == glyphPosition); //可能是4字节字符用的同一glypth
    double clusterWid = line.textWidth;
    do { // calculate the textWidth for the rest of the current cluster.
        if (!glyphs.attributes[glyphPosition].dontPrint){
            line.textWidth += glyphs.advances[glyphPosition];
        }
        ++glyphPosition;
    } while (glyphPosition < current.numGlyphs && !glyphs.attributes[glyphPosition].clusterStart);

    if (clusterWidth){
        *clusterWidth += (line.textWidth - clusterWid);
    }
    ++glyphCount;
}


void LTextLine::setLineWidth(double width)
{
    if(m_engine == nullptr){
        return;
    }

    LScriptLine &line = m_engine->m_lines[index];
    if (m_engine->m_layoutData == nullptr) {
        lWarn() << "LTextLine: Can't set a line width while not layouting.";
        return;
    }

    if (width > LK_TEXT_LINE_MAX_WIDTH){
        width = LK_TEXT_LINE_MAX_WIDTH;
    }

    line.width = width;
    if (line.length
        && line.textWidth <= line.width
        && line.from + line.length == m_engine->m_layoutData->m_string.length()){
        // no need to do anything if the line is already layouted and the last one. This optimization helps
        // when using things in a single line layout.
        return;
    }
    line.length = 0;
    line.textWidth = 0;

    layoutHelper(INT_MAX);
}

void LTextLine::setPosition(const LPoint<double> &pos)
{
    if(m_engine == nullptr){
        return;
    }
    m_engine->m_lines[index].x = pos.m_x;
    m_engine->m_lines[index].y = pos.m_y;

}

LPoint<double> LTextLine::position() const
{
    if(m_engine == nullptr){
        return LPoint<double>();
    }
    return LPoint<double>(m_engine->m_lines[index].x, m_engine->m_lines[index].y);
}

void LTextLine::setNumColumns(int numColumns)
{
    if(m_engine == nullptr){
        return;
    }
    LScriptLine &line = m_engine->m_lines[index];
    line.width = LK_TEXT_LINE_MAX_WIDTH;
    line.length = 0;
    line.textWidth = 0;
    layoutHelper(numColumns);
}

void LTextLine::setNumColumns(int numColumns, double alignmentWidth)
{
    if(m_engine == nullptr){
        return;
    }
    LScriptLine &line = m_engine->m_lines[index];
    line.width = alignmentWidth;
    line.length = 0;
    line.textWidth = 0;
    layoutHelper(numColumns);
}

int LTextLine::textStart() const
{
    if(m_engine == nullptr){
        return 0;
    }
    return m_engine->m_lines[index].from;
}

int LTextLine::textLength() const
{
    if(m_engine == nullptr){
        return 0;
    }
    return m_engine->m_lines[index].length + m_engine->m_lines[index].trailingSpaces;
}

LRect<double> LTextLine::rect() const
{
    if(m_engine == nullptr){
        return LRect<double>();
    }
    const LScriptLine& sl = m_engine->m_lines[index];
    return LRect<double>(sl.x, sl.y, sl.width, sl.height(), false);
}

double LTextLine::x() const
{
    if(m_engine == nullptr){
        return 0;
    }
    return m_engine->m_lines[index].x;
}

double LTextLine::y() const
{
    if(m_engine == nullptr){
        return 0;
    }
    return m_engine->m_lines[index].y;
}

double LTextLine::width() const
{
    if(m_engine == nullptr){
        return 0;
    }
    return m_engine->m_lines[index].width;
}

double LTextLine::ascent() const
{
    if(m_engine == nullptr){
        return 0;
    }
    return m_engine->m_lines[index].ascent;
}

double LTextLine::descent() const
{
    if(m_engine == nullptr){
        return 0;
    }
    return m_engine->m_lines[index].descent;
}

double LTextLine::height() const
{
    if(m_engine == nullptr){
        return 0;
    }
    return std::ceil(m_engine->m_lines[index].height());
}

double LTextLine::leading() const
{
    if(m_engine == nullptr){
        return 0;
    }
    return m_engine->m_lines[index].leading;
}

double LTextLine::naturalTextWidth() const
{
    if(m_engine == nullptr){
        return 0;
    }
    return m_engine->m_lines[index].textWidth;
}

LRect<double> LTextLine::naturalTextRect() const
{
    if(m_engine == nullptr){
        return LRect<double>();
    }
    const LScriptLine& sl = m_engine->m_lines[index];
    double x = sl.x + m_engine->alignLine(sl);

    double width = sl.textWidth;
    if (sl.justified){
        width = sl.width;
    }
    return LRect<double>(x, sl.y, width, sl.height(), false);
}

double LTextLine::horizontalAdvance() const
{
    if(m_engine == nullptr){
        return 0;
    }
    return m_engine->m_lines[index].textAdvance;
}

static void setPenAndDrawBackground(LPainter *p,
                                    const std::shared_ptr<LPen> defaultPen,
                                    const LTextCharFormat &chf,
                                    const LRect<double> &r)
{
    std::shared_ptr<LBrush> c = chf.foreground();
    if (c->m_style == LBrush::NoPattern) {
        p->setPen(defaultPen, false);
    }

    std::shared_ptr<LBrush> bg = chf.background();
    if (bg->m_style != LBrush::NoPattern && !chf.property(LTextFormat::SuppressBackground).toBool()){
        p->fillRect(r.toAlignedRect().toRectF(), bg);
    }
    if (c->m_style != LBrush::NoPattern) {
        std::shared_ptr<LPen> pen = std::make_shared<LPen>(c, 0);
        p->setPen(pen, false);
    }
}

void LTextLine::draw(LPainter *p, const LPoint<double> &pos, const LTextLayout::FormatRange *selection) const
{
    if(m_engine == nullptr){
        return;
    }

    const LScriptLine &line = m_engine->m_lines[index];
    std::shared_ptr<LPen> oldPen = p->pen();
//    lWarn() << "Pen color:" << oldPen->m_brush->m_color.name();

    bool noText = (selection && selection->format.property(LTextFormat::SuppressText).toBool());

    if (!line.length) {
        if (selection
            && selection->start <= line.from
            && selection->start + selection->length > line.from) {

            const double lineHeight = line.height();
            LRect<double> r(pos.m_x + line.x, pos.m_y + line.y,
                            lineHeight/2, LFontMetrics(m_engine->font()).horizontalAdvance(wchar_t(' ')), false);
            setPenAndDrawBackground(p, nullptr, selection->format, r);
            p->setPen(oldPen, false);
        }
        return;
    }

//    lWarn() << "pos: " << pos.m_x << ", " << pos.m_y;

    LTextLineItemIterator iterator(m_engine, index, pos, selection);
    double lineBase = line.base();
    m_engine->clearDecorations();
    m_engine->enableDelayDecorations();

    const double y = pos.m_y + line.y + lineBase;

    bool suppressColors = (m_engine->m_option.m_flags & LTextOption::SuppressColors);
    while (!iterator.atEnd()) {
        LScriptItem &si = iterator.next();

        if (selection && selection->start >= 0 && iterator.isOutsideSelection()){
            continue;
        }

        if (si.analysis.flags == LScriptAnalysis::LineOrParagraphSeparator
            && !(m_engine->m_option.m_flags & LTextOption::ShowLineAndParagraphSeparators)){
            continue;
        }

        double itemBaseLine = y;
        std::shared_ptr<LFont> f = m_engine->font();
        LTextCharFormat format;


        if (m_engine->hasFormats() || selection) {
            format = m_engine->format(&si);
            if (suppressColors) {
                format.clearForeground();
                format.clearBackground();
                format.clearProperty(LTextFormat::TextUnderlineColor);
            }
            if (selection != nullptr){
                format.merge(selection->format);
            }

            setPenAndDrawBackground(p, oldPen, format, LRect<double>(iterator.x, (y - lineBase),
                                                           iterator.itemWidth, line.height(), false));

            LTextCharFormat::VerticalAlignment valign = format.verticalAlignment();
            if (valign == LTextCharFormat::AlignSuperScript || valign == LTextCharFormat::AlignSubScript) {
                std::shared_ptr<LFontEngine> fe = f->engine();
                double height = fe->ascent() + fe->descent();
                if (valign == LTextCharFormat::AlignSubScript){
                    itemBaseLine += height / 6;
                }
                else if (valign == LTextCharFormat::AlignSuperScript){
                    itemBaseLine -= height / 2;
                }
            }
        }

        if (si.analysis.flags >= LScriptAnalysis::TabOrObject) {

            if (m_engine->hasFormats()) {
                p->save();
                std::shared_ptr<LFont> f = m_engine->font(si);
                LTextItem gf(&si, f, format);
                gf.chars = nullptr;
                gf.numChars = 0;
                gf.width = iterator.itemWidth;
                p->drawTextItem(LPoint<double>(iterator.x, y), gf, m_engine);
                if (m_engine->m_option.m_flags & LTextOption::ShowTabsAndSpaces) {
                    wchar_t visualTab(0x2192);
                    double w = LFontMetrics(f).horizontalAdvance(visualTab);
                    double x = iterator.itemWidth - w; // Right-aligned
                    if (x < 0){
                        p->setClipRect(LRect<double>(iterator.x, line.y,
                                              iterator.itemWidth, line.height(), false),
                                       LClipOper::Intersect);
                    }else{
                        x /= 2; // Centered
                    }
                    p->setFont(f);
                    p->drawText(LPoint<double>(iterator.x + x, y), std::wstring(&visualTab, 1));
                }
                p->restore();
            }

            continue;
        }

        const uint16_t *logClusters = m_engine->logClusters(&si);
        LGlyphLayout glyphs = m_engine->shapedGlyphs(&si);
        LTextItem gf(glyphs.mid(iterator.glyphsStart, iterator.glyphsEnd - iterator.glyphsStart),
                        f, m_engine->m_layoutData->m_string.data() + iterator.itemStart,
                        iterator.itemEnd - iterator.itemStart, m_engine->fontEngine(), format);
        gf.logClusters = logClusters + iterator.itemStart - si.position;
        gf.width = iterator.itemWidth;
        gf.justified = line.justified;
        gf.initWithScriptItem(&si);

        LPoint<double> pos(iterator.x, itemBaseLine);

        std::shared_ptr<LPen> pen = format.penProperty(LTextFormat::TextOutline);
        // pen->m_style = LPenStyle::Solid;
        if (pen->m_style != LPenStyle::No) {
            std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
            path->setFillRule(LFillRule::Winding);

            if (gf.glyphs.numGlyphs){
                gf.fontEngine->addOutlineToPath(pos.m_x, pos.m_y, gf.glyphs, path.get(), gf.flags);
            }
            if (gf.flags) {
                const std::shared_ptr<LFontEngine> fe = gf.fontEngine;
                const double lw = fe->lineThickness();
                if (gf.flags & LTextRenderHint::Underline) {
                    double offs = fe->underlinePosition();
                    path->addRect(pos.m_x, pos.m_y + offs, gf.width, lw);
                }
                if (gf.flags & LTextRenderHint::Overline) {
                    double offs = fe->ascent() + 1;
                    path->addRect(pos.m_x, pos.m_y- offs, gf.width, lw);
                }
                if (gf.flags & LTextRenderHint::StrikeOut) {
                    double offs = fe->ascent() / 3;
                    path->addRect(pos.m_x, pos.m_y - offs, gf.width, lw);
                }
            }

            p->save();
            p->setRenderHints(LRenderHint::Antialiasing);
            //Currently LPen with a NoPen style still returns a default
            //LBrush which != NoPattern so we need this specialcase to reset it
            pen = p->pen();
            if (pen->m_style == LPenStyle::No){
                p->setBrush(LBrush::NoPattern);
            }else{
                p->setBrush(pen->brush(), false);
            }

            p->setPen(format.textOutline());
            p->drawPath(path);
            p->restore();
        } else {
            if (noText){
                gf.glyphs.numGlyphs = 0; // slightly less elegant than it should be
            }
            p->drawTextItem(pos, gf, m_engine);
        }

        if ((si.analysis.flags == LScriptAnalysis::Space
             || si.analysis.flags == LScriptAnalysis::Nbsp)
            && (m_engine->m_option.m_flags & LTextOption::ShowTabsAndSpaces)) {
            std::shared_ptr<LBrush> c = format.foreground();
            if (c->m_style != LBrush::NoPattern){
                p->setPen(c->m_color);
            }

            wchar_t visualSpace(si.analysis.flags == LScriptAnalysis::Space ? (uint16_t)0xb7 : (uint16_t)0xb0);
            std::shared_ptr<LFont> oldFont = p->font();
            p->setFont(m_engine->font(si));
            p->drawText(LPoint<double>(iterator.x, itemBaseLine), std::wstring(&visualSpace, 1));
            p->setPen(oldPen, false);
            p->setFont(oldFont);
        }
    }
    m_engine->drawDecorations(p);

    if (m_engine->hasFormats()){
        p->setPen(oldPen, false);
    }
}

int LTextLine::xToCursor(double x, CursorPosition cpos) const
{
    if(m_engine == nullptr){
        return 0;
    }

    const LScriptLine &line = m_engine->m_lines[index];
    bool lastLine = index >= ((int)m_engine->m_lines.size() - 1);
    int lineNum = index;

    if (m_engine->m_layoutData == nullptr){
        m_engine->itemize();
    }

    int lineLength = textLength();

    if (!lineLength){
        return line.from;
    }

    int firstItem = m_engine->findItem(line.from);
    int lastItem = m_engine->findItem(line.from + lineLength - 1, firstItem);
    int nItems = (firstItem >= 0 && lastItem >= firstItem)? (lastItem-firstItem+1) : 0;

    if (!nItems){
        return 0;
    }

    x -= line.x;
    x -= m_engine->alignLine(line);

    std::vector<int> visualOrder(nItems);
    std::vector<unsigned char> levels(nItems);
    for (int i = 0; i < nItems; ++i){
        levels[i] = m_engine->m_layoutData->m_items[i+firstItem].analysis.bidiLevel;
    }
    LTextEngine::bidiReorder(nItems, levels.data(), visualOrder.data());

    bool visual = m_engine->m_visualMovement;
    if (x <= 0) {
        // left of first item
        int item = visualOrder[0]+firstItem;
        LScriptItem &si = m_engine->m_layoutData->m_items[item];
        if (!si.numGlyphs){
            m_engine->shape(item);
        }
        int pos = si.position;
        if (si.analysis.bidiLevel % 2){
            pos += m_engine->length(item);
        }
        pos = std::max<int>(line.from, pos);
        pos = std::min<int>(line.from + lineLength, pos);
        return pos;
    } else if (x < line.textWidth
               || (line.justified && x < line.width)) {
        // has to be in one of the runs
        double pos = 0;
        bool rtl = m_engine->isRightToLeft();

        m_engine->shapeLine(line);
        const auto insertionPoints = (visual && rtl) ? m_engine->insertionPointsForLine(lineNum) : std::vector<int>();
        int nchars = 0;
        for (int i = 0; i < nItems; ++i) {
            int item = visualOrder[i]+firstItem;
            LScriptItem &si = m_engine->m_layoutData->m_items[item];
            int itemLength = m_engine->length(item);

            int start = std::max<int>(line.from - si.position, 0);
            int end = std::min<int>(line.from + lineLength - si.position, itemLength);

            unsigned short *logClusters = m_engine->logClusters(&si);

            int gs = logClusters[start];
            int ge = (end == itemLength ? si.numGlyphs : logClusters[end]) - 1;
            LGlyphLayout glyphs = m_engine->shapedGlyphs(&si);

            double itemWidth = 0;
            if (si.analysis.flags >= LScriptAnalysis::TabOrObject) {
                itemWidth = si.width;
            } else {
                int g = gs;
                while (g <= ge) {
                    itemWidth += glyphs.effectiveAdvance(g);
                    ++g;
                }
            }

            if (pos + itemWidth < x) {
                pos += itemWidth;
                nchars += end;
                continue;
            }

            if (si.analysis.flags >= LScriptAnalysis::TabOrObject) {
                if (cpos == LTextLine::CursorOnCharacter){
                    return si.position;
                }
                bool leftHalf = (x - pos) < itemWidth/2;

                if (bool(si.analysis.bidiLevel % 2) != leftHalf){
                    return si.position;
                }
                return si.position + 1;
            }

            int glyphPos = -1;
            double edge;
            // has to be inside run
            if (cpos == LTextLine::CursorOnCharacter) {
                if (si.analysis.bidiLevel % 2) {
                    pos += itemWidth;
                    glyphPos = gs;
                    while (gs <= ge) {
                        if (glyphs.attributes[gs].clusterStart) {
                            if (pos < x){
                                break;
                            }
                            glyphPos = gs;
                            edge = pos;
                        }
                        pos -= glyphs.effectiveAdvance(gs);
                        ++gs;
                    }
                } else {
                    glyphPos = gs;
                    while (gs <= ge) {
                        if (glyphs.attributes[gs].clusterStart) {
                            if (pos > x){
                                break;
                            }
                            glyphPos = gs;
                            edge = pos;
                        }
                        pos += glyphs.effectiveAdvance(gs);
                        ++gs;
                    }
                }
            } else {
                double dist = LK_TEXT_LINE_MAX_WIDTH;
                if (si.analysis.bidiLevel % 2) {
                    if (!visual || rtl || (lastLine && i == nItems - 1)) {
                        pos += itemWidth;
                        while (gs <= ge) {
                            if (glyphs.attributes[gs].clusterStart && std::abs(x-pos) < dist) {
                                glyphPos = gs;
                                edge = pos;
                                dist = std::abs(x-pos);
                            }
                            pos -= glyphs.effectiveAdvance(gs);
                            ++gs;
                        }
                    } else {
                        while (ge >= gs) {
                            if (glyphs.attributes[ge].clusterStart && std::abs(x-pos) < dist) {
                                glyphPos = ge;
                                edge = pos;
                                dist = std::abs(x-pos);
                            }
                            pos += glyphs.effectiveAdvance(ge);
                            --ge;
                        }
                    }
                } else {
                    if (!visual || !rtl || (lastLine && i == 0)) {
                        while (gs <= ge) {
                            if (glyphs.attributes[gs].clusterStart && std::abs(x-pos) < dist) {
                                glyphPos = gs;
                                edge = pos;
                                dist = std::abs(x-pos);
                            }
                            pos += glyphs.effectiveAdvance(gs);
                            ++gs;
                        }
                    } else {
                        double oldPos = pos;
                        while (gs <= ge) {
                            pos += glyphs.effectiveAdvance(gs);
                            if (glyphs.attributes[gs].clusterStart && std::abs(x-pos) < dist) {
                                glyphPos = gs;
                                edge = pos;
                                dist = std::abs(x-pos);
                            }
                            ++gs;
                        }
                        pos = oldPos;
                    }
                }
                if (std::abs(x-pos) < dist) {
                    if (visual) {
                        if (!rtl && i < nItems - 1) {
                            nchars += end;
                            continue;
                        }
                        if (rtl && nchars > 0){
                            return insertionPoints[size_t(lastLine ? nchars : nchars - 1)];
                        }
                    }
                    return m_engine->positionInLigature(&si, end, x, pos, -1, cpos == CursorOnCharacter);
                }
            }
            //std::assert(glyphPos != -1)
            return m_engine->positionInLigature(&si, end, x, edge, glyphPos, cpos == CursorOnCharacter);
        }
    }
    // right of last item
    int item = visualOrder[nItems-1]+firstItem;
    LScriptItem &si = m_engine->m_layoutData->m_items[item];
    if (!si.numGlyphs){
        m_engine->shape(item);
    }
    int pos = si.position;
    if (!(si.analysis.bidiLevel % 2)){
        pos += m_engine->length(item);
    }
    pos = std::max<int>(line.from, pos);

    int maxPos = line.from + lineLength;

    // except for the last line we assume that the
    // character between lines is a space and we want
    // to position the cursor to the left of that
    // character.
    if (index < ((int)m_engine->m_lines.size() - 1)){
        maxPos = m_engine->previousLogicalPosition(maxPos);
    }

    pos = std::min<int>(pos, maxPos);
    return pos;
}

double LTextLine::cursorToX(int *cursorPos, Edge edge) const
{
    if(m_engine == nullptr){
        return 0;
    }

    const LScriptLine &line = m_engine->m_lines[index];
    bool lastLine = index >= ((int)m_engine->m_lines.size() - 1);

    double x = line.x + m_engine->alignLine(line) - m_engine->leadingSpaceWidth(line);

    if (!m_engine->m_layoutData){
        m_engine->itemize();
    }
    if (!m_engine->m_layoutData->m_items.size()) {
        *cursorPos = line.from;
        return x;
    }

    int lineEnd = line.from + line.length + line.trailingSpaces;
    int pos = lBound(line.from, *cursorPos, lineEnd);
    int itm;
    const LCharAttributes *attributes = m_engine->attributes();
    if (!attributes) {
        *cursorPos = line.from;
        return x;
    }
    while (pos < lineEnd && !attributes[pos].graphemeBoundary){
        pos++;
    }
    if (pos == lineEnd) {
        // end of line ensure we have the last item on the line
        itm = m_engine->findItem(pos-1);
    }
    else{
        itm = m_engine->findItem(pos);
    }
    if (itm < 0) {
        *cursorPos = line.from;
        return x;
    }
    m_engine->shapeLine(line);

    const LScriptItem *si = &m_engine->m_layoutData->m_items[itm];
    if (!si->numGlyphs){
        m_engine->shape(itm);
    }

    const int l = m_engine->length(itm);
    pos = lBound(0, pos - si->position, l);

    LGlyphLayout glyphs = m_engine->shapedGlyphs(si);
    unsigned short *logClusters = m_engine->logClusters(si);
    //std::assert(logClusters);

    int glyphPos = pos == l ? si->numGlyphs : logClusters[pos];
    if (edge == Trailing && glyphPos < si->numGlyphs) {
        // trailing edge is leading edge of next cluster
        glyphPos++;
        while (glyphPos < si->numGlyphs && !glyphs.attributes[glyphPos].clusterStart){
            glyphPos++;
        }
    }

    bool reverse = si->analysis.bidiLevel % 2;
    // add the items left of the cursor

    int firstItem = m_engine->findItem(line.from);
    int lastItem = m_engine->findItem(lineEnd - 1, itm);
    int nItems = (firstItem >= 0 && lastItem >= firstItem)? (lastItem-firstItem+1) : 0;

    std::vector<int> visualOrder(nItems);
    std::vector<uint8_t> levels(nItems);
    for (int i = 0; i < nItems; ++i){
        levels[i] = m_engine->m_layoutData->m_items[i+firstItem].analysis.bidiLevel;
    }
    LTextEngine::bidiReorder(nItems, levels.data(), visualOrder.data());

    for (int i = 0; i < nItems; ++i) {
        int item = visualOrder[i]+firstItem;
        if (item == itm){
            break;
        }
        LScriptItem &si = m_engine->m_layoutData->m_items[item];
        if (!si.numGlyphs){
            m_engine->shape(item);
        }

        if (si.analysis.flags >= LScriptAnalysis::TabOrObject) {
            x += si.width;
            continue;
        }

        const int itemLength = m_engine->length(item);
        int start = std::max(line.from, si.position);
        int end = std::min(lineEnd, si.position + itemLength);

        logClusters = m_engine->logClusters(&si);

        int gs = logClusters[start-si.position];
        int ge = (end == si.position + itemLength) ? si.numGlyphs-1 : logClusters[end-si.position-1];

        LGlyphLayout glyphs = m_engine->shapedGlyphs(&si);

        while (gs <= ge) {
            x += glyphs.effectiveAdvance(gs);
            ++gs;
        }
    }

    logClusters = m_engine->logClusters(si);
    glyphs = m_engine->shapedGlyphs(si);
    if (si->analysis.flags >= LScriptAnalysis::TabOrObject) {
        if (pos == (reverse ? 0 : l)){
            x += si->width;
        }
    } else {
        bool rtl = m_engine->isRightToLeft();
        bool visual = m_engine->m_visualMovement;
        int end = std::min(lineEnd, si->position + l) - si->position;
        if (reverse) {
            int glyphEnd = end == l ? si->numGlyphs : logClusters[end];
            int glyphStart = glyphPos;
            if (visual && !rtl && !(lastLine && itm == (visualOrder[nItems - 1] + firstItem))){
                glyphStart++;
            }
            for (int i = glyphEnd - 1; i >= glyphStart; i--){
                x += glyphs.effectiveAdvance(i);
            }
            x -= m_engine->offsetInLigature(si, pos, end, glyphPos);
        } else {
            int start = std::max(line.from - si->position, 0);
            int glyphStart = logClusters[start];
            int glyphEnd = glyphPos;
            if (!visual || !rtl || (lastLine && itm == visualOrder[0] + firstItem)){
                glyphEnd--;
            }
            for (int i = glyphStart; i <= glyphEnd; i++){
                x += glyphs.effectiveAdvance(i);
            }
            x += m_engine->offsetInLigature(si, pos, end, glyphPos);
        }
    }

    if (m_engine->m_option.m_wordWrap != LTextOption::NoWrap && x > line.x + line.width){
        x = line.x + line.width;
    }
    if (m_engine->m_option.m_wordWrap != LTextOption::NoWrap && x < 0){
        x = 0;
    }

    *cursorPos = pos + si->position;
    return x;
}

void LTextLine::layoutHelper(int maxGlyphs)
{
    if(m_engine == nullptr){
        return;
    }

    LScriptLine &line = m_engine->m_lines[index];
    line.length = 0;
    line.trailingSpaces = 0;
    line.textWidth = 0;
    line.hasTrailingSpaces = false;

    if (m_engine->m_layoutData->m_items.empty() || line.from >= (int)m_engine->m_layoutData->m_string.length()) {
        line.setDefaultHeight(m_engine);
        return;
    }

    LLineBreakHelper lbh;
    lbh.maxGlyphs = maxGlyphs;

    LTextOption::WrapMode wrapMode = (LTextOption::WrapMode)m_engine->m_option.m_wordWrap;
    bool breakany = (wrapMode == LTextOption::WrapAnywhere);
    const bool breakWordOrAny = breakany || (wrapMode == LTextOption::WrapAtWordBoundaryOrAnywhere);
    lbh.manualWrap = (wrapMode == LTextOption::ManualWrap || wrapMode == LTextOption::NoWrap);

    int item = -1;
    int newItem = m_engine->findItem(line.from);

    LFlags<LAlignment> alignment = m_engine->m_option.m_align;

    const LCharAttributes *attributes = m_engine->attributes();
    if (attributes == nullptr){
        return;
    }
    lbh.currentPosition = line.from;
    int end = 0;
    lbh.logClusters = m_engine->m_layoutData->m_logClustersPtr;
    lbh.previousGlyph = 0;

    //bool hasInlineObject = false;
    double maxInlineObjectHeight = 0;

    while (newItem>=0 && newItem < (int)m_engine->m_layoutData->m_items.size()) {
        lbh.resetRightBearing();
        if (newItem != item) {
            item = newItem;
            const LScriptItem &current = m_engine->m_layoutData->m_items.at(item);
            if (!current.numGlyphs) {
                m_engine->shape(item);
                attributes = m_engine->attributes();
                if (attributes == nullptr){
                    return;
                }
                lbh.logClusters = m_engine->m_layoutData->m_logClustersPtr;
            }
            lbh.currentPosition = std::max<int>(line.from, current.position);
            end = current.position + m_engine->length(item);
            lbh.glyphs = m_engine->shapedGlyphs(&current);
            std::shared_ptr<LFontEngine> fontEngine = m_engine->fontEngine();
            if (lbh.fontEngine != fontEngine) {
                lbh.fontEngine = fontEngine;
                lbh.minimumRightBearing = std::min<double>(0,
                                               fontEngine->minRightBearing());
            }
        }
        const LScriptItem &current = m_engine->m_layoutData->m_items.at(item);

        lbh.tmpData.leading = std::max<double>(lbh.tmpData.leading + lbh.tmpData.ascent,
                                   current.leading + current.ascent)- std::max<double>(lbh.tmpData.ascent, current.ascent);

        if (current.analysis.flags != LScriptAnalysis::Object) {
            // objects need some special treatment as they can special alignment or be floating
            lbh.tmpData.ascent = std::max<double>(lbh.tmpData.ascent, current.ascent);
            lbh.tmpData.descent = std::max<double>(lbh.tmpData.descent, current.descent);
        }

        if (current.analysis.flags == LScriptAnalysis::Tab && (alignment &
                                                               (LAlignment::Left
                                                                | LAlignment::Right
                                                                | LAlignment::Center
                                                                | LAlignment::Justify))) {
            lbh.whiteSpaceOrObject = true;
            if (lbh.checkFullOtherwiseExtend(line)){
                goto found;
            }

            double x = line.x + line.textWidth + lbh.tmpData.textWidth + lbh.spaceData.textWidth;
            double tabWidth = m_engine->calculateTabWidth(item, x);
            attributes = m_engine->attributes();
            if (!attributes){
                return;
            }
            lbh.logClusters = m_engine->m_layoutData->m_logClustersPtr;
            lbh.glyphs = m_engine->shapedGlyphs(&current);

            lbh.spaceData.textWidth += tabWidth;
            lbh.spaceData.length++;
            newItem = item + 1;

            double averageCharWidth = m_engine->fontEngine()->averageCharWidth();
            lbh.glyphCount += std::lround(tabWidth / averageCharWidth);

            if (lbh.checkFullOtherwiseExtend(line)){
                goto found;
            }
        } else if (current.analysis.flags == LScriptAnalysis::LineOrParagraphSeparator) {
            lbh.whiteSpaceOrObject = true;
            // if the line consists only of the line separator make sure
            // we have a sane height
            if (!line.length && !lbh.tmpData.length){
                line.setDefaultHeight(m_engine);
            }
            if (m_engine->m_option.m_flags & LTextOption::ShowLineAndParagraphSeparators) {
                if (lbh.checkFullOtherwiseExtend(line)){
                    goto found;
                }

                addNextCluster(lbh.currentPosition, end, lbh.tmpData, lbh.glyphCount,
                               current, lbh.logClusters, lbh.glyphs);
            } else {
                lbh.tmpData.length++;
                lbh.calculateRightBearingForPreviousGlyph();
            }
            line += lbh.tmpData;
            goto found;
        } else if (current.analysis.flags == LScriptAnalysis::Object) {
            lbh.whiteSpaceOrObject = true;
            lbh.tmpData.length++;

            // hasInlineObject = true;
            maxInlineObjectHeight = std::max<double>(maxInlineObjectHeight, current.ascent + current.descent);

            lbh.tmpData.textWidth += current.width;

            newItem = item + 1;
            ++lbh.glyphCount;
            if (lbh.checkFullOtherwiseExtend(line)){
                goto found;
            }
        } else if (attributes[lbh.currentPosition].whiteSpace
                   && LStringUtil::decompositionTag(m_engine->m_layoutData->m_string.at(lbh.currentPosition)) != lchar::NoBreak) {
            lbh.whiteSpaceOrObject = true;
            while (lbh.currentPosition < end
                   && attributes[lbh.currentPosition].whiteSpace
                   && LStringUtil::decompositionTag(m_engine->m_layoutData->m_string.at(lbh.currentPosition)) != lchar::NoBreak) {
                addNextCluster(lbh.currentPosition, end, lbh.spaceData, lbh.glyphCount,
                               current, lbh.logClusters, lbh.glyphs);
            }

            if (!lbh.manualWrap && lbh.spaceData.textWidth > line.width) {
                lbh.spaceData.textWidth = line.width; // ignore spaces that fall out of the line.
                goto found;
            }
        } else {
            lbh.whiteSpaceOrObject = false;
            bool sbOrWs = false;
            lbh.saveCurrentGlyph();
            double accumulatedTextWidth =0;
            do {
                addNextCluster(lbh.currentPosition, end, lbh.tmpData, lbh.glyphCount,
                               current, lbh.logClusters, lbh.glyphs, &accumulatedTextWidth);

                bool isBreakableSpace = lbh.currentPosition < (int)m_engine->m_layoutData->m_string.length()
                                        && attributes[lbh.currentPosition].whiteSpace
                                        && LStringUtil::decompositionTag(m_engine->m_layoutData->m_string.at(lbh.currentPosition)) != lchar::NoBreak;

                if (lbh.currentPosition >= (int)m_engine->m_layoutData->m_string.length()
                    || isBreakableSpace
                    || attributes[lbh.currentPosition].mandatoryBreak) {
                    //|| attributes[lbh.currentPosition].lineBreak) {
                    sbOrWs = true;
                    break;
                } else if (attributes[lbh.currentPosition].graphemeBoundary) {
                    if (breakWordOrAny) {
                        lbh.minw = std::max<double>(accumulatedTextWidth, lbh.minw);
                        accumulatedTextWidth = 0;
                    }
                    if (breakany){
                        break;
                    }
                }
            } while (lbh.currentPosition < end);
            lbh.minw = std::max<double>(accumulatedTextWidth, lbh.minw);

            if (lbh.currentPosition > 0 && lbh.currentPosition <= end
                && (lbh.currentPosition == end || attributes[lbh.currentPosition].lineBreak)
                && m_engine->m_layoutData->m_string.at(lbh.currentPosition - 1) == lchar::SoftHyphen) {
                // if we are splitting up a word because of
                // a soft hyphen then we ...
                //
                //  a) have to take the width of the soft hyphen into
                //     account to see if the first syllable(s) /and/
                //     the soft hyphen fit into the line
                //
                //  b) if we are so short of available width that the
                //     soft hyphen is the first breakable position, then
                //     we don't want to show it. However we initially
                //     have to take the width for it into account so that
                //     the text document layout sees the overflow and
                //     switch to break-anywhere mode, in which we
                //     want the soft-hyphen to slip into the next line
                //     and thus become invisible again.
                //
                lbh.currentSoftHyphenWidth = lbh.glyphs.advances[lbh.logClusters[lbh.currentPosition - 1]];
            }

            if (sbOrWs|breakany) {
                // To compute the final width of the text we need to take negative right bearing
                // into account (negative right bearing means the glyph has pixel data past the
                // advance length). Note that the negative right bearing is an absolute number,
                // so that we can apply it to the width using straight forward addition.

                // Store previous right bearing (for the already accepted glyph) in case we
                // end up breaking due to the current glyph being too wide.
                double previousRightBearing = lbh.rightBearing;

                // We skip calculating the right bearing if the minimum negative bearing is too
                // small to possibly expand the text beyond the edge. Note that this optimization
                // will in some cases fail, as the minimum right bearing reported by the font
                // engine may not cover all the glyphs in the font. The result is that we think
                // we don't need to break at the current glyph (because the right bearing is 0),
                // and when we then end up breaking on the next glyph we compute the right bearing
                // and end up with a line width that is slightly larger width than what was requested.
                // Unfortunately we can't remove this optimization as it will slow down text
                // layouting significantly, so we accept the slight correctnes issue.
                if ((lbh.calculateNewWidth(line) + std::abs(lbh.minimumRightBearing)) > line.width){
                    lbh.calculateRightBearing();
                }

                if (lbh.checkFullOtherwiseExtend(line)) {

                    // We are too wide to accept the next glyph with its bearing, so we restore the
                    // right bearing to that of the previous glyph (the one that was already accepted),
                    // so that the bearing can be be applied to the final width of the text below.
                    if (previousRightBearing != LLineBreakHelper::rightBearingNotCalculated){
                        lbh.rightBearing = previousRightBearing;
                    }else{
                        lbh.calculateRightBearingForPreviousGlyph();
                    }
                    line.textWidth += lbh.commitedSoftHyphenWidth;

                    goto found;
                }
            }
            lbh.saveCurrentGlyph();
        }
        if (lbh.currentPosition == end){
            newItem = item + 1;
        }
    }

    lbh.checkFullOtherwiseExtend(line);
    line.textWidth += lbh.commitedSoftHyphenWidth;
found:
    line.textAdvance = line.textWidth;

    // If right bearing has not been calculated yet, do that now
    if (lbh.rightBearing == LLineBreakHelper::rightBearingNotCalculated && !lbh.whiteSpaceOrObject){
        lbh.calculateRightBearing();
    }

    // Then apply any negative right bearing
    line.textWidth += lbh.negativeRightBearing();

    if (line.length == 0) {
        line += lbh.tmpData;
    }

    const double trailingSpace = (m_engine->m_option.m_flags & LTextOption::IncludeTrailingSpaces
                                      ? lbh.spaceData.textWidth
                                      : double(0));
    if (m_engine->m_option.m_wordWrap == LTextOption::WrapAtWordBoundaryOrAnywhere) {
        if ((lbh.maxGlyphs != INT_MAX && lbh.glyphCount > lbh.maxGlyphs)
            || (lbh.maxGlyphs == INT_MAX && line.textWidth > (line.width -  trailingSpace))) {

            m_engine->m_option.m_wordWrap = LTextOption::WrapAnywhere;
            layoutHelper(lbh.maxGlyphs);
            m_engine->m_option.m_wordWrap = LTextOption::WrapAtWordBoundaryOrAnywhere;
            return;
        }
    }

    if (lbh.manualWrap) {
        m_engine->m_minWidth = std::max<double>(m_engine->m_minWidth, line.textWidth);
        m_engine->m_maxWidth = std::max<double>(m_engine->m_maxWidth, line.textWidth);
    } else {
        m_engine->m_minWidth = std::max<double>(m_engine->m_minWidth, lbh.minw);
        m_engine->m_maxWidth += line.textWidth;
    }

    if (line.textWidth > 0 && item < (int)m_engine->m_layoutData->m_items.size()){
        m_engine->m_maxWidth += lbh.spaceData.textWidth;
    }

    line.textWidth += trailingSpace;
    if (lbh.spaceData.length) {
        line.trailingSpaces = lbh.spaceData.length;
        line.hasTrailingSpaces = true;
    }

    line.justified = false;
    line.gridfitted = false;
}


LTextLayout::LTextLayout(std::shared_ptr<LTextEngine> e)
{
    if(e == nullptr){
        m_engine = std::make_shared<LTextEngine>();
    }else{
        m_engine = e;
    }
}

const std::shared_ptr<LFont> LTextLayout::font() const
{
    return m_engine->font();
}

void LTextLayout::setFont(std::shared_ptr<LFont> f)
{
    m_engine->setFont(f);
}

void LTextLayout::beginLayout()
{
    m_engine->invalidate();
    m_engine->clearLineData();
    m_engine->itemize();
    m_engine->m_layoutData->m_layoutState = LTextEngine::InLayout;
}

void LTextLayout::endLayout()
{
    int l = m_engine->m_lines.size();
    if (l && m_engine->m_lines[l-1].length < 0) {
        LTextLine(l-1, m_engine).setNumColumns(INT_MAX);
    }
    m_engine->m_layoutData->m_layoutState = LTextEngine::LayoutEmpty;
    if (!m_engine->m_cacheGlyphs){
        m_engine->freeMemory();
    }
}

void LTextLayout::clearLayout()
{
    m_engine->clearLineData();
}

LTextLine LTextLayout::createLine()
{
    if (m_engine->m_layoutData->m_layoutState == LTextEngine::LayoutFailed){
        return LTextLine();
    }

    int l = m_engine->m_lines.size();
    int from = 0;
    int strlen = m_engine->m_layoutData->m_string.length();
    if (l>0){
        if(m_engine->m_lines[l-1].length < 0) {
            LTextLine(l-1, m_engine).setNumColumns(INT_MAX);
        }
        const LScriptLine& tLine = m_engine->m_lines[l-1];
        from = tLine.from + tLine.length + tLine.trailingSpaces;
        if (from >= strlen) {
            if (!tLine.length
                || m_engine->m_layoutData->m_string[strlen - 1] != lchar::LineSeparator){
                return LTextLine();
            }
        }
    }

    LScriptLine line;
    line.from = from;
    line.length = -1;
    line.justified = false;
    line.gridfitted = false;

    m_engine->m_lines.push_back(line);
    return LTextLine(l, m_engine);

}

int LTextLayout::lineCount() const
{
    return m_engine->m_lines.size();
}

LTextLine LTextLayout::lineAt(int i) const
{
    return i < (int)m_engine->m_lines.size() ? LTextLine(i, m_engine) : LTextLine();
}

LTextLine LTextLayout::lineForTextPosition(int pos) const
{
    int lineNum = m_engine->lineNumberForTextPosition(pos);
    return lineNum >= 0 ? lineAt(lineNum) : LTextLine();
}

int LTextLayout::lineNumberForTextPosition(int pos) const
{
    return m_engine->lineNumberForTextPosition(pos);
}

LPoint<double> LTextLayout::position() const
{
    return m_engine->m_position;
}

void LTextLayout::setPosition(const LPoint<double> &p)
{
    m_engine->m_position = p;
}

LRect<double> LTextLayout::boundingRect() const
{
    if (m_engine->m_lines.empty()){
        return LRect<double>();
    }

    double xmax = 0, ymax = 0;
    double xmin = m_engine->m_lines.at(0).x;
    double ymin = m_engine->m_lines.at(0).y;

    for (const LScriptLine &si : m_engine->m_lines) {
        xmin = std::min(xmin, si.x);
        ymin = std::min(ymin, si.y);
        double lineWidth = si.width < LK_TEXT_LINE_MAX_WIDTH ? std::max(si.width, si.textWidth) : si.textWidth;
        xmax = std::max(xmax, si.x+lineWidth);
        ymax = std::max(ymax, si.y+std::ceil(si.height()));
    }
    return LRect<double>(xmin, ymin, xmax, ymax, true);
}

double LTextLayout::minimumWidth() const
{
    return m_engine->m_minWidth;
}

double LTextLayout::maximumWidth() const
{
    return m_engine->m_maxWidth;
}

bool LTextLayout::isValidCursorPosition(int pos) const
{
    const LCharAttributes *attributes = m_engine->attributes();
    if (!attributes || pos < 0 || pos > (int)m_engine->m_layoutData->m_string.length()){
        return false;
    }
    return attributes[pos].graphemeBoundary;
}

int LTextLayout::nextCursorPosition(int oldPos, CursorMode mode) const
{
    const LCharAttributes *attributes = m_engine->attributes();
    int len =  m_engine->m_layoutData->m_string.length();
    if (!attributes || oldPos < 0 || oldPos >= len){
        return oldPos;
    }

    if (mode == SkipCharacters) {
        oldPos++;
        while (oldPos < len && !attributes[oldPos].graphemeBoundary)
            oldPos++;
    } else {
        if (oldPos < len && m_engine->atWordSeparator(oldPos)) {
            oldPos++;
            while (oldPos < len && m_engine->atWordSeparator(oldPos)){
                oldPos++;
            }
        } else {
            while (oldPos < len && !attributes[oldPos].whiteSpace && !m_engine->atWordSeparator(oldPos)){
                oldPos++;
            }
        }
        while (oldPos < len && attributes[oldPos].whiteSpace){
            oldPos++;
        }
    }

    return oldPos;
}

int LTextLayout::previousCursorPosition(int oldPos, CursorMode mode) const
{
    const LCharAttributes *attributes = m_engine->attributes();
    int len = m_engine->m_layoutData->m_string.length();
    if (!attributes || oldPos <= 0 || oldPos > len){
        return oldPos;
    }

    if (mode == SkipCharacters) {
        oldPos--;
        while (oldPos && !attributes[oldPos].graphemeBoundary){
            oldPos--;
        }
    } else {
        while (oldPos > 0 && attributes[oldPos - 1].whiteSpace){
            oldPos--;
        }

        if (oldPos && m_engine->atWordSeparator(oldPos-1)) {
            oldPos--;
            while (oldPos && m_engine->atWordSeparator(oldPos-1)){
                oldPos--;
            }
        } else {
            while (oldPos > 0 && !attributes[oldPos - 1].whiteSpace && !m_engine->atWordSeparator(oldPos-1)){
                oldPos--;
            }
        }
    }

    return oldPos;
}

int LTextLayout::leftCursorPosition(int oldPos) const
{
    int newPos = m_engine->positionAfterVisualMovement(oldPos, LMoveOper::Left);
    return newPos;
}

int LTextLayout::rightCursorPosition(int oldPos) const
{
    int newPos = m_engine->positionAfterVisualMovement(oldPos, LMoveOper::Right);
    return newPos;
}

void LTextLayout::setText(const std::wstring &string)
{
    m_engine->invalidate();
    m_engine->clearLineData();
    m_engine->m_text = string;
}

const std::wstring &LTextLayout::text() const
{
    return m_engine->m_text;
}

void LTextLayout::insertText(int pos, const std::wstring &v)
{
    m_engine->invalidate();
    m_engine->clearLineData();
    m_engine->m_text.insert(pos, v);
}

void LTextLayout::draw(LPainter *p,
                       const LPoint<double> &pos,
                       const std::vector<FormatRange> &selections,
                       const LRect<double> &clip) const
{
    if (m_engine->m_lines.empty()){
        return;
    }

    if (!m_engine->m_layoutData){
        m_engine->itemize();
    }

    LPoint<double> position = pos + m_engine->m_position;

    double clipy = (INT_MIN/256.0);
    double clipe = (INT_MAX/256.0);
    if (clip.isValid()) {
        clipy = clip.m_y - position.m_y;
        clipe = clipy + clip.height();
    }

    int firstLine = 0;
    int lastLine = (int)m_engine->m_lines.size();
    for (int i = 0; i < (int)m_engine->m_lines.size(); ++i) {
        const LScriptLine &sl = m_engine->m_lines.at(i);

        if (sl.y > clipe) {
            lastLine = i;
            break;
        }
        if ((sl.y + sl.height()) < clipy) {
            firstLine = i;
            continue;
        }
    }

    std::shared_ptr<LPainterPath> excludedRegion = std::make_shared<LPainterPath>();
    std::shared_ptr<LPainterPath> textDoneRegion  = std::make_shared<LPainterPath>();
    for (int i = 0; i < (int)selections.size(); ++i) {
        FormatRange selection = selections.at(i);
        std::shared_ptr<LPainterPath> region= std::make_shared<LPainterPath>();
        region->setFillRule(LFillRule::Winding);

        for (int line = firstLine; line < lastLine; ++line) {
            const LScriptLine &sl = m_engine->m_lines.at(line);
            LTextLine tl(line, m_engine);

            LRect<double> lineRect(tl.naturalTextRect());
            lineRect.translate(position);
            lineRect.adjust(0, 0, m_engine->leadingSpaceWidth(sl), 0);

            bool isLastLineInBlock = (line == m_engine->m_lines.size()-1);
            int sl_length = sl.length + (isLastLineInBlock? 1 : 0); // the infamous newline


            if (sl.from > selection.start + selection.length || sl.from + sl_length <= selection.start){
                continue; // no actual intersection
            }

            const bool selectionStartInLine = sl.from <= selection.start;
            const bool selectionEndInLine = selection.start + selection.length < sl.from + sl_length;

            if (sl.length && (selectionStartInLine || selectionEndInLine)) {
                addSelectedRegionsToPath(m_engine, line, position, &selection, region, clipIfValid(lineRect, clip));
            } else {
                region->addRect(clipIfValid(lineRect, clip));
            }

            if (selection.format.boolProperty(LTextFormat::FullWidthSelection)) {
                LRect<double> fullLineRect(tl.rect());
                fullLineRect.translate(position);
                fullLineRect.setX2(INT_MAX/256.0);
                if (!selectionEndInLine)
                    region->addRect(clipIfValid(LRect<double>(lineRect.topRight(), fullLineRect.bottomRight()), clip));
                if (!selectionStartInLine)
                    region->addRect(clipIfValid(LRect<double>(fullLineRect.topLeft(), lineRect.bottomLeft()), clip));
            } else if (!selectionEndInLine
                       && isLastLineInBlock
                       &&!(m_engine->m_option.m_flags & LTextOption::ShowLineAndParagraphSeparators)) {
                region->addRect(clipIfValid(LRect<double>(lineRect.x2(), lineRect.m_y,
                                                  lineRect.height()/4, lineRect.height(), false), clip));
            }

        }
        {
            const std::shared_ptr<LPen> oldPen = p->pen();
            const std::shared_ptr<LBrush> oldBrush = p->brush();

            p->setPen(selection.format.penProperty(LTextFormat::OutlinePen), false);
            p->setBrush(selection.format.brushProperty(LTextFormat::BackgroundBrush), false);
            p->drawPath(region);

            p->setPen(oldPen, false);
            p->setBrush(oldBrush, false);
        }


        std::shared_ptr<LBrush> foreground = selection.format.foreground();
        std::shared_ptr<LBrush> background = selection.format.background();

        bool hasText = (foreground->m_style != LBrush::NoPattern);
        bool hasBackground= (background->m_style != LBrush::NoPattern);

        if (hasBackground) {
            selection.format.setProperty(LTextFormat::ObjectSelectionBrush, selection.format.property(LTextFormat::BackgroundBrush));
            // don't just clear the property, set an empty brush that overrides a potential
            // background brush specified in the text
            std::shared_ptr<LBrush>  b = std::make_shared<LBrush>();
            selection.format.setProperty(LTextFormat::BackgroundBrush, LVariant(b));;
            selection.format.clearProperty(LTextFormat::OutlinePen);
        }

        selection.format.setProperty(LTextFormat::SuppressText, !hasText);

        if (hasText && !hasBackground){
            std::shared_ptr<LPainterPath>  t = textDoneRegion->intersected(region);
            if(!t->isEmpty()){
                continue;
            }
        }

        p->save();
        p->setClipPath(region, LClipOper::Intersect);

        for (int line = firstLine; line < lastLine; ++line) {
            LTextLine l(line, m_engine);
            l.draw(p, position, &selection);
        }
        p->restore();
        if (hasText) {
            textDoneRegion = textDoneRegion->united(region);
        } else {
            if (hasBackground){
                textDoneRegion = textDoneRegion->subtracted(region);
            }
        }

        excludedRegion = excludedRegion->united(region);
    }

    std::shared_ptr<LPainterPath> needsTextButNoBackground = excludedRegion->subtracted(textDoneRegion);
    if (!needsTextButNoBackground->isEmpty()){
        p->save();
        p->setClipPath(needsTextButNoBackground, LClipOper::Intersect);
        FormatRange selection;
        selection.start = 0;
        selection.length = INT_MAX;
        selection.format.setProperty(LTextFormat::SuppressBackground, true);
        for (int line = firstLine; line < lastLine; ++line) {
            LTextLine l(line, m_engine);
            l.draw(p, position, &selection);
        }
        p->restore();
    }

    if (!excludedRegion->isEmpty()) {
        p->save();
        std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
        LRect<double> br = boundingRect().translated(position);
        br.setX2(INT_MAX/256.0);
        if (!clip.isNull()){
            br = br.intersected(clip);
        }
        path->addRect(br);
        path = path->subtracted(excludedRegion);
        p->setClipPath(path, LClipOper::Intersect);
    }

    for (int i = firstLine; i < lastLine; ++i) {
        LTextLine l(i, m_engine);
        l.draw(p, position);
    }
    if (!excludedRegion->isEmpty()){
        p->restore();
    }

    if (!m_engine->m_cacheGlyphs){
        m_engine->freeMemory();
    }
}

void LTextLayout::drawCursor(LPainter *p, const LPoint<double> &pos, int cursorPosition, int width) const
{
    if (m_engine->m_lines.empty()){
        return;
    }

    if (m_engine->m_layoutData == nullptr){
        m_engine->itemize();
    }

    LPoint<double> position = pos + m_engine->m_position;

    cursorPosition = lBound<int>(0, cursorPosition, m_engine->m_layoutData->m_string.length());
    int line = m_engine->lineNumberForTextPosition(cursorPosition);
    if (line < 0){
        line = 0;
    }
    if (line >= (int)m_engine->m_lines.size()){
        return;
    }

    LTextLine l(line, m_engine);
    const LScriptLine &sl = m_engine->m_lines.at(line);

    double x = position.m_x + l.cursorToX(cursorPosition);
    int itm = 0;

    if (m_engine->m_visualMovement) {
        if (cursorPosition == sl.from + sl.length){
            cursorPosition--;
        }
        itm = m_engine->findItem(cursorPosition);
    } else{
        itm = m_engine->findItem(cursorPosition - 1);
    }

    double base = sl.base();
    double descent = sl.descent;
    bool rightToLeft = m_engine->isRightToLeft();
    if (itm >= 0) {
        const LScriptItem &si = m_engine->m_layoutData->m_items.at(itm);
        if (si.ascent > 0){
            base = si.ascent;
        }
        if (si.descent > 0){
            descent = si.descent;
        }
        rightToLeft = si.analysis.bidiLevel % 2;
    }
    double y = position.m_y + (sl.y + sl.base() - base);
    bool toggleAntialiasing = !(p->renderHints() & LRenderHint::Antialiasing)
                              && (p->transform().type() > LTransform2D::TxTranslate);
    if (toggleAntialiasing){
        p->setRenderHints(LRenderHint::Antialiasing);
    }
    LCompositionMode origCompositionMode = p->compositionMode();
    if (p->engine()->hasFeature(LPaintEngineFeature::RasterOpModes)){
        p->setCompositionMode(LCompositionMode::RasterOpNotDestination);
    }
    p->fillRect(LRect<double>(x, y, width, (base + descent), false), p->pen()->brush());
    p->setCompositionMode(origCompositionMode);
    if (toggleAntialiasing){
        p->setRenderHints(LRenderHint::Antialiasing, false);
    }
    if (m_engine->m_layoutData->m_hasBidi) {
        const int arrow_extent = 4;
        int sign = rightToLeft ? -1 : 1;
        p->drawLine(LLine<double>(x, y, x + (sign * arrow_extent/2), y + arrow_extent/2));
        p->drawLine(LLine<double>(x, y+arrow_extent, x + (sign * arrow_extent/2), y + arrow_extent/2));
    }
    return;
}

void LTextLayout::setCacheEnabled(bool enable)
{
    m_engine->m_cacheGlyphs = enable;
}

bool LTextLayout::cacheEnabled() const
{
    return m_engine->m_cacheGlyphs;
}

LK_NAMESPACE_END
