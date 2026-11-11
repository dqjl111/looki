#ifndef LSVGRENDERER_H
#define LSVGRENDERER_H

#include "lkcore/lobject.h"
#include "lkgraph/svg/lsvgdocument.h"
#include "lkcore/lsignal.h"
#include "lkcore/time/ltimer.h"

LK_NAMESPACE_BEGIN

class LKSVGDEF LSvgRenderer : public LObject
{
public:
    LSvgRenderer();

    bool load(const std::string &filename);
    bool load(const std::wstring &filename);
    bool loadContent(const std::string &data);

    void render(LPainter *p);
    void render(LPainter *p, const LRect<double> &bounds);
    void render(LPainter *p, const std::string &elementId, const LRect<double>  &bounds= LRect<double> ());

    void repaintNeeded();

    LSignal<> sigRepaintNeeded;
private:
    void startOrStopTimer();
    void ensureTimerCreated();

    std::shared_ptr<LSvgDocument> doc;
    std::shared_ptr<LTimer> timer;
    int fps;
    LFlags<LSvgOption> options;
    bool animationEnabled = true;
};


LK_NAMESPACE_END

#endif // LSVGRENDERER_H
