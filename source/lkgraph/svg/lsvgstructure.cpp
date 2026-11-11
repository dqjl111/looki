#include "lsvgstructure.h"
#include "lsvgdocument.h"
#include "lsvgfilter.h"
#include "lsvggraphics.h"
#include <cstddef>

LK_NAMESPACE_BEGIN

LSvgG::LSvgG(std::shared_ptr<LSvgNode>  parent)
    : LSvgStructure(parent)
{

}

void LSvgG::drawCommand(LPainter *p, LSvgExtraStates &states)
{
    std::list<std::shared_ptr<LSvgNode>>::iterator itr = m_renderers.begin();
    while (itr != m_renderers.end()) {
        std::shared_ptr<LSvgNode> node = *itr;
        if ((node->isVisible()) && (node->displayMode() != LSvgNode::NoneMode)){
            node->draw(p, states);
        }
        ++itr;
    }
}

bool LSvgG::shouldDrawNode(LPainter *, LSvgExtraStates &) const
{
    return true;
}

LSvgNode::Type LSvgG::type() const
{
    return Group;
}

bool LSvgG::requiresGroupRendering() const
{
    return m_renderers.size() > 1;
}

LSvgStructure::LSvgStructure(std::shared_ptr<LSvgNode>  parent)
    :LSvgNode(parent)
{

}

std::shared_ptr<LSvgNode> LSvgStructure::scopeNode(const std::string &id) const
{
    std::shared_ptr<LSvgDocument> doc = document();
    return doc != nullptr ? doc->namedNode(id) : nullptr;
}

void LSvgStructure::addChild(std::shared_ptr<LSvgNode> child, const std::string &id)
{
    if(child == nullptr){
        return;
    }

    if (!id.empty()){
        std::shared_ptr<LSvgDocument> doc = document();
        if (doc != nullptr){
            doc->addNamedNode(id, child);
        }
    }
    m_renderers.push_back(child);
}

LSvgDefs::LSvgDefs(std::shared_ptr<LSvgNode> parent)
    : LSvgStructure(parent)
{
}

bool LSvgDefs::shouldDrawNode(LPainter *, LSvgExtraStates &) const
{
    return false;
}

LSvgNode::Type LSvgDefs::type() const
{
    return Defs;
}

LSvgSymbolLike::LSvgSymbolLike(std::shared_ptr<LSvgNode> parent, LRect<double> bounds, LRect<double> viewBox, LPoint<double> refP,
                               LFlags<LSvgPreserveAspectRatio> pAspectRatios, LSvgSymbolLike::Overflow overflow)
    : LSvgStructure(parent)
    , m_rect(bounds)
    , m_viewBox(viewBox)
    , m_refP(refP)
    , m_pAspectRatios(pAspectRatios)
    , m_overflow(overflow)
{

}

LRect<double> LSvgSymbolLike::decoratedInternalBounds(LPainter *p, LSvgExtraStates &states) const
{
    p->save();
    setPainterToRectAndAdjustment(p);
    LRect<double> rect = internalBounds(p, states);
    p->restore();
    return rect;
}

bool LSvgSymbolLike::requiresGroupRendering() const
{
    return m_renderers.size() > 1;
}

void LSvgSymbolLike::setPainterToRectAndAdjustment(LPainter *p) const
{
    double scaleX = 1;
    if (m_rect.width() > 0 && m_viewBox.width() > 0){
        scaleX = m_rect.width()/m_viewBox.width();
    }
    double scaleY = 1;
    if (m_rect.height() > 0 && m_viewBox.height() > 0){
        scaleY = m_rect.height()/m_viewBox.height();
    }

    if (m_overflow == Overflow::Hidden) {
        LTransform2D t;
        t.translate(- m_refP.m_x * scaleX - m_rect.m_x - m_viewBox.m_x * scaleX,
                    - m_refP.m_y * scaleY - m_rect.m_y - m_viewBox.m_y * scaleY);
        t.scale(scaleX, scaleY);

        if (m_viewBox.isValid()){
            p->setClipRect(t.mapRect(m_viewBox));
        }
    }

    double offsetX = 0;
    double offsetY = 0;

    if (!lFuzzyCompare(scaleX, scaleY) &&
        (m_pAspectRatios & LSvgPreserveAspectRatio::xyMask)) {

        if (m_pAspectRatios & LSvgPreserveAspectRatio::meet){
            scaleX = scaleY = std::min<double>(scaleX, scaleY);
        }else{
            scaleX = scaleY = std::max<double>(scaleX, scaleY);
        }

        double xOverflow = scaleX * m_viewBox.width() - m_rect.width();
        double yOverflow = scaleY * m_viewBox.height() - m_rect.height();

        if ((m_pAspectRatios & LSvgPreserveAspectRatio::xMask) == LSvgPreserveAspectRatio::xMid){
            offsetX -= xOverflow / 2.;
        }
        else if ((m_pAspectRatios & LSvgPreserveAspectRatio::xMask) == LSvgPreserveAspectRatio::xMax){
            offsetX -= xOverflow;
        }

        if ((m_pAspectRatios & LSvgPreserveAspectRatio::yMask) == LSvgPreserveAspectRatio::yMid){
            offsetY -= yOverflow / 2.;
        }
        else if ((m_pAspectRatios & LSvgPreserveAspectRatio::yMask) == LSvgPreserveAspectRatio::yMax){
            offsetY -= yOverflow;
        }
    }

    p->translate(LPoint<double>(offsetX - m_refP.m_x * scaleX, offsetY - m_refP.m_y * scaleY));
    p->scale(scaleX, scaleY);
}

LSvgSymbol::LSvgSymbol(std::shared_ptr<LSvgNode> parent, LRect<double> bounds, LRect<double> viewBox, LPoint<double> refP,
                       LFlags<LSvgPreserveAspectRatio> pAspectRatios,
                       LSvgSymbol::Overflow overflow)
    : LSvgSymbolLike(parent, bounds, viewBox, refP, pAspectRatios, overflow)
{
}

void LSvgSymbol::drawCommand(LPainter *p, LSvgExtraStates &states)
{
    if (!states.inUse) //Symbol is only drawn when within a use node.
        return;

    std::list<std::shared_ptr<LSvgNode>>::iterator itr = m_renderers.begin();

    p->save();
    setPainterToRectAndAdjustment(p);
    while (itr != m_renderers.end()) {
        std::shared_ptr<LSvgNode> node = *itr;
        if ((node->isVisible()) && (node->displayMode() != LSvgNode::NoneMode)){
            node->draw(p, states);
        }
        ++itr;
    }
    p->restore();
}

LSvgNode::Type LSvgSymbol::type() const
{
    return Symbol;
}

LSvgMarker::LSvgMarker(std::shared_ptr<LSvgNode> parent, LRect<double> bounds, LRect<double> viewBox, LPoint<double> refP,
                       LFlags<LSvgPreserveAspectRatio> pAspectRatios, LSvgSymbol::Overflow overflow,
                       Orientation orientation, double orientationAngle, MarkerUnits markerUnits)
    : LSvgSymbolLike(parent, bounds, viewBox, refP, pAspectRatios, overflow)
    , m_orientation(orientation)
    , m_orientationAngle(orientationAngle)
    , m_markerUnits(markerUnits)
{
    // apply the svg standard style
    std::shared_ptr<LSvgFillStyle> fillProp = std::make_shared<LSvgFillStyle>();
    fillProp->setBrush(LColorID::Black);
    appendStyleProperty(fillProp, "");

    std::shared_ptr<LSvgStrokeStyle> strokeProp = std::make_shared<LSvgStrokeStyle>();
    strokeProp->setMiterLimit(4);
    strokeProp->setWidth(1);
    strokeProp->setLineCap(LPenCapStyle::Flat);
    strokeProp->setLineJoin(LPenJoinStyle::SvgMiter);
    strokeProp->setStroke(nullptr);
    appendStyleProperty(strokeProp, "");
}

LSvgFilterContainer::LSvgFilterContainer(std::shared_ptr<LSvgNode> parent, const LSvgRectInfo &bounds,
                                         LSvgUnitTypes filterUnits, LSvgUnitTypes primitiveUnits)
    : LSvgStructure(parent)
    , m_rectInfo(bounds)
    , m_filterUnits(filterUnits)
    , m_primitiveUnits(primitiveUnits)
    , m_supported(true)
{

}

bool LSvgFilterContainer::shouldDrawNode(LPainter *, LSvgExtraStates &) const
{
    return false;
}

void LSvgMarker::drawCommand(LPainter *p, LSvgExtraStates &states)
{
    if (!states.inUse){ //Symbol is only drawn in combination with another node.
        return;
    }

    if (L_UNLIKELY(m_recursing)){
        return;
    }
    m_recursing = true;

    std::list<std::shared_ptr<LSvgNode>>::iterator itr = m_renderers.begin();
    p->save();
    setPainterToRectAndAdjustment(p);

    while (itr != m_renderers.end()) {
        std::shared_ptr<LSvgNode> node = *itr;
        if ((node->isVisible()) && (node->displayMode() != LSvgNode::NoneMode)){
            node->draw(p, states);
        }
        ++itr;
    }
    p->restore();
    m_recursing = false;
}

namespace {

struct PositionMarkerPair {
    double x;
    double y;
    double angle;
    std::string markerId;
    bool isStartNode = false;
};

std::list<PositionMarkerPair> markersForNode(const std::shared_ptr<LSvgNode> node)
{
    if (!node->hasAnyMarker()){
        return std::list<PositionMarkerPair> ();
    }

    auto getMeanAngle = [](LPoint<double> p0, LPoint<double> p1, LPoint<double> p2) -> double {
        LPoint<double> t1 = p1 - p0;
        LPoint<double> t2 = p2 - p1;
        double hyp1 =  hypot(t1.m_x, t1.m_y);
        if (hyp1 > 0){
            t1 /= hyp1;
        }else{
            return 0.;
        }
        double hyp2 =  hypot(t2.m_x, t2.m_y);
        if (hyp2 > 0){
            t2 /= hyp2;
        }else{
            return 0.;
        }
        LPoint<double> tangent = t1 + t2;
        return -atan2(tangent.m_y, tangent.m_x) / std::numbers::pi * 180.;
    };

    std::list<PositionMarkerPair> markers;

    switch (node->type()) {
    case LSvgNode::Line: {
        const std::shared_ptr<LSvgLine> line = std::dynamic_pointer_cast<LSvgLine>(node);
        if (node->hasMarkerStart()){
            markers.push_back(PositionMarkerPair { line->line().m_pt1.m_x, line->line().m_pt1.m_y,
                                          line->line().angle(), line->markerStartId(),
                                          true});
        }
        if (node->hasMarkerEnd()){
            markers.push_back(PositionMarkerPair { line->line().m_pt2.m_x, line->line().m_pt2.m_y,
                                          line->line().angle(), line->markerEndId() });
        }
        break;
    }
    case LSvgNode::Polyline:
    case LSvgNode::Polygon: {
        const std::shared_ptr<LSvgPolyline> polyline = std::dynamic_pointer_cast<LSvgPolyline>(node);
        const std::shared_ptr<LSvgPolygon> polygon = std::dynamic_pointer_cast<LSvgPolygon>(node);

        const LPolygon<double> &polyData = (node->type() == LSvgNode::Polyline)
                                        ? polyline->polygon()
                                        : polygon->polygon();

        if (node->hasMarkerStart() && polyData.size() > 1) {
            LLine<double> line(polyData.at(0), polyData.at(1));
            markers.push_back(PositionMarkerPair { line.m_pt1.m_x,
                                          line.m_pt1.m_y,
                                          line.angle(),
                                          node->markerStartId(),
                                          true });
        }
        if (node->hasMarkerMid()) {
            for (int i = 1; i < polyData.size() - 1; i++) {
                LPoint<double> p0 = polyData.at(i - 1);
                LPoint<double> p1 = polyData.at(i);
                LPoint<double> p2 = polyData.at(i + 1);

                markers.push_back(PositionMarkerPair { p1.m_x,
                                              p1.m_y,
                                              getMeanAngle(p0, p1, p2),
                                              node->markerStartId() });
            }
        }
        if (node->hasMarkerEnd() && polyData.size() > 1) {
            LLine<double> line(polyData.at(polyData.size() - 1), polyData[polyData.size()-1]);
            markers.push_back(PositionMarkerPair { line.m_pt2.m_x,
                                          line.m_pt2.m_y,
                                          line.angle(),
                                          node->markerEndId() });
        }
        break;
    }
    case LSvgNode::Path: {
        const std::shared_ptr<LSvgPath> path = std::dynamic_pointer_cast<LSvgPath>(node);
        std::shared_ptr<LPainterPath> pp = path->path();
        if(pp != nullptr){
            if (node->hasMarkerStart())
                markers.push_back( PositionMarkerPair { pp->pointAtPercent(0.).m_x,
                                              pp->pointAtPercent(0.).m_y,
                                              pp->angleAtPercent(0.),
                                              path->markerStartId(),
                                              true });
            if (node->hasMarkerMid()) {
                for (int i = 1; i < pp->elementCount() - 1; i++) {
                    if (pp->elementAt(i).m_type == LPainterPath::MoveToElement){
                        continue;
                    }
                    if (pp->elementAt(i).m_type == LPainterPath::CurveToElement){
                        continue;
                    }
                    if (( pp->elementAt(i).m_type == LPainterPath::CurveToDataElement &&
                         pp->elementAt(i + 1).m_type != LPainterPath::CurveToDataElement ) ||
                        pp->elementAt(i).m_type == LPainterPath::LineToElement) {

                        LPoint<double> p0(pp->elementAt(i - 1).m_x, pp->elementAt(i - 1).m_y);
                        LPoint<double> p1(pp->elementAt(i).m_x, pp->elementAt(i).m_y);
                        LPoint<double> p2(pp->elementAt(i + 1).m_x, pp->elementAt(i + 1).m_y);

                        markers.push_back( PositionMarkerPair { p1.m_x,
                                                      p1.m_y,
                                                      getMeanAngle(p0, p1, p2),
                                                      path->markerMidId() });
                    }
                }
            }
            if (node->hasMarkerEnd()){
                markers.push_back( PositionMarkerPair { pp->pointAtPercent(1.).m_x,
                                              pp->pointAtPercent(1.).m_y,
                                              pp->angleAtPercent(1.),
                                              path->markerEndId() });
            }
        }
        break;
    }
    default:{
        break;
    }
    }

    return markers;
}

} // anonymous namespace


void LSvgMarker::drawMarkersForNode(std::shared_ptr<LSvgNode> node, LPainter *p, LSvgExtraStates &states)
{
    drawHelper(node, p, states);
}

LRect<double> LSvgMarker::markersBoundsForNode(const std::shared_ptr<LSvgNode> node, LPainter *p, LSvgExtraStates &states)
{
    LRect<double> bounds;
    drawHelper(node, p, states, &bounds);
    return bounds;
}

LSvgNode::Type LSvgMarker::type() const
{
    return Marker;
}

void LSvgMarker::drawHelper(const std::shared_ptr<LSvgNode> node, LPainter *p,
                            LSvgExtraStates &states, LRect<double> *boundingRect)
{

    states.inUse = true;

    const bool isPainting = (boundingRect == nullptr);
    const auto markers = markersForNode(node);
    for (auto &i : markers) {
        std::shared_ptr<LSvgMarker> markNode = std::dynamic_pointer_cast<LSvgMarker>(node->document()->namedNode(i.markerId));
        if (!markNode)
            continue;

        p->save();
        p->translate(LPoint<double>(i.x, i.y));
        if (markNode->orientation() == LSvgMarker::Orientation::Value) {
            p->rotate(markNode->orientationAngle());
        } else {
            p->rotate(-i.angle);
            if (i.isStartNode && markNode->orientation()
                                     == LSvgMarker::Orientation::AutoStartReverse) {
                p->scale(-1, -1);
            }
        }

        LRect<double> oldRect = markNode->m_rect;
        if (markNode->markerUnits() == LSvgMarker::MarkerUnits::StrokeWidth) {
            markNode->m_rect.setWidth(markNode->m_rect.width() * p->pen()->m_width);
            markNode->m_rect.setHeight(markNode->m_rect.height() * p->pen()->m_width);
        }
        if (isPainting)
            markNode->draw(p, states);

        if (boundingRect) {
            LTransform2D xf = p->transform();
            p->resetTransform();
            *boundingRect |=  xf.mapRect(markNode->decoratedInternalBounds(p, states));
        }

        markNode->m_rect = oldRect;
        p->restore();
    }
    states.inUse = false;
}

std::shared_ptr<LImage>  LSvgFilterContainer::applyFilter(const std::shared_ptr<LImage>  &buffer, LPainter *p, const LRect<double> &bounds) const
{
    LRect<double> localFilterRegion = m_rectInfo.resolveRelativeLengths(bounds, m_filterUnits);
    LRect<int> globalFilterRegion = p->transform().mapRect(localFilterRegion).toRect();
    LRect<int> globalFilterRegionRel = globalFilterRegion.translated(-buffer->offset());

    if (globalFilterRegionRel.isEmpty())
        return buffer;

    std::shared_ptr<LImage>  proxy = std::make_shared<LImage>(globalFilterRegionRel.size(), buffer->format());
    proxy = buffer->copy(globalFilterRegionRel);
    proxy->setOffset(globalFilterRegion.topLeft());
    if (proxy->isNull()){
        return buffer;
    }

    std::map<std::string, std::shared_ptr<LImage> > buffers;
    buffers[std::string("")] = proxy;
    buffers[std::string("SourceGraphic")] = proxy;

    bool requiresSourceAlpha = false;

    const std::list<std::shared_ptr<LSvgNode> > children = renderers();
    for (const std::shared_ptr<LSvgNode>& renderer : children) {
        const std::shared_ptr<LSvgFeFilterPrimitive>  filter = LSvgFeFilterPrimitive::castToFilterPrimitive(renderer);
        if (filter!= nullptr && filter->requiresSourceAlpha()) {
            requiresSourceAlpha = true;
            break;
        }
    }

    if (requiresSourceAlpha) {
        std::shared_ptr<LImage>  proxyAlpha = proxy->convertToFormat(LImage::Format::Alpha8);
        proxyAlpha = proxyAlpha->convertToFormat(proxy->format());
        proxyAlpha->setOffset(proxy->offset());
        if (proxyAlpha->isNull()){
            return buffer;
        }
        buffers[std::string("SourceAlpha")] = proxyAlpha;
    }

    std::shared_ptr<LImage>  result;
    for (const std::shared_ptr<LSvgNode>& renderer : children) {
        const std::shared_ptr<LSvgFeFilterPrimitive> filter = LSvgFeFilterPrimitive::castToFilterPrimitive(renderer);
        if (filter != nullptr) {
            result = filter->apply(buffers, p, bounds, localFilterRegion, m_primitiveUnits, m_filterUnits);
            if (result!= nullptr && !result->isNull()) {
                buffers[std::string("")] = result;
                buffers[filter->result()] = result;
            }
        }
    }
    return result;
}

void LSvgFilterContainer::setSupported(bool supported)
{
    m_supported = supported;
}

bool LSvgFilterContainer::supported() const
{
    return m_supported;
}

LRect<double> LSvgFilterContainer::filterRegion(const LRect<double> &itemBounds) const
{
    return m_rectInfo.resolveRelativeLengths(itemBounds, m_filterUnits);
}

LSvgNode::Type LSvgFilterContainer::type() const
{
    return Filter;
}


inline static bool isSupportedSvgFeature(const std::string &str)
{
    static const std::array<std::string, 20> supportFeatures = {
        std::string("http://www.w3.org/Graphics/SVG/feature/1.2/#Text"),
        std::string("http://www.w3.org/Graphics/SVG/feature/1.2/#Shape"),
        std::string("http://www.w3.org/Graphics/SVG/feature/1.2/#SVG"),
        std::string("http://www.w3.org/Graphics/SVG/feature/1.2/#Structure"),
        std::string("http://www.w3.org/Graphics/SVG/feature/1.2/#SolidColor"),
        std::string("http://www.w3.org/Graphics/SVG/feature/1.2/#Hyperlinking"),
        std::string("http://www.w3.org/Graphics/SVG/feature/1.2/#CoreAttribute"),
        std::string("http://www.w3.org/Graphics/SVG/feature/1.2/#XlinkAttribute"),
        std::string("http://www.w3.org/Graphics/SVG/feature/1.2/#SVG-static"),
        std::string("http://www.w3.org/Graphics/SVG/feature/1.2/#OpacityAttribute"),
        std::string("http://www.w3.org/Graphics/SVG/feature/1.2/#Gradient"),
        std::string("http://www.w3.org/Graphics/SVG/feature/1.2/#Font"),
        std::string("http://www.w3.org/Graphics/SVG/feature/1.2/#Image"),
        std::string("http://www.w3.org/Graphics/SVG/feature/1.2/#ConditionalProcessing"),
        std::string("http://www.w3.org/Graphics/SVG/feature/1.2/#Extensibility"),
        std::string("http://www.w3.org/Graphics/SVG/feature/1.2/#GraphicsAttribute"),
        std::string("http://www.w3.org/Graphics/SVG/feature/1.2/#Prefetch"),
        std::string("http://www.w3.org/Graphics/SVG/feature/1.2/#PaintAttribute"),
        std::string("http://www.w3.org/Graphics/SVG/feature/1.2/#ConditionalProcessingAttribute"),
        std::string("http://www.w3.org/Graphics/SVG/feature/1.2/#ExternalResourcesRequiredAttribute")
    };

    for(size_t i=0; i<supportFeatures.size(); ++i){
        if(str == supportFeatures[i]){
            return true;
        }
    }
    return false;
}

static inline bool isSupportedSvgExtension(const std::string &)
{
    return false;
}


LSvgSwitch::LSvgSwitch(std::shared_ptr<LSvgNode> parent)
    : LSvgStructure(parent)
{
    initSvg();
}

std::shared_ptr<LSvgNode> LSvgSwitch::childToRender() const
{
    auto itr = m_renderers.cbegin();

    while (itr != m_renderers.cend()) {
        std::shared_ptr<LSvgNode> node = *itr;
        if (node->isVisible() && (node->displayMode() != LSvgNode::NoneMode)) {
            const std::list<std::string> &features  = node->requiredFeatures();
            const std::list<std::string> &extensions = node->requiredExtensions();
            const std::list<std::string> &languages = node->requiredLanguages();
            const std::list<std::string> &formats = node->requiredFormats();
            const std::list<std::string> &fonts = node->requiredFonts();

            bool okToRender = true;
            if (!features.empty()) {
                std::list<std::string>::const_iterator sitr = features.cbegin();
                for (; sitr != features.cend(); ++sitr) {
                    if (!isSupportedSvgFeature(*sitr)) {
                        okToRender = false;
                        break;
                    }
                }
            }

            if (okToRender && !extensions.empty()) {
                std::list<std::string>::const_iterator sitr = extensions.cbegin();
                for (; sitr != extensions.cend(); ++sitr) {
                    if (!isSupportedSvgExtension(*sitr)) {
                        okToRender = false;
                        break;
                    }
                }
            }

            if (okToRender && !languages.empty()) {
                std::list<std::string>::const_iterator sitr = languages.cbegin();
                okToRender = false;
                for (; sitr != languages.cend(); ++sitr) {
                    if ((*sitr).starts_with(m_systemLanguagePrefix)) {
                        okToRender = true;
                        break;
                    }
                }
            }

            if (okToRender && !formats.empty()){
                okToRender = false;
            }

            if (okToRender && !fonts.empty()){
                okToRender = false;
            }

            if (okToRender){
                return node;
            }
        }

        ++itr;
    }

    return nullptr;
}

void LSvgSwitch::drawCommand(LPainter *p, LSvgExtraStates &states)
{
    std::shared_ptr<LSvgNode> node = childToRender();
    if (node != nullptr){
        node->draw(p, states);
    }
}

LSvgNode::Type LSvgSwitch::type() const
{
    return Switch;
}

void LSvgSwitch::initSvg()
{
    std::locale locale;
    m_systemLanguage = LStringUtil::replace(locale.name(), "_", "-");
    int idx = m_systemLanguage.find('-');
    m_systemLanguagePrefix = m_systemLanguage.substr(0, idx);
}

LRect<double> LSvgStructure::internalBounds(LPainter *p, LSvgExtraStates &states) const
{
    LRect<double> bounds;
    if (!m_recursing) {
        m_recursing = true;
        for (const std::shared_ptr<LSvgNode>& node : m_renderers){
            bounds |= node->bounds(p, states);
        }
        m_recursing = false;
    }
    return bounds;
}

LRect<double> LSvgStructure::decoratedInternalBounds(LPainter *p, LSvgExtraStates &states) const
{
    LRect<double> bounds;
    if (!m_recursing) {
        m_recursing = true;
        for (const std::shared_ptr<LSvgNode>& node : m_renderers){
            bounds |= node->decoratedBounds(p, states);
        }
        m_recursing = false;
    }
    return bounds;
}

std::shared_ptr<LSvgNode> LSvgStructure::previousSiblingNode(std::shared_ptr<LSvgNode> n) const
{
    std::shared_ptr<LSvgNode> prev = nullptr;
    std::list<std::shared_ptr<LSvgNode>>::const_iterator itr = m_renderers.cbegin();
    for (; itr != m_renderers.cend(); ++itr) {
        std::shared_ptr<LSvgNode> node = *itr;
        if (node == n){
            return prev;
        }
        prev = node;
    }
    return prev;
}

LSvgMask::LSvgMask(std::shared_ptr<LSvgNode> parent, const LSvgRectInfo& bounds, LSvgUnitTypes contentUnits)
    : LSvgStructure(parent)
    , m_rectInfo(bounds)
    , m_contentUnits(contentUnits)
{
}

bool LSvgMask::shouldDrawNode(LPainter *, LSvgExtraStates &) const
{
    return false;
}

std::shared_ptr<LImage>  LSvgMask::createMask(LPainter *p, LSvgExtraStates &states, std::shared_ptr<LSvgNode> targetNode, LRect<double> *globalRect) const
{
    LTransform2D t = p->transform();
    p->resetTransform();
    LRect<double> basicRect = targetNode->internalBounds(p, states);
    *globalRect = t.mapRect(basicRect);
    p->setTransform(t);
    return createMask(p, states, basicRect, globalRect);
}

std::shared_ptr<LImage>  LSvgMask::createMask(LPainter *p, LSvgExtraStates &states, const LRect<double> &localRect, LRect<double> *globalRect) const
{
    LRect<int> imageBound = globalRect->toAlignedRect();
    *globalRect = imageBound.toRectF();


    std::shared_ptr<LImage>  mask = nullptr;
    if (m_recursing){
        mask = std::make_shared<LImage>(imageBound.size(), LImage::Format::ARGB32);
        return mask;
    }
    m_recursing = true;

    // Chrome seems to return the mask of the mask if a mask is set on the mask
    if (hasMask()) {
        std::shared_ptr<LSvgMask> maskNode = std::dynamic_pointer_cast<LSvgMask>(document()->namedNode(this->maskId()));
        if (maskNode != nullptr) {
            LRect<double> boundsRect;
            mask = maskNode->createMask(p, states, localRect, &boundsRect);
            m_recursing = false;
            return mask;
        }
    }

     mask = std::make_shared<LImage>(imageBound.size(), LImage::Format::ABGR32);

    // The mask is created with other elements during rendering.
    // Black pixels are masked out, white pixels are not masked.
    // The strategy is to draw the elements in a buffer (std::shared_ptr<LImage> ) and to map
    // the white-black image into a transparent-white image that can be used
    // with LPainters composition mode to set the mask.

    mask->fill(LColorID::Transparent);
    LPainter painter(mask);
    initPainter(&painter);

    LSvgExtraStates maskNodeStates;
    applyStyleRecursive(&painter, maskNodeStates);

    // The transformation of the mask node is not relevant. What matters are the contentUnits
    // and the position/scale of the node that the mask is applied to.
    painter.resetTransform();
    painter.translate(-imageBound.topLeft().toPointF());
    painter.setTransform(p->transform(), true);

    LTransform2D oldT = painter.transform();
    if (m_contentUnits == LSvgUnitTypes::objectBoundingBox){
        painter.translate(localRect.topLeft());
        painter.scale(localRect.width(), localRect.height());
    }

    // Draw all content items of the mask to generate the mask
    std::list<std::shared_ptr<LSvgNode>>::const_iterator itr = m_renderers.begin();
    while (itr != m_renderers.end()) {
        std::shared_ptr<LSvgNode> node = *itr;
        if ((node->isVisible()) && (node->displayMode() != LSvgNode::NoneMode)){
            node->draw(&painter, maskNodeStates);
        }
        ++itr;
    }

    uint32_t lineLen = 4*mask->width();
    for (int i=0; i < mask->height(); i++) {
        uint8_t *line = mask->scanLine(i);
        for (int j=0; j < lineLen; j+=4) {
            //0xRR,0xGG,0xBB,0xAA
            const double rC = 0.2125, gC = 0.7154, bC = 0.0721; //luminanceToAlpha times alpha following SVG 1.1
            int alpha = 255 - (line[j] * rC + line[j+1] * gC + line[j+2] * bC) * line[j+3]/255.;
            line[j] = 0;
            line[j+1] = 0;
            line[j+2] = 0;
            line[j+3] = alpha;
        }
    }

    // Make a path out of the clipRectangle and draw it inverted - black over all content items.
    // This is required to apply a clip rectangle with transformations.
    // painter.setClipRect(clipRect) sounds like the obvious thing to do but
    // created artifacts due to antialiasing.
    LRect<double> clipRect = m_rectInfo.resolveRelativeLengths(localRect);
    std::shared_ptr<LPainterPath> clipPath = std::make_shared<LPainterPath>();
    clipPath->setFillRule(LFillRule::OddEven);
    clipPath->addRect(mask->rect().adjusted(-10, -10, 20, 20).toRectF());
    clipPath->addPolygon(oldT.map(LPolygon<double>(clipRect)));
    painter.resetTransform();
    painter.fillPath(clipPath, LColorID::Black);
    revertStyleRecursive(&painter, maskNodeStates);

    m_recursing = false;
    return mask;
}

LSvgNode::Type LSvgMask::type() const
{
    return Mask;
}

LSvgPattern::LSvgPattern(std::shared_ptr<LSvgNode> parent, const LSvgRectInfo& bounds, const LRect<double>& viewBox,
                         LSvgUnitTypes contentUnits, const LTransform2D& transform)
    : LSvgStructure(parent),
    m_rectInfo(bounds),
    m_viewBox(viewBox),
    m_contentUnits(contentUnits),
    m_transform(transform)

{

}

bool LSvgPattern::shouldDrawNode(LPainter *, LSvgExtraStates &) const
{
    return false;
}

static std::shared_ptr<LImage> defaultPattern()
{
    static std::shared_ptr<LImage>  checkerPattern;
    if (checkerPattern == nullptr) {
        checkerPattern = std::make_shared<LImage> (LSize<int>(8, 8), LImage::Format::ARGB32);
        LPainter p(checkerPattern);
        p.fillRect(LRect<double>(0, 0, 4, 4, false), LColorID::White);
        p.fillRect(LRect<double>(4, 0, 4, 4, false), LColorID::Black);
        p.fillRect(LRect<double>(0, 4, 4, 4, false), LColorID::Black);
        p.fillRect(LRect<double>(4, 4, 4, 4, false), LColorID::White);
    }
    return checkerPattern;
}

std::shared_ptr<LImage>  LSvgPattern::patternImage(LPainter *p, LSvgExtraStates &states, const std::shared_ptr<LSvgNode> patternElement)
{
    // pe stands for Pattern Element
    LRect<double> peBoundingBox;
    LRect<double> peWorldBoundingBox;

    LTransform2D t = p->transform();
    p->resetTransform();
    peBoundingBox = patternElement->internalBounds(p, states);
    peWorldBoundingBox = t.mapRect(peBoundingBox);
    p->setTransform(t);

    // This function renders the pattern into an Image, so we need to apply the correct
    // scaling values when we draw the pattern. The scaling is affected by two factors :
    //      - The "patternTransform" attribute which itself might contain a scaling
    //      - The scaling applied globally.
    // The first is obtained from m11 and m22 matrix elements,
    // while the second is calculated by dividing the patternElement global size
    // by its local size.
    double contentScaleFactorX = m_transform.m11();
    double contentScaleFactorY = m_transform.m22();
    if (m_contentUnits == LSvgUnitTypes::userSpaceOnUse) {
        contentScaleFactorX *= t.m11();
        contentScaleFactorY *= t.m22();
    } else {
        contentScaleFactorX *= peWorldBoundingBox.width();
        contentScaleFactorY *= peWorldBoundingBox.height();
    }

    // Calculate the pattern bounding box depending on the used UnitTypes
    LRect<double> patternBoundingBox = m_rectInfo.resolveRelativeLengths(peBoundingBox);

    LSize<int> imageSize;
    imageSize.setWidth(std::ceil(patternBoundingBox.width() * t.m11() * m_transform.m11()));
    imageSize.setHeight(std::ceil(patternBoundingBox.height() * t.m22() * m_transform.m22()));

    calculateAppliedTransform(t, peBoundingBox, imageSize);
    return renderPattern(imageSize, contentScaleFactorX, contentScaleFactorY);
}

LSvgNode::Type LSvgPattern::type() const
{
    return Pattern;
}

std::shared_ptr<LImage>  LSvgPattern::renderPattern(LSize<int> size, double contentScaleX, double contentScaleY)
{
    if (size.isEmpty() || !std::isfinite(contentScaleX) || !std::isfinite(contentScaleY))
        return defaultPattern();

    // Allocate a std::shared_ptr<LImage>  to draw the pattern in with the calculated size.
    std::shared_ptr<LImage>  pattern = std::make_shared<LImage>(size, LImage::Format::ARGB32);
    pattern->fill(LColorID::Transparent);

    // Draw the pattern using our LPainter.
    LPainter patternPainter(pattern);
    LSvgExtraStates patternStates;
    initPainter(&patternPainter);
    applyStyleRecursive(&patternPainter, patternStates);
    patternPainter.resetTransform();

    // According to the <pattern> definition, if viewBox exists then patternContentUnits
    // is ignored
    if (m_viewBox.isNull()){
        patternPainter.scale(contentScaleX, contentScaleY);
    }else{
        patternPainter.setWindow(m_viewBox.toRect());
    }
    // Draw all this Pattern children nodes with our LPainter,
    // no need to use any Extra States
    for (std::shared_ptr<LSvgNode> node : m_renderers){
        node->draw(&patternPainter, patternStates);
    }

    revertStyleRecursive(&patternPainter, patternStates);
    return pattern;
}

void LSvgPattern::calculateAppliedTransform(LTransform2D &worldTransform, const LRect<double>& peLocalBB, const LSize<int>& imageSize)
{
    // Calculate the required transform to be applied to the LBrush used for correct
    // pattern drawing with the object being rendered.
    // Scale : Apply inverse the scale used above because LBrush uses the transform used
    //         by the LPainter and this function has already rendered the LImage with the
    //         correct size. Moreover, take into account the difference between the required
    //         ideal image size in float and the LSize<int> given to image as an integer value.
    //
    // Translate : Apply translation depending on the calculated x and y values so that the
    //             drawn pattern can be shifted inside the object.
    // Pattern Transform : Apply the transform in the "patternTransform" attribute. This
    //                     transform contains everything except scaling, because it is
    //                     already applied above on the std::shared_ptr<LImage>  and the LPainter while
    //                     drawing the pattern tile.
    m_appliedTransform.reset();
    double imageDownScaleFactorX = 1 / worldTransform.m11();
    double imageDownScaleFactorY = 1 / worldTransform.m22();

    m_appliedTransform.scale(std::isfinite(imageDownScaleFactorX) ? imageDownScaleFactorX : 1.0,
                             std::isfinite(imageDownScaleFactorY) ? imageDownScaleFactorY : 1.0);

    LRect<double> p = m_rectInfo.resolveRelativeLengths(peLocalBB);
    m_appliedTransform.scale((p.width() * worldTransform.m11() * m_transform.m11()) / imageSize.width(),
                             (p.height() * worldTransform.m22() * m_transform.m22()) / imageSize.height());

    LPoint<double> translation = m_rectInfo.translationRelativeToBoundingBox(peLocalBB);
    m_appliedTransform.translate(translation.m_x * worldTransform.m11(), translation.m_y * worldTransform.m22());

    LTransform2D scalelessTransform = m_transform;
    scalelessTransform.scale(1 / m_transform.m11(), 1 / m_transform.m22());

    m_appliedTransform = m_appliedTransform * scalelessTransform;
}


LK_NAMESPACE_END
