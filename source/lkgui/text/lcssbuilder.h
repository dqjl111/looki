#ifndef LCSSBUILDER_H
#define LCSSBUILDER_H

#include "lkgui/lsize.h"
#include "lkgui/text/lfont.h"
#include "lkgui/text/lcssparser.h"
#include "lkgui/image/ltexture.h"

LK_NAMESPACE_BEGIN

struct LCssBorderImageData{
    std::shared_ptr<LTexture> texture = nullptr;
    std::array<double, 4> slices;
    LTileRule horizRepeat = LTileRule::Stretch;
    LTileRule vertRepeat = LTileRule::Stretch;
};


struct LCssBorderData{
    std::array<LSize<double>, 4> radiis;
    std::array<lcss::BorderStyle, 4> styles;
    std::array<LColor, 4> colors;
    std::array<double, 4> borders;
};



struct LCssBoxData{
    std::array<double, 4> margins;
    std::array<double, 4> paddings;
};

struct LCssBackgroundData{
    lcss::Repeat repeat;
    lcss::Origin origin;
    lcss::Origin clip;
    lcss::Attachment attachment;
    LFlags<LAlignment> position;
    LColor color;
    std::shared_ptr<LTexture> texture = nullptr;
    std::shared_ptr<LGradient> gradient = nullptr;
};


struct LCssForegroundData{
    LColor color;
    std::shared_ptr<LFont> font = nullptr;
};

struct LCssImageData
{
    std::shared_ptr<LTexture> texture = nullptr;
    LFlags<LAlignment> alignment;
};

class LKGUIDEF LCssBuilder
{
protected:
    int m_inToPx = 96;
    int m_emToPx = 16;
    double m_cmToPx = 37.8;
    double m_mmToPx = 3.78;
    LSize<int> m_viewSize = LSize<int>(100, 100);
    std::shared_ptr<lcss::StyleComputed> m_styleComputed = nullptr;
public:
    LCssBuilder(std::shared_ptr<lcss::StyleComputed> styleComputed = nullptr);
    void setStyleComputed(std::shared_ptr<lcss::StyleComputed> styleComputed);
    inline std::shared_ptr<lcss::StyleComputed> styleComputed() const{
        return m_styleComputed;
    }

    inline void setViewSize(const LSize<int> s){
        m_viewSize = s;
    }
    const inline LSize<int>& viewSize() const{
        return m_viewSize;
    }

    void setDpi(int dpi);
    inline int dpi() const{
        return m_inToPx;
    }


    double widthToPx(const lcss::StyleLength& len) const;
    double heightToPx(const lcss::StyleLength& len) const;
    double lengthToPx(const lcss::StyleLength& len) const;

    std::array<LColor, 4> buildBorderColors() const;
    std::array<LSize<double>, 4> buildBorderRadiuses() const;
    std::array<double, 4> buildBorderWidths() const;
    std::array<lcss::BorderStyle, 4> buildBorderStyles() const;
    std::array<double, 4> buildBorderImageSlices() const;

    std::array<double, 4> buildBoxMargins() const;
    std::array<double, 4> buildBoxPaddings() const;

    std::shared_ptr<LFont> buildFont() const;
    std::shared_ptr<LCssBorderData> buildBorderData() const;
    std::shared_ptr<LCssBorderImageData> buildBorderImage() const;
    std::shared_ptr<LCssBoxData> buildBoxData() const;
    std::shared_ptr<LCssBackgroundData> buildBackgroundData() const;
    std::shared_ptr<LCssForegroundData> buildForegroundData() const;

    bool hasBorderImage() const;
    bool hasBorderData() const;
    bool hasBoxData() const;
    bool hasBackgroundData() const;
    bool hasForegroundData() const;
};


LK_NAMESPACE_END
#endif // LCSSBUILDER_H
