#include "lwindowsfontengine.h"
#include "lkcore/ldebug.h"
#include "lkgui/paint/lglyphmetrics.h"
#include "lwindowsfontdatabase.h"
#include "lkcore/io/lendian.h"
#include "lkcore/lstringutil.h"
#include "lkcore/plugin/ldynamiclibrary.h"

LK_NAMESPACE_BEGIN

namespace {
#   pragma pack(1)

    struct OS2Table
    {
        uint16_t version;
        int16_t  avgCharWidth;
        uint16_t weightClass;
        uint16_t widthClass;
        uint16_t type;
        int16_t  subscriptXSize;
        int16_t  subscriptYSize;
        int16_t  subscriptXOffset;
        int16_t  subscriptYOffset;
        int16_t  superscriptXSize;
        int16_t  superscriptYSize;
        int16_t  superscriptXOffset;
        int16_t  superscriptYOffset;
        int16_t  strikeOutSize;
        int16_t  strikeOutPosition;
        int16_t  familyClass;
        uint8_t  panose[10];
        uint32_t unicodeRanges[4];
        uint8_t  vendorID[4];
        uint16_t selection;
        uint16_t firstCharIndex;
        uint16_t lastCharIndex;
        int16_t  typoAscender;
        int16_t  typoDescender;
        int16_t  typoLineGap;
        uint16_t winAscent;
        uint16_t winDescent;
        uint32_t codepageRanges[2];
        int16_t  height;
        int16_t  capHeight;
        uint16_t defaultChar;
        uint16_t breakChar;
        uint16_t maxContext;
    };
#   pragma pack()
}

static OUTLINETEXTMETRICW *getOutlineTextMetric(HDC hdc)
{
    const auto size = GetOutlineTextMetricsW(hdc, 0, nullptr);
    auto otm = reinterpret_cast<OUTLINETEXTMETRICW *>(malloc(size));
    GetOutlineTextMetricsW(hdc, size, otm);
    return otm;
}

std::shared_ptr<LWindowsFontEngineData> LWindowsFontEngine::m_data = nullptr;

LWindowsFontEngineData::LWindowsFontEngineData()
    : m_fontSmoothingGamma(LWindowsFontDatabase::fontSmoothingGamma())
{
    // from qapplication_win.cpp
    UINT result = 0;
    if (SystemParametersInfoW(SPI_GETFONTSMOOTHINGTYPE, 0, &result, 0)){
        m_clearTypeEnabled = (result == FE_FONTSMOOTHINGCLEARTYPE);
    }

    const double grayGamma = 2.31;
    for (int i=0; i<256; ++i){
        m_powGamma[i] = uint32_t(std::lround(std::pow(i / double(255.), grayGamma) * 2047));
    }

    HDC displayDC = GetDC(0);
    m_hdc = ::CreateCompatibleDC(displayDC);
    ReleaseDC(0, displayDC);
}

static inline double fixedToDouble(const FIXED &p) {
    return ((p.value << 16) + p.fract) / 65536.0;
}

static inline LPoint<double> pointFixToPointF(const POINTFX &pt, double scale, double stretch) {
    return LPoint<double>(fixedToDouble(pt.x) * scale * stretch, -fixedToDouble(pt.y) * scale);
}

static bool addGlyphToPath(uint32_t glyph, const LPoint<double> &position, HDC hdc,
                           LPainterPath *path, bool ttf, LGlyphMetrics<double> *metric = 0,
                           double scale = 1.0, double stretch = 1.0)
{
    MAT2 mat;
    mat.eM11.value = mat.eM22.value = 1;
    mat.eM11.fract = mat.eM22.fract = 0;
    mat.eM21.value = mat.eM12.value = 0;
    mat.eM21.fract = mat.eM12.fract = 0;

    GLYPHMETRICS gMetric;
    memset(&gMetric, 0, sizeof(GLYPHMETRICS));

    if (metric) {
        // If metrics requested, retrieve first using GGO_METRICS, because the returned
        // values are incorrect for OpenType PS fonts if obtained at the same time as the
        // glyph paths themselves (ie. with GGO_NATIVE as the format).
        uint32_t format = GGO_METRICS;
        if (ttf)
            format |= GGO_GLYPH_INDEX;
        if (GetGlyphOutline(hdc, glyph, format, &gMetric, 0, 0, &mat) == GDI_ERROR){
            return false;
        }
        // #### obey scale
        *metric = LGlyphMetrics<double>(gMetric.gmptGlyphOrigin.x, -gMetric.gmptGlyphOrigin.y,
                                  int(gMetric.gmBlackBoxX), int(gMetric.gmBlackBoxY),
                                  gMetric.gmCellIncX, gMetric.gmCellIncY);
    }

    uint32_t glyphFormat = GGO_NATIVE;

    if (ttf){
        glyphFormat |= GGO_GLYPH_INDEX;
    }

    const DWORD bufferSize = GetGlyphOutline(hdc, glyph, glyphFormat, &gMetric, 0, 0, &mat);
    if (bufferSize == GDI_ERROR){
        return false;
    }

    std::shared_ptr<char[]> dataBuffer= std::make_shared<char[]>(bufferSize);
    DWORD ret = GDI_ERROR;
    ret = GetGlyphOutline(hdc, glyph, glyphFormat, &gMetric, bufferSize, dataBuffer.get(), &mat);
    if (ret == GDI_ERROR) {
        return false;
    }

    DWORD offset = 0;
    DWORD headerOffset = 0;

    LPoint<double> oset = position;
    while (headerOffset < bufferSize) {
        const TTPOLYGONHEADER *ttph = reinterpret_cast<const TTPOLYGONHEADER *>(dataBuffer.get() + headerOffset);

        LPoint<double> lastPoint(pointFixToPointF(ttph->pfxStart, scale, stretch));
        path->moveTo(lastPoint + oset);
        offset += sizeof(TTPOLYGONHEADER);
        while (offset < headerOffset + ttph->cb) {
            const TTPOLYCURVE *curve = reinterpret_cast<const TTPOLYCURVE *>(dataBuffer.get() + offset);
            switch (curve->wType) {
            case TT_PRIM_LINE: {
                for (int i=0; i<curve->cpfx; ++i) {
                    LPoint<double> p = pointFixToPointF(curve->apfx[i], scale, stretch) + oset;
                    path->lineTo(p);
                }
                break;
            }
            case TT_PRIM_QSPLINE: {
                const LPainterPath::Element &elm = path->elementAt(path->elementCount()-1);
                LPoint<double> prev(elm.m_x, elm.m_y);
                LPoint<double> endPoint;
                for (int i=0; i<curve->cpfx - 1; ++i) {
                    LPoint<double> p1 = pointFixToPointF(curve->apfx[i], scale, stretch) + oset;
                    LPoint<double> p2 = pointFixToPointF(curve->apfx[i+1], scale, stretch) + oset;
                    if (i < curve->cpfx - 2) {
                        endPoint = LPoint<double>((p1.m_x + p2.m_x) / 2, (p1.m_y + p2.m_y) / 2);
                    } else {
                        endPoint = p2;
                    }

                    path->quadTo(p1, endPoint);
                    prev = endPoint;
                }

                break;
            }
            case TT_PRIM_CSPLINE: {
                for (int i=0; i<curve->cpfx; ) {
                    LPoint<double> p2 = pointFixToPointF(curve->apfx[i++], scale, stretch) + oset;
                    LPoint<double> p3 = pointFixToPointF(curve->apfx[i++], scale, stretch) + oset;
                    LPoint<double> p4 = pointFixToPointF(curve->apfx[i++], scale, stretch) + oset;
                    path->cubicTo(p2, p3, p4);
                }
                break;
            }
            default:{
                lWarn()<< "win addOutlineToPath, unhandled switch case";
            }
            }
            offset += sizeof(TTPOLYCURVE) + (curve->cpfx-1) * sizeof(POINTFX);
        }
        path->closeSubpath();
        headerOffset += ttph->cb;
    }
    return true;
}


typedef BOOL (WINAPI *PtrGetCharWidthI)(HDC, UINT, UINT, LPWORD, LPINT);
static PtrGetCharWidthI ptrGetCharWidthI = 0;
static bool resolvedGetCharWidthI = false;

static void resolveGetCharWidthI()
{
    if (resolvedGetCharWidthI){
        return;
    }
    resolvedGetCharWidthI = true;
    ptrGetCharWidthI = (PtrGetCharWidthI)LDynamicLibrary::resolve("gdi32", "GetCharWidthI");
}


LWindowsFontEngine::LWindowsFontEngine(const std::wstring &name, LOGFONTW lf)
    :LFontEngine(Win),
    m_name(name),
    m_logfont(lf),
    m_ttf(0),
    m_hasOutline(0)
{
    m_hfont = CreateFontIndirectW(&m_logfont);
    if (!m_hfont) {
        lWarn() << "CreateFontIndirect failed for family:" << m_name;
        m_hfont = LWindowsFontDatabase::systemFont();
    }
    sharedData();
    SelectObject(m_data->m_hdc, m_hfont);
    const BOOL res = GetTextMetricsW(m_data->m_hdc, &m_tm);
    if (!res) {
        lWarn() << "GetTextMetrics failed";
        ZeroMemory(&m_tm, sizeof(TEXTMETRICW));
    }

    m_fontDef.pixelSize = -lf.lfHeight;
    m_fontDef.fixedPitch = !(m_tm.tmPitchAndFamily & TMPF_FIXED_PITCH);

//    cache_cost = tm.tmHeight * tm.tmAveCharWidth * 2000;
    getCMap();

    if (!resolvedGetCharWidthI){
        resolveGetCharWidthI();
    }

    m_hasUnreliableOutline = (m_tm.tmPitchAndFamily & (TMPF_TRUETYPE | TMPF_VECTOR)) == 0;
}

LWindowsFontEngineData::~LWindowsFontEngineData()
{
    if (m_hdc){
        DeleteDC(m_hdc);
    }
}


double LWindowsFontEngine::ascent() const
{
    return m_tm.tmAscent;
}

double LWindowsFontEngine::capHeight() const
{
    const std::vector<uint8_t> tableData = getSfntTable(MAKE_TAG_REVERSE('O', 'S', '/', '2'));
    if (size_t(tableData.size()) >= sizeof(OS2Table)) {
        const OS2Table *table = reinterpret_cast<const OS2Table *>(tableData.data());
        if (lFromBigEndian<uint16_t>(table->version) >= 2) {
            int16_t capHeight = lFromBigEndian<int16_t>(table->capHeight);
            if (capHeight > 0){
                return capHeight / m_designToDevice;
            }
        }
    }
    return calculatedCapHeight();
}

double LWindowsFontEngine::descent() const
{
    return m_tm.tmDescent;
}

double LWindowsFontEngine::leading() const
{
    return m_tm.tmExternalLeading;
}

uint32_t LWindowsFontEngine::glyphIndex(uint32_t ucs4) const
{
    uint32_t glyph = 0;
    if (m_symbol) {
        glyph = getTrueTypeGlyphIndex(m_cmap, m_cmapSize, ucs4);
        if (glyph == 0 && ucs4 < 0x100){
            glyph = getTrueTypeGlyphIndex(m_cmap, m_cmapSize, ucs4 + 0xf000);
        }
    } else if (m_ttf) {
        glyph = getTrueTypeGlyphIndex(m_cmap, m_cmapSize, ucs4);
    } else if (ucs4 >= m_tm.tmFirstChar && ucs4 <= m_tm.tmLastChar) {
        glyph = ucs4;
    }
    return glyph;

}

bool LWindowsFontEngine::stringToCMap(const wchar_t *str,
                                      int len,
                                      LGlyphLayout *glyphs,
                                      int *nglyphs,
                                      uint8_t shapeFlags) const
{
    if (*nglyphs < len) {
        *nglyphs = len;
        return false;
    }

    glyphs->numGlyphs = *nglyphs;
    *nglyphs = getGlyphIndexes(str, len, glyphs);

    if (!(shapeFlags & GlyphIndicesOnly)){
        recalcAdvances(glyphs, shapeFlags);
    }
    return true;
}

double LWindowsFontEngine::emSquareSize() const
{
    return m_unitsPerEm;
}

double LWindowsFontEngine::maxCharWidth() const
{
    return m_tm.tmMaxCharWidth;
}

std::shared_ptr<LWindowsFontEngineData> LWindowsFontEngine::sharedData()
{
    if(m_data == nullptr){
        m_data = std::make_shared<LWindowsFontEngineData>();
    }
    return m_data;
}

void LWindowsFontEngine::initFontInfo(const LFont::Def &request, int dpi)
{
    m_fontDef = request; // most settings are equal
    HDC dc = m_data->m_hdc;
    SelectObject(dc, m_hfont);
    wchar_t n[64];
    GetTextFaceW(dc, 64, n);
    m_fontDef.family = n;
    m_fontDef.fixedPitch = !(m_tm.tmPitchAndFamily & TMPF_FIXED_PITCH);
    if (m_fontDef.pointSize < 0) {
        m_fontDef.pointSize = m_fontDef.pixelSize * 72. / dpi;
    } else if (m_fontDef.pixelSize == -1) {
        m_fontDef.pixelSize = std::lround(m_fontDef.pointSize * dpi / 72.);
    }
}

void LWindowsFontEngine::addGlyphsToPath(uint32_t *glyphs, LPoint<double> *positions, int nglyphs,
                                         LPainterPath *path, LFlags<LTextRenderHint> flags)
{
    LOGFONTW lf = m_logfont;
    // The sign must be negative here to make sure we match against character height instead of
    // hinted cell height. This ensures that we get linear matching, and we need this for
    // paths since we later on apply a scaling transform to the glyph outline to get the
    // font at the correct pixel size.
    lf.lfHeight = -m_unitsPerEm;
    lf.lfWidth = 0;
    HFONT hf = CreateFontIndirectW(&lf);
    HDC hdc = m_data->m_hdc;
    HGDIOBJ oldfont = SelectObject(hdc, hf);

    double scale = double(m_fontDef.pixelSize) / m_unitsPerEm;
    double stretch = m_fontDef.stretch ? double(m_fontDef.stretch) / 100 : 1.0;
    for(int i = 0; i < nglyphs; ++i) {
        //lWarn() << "glyph: " <<  glyphs[i];

        if (!addGlyphToPath(glyphs[i], positions[i], hdc, path, m_ttf, /*metric*/0,
                            scale, stretch)) {
            // Some windows fonts, like "Modern", are vector stroke
            // fonts, which are reported as TMPF_VECTOR but do not
            // support GetGlyphOutline, and thus we set this bit so
            // that addOutLineToPath can check it and return safely...
            m_hasOutline = false;
            break;
        }
    }
    DeleteObject(SelectObject(hdc, oldfont));
}

bool LWindowsFontEngine::getSfntTableData(uint32_t tag, uint8_t *buffer, uint32_t *length) const
{
    if (!m_ttf && !m_cffTable){
        return false;
    }
    HDC hdc = m_data->m_hdc;
    SelectObject(hdc, m_hfont);
    *length = GetFontData(hdc, tag, 0, buffer, *length);
    return *length != GDI_ERROR;
}

LFontEngine::FaceId LWindowsFontEngine::faceId() const
{
    return m_faceId;
}

double LWindowsFontEngine::averageCharWidth() const
{
    return m_tm.tmAveCharWidth;
}

void LWindowsFontEngine::addOutlineToPath(double x, double y,
                                          const LGlyphLayout &glyphs,
                                          LPainterPath *path,
                                          LFlags<LTextRenderHint> flags)
{
    if(m_tm.tmPitchAndFamily & (TMPF_TRUETYPE | TMPF_VECTOR)) {
        m_hasOutline = true;
        LFontEngine::addOutlineToPath(x, y, glyphs, path, flags);
        if (m_hasOutline)  {
            // has_outline is set to false if addGlyphToPath gets
            // false from GetGlyphOutline, meaning its not an outline
            // font.
            return;
        }
    }
    LFontEngine::addBitmapFontToPath(x, y, glyphs, path, flags);

}

double LWindowsFontEngine::minLeftBearing() const
{
    if (m_lbearing == SHRT_MIN){
        minRightBearing(); // calculates both
    }

    return m_lbearing;
}

enum { max_font_count = 256 };
static const uint16_t charTable[] = {
    40,
    67,
    70,
    75,
    86,
    88,
    89,
    91,
    102,
    114,
    124,
    127,
    205,
    645,
    884,
    922,
    1070,
    12386,
    0
};

static const int charTableEntries = sizeof(charTable)/sizeof(uint16_t);

double LWindowsFontEngine::minRightBearing() const
{
    if (m_rbearing == SHRT_MIN) {
        int ml = 0;
        int mr = 0;
        HDC hdc = m_data->m_hdc;
        SelectObject(hdc, m_hfont);
        if (m_ttf) {
            std::vector<ABC> abc;
            int n = m_tm.tmLastChar - m_tm.tmFirstChar+1;
            if (n <= max_font_count) {
                abc.resize(n);
                GetCharABCWidths(hdc, m_tm.tmFirstChar, m_tm.tmLastChar, abc.data());
            } else {
                abc.resize(charTableEntries);
                for(int i = 0; i < charTableEntries; i++){
                    GetCharABCWidths(hdc, charTable[i], charTable[i], &abc[i]);
                }
                n = charTableEntries;
            }
            ml = abc[0].abcA;
            mr = abc[0].abcC;
            for (int i = 1; i < n; i++) {
                if (abc[i].abcA + abc[i].abcB + abc[i].abcC != 0) {
                    ml = std::min<int>(ml,abc[i].abcA);
                    mr = std::min<int>(mr,abc[i].abcC);
                }
            }
        } else {
            std::vector<ABCFLOAT> abc;
            int n = m_tm.tmLastChar - m_tm.tmFirstChar+1;
            if (n <= max_font_count) {
                abc.resize(n);
                GetCharABCWidthsFloat(hdc, m_tm.tmFirstChar, m_tm.tmLastChar, abc.data());
            } else {
                abc.resize(charTableEntries);
                for(int i = 0; i < charTableEntries; i++){
                    GetCharABCWidthsFloat(hdc, charTable[i], charTable[i], &abc[i]);
                }
                n = charTableEntries;
            }
            float fml = abc[0].abcfA;
            float fmr = abc[0].abcfC;
            for (int i=1; i<n; i++) {
                if (abc[i].abcfA + abc[i].abcfB + abc[i].abcfC != 0) {
                    fml = std::min<int>(fml,abc[i].abcfA);
                    fmr = std::min<int>(fmr,abc[i].abcfC);
                }
            }
            ml = int(fml - 0.9999);
            mr = int(fmr - 0.9999);
        }
        m_lbearing = ml;
        m_rbearing = mr;
    }

    return m_rbearing;

}

HGDIOBJ LWindowsFontEngine::selectDesignFont() const
{
    LOGFONTW f = m_logfont;
    f.lfHeight = -m_unitsPerEm;
    f.lfWidth = 0;
    HFONT designFont = CreateFontIndirectW(&f);
    return SelectObject(m_data->m_hdc, designFont);
}

int LWindowsFontEngine::getGlyphIndexes(const wchar_t *str, int numChars, LGlyphLayout *glyphs) const
{
    int glyph_pos = 0;
    {
        if (m_symbol) {
            std::wstring_view s(str, numChars);
            std::wstring_view::const_iterator it = s.cbegin();
            while (it != s.cend()) {
                const uint32_t uc = *it;
                glyphs->glyphs[glyph_pos] = getTrueTypeGlyphIndex(m_cmap, m_cmapSize, uc);
                if(!glyphs->glyphs[glyph_pos] && uc < 0x100){
                    glyphs->glyphs[glyph_pos] = getTrueTypeGlyphIndex(m_cmap, m_cmapSize, uc + 0xf000);
                }
                ++glyph_pos;
                ++it;
            }
        } else if (m_ttf) {
            std::wstring_view s(str, numChars);
            std::wstring_view::const_iterator it = s.cbegin();
            while (it != s.cend()) {
                const uint32_t uc = *it;
                glyphs->glyphs[glyph_pos] = getTrueTypeGlyphIndex(m_cmap, m_cmapSize, uc);
                ++glyph_pos;
                ++it;
            }
        } else {
            std::wstring_view s(str, numChars);
            std::wstring_view::const_iterator it = s.cbegin();
            while (it != s.cend()) {
                const uint32_t uc = *it;
                if (uc >= m_tm.tmFirstChar && uc <= m_tm.tmLastChar){
                    glyphs->glyphs[glyph_pos] = uc;
                }else{
                    glyphs->glyphs[glyph_pos] = 0;
                }
                ++glyph_pos;
                ++it;
            }
        }
    }
    glyphs->numGlyphs = glyph_pos;
    return glyph_pos;

}


LGlyphMetrics<double> LWindowsFontEngine::boundingBox(uint32_t glyph, const LTransform2D &t)
{
    HDC hdc = m_data->m_hdc;
    SelectObject(hdc, m_hfont);

    LGlyphMetrics<double> glyphMetrics;
    bool success = getOutlineMetrics(glyph, t, &glyphMetrics);

    if (!m_ttf && !success) {
        // Bitmap fonts
        wchar_t ch = wchar_t(glyph);
        ABCFLOAT abc;
        GetCharABCWidthsFloatW(hdc, ch, ch, &abc);
        double width = abc.abcfB;
        double x = abc.abcfA;

        return LGlyphMetrics<double>(x,
                                  -m_tm.tmAscent,
                                  width,
                                  m_tm.tmHeight,
                                  width,
                                  0).transformed(t);
    }

    return glyphMetrics;
}

bool LWindowsFontEngine::supportsTransformation(const LTransform2D &transform) const
{
    return m_ttf || transform.type() <= LTransform2D::TxTranslate;
}

double LWindowsFontEngine::lineThickness() const
{
    if(m_lineWidth > 0){
        return m_lineWidth;
    }
    return LFontEngine::lineThickness();
}

inline void calculateTTFGlyphWidth(HDC hdc, UINT glyph, int &width)
{
    if (ptrGetCharWidthI){
        ptrGetCharWidthI(hdc, glyph, 1, 0, &width);
    }
}

void LWindowsFontEngine::recalcAdvances(LGlyphLayout *glyphs, uint8_t flags) const
{
    HGDIOBJ oldFont = 0;
    HDC hdc = m_data->m_hdc;
    if (m_ttf && (flags & DesignMetrics)) {
        for(int i = 0; i < glyphs->numGlyphs; i++) {
            uint32_t glyph = glyphs->glyphs[i];
            if(glyph >= (uint32_t)m_designAdvances.size()) {
                const uint32_t newSize = (glyph + uint32_t(256)) >> 8 << 8;
                uint32_t orgSize =(uint32_t) m_designAdvances.size();
                m_designAdvances.resize(newSize);
                for(uint32_t i = orgSize; i < newSize; ++i){
                    m_designAdvances[i] = -1000000;
                }
            }
            if (m_designAdvances[glyph] < -999999) {
                if (!oldFont){
                    oldFont = selectDesignFont();
                }

                int width = 0;
                calculateTTFGlyphWidth(hdc, glyph, width);
                m_designAdvances[glyph] = width / m_designToDevice;
            }
            glyphs->advances[i] = m_designAdvances[glyph];
        }
        if(oldFont){
            DeleteObject(SelectObject(hdc, oldFont));
        }
    } else {
        for(int i = 0; i < glyphs->numGlyphs; i++) {
            uint32_t glyph = glyphs->glyphs[i];

            if (glyph >= m_widthCache.size()) {
                const uint32_t newSize = (glyph + 256) >> 8 << 8;
                uint32_t orgSize = (uint32_t)m_widthCache.size();
                m_widthCache.resize(newSize);
                // memset(m_widthCache.data() + orgSize, 0, newSize - orgSize);
                for(uint32_t i = orgSize; i < newSize; ++i){
                    m_widthCache[i] = 0;
                }
            }
            glyphs->advances[i] = m_widthCache[glyph];
            // font-width cache failed
            if (lFuzzyIsZero(glyphs->advances[i])) {
                int width = 0;
                if (!oldFont){
                    oldFont = SelectObject(hdc, m_hfont);
                }

                if (!m_ttf) {
                    wchar_t ch[2] = { wchar_t(glyph), 0 };
                    int chrLen = 1;
                    if (LStringUtil::requiresSurrogates(glyph)) {
                        ch[0] = LStringUtil::highSurrogate(glyph);
                        ch[1] = LStringUtil::lowSurrogate(glyph);
                        ++chrLen;
                    }
                    SIZE size = {0, 0};
                    GetTextExtentPoint32W(hdc, ch, chrLen, &size);
                    width = size.cx;
                } else {
                    calculateTTFGlyphWidth(hdc, glyph, width);
                }
                glyphs->advances[i] = width;
                // if glyph's within cache range, store it for later
                if (width > 0 && width < 0x100){
                    m_widthCache[glyph] = uint8_t(width);
                }
            }
        }

        if (oldFont){
            SelectObject(hdc, oldFont);
        }
    }
}

bool LWindowsFontEngine::getOutlineMetrics(uint32_t glyph, const LTransform2D &t, LGlyphMetrics<double> *metrics) const
{
    HDC hdc = m_data->m_hdc;

    GLYPHMETRICS gm;
    DWORD res = 0;
    MAT2 mat;
    mat.eM11.value = mat.eM22.value = 1;
    mat.eM11.fract = mat.eM22.fract = 0;
    mat.eM21.value = mat.eM12.value = 0;
    mat.eM21.fract = mat.eM12.fract = 0;

    if (t.type() > LTransform2D::TxTranslate) {
        // We need to set the transform using the HDC's world
        // matrix rather than using the MAT2 above, because the
        // results provided when transforming via MAT2 does not
        // match the glyphs that are drawn using a WorldTransform
        XFORM xform;
        xform.eM11 = FLOAT(t.m11());
        xform.eM12 = FLOAT(t.m12());
        xform.eM21 = FLOAT(t.m21());
        xform.eM22 = FLOAT(t.m22());
        xform.eDx = 0;
        xform.eDy = 0;
        SetGraphicsMode(hdc, GM_ADVANCED);
        SetWorldTransform(hdc, &xform);
    }

    uint32_t format = GGO_METRICS;
    if (m_ttf){
        format |= GGO_GLYPH_INDEX;
    }
    res = GetGlyphOutlineW(hdc, glyph, format, &gm, 0, 0, &mat);

    if (t.type() > LTransform2D::TxTranslate) {
        XFORM xform;
        xform.eM11 = xform.eM22 = 1;
        xform.eM12 = xform.eM21 = xform.eDx = xform.eDy = 0;
        SetWorldTransform(hdc, &xform);
        SetGraphicsMode(hdc, GM_COMPATIBLE);
    }

    if (res != GDI_ERROR) {
        *metrics = LGlyphMetrics<double>(gm.gmptGlyphOrigin.x, -gm.gmptGlyphOrigin.y,
                                   gm.gmBlackBoxX, gm.gmBlackBoxY,
                                   gm.gmCellIncX, gm.gmCellIncY);
        return true;
    } else {
        return false;
    }

}

bool LWindowsFontEngine::hasCFFTable() const
{
    HDC hdc = m_data->m_hdc;
    SelectObject(hdc, m_hfont);
    return GetFontData(hdc, MAKE_TAG_REVERSE('C', 'F', 'F', ' '), 0, 0, 0) != GDI_ERROR;
}

bool LWindowsFontEngine::hasCMapTable() const
{
    HDC hdc = m_data->m_hdc;
    SelectObject(hdc, m_hfont);
    return GetFontData(hdc, MAKE_TAG_REVERSE('c', 'm', 'a', 'p'), 0, 0, 0) != GDI_ERROR;
}


void LWindowsFontEngine::getCMap()
{
    m_ttf = (bool)(m_tm.tmPitchAndFamily & TMPF_TRUETYPE) || hasCMapTable();

    m_cffTable = hasCFFTable();
    HDC hdc = m_data->m_hdc;
    SelectObject(hdc, m_hfont);
    bool symb = false;
    if (m_ttf) {
        m_cmapTable = getSfntTable(MAKE_TAG_REVERSE('c', 'm', 'a', 'p'));
        m_cmap = LFontEngine::getCMap(m_cmapTable.data(), m_cmapTable.size(), &symb, &m_cmapSize);
    }
    if (m_cmap == nullptr) {
        m_ttf = false;
        symb = false;
    }
    m_symbol = symb;
    m_designToDevice = 1;
    m_faceId.index = 0;
    if(m_cmap != nullptr) {
        OUTLINETEXTMETRICW *otm = getOutlineTextMetric(hdc);
        m_unitsPerEm = int(otm->otmEMSquare);
        m_designToDevice = m_unitsPerEm / m_fontDef.pixelSize;
        m_xheight = int(otm->otmsXHeight);
        loadKerningPairs(m_designToDevice);
        const uint8_t *p = reinterpret_cast<const uint8_t *>(otm) + luintptr(otm->otmpFullName);
        m_faceId.filename = reinterpret_cast<const wchar_t *>(p);
        m_lineWidth = otm->otmsUnderscoreSize;
        m_fsType = otm->otmfsType;
        free(otm);
    } else {
        m_unitsPerEm = m_tm.tmHeight;
    }
}


LK_NAMESPACE_END
