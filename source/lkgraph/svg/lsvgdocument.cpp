#include "lsvgdocument.h"
#include "lsvgfont.h"
#include "lsvgnode.h"
#include "lsvghandler.h"
#include <fstream>

LK_NAMESPACE_BEGIN


LSvgDocument::LSvgDocument(LFlags<LSvgOption> options, LSvgAnimatorType type)
    : LSvgStructure(nullptr)
    , m_widthPercent(false)
    , m_heightPercent(false)
    , m_animated(false)
    , m_fps(30)
    , m_options(options)
{
    bool animationEnabled = !m_options.testFlag(LSvgOption::DisableAnimations);
    switch (type) {
    case LSvgAnimatorType::Automatic:{
        if (animationEnabled){
            m_animator =  std::make_shared<LSvgAnimator>();
        }
        break;
    }
    case LSvgAnimatorType::Controlled:{
        if (animationEnabled){
            m_animator = std::make_shared<LSvgAnimationController>();
        }
    }
    default:{
        break;
    }
    }
}

LSvgDocument::~LSvgDocument()
{
}

std::shared_ptr<LSvgDocument>  LSvgDocument::load(const std::string &fileName, LFlags<LSvgOption> options,
                                         LSvgAnimatorType type)
{
    std::ifstream xmlFile(fileName);
    std::string   xmlContent{std::istreambuf_iterator<char>(xmlFile),std::istreambuf_iterator<char>()};
    return loadContent(xmlContent, options, type);
}

std::shared_ptr<LSvgDocument> LSvgDocument::load(const std::wstring &file, LFlags<LSvgOption> options, LSvgAnimatorType type)
{
    std::string fileUtf8 = LStringUtil::wideToUtf8(file);
    return load(fileUtf8, options, type);
}
std::shared_ptr<LSvgDocument> LSvgDocument::loadContent(const std::string &contents, LFlags<LSvgOption> options,
                                         LSvgAnimatorType type)
{
    if (contents.empty()){
        return nullptr;        
    }

    LSvgHandler handler(contents, options, type);
    std::shared_ptr<LSvgDocument> doc = nullptr;
    if (handler.ok()) {
        doc = handler.document();
        doc->m_animator->setAnimationDuration(handler.animationDuration());
    }
    return doc;
}


void LSvgDocument::draw(LPainter *p, const LRect<double> &bounds)
{
    if (displayMode() == LSvgNode::NoneMode){
        return;
    }

    p->save();
    //sets default style on the painter
    //### not the most optimal way
    mapSourceToTarget(p, bounds);
    initPainter(p);
    std::list<std::shared_ptr<LSvgNode>>::iterator itr = m_renderers.begin();
    applyStyle(p, m_states);
    while (itr != m_renderers.end()) {
        std::shared_ptr<LSvgNode> node = *itr;
        if ((node->isVisible()) && (node->displayMode() != LSvgNode::NoneMode)){
            node->draw(p, m_states);
        }
        ++itr;
    }
    revertStyle(p, m_states);
    p->restore();
}


void LSvgDocument::draw(LPainter *p, const std::string &id, const LRect<double> &bounds)
{
    std::shared_ptr<LSvgNode> node = scopeNode(id);

    if (!node) {
        lWarn() << "Couldn't find node " << id;
        return;
    }

    if (node->displayMode() == LSvgNode::NoneMode)
        return;

    p->save();

    const LRect<double> elementBounds = node->bounds();

    mapSourceToTarget(p, bounds, elementBounds);
    LTransform2D originalTransform = p->worldTransform();

    //XXX set default style on the painter
    std::shared_ptr<LPen> pen = std::make_shared<LPen>(nullptr, 1, LPenStyle::Solid, LPenCapStyle::Flat, LPenJoinStyle::SvgMiter);
    pen->m_miterLimit = 4;
    p->setPen(pen);
    p->setBrush(LColorID::Black);
    p->setRenderHints(LRenderHint::Antialiasing);
    p->setRenderHints(LRenderHint::SmoothPixmapTransform);

    std::vector<std::shared_ptr<LSvgNode>> parentApplyStack;
    std::shared_ptr<LSvgNode> parent = node->parent();
    while (parent) {
        parentApplyStack.push_back(parent);
        parent = parent->parent();
    }

    for (int i = (int)parentApplyStack.size() - 1; i >= 0; --i){
        parentApplyStack[i]->applyStyle(p, m_states);
    }

    // Reset the world transform so that our parents don't affect
    // the position
    LTransform2D currentTransform = p->worldTransform();
    p->setWorldTransform(originalTransform);

    node->draw(p, m_states);

    p->setWorldTransform(currentTransform);

    for (const std::shared_ptr<LSvgNode>& n : parentApplyStack){
        n->revertStyle(p, m_states);
    }
    p->restore();
}

LSvgNode::Type LSvgDocument::type() const
{
    return Doc;
}

void LSvgDocument::setWidth(int len, bool percent)
{
    m_size.setWidth(len);
    m_widthPercent = percent;
}

void LSvgDocument::setHeight(int len, bool percent)
{
    m_size.setHeight(len);
    m_heightPercent = percent;
}

void LSvgDocument::setPreserveAspectRatio(bool on)
{
    m_preserveAspectRatio = on;
}

void LSvgDocument::setViewBox(const LRect<double> &rect)
{
    m_viewBox = rect;
    m_implicitViewBox = rect.isNull();
}

LFlags<LSvgOption> LSvgDocument::options() const
{
    return m_options;
}

void LSvgDocument::addSvgFont(std::shared_ptr<LSvgFont> font)
{
    if(font != nullptr){
        m_fonts[font->familyName()] = font;
    }
}

std::shared_ptr<LSvgFont> LSvgDocument::svgFont(const std::string &family) const
{
    std::unordered_map<std::string, std::shared_ptr<LSvgFont> >::const_iterator iter = m_fonts.find(family);
    for(; iter != m_fonts.cend(); ++iter){
        return iter->second;
    }
    return nullptr;
}

void LSvgDocument::addNamedNode(const std::string &id, std::shared_ptr<LSvgNode> node)
{
    if(node != nullptr){
        m_namedNodes[id] = node;
    }
}

std::shared_ptr<LSvgNode> LSvgDocument::namedNode(const std::string &id) const
{
    std::unordered_map<std::string, std::shared_ptr<LSvgNode> >::const_iterator iter = m_namedNodes.find(id);
    for(; iter != m_namedNodes.cend(); ++iter){
        return iter->second;
    }
    return nullptr;
}

void LSvgDocument::addNamedStyle(const std::string &id, std::shared_ptr<LSvgPaintStyleProperty> style)
{
    if(style != nullptr){
        m_namedStyles[id] = style;
    }
}

std::shared_ptr<LSvgPaintStyleProperty> LSvgDocument::namedStyle(const std::string &id) const
{    
    std::unordered_map<std::string, std::shared_ptr<LSvgPaintStyleProperty> >::const_iterator iter = m_namedStyles.find(id);
    for(; iter != m_namedStyles.cend(); ++iter){
        return iter->second;
    }
    return nullptr;
}

void LSvgDocument::restartAnimation()
{
    m_animator->restartAnimation();
}

bool LSvgDocument::animated() const
{
    return m_animated;
}

void LSvgDocument::setAnimated(bool a)
{
    m_animated = a;
}

void LSvgDocument::draw(LPainter *p)
{
    draw(p, LRect<double>());
}

void LSvgDocument::drawCommand(LPainter *, LSvgExtraStates &)
{
    lWarn() << "LSvgDocument does not support nested <svg> elements: ignored.";
    return;
}

static bool isValidMatrix(const LTransform2D &transform)
{
    double determinant = transform.determinant();
    return std::isfinite(determinant);
}

void LSvgDocument::mapSourceToTarget(LPainter *p, const LRect<double> &targetRect, const LRect<double> &sourceRect)
{
    LTransform2D oldTransform = p->worldTransform();

    LRect<double> target = targetRect;
    if (target.isEmpty()) {
        std::shared_ptr<LPaintDevice> dev = p->device();
        LRect<double> deviceRect(0, 0, dev->width(), dev->height());
        if (deviceRect.isEmpty()) {
            if (sourceRect.isEmpty()){
                target = LRect<double>(LPoint<double>(0, 0), size().toSizeF());
            }else{
                target = LRect<double>(LPoint<double>(0, 0), sourceRect.size());
            }
        } else {
            target = deviceRect;
        }
    }

    LRect<double> source = sourceRect;
    if (source.isEmpty())
        source = viewBox();

    if (source != target && !lFuzzyIsZero(source.width()) && !lFuzzyIsZero(source.height())) {
        if (m_implicitViewBox || !preserveAspectRatio()) {
            // Code path used when no view box is set, or IgnoreAspectRatio requested
            LTransform2D transform;
            transform.scale(target.width() / source.width(),
                            target.height() / source.height());
            LRect<double> c2 = transform.mapRect(source);
            p->translate(LPoint<double>(target.m_x - c2.m_x, target.m_y - c2.m_y));
            p->scale(target.width() / source.width(),
                     target.height() / source.height());
        } else {
            // Code path used when KeepAspectRatio is requested. This attempts to emulate the default values
            // of the <preserveAspectRatio tag that's implicitly defined when <viewbox> is used.

            // Scale the view box into the view port (target) by preserve the aspect ratio.
            LSize<double> viewBoxSize = source.size();
            viewBoxSize.scale(target.width(), target.height(), LAspectRatioMode::KeepAspectRatio);

            // Center the view box in the view port
            p->translate(LPoint<double>(target.m_x + (target.m_width - viewBoxSize.m_x) / 2,
                         target.m_y + (target.m_height - viewBoxSize.m_y) / 2));

            p->scale(viewBoxSize.width() / source.width(),
                     viewBoxSize.height() / source.height());

            // Apply the view box translation if specified.
            p->translate(LPoint<double>(-source.m_x, -source.m_y));
        }
    }

    if (!isValidMatrix(p->worldTransform())){
        p->setWorldTransform(oldTransform);
    }
}

LRect<double> LSvgDocument::boundsOnElement(const std::string &id) const
{
    std::shared_ptr<LSvgNode> node = scopeNode(id);
    if (node == nullptr){
        return bounds();
    }
    return node->bounds();
}

bool LSvgDocument::elementExists(const std::string &id) const
{
    std::shared_ptr<LSvgNode> node = scopeNode(id);

    return (node!=0);
}

LTransform2D LSvgDocument::transformForElement(const std::string &id) const
{
    std::shared_ptr<LSvgNode> node = scopeNode(id);

    if (!node) {
        lWarn() <<"Couldn't find node " << id;
        return LTransform2D();
    }

    LTransform2D t;

    node = node->parent();
    while (node) {
        if (node->m_style.transform){
            t *= node->m_style.transform->transform();
        }
        node = node->parent();
    }

    return t;
}

int LSvgDocument::currentFrame() const
{
    const double runningPercentage = std::min<double>(currentElapsed() / double(animationDuration()), 1.);
    const int totalFrames = m_fps * animationDuration() / 1000;
    return int(runningPercentage * totalFrames);
}

void LSvgDocument::setCurrentFrame(int frame)
{
    const int totalFrames = m_fps * animationDuration() / 1000;
    const int timeForFrame = frame * animationDuration() / totalFrames; //in ms
    const int timeToAdd = timeForFrame - currentElapsed();
    m_animator->setAnimatorTime(timeToAdd);
}

void LSvgDocument::setFramesPerSecond(int num)
{
    m_fps = num;
}

std::shared_ptr<LSvgAbstractAnimator> LSvgDocument::animator() const
{
    return m_animator;
}



LK_NAMESPACE_END
