#ifndef LRASTERIZER_H
#define LRASTERIZER_H
#include <lkgui/lrect.h>
#include <lkgui/paint/lpainterpath.h>
#include "lkgui/paint/ldrawhelper.h"

LK_NAMESPACE_BEGIN

class LRasterConvertBase;
class LKGUIDEF LRasterizer
{
public:
    LRasterizer();

    void setAntialiased(bool antialiased);
    void setClipRect(const LRect<int> &clipRect);

    void initialize(ProcessSpans blend, void *data);
    void rasterize(const std::shared_ptr<LPainterPath> path);

    // width should be in units of |a-b|
    void rasterizeLine(const LPoint<double> &a,
                       const LPoint<double> &b,
                       double width,
                       bool squareCap = false);

    inline bool isAntialised() const{
        return m_antialiased;
    }

    inline ProcessSpans blend() const{
        return m_blend;
    }

    const LRect<int>& clipRect() const{
        return m_clipRect;
    }

    void* data() const{
        return m_data;
    }
private:
    bool m_antialiased;
    ProcessSpans m_blend;
    void *m_data;
    LRect<int> m_clipRect;
    std::shared_ptr<LRasterConvertBase> m_converter;
    std::shared_ptr<LRasterConvertBase> m_grayConverter;
};


LK_NAMESPACE_END
#endif // LRASTERIZER_H
