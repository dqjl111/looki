#include "lsvgnode.h"
#include "lsvgdocument.h"

LK_NAMESPACE_BEGIN

#if !defined(LK_SVG_SIZE_LIMIT)
#  define LK_SVG_SIZE_LIMIT 32767
#endif

LSvgNode::LSvgNode(std::shared_ptr<LSvgNode> parent)
    : m_parent(parent),
    m_visible(true),
    m_displayMode(BlockMode)
{
}

LSvgNode::~LSvgNode()
{

}

void LSvgNode::draw(LPainter *p, LSvgExtraStates &states)
{
    if (shouldDrawNode(p, states)) {
        applyStyle(p, states);
        applyAnimatedStyle(p, states);
        std::shared_ptr<LSvgMask> maskNode = std::dynamic_pointer_cast<LSvgMask>(this->hasMask() ? document()->namedNode(this->maskId()) : nullptr);
        std::shared_ptr<LSvgFilterContainer> filterNode = this->hasFilter() ? std::dynamic_pointer_cast<LSvgFilterContainer>(document()->namedNode(this->filterId()))
                                                            : nullptr;
        if (filterNode!=nullptr && filterNode->type() == LSvgNode::Filter && filterNode->supported()) {
            LTransform2D xf = p->transform();
            p->resetTransform();
            LRect<double> localRect = internalBounds(p, states);
            p->setTransform(xf);
            LRect<double> boundsRect = xf.mapRect(filterNode->filterRegion(localRect));
            std::shared_ptr<LImage> proxy = drawIntoBuffer(p, states, boundsRect.toRect());
            proxy = filterNode->applyFilter(proxy, p, localRect);
            if (maskNode !=nullptr && maskNode->type() == LSvgNode::Mask) {
                boundsRect = LRect<double>(proxy->offset().m_x, proxy->offset().m_y, proxy->width(), proxy->height(), false);
                localRect = p->transform().inverted().mapRect(boundsRect);
                std::shared_ptr<LImage> mask = maskNode->createMask(p, states, localRect, &boundsRect);
                applyMaskToBuffer(proxy, mask);
            }
            applyBufferToCanvas(p, proxy);

        } else if (maskNode !=nullptr && maskNode->type() == LSvgNode::Mask) {
            LRect<double> boundsRect;
            std::shared_ptr<LSvgNode> tThis = getPtrT<LSvgNode>();
            std::shared_ptr<LImage> mask = maskNode->createMask(p, states, tThis, &boundsRect);
            drawWithMask(p, states, mask, boundsRect.toRect());
        } else if (!lFuzzyCompare(p->opacity(), double(1.0)) && requiresGroupRendering()) {
            LTransform2D xf = p->transform();
            p->resetTransform();

            LRect<double> localRect = decoratedInternalBounds(p, states);
            // adding safety border needed because of the antialiazing effects
            LRect<double> boundsRect = xf.mapRect(localRect);
            const int deltaX = boundsRect.width() * 0.1;
            const int deltaY = boundsRect.height() * 0.1;
            boundsRect = boundsRect.adjusted(-deltaX, -deltaY, deltaX, deltaY);

            p->setTransform(xf);

            std::shared_ptr<LImage> proxy = drawIntoBuffer(p, states, boundsRect.toAlignedRect());
            applyBufferToCanvas(p, proxy);
        } else {
            if (separateFillStroke(states)){
                fillThenStroke(p, states);
            }else{
                drawCommand(p, states);
            }

        }
        revertAnimatedStyle(p ,states);
        revertStyle(p, states);
    }
}

void LSvgNode::fillThenStroke(LPainter *p, LSvgExtraStates &states)
{
    double oldOpacity = p->opacity();
    std::shared_ptr<LPen> tPen = p->pen()->backPen();
    if (p->brush()->m_style != LBrush::NoPattern) {
        p->setPen(LPenStyle::No);
        p->setOpacity(oldOpacity * states.fillOpacity);

        drawCommand(p, states);
        p->setPen(tPen);
    }
    tPen = p->pen();
    if (tPen->m_style != LPenStyle::No && tPen->brush()->m_style != LBrush::NoPattern && tPen->m_width != 0) {
        std::shared_ptr<LBrush> oldBrush = p->brush()->backBrush();
        p->setOpacity(oldOpacity * states.strokeOpacity);
        p->setBrush(LBrush::NoPattern);
        drawCommand(p, states);

        p->setBrush(oldBrush);
    }
    p->setOpacity(oldOpacity);
}

void LSvgNode::drawWithMask(LPainter *p, LSvgExtraStates &states, const std::shared_ptr<LImage> mask, const LRect<int> &boundsRect)
{
    std::shared_ptr<LImage> proxy = drawIntoBuffer(p, states, boundsRect);
    if (proxy == nullptr || proxy->isNull()){
        return;
    }
    applyMaskToBuffer(proxy, mask);

    p->save();
    p->resetTransform();
    p->drawImage(boundsRect.toRectF(), proxy, LRect<double>(0,0, proxy->width(), proxy->height(), false));
    p->restore();
}

std::shared_ptr<LImage> LSvgNode::drawIntoBuffer(LPainter *p, LSvgExtraStates &states, const LRect<int> &boundsRect)
{
    std::shared_ptr<LImage> proxy
        = std::make_shared<LImage>(boundsRect.size(), LImage::Format::ARGB32Premultiplied);

    proxy->setOffset(boundsRect.topLeft());
    proxy->fill(LColorID::Transparent);
    LPainter proxyPainter(proxy);
    proxyPainter.setPen(p->pen());
    proxyPainter.setBrush(p->brush());
    proxyPainter.setFont(p->font());
    proxyPainter.translate(-boundsRect.topLeft().toPointF());
    proxyPainter.setTransform(p->transform(), true);
    proxyPainter.setRenderHints(p->renderHints());
    if (separateFillStroke(states)){
        fillThenStroke(&proxyPainter, states);
    }else{
        drawCommand(&proxyPainter, states);
    }
    return proxy;
}

void LSvgNode::applyMaskToBuffer(std::shared_ptr<LImage> proxy, std::shared_ptr<LImage> mask) const
{
    if((proxy != nullptr) && (mask != nullptr)){
        LPainter proxyPainter(proxy);
        proxyPainter.setCompositionMode(LCompositionMode::DestinationOut);
        proxyPainter.resetTransform();
        proxyPainter.drawImage(LRect<int>(0, 0, mask->width(), mask->height()).toRectF(), mask, LRect<double>(0,0, mask->width(), mask->height(), false));
    }
}

void LSvgNode::applyBufferToCanvas(LPainter *p, std::shared_ptr<LImage> proxy) const
{
    LTransform2D xf = p->transform();
    p->resetTransform();
    p->drawImage(LRect<int>(proxy->offset(), proxy->size()).toRectF(), proxy, LRect<double>(0,0, proxy->width(), proxy->height(), false));
    p->setTransform(xf);
}

bool LSvgNode::isDescendantOf(const std::shared_ptr<LSvgNode> parent) const
{
    std::shared_ptr<LSvgNode> n = getPtrT<LSvgNode>();
    while (n) {
        if (n == parent){
            return true;
        }
        n = n->m_parent.lock();
    }
    return false;
}

void LSvgNode::appendStyleProperty(std::shared_ptr<LSvgStyleProperty> prop, const std::string &id)
{
    switch (prop->type()) {
    case LSvgStyleProperty::QUALITY:{
        m_style.quality = std::dynamic_pointer_cast<LSvgQualityStyle>(prop);
        break;
    }
    case LSvgStyleProperty::FILL:{
        m_style.fill = std::dynamic_pointer_cast<LSvgFillStyle>(prop);
        break;
    }
    case LSvgStyleProperty::VIEWPORT_FILL:{
        m_style.viewportFill = std::dynamic_pointer_cast<LSvgViewportFillStyle>(prop);
        break;
    }
    case LSvgStyleProperty::FONT:{
        m_style.font = std::dynamic_pointer_cast<LSvgFontStyle>(prop);
        break;
    }
    case LSvgStyleProperty::STROKE:{
        m_style.stroke = std::dynamic_pointer_cast<LSvgStrokeStyle>(prop);
        break;
    }
    case LSvgStyleProperty::SOLID_COLOR:{
        m_style.solidColor = std::dynamic_pointer_cast<LSvgSolidColorStyle>(prop);
        std::shared_ptr<LSvgDocument> doc = document();
        if (doc!= nullptr && !id.empty()){
            doc->addNamedStyle(id, m_style.solidColor);
        }
        break;
    }
    case LSvgStyleProperty::GRADIENT:{
        m_style.gradient = std::dynamic_pointer_cast<LSvgGradientStyle>(prop);
        std::shared_ptr<LSvgDocument> doc = document();
        if (doc!= nullptr && !id.empty()){
            doc->addNamedStyle(id, m_style.gradient);
        }
        break;
    }
    case LSvgStyleProperty::PATTERN:{
        m_style.pattern = std::dynamic_pointer_cast<LSvgPatternStyle>(prop);
        std::shared_ptr<LSvgDocument> doc = document();
        if (doc!= nullptr && !id.empty()){
            doc->addNamedStyle(id, m_style.pattern);
        }
        break;
    }
    case LSvgStyleProperty::TRANSFORM:{
        m_style.transform = std::dynamic_pointer_cast<LSvgTransformStyle>(prop);
        break;
    }
    case LSvgStyleProperty::OPACITY:{
        m_style.opacity = std::dynamic_pointer_cast<LSvgOpacityStyle>(prop);
        break;
    }
    case LSvgStyleProperty::COMP_OP:{
        m_style.compop = std::dynamic_pointer_cast<LSvgCompOpStyle>(prop);
        break;
    }
    default:
        lWarn() << "LSvgNode: Trying to append unknown property!";
        break;
    }
}

void LSvgNode::applyStyle(LPainter *p, LSvgExtraStates &states) const
{
    std::shared_ptr<LSvgNode> tThis =getPtrT<LSvgNode>();
    m_style.apply(p, tThis, states);
}

/*!
    \internal

    Apply the styles of all parents to the painter and the states.
    The styles are applied from the top level node to the current node.
    This function can be used to set the correct style for a node
    if it's draw function is triggered out of the ordinary draw context,
    for example the mask node, that is cross-referenced.
*/
void LSvgNode::applyStyleRecursive(LPainter *p, LSvgExtraStates &states) const
{
    std::shared_ptr<LSvgNode> pa = parent();
    if (pa!= nullptr){
        pa->applyStyleRecursive(p, states);
    }
    applyStyle(p, states);
}

void LSvgNode::revertStyle(LPainter *p, LSvgExtraStates &states) const
{
    m_style.revert(p, states);
}

void LSvgNode::revertStyleRecursive(LPainter *p, LSvgExtraStates &states) const
{
    revertStyle(p, states);
    std::shared_ptr<LSvgNode> pa = parent();
    if (pa!= nullptr){
        pa->revertStyleRecursive(p, states);
    }
}

void LSvgNode::applyAnimatedStyle(LPainter *p, LSvgExtraStates &states) const
{
    std::shared_ptr<LSvgDocument> doc = document();
    if(doc != nullptr){
        if (doc->animated()){
            std::shared_ptr<LSvgNode> tThis = getPtrT<LSvgNode>();
            m_animatedStyle.apply(p, tThis, states);
        }
    }
}

void LSvgNode::revertAnimatedStyle(LPainter *p, LSvgExtraStates &states) const
{
    std::shared_ptr<LSvgDocument> doc = document();
    if(doc != nullptr){
        if (doc->animated()){
            m_animatedStyle.revert(p, states);
        }
    }
}

std::shared_ptr<LSvgStyleProperty> LSvgNode::styleProperty(LSvgStyleProperty::Type type) const
{
    std::shared_ptr<LSvgNode> node = getPtrT<LSvgNode>();
    while (node != nullptr) {
        switch (type) {
        case LSvgStyleProperty::QUALITY:
            if (node->m_style.quality){
                return node->m_style.quality;
            }
            break;
        case LSvgStyleProperty::FILL:
            if (node->m_style.fill){
                return node->m_style.fill;
            }
            break;
        case LSvgStyleProperty::VIEWPORT_FILL:
            if (m_style.viewportFill){
                return node->m_style.viewportFill;
            }
            break;
        case LSvgStyleProperty::FONT:
            if (node->m_style.font){
                return node->m_style.font;
            }
            break;
        case LSvgStyleProperty::STROKE:
            if (node->m_style.stroke){
                return node->m_style.stroke;
            }
            break;
        case LSvgStyleProperty::SOLID_COLOR:
            if (node->m_style.solidColor){
                return node->m_style.solidColor;
            }
            break;
        case LSvgStyleProperty::GRADIENT:
            if (node->m_style.gradient){
                return node->m_style.gradient;
            }
            break;
        case LSvgStyleProperty::PATTERN:
            if (node->m_style.pattern){
                return node->m_style.pattern;
            }
            break;
        case LSvgStyleProperty::TRANSFORM:
            if (node->m_style.transform){
                return node->m_style.transform;
            }
            break;
        case LSvgStyleProperty::OPACITY:
            if (node->m_style.opacity){
                return node->m_style.opacity;
            }
            break;
        case LSvgStyleProperty::COMP_OP:
            if (node->m_style.compop){
                return node->m_style.compop;
            }
            break;
        default:
            break;
        }
        node = node->parent();
    }

    return 0;
}

std::shared_ptr<LSvgPaintStyleProperty> LSvgNode::styleProperty(const std::string &id) const
{
    std::string rid = id;
    if(rid.starts_with('#')){
        rid.erase(0, 1);
    }

    std::shared_ptr<LSvgDocument> doc = document();
    return doc ? doc->namedStyle(rid) : 0;
}

LRect<double> LSvgNode::internalFastBounds(LPainter *p, LSvgExtraStates &states) const
{
    return internalBounds(p, states);
}

LRect<double> LSvgNode::internalBounds(LPainter *, LSvgExtraStates &) const
{
    return LRect<double>(0, 0, 0, 0);
}

LRect<double> LSvgNode::bounds() const
{
    if (!m_cachedBounds.isEmpty()){
        return m_cachedBounds;
    }

    std::shared_ptr<LImage> dummy = std::make_shared<LImage>(1, 1, LImage::Format::RGB32);
    LPainter p(dummy);
    initPainter(&p);
    LSvgExtraStates states;

    std::shared_ptr<LSvgNode> pa = parent();
    if (pa!= nullptr){
        pa->applyStyleRecursive(&p, states);
    }
    p.setWorldTransform(LTransform2D());
    m_cachedBounds = bounds(&p, states);
    if (pa!= nullptr){// always revert the style to not store old transformations
        pa->revertStyleRecursive(&p, states);
    }
    return m_cachedBounds;
}

std::shared_ptr<LSvgDocument> LSvgNode::document() const
{
    std::shared_ptr<LSvgDocument>  doc = nullptr;
    std::shared_ptr<LSvgNode> node = getPtrT<LSvgNode>();
    while ((node!= nullptr) && node->type() != LSvgNode::Doc) {
        node = node->parent();
    }
    doc = std::dynamic_pointer_cast<LSvgDocument>(node);
    return doc;
}

std::string LSvgNode::typeName() const
{
    switch (type()) {
    case Doc: return ("svg");
    case Group: return ("g");
    case Defs: return ("defs");
    case Switch: return ("switch");
    case AnimateColor: return ("animateColor");
    case AnimateTransform: return ("animateTransform");
    case Circle: return ("circle");
    case Ellipse: return ("ellipse");
    case Image: return ("image");
    case Line: return ("line");
    case Path: return ("path");
    case Polygon: return ("polygon");
    case Polyline: return ("polyline");
    case Rect: return ("rect");
    case Text: return ("text");
    case Textarea: return ("textarea");
    case Tspan: return ("tspan");
    case Use: return ("use");
    case Video: return ("video");
    case Mask: return ("mask");
    case Symbol: return ("symbol");
    case Marker: return ("marker");
    case Pattern: return ("pattern");
    case Filter: return ("filter");
    case FeMerge: return ("feMerge");
    case FeMergenode: return ("feMergeNode");
    case FeColormatrix: return ("feColorMatrix");
    case FeGaussianblur: return ("feGaussianBlur");
    case FeOffset: return ("feOffset");
    case FeComposite: return ("feComposite");
    case FeFlood: return ("feFlood");
    case FeBlend: return ("feBlend");
    case FeUnsupported: return ("feUnsupported");
    }
    return ("unknown");
}

void LSvgNode::setRequiredFeatures(const std::list<std::string> &lst)
{
    m_requiredFeatures = lst;
}

const std::list<std::string> & LSvgNode::requiredFeatures() const
{
    return m_requiredFeatures;
}

void LSvgNode::setRequiredExtensions(const std::list<std::string> &lst)
{
    m_requiredExtensions = lst;
}

const std::list<std::string> & LSvgNode::requiredExtensions() const
{
    return m_requiredExtensions;
}

void LSvgNode::setRequiredLanguages(const std::list<std::string> &lst)
{
    m_requiredLanguages = lst;
}

const std::list<std::string> & LSvgNode::requiredLanguages() const
{
    return m_requiredLanguages;
}

void LSvgNode::setRequiredFormats(const std::list<std::string> &lst)
{
    m_requiredFormats = lst;
}

const std::list<std::string> & LSvgNode::requiredFormats() const
{
    return m_requiredFormats;
}

void LSvgNode::setRequiredFonts(const std::list<std::string> &lst)
{
    m_requiredFonts = lst;
}

const std::list<std::string> & LSvgNode::requiredFonts() const
{
    return m_requiredFonts;
}

void LSvgNode::setVisible(bool visible)
{
    //propagate visibility change of true to the parent
    //not propagating false is just a small performance
    //degradation since we'll iterate over children without
    //drawing any of them
    std::shared_ptr<LSvgNode> p = m_parent.lock();
    if(p != nullptr && visible){
        if(!p->isVisible()){
            p->setVisible(true);
        }
    }
    m_visible = visible;
}

LRect<double> LSvgNode::bounds(LPainter *p, LSvgExtraStates &states) const
{
    applyStyle(p, states);
    LRect<double> rect = internalBounds(p, states);
    revertStyle(p, states);
    return rect;
}

LRect<double> LSvgNode::decoratedInternalBounds(LPainter *p, LSvgExtraStates &states) const
{
    return filterRegion(internalBounds(p, states));
}

LRect<double> LSvgNode::decoratedBounds(LPainter *p, LSvgExtraStates &states) const
{
    applyStyle(p, states);
    LRect<double> rect = decoratedInternalBounds(p, states);
    revertStyle(p, states);
    return rect;
}

void LSvgNode::setNodeId(const std::string &i)
{
    m_id = i;
}

void LSvgNode::setXmlClass(const std::string &str)
{
    m_class = str;
}

std::string LSvgNode::maskId() const
{
    return m_maskId;
}

void LSvgNode::setMaskId(const std::string &str)
{
    m_maskId = str;
}

bool LSvgNode::hasMask() const
{
    if (document()->options().testFlag(LSvgOption::Tiny12FeaturesOnly)){
        return false;
    }
    return !m_maskId.empty();
}

std::string LSvgNode::filterId() const
{
    return m_filterId;
}

void LSvgNode::setFilterId(const std::string &str)
{
    m_filterId = str;
}

bool LSvgNode::hasFilter() const
{
    if (document()->options().testFlag(LSvgOption::Tiny12FeaturesOnly)){
        return false;
    }
    return !m_filterId.empty();
}

std::string LSvgNode::markerStartId() const
{
    return m_markerStartId;
}

void LSvgNode::setMarkerStartId(const std::string &str)
{
    m_markerStartId = str;
}

bool LSvgNode::hasMarkerStart() const
{
    if (document()->options().testFlag(LSvgOption::Tiny12FeaturesOnly)){
        return false;
    }
    return !m_markerStartId.empty();
}

std::string LSvgNode::markerMidId() const
{
    return m_markerMidId;
}

void LSvgNode::setMarkerMidId(const std::string &str)
{
    m_markerMidId = str;
}

bool LSvgNode::hasMarkerMid() const
{
    if (document()->options().testFlag(LSvgOption::Tiny12FeaturesOnly)){
        return false;
    }
    return !m_markerMidId.empty();
}

std::string LSvgNode::markerEndId() const
{
    return m_markerEndId;
}

void LSvgNode::setMarkerEndId(const std::string &str)
{
    m_markerEndId = str;
}

bool LSvgNode::hasMarkerEnd() const
{
    if (document()->options().testFlag(LSvgOption::Tiny12FeaturesOnly)){
        return false;
    }
    return !m_markerEndId.empty();
}

bool LSvgNode::hasAnyMarker() const
{
    if (document()->options().testFlag(LSvgOption::Tiny12FeaturesOnly)){
        return false;
    }
    return hasMarkerStart() || hasMarkerMid() || hasMarkerEnd();
}

bool LSvgNode::requiresGroupRendering() const
{
    return false;
}

void LSvgNode::setDisplayMode(DisplayMode mode)
{
    m_displayMode = mode;
}

LSvgNode::DisplayMode LSvgNode::displayMode() const
{
    return m_displayMode;
}

double LSvgNode::strokeWidth(LPainter *p)
{
    const std::shared_ptr<LPen> pen = p->pen();
    if (pen->m_style == LPenStyle::No || pen->brush()->m_style == LBrush::NoPattern || pen->isCosmetic()){
        return 0;
    }
    return pen->m_width;
}

void LSvgNode::initPainter(LPainter *p)
{
    std::shared_ptr<LPen> pen = std::make_shared<LPen>(nullptr, 1, LPenStyle::Solid, LPenCapStyle::Flat, LPenJoinStyle::SvgMiter);
    pen->m_miterLimit = 4;
    p->setPen(pen);
    p->setBrush(LColorID::Black);
    p->setRenderHints(LRenderHint::Antialiasing|LRenderHint::SmoothPixmapTransform);
    std::shared_ptr<LFont> font = p->font();
    if (font->pointSize() < 0 && font->pixelSize() > 0) {
        font = std::make_shared<LFont>(font);
        font->setPointSizeF(font->pixelSize() * 72.0 / p->device()->logicalDpiY());
        p->setFont(font);
    }
}

LRect<double> LSvgNode::boundsOnStroke(LPainter *p,
                                    const std::shared_ptr<LPainterPath> path,
                                    double width, BoundsMode mode)
{
    LPainterPathStroker stroker;
    stroker.setWidth(width);
    if (mode == BoundsMode::IncludeMiterLimit) {
        std::shared_ptr<LPen> pen = p->pen();
        stroker.setJoinStyle(pen->m_joinStyle);
        stroker.setMiterLimit(pen->m_miterLimit);
    }
    std::shared_ptr<LPainterPath> stroke = stroker.createStroke(path);
    return p->transform().map(stroke)->boundingRect();
}

bool LSvgNode::shouldDrawNode(LPainter *p, LSvgExtraStates &states) const
{
    if (m_displayMode == DisplayMode::NoneMode){
        return false;
    }

    if (document() && document()->options().testFlag(LSvgOption::AssumeTrustedSource)){
        return true;
    }

    LRect<double> brect = internalFastBounds(p, states);
    if (brect.width() <= LK_SVG_SIZE_LIMIT && brect.height() <= LK_SVG_SIZE_LIMIT) {
        return true;
    } else {
        return false;
    }
}

LRect<double> LSvgNode::filterRegion(LRect<double> bounds) const
{
    if(hasFilter()){
        std::shared_ptr<LSvgNode> t = document()->namedNode(filterId());
        std::shared_ptr<LSvgFilterContainer> filterNode = std::dynamic_pointer_cast<LSvgFilterContainer>(t);
        if(filterNode != nullptr){
            if(filterNode->type() == LSvgNode::Filter && filterNode->supported()){
                return filterNode->filterRegion(bounds);
            }
        }
        return bounds;
    }else{
        return bounds;
    }
}

LK_NAMESPACE_END
