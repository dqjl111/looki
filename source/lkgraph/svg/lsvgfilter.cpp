#include "lsvgfilter.h"

LK_NAMESPACE_BEGIN


LSvgFeFilterPrimitive::LSvgFeFilterPrimitive(std::shared_ptr<LSvgNode> parent, const std::string &input,
                                             const std::string &result, const LSvgRectInfo &rect)
    : LSvgStructure(parent)
    , m_input(input)
    , m_result(result)
    , m_rect(rect)
{

}

bool LSvgFeFilterPrimitive::shouldDrawNode(LPainter *, LSvgExtraStates &) const
{
    return false;
}

LRect<double> LSvgFeFilterPrimitive::localSubRegion(const LRect<double> &itemBounds, const LRect<double> &filterBounds,
                                             LSvgUnitTypes primitiveUnits, LSvgUnitTypes) const
{
    // 15.7.3 Filter primitive subregion
    // https://www.w3.org/TR/SVG11/filters.html#FilterPrimitiveSubRegion

    LRect<double> clipRect = m_rect.resolveRelativeLengths(itemBounds, primitiveUnits);

    // the default subregion is 0%,0%,100%,100%, where as a special-case the percentages are
    // relative to the dimensions of the filter region, thus making the the default filter primitive
    // subregion equal to the filter region.
    if (m_rect.m_unitX == LSvgUnitTypes::unknown){
        clipRect.m_x = filterBounds.m_x;
    }
    if (m_rect.m_unitY == LSvgUnitTypes::unknown){
        clipRect.m_y= filterBounds.m_y;
    }
    if (m_rect.m_unitW == LSvgUnitTypes::unknown){
        clipRect.m_width = filterBounds.m_width;
    }
    if (m_rect.m_unitH == LSvgUnitTypes::unknown){
        clipRect.m_height = filterBounds.m_height;
    }
    clipRect = clipRect.intersected(filterBounds);

    return clipRect;
}

LRect<double> LSvgFeFilterPrimitive::globalSubRegion(LPainter *p,
                                              const LRect<double> &itemBounds, const LRect<double> &filterBounds,
                                              LSvgUnitTypes primitiveUnits, LSvgUnitTypes filterUnits) const
{
    return p->transform().mapRect(localSubRegion(itemBounds, filterBounds, primitiveUnits, filterUnits));
}

void LSvgFeFilterPrimitive::clipToTransformedBounds(std::shared_ptr<LImage> buffer, LPainter *p, const LRect<double> &localRect) const
{
    LPainter painter(buffer);
    painter.setRenderHints(p->renderHints());
    painter.translate(-buffer->offset().toPointF());
    std::shared_ptr<LPainterPath> clipPath = std::make_shared<LPainterPath>();
    clipPath->setFillRule(LFillRule::OddEven);
    clipPath->addRect(LRect<int>(buffer->offset(), buffer->size()).adjusted(-10, -10, 20, 20).toRectF());
    clipPath->addPolygon(p->transform().map(LPolygon<double>(localRect)));
    painter.setCompositionMode(LCompositionMode::SourceIn);
    painter.fillPath(clipPath, LColorID::Transparent);
}

bool LSvgFeFilterPrimitive::requiresSourceAlpha() const
{
    return m_input == "SourceAlpha";
}

std::shared_ptr<LSvgFeFilterPrimitive> LSvgFeFilterPrimitive::castToFilterPrimitive(std::shared_ptr<LSvgNode> node)
{
    if (node->type() == LSvgNode::FeMerge ||
        node->type() == LSvgNode::FeColormatrix ||
        node->type() == LSvgNode::FeGaussianblur ||
        node->type() == LSvgNode::FeOffset ||
        node->type() == LSvgNode::FeComposite ||
        node->type() == LSvgNode::FeFlood ||
        node->type() == LSvgNode::FeBlend ) {
        return std::dynamic_pointer_cast<LSvgFeFilterPrimitive>(node);
    } else {
        return nullptr;
    }
}

LSvgFeColorMatrix::LSvgFeColorMatrix(std::shared_ptr<LSvgNode> parent, const std::string &input, const std::string &result,
                                     const LSvgRectInfo &rect, ColorShiftType type,
                                     const Matrix &matrix)
    : LSvgFeFilterPrimitive(parent, input, result, rect)
    , m_matrix(matrix)
{
    //Magic numbers see SVG 1.1(Second edition)
    if (type == ColorShiftType::Saturate) {
        double s = lBound(0., m_matrix[0][0], 1.);

        m_matrix[0][0] = 0.213 + 0.787 * s;
        m_matrix[0][1] = 0.715 - 0.717 * s;
        m_matrix[0][2] = 0.072 - 0.072 * s;

        m_matrix[1][0] = 0.213  - 0.213 * s;
        m_matrix[1][1] = 0.715  + 0.285 * s;
        m_matrix[1][2] = 0.072 - 0.072 * s;

        m_matrix[2][0] = 0.213 - 0.213 * s;
        m_matrix[2][1] = 0.715 - 0.715 * s;
        m_matrix[2][2] = 0.072 + 0.928 * s;

        m_matrix[3][3] = 1;
    } else if (type == ColorShiftType::HueRotate){
        double angle = m_matrix[0][0]/180.*std::numbers::pi;
        double s = sin(angle);
        double c = cos(angle);

        m_matrix[0][0] = 0.213 + 0.787 * c -0.213 * s;
        m_matrix[0][1] = 0.715*(1.-c-s);
        m_matrix[0][2] = 0.072 -0.072 * c +0.928 * s;

        m_matrix[1][0] = 0.213 -0.213 * c +0.143 * s;
        m_matrix[1][1] = 0.715 +0.285 * c +0.140 * s;
        m_matrix[1][2] = 0.072 -0.072 * c -0.283 * s;

        m_matrix[2][0] = 0.213 -0.213 * c -0.787 * s;
        m_matrix[2][1] = 0.715 -0.715 * c +0.715 * s;
        m_matrix[2][2] = 0.072 +0.928 * c +0.072 * s;

        m_matrix[3][3] = 1;
    } else if (type == ColorShiftType::LuminanceToAlpha){
        m_matrix[0][0] = 0;
        m_matrix[0][1] = 0;
        m_matrix[0][2] = 0;

        m_matrix[1][0] = 0;
        m_matrix[1][1] = 0;
        m_matrix[1][2] = 0;

        m_matrix[2][0] = 0;
        m_matrix[2][1] = 0;
        m_matrix[2][2] = 0;

        m_matrix[3][0] = 0.2125;
        m_matrix[3][1] = 0.7154;
        m_matrix[3][2] = 0.0721;
    }

    if(type != ColorShiftType::Matrix){
        m_matrix[0][3] = 0;
        m_matrix[0][4] = 0;

        m_matrix[1][3] = 0;
        m_matrix[1][4] = 0;

        m_matrix[2][3] = 0;
        m_matrix[2][4] = 0;

        if(type != ColorShiftType::LuminanceToAlpha){
            m_matrix[3][0] = 0;
            m_matrix[3][1] = 0;
            m_matrix[3][2] = 0;
        }else{
            m_matrix[3][3] = 0;
        }
        m_matrix[3][4] = 0;

        m_matrix[4][0] = 0;
        m_matrix[4][1] = 0;
        m_matrix[4][2] = 0;
        m_matrix[4][3] = 0;
        m_matrix[4][4] = 0;
    }
}

LSvgNode::Type LSvgFeColorMatrix::type() const
{
    return LSvgNode::FeColormatrix;
}

std::shared_ptr<LImage> LSvgFeColorMatrix::apply(const std::map<std::string, std::shared_ptr<LImage>> &sources, LPainter *p,
                                const LRect<double> &itemBounds, const LRect<double> &filterBounds,
                                LSvgUnitTypes primitiveUnits, LSvgUnitTypes filterUnits) const
{
    std::map<std::string, std::shared_ptr<LImage>>::const_iterator sourceIter = sources.find(m_input);
    if (sourceIter == sources.cend()){
        return nullptr;
    }

    LRect<int> clipRectGlob = globalSubRegion(p, itemBounds, filterBounds, primitiveUnits, filterUnits).toRect();
    if (clipRectGlob.isEmpty()){
        return nullptr;
    }

    std::shared_ptr<LImage> source = sourceIter->second;

    std::shared_ptr<LImage> result = std::make_shared<LImage>(clipRectGlob.size(), LImage::Format::ARGB32Premultiplied);

    result->setOffset(clipRectGlob.topLeft());
    result->fill(LColorID::Transparent);

    assert(source->depth() == 32);

    for (int i = 0; i < result->height(); i++) {
        int sourceI = i - source->offset().m_y + result->offset().m_y;

        if (sourceI < 0 || sourceI >= source->height()){
            continue;
        }

        uint32_t *sourceLine = reinterpret_cast<uint32_t *>(source->scanLine(sourceI));
        uint32_t *resultLine = reinterpret_cast<uint32_t *>(result->scanLine(i));

        for (int j = 0; j < result->width(); j++) {
            int sourceJ = j - source->offset().m_x + result->offset().m_x;

            if (sourceJ < 0 || sourceJ >= source->width())
                continue;

            uint32_t sourceColor = LColor::unpremultiply(sourceLine[sourceJ]);
            double a = LColor::uintToAlpha(sourceColor);
            double r = LColor::uintToRed(sourceColor);
            double g = LColor::uintToGreen(sourceColor);
            double b = LColor::uintToBlue(sourceColor);

            double r2 = m_matrix[0][0] * r +
                       m_matrix[0][1] * g +
                       m_matrix[0][2] * b +
                       m_matrix[0][3] * a +
                       m_matrix[0][4] * 255.;
            double g2 = m_matrix[1][0] * r +
                       m_matrix[1][1] * g +
                       m_matrix[1][2] * b +
                       m_matrix[1][3] * a +
                       m_matrix[1][4] * 255.;
            double b2 = m_matrix[2][0] * r +
                       m_matrix[2][1] * g +
                       m_matrix[2][2] * b +
                       m_matrix[2][3] * a +
                       m_matrix[2][4] * 255.;
            double a2 = m_matrix[3][0] * r +
                       m_matrix[3][1] * g +
                       m_matrix[3][2] * b +
                       m_matrix[3][3] * a +
                       m_matrix[3][4] * 255.;

            uint32_t rgba = LColor::argbToUInt(lBound(0, int(r2), 255),
                                              lBound(0, int(g2), 255),
                                              lBound(0, int(b2), 255),
                                              lBound(0, int(a2), 255));
            resultLine[j] = LColor::premultiply(rgba);
        }
    }

    clipToTransformedBounds(result, p, localSubRegion(itemBounds, filterBounds, primitiveUnits, filterUnits));
    return result;
}

void LSvgFeColorMatrix::fill(Matrix &m, double v)
{
    for(int i=0; i<5; ++i){
        for(int j=0; j<5; ++j){
            m[i][j] = v;
        }
    }
}

void LSvgFeColorMatrix::setIdentity(Matrix &m)
{
    for(int i=0; i<5; ++i){
        for(int j=0; j<5; ++j){
            if(i == j){
                m[i][j] = 1;
            }else{
                m[i][j] = 0;
            }
        }
    }
}

LSvgFeGaussianBlur::LSvgFeGaussianBlur(std::shared_ptr<LSvgNode> parent, const std::string &input,
                                       const std::string &result, const LSvgRectInfo &rect,
                                       double stdDeviationX, double stdDeviationY, EdgeMode edgemode)
    : LSvgFeFilterPrimitive(parent, input, result, rect)
    , m_stdDeviationX(stdDeviationX)
    , m_stdDeviationY(stdDeviationY)
    , m_edgemode(edgemode)
{
}

LSvgNode::Type LSvgFeGaussianBlur::type() const
{
    return LSvgNode::FeGaussianblur;
}

std::shared_ptr<LImage> LSvgFeGaussianBlur::apply(const std::map<std::string, std::shared_ptr<LImage>> &sources, LPainter *p,
                                 const LRect<double> &itemBounds, const LRect<double> &filterBounds,
                                 LSvgUnitTypes primitiveUnits, LSvgUnitTypes filterUnits) const
{
    std::map<std::string, std::shared_ptr<LImage>>::const_iterator sourceIter = sources.find(m_input);
    if (sourceIter == sources.cend()){
        return nullptr;
    }
    std::shared_ptr<LImage> source = sourceIter->second;
    assert(source->depth() == 32);

    if (m_stdDeviationX == 0 && m_stdDeviationY == 0){
        return source;
    }

    const double scaleX = hypot(p->transform().m11(), p->transform().m21());
    const double scaleY = hypot(p->transform().m12(), p->transform().m22());

    double sigma_x = scaleX * m_stdDeviationX;
    double sigma_y = scaleY * m_stdDeviationY;
    if (primitiveUnits == LSvgUnitTypes::objectBoundingBox) {
        sigma_x *= itemBounds.width();
        sigma_y *= itemBounds.height();
    }

    constexpr double sd = 0.75 * std::numbers::sqrt2 /std::numbers::inv_sqrtpi; // 3 * sqrt(2 * pi) / 4
    const int dx = floor(sigma_x * sd + 0.5);
    const int dy = floor(sigma_y * sd + 0.5);

    const LTransform2D scaleXr = LTransform2D::fromScale(scaleX, scaleY);
    const LTransform2D restXr = scaleXr.inverted() * p->transform();

    LRect<int> clipRectGlob = scaleXr.mapRect(localSubRegion(itemBounds, filterBounds, primitiveUnits, filterUnits)).toRect();
    if (clipRectGlob.isEmpty())
        return std::shared_ptr<LImage>();

    std::shared_ptr<LImage> tempSource = std::make_shared<LImage>(clipRectGlob.size(), LImage::Format::ARGB32Premultiplied);

    tempSource->setOffset(clipRectGlob.topLeft());
    tempSource->fill(LColorID::Transparent);
    LPainter copyPainter(tempSource);
    copyPainter.translate(-tempSource->offset().toPointF());
    copyPainter.setTransform(restXr.inverted(), true);
    copyPainter.drawImage(source->offset().toPointF(), source);
    copyPainter.end();

    std::vector<uint64_t> buffer(tempSource->width() * tempSource->height());

    const int sourceHeight = tempSource->height();
    const int sourceWidth = tempSource->width();
    uint32_t *rawImage = reinterpret_cast<uint32_t *>(tempSource->bits());

    // https://www.w3.org/TR/SVG11/filters.html#feGaussianBlurElement:
    // if d is odd, use three box-blurs of size 'd', centered on the output pixel.
    // if d is even, two box-blurs of size 'd' (the first one centered on the pixel boundary
    // between the output pixel and the one to the left, the second one centered on the pixel
    // boundary between the output pixel and the one to the right) and one box blur of size
    // 'd+1' centered on the output pixel.
    auto adjustD = [](int d, int iteration) {
        d = std::max<int>(1, d);     // Treat d == 0 just like d == 1
        std::pair<int, int> result;
        if (d % 2 == 1){
            result = {d / 2 + 1, d / 2};
        }else if (iteration == 0){
            result = {d / 2 + 1, d / 2 - 1};
        }else if (iteration == 1){
            result = {d / 2, d / 2};
        }else{
            result = {d / 2 + 1, d / 2};
        }
        assert(result.first + result.second > 0);
        return result;
    };
    // https://www.w3.org/TR/SVG11/filters.html#feGaussianBlurElement:
    // Three successive box-blurs build a piece-wise quadratic convolution kernel,
    // which approximates the Gaussian kernel
    for (int m = 0; m < 3; m++) {
        for (int col = 0; col < 4 * 8; col += 8 ){
            // Generating the partial sum of color values from the top left corner
            // These sums can be combined to yield the partial sum of any rectangular subregion
            for (int i = 0; i < sourceWidth; i++) {
                for (int j = 0; j < sourceHeight; j++) {
                    buffer[i + j * sourceWidth] = (rawImage[i + j * sourceWidth] >> col) & 0xff;
                    if (i > 0)
                        buffer[i + j * sourceWidth] += buffer[(i - 1) + j * sourceWidth];
                    if (j > 0)
                        buffer[i + j * sourceWidth] += buffer[i + (j - 1) * sourceWidth];
                    if (i > 0 && j > 0)
                        buffer[i + j * sourceWidth] -= buffer[(i - 1) + (j - 1) * sourceWidth];
                }
            }


            const auto [dxleft, dxright] = adjustD(dx, m);
            const auto [dytop, dybottom] = adjustD(dy, m);
            for (int i = 0; i < sourceWidth; i++) {
                for (int j = 0; j < sourceHeight; j++) {
                    const int i1 = std::max<int>(0, i - dxleft);
                    const int i2 = std::min<int>(sourceWidth - 1, i + dxright);
                    const int j1 = std::max<int>(0, j - dytop);
                    const int j2 = std::min<int>(sourceHeight - 1, j + dybottom);

                    uint64_t colorValue64 = buffer[i2 + j2 * sourceWidth];
                    colorValue64 -= buffer[i1 + j2 * sourceWidth];
                    colorValue64 -= buffer[i2 + j1 * sourceWidth];
                    colorValue64 += buffer[i1 + j1 * sourceWidth];
                    colorValue64 /= uint64_t(dxleft + dxright) * uint64_t(dytop + dybottom);

                    const unsigned int colorValue = colorValue64;
                    rawImage[i + j * sourceWidth] &= ~(0xff << col);
                    rawImage[i + j * sourceWidth] |= colorValue << col;

                }
            }
        }
    }

    LRect<double> trueClipRectGlob = globalSubRegion(p, itemBounds, filterBounds, primitiveUnits, filterUnits);

    std::shared_ptr<LImage> result= std::make_shared<LImage>(trueClipRectGlob.toRect().size(), LImage::Format::ARGB32Premultiplied);

    result->setOffset(trueClipRectGlob.toRect().topLeft());
    result->fill(LColorID::Transparent);
    LPainter transformPainter(result);
    transformPainter.setRenderHints(LRenderHint::Antialiasing, true);

    transformPainter.translate(-result->offset().toPointF());
    transformPainter.setTransform(restXr, true);
    transformPainter.drawImage(clipRectGlob.topLeft().toPointF(), tempSource);
    transformPainter.end();

    clipToTransformedBounds(result, p, localSubRegion(itemBounds, filterBounds, primitiveUnits, filterUnits));
    return result;
}

LSvgFeOffset::LSvgFeOffset(std::shared_ptr<LSvgNode> parent, const std::string &input, const std::string &result,
                           const LSvgRectInfo &rect, double dx, double dy)
    : LSvgFeFilterPrimitive(parent, input, result, rect)
    , m_dx(dx)
    , m_dy(dy)
{

}

LSvgNode::Type LSvgFeOffset::type() const
{
    return LSvgNode::FeOffset;
}

std::shared_ptr<LImage> LSvgFeOffset::apply(const std::map<std::string, std::shared_ptr<LImage>> &sources, LPainter *p,
                           const LRect<double> &itemBounds, const LRect<double> &filterBounds,
                           LSvgUnitTypes primitiveUnits, LSvgUnitTypes filterUnits) const
{
    std::map<std::string, std::shared_ptr<LImage>>::const_iterator sourceIter = sources.find(m_input);
    if (sourceIter == sources.cend()){
        return nullptr;
    }
    LRect<double> clipRect = localSubRegion(itemBounds, filterBounds, primitiveUnits, filterUnits);
    LRect<int> clipRectGlob = p->transform().mapRect(clipRect).toRect();

    if (clipRectGlob.isEmpty()){
        return nullptr;
    }

    std::shared_ptr<LImage> source = sourceIter->second;

    LPoint<double> offset(m_dx, m_dy);
    if (primitiveUnits == LSvgUnitTypes::objectBoundingBox) {
        offset = LPoint<double>(m_dx * itemBounds.width(), m_dy * itemBounds.height());
    }
    offset = p->transform().map(offset) - p->transform().map(LPoint<double>(0, 0));

    std::shared_ptr<LImage> result = std::make_shared<LImage>(clipRectGlob.size(), LImage::Format::ARGB32Premultiplied);
    result->setOffset(clipRectGlob.topLeft());
    result->fill(LColorID::Transparent);

    LPainter copyPainter(result);
    copyPainter.drawImage(source->offset().toPointF() - result->offset().toPointF() + offset, source);
    copyPainter.end();

    clipToTransformedBounds(result, p, clipRect);
    return result;
}


LSvgFeMerge::LSvgFeMerge(std::shared_ptr<LSvgNode> parent, const std::string &input,
                         const std::string &result, const LSvgRectInfo &rect)
    : LSvgFeFilterPrimitive(parent, input, result, rect)
{

}

LSvgNode::Type LSvgFeMerge::type() const
{
    return LSvgNode::FeMerge;
}

std::shared_ptr<LImage> LSvgFeMerge::apply(const std::map<std::string, std::shared_ptr<LImage>> &sources, LPainter *p,
                          const LRect<double> &itemBounds, const LRect<double> &filterBounds,
                          LSvgUnitTypes primitiveUnits, LSvgUnitTypes filterUnits) const
{
    LRect<double> clipRect = localSubRegion(itemBounds, filterBounds, primitiveUnits, filterUnits);
    LRect<int> clipRectGlob = p->transform().mapRect(clipRect).toRect();
    if (clipRectGlob.isEmpty()){
        return nullptr;
    }

    std::list<std::shared_ptr<LImage>> mergeNodeResults;
    for (const std::shared_ptr<LSvgNode>& child : m_renderers) {
        if (child->type() == LSvgNode::FeMergenode) {
            std::shared_ptr<LSvgFeMergeNode> filter = std::dynamic_pointer_cast<LSvgFeMergeNode>(child);
            std::shared_ptr<LImage> image = filter->apply(sources, p, itemBounds, filterBounds, primitiveUnits, filterUnits);
            if(image != nullptr){
                mergeNodeResults.push_back(image);
            }
        }
    }

    std::shared_ptr<LImage> result = std::make_shared<LImage>(clipRectGlob.size(), LImage::Format::ARGB32Premultiplied);
    result->setOffset(clipRectGlob.topLeft());
    result->fill(LColorID::Transparent);

    LPainter proxyPainter(result);
    for (const std::shared_ptr<LImage> & i : mergeNodeResults) {
        proxyPainter.drawImage(LRect<int>(i->offset() - result->offset(), i->size()).toRectF(),
                               i,
                               LRect<double>(0, 0, i->width(), i->height(), false));
    }
    proxyPainter.end();

    clipToTransformedBounds(result, p, clipRect);
    return result;
}

bool LSvgFeMerge::requiresSourceAlpha() const
{
    for (const std::shared_ptr<LSvgNode>& child: m_renderers) {
        if (child->type() == LSvgNode::FeMergenode) {
            std::shared_ptr<LSvgFeMergeNode> filter = std::dynamic_pointer_cast<LSvgFeMergeNode>(child);
            if(filter != nullptr){
                if (filter->requiresSourceAlpha()){
                    return true;
                }
            }
        }
    }
    return false;
}

LSvgFeMergeNode::LSvgFeMergeNode(std::shared_ptr<LSvgNode> parent, const std::string &input,
                                 const std::string &result, const LSvgRectInfo &rect)
    : LSvgFeFilterPrimitive(parent, input, result, rect)
{

}

LSvgNode::Type LSvgFeMergeNode::type() const
{
    return LSvgNode::FeMergenode;
}

std::shared_ptr<LImage> LSvgFeMergeNode::apply(const std::map<std::string, std::shared_ptr<LImage>> &sources, LPainter *,
                              const LRect<double> &, const LRect<double> &, LSvgUnitTypes, LSvgUnitTypes) const
{
    const std::map<std::string, std::shared_ptr<LImage>>::const_iterator iter = sources.find(m_input);
    if(iter != sources.end()){
        return iter->second;
    }
    return nullptr;
}

LSvgFeComposite::LSvgFeComposite(std::shared_ptr<LSvgNode> parent, const std::string &input, const std::string &result,
                                 const LSvgRectInfo &rect, const std::string &input2, Operator op,
                                 const std::array<double, 4> &k)
    : LSvgFeFilterPrimitive(parent, input, result, rect)
    , m_input2(input2)
    , m_operator(op)
    , m_k(k)
{

}

LSvgNode::Type LSvgFeComposite::type() const
{
    return LSvgNode::FeComposite;
}

std::shared_ptr<LImage> LSvgFeComposite::apply(const std::map<std::string, std::shared_ptr<LImage>> &sources, LPainter *p,
                              const LRect<double> &itemBounds, const LRect<double> &filterBounds,
                              LSvgUnitTypes primitiveUnits, LSvgUnitTypes filterUnits) const
{
    std::map<std::string, std::shared_ptr<LImage>>::const_iterator sourceIter = sources.find(m_input);
    if (sourceIter == sources.cend()){
        return nullptr;
    }
    std::shared_ptr<LImage> source1 = sourceIter->second;
    sourceIter = sources.find(m_input2);
    if (sourceIter == sources.cend()){
        return nullptr;
    }

    LRect<double> clipRect = localSubRegion(itemBounds, filterBounds, primitiveUnits, filterUnits);
    LRect<int> clipRectGlob = p->transform().mapRect(clipRect).toRect();

    if (clipRectGlob.isEmpty()){
        return nullptr;
    }

    std::shared_ptr<LImage> source2 = sourceIter->second;

    assert(source1->depth() == 32);
    assert(source2->depth() == 32);

    std::shared_ptr<LImage> result = std::make_shared<LImage>(clipRectGlob.size(), LImage::Format::ARGB32Premultiplied);

    result->setOffset(clipRectGlob.topLeft());
    result->fill(LColorID::Transparent);

    if (m_operator == Operator::Arithmetic) {
        const double k1 = m_k[0];
        const double k2 = m_k[1];
        const double k3 = m_k[2];
        const double k4 = m_k[3];

        for (int j = 0; j < result->height(); j++) {
            int jj1 = j - source1->offset().m_y + result->offset().m_y;
            int jj2 = j - source2->offset().m_y + result->offset().m_y;

            uint32_t *resultLine = reinterpret_cast<uint32_t *>(result->scanLine(j));
            uint32_t *source1Line = nullptr;
            uint32_t *source2Line = nullptr;

            if (jj1 >= 0 && jj1 < source1->height()){
                source1Line = reinterpret_cast<uint32_t *>(source1->scanLine(jj1));
            }
            if (jj2 >= 0 && jj2 < source2->height()){
                source2Line = reinterpret_cast<uint32_t *>(source2->scanLine(jj2));
            }

            for (int i = 0; i < result->width(); i++) {
                int ii1 = i - source1->offset().m_x + result->offset().m_x;
                int ii2 = i - source2->offset().m_x + result->offset().m_x;

                LColor s1 = LColor(0, 0, 0, 0);
                LColor s2 = LColor(0, 0, 0, 0);

                if (ii1 >= 0 && ii1 < source1->width() && source1Line) {
                    s1 = LColor::uintToColor(source1Line[ii1]);
                }

                if (ii2 >= 0 && ii2 < source2->width() && source2Line) {
                    s1 = LColor::uintToColor(source2Line[ii2]);
                }

                int r = k1 * s1.m_red * s2.m_red / 255. + k2 * s1.m_red + k3 * s2.m_red + k4 * 255.;
                int g = k1 * s1.m_green * s2.m_green / 255. + k2 * s1.m_green + k3 * s2.m_green + k4 * 255.;
                int b = k1 * s1.m_blue * s2.m_blue / 255. + k2 * s1.m_blue + k3 * s2.m_blue + k4 * 255.;
                int a = k1 * s1.m_alpha * s2.m_alpha / 255. + k2 * s1.m_alpha + k3 * s2.m_alpha + k4 * 255.;

                a = lBound(0, a, 255);
                resultLine[i] =  LColor::argbToUInt(lBound(0, r, a),
                                                    lBound(0, g, a),
                                                    lBound(0, b, a),
                                                    a);
            }
        }
    } else {
        LPainter proxyPainter(result);
        proxyPainter.drawImage(LRect<int>(source1->offset() - result->offset(), source1->size()).toRectF(),
                               source1,
                               LRect<double>(0,0,source1->width(),source1->height(),false));

        switch (m_operator) {
        case Operator::In:{
            proxyPainter.setCompositionMode(LCompositionMode::DestinationIn);
            break;
        }
        case Operator::Out:{
            proxyPainter.setCompositionMode(LCompositionMode::DestinationOut);
            break;
        }
        case Operator::Xor:{
            proxyPainter.setCompositionMode(LCompositionMode::Xor);
            break;
        }
        case Operator::Lighter:{
            proxyPainter.setCompositionMode(LCompositionMode::SvgLighten);
            break;
        }
        case Operator::Atop:{
            proxyPainter.setCompositionMode(LCompositionMode::DestinationAtop);
            break;
        }
        case Operator::Over:{
            proxyPainter.setCompositionMode(LCompositionMode::DestinationOver);
            break;
        }
        default:{
            break;
        }
        }
        proxyPainter.drawImage(LRect<int>(source2->offset()-result->offset(), source2->size()).toRectF(),
                               source2,
                               LRect<double>(0,0,source2->width(),source2->height(),false));
        proxyPainter.end();
    }

    clipToTransformedBounds(result, p, clipRect);
    return result;
}

bool LSvgFeComposite::requiresSourceAlpha() const
{
    if (LSvgFeFilterPrimitive::requiresSourceAlpha()){
        return true;
    }
    return m_input2 == "SourceAlpha";
}


LSvgFeFlood::LSvgFeFlood(std::shared_ptr<LSvgNode> parent, const std::string &input, const std::string &result,
                         const LSvgRectInfo &rect, const LColor &color)
    : LSvgFeFilterPrimitive(parent, input, result, rect)
    , m_color(color)
{

}

LSvgNode::Type LSvgFeFlood::type() const
{
    return LSvgNode::FeFlood;
}

std::shared_ptr<LImage> LSvgFeFlood::apply(const std::map<std::string, std::shared_ptr<LImage>> &,
                          LPainter *p, const LRect<double> &itemBounds, const LRect<double> &filterBounds,
                          LSvgUnitTypes primitiveUnits, LSvgUnitTypes filterUnits) const
{

    LRect<double> clipRect = localSubRegion(itemBounds, filterBounds, primitiveUnits, filterUnits);
    LRect<int> clipRectGlob = p->transform().mapRect(clipRect).toRect();

    std::shared_ptr<LImage> result = std::make_shared<LImage>(clipRectGlob.size(), LImage::Format::ARGB32Premultiplied);
    result->setOffset(clipRectGlob.topLeft());
    result->fill(m_color);
    clipToTransformedBounds(result, p, clipRect);
    return result;
}

LSvgFeBlend::LSvgFeBlend(std::shared_ptr<LSvgNode> parent, const std::string &input, const std::string &result,
                         const LSvgRectInfo &rect, const std::string &input2, Mode mode)
    : LSvgFeFilterPrimitive(parent, input, result, rect)
    , m_input2(input2)
    , m_mode(mode)
{

}

LSvgNode::Type LSvgFeBlend::type() const
{
    return LSvgNode::FeBlend;
}

std::shared_ptr<LImage> LSvgFeBlend::apply(const std::map<std::string, std::shared_ptr<LImage>> &sources, LPainter *p,
                          const LRect<double> &itemBounds, const LRect<double> &filterBounds,
                          LSvgUnitTypes primitiveUnits, LSvgUnitTypes filterUnits) const
{
    std::map<std::string, std::shared_ptr<LImage>>::const_iterator sourceIter = sources.find(m_input);
    if (sourceIter == sources.cend()){
        return nullptr;
    }
    std::shared_ptr<LImage> source1 = sourceIter->second;
    sourceIter = sources.find(m_input2);
    if (sourceIter == sources.cend()){
        return nullptr;
    }

    LRect<double> clipRect = localSubRegion(itemBounds, filterBounds, primitiveUnits, filterUnits);
    LRect<int> clipRectGlob = p->transform().mapRect(clipRect).toRect();

    if (clipRectGlob.isEmpty()){
        return nullptr;
    }

    std::shared_ptr<LImage> source2 = sourceIter->second;
    std::shared_ptr<LImage> result = std::make_shared<LImage>(clipRectGlob.size(), LImage::Format::ARGB32Premultiplied);

    result->setOffset(clipRectGlob.topLeft());
    result->fill(LColorID::Transparent);

    for (int j = 0; j < result->height(); j++) {
        int jj1 = j - source1->offset().m_y + result->offset().m_y;
        int jj2 = j - source2->offset().m_y + result->offset().m_y;

        uint32_t *resultLine = reinterpret_cast<uint32_t *>(result->scanLine(j));
        uint32_t *source1Line = nullptr;
        uint32_t *source2Line = nullptr;

        if (jj1 >= 0 && jj1 < source1->height()){
            source1Line = reinterpret_cast<uint32_t *>(source1->scanLine(jj1));
        }
        if (jj2 >= 0 && jj2 < source2->height()){
            source2Line = reinterpret_cast<uint32_t *>(source2->scanLine(jj2));
        }

        for (int i = 0; i < result->width(); i++) {
            int ii1 = i - source1->offset().m_x + result->offset().m_x;
            int ii2 = i - source2->offset().m_x + result->offset().m_x;

            uint32_t pixel1 = (ii1 >= 0 && ii1 < source1->width() && source1Line) ?
                              source1Line[ii1] : 0;
            uint32_t pixel2 = (ii2 >= 0 && ii2 < source2->width() && source2Line) ?
                              source2Line[ii2] : 0;

            double r = 0, g = 0, b = 0;
            double red1 = LColor::uintToRed(pixel1);
            double red2 = LColor::uintToRed(pixel2);
            double green1 = LColor::uintToGreen(pixel1);
            double green2 = LColor::uintToGreen(pixel2);
            double blue1 = LColor::uintToBlue(pixel1);
            double blue2 = LColor::uintToBlue(pixel2);
            double alpha1 = LColor::uintToAlpha(pixel1) / 255.;
            double alpha2 = LColor::uintToAlpha(pixel2) / 255.;
            double a = 255 - (1 - alpha1) * (1 - alpha2) * 255.;

            switch (m_mode) {
            case Mode::Normal:{
                r = (1 - alpha1) * red2 + red1;
                g = (1 - alpha1) * green2 + green1;
                b = (1 - alpha1) * blue2 + blue1;
                break;
            }
            case Mode::Multiply:{
                r = (1 - alpha1) * red2 + (1 - alpha2) * red1 + red1 * red2;
                g = (1 - alpha1) * green2 + (1 - alpha2) * green1 + green1 * green2;
                b = (1 - alpha1) * blue2 + (1 - alpha2) * blue1 + blue1 * blue2;
                break;
            }
            case Mode::Screen:{
                r = red2 + red1 - red1 * red2;
                g = green2 + green1 - green1 * green2;
                b = blue2 + blue1 - blue1 * blue2;
                break;
            }
            case Mode::Darken:{
                r = std::min<double>((1 - alpha1) * red2 + red1, (1 - alpha2) * red1 + red2);
                g = std::min<double>((1 - alpha1) * green2 + green1, (1 - alpha2) * green1 + green2);
                b = std::min<double>((1 - alpha1) * blue2 + blue1, (1 - alpha2) * blue1 + blue2);
                break;
            }
            case Mode::Lighten:{
                r = std::max<double>((1 - alpha1) * red2 + red1, (1 - alpha2) * red1 + red2);
                g = std::max<double>((1 - alpha1) * green2 + green1, (1 - alpha2) * green1 + green2);
                b = std::max<double>((1 - alpha1) * blue2 + blue1, (1 - alpha2) * blue1 + blue2);
                break;
            }
            default:{
                break;
            }
            }
            a = lBound(0., a, 255.);
            resultLine[i] = LColor::argbToUInt(lBound(0., r, a),
                                              lBound(0., g, a),
                                              lBound(0., b, a),
                                              a);
        }
    }
    clipToTransformedBounds(result, p, clipRect);
    return result;
}

bool LSvgFeBlend::requiresSourceAlpha() const
{
    if (LSvgFeFilterPrimitive::requiresSourceAlpha()){
        return true;
    }
    return m_input2 == "SourceAlpha";
}

LSvgFeUnsupported::LSvgFeUnsupported(std::shared_ptr<LSvgNode> parent, const std::string &input, const std::string &result,
                                     const LSvgRectInfo &rect)
    : LSvgFeFilterPrimitive(parent, input, result, rect)
{
}

LSvgNode::Type LSvgFeUnsupported::type() const
{
    return LSvgNode::FeUnsupported;
}

std::shared_ptr<LImage> LSvgFeUnsupported::apply(const std::map<std::string, std::shared_ptr<LImage>> &,
                                LPainter *, const LRect<double> &, const LRect<double> &,
                                LSvgUnitTypes, LSvgUnitTypes) const
{
    lWarn() << "Unsupported filter primitive should not be applied.";
    return nullptr;
}


LK_NAMESPACE_END
