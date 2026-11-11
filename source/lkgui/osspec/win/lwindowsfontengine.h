#ifndef LWINDOWSFONTENGINE_H
#define LWINDOWSFONTENGINE_H
#include "lkgui/text/lfontengine.h"
#include "lkgui/paint/ltransform2d.h"
#include "lkgui/paint/lpainterpath.h"
#include "lkgui/osspec/win/lwindows.h"

LK_NAMESPACE_BEGIN

class LWindowsFontEngineData{
public:
    LWindowsFontEngineData();
    ~LWindowsFontEngineData();

    uint32_t m_powGamma[256];
    bool m_clearTypeEnabled = false;
    double m_fontSmoothingGamma;
    HDC m_hdc = 0;
};

class LWindowsFontEngine : public LFontEngine
{
public:
    LWindowsFontEngine(const std::wstring &name, LOGFONTW lf);

    virtual double ascent() const override;
    virtual double capHeight() const override;
    virtual double descent() const override;
    virtual double leading() const override;
    virtual uint32_t glyphIndex(uint32_t ucs4) const override;
    virtual bool stringToCMap(const wchar_t *str, int len, LGlyphLayout *glyphs, int *nglyphs, uint8_t shapeFlags) const override;

    virtual double emSquareSize() const override;
    virtual double maxCharWidth() const override;

    virtual LGlyphMetrics<double> boundingBox(uint32_t glyph) override { return boundingBox(glyph, LTransform2D()); }
    virtual LGlyphMetrics<double> boundingBox(uint32_t glyph, const LTransform2D &t) override;
    virtual bool supportsTransformation(const LTransform2D &transform) const override;
    virtual double lineThickness() const override;
    virtual void recalcAdvances(LGlyphLayout *, uint8_t shapeFlags) const override;

    bool getOutlineMetrics(uint32_t glyph, const LTransform2D &t, LGlyphMetrics<double> *metrics) const;

    static std::shared_ptr<LWindowsFontEngineData> sharedData();
    void initFontInfo(const LFont::Def &request, int dpi);

    virtual void addGlyphsToPath(uint32_t *glyphs, LPoint<double> *positions, int nglyphs,
                                 LPainterPath *path, LFlags<LTextRenderHint> flags) override;
    virtual bool getSfntTableData(uint32_t tag, uint8_t *buffer, uint32_t *length) const override;
    virtual FaceId faceId() const override;
    virtual double averageCharWidth() const override;

    virtual void addOutlineToPath(double x, double y,
                                  const LGlyphLayout &glyphs,
                                  LPainterPath *path,
                                  LFlags<LTextRenderHint> flags) override;

    double minLeftBearing() const override;
    double minRightBearing() const override;


    HGDIOBJ selectDesignFont() const;
    int getGlyphIndexes(const wchar_t *ch, int numChars, LGlyphLayout *glyphs) const;
private:
    bool hasCFFTable() const;
    bool hasCMapTable() const;
    void getCMap();

    std::wstring  m_name;
    std::wstring  m_uniqueFamilyName;
    std::vector<uint8_t> m_cmapTable;
    const uint8_t *m_cmap = nullptr;
    int m_cmapSize = 0;
    double m_designToDevice;
    double m_xheight = -1;
    FaceId m_faceId;


    HFONT m_hfont = 0;
    LOGFONTW m_logfont;
    uint32_t m_ttf : 1;
    uint32_t m_hasOutline : 1;
    uint32_t m_hasUnreliableOutline : 1;
    uint32_t m_cffTable : 1;
    TEXTMETRICW m_tm;

    int m_unitsPerEm = 0;

    mutable int m_synthesizedFlags = -1;
    mutable double m_lineWidth = -1;

    mutable double m_lbearing = SHRT_MIN;
    mutable double m_rbearing = SHRT_MIN;


    static std::shared_ptr<LWindowsFontEngineData> m_data;
    mutable std::vector<uint8_t> m_widthCache;
    mutable std::vector<double> m_designAdvances;
};


LK_NAMESPACE_END
#endif // LWINDOWSFONTENGINE_H
