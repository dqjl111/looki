#include "lsvgstyle.h"
#include "lsvgnode.h"
#include "lsvgdocument.h"
#include "lsvganimatenode.h"
#include "lkcore/ldebug.h"
#include <algorithm>
LK_NAMESPACE_BEGIN

static LColor sumColor(const LColor &c1, const LColor &c2)
{
    uint8_t sumRed = (uint8_t)std::max<uint32_t>(uint32_t(c1.m_red) + uint32_t(c2.m_red), 255);
    uint8_t sumGreen = (uint8_t)std::max<uint32_t>(uint32_t(c1.m_green) + uint32_t(c2.m_green), 255);
    uint8_t sumBlue = (uint8_t)std::max<uint32_t>(uint32_t(c1.m_blue) + uint32_t(c2.m_blue), 255);
    return LColor(sumRed, sumGreen, sumBlue);
}

LSvgExtraStates::LSvgExtraStates()
{
}

LSvgStyleProperty::~LSvgStyleProperty()
{
}

void LSvgPaintStyleProperty::apply(LPainter *, const std::shared_ptr<LSvgNode> , LSvgExtraStates &)
{
    lWarn() << "This should not be called!";
}

void LSvgPaintStyleProperty::revert(LPainter *, LSvgExtraStates &)
{
    lWarn() << "This should not be called!";
}


LSvgQualityStyle::LSvgQualityStyle()
    : m_imageRendering(LSvgQualityStyle::ImageRenderingAuto)
    , m_oldImageRendering(LSvgQualityStyle::ImageRenderingAuto)
    , m_imageRenderingSet(0)
{
}

void LSvgQualityStyle::setImageRendering(ImageRendering hint) {
    m_imageRendering = hint;
    m_imageRenderingSet = 1;
}

void LSvgQualityStyle::apply(LPainter *p, const std::shared_ptr<LSvgNode> , LSvgExtraStates &states)
{
    m_oldImageRendering = states.imageRendering;
    if (m_imageRenderingSet) {
        states.imageRendering = m_imageRendering;
    }
    if (m_imageRenderingSet) {
        bool smooth = false;
        if (m_imageRendering == ImageRenderingAuto){
            // auto (the spec says to prefer quality)
            smooth = true;
        }else{
            smooth = (m_imageRendering == ImageRenderingOptimizeQuality);
        }
        p->setRenderHints(LRenderHint::SmoothPixmapTransform, smooth);
    }
}

void LSvgQualityStyle::revert(LPainter *p, LSvgExtraStates &states)
{
    if (m_imageRenderingSet) {
        states.imageRendering = m_oldImageRendering;
        bool smooth = false;
        if (m_oldImageRendering == ImageRenderingAuto){
            smooth = true;
        }else{
            smooth = (m_oldImageRendering == ImageRenderingOptimizeQuality);
        }
        p->setRenderHints(LRenderHint::SmoothPixmapTransform, smooth);
    }
}

LSvgFillStyle::LSvgFillStyle()
    : m_style(0)
    , m_fillRule(LFillRule::Winding)
    , m_oldFillRule(LFillRule::Winding)
    , m_fillOpacity(1.0)
    , m_oldFillOpacity(0)
    , m_paintStyleResolved(1)
    , m_fillRuleSet(0)
    , m_fillOpacitySet(0)
    , m_fillSet(0)
{
}

void LSvgFillStyle::setFillRule(LFillRule f)
{
    m_fillRuleSet = 1;
    m_fillRule = f;
}

void LSvgFillStyle::setFillOpacity(double opacity)
{
    m_fillOpacitySet = 1;
    m_fillOpacity = opacity;
}

void LSvgFillStyle::setFillStyle(std::shared_ptr<LSvgPaintStyleProperty> style)
{
    m_style = style;
    m_fillSet = 1;
}

void LSvgFillStyle::setBrush(std::shared_ptr<LBrush> brush)
{
    m_fill = brush;
    m_style = nullptr;
    m_fillSet = 1;
}

void LSvgFillStyle::setBrush(LColor color)
{
    if(m_fill == nullptr){
        m_fill = std::make_shared<LBrush>();
    }
    m_fill->m_style = LBrush::SolidPattern;
    m_fill->m_color= color;
    m_fillSet = 1;
}

void LSvgFillStyle::apply(LPainter *p, const std::shared_ptr<LSvgNode> n, LSvgExtraStates &states)
{
    m_oldFill = p->brush();
    m_oldFillRule = states.fillRule;
    m_oldFillOpacity = states.fillOpacity;

    if (m_fillRuleSet){
        states.fillRule = m_fillRule;
    }
    if (m_fillSet) {
        if (m_style){
            p->setBrush(m_style->brush(p, n, states));
        }else{
            p->setBrush(m_fill);
        }
    }
    if (m_fillOpacitySet){
        states.fillOpacity = m_fillOpacity;
    }
}

void LSvgFillStyle::revert(LPainter *p, LSvgExtraStates &states)
{
    if (m_fillOpacitySet){
        states.fillOpacity = m_oldFillOpacity;
    }
    if (m_fillSet){
        p->setBrush(m_oldFill);
    }
    if (m_fillRuleSet){
        states.fillRule = m_oldFillRule;
    }
}

LSvgViewportFillStyle::LSvgViewportFillStyle(const std::shared_ptr<LBrush> brush)
    : m_viewportFill(brush)
{
}

void LSvgViewportFillStyle::apply(LPainter *p, const std::shared_ptr<LSvgNode>, LSvgExtraStates &)
{
    m_oldFill = p->brush();
    p->setBrush(m_viewportFill);
}

void LSvgViewportFillStyle::revert(LPainter *p, LSvgExtraStates &)
{
    p->setBrush(m_oldFill);
}

LSvgFontStyle::LSvgFontStyle(std::shared_ptr<LSvgFont> font, std::shared_ptr<LSvgDocument> doc)
    : m_svgFont(font)
    , m_doc(doc)
    , m_familySet(0)
    , m_sizeSet(0)
    , m_styleSet(0)
    , m_variantSet(0)
    , m_weightSet(0)
    , m_textAnchorSet(0)
{
}

LSvgFontStyle::LSvgFontStyle()
    : m_familySet(0)
    , m_sizeSet(0)
    , m_styleSet(0)
    , m_variantSet(0)
    , m_weightSet(0)
    , m_textAnchorSet(0)
{
}

void LSvgFontStyle::apply(LPainter *p, const std::shared_ptr<LSvgNode>, LSvgExtraStates &states)
{
    m_oldFont = p->font();
    m_oldSvgFont = states.svgFont;
    m_oldTextAnchor = states.textAnchor;
    m_oldWeight = states.fontWeight;

    if (m_textAnchorSet)
        states.textAnchor = m_textAnchor;

    std::shared_ptr<LFont> font = std::make_shared<LFont>(m_oldFont);
    if (m_familySet) {
        states.svgFont = m_svgFont;
        font->setFamily(m_font->family());
    }

    if (m_sizeSet){
        font->setPointSizeF(m_font->pointSizeF());
    }

    if (m_styleSet){
        font->setStyle(m_font->style());
    }

    if (m_variantSet){
        font->setCapitalization(m_font->capitalization());
    }

    if (m_weightSet) {
        if (m_weight == BOLDER) {
            states.fontWeight =std::min<int>(states.fontWeight + 100, static_cast<int>(LFont::Black));
        } else if (m_weight == LIGHTER) {
            states.fontWeight = std::max<int>(states.fontWeight - 100, static_cast<int>(LFont::Thin));
        } else {
            states.fontWeight = m_weight;
        }
        font->setWeight(lBound(static_cast<int>(LFont::Weight::Thin), states.fontWeight, static_cast<int>(LFont::Weight::Black)));
    }
    p->setFont(font);
}

void LSvgFontStyle::revert(LPainter *p, LSvgExtraStates &states)
{
    p->setFont(m_oldFont);
    states.svgFont = m_oldSvgFont;
    states.textAnchor = m_oldTextAnchor;
    states.fontWeight = m_oldWeight;
}

LSvgStrokeStyle::LSvgStrokeStyle()
    : m_strokeOpacity(1.0)
    , m_oldStrokeOpacity(0.0)
    , m_strokeDashOffset(0)
    , m_oldStrokeDashOffset(0)
    , m_style(0)
    , m_paintStyleResolved(1)
    , m_vectorEffect(0)
    , m_oldVectorEffect(0)
    , m_strokeSet(0)
    , m_strokeDashArraySet(0)
    , m_strokeDashOffsetSet(0)
    , m_strokeLineCapSet(0)
    , m_strokeLineJoinSet(0)
    , m_strokeMiterLimitSet(0)
    , m_strokeOpacitySet(0)
    , m_strokeWidthSet(0)
    , m_vectorEffectSet(0)
{
    m_stroke = std::make_shared<LPen>();
}

void LSvgStrokeStyle::apply(LPainter *p, const std::shared_ptr<LSvgNode> n, LSvgExtraStates &states)
{
    m_oldStroke = p->pen();
    m_oldStrokeOpacity = states.strokeOpacity;
    m_oldStrokeDashOffset = states.strokeDashOffset;
    m_oldVectorEffect = states.vectorEffect;

    std::shared_ptr<LPen> pen = p->pen();

    double oldWidth = pen->m_width;
    double width = m_stroke->m_width;
    if (oldWidth == 0){
        oldWidth = 1;
    }
    if (width == 0){
        width = 1;
    }
    double scale = oldWidth / width;

    if (m_strokeOpacitySet){
        states.strokeOpacity = m_strokeOpacity;
    }

    if (m_vectorEffectSet){
        states.vectorEffect = m_vectorEffect;
    }

    if (m_strokeSet) {
        if (m_style){
            pen->setBrush(m_style->brush(p, n, states));
        }else{
            pen->setBrush(m_stroke->brush());
        }
    }

    if (m_strokeWidthSet){
        pen->m_width = m_stroke->m_width;
    }

    bool setDashOffsetNeeded = false;

    if (m_strokeDashOffsetSet) {
        states.strokeDashOffset = m_strokeDashOffset;
        setDashOffsetNeeded = true;
    }

    if (m_strokeDashArraySet) {
        if (m_stroke->m_style== LPenStyle::Solid) {
            pen->m_style = LPenStyle::Solid;
        } else if (m_strokeWidthSet || oldWidth == 1) {
            // If both width and dash array was set, the dash array is already scaled correctly.
            pen->setDashPattern(m_stroke->dashPattern());
            setDashOffsetNeeded = true;
        } else {
            // If dash array was set, but not the width, the dash array has to be scaled with respect to the old width.
            std::vector<double> dashes = m_stroke->dashPattern();
            for (int i = 0; i < dashes.size(); ++i){
                dashes[i] /= oldWidth;
            }
            pen->setDashPattern(dashes);
            setDashOffsetNeeded = true;
        }
    } else if (m_strokeWidthSet && pen->m_style != LPenStyle::Solid && scale != 1) {
        // If the width was set, but not the dash array, the old dash array must be scaled with respect to the new width.
        std::vector<double> dashes = pen->dashPattern();
        for (int i = 0; i < dashes.size(); ++i){
            dashes[i] *= scale;
        }
        pen->setDashPattern(dashes);
        setDashOffsetNeeded = true;
    }

    if (m_strokeLineCapSet){
        pen->m_capStyle = m_stroke->m_capStyle;
    }
    if (m_strokeLineJoinSet){
        pen->m_joinStyle = m_stroke->m_joinStyle;
    }
    if (m_strokeMiterLimitSet){
        pen->m_miterLimit = m_stroke->m_miterLimit;
    }


    if (setDashOffsetNeeded && pen->m_style != LPenStyle::Solid) {
        double currentWidth = pen->m_width;
        if (currentWidth == 0){
            currentWidth = 1;
        }
        pen->m_dashOffset = (states.strokeDashOffset / currentWidth);
    }

    pen->m_cosmetic = states.vectorEffect;
    p->setPen(pen);
}

void LSvgStrokeStyle::revert(LPainter *p, LSvgExtraStates &states)
{
    p->setPen(m_oldStroke);
    states.strokeOpacity = m_oldStrokeOpacity;
    states.strokeDashOffset = m_oldStrokeDashOffset;
    states.vectorEffect = m_oldVectorEffect;
}

void LSvgStrokeStyle::setDashArray(const std::vector<double> &dashes)
{
    if (m_strokeWidthSet) {
        std::vector<double> d = dashes;
        double w = m_stroke->m_width;
        if (w != 0 && w != 1) {
            for (int i = 0; i < d.size(); ++i){
                d[i] /= w;
            }
        }
        m_stroke->setDashPattern(d);
    } else {
        m_stroke->setDashPattern(dashes);
    }
    m_strokeDashArraySet = 1;
}

LSvgSolidColorStyle::LSvgSolidColorStyle(const LColor &color)
    : m_solidColor(color)
{
}

LSvgGradientStyle::LSvgGradientStyle(std::shared_ptr<LGradient> grad)
    : m_gradient(grad), m_gradientStopsSet(false)
{
}

std::shared_ptr<LBrush> LSvgGradientStyle::brush(LPainter *, const std::shared_ptr<LSvgNode> , LSvgExtraStates &)
{
    if (!m_link.empty()) {
        resolveStops();
    }

    // If the gradient is marked as empty, insert transparent black
    if (!m_gradientStopsSet) {
        m_gradient->m_stops.clear();
        m_gradient->m_stops.push_back(LGradientStop(0.0, LColor(0, 0, 0, 0)));
        m_gradientStopsSet = true;
    }

    std::shared_ptr<LBrush> b = std::make_shared<LBrush>(m_gradient);

    if (!m_transform.isIdentity()){
        b->m_transform = m_transform;
    }
    return b;
}


void LSvgGradientStyle::setTransform(const LTransform2D &transform)
{
    m_transform = transform;
}

LSvgPatternStyle::LSvgPatternStyle(std::shared_ptr<LSvgPattern> pattern)
    : m_pattern(pattern)
{

}

std::shared_ptr<LBrush> LSvgPatternStyle::brush(LPainter *p, const std::shared_ptr<LSvgNode> node, LSvgExtraStates &states)
{
    std::shared_ptr<LSvgPattern> pattern = m_pattern;
    if(pattern != nullptr){
        m_patternImage = pattern->patternImage(p, states, node);
        std::shared_ptr<LBrush> b = std::make_shared<LBrush>(m_patternImage);
        b->m_transform = pattern->appliedTransform();
        return b;
    }
    return nullptr;
}

LSvgTransformStyle::LSvgTransformStyle(const LTransform2D &trans)
    : m_transform(trans)
{
}

void LSvgTransformStyle::apply(LPainter *p, const std::shared_ptr<LSvgNode>, LSvgExtraStates &)
{
    m_oldWorldTransform.push(p->worldTransform());
    p->setWorldTransform(m_transform, true);
}

void LSvgTransformStyle::revert(LPainter *p, LSvgExtraStates &)
{
    if(!m_oldWorldTransform.empty()){
        p->setWorldTransform(m_oldWorldTransform.top(), false);
        m_oldWorldTransform.pop();
    }
}

LSvgStyleProperty::Type LSvgQualityStyle::type() const
{
    return QUALITY;
}

LSvgStyleProperty::Type LSvgFillStyle::type() const
{
    return FILL;
}

LSvgStyleProperty::Type LSvgViewportFillStyle::type() const
{
    return VIEWPORT_FILL;
}

LSvgStyleProperty::Type LSvgFontStyle::type() const
{
    return FONT;
}

LSvgStyleProperty::Type LSvgStrokeStyle::type() const
{
    return STROKE;
}

LSvgStyleProperty::Type LSvgSolidColorStyle::type() const
{
    return SOLID_COLOR;
}

LSvgStyleProperty::Type LSvgGradientStyle::type() const
{
    return GRADIENT;
}

LSvgStyleProperty::Type LSvgPatternStyle::type() const
{
    return PATTERN;
}

LSvgStyleProperty::Type LSvgTransformStyle::type() const
{
    return TRANSFORM;
}


LSvgCompOpStyle::LSvgCompOpStyle(LCompositionMode mode)
    : m_mode(mode)
{

}

void LSvgCompOpStyle::apply(LPainter *p, const std::shared_ptr<LSvgNode>, LSvgExtraStates &)
{
    m_oldMode = p->compositionMode();
    p->setCompositionMode(m_mode);
}

void LSvgCompOpStyle::revert(LPainter *p, LSvgExtraStates &)
{
    p->setCompositionMode(m_oldMode);
}

LSvgStyleProperty::Type LSvgCompOpStyle::type() const
{
    return COMP_OP;
}

LSvgOpacityStyle::LSvgOpacityStyle(double opacity)
    : m_opacity(opacity), m_oldOpacity(0)
{

}

void LSvgOpacityStyle::apply(LPainter *p, const std::shared_ptr<LSvgNode>, LSvgExtraStates &)
{
    m_oldOpacity = p->opacity();
    p->setOpacity(m_opacity * m_oldOpacity);
}

void LSvgOpacityStyle::revert(LPainter *p, LSvgExtraStates &)
{
    p->setOpacity(m_oldOpacity);
}

LSvgStyleProperty::Type LSvgOpacityStyle::type() const
{
    return OPACITY;
}

void LSvgGradientStyle::setStopLink(const std::string &link, std::shared_ptr<LSvgDocument> doc)
{
    m_link = link;
    m_doc  = doc;
}

void LSvgGradientStyle::resolveStops()
{
    std::list<std::string> visited;
    resolveStopsHelper(&visited);
}

void LSvgGradientStyle::resolveStopsHelper(std::list<std::string> *visited)
{
    std::shared_ptr<LSvgDocument> doc = m_doc.lock();
    if ((!m_link.empty()) && (doc!= nullptr)) {
        std::shared_ptr<LSvgStyleProperty> prop = doc->styleProperty(m_link);
        if ((prop != nullptr)) {
            std::list<std::string>::iterator it = std::find(visited->begin(), visited->end(), m_link);
            if(it != visited->end()){
                visited->push_back(m_link);
                if (prop->type() == LSvgStyleProperty::GRADIENT) {
                    std::shared_ptr<LSvgGradientStyle> st = std::dynamic_pointer_cast<LSvgGradientStyle>(prop);
                    st->resolveStopsHelper(visited);
                    m_gradient->m_stops = st->m_gradient->m_stops;
                    m_gradientStopsSet = st->m_gradientStopsSet;
                    m_link.clear();
                }
            }
        }else{
            lWarn() << "Could not resolve property :" << m_link;
        }
        m_link.clear();
    }
}

LSvgStaticStyle::LSvgStaticStyle()
    : quality(0)
    , fill(0)
    , viewportFill(0)
    , font(0)
    , stroke(0)
    , solidColor(0)
    , gradient(0)
    , pattern(0)
    , transform(0)
    , opacity(0)
    , compop(0)
{
}

LSvgStaticStyle::~LSvgStaticStyle()
{
}

void LSvgStaticStyle::apply(LPainter *p, const std::shared_ptr<LSvgNode> node, LSvgExtraStates &states)
{
    if (quality) {
        quality->apply(p, node, states);
    }

    if (fill) {
        fill->apply(p, node, states);
    }

    if (viewportFill) {
        viewportFill->apply(p, node, states);
    }

    if (font) {
        font->apply(p, node, states);
    }

    if (stroke) {
        stroke->apply(p, node, states);
    }

    if (transform) {
        transform->apply(p, node, states);
    }

    if (opacity) {
        opacity->apply(p, node, states);
    }

    if (compop) {
        compop->apply(p, node, states);
    }
}

void LSvgStaticStyle::revert(LPainter *p, LSvgExtraStates &states)
{
    if (quality) {
        quality->revert(p, states);
    }

    if (fill) {
        fill->revert(p, states);
    }

    if (viewportFill) {
        viewportFill->revert(p, states);
    }

    if (font) {
        font->revert(p, states);
    }

    if (stroke) {
        stroke->revert(p, states);
    }

    if (transform) {
        transform->revert(p, states);
    }

    if (opacity) {
        opacity->revert(p, states);
    }

    if (compop) {
        compop->revert(p, states);
    }
}

LSvgAnimatedStyle::LSvgAnimatedStyle()
{
}

LSvgAnimatedStyle::~LSvgAnimatedStyle()
{
}

void LSvgAnimatedStyle::apply(LPainter *p, const std::shared_ptr<LSvgNode> node, LSvgExtraStates &states)
{
    std::shared_ptr<LSvgAbstractAnimator> animator = node->document()->animator();
    std::list<std::shared_ptr<LSvgAbstractAnimation> > nodeAnims = animator->animationsForNode(node);

    savePaintingState(p, node, states);
    if (nodeAnims.empty()){
        return;
    }

    for (const std::shared_ptr<LSvgAbstractAnimation>& anim : nodeAnims) {
        if (!anim->isActive()){
            continue;
        }

        bool replace = true;
        if(anim->animationType() != LSvgAbstractAnimation::CSS){
            std::shared_ptr<LSvgAnimateNode> node = std::dynamic_pointer_cast<LSvgAnimateNode>(anim);
            replace = node->additiveType() == LSvgAnimateNode::Replace;
        }

        std::list<std::shared_ptr<LSvgAbstractAnimatedProperty> > props = anim->properties();
        for (const auto& prop : props){
            applyPropertyAnimation(p, prop, replace);
        }
    }
}

void LSvgAnimatedStyle::revert(LPainter *p, LSvgExtraStates &states)
{
    p->setWorldTransform(m_worldTransform, false);
    p->setBrush(m_brush);
    p->setPen(m_pen);
}

void LSvgAnimatedStyle::savePaintingState(const LPainter *p, const std::shared_ptr<LSvgNode> node, LSvgExtraStates &states)
{
    const LSvgStaticStyle& style = node->style();
    m_worldTransform = m_transformToNode = p->worldTransform();
    if (style.transform != nullptr){
        m_transformToNode = style.transform->transform().inverted() * m_transformToNode;
    }
    m_brush = p->brush();
    m_pen = p->pen();
}

void LSvgAnimatedStyle::applyPropertyAnimation(LPainter *p,
                                               std::shared_ptr<LSvgAbstractAnimatedProperty> property,
                                               bool replace)
{
    if (property->propertyName() == "fill") {
        std::shared_ptr<LBrush> brush = p->brush();
        LColor brushColor = brush->m_color;
        LColor animatedColor = LColor::uintToColor(property->interpolatedValue().toUInt());
        brush->m_color = (replace == true ? animatedColor : sumColor(brushColor, animatedColor));
        p->setBrush(brush);
    } else if (property->propertyName() == "stroke") {
        std::shared_ptr<LPen> pen = p->pen();
        std::shared_ptr<LBrush> penBrush = pen->brush();
        LColor penColor = penBrush->m_color;
        LColor animatedColor = LColor::uintToColor(property->interpolatedValue().toUInt());
        penBrush->m_color = (replace == true ? animatedColor : sumColor(penColor, animatedColor));
        penBrush->m_style = LBrush::SolidPattern;
        pen->setBrush(penBrush);
        p->setPen(pen);
    } else if (property->propertyName() == "transform") {
        LTransform2D t;
        t.fromCbor(property->interpolatedValue().toCbor());
        if (replace){
            p->setWorldTransform(t * m_transformToNode);
        }else{
            p->setWorldTransform(t * p->worldTransform());
        }
    }
}


LK_NAMESPACE_END
