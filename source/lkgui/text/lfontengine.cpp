#include "lfontengine.h"
#include "lkcore/io/lendian.h"
#include "lkcore/ldebug.h"
#include "lkgui/paint/lpainter.h"

LK_NAMESPACE_BEGIN
#define LkMinLeftSideBearingOffset 12
#define LkMinRightSideBearingOffset 14

#define GRID(x, y) grid[(y)*(w+1) + (x)]
#define SET(x, y) (*(image_data + (y)*bpl + ((x) >> 3)) & (0x80 >> ((x) & 7)))

enum { EdgeRight = 0x1,
       EdgeDown = 0x2,
       EdgeLeft = 0x4,
       EdgeUp = 0x8
};

static void collectSingleContour(double x0, double y0,
                                 uint32_t *grid,
                                 int x, int y, int w, int h,
                                 LPainterPath *path)
{
    path->moveTo(x + x0, y + y0);
    while (GRID(x, y)) {
        if (GRID(x, y) & EdgeRight) {
            while (GRID(x, y) & EdgeRight) {
                GRID(x, y) &= ~EdgeRight;
                ++x;
            }
            path->lineTo(x + x0, y + y0);
            continue;
        }
        if (GRID(x, y) & EdgeDown) {
            while (GRID(x, y) & EdgeDown) {
                GRID(x, y) &= ~EdgeDown;
                ++y;
            }
            path->lineTo(x + x0, y + y0);
            continue;
        }
        if (GRID(x, y) & EdgeLeft) {
            while (GRID(x, y) & EdgeLeft) {
                GRID(x, y) &= ~EdgeLeft;
                --x;
            }
            path->lineTo(x + x0, y + y0);
            continue;
        }
        if (GRID(x, y) & EdgeUp) {
            while (GRID(x, y) & EdgeUp) {
                GRID(x, y) &= ~EdgeUp;
                --y;
            }
            path->lineTo(x + x0, y + y0);
            continue;
        }
    }
    path->closeSubpath();
}

void lAddBitmapToPath(double x0, double y0, const uint8_t *image_data, int bpl, int w, int h, LPainterPath *path)
{
    uint32_t *grid = new uint32_t[(w+1)*(h+1)];
    // set up edges
    for (int y = 0; y <= h; ++y) {
        for (int x = 0; x <= w; ++x) {
            bool topLeft = (x == 0 || y == 0) ? false : SET(x - 1, y - 1);
            bool topRight = (x == w || y == 0) ? false : SET(x, y - 1);
            bool bottomLeft = (x == 0 || y == h) ? false : SET(x - 1, y);
            bool bottomRight = (x == w || y == h) ? false : SET(x, y);

            GRID(x, y) = 0;
            if ((!topRight) & bottomRight)
                GRID(x, y) |= EdgeRight;
            if ((!bottomRight) & bottomLeft)
                GRID(x, y) |= EdgeDown;
            if ((!bottomLeft) & topLeft)
                GRID(x, y) |= EdgeLeft;
            if ((!topLeft) & topRight)
                GRID(x, y) |= EdgeUp;
        }
    }

    // collect edges
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!GRID(x, y)){
                continue;
            }
            // found start of a contour, follow it
            collectSingleContour(x0, y0, grid, x, y, w, h, path);
        }
    }
    delete [] grid;
}

#undef GRID
#undef SET




template<typename T>
static inline bool lSafeFromBigEndian(const uint8_t *source, const uint8_t *end, T *output)
{
    if (source + sizeof(T) > end){
        return false;
    }

    *output = lFromBigEndian<T>(source);
    return true;
}

class LGlyphLayoutArray : private std::vector<void *>, public LGlyphLayout
{
private:
    typedef std::vector<void *> Array;
public:
    LGlyphLayoutArray(int totalGlyphs)
        : Array((totalGlyphs * SpaceNeeded) / sizeof(void *) + 1)
        , LGlyphLayout(reinterpret_cast<char *>(Array::data()), totalGlyphs)
    {
        memset(Array::data(), 0, Array::size() * sizeof(void *));
    }

    void resize(int totalGlyphs)
    {
        Array::resize((totalGlyphs * SpaceNeeded) / sizeof(void *) + 1);

        *((LGlyphLayout *)this) = LGlyphLayout(reinterpret_cast<char *>(Array::data()), totalGlyphs);
        memset(Array::data(), 0, Array::size() * sizeof(void *));
    }
};

LFontEngine::LFontEngine(Type _type)
    :m_minLeftBearing(std::numeric_limits<double>::max()),
    m_minRightBearing(std::numeric_limits<double>::max()),
    m_type(_type)
{
}

double LFontEngine::minLeftBearing() const
{
    if (m_minLeftBearing == std::numeric_limits<double>::max()){
        minRightBearing(); // Initializes both (see below)
    }
    return m_minLeftBearing;
}

double LFontEngine::minRightBearing() const
{
    if (m_minRightBearing == std::numeric_limits<double>::max()) {

        // Try the 'hhea' font table first, which covers the entire font
        std::vector<uint8_t>  hheaTable = getSfntTable(MAKE_TAG_REVERSE('h', 'h', 'e', 'a'));
        if (hheaTable.size() >= int(LkMinRightSideBearingOffset + sizeof(int16_t))) {
            const uint8_t *tableData = hheaTable.data();

            int16_t minLeftSideBearing = lFromBigEndian<int16_t>(tableData + LkMinLeftSideBearingOffset);
            int16_t minRightSideBearing = lFromBigEndian<int16_t>(tableData + LkMinRightSideBearingOffset);

            // The table data is expressed as FUnits, meaning we have to take the number
            // of units per em into account. Since pixelSize already has taken DPI into
            // account we can use that directly instead of the point size.
            double unitsPerEm = emSquareSize();
            double funitToPixelFactor = m_fontDef.pixelSize / unitsPerEm;

            // Some fonts on OS X (such as Gurmukhi Sangam MN, Khmer MN, Lao Sangam MN, etc.), have
            // invalid values for their NBSPACE left bearing, causing the 'hhea' minimum bearings to
            // be way off. We detect this by assuming that the minimum bearsings are within a certain
            // range of the em square size.
            static const int largestValidBearing = 4 * unitsPerEm;

            if (std::abs(minLeftSideBearing) < largestValidBearing){
                m_minLeftBearing = minLeftSideBearing * funitToPixelFactor;
            }
            if (std::abs(minRightSideBearing) < largestValidBearing){
                m_minRightBearing = minRightSideBearing * funitToPixelFactor;
            }
        }

        // Fallback in case of missing 'hhea' table (bitmap fonts e.g.) or broken 'hhea' values
        if (m_minLeftBearing == std::numeric_limits<double>::max()
            || m_minRightBearing == std::numeric_limits<double>::max()) {

            // To balance performance and correctness we only look at a subset of the
            // possible glyphs in the font, based on which characters are more likely
            // to have a left or right bearing.
            static const uint16_t characterSubset[] = {
                '(', 'C', 'F', 'K', 'V', 'X', 'Y', ']', '_', 'f', 'r', '|',
                127, 205, 645, 884, 922, 1070, 12386
            };

            // The font may have minimum bearings larger than 0, so we have to start at the max
            m_minLeftBearing = m_minRightBearing = std::numeric_limits<double>::max();

            for (uint32_t i = 0; i < (sizeof(characterSubset) / sizeof(uint16_t)); ++i) {
                const uint32_t glyph = glyphIndex(characterSubset[i]);
                if (glyph==0){
                    continue;
                }

                LGlyphMetrics<double> glyphMetrics = const_cast<LFontEngine *>(this)->boundingBox(glyph);

                // Glyphs with no contours shouldn't contribute to bearings
                if (lFuzzyIsZero(glyphMetrics.m_width) || lFuzzyIsZero(glyphMetrics.m_height)){
                    continue;
                }

                m_minLeftBearing = std::min<double>(m_minLeftBearing, glyphMetrics.leftBearing());
                m_minRightBearing = std::min<double>(m_minRightBearing, glyphMetrics.rightBearing());
            }
        }

        if (m_minLeftBearing == std::numeric_limits<double>::max()
            || m_minRightBearing == std::numeric_limits<double>::max()){
            lWarn() << "Failed to compute left/right minimum bearings for" << m_fontDef.family;
        }
    }
    return m_minRightBearing;
}

LGlyphMetrics<double> LFontEngine::boundingBox(uint32_t glyph, const LTransform2D &matrix)
{
    LGlyphMetrics<double> metrics = boundingBox(glyph);

    if (matrix.type() > LTransform2D::TxTranslate) {
        return metrics.transformed(matrix);
    }
    return metrics;
}

bool LFontEngine::supportsTransformation(const LTransform2D &transform) const
{
    return transform.type() < LTransform2D::TxProject;
}

void LFontEngine::addGlyphsToPath(uint32_t *glyphs, LPoint<double> *positions, int nglyphs,
                                  LPainterPath *path, LFlags<LTextRenderHint> flags)
{
    double x = positions[0].m_x;
    double y = positions[0].m_y;
    LGlyphLayoutArray g(nglyphs);

    for (int i = 0; i < nglyphs - 1; ++i) {
        g.glyphs[i] = glyphs[i];
        g.advances[i] = positions[i + 1].m_x - positions[i].m_x;
    }
    g.glyphs[nglyphs - 1] = glyphs[nglyphs - 1];
    g.advances[nglyphs - 1] = maxCharWidth();

    addBitmapFontToPath(x, y, g, path, flags);
}

void LFontEngine::getGlyphPositions(const LGlyphLayout &glyphs,
                                    const LTransform2D &matrix,
                                    LFlags<LTextRenderHint> flags,
                                    std::vector<uint32_t> &glyphsOut,
                                    std::vector<LPoint<double> > &positions)
{
//    for(int i=0; i<glyphs.numGlyphs; ++i){
//        lWarn() << "In glyphs:" << glyphs.glyphs[i];
//    }

    double xpos = 0;
    double ypos = 0;

    const bool transform = matrix.m11() != 1.
                           || matrix.m12() != 0.
                           || matrix.m21() != 0.
                           || matrix.m22() != 1.;
    if (!transform) {
        xpos = matrix.dx();
        ypos = matrix.dy();
    }

    int current = 0;
    if (flags & LTextRenderHint::RightToLeft) {
        int i = glyphs.numGlyphs;
        int totalKashidas = 0;
        while(i--) {
            if (glyphs.attributes[i].dontPrint){
                continue;
            }
            xpos += glyphs.advances[i] + glyphs.justifications[i].space;
            totalKashidas += glyphs.justifications[i].nKashidas;
        }
        positions.resize(glyphs.numGlyphs+totalKashidas);
        glyphsOut.resize(glyphs.numGlyphs+totalKashidas);

        i = 0;
        while(i < glyphs.numGlyphs) {
            if (glyphs.attributes[i].dontPrint) {
                ++i;
                continue;
            }
            xpos -= glyphs.advances[i];

            double gpos_x = xpos + glyphs.offsets[i].m_x;
            double gpos_y = ypos + glyphs.offsets[i].m_y;
            if (transform) {
                LPoint<double> gpos(gpos_x, gpos_y);
                gpos = gpos * matrix;
                gpos_x = gpos.m_x;
                gpos_y = gpos.m_y;
            }
            positions[current].m_x = gpos_x;
            positions[current].m_y = gpos_y;
            glyphsOut[current] = glyphs.glyphs[i];

            ++current;
            if (glyphs.justifications[i].nKashidas) {
                wchar_t ch(0x640); // Kashida character

                uint32_t kashidaGlyph = glyphIndex(ch);
                double kashidaWidth;

                LGlyphLayout g;
                g.numGlyphs = 1;
                g.glyphs = &kashidaGlyph;
                g.advances = &kashidaWidth;
                recalcAdvances(&g, { });

                for (uint32_t k = 0; k < glyphs.justifications[i].nKashidas; ++k) {
                    xpos -= kashidaWidth;

                    double gpos_x = xpos + glyphs.offsets[i].m_x;
                    double gpos_y = ypos + glyphs.offsets[i].m_y;
                    if (transform) {
                        LPoint<double> gpos(gpos_x, gpos_y);
                        gpos = gpos * matrix;
                        gpos_x = gpos.m_x;
                        gpos_y = gpos.m_y;
                    }
                    positions[current].m_x = gpos_x;
                    positions[current].m_y = gpos_y;
                    glyphsOut[current] = kashidaGlyph;
                    ++current;
                }
            } else {
                xpos -= glyphs.justifications[i].space;
            }
            ++i;
        }
    } else {
        positions.resize(glyphs.numGlyphs);
        glyphsOut.resize(glyphs.numGlyphs);
        int i = 0;
        if (!transform) {
            while (i < glyphs.numGlyphs) {
                if (!glyphs.attributes[i].dontPrint) {
                    positions[current].m_x = xpos + glyphs.offsets[i].m_x;
                    positions[current].m_y = ypos + glyphs.offsets[i].m_y;
                    glyphsOut[current] = glyphs.glyphs[i];
                    xpos += glyphs.advances[i] + glyphs.justifications[i].space;
                    ++current;
                }
                ++i;
            }
        } else {
            while (i < glyphs.numGlyphs) {
                if (!glyphs.attributes[i].dontPrint) {
                    double gpos_x = xpos + glyphs.offsets[i].m_x;
                    double gpos_y = ypos + glyphs.offsets[i].m_y;
                    LPoint<double> gpos(gpos_x, gpos_y);
                    gpos = gpos * matrix;
                    positions[current].m_x = gpos.m_x;
                    positions[current].m_y = gpos.m_y;
                    glyphsOut[current] = glyphs.glyphs[i];
                    xpos += glyphs.advances[i] + glyphs.justifications[i].space;
                    ++current;
                }
                ++i;
            }
        }
    }
    positions.resize(current);
    glyphsOut.resize(current);
}

std::vector<uint8_t> LFontEngine::getSfntTable(uint32_t tag) const
{
    std::vector<uint8_t> table;
    uint32_t len = 0;
    if (!getSfntTableData(tag, nullptr, &len)){
        return table;
    }
    table.resize(len);
    if (!getSfntTableData(tag, table.data(), &len)){
        return std::vector<uint8_t>();
    }
    return table;
}

bool LFontEngine::getSfntTableData(uint32_t tag, uint8_t *buffer, uint32_t *length) const
{
    return false;
}

std::shared_ptr<LGlyph> LFontEngine::glyphData(uint32_t glyph,
                                               double subPixelPosition,
                                               LGlyphFormat neededFormat,
                                               const LTransform2D &t)
{
    return nullptr;
}

double LFontEngine::lineThickness() const
{
    int score = m_fontDef.weight * m_fontDef.pixelSize;
    int lw = score / 700;
    if (lw <= 0){
        return 1;
    }
    if (lw < 2 && score >= 1050){
        return 2;
    }
    return lw;
}

double LFontEngine::underlinePosition() const
{
    return ((lineThickness() * 2) + 3) / 6;
}

void LFontEngine::getGlyphBearings(uint32_t glyph, double *leftBearing, double *rightBearing)
{
    LGlyphMetrics<double>  gi = boundingBox(glyph);
    if (leftBearing != nullptr){
        *leftBearing = gi.leftBearing();
    }
    if (rightBearing != nullptr){
        *rightBearing = gi.rightBearing();
    }
}

double LFontEngine::averageCharWidth() const
{
    const uint32_t glyph = glyphIndex('x');
    LGlyphMetrics<double> bb = const_cast<LFontEngine *>(this)->boundingBox(glyph);
    return bb.m_xoff;
}

void LFontEngine::addOutlineToPath(double x, double y,
                                   const LGlyphLayout &glyphs,
                                   LPainterPath *path,
                                   LFlags<LTextRenderHint> flags)
{
    if (!glyphs.numGlyphs){
        return;
    }

    std::vector<LPoint<double>> positions;
    std::vector<uint32_t> positionedGlyphs;
    LTransform2D matrix = LTransform2D::fromTranslate(x, y);
    getGlyphPositions(glyphs, matrix, flags, positionedGlyphs, positions);
    addGlyphsToPath(positionedGlyphs.data(), positions.data(), positionedGlyphs.size(), path, flags);

}

void LFontEngine::addBitmapFontToPath(double x, double y,
                                      const LGlyphLayout &glyphs,
                                      LPainterPath *path,
                                      LFlags<LTextRenderHint> flags)
{
    double advanceX = x;
    double advanceY = y;
    for (int i=0; i < glyphs.numGlyphs; ++i) {
        LGlyphMetrics<double> metrics = boundingBox(glyphs.glyphs[i]);
        if (lFuzzyIsZero(metrics.m_width) || lFuzzyIsZero(metrics.m_height)) {
            advanceX += glyphs.advances[i];
            continue;
        }
        const std::shared_ptr<LImage> alphaMask = alphaMapForGlyph(glyphs.glyphs[i]);
        if(alphaMask == nullptr){
            continue;
        }

        const int w = alphaMask->width();
        const int h = alphaMask->height();
        const int srcBpl = alphaMask->bytesPerLine();
        std::shared_ptr<LImage> bitmap;
        if (alphaMask->depth() == 1) {
            bitmap = alphaMask;
        } else {
            bitmap = std::make_shared<LImage>(w, h, LImage::Format::Mono);
            const uint8_t *imageData = alphaMask->bits();
            const int destBpl = bitmap->bytesPerLine();
            uint8_t *bitmapData = bitmap->bits();

            for (int yi = 0; yi < h; ++yi) {
                const uint8_t *src = imageData + yi*srcBpl;
                uint8_t *dst = bitmapData + yi*destBpl;
                for (int xi = 0; xi < w; ++xi) {
                    const int byte = xi / 8;
                    const int bit = xi % 8;
                    if (bit == 0){
                        dst[byte] = 0;
                    }
                    if (src[xi]){
                        dst[byte] |= 128 >> bit;
                    }
                }
            }
        }
        const uint8_t *bitmap_data = bitmap->bits();
        LPoint<double> offset = glyphs.offsets[i];
        advanceX += offset.m_x;
        advanceY += offset.m_y;
        lAddBitmapToPath((advanceX + metrics.m_x),
                           (advanceY + metrics.m_y),
                           bitmap_data,
                           bitmap->bytesPerLine(), w, h, path);
        advanceX += glyphs.advances[i];
    }

}

std::shared_ptr<LImage> LFontEngine::alphaMapForGlyph(uint32_t glyph)
{
    LGlyphMetrics<double> gm = boundingBox(glyph);
    int glyph_x = std::floor(gm.m_x);
    int glyph_y = std::floor(gm.m_y);
    int glyph_width = std::ceil(gm.m_x + gm.m_width) -  glyph_x;
    int glyph_height = std::ceil(gm.m_y + gm.m_height) - glyph_y;

    if (glyph_width <= 0 || glyph_height <= 0){
        return nullptr;
    }
    LPoint<double> pt;
    pt.m_x = -glyph_x;
    pt.m_y = -glyph_y; // the baseline
    std::shared_ptr<LPainterPath> path = std::make_shared<LPainterPath>();
    path->setFillRule(LFillRule::Winding);
    std::shared_ptr<LImage> im
        = std::make_shared<LImage>(glyph_width, glyph_height, LImage::Format::ARGB32Premultiplied);
    im->fill(LColorID::Transparent);
    LPainter p(im);
    p.setRenderHints(LRenderHint::Antialiasing);
    addGlyphsToPath(&glyph, &pt, 1, path.get(), { });
    p.setPen(LPenStyle::No);
    p.setBrush(LColor(LColorID::Black));
    p.drawPath(path);
    p.end();

    std::shared_ptr<LImage> alphaMap
        = std::make_shared<LImage>(im->width(), im->height(), LImage::Format::Alpha8);

    for (int y=0; y<im->height(); ++y) {
        uint8_t *dst = (uint8_t *) alphaMap->scanLine(y);
        const uint32_t *src = reinterpret_cast<const uint32_t *>(im->scanLine(y));
        for (int x=0; x<im->width(); ++x){
            dst[x] = LColor::uintToAlpha(src[x]);
        }
    }

    return alphaMap;
}

std::shared_ptr<LImage> LFontEngine::bitmapForGlyph(uint32_t glyph,
                                                    double subPixelPosition,
                                                    const LTransform2D &t,
                                                    const LColor &color)
{
    return nullptr;
}


void LFontEngine::loadKerningPairs(double scalingFactor)
{
    m_kerningPairs.clear();

    std::vector<uint8_t> tab = getSfntTable(MAKE_TAG_REVERSE('k', 'e', 'r', 'n'));
    if (tab.empty()){
        return;
    }

    const uint8_t *table = reinterpret_cast<const uint8_t *>(tab.data());
    const uint8_t *end = table + tab.size();

    uint16_t version;
    if (!lSafeFromBigEndian(table, end, &version)){
        return;
    }

    if (version != 0) {
        return;
    }

    uint16_t numTables;
    if (!lSafeFromBigEndian(table + 2, end, &numTables))
        return;

    {
        int offset = 4;
        for(int i = 0; i < numTables; ++i) {
            const uint8_t *header = table + offset;

            uint16_t version;
            if (!lSafeFromBigEndian(header, end, &version)){
                goto end;
            }

            uint16_t length;
            if (!lSafeFromBigEndian(header + 2, end, &length)){
                goto end;
            }

            uint16_t coverage;
            if (!lSafeFromBigEndian(header + 4, end, &coverage)){
                goto end;
            }

            //            qDebug("subtable: version=%d, coverage=%x",version, coverage);
            if(version == 0 && coverage == 0x0001) {
                if (offset + length > tab.size()) {
                    //                    qDebug("length ouf ot bounds");
                    goto end;
                }
                const uint8_t *data = table + offset + 6;

                uint16_t nPairs;
                if (!lSafeFromBigEndian(data, end, &nPairs))
                    goto end;

                if(nPairs * 6 + 8 > length - 6) {
                    //                    qDebug("corrupt table!");
                    // corrupt table
                    goto end;
                }

                int off = 8;
                for(int i = 0; i < nPairs; ++i) {
                    LFontEngine::KernPair p;

                    uint16_t tmp;
                    if (!lSafeFromBigEndian(data + off, end, &tmp)){
                        goto end;
                    }

                    p.left_right = uint32_t(tmp) << 16;
                    if (!lSafeFromBigEndian(data + off + 2, end, &tmp)){
                        goto end;
                    }

                    p.left_right |= tmp;

                    if (!lSafeFromBigEndian(data + off + 4, end, &tmp)){
                        goto end;
                    }

                    p.adjust = tmp / scalingFactor;
                    m_kerningPairs.push_back(p);
                    off += 6;
                }
            }
            offset += length;
        }
    }
end:
    sort(m_kerningPairs.begin(), m_kerningPairs.end());

}

const uint8_t *LFontEngine::getCMap(const uint8_t *table, uint32_t tableSize,
                                    bool *isSymbolFont, int *cmapSize)
{
    const uint8_t *header = table;
    const uint8_t *endPtr = table + tableSize;

    // version check
    uint16_t version;
    if (!lSafeFromBigEndian(header, endPtr, &version) || version != 0){
        return nullptr;
    }

    uint16_t numTables;
    if (!lSafeFromBigEndian(header + 2, endPtr, &numTables)){
        return nullptr;
    }

    const uint8_t *maps = table + 4;

    enum {
        Invalid,
        AppleRoman,
        Symbol,
        Unicode11,
        Unicode,
        MicrosoftUnicode,
        MicrosoftUnicodeExtended
    };

    int symbolTable = -1;
    int tableToUse = -1;
    int score = Invalid;
    for (int n = 0; n < numTables; ++n) {
        uint16_t platformId;
        if (!lSafeFromBigEndian(maps + 8 * n, endPtr, &platformId)){
            return nullptr;
        }

        uint16_t platformSpecificId = 0;
        if (!lSafeFromBigEndian(maps + 8 * n + 2, endPtr, &platformSpecificId)){
            return nullptr;
        }

        switch (platformId) {
        case 0:{ // Unicode
            if (score < Unicode &&
                (platformSpecificId == 0 ||
                 platformSpecificId == 2 ||
                 platformSpecificId == 3)) {
                tableToUse = n;
                score = Unicode;
            } else if (score < Unicode11 && platformSpecificId == 1) {
                tableToUse = n;
                score = Unicode11;
            }
            break;
        }
        case 1:{ // Apple
            if (score < AppleRoman && platformSpecificId == 0) { // Apple Roman
                tableToUse = n;
                score = AppleRoman;
            }
            break;
        }
        case 3:{ // Microsoft
            switch (platformSpecificId) {
            case 0:{
                symbolTable = n;
                if (score < Symbol) {
                    tableToUse = n;
                    score = Symbol;
                }
                break;
            }
            case 1:{
                if (score < MicrosoftUnicode) {
                    tableToUse = n;
                    score = MicrosoftUnicode;
                }
                break;
            }
            case 0xa:{
                if (score < MicrosoftUnicodeExtended) {
                    tableToUse = n;
                    score = MicrosoftUnicodeExtended;
                }
                break;
            }
            default:{
                break;
            }
            }
        }
        default:
            break;
        }
    }
    if(tableToUse < 0)
        return nullptr;

resolveTable:
    *isSymbolFont = (symbolTable > -1);

    uint32_t unicode_table = 0;
    if (!lSafeFromBigEndian(maps + 8 * tableToUse + 4, endPtr, &unicode_table)){
        return nullptr;
    }

    if (!unicode_table){
        return nullptr;
    }

    // get the header of the unicode table
    header = table + unicode_table;

    uint16_t format;
    if (!lSafeFromBigEndian(header, endPtr, &format)){
        return nullptr;
    }

    uint32_t length;
    if (format < 8) {
        uint16_t tmp;
        if (!lSafeFromBigEndian(header + 2, endPtr, &tmp)){
            return nullptr;
        }
        length = tmp;
    } else {
        if (!lSafeFromBigEndian(header + 4, endPtr, &length)){
            return nullptr;
        }
    }

    if (table + unicode_table + length > endPtr){
        return nullptr;
    }
    *cmapSize = length;

    // To support symbol fonts that contain a unicode table for the symbol area
    // we check the cmap tables and fall back to symbol font unless that would
    // involve losing information from the unicode table
    if (symbolTable > -1 && ((score == Unicode) || (score == Unicode11))) {
        const uint8_t *selectedTable = table + unicode_table;

        // Check that none of the latin1 range are in the unicode table
        bool unicodeTableHasLatin1 = false;
        for (int uc=0x00; uc<0x100; ++uc) {
            if (getTrueTypeGlyphIndex(selectedTable, length, uc) != 0) {
                unicodeTableHasLatin1 = true;
                break;
            }
        }

        // Check that at least one symbol char is in the unicode table
        bool unicodeTableHasSymbols = false;
        if (!unicodeTableHasLatin1) {
            for (int uc=0xf000; uc<0xf100; ++uc) {
                if (getTrueTypeGlyphIndex(selectedTable, length, uc) != 0) {
                    unicodeTableHasSymbols = true;
                    break;
                }
            }
        }

        // Fall back to symbol table
        if (!unicodeTableHasLatin1 && unicodeTableHasSymbols) {
            tableToUse = symbolTable;
            score = Symbol;
            goto resolveTable;
        }
    }

    return table + unicode_table;
}

uint32_t LFontEngine::getTrueTypeGlyphIndex(const uint8_t *cmap, int cmapSize, uint32_t unicode)
{
    const uint8_t *end = cmap + cmapSize;
    uint16_t format;
    if (!lSafeFromBigEndian(cmap, end, &format)){
        return 0;
    }

    if (format == 0) {
        const uint8_t *ptr = cmap + 6 + unicode;
        if (unicode < 256 && ptr < end){
            return uint32_t(*ptr);
        }
    } else if (format == 4) {
        /* some fonts come with invalid cmap tables, where the last segment
           specified end = start = rangeoffset = 0xffff, delta = 0x0001
           Since 0xffff is never a valid Unicode char anyway, we just get rid of the issue
           by returning 0 for 0xffff
        */
        if(unicode >= 0xffff){
            return 0;
        }

        uint16_t segCountX2;
        if (!lSafeFromBigEndian(cmap + 6, end, &segCountX2)){
            return 0;
        }

        const unsigned char *ends = cmap + 14;

        int i = 0;
        for (; i < segCountX2/2; ++i) {
            uint16_t codePoint;
            if (!lSafeFromBigEndian(ends + 2 * i, end, &codePoint)){
                return 0;
            }
            if (codePoint >= unicode){
                break;
            }
        }

        const unsigned char *idx = ends + segCountX2 + 2 + 2*i;

        uint16_t startIndex;
        if (!lSafeFromBigEndian(idx, end, &startIndex)){
            return 0;
        }
        if (startIndex > unicode){
            return 0;
        }

        idx += segCountX2;

        uint16_t tmp;
        if (!lSafeFromBigEndian(idx, end, &tmp)){
            return 0;
        }
        int16_t idDelta = int16_t(tmp);

        idx += segCountX2;

        uint16_t idRangeoffset_t;
        if (!lSafeFromBigEndian(idx, end, &idRangeoffset_t)){
            return 0;
        }

        uint16_t glyphIndex;
        if (idRangeoffset_t) {
            uint16_t id;
            if (!lSafeFromBigEndian(idRangeoffset_t + 2 * (unicode - startIndex) + idx, end, &id)){
                return 0;
            }

            if (id){
                glyphIndex = (idDelta + id) % 0x10000;
            }else{
                glyphIndex = 0;
            }
        } else {
            glyphIndex = (idDelta + unicode) % 0x10000;
        }
        return glyphIndex;
    } else if (format == 6) {
        uint16_t tableSize;
        if (!lSafeFromBigEndian(cmap + 2, end, &tableSize)){
            return 0;
        }

        uint16_t firstCode6;
        if (!lSafeFromBigEndian(cmap + 6, end, &firstCode6)){
            return 0;
        }
        if (unicode < firstCode6){
            return 0;
        }

        uint16_t entryCount6;
        if (!lSafeFromBigEndian(cmap + 8, end, &entryCount6)){
            return 0;
        }
        if (entryCount6 * 2 + 10 > tableSize){
            return 0;
        }

        uint16_t sentinel6 = firstCode6 + entryCount6;
        if (unicode >= sentinel6){
            return 0;
        }

        uint16_t entryIndex6 = unicode - firstCode6;

        uint16_t index = 0;
        lSafeFromBigEndian(cmap + 10 + (entryIndex6 * 2), end, &index);
        return index;
    } else if (format == 12) {
        uint32_t nGroups;
        if (!lSafeFromBigEndian(cmap + 12, end, &nGroups)){
            return 0;
        }

        cmap += 16; // move to start of groups

        int left = 0, right = nGroups - 1;
        while (left <= right) {
            int middle = left + ( ( right - left ) >> 1 );

            uint32_t startCharCode;
            if (!lSafeFromBigEndian(cmap + 12 * middle, end, &startCharCode)){
                return 0;
            }

            if(unicode < startCharCode){
                right = middle - 1;
            }else {
                uint32_t endCharCode;
                if (!lSafeFromBigEndian(cmap + 12 * middle + 4, end, &endCharCode)){
                    return 0;
                }

                if (unicode <= endCharCode) {
                    uint32_t index;
                    if (!lSafeFromBigEndian(cmap + 12 * middle + 8, end, &index)){
                        return 0;
                    }

                    return index + unicode - startCharCode;
                }
                left = middle + 1;
            }
        }
    } else {
        lWarn() << "cmap table of format " << format << " not implemented";
    }

    return 0;
}

double LFontEngine::calculatedCapHeight() const
{
    const uint32_t glyph = glyphIndex('H');
    LGlyphMetrics<double> bb = const_cast<LFontEngine *>(this)->boundingBox(glyph);
    return bb.m_height;
}

LK_NAMESPACE_END
