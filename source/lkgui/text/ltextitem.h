#ifndef LTEXTITEM_H
#define LTEXTITEM_H

#include "lfont.h"
#include "lkgui/lguienums.h"
#include "lkgui/lpoint.h"
#include "lkgui/text/ltextformat.h"

LK_NAMESPACE_BEGIN
enum class LGlyphFormat {
    None,
    Render = None,
    Mono,
    A8,
    A32,
    ARGB
};

struct LGlyphJustification
{
    inline LGlyphJustification()
        : type(0), nKashidas(0), space(0)
    {}

    enum JustificationType {
        JustifyNone,
        JustifySpace,
        JustifyKashida
    };

    uint8_t type :2;
    uint8_t nKashidas : 6; // more do not make sense...
    double space = 0;
};

struct LGlyphAttributes {
    uint8_t clusterStart  : 1;
    uint8_t dontPrint     : 1;
    uint8_t justification : 4;
    uint8_t reserved      : 2;
};

struct LGlyphLayout
{

    enum {
        SpaceNeeded = sizeof(uint32_t) + sizeof(double) + sizeof(LPoint<double>)
                      + sizeof(LGlyphAttributes) + sizeof(LGlyphJustification)
    };
    // init to 0 not needed, done when shaping
    LPoint<double> *offsets = nullptr;
    uint32_t *glyphs = nullptr;
    double *advances = nullptr;
    LGlyphJustification *justifications = nullptr;
    LGlyphAttributes *attributes = nullptr;

    int numGlyphs = 0;
    inline LGlyphLayout(){}

    inline explicit LGlyphLayout(char *address, int totalGlyphs)
    {
        offsets = reinterpret_cast<LPoint<double> *>(address);
        int offset = totalGlyphs * sizeof(LPoint<double>);
        glyphs = reinterpret_cast<uint32_t *>(address + offset);
        offset += totalGlyphs * sizeof(uint32_t);
        advances = reinterpret_cast<double *>(address + offset);
        offset += totalGlyphs * sizeof(double);
        justifications = reinterpret_cast<LGlyphJustification *>(address + offset);
        offset += totalGlyphs * sizeof(LGlyphJustification);
        attributes = reinterpret_cast<LGlyphAttributes *>(address + offset);
        numGlyphs = totalGlyphs;
    }


    inline LGlyphLayout mid(int position, int n = -1) const {
        LGlyphLayout copy = *this;
        copy.glyphs += position;
        copy.advances += position;
        copy.offsets += position;
        copy.justifications += position;
        copy.attributes += position;
        if (n == -1){
            copy.numGlyphs -= position;
        }
        else{
            copy.numGlyphs = n;
        }
        return copy;
    }

    inline double effectiveAdvance(int item) const
    {
        return (advances[item] + justifications[item].space) * !attributes[item].dontPrint;
    }

    inline void clear(int first = 0, int last = -1) {
        if (last == -1){
            last = numGlyphs;
        }
        if (first == 0 && last == numGlyphs
            && reinterpret_cast<char *>(offsets + numGlyphs) == reinterpret_cast<char *>(glyphs)) {
            memset(static_cast<void *>(offsets), 0, (numGlyphs * SpaceNeeded));
        } else {
            const int num = last - first;
            memset(static_cast<void *>(offsets + first), 0, num * sizeof(double));
            memset(glyphs + first, 0, num * sizeof(uint32_t));
            memset(static_cast<void *>(advances + first), 0, num * sizeof(double));
            memset(static_cast<void *>(justifications + first), 0, num * sizeof(LGlyphJustification));
            memset(attributes + first, 0, num * sizeof(LGlyphAttributes));
        }
    }

    inline char *data() {
        return reinterpret_cast<char *>(offsets);
    }

    void grow(char *address, int totalGlyphs);
};


class LScriptItem;
class LKGUIDEF LTextItem
{
public:
    LTextItem();
    LTextItem(const LScriptItem* si, std::shared_ptr<LFont> font, const LTextCharFormat &format = LTextCharFormat());
    LTextItem(const LGlyphLayout &g,
              std::shared_ptr<LFont> font,
              const wchar_t *chars,
              int numChars,
              std::shared_ptr<LFontEngine> fe,
              const LTextCharFormat &format = LTextCharFormat());


    double descent = 0;
    double ascent = 0;
    double width = 0;

    LFlags<LTextRenderHint> flags;
    bool justified = false;
    LPenStyle underlineStyle = LPenStyle::No;
    LTextCharFormat charFormat;
    int numChars = 0;
    const wchar_t *chars = nullptr;
    const unsigned short *logClusters = nullptr;
    const std::shared_ptr<LFont> font = nullptr;

    LGlyphLayout glyphs;
    std::shared_ptr<LFontEngine> fontEngine = nullptr;

   void initWithScriptItem(const LScriptItem* si);
   //std::set<LProperty> props;
};


LK_NAMESPACE_END
#endif // LTEXTITEM_H
