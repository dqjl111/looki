#ifndef LFONTENGINE_H
#define LFONTENGINE_H
#include "lkgui/image/limage.h"
#include "lkgui/paint/lglyphmetrics.h"
#include "ltextitem.h"
#include "lkcore/text/lchar.h"

LK_NAMESPACE_BEGIN
/* Used with the Freetype font engine. */
class LGlyph {
public:
    LGlyph() = default;
    short m_linearAdvance = 0;
    uint16_t m_width = 0;
    uint16_t m_height = 0;
    short m_x = 0;
    short m_y = 0;
    short m_advance = 0;
    uint8_t m_format = 0;
    std::vector<uint8_t> data;
};


class LKGUIDEF LFontEngine
{
public:
    enum Type {
        Box,
        // MS Windows types
        Win,
        // Apple Mac OS types
        Mac,
        // QWS types
        Freetype,
        QPF1,
        QPF2,
        Proxy,
        DirectWrite,
        TestFontEngine = 0x1000
    };


    enum ShaperFlag {
        DesignMetrics = 0x02,
        GlyphIndicesOnly = 0x04
    };

    LFontEngine(Type _type = Box);

    virtual double ascent() const = 0;
    virtual double capHeight() const = 0;
    virtual double descent() const = 0;
    virtual double leading() const = 0;
    virtual bool stringToCMap(const wchar_t *str, int len, LGlyphLayout *glyphs, int *nglyphs, uint8_t shapeFlags) const = 0;

    virtual double maxCharWidth() const = 0;
    virtual double minLeftBearing() const;
    virtual double minRightBearing() const;

    virtual double emSquareSize() const { return ascent(); }

    virtual uint32_t glyphIndex(uint32_t ucs4) const = 0;

    virtual LGlyphMetrics<double> boundingBox(uint32_t glyph) = 0;
    virtual LGlyphMetrics<double> boundingBox(uint32_t glyph, const LTransform2D &matrix);
    virtual bool supportsTransformation(const LTransform2D &transform) const;

    virtual void addGlyphsToPath(uint32_t *glyphs, LPoint<double> *positions, int nglyphs,
                                 LPainterPath *path, LFlags<LTextRenderHint> flags);
    void getGlyphPositions(const LGlyphLayout &glyphs,
                           const LTransform2D &matrix,
                           LFlags<LTextRenderHint> flags,
                           std::vector<uint32_t> &glyphsOut,
                           std::vector<LPoint<double>> &positions);


    std::vector<uint8_t> getSfntTable(uint32_t tag) const;
    virtual bool getSfntTableData(uint32_t tag, uint8_t *buffer, uint32_t *length) const;
    virtual bool hasInternalCaching() const { return false; }
    virtual std::shared_ptr<LGlyph> glyphData(uint32_t glyph,
                                              double subPixelPosition,
                                              LGlyphFormat neededFormat,
                                              const LTransform2D &t);

    virtual double lineThickness() const;
    virtual double underlinePosition() const;

    virtual void getGlyphBearings(uint32_t glyph,
                                  double *leftBearing = nullptr,
                                  double *rightBearing = nullptr);

    virtual double averageCharWidth() const;
    virtual void addOutlineToPath(double x, double y,
                                  const LGlyphLayout &glyphs,
                                  LPainterPath *path,
                                  LFlags<LTextRenderHint> flags);
    void addBitmapFontToPath(double x, double y,
                             const LGlyphLayout &,
                             LPainterPath *path,
                             LFlags<LTextRenderHint> flags);

    virtual std::shared_ptr<LImage> alphaMapForGlyph(uint32_t glyph);
    virtual std::shared_ptr<LImage> bitmapForGlyph(uint32_t glyph,
                                      double subPixelPosition,
                                      const LTransform2D &t,
                                      const LColor &color = LColor());

    virtual void recalcAdvances(LGlyphLayout *, uint8_t shapeFlags) const {}

    struct FaceId {
        FaceId() : index(0), encoding(0) {}
        std::wstring filename;
        std::vector<uint8_t> uuid;
        int index;
        int encoding;
    };
    virtual FaceId faceId() const { return FaceId(); }

    struct KernPair {
        uint32_t left_right;
        double adjust;

        inline bool operator<(const KernPair &other) const
        {
            return left_right < other.left_right;
        }
    };
    std::vector<KernPair> m_kerningPairs;
    void loadKerningPairs(double scalingFactor);

    inline Type type(){
        return m_type;
    }

    LFont::Def m_fontDef;
    LGlyphFormat m_glyphFormat = LGlyphFormat::None;
    bool m_symbol = false;
    uint16_t m_fsType = 0;

protected:
    static const uint8_t *getCMap(const uint8_t *table, uint32_t tableSize, bool *isSymbolFont, int *cmapSize);
    static uint32_t getTrueTypeGlyphIndex(const uint8_t *cmap, int cmapSize, uint32_t unicode);
    double calculatedCapHeight() const;

    mutable double m_minLeftBearing;
    mutable double m_minRightBearing;
    Type m_type;

};


LK_NAMESPACE_END
#endif // LFONTENGINE_H
