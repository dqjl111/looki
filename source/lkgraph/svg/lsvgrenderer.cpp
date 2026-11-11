#include "lsvgrenderer.h"
#include <fstream>
#include "lkcore/lstringutil.h"
LK_NAMESPACE_BEGIN

LSvgRenderer::LSvgRenderer() {}

bool LSvgRenderer::load(const std::string &filename)
{
    std::ifstream xmlFile(filename);
    std::string   xmlContent{std::istreambuf_iterator<char>(xmlFile),std::istreambuf_iterator<char>()};
    return loadContent(xmlContent);
}

bool LSvgRenderer::load(const std::wstring &filename)
{
    std::string fileUtf8 = LStringUtil::wideToUtf8(filename);
    return load(fileUtf8);
}

bool LSvgRenderer::loadContent(const std::string &data)
{
    doc = LSvgDocument::loadContent(data, options);
    if(doc != nullptr){
        if(doc->size().isValid()){
            startOrStopTimer();
            doc->restartAnimation();
            repaintNeeded();
            return true;
        }else{
            doc = nullptr;
        }
    }
    return false;
}

void LSvgRenderer::render(LPainter *p)
{
    if (doc != nullptr) {
        doc->animator()->advanceAnimations();
        doc->draw(p);
    }
}

void LSvgRenderer::render(LPainter *p, const LRect<double> &bounds)
{
    if (doc != nullptr) {
        doc->animator()->advanceAnimations();
        doc->draw(p, bounds);
    }
}

void LSvgRenderer::render(LPainter *p, const std::string &elementId, const LRect<double> &bounds)
{
    if (doc != nullptr) {
        doc->animator()->advanceAnimations();
        doc->draw(p, elementId, bounds);
    }
}

void LSvgRenderer::repaintNeeded()
{
    sigRepaintNeeded.emt();
}

void LSvgRenderer::startOrStopTimer()
{
    if (animationEnabled && doc != nullptr && doc->animated() && fps > 0) {
        ensureTimerCreated();
        timer->start(1000 / fps);
    } else if (timer != nullptr) {
        timer->stop();
    }
}

void LSvgRenderer::ensureTimerCreated()
{
    if(timer == nullptr){
        timer = std::make_shared<LTimer>();
        timer->sigTimeout.connect(this, &LSvgRenderer::repaintNeeded);
    }
}





LK_NAMESPACE_END