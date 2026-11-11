#ifndef LSVGSTYLE_H
#define LSVGSTYLE_H

#include "lkgui/paint/lpainter.h"
#include "lkgui/paint/lcolor.h"
#include "lkgui/lrect.h"
#include "lkgui/paint/ltransform2d.h"
#include <stack>

LK_NAMESPACE_BEGIN
class LSvgNode;
class LSvgFont;
class LSvgDocument;
class LSvgPattern;

struct LKSVGDEF LSvgExtraStates
{
    LSvgExtraStates();

    double fillOpacity = 1;
    double strokeOpacity = 1;
    std::shared_ptr<LSvgFont> svgFont;
    LAlignment textAnchor = LAlignment::Left;
    int fontWeight = LFont::Normal;
    LFillRule fillRule = LFillRule::Winding;
    double strokeDashOffset = 0;
    int nestedUseLevel = 0;
    int nestedUseCount = 0;
    bool vectorEffect = false; // true if pen is cosmetic
    int8_t imageRendering= 0; // LSvgQualityStyle::ImageRendering
    bool inUse = false; // true if currently in LSvgUseNode
};

class LKSVGDEF LSvgStyleProperty : public LIVar
{
public:
    enum Type
    {
        QUALITY,
        FILL,
        VIEWPORT_FILL,
        FONT,
        STROKE,
        SOLID_COLOR,
        GRADIENT,
        PATTERN,
        TRANSFORM,
        ANIMATE_TRANSFORM,
        ANIMATE_COLOR,
        OPACITY,
        COMP_OP
    };
public:
    virtual ~LSvgStyleProperty();
    virtual void apply(LPainter *p, const std::shared_ptr<LSvgNode> node, LSvgExtraStates &states) = 0;
    virtual void revert(LPainter *p, LSvgExtraStates &states) =0;
    virtual Type type() const=0;
    bool isDefault() const { return false; } // [not virtual since called from templated class]
};

class LKSVGDEF LSvgPaintStyleProperty : public LSvgStyleProperty
{
public:
    virtual std::shared_ptr<LBrush>  brush(LPainter *p, const std::shared_ptr<LSvgNode> node, LSvgExtraStates &states) = 0;
    void apply(LPainter *p, const std::shared_ptr<LSvgNode> node, LSvgExtraStates &states) override;
    void revert(LPainter *p, LSvgExtraStates &states) override;
};

class LKSVGDEF LSvgQualityStyle : public LSvgStyleProperty
{
public:
    enum ImageRendering: int8_t {
        ImageRenderingAuto = 0,
        ImageRenderingOptimizeSpeed = 1,
        ImageRenderingOptimizeQuality = 2,
    };

    LSvgQualityStyle();
    void apply(LPainter *p, const std::shared_ptr<LSvgNode> node, LSvgExtraStates &states) override;
    void revert(LPainter *p, LSvgExtraStates &states) override;
    Type type() const override;

    void setImageRendering(ImageRendering);
private:
    int32_t m_imageRendering: 4;
    int32_t m_oldImageRendering: 4;
    uint32_t m_imageRenderingSet: 1;
};



class LKSVGDEF LSvgOpacityStyle : public LSvgStyleProperty
{
public:
    LSvgOpacityStyle(double opacity);
    void apply(LPainter *p, const std::shared_ptr<LSvgNode> node, LSvgExtraStates &states) override;
    void revert(LPainter *p, LSvgExtraStates &states) override;
    Type type() const override;
    double opacity() const { return m_opacity; }
    bool isDefault() const { return lFuzzyCompare(m_opacity, double(1.0)); }

private:
    double m_opacity;
    double m_oldOpacity;
};

class LKSVGDEF LSvgFillStyle : public LSvgStyleProperty
{
public:
    LSvgFillStyle();
    void apply(LPainter *p, const std::shared_ptr<LSvgNode> node, LSvgExtraStates &states) override;
    void revert(LPainter *p, LSvgExtraStates &states) override;
    Type type() const override;

    void setFillRule(LFillRule f);
    void setFillOpacity(double opacity);
    void setFillStyle(std::shared_ptr<LSvgPaintStyleProperty> style);
    void setBrush(std::shared_ptr<LBrush>  brush);
    void setBrush(LColor color);

    const std::shared_ptr<LBrush> brush() const
    {
        return m_fill;
    }

    double fillOpacity() const
    {
        return m_fillOpacity;
    }

    LFillRule fillRule() const
    {
        return m_fillRule;
    }

    std::shared_ptr<LSvgPaintStyleProperty> style() const
    {
        return m_style;
    }

    void setPaintStyleId(const std::string &Id)
    {
        m_paintStyleId = Id;
    }

    std::string paintStyleId() const
    {
        return m_paintStyleId;
    }

    void setPaintStyleResolved(bool resolved)
    {
        m_paintStyleResolved = resolved;
    }

    bool isPaintStyleResolved() const
    {
        return m_paintStyleResolved;
    }

private:
    // fill            v 	v 	'inherit' | <Paint.datatype>
    // fill-opacity    v 	v 	'inherit' | <OpacityValue.datatype>
    std::shared_ptr<LBrush>  m_fill;
    std::shared_ptr<LBrush>  m_oldFill;
    std::shared_ptr<LSvgPaintStyleProperty> m_style;

    LFillRule m_fillRule;
    LFillRule m_oldFillRule;
    double m_fillOpacity;
    double m_oldFillOpacity;

    std::string m_paintStyleId;
    uint32_t m_paintStyleResolved : 1;
    uint32_t m_fillRuleSet : 1;
    uint32_t m_fillOpacitySet : 1;
    uint32_t m_fillSet : 1;
};

class LKSVGDEF LSvgViewportFillStyle : public LSvgStyleProperty
{
public:
    LSvgViewportFillStyle(const std::shared_ptr<LBrush> brush);
    void apply(LPainter *p, const std::shared_ptr<LSvgNode> node, LSvgExtraStates &states) override;
    void revert(LPainter *p, LSvgExtraStates &states) override;
    Type type() const override;

    const std::shared_ptr<LBrush> brush() const
    {
        return m_viewportFill;
    }
private:
    // viewport-fill         v 	x 	'inherit' | <Paint.datatype>
    // viewport-fill-opacity 	v 	x 	'inherit' | <OpacityValue.datatype>
    std::shared_ptr<LBrush>  m_viewportFill;
    std::shared_ptr<LBrush>  m_oldFill;
};

class LKSVGDEF LSvgFontStyle : public LSvgStyleProperty
{
public:
    static const int LIGHTER = -1;
    static const int BOLDER = 1;

    LSvgFontStyle(std::shared_ptr<LSvgFont> font, std::shared_ptr<LSvgDocument> doc);
    LSvgFontStyle();
    void apply(LPainter *p, const std::shared_ptr<LSvgNode> node, LSvgExtraStates &states) override;
    void revert(LPainter *p, LSvgExtraStates &states) override;
    Type type() const override;

    void setSize(double size)
    {
        // Store the _pixel_ size in the font. Since QFont::setPixelSize() only takes an int, call
        // LFont::SetPointSize() instead. Set proper font size just before rendering.
        m_font->setPointSizeF(size);
        m_sizeSet = 1;
    }

    void setTextAnchor(LAlignment anchor)
    {
        m_textAnchor = anchor;
        m_textAnchorSet = 1;
    }

    void setFamily(const std::string &family)
    {
        m_font->setFamily(family);
        m_familySet = 1;
    }

    void setStyle(LFont::Style fontStyle) {
        m_font->setStyle(fontStyle);
        m_styleSet = 1;
    }

    void setVariant(LFont::Capitalization fontVariant)
    {
        m_font->setCapitalization(fontVariant);
        m_variantSet = 1;
    }

    void setWeight(int weight)
    {
        m_weight = weight;
        m_weightSet = 1;
    }

    std::shared_ptr<LSvgFont> svgFont() const
    {
        return m_svgFont;
    }

    const std::shared_ptr<LFont> font() const
    {
        return m_font;
    }

    std::shared_ptr<LSvgDocument> doc() const {return m_doc.lock();}

private:
    std::shared_ptr<LSvgFont> m_svgFont;
    std::weak_ptr<LSvgDocument> m_doc;
    std::shared_ptr<LFont> m_font;

    int m_weight;
    LAlignment m_textAnchor;

    std::shared_ptr<LSvgFont> m_oldSvgFont;
    std::shared_ptr<LFont>  m_oldFont;
    LAlignment m_oldTextAnchor;
    int m_oldWeight;

    uint32_t m_familySet : 1;
    uint32_t m_sizeSet : 1;
    uint32_t m_styleSet : 1;
    uint32_t m_variantSet : 1;
    uint32_t m_weightSet : 1;
    uint32_t m_textAnchorSet : 1;
};

class LKSVGDEF LSvgStrokeStyle : public LSvgStyleProperty
{
public:
    LSvgStrokeStyle();
    void apply(LPainter *p, const std::shared_ptr<LSvgNode> node, LSvgExtraStates &states) override;
    void revert(LPainter *p, LSvgExtraStates &states) override;
    Type type() const override;

    void setStroke(std::shared_ptr<LBrush>  brush)
    {
        m_stroke->setBrush(brush);
        m_style = nullptr;
        m_strokeSet = 1;
    }


    void setStyle(std::shared_ptr<LSvgPaintStyleProperty> style)
    {
        m_style = style;
        m_strokeSet = 1;
    }

    void setDashArray(const std::vector<double> &dashes);

    void setDashArrayNone()
    {
        m_stroke->m_style = (LPenStyle::Solid);
        m_strokeDashArraySet = 1;
    }

    void setDashOffset(double offset)
    {
        m_strokeDashOffset = offset;
        m_strokeDashOffsetSet = 1;
    }

    void setLineCap(LPenCapStyle cap)
    {
        m_stroke->m_capStyle = cap;
        m_strokeLineCapSet = 1;
    }

    void setLineJoin(LPenJoinStyle join)
    {
        m_stroke->m_joinStyle = join;
        m_strokeLineJoinSet = 1;
    }

    void setMiterLimit(double limit)
    {
        m_stroke->m_miterLimit = limit;
        m_strokeMiterLimitSet = 1;
    }

    void setOpacity(double opacity)
    {
        m_strokeOpacity = opacity;
        m_strokeOpacitySet = 1;
    }

    void setWidth(double width)
    {
        m_stroke->m_width = width;
        m_strokeWidthSet = 1;
    }

    double width()
    {
        return m_stroke->m_width;
    }

    void setVectorEffect(bool nonScalingStroke)
    {
        m_vectorEffect = nonScalingStroke;
        m_vectorEffectSet = 1;
    }

    std::shared_ptr<LSvgPaintStyleProperty> style() const
    {
        return m_style;
    }

    void setPaintStyleId(const std::string &Id)
    {
        m_paintStyleId = Id;
    }

    std::string paintStyleId() const
    {
        return m_paintStyleId;
    }

    void setPaintStyleResolved(bool resolved)
    {
        m_paintStyleResolved = resolved;
    }

    bool isPaintStyleResolved() const
    {
        return m_paintStyleResolved;
    }

    std::shared_ptr<LPen>  stroke() const
    {
        return m_stroke;
    }

private:
    // stroke            v 	v 	'inherit' | <Paint.datatype>
    // stroke-dasharray  v 	v 	'inherit' | <StrokeDashArrayValue.datatype>
    // stroke-dashoffset v 	v 	'inherit' | <StrokeDashOffsetValue.datatype>
    // stroke-linecap    v 	v 	'butt' | 'round' | 'square' | 'inherit'
    // stroke-linejoin   v 	v 	'miter' | 'round' | 'bevel' | 'inherit'
    // stroke-miterlimit v 	v 	'inherit' | <StrokeMiterLimitValue.datatype>
    // stroke-opacity    v 	v 	'inherit' | <OpacityValue.datatype>
    // stroke-width      v 	v 	'inherit' | <StrokeWidthValue.datatype>
    std::shared_ptr<LPen>  m_stroke;
    std::shared_ptr<LPen>  m_oldStroke;
    double m_strokeOpacity;
    double m_oldStrokeOpacity;
    double m_strokeDashOffset;
    double m_oldStrokeDashOffset;

    std::shared_ptr<LSvgPaintStyleProperty> m_style;
    std::string m_paintStyleId;
    uint32_t m_paintStyleResolved : 1;
    uint32_t m_vectorEffect : 1;
    uint32_t m_oldVectorEffect : 1;

    uint32_t m_strokeSet : 1;
    uint32_t m_strokeDashArraySet : 1;
    uint32_t m_strokeDashOffsetSet : 1;
    uint32_t m_strokeLineCapSet : 1;
    uint32_t m_strokeLineJoinSet : 1;
    uint32_t m_strokeMiterLimitSet : 1;
    uint32_t m_strokeOpacitySet : 1;
    uint32_t m_strokeWidthSet : 1;
    uint32_t m_vectorEffectSet : 1;
};

class LKSVGDEF LSvgSolidColorStyle : public LSvgPaintStyleProperty
{
public:
    LSvgSolidColorStyle(const LColor &color);
    Type type() const override;

    const LColor & color() const
    {
        return m_solidColor;
    }

    std::shared_ptr<LBrush>  brush(LPainter *, const std::shared_ptr<LSvgNode> , LSvgExtraStates &) override
    {
        return std::make_shared<LBrush>(m_solidColor);
    }

private:
    // solid-color       v 	x 	'inherit' | <SVGColor.datatype>
    // solid-opacity     v 	x 	'inherit' | <OpacityValue.datatype>
    LColor m_solidColor;

    std::shared_ptr<LBrush>  m_oldFill;
    std::shared_ptr<LPen>    m_oldStroke;
};

class LKSVGDEF LSvgGradientStyle : public LSvgPaintStyleProperty
{
public:
    LSvgGradientStyle(std::shared_ptr<LGradient> grad);

    Type type() const override;

    void setStopLink(const std::string &link, std::shared_ptr<LSvgDocument> doc);
    std::string stopLink() const { return m_link; }
    void resolveStops();
    void resolveStopsHelper(std::list<std::string> *visited);

    void setTransform(const LTransform2D &transform);
    LTransform2D transform() const
    {
        return m_transform;
    }

    std::shared_ptr<LGradient> gradient() const
    {
        return m_gradient;
    }

    bool gradientStopsSet() const
    {
        return m_gradientStopsSet;
    }

    void setGradientStopsSet(bool set)
    {
        m_gradientStopsSet = set;
    }

    std::shared_ptr<LBrush>  brush(LPainter *, const std::shared_ptr<LSvgNode> , LSvgExtraStates &) override;
private:
    std::shared_ptr<LGradient> m_gradient;
    LTransform2D m_transform;

    std::weak_ptr<LSvgDocument> m_doc;
    std::string           m_link;
    bool m_gradientStopsSet;
};

class LKSVGDEF LSvgPatternStyle : public LSvgPaintStyleProperty
{
public:
    LSvgPatternStyle(std::shared_ptr<LSvgPattern> pattern);
    ~LSvgPatternStyle() = default;
    Type type() const override;

    std::shared_ptr<LBrush>  brush(LPainter *, const std::shared_ptr<LSvgNode> , LSvgExtraStates &) override;
    std::shared_ptr<LSvgPattern> patternNode() { return m_pattern; }
private:
    std::shared_ptr<LSvgPattern> m_pattern;
    std::shared_ptr<LImage> m_patternImage;
    LRect<double> m_parentBound;
};


class LKSVGDEF LSvgTransformStyle : public LSvgStyleProperty
{
public:
    LSvgTransformStyle(const LTransform2D &transform);
    void apply(LPainter *p, const std::shared_ptr<LSvgNode> node, LSvgExtraStates &states) override;
    void revert(LPainter *p, LSvgExtraStates &states) override;
    Type type() const override;

    const LTransform2D & transform() const
    {
        return m_transform;
    }
    bool isDefault() const { return m_transform.isIdentity(); }
private:
    LTransform2D m_transform;
    std::stack<LTransform2D> m_oldWorldTransform;
};

class LKSVGDEF LSvgCompOpStyle : public LSvgStyleProperty
{
public:
    LSvgCompOpStyle(LCompositionMode mode);
    void apply(LPainter *p, const std::shared_ptr<LSvgNode> node, LSvgExtraStates &states) override;
    void revert(LPainter *p, LSvgExtraStates &states) override;
    Type type() const override;

    const LCompositionMode & compOp() const
    {
        return m_mode;
    }
private:
    //comp-op attribute
    LCompositionMode m_mode;
    LCompositionMode m_oldMode;
};

class LKSVGDEF LSvgStaticStyle
{
public:
    LSvgStaticStyle();
    ~LSvgStaticStyle();

    void apply(LPainter *p, const std::shared_ptr<LSvgNode> node, LSvgExtraStates &states);
    void revert(LPainter *p, LSvgExtraStates &states);
    std::shared_ptr<LSvgQualityStyle>      quality;
    std::shared_ptr<LSvgFillStyle>         fill;
    std::shared_ptr<LSvgViewportFillStyle> viewportFill;
    std::shared_ptr<LSvgFontStyle>         font;
    std::shared_ptr<LSvgStrokeStyle>       stroke;
    std::shared_ptr<LSvgSolidColorStyle>   solidColor;
    std::shared_ptr<LSvgGradientStyle>     gradient;
    std::shared_ptr<LSvgPatternStyle>      pattern;
    std::shared_ptr<LSvgTransformStyle>    transform;
    std::shared_ptr<LSvgOpacityStyle>      opacity;
    std::shared_ptr<LSvgCompOpStyle>       compop;
};

class LSvgAbstractAnimatedProperty;
class LKSVGDEF LSvgAnimatedStyle
{
public:
    LSvgAnimatedStyle();
    ~LSvgAnimatedStyle();

    void apply(LPainter *p, const std::shared_ptr<LSvgNode> node, LSvgExtraStates &states);
    void revert(LPainter *p, LSvgExtraStates &states);

private:
    void savePaintingState(const LPainter *p, const std::shared_ptr<LSvgNode> node, LSvgExtraStates &states);
    void applyPropertyAnimation(LPainter *p, std::shared_ptr<LSvgAbstractAnimatedProperty> property, bool replace);

private:
    std::shared_ptr<LBrush>  m_brush;
    std::shared_ptr<LPen>  m_pen;
    LTransform2D m_worldTransform;
    LTransform2D m_transformToNode;
};

LK_NAMESPACE_END

#endif // LSVGSTYLE_H
