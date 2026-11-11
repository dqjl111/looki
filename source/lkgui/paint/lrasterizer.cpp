#include "lrasterizer.h"
#include "ldrawhelper.h"
#include "lkcore/ldebug.h"
#include "lkcore/3rd/fpmfixed.hpp"
#include "lkcore/3rd/fpmmath.hpp"
#include "lkgui/paint/lgrayraster.h"
#include <algorithm>

LK_NAMESPACE_BEGIN

#define SPAN_BUFFER_SIZE 256

typedef fpm::fixed_16_16 LFixedA;
typedef fpm::fixed_24_8 LFixedB;

#define LFixedAToFloat(i) double(i)
#define FloatToLFixedA(i) LFixedA(i)
#define IntToLFixedA(i) LFixedA(i)
#define LFixedAToInt(i) int(i)
#define LFixedAFactor LFixedA::FRACTION_MULT

#define LFixedAMultiply(x, y) (x).multiplyNoRound(y)
#define LFixedAFastMultiply(x, y) (x).fastMultiply(y)

#define SPAN_BUFFER_SIZE 256

static inline LPoint<LFixedB> elementToPointFixedB(const LPainterPath::Element &p)
{
    return LPoint<LFixedB>(LFixedB(p.m_x), LFixedB(p.m_y));
}


class LSpanBuffer {
public:
    LSpanBuffer(){}
    LSpanBuffer(ProcessSpans blend, void *data, const LRect<int> &clipRect)
        : m_spanCount(0)
        , m_blend(blend)
        , m_data(data)
        , m_clipRect(clipRect)
    {
    }



    void init(ProcessSpans blend, void *data, const LRect<int> &clipRect){
        m_spanCount = 0;
        m_blend = blend;
        m_data = data;
        m_clipRect = clipRect;
        memset((char*)(m_spans), 0, sizeof(LSpan)*SPAN_BUFFER_SIZE);
    }

    ~LSpanBuffer()
    {
        if(m_spanCount!=0){
            flushSpans();
        }
    }

    void addSpan(int x, unsigned int len, int y, unsigned char coverage)
    {
        if (!coverage || !len){
            return;
        }

        if(y >=m_clipRect.m_y && y < m_clipRect.y2()){
            int leftX = std::max<int>(x, m_clipRect.m_x);
            int rightX = std::min<int>(x+len, m_clipRect.x2());
            if(rightX>leftX){
                m_spans[m_spanCount].x = leftX;
                m_spans[m_spanCount].len = rightX-leftX;
                m_spans[m_spanCount].y = y;
                m_spans[m_spanCount].coverage = coverage;

                if (++m_spanCount == SPAN_BUFFER_SIZE){
                    flushSpans();
                }
            }
        }
    }

    void flushSpans()
    {
        m_blend(m_spanCount, m_spans, m_data);
        m_spanCount = 0;
    }

private:
    LSpan m_spans[SPAN_BUFFER_SIZE];
    int m_spanCount = 0;
    ProcessSpans m_blend = nullptr;
    void *m_data = nullptr;

    LRect<int> m_clipRect;
};

class LRasterConvertBase
{
public:
    LRasterConvertBase(){}
    virtual bool begin(const LRasterizer* rasterizer, const std::shared_ptr<LPainterPath> path)= 0;
    virtual void end() = 0;

    virtual void mergePath(const std::shared_ptr<LPainterPath> path) = 0;
};

#define CHUNK_SIZE 64
class LScanConverter : public LRasterConvertBase
{
public:
    LScanConverter();

    virtual bool begin(const LRasterizer* rasterizer, const std::shared_ptr<LPainterPath> path) override;
    virtual void end() override;

    virtual void mergePath(const std::shared_ptr<LPainterPath> path) override;
    void mergeCurve(const LPoint<LFixedB> &a, const LPoint<LFixedB> &b,
                    const LPoint<LFixedB> &c, const LPoint<LFixedB> &d);
    void mergeLine(LPoint<LFixedB> a, LPoint<LFixedB> b);
    inline void allocate(int size);

    struct Line
    {
        LFixedA x;
        LFixedA delta;
        int top, bottom;
        int winding;
    };
private:
    struct Intersection
    {
        int x = 0;
        int winding = 0;
        int left = 0, right = 0;
    };

    inline bool clip(LFixedA &xFP,
                     int &iTop,
                     int &iBottom,
                     LFixedA slopeFP,
                     LFixedA edgeFP,
                     int winding);
    inline void mergeIntersection(int currentId, const Intersection &isect);

    void prepareChunk(int chunkSize);

    void emitNode(int nodeId);
    void emitSpans(int chunk, int chunkSize);
    std::vector<Line> m_lines;
    int m_addInterNum;

    int m_top;
    int m_bottom;

    LFixedA m_leftFP;
    LFixedA m_rightFP;

    uint32_t m_fillRuleMask;
    int m_x;
    int m_y;
    int m_winding;

    std::vector<Intersection> m_intersections;

    LSpanBuffer m_spanBuffer;
    std::vector<Line *> m_active;

    friend void lScanConvert(LScanConverter* d, bool allVertical);
};


LScanConverter::LScanConverter():m_addInterNum(0)
{
}

bool LScanConverter::begin(const LRasterizer* rasterizer, const std::shared_ptr<LPainterPath> path)
{
    LRect<double> bounds = path->controlPointRect();
    m_top = std::max(rasterizer->clipRect().y1(), int(bounds.y1() + 0.5));
    m_bottom = std::min(rasterizer->clipRect().y2(), int(bounds.y2() - 0.5));
    if (m_top > m_bottom){
        return false;
    }

    m_top = bounds.m_y;
    m_bottom = bounds.y2();
    m_leftFP = IntToLFixedA(bounds.m_x);
    m_rightFP = IntToLFixedA(bounds.x2() + 1);

    m_lines.clear();
    m_fillRuleMask = path->fillRule() == LFillRule::Winding ? ~uint32_t(0) : 0x1;
    m_spanBuffer.init(rasterizer->blend(), rasterizer->data(), rasterizer->clipRect());
    return true;
}

void LScanConverter::prepareChunk(int chunkSize)
{
    m_intersections.resize(chunkSize);
    memset((char*)m_intersections.data(), 0, chunkSize * sizeof(Intersection));
    m_addInterNum = chunkSize;
}

void LScanConverter::emitNode(int nodeId)
{
    if(nodeId <0 || nodeId >=m_intersections.size() ){
        return;
    }
tail_call:
    const Intersection& node = m_intersections[nodeId];

    if (node.left !=0 ){
        emitNode(nodeId + node.left);
    }

    if (m_winding & m_fillRuleMask){
        m_spanBuffer.addSpan(m_x, node.x - m_x, m_y, 0xff);
    }

    m_x = node.x;
    m_winding += node.winding;

    if (node.right != 0) {
        nodeId += node.right;

        if(nodeId <0 || nodeId >=m_intersections.size() ){
            return;
        }else{
            goto tail_call;
        }
    }
}

void LScanConverter::emitSpans(int chunk, int chunkSize)
{
    for (int dy = 0; dy < chunkSize; ++dy) {
        m_x = 0;
        m_y = chunk + dy;
        m_winding = 0;

        emitNode(dy);
    }
}


// split control points b[0] ... b[3] into
// left (b[0] ... b[3]) and right (b[3] ... b[6])
static void split(LPoint<LFixedB> *b)
{
    b[6] = b[3];
    {
        const LFixedB temp = (b[1].m_x + b[2].m_x)/2;

        b[1].m_x = (b[0].m_x + b[1].m_x)/2;
        b[5].m_x = (b[2].m_x + b[3].m_x)/2;
        b[2].m_x = (b[1].m_x + temp)/2;
        b[4].m_x = (b[5].m_x + temp)/2;
        b[3].m_x = (b[2].m_x + b[4].m_x)/2;
    }
    {
        const LFixedB temp = (b[1].m_y + b[2].m_y)/2;

        b[1].m_y = (b[0].m_y + b[1].m_y)/2;
        b[5].m_y = (b[2].m_y + b[3].m_y)/2;
        b[2].m_y = (b[1].m_y + temp)/2;
        b[4].m_y = (b[5].m_y + temp)/2;
        b[3].m_y = (b[2].m_y + b[4].m_y)/2;
    }
}

static inline bool topOrder(const LScanConverter::Line &a, const LScanConverter::Line &b)
{
    return a.top < b.top;
}

static inline bool xOrder(const LScanConverter::Line *a, const LScanConverter::Line *b)
{
    return a->x < b->x;
}

void lScanConvert(LScanConverter* d, bool allVertical)
{
    d->m_active.clear();
    if (d->m_lines.empty()) {        
        return;
    }
    sort(d->m_lines.begin(), d->m_lines.end(), topOrder);
    uint32_t line = 0;
    for (int y = d->m_lines[0].top; y < d->m_bottom; ++y) {
        for (; line < d->m_lines.size() && d->m_lines[line].top == y; ++line) {
            LScanConverter::Line *l = &d->m_lines[line];
            // add node to active list
            if (allVertical) {
                d->m_active.resize(d->m_active.size() + 1);
                int j;
                for (j = d->m_active.size() - 2; j >= 0 && xOrder(l, d->m_active[j]); --j){
                    d->m_active[j+1] = d->m_active[j];
                }
                d->m_active[j+1] = l;
            } else {
                d->m_active.push_back(l);
            }
        }

        int numActive = d->m_active.size();
        if (!allVertical) {
            // use insertion sort, as the active edge list is quite small
            // and in the average case already sorted
            for (int i = 1; i < numActive; ++i) {
                LScanConverter::Line *l = d->m_active[i];
                int j;
                for (j = i-1; j >= 0 && xOrder(l, d->m_active[j]); --j){
                    d->m_active[j+1] = d->m_active[j];
                }
                d->m_active[j+1] = l;
            }
        }

        int x = 0;
        int winding = 0;

        for (int i = 0; i < numActive; ++i) {
            LScanConverter::Line *node = d->m_active[i];
            const int current = std::lround(LFixedAToFloat(node->x));
            if (winding & d->m_fillRuleMask){
                d->m_spanBuffer.addSpan(x, current - x, y, 0xff);
            }

            winding += node->winding;
            x = current;

            if (node->bottom <= y) {
                // remove node from active list
                for (int j = i; j < numActive - 1; ++j){
                    d->m_active[j] = d->m_active[j+1];
                }
                d->m_active.resize(--numActive);
                --i;
            } else if (!allVertical){
                node->x += node->delta;
            }
        }
    }
    d->m_active.clear();
}

inline void LScanConverter::allocate(int size)
{
    if ((int)m_intersections.size() < size) {
        int newAlloc = std::max<int>(size, 2 * m_intersections.size());
        m_intersections.resize(newAlloc);
    }
}


void LScanConverter::end()
{
    if (!m_lines.empty()){
        if (m_lines.size() <= 32) {
            bool allVertical = true;
            for (const Line& line : m_lines) {
                if (line.delta.raw_value()) {
                    allVertical = false;
                    break;
                }
            }
            lScanConvert(this, allVertical);
        } else {
            for (int chunkTop = m_top; chunkTop < m_bottom; chunkTop += CHUNK_SIZE) {
                prepareChunk(CHUNK_SIZE);

                Intersection isect = { 0, 0, 0, 0 };

                const int chunkBottom = chunkTop + CHUNK_SIZE;
                for (Line &line : m_lines) {
                    if ((line.bottom < chunkTop) || (line.top > chunkBottom)){ //线条不在区块内跳过
                        //lWarn() << "skip line";
                        continue;
                    }

                    const int top = std::max(0, line.top - chunkTop);
                    const int bottom = std::min(CHUNK_SIZE, line.bottom + 1 - chunkTop);

                    allocate(m_addInterNum + bottom - top);

                    isect.winding = line.winding;
                    int itId = top;

                    if (line.delta) {
                        for (; itId < bottom; ++itId) {
                            isect.x = std::lround(LFixedAToFloat(line.x));
                            line.x += line.delta;
                            mergeIntersection(itId, isect);
                        }
                    } else {
                        isect.x = std::lround(LFixedAToFloat(line.x));
                        for (; itId < bottom; ++itId){
                            mergeIntersection(itId, isect);
                        }
                    }
                }
                emitSpans(chunkTop, CHUNK_SIZE);
            }
        }

        if (m_intersections.size() > 1024) {
            m_addInterNum = 0;
            m_intersections.clear();
        }

        if (m_lines.size() > 1024){
            m_lines.clear();
        }
    }
    m_spanBuffer.flushSpans();
}

inline void LScanConverter::mergeIntersection(int currentId, const Intersection &isect)
{
    while ((currentId < (int)m_intersections.size())
           && (currentId >=0)
           && (isect.x != m_intersections[currentId].x)) {

        Intersection& current = m_intersections[currentId];
        int& next = isect.x < current.x ? current.left : current.right;
        if (next){
            currentId += next;
        }
        else {
            next = m_addInterNum - currentId;
            m_intersections[m_addInterNum] = isect;
            ++m_addInterNum;
            return;
        }
    }

    m_intersections[currentId].winding += isect.winding;
}


inline bool LScanConverter::clip(LFixedA &xFP,
                                 int &iTop,
                                 int &iBottom,
                                 LFixedA slopeFP,
                                 LFixedA edgeFP,
                                 int winding)
{
    bool right = edgeFP == m_rightFP;

    if (xFP == edgeFP) {
        if ((slopeFP > LFixedA()) ^ right){
            return false;
        }
        else {
            Line line = { edgeFP, LFixedA(), int(iTop), int(iBottom), winding};
            m_lines.push_back(line);
            return true;
        }
    }

    LFixedA lastFP = xFP + slopeFP * (iBottom - iTop);

    if (lastFP == edgeFP) {
        if ((slopeFP < LFixedA()) ^ right){
            return false;
        }
        else {
            Line line = { edgeFP, LFixedA(), int(iTop), int(iBottom), winding};
            m_lines.push_back(line);
            return true;
        }
    }

    // does line cross edge?
    if ((lastFP < edgeFP) ^ (xFP < edgeFP)) {
        LFixedA deltaY = (edgeFP - xFP) / slopeFP;

        if ((xFP < edgeFP) ^ right) {
            // top segment needs to be clipped
            int iHeight = LFixedAToInt(deltaY) +1;
            int iMiddle = iTop + iHeight;
            // LFixedA iHeight = deltaY  + LFixedA(1);
            // LFixedA iMiddle = iTop + iHeight;

            Line line = { edgeFP, LFixedA(), int(iTop), int(iMiddle), winding};
            m_lines.push_back(line);

            if (iMiddle != iBottom) {
                xFP += slopeFP * (iHeight + 1);
                iTop = iMiddle + 1;
            } else{
                return true;
            }
        } else {
            // bottom segment needs to be clipped
            int iHeight = LFixedAToInt(deltaY);
            int iMiddle = iTop + iHeight;
            // LFixedA iMiddle = iTop + deltaY;
            if (iMiddle != iBottom) {
                Line line = { edgeFP, LFixedA(), int(iMiddle + 1), int(iBottom), winding};
                m_lines.push_back(line);
                iBottom = iMiddle;
            }
        }
        return false;
    } else if ((xFP < edgeFP) ^ right) {
        Line line = { edgeFP, LFixedA(), int(iTop), int(iBottom), winding};
        m_lines.push_back(line);
        return true;
    }

    return false;
}

void LScanConverter::mergeLine(LPoint<LFixedB> a, LPoint<LFixedB> b)
{
    int winding = 1;
    if (a.m_y > b.m_y) {
        std::swap(a, b);
        winding = -1;
    }
    const static LFixedB halfB =LFixedB::from_raw_value(LFixedB::FRACTION_MULT/2);
    const static LFixedA halfA =LFixedA::from_raw_value(LFixedA::FRACTION_MULT/2);

    int iTop = std::max(m_top, int(a.m_y+halfB));
    int iBottom = std::min(m_bottom, int(b.m_y-halfB));

    if (iTop <= iBottom) {
        if (b.m_x == a.m_x) {
            Line line = { lBound<LFixedA>(m_leftFP, LFixedA(a.m_x)+halfA, m_rightFP), LFixedA(), iTop, iBottom, winding};
            m_lines.push_back(line);
        } else {
            if(b.m_y != a.m_y){
                LFixedA slopeFP = LFixedA(b.m_x - a.m_x) / LFixedA(b.m_y - a.m_y);
                LFixedA xFP = LFixedA(a.m_x)+halfA + LFixedAMultiply(slopeFP, LFixedA(iTop) - LFixedA(a.m_y)+halfA);

                if (clip(xFP, iTop, iBottom, slopeFP, m_leftFP, winding)){
                    return;
                }

                if (clip(xFP, iTop, iBottom, slopeFP, m_rightFP, winding)){
                    return;
                }

                Line line = { xFP, slopeFP, iTop, iBottom, winding};
                m_lines.push_back(line);
            }
        }
    }
}

void LScanConverter::mergeCurve(const LPoint<LFixedB> &pa, const LPoint<LFixedB> &pb,
                                const LPoint<LFixedB> &pc, const LPoint<LFixedB> &pd)
{
    // make room for splits
    LPoint<LFixedB> beziers[4 + 3 * 32];
    LPoint<LFixedB> *b = beziers;

    b[0] = pa;
    b[1] = pb;
    b[2] = pc;
    b[3] = pd;

    const LFixedB flatness(0.25);
    const LFixedB one(1);

    while (b >= beziers) {
        LPoint<LFixedB> delta = { b[3].m_x - b[0].m_x, b[3].m_y - b[0].m_y };
        LFixedB l = fpm::abs(delta.m_x) + fpm::abs(delta.m_y);

        bool belowThreshold;
        if (l > one) {
            LFixedB d2 = fpm::abs((b[1].m_x-b[0].m_x) * delta.m_y - (b[1].m_y-b[0].m_y) * delta.m_x);
            LFixedB d3 = fpm::abs((b[2].m_x-b[0].m_x) * delta.m_y - (b[2].m_y-b[0].m_y) * delta.m_x);

            LFixedB d = d2 + d3;
            belowThreshold = (d <= flatness * l);
        } else {
            LFixedB d = fpm::abs(b[0].m_x-b[1].m_x) + fpm::abs(b[0].m_y-b[1].m_y) +
                        fpm::abs(b[0].m_x-b[2].m_x) + fpm::abs(b[0].m_y-b[2].m_y);

            belowThreshold = (d <= flatness);
        }

        if (belowThreshold || b == beziers + 3 *  32) {
            mergeLine(b[0], b[3]);
            b -= 3;
            continue;
        }

        split(b);
        b += 3;
    }
}

void LScanConverter::mergePath(const std::shared_ptr<LPainterPath> path){
    const std::vector<LPainterPath::Element>&  es = path->elements();

    if(path->isOutline()){
        for (uint32_t i = 0; i < es.size(); ++i) {
            const LPainterPath::Element& ele = es.at(i);
            if((i+1) < es.size()){
                switch(es.at(i+1).m_type){
                case LPainterPath::CurveToElement:{
                    if(i+2 < es.size()){
                        LPoint<LFixedB> p1 = elementToPointFixedB(ele);
                        LPoint<LFixedB> p2 = elementToPointFixedB(es.at(i+1));
                        LPoint<LFixedB> p3 = elementToPointFixedB(es.at(i+2));
                        LPoint<LFixedB> p4 = elementToPointFixedB(es.at(i+3));
                        mergeCurve(p1, p2, p3, p4);
                    }
                    i+=2;
                    break;
                }
                case LPainterPath::LineToElement:{
                    LPoint<LFixedB> p1 = elementToPointFixedB(ele);
                    LPoint<LFixedB> p2 = elementToPointFixedB(es.at(i+1));
                    mergeLine(p1, p2);
                    break;
                }
                default:{
                    break;
                }
                }
            }
        }
    }else{
        uint32_t subpathStart = 0;
        LPoint<LFixedB> last;
        for (uint32_t i = 0; i < es.size(); ++i) {
            const LPainterPath::Element& ele = es.at(i);

            switch (ele.m_type) {
            case LPainterPath::LineToElement:
            {
                LPoint<LFixedB> p = elementToPointFixedB(ele);
                mergeLine(last, p);
                last = p;
                break;
            }
            case LPainterPath::MoveToElement:
            {
                if (i != 0) {
                    LPoint<LFixedB> first = elementToPointFixedB(es.at(subpathStart));
                    // close previous subpath
                    if (first.m_x != last.m_x || first.m_y != last.m_y){
                        mergeLine(last, first);
                    }
                }
                subpathStart = i;
                last = elementToPointFixedB(ele);
                break;
            }
            case LPainterPath::CurveToElement:
            {
                if(i+2 < es.size()){
                    LPoint<LFixedB> p2 = elementToPointFixedB(ele);
                    LPoint<LFixedB> p3 = elementToPointFixedB(es.at(i+1));
                    LPoint<LFixedB> p4 = elementToPointFixedB(es.at(i+2));
                    mergeCurve(last, p2, p3, p4);
                    last = p4;
                }
                i+=2;
                break;
            }
            default:
                break;
            }
        }

        LPoint<LFixedB> first = elementToPointFixedB(es.at(subpathStart));
        // close path
        if (first.m_x != last.m_x || first.m_y != last.m_y){
            mergeLine(last, first);
        }
    }
}


////////////////////////////////////////////////
class LGrayScaleConverter : public LRasterConvertBase
{
public:
    LGrayScaleConverter(){}

    virtual bool begin(const LRasterizer* rasterizer, const std::shared_ptr<LPainterPath> path) override{
        m_points.clear();
        m_tags.clear();
        m_contours.clear();

        buildOutline(path);

        const LRect<int>& bounds = rasterizer->clipRect();
        m_rasterParams.source = &m_outline;
        m_rasterParams.flags = LK_FT_RASTER_FLAG_CLIP|LK_FT_RASTER_FLAG_AA;
        m_rasterParams.gray_spans = rasterizer->blend();
        m_rasterParams.user = rasterizer->data();
        m_rasterParams.clip_box = {bounds.m_x, bounds.m_y, bounds.x2(), bounds.y2() };
        return true;
    }
    virtual void end() override{}
    virtual void mergePath(std::shared_ptr<LPainterPath> path) override{

        // Initial size for raster pool is MINIMUM_FT_POOL_SIZE so as to
        // minimize memory reallocations. However if initial size for
        // raster pool is changed for lower value, reallocations will
        // occur normally.
        int rasterPoolSize = MINIMUM_FT_POOL_SIZE;
        uint8_t rasterPoolOnStack[MINIMUM_FT_POOL_SIZE + 0xf];
        uint8_t *rasterPoolBase = alignAddress(rasterPoolOnStack, 0xf);
        uint8_t *rasterPoolOnHeap = nullptr;


        m_raster.init();
        m_raster.reset(rasterPoolBase, rasterPoolSize);

        bool done = false;
        int error;

        int renderedSpans = 0;

        while (!done) {
            m_rasterParams.skip_spans = renderedSpans;
            error = m_raster.render(&m_rasterParams);

            // Out of memory, reallocate some more and try again...
            if (error == -6) { // ErrRaster_OutOfMemory from qgrayraster.c
                rasterPoolSize *= 2;
                if (rasterPoolSize > 1024 * 1024) {
                    lWarn() << "LPainter: Rasterization of primitive failed";
                    break;
                }

                renderedSpans += m_raster.renderedSpans();

                free(rasterPoolOnHeap);
                rasterPoolOnHeap = (uint8_t *)malloc(rasterPoolSize + 0xf);
                rasterPoolBase = alignAddress(rasterPoolOnHeap, 0xf);

                m_raster.init();
                m_raster.reset(rasterPoolBase, rasterPoolSize);
            } else {
                done = true;
            }
        }
        free(rasterPoolOnHeap);
    }


private:
    void buildOutline(std::shared_ptr<LPainterPath> path){
        if(path->isOutline()){
            path = path->toOutline();
        }

        const std::vector<LPainterPath::Element>&  es = path->elements();

        for (uint32_t i = 0; i < es.size(); ++i) {
            const LPainterPath::Element& ele = es.at(i);
            switch(ele.m_type){
            case LPainterPath::CurveToElement:{
                if(i+2 < es.size()){
                    LK_FT_Vector p1 = {std::lround(ele.m_x*LFixedB::FRACTION_MULT),
                                       std::lround(ele.m_y*LFixedB::FRACTION_MULT)};

                    const LPainterPath::Element& ele2 = es.at(i+1);
                    LK_FT_Vector p2 = {std::lround(ele2.m_x*LFixedB::FRACTION_MULT),
                                       std::lround(ele2.m_y*LFixedB::FRACTION_MULT)};

                    const LPainterPath::Element& ele3 = es.at(i+2);
                    LK_FT_Vector p3 = {std::lround(ele3.m_x*LFixedB::FRACTION_MULT),
                                       std::lround(ele3.m_y*LFixedB::FRACTION_MULT)};

                    m_points.push_back(p1);
                    m_points.push_back(p2);
                    m_points.push_back(p3);
                    m_tags.push_back(LK_FT_CURVE_TAG_CUBIC);
                    m_tags.push_back(LK_FT_CURVE_TAG_CUBIC);
                    m_tags.push_back(LK_FT_CURVE_TAG_ON);
                }
                i+=2;
                break;
            }
            case LPainterPath::LineToElement:{
                LK_FT_Vector ptFixed = {std::lround(ele.m_x*LFixedB::FRACTION_MULT),
                                        std::lround(ele.m_y*LFixedB::FRACTION_MULT)};
                m_points.push_back(ptFixed);
                m_tags.push_back(LK_FT_CURVE_TAG_ON);
                break;
            }
            case LPainterPath::MoveToElement:{
                LK_FT_Vector ptFixed = {std::lround(ele.m_x*LFixedB::FRACTION_MULT),
                                        std::lround(ele.m_y*LFixedB::FRACTION_MULT)};

                if (i != 0){
                    m_contours.push_back(m_points.size() - 1);
                }
                m_points.push_back(ptFixed);
                m_tags.push_back(LK_FT_CURVE_TAG_ON);
                break;
            }
            default:{
                break;
            }
            }
        }

        // close the very last subpath
        m_contours.push_back(m_points.size() - 1);

        m_outline.flags = path->fillRule() == LFillRule::Winding
                              ? LK_FT_OUTLINE_NONE
                              : LK_FT_OUTLINE_EVEN_ODD_FILL;

        m_outline.n_contours = m_contours.size();
        m_outline.n_points = m_points.size();

        m_outline.points = m_points.data();
        m_outline.tags = m_tags.data();
        m_outline.contours = m_contours.data();
    }

    static inline uint8_t *alignAddress(uint8_t *address, luintptr alignmentMask)
    {
        return (uint8_t *)(((luintptr)address + alignmentMask) & ~alignmentMask);
    }

    LK_FT_Raster_Params m_rasterParams;
    LGrayRaster m_raster;
    LK_FT_Outline m_outline;

    std::vector<LK_FT_Vector> m_points;
    std::vector<char> m_tags;
    std::vector<int> m_contours;
};


/////////////////////////////////////////////////



LRasterizer::LRasterizer(){
}

void LRasterizer::setAntialiased(bool antialiased)
{
    m_antialiased = antialiased;
}

void LRasterizer::setClipRect(const LRect<int> &clipRect)
{
    m_clipRect = clipRect;
}

void LRasterizer::initialize(ProcessSpans blend, void *data)
{
    m_blend = blend;
    m_data = data;
}

void LRasterizer::rasterize(const std::shared_ptr<LPainterPath> path)
{
    if (path->isEmpty()){
        return;
    }

    std::shared_ptr<LRasterConvertBase> converter = nullptr;
    if(m_antialiased){
        if(m_grayConverter == nullptr){
            m_grayConverter = std::make_shared<LGrayScaleConverter>();
        }
        converter = m_grayConverter;
    }else{
        if(m_converter == nullptr){
            m_converter = std::make_shared<LScanConverter>();
        }
        converter = m_converter;
    }

    if(converter->begin(this, path)){
        converter->mergePath(path);
        converter->end();
    }
}

static LFixedA intersectPixelFP(int x, LFixedA top, LFixedA bottom, LFixedA leftIntersectX, LFixedA rightIntersectX, LFixedA slope, LFixedA invSlope)
{
    LFixedA leftX = IntToLFixedA(x);
    LFixedA rightX = IntToLFixedA(x) + LFixedA(1);

    LFixedA leftIntersectY, rightIntersectY;
    if (slope > LFixedA()) {
        leftIntersectY = top + LFixedAMultiply(leftX - leftIntersectX, invSlope);
        rightIntersectY = leftIntersectY + invSlope;
    } else {
        leftIntersectY = top + LFixedAMultiply(leftX - rightIntersectX, invSlope);
        rightIntersectY = leftIntersectY + invSlope;
    }

    if (leftIntersectX >= leftX && rightIntersectX <= rightX) {
        return LFixedAMultiply(bottom - top, leftIntersectX - leftX + ((rightIntersectX - leftIntersectX) >> 1));
    } else if (leftIntersectX >= rightX) {
        return bottom - top;
    } else if (leftIntersectX >= leftX) {
        if (slope > LFixedA()) {
            return (bottom - top) - LFixedAFastMultiply((rightX - leftIntersectX) >> 1, rightIntersectY - top);
        } else {
            return (bottom - top) - LFixedAFastMultiply((rightX - leftIntersectX) >> 1, bottom - rightIntersectY);
        }
    } else if (rightIntersectX <= leftX) {
        return LFixedA();
    } else if (rightIntersectX <= rightX) {
        if (slope > LFixedA()) {
            return LFixedAFastMultiply((rightIntersectX - leftX) >> 1, bottom - leftIntersectY);
        } else {
            return LFixedAFastMultiply((rightIntersectX - leftX) >> 1, leftIntersectY - top);
        }
    } else {
        if (slope > LFixedA()) {
            return (bottom - rightIntersectY) + ((rightIntersectY - leftIntersectY) >> 1);
        } else {
            return (rightIntersectY - top) + ((leftIntersectY - rightIntersectY) >> 1);
        }
    }
}



static inline bool lFixedBCompare(double p1, double p2)
{
    return LFixedB((p2  - p1)) == LFixedB();
}

static inline LPoint<double> snapToFixedBGrid(const LPoint<double> &p)
{
    return LPoint<double>(double(fpm::floor(LFixedB(p.m_x))), double(fpm::floor(LFixedB(p.m_y))));;
}

/*
   The rasterize line function relies on some div by zero which should
   result in +/-inf values. However, when floating point exceptions are
   enabled, this will cause crashes, so we return high numbers instead.
   As the returned value is used in further arithmetic, returning
   FLT_MAX/DBL_MAX will also cause values, so instead return a value
   that is well outside the int-range.
 */
static inline double lSafeDivide(double x, double y)
{
    if (y == 0){
        return x > 0 ? 1e20 : -1e20;
    }
    return x / y;
}

/* Conversion to int fails if the value is too large to fit into INT_MAX or
   too small to fit into INT_MIN, so we need this slightly safer conversion
   when floating point exceptions are enabled
 */
static inline LFixedA lSafeFloatToLFixedA(double x)
{
    return LFixedA::safe_from(x);
}

// returns true if the whole line gets clipped away
static inline bool lClipLine(LPoint<double> *pt1, LPoint<double> *pt2, const LRect<double> &clip)
{
    double &x1 = pt1->m_x;
    double &y1 = pt1->m_y;
    double &x2 = pt2->m_x;
    double &y2 = pt2->m_y;

    if (!std::isfinite(x1) || !std::isfinite(y1) || !std::isfinite(x2) || !std::isfinite(y2))
        return true;

    const double xmin = clip.m_x;
    const double xmax = clip.x2();
    const double ymin = clip.m_y;
    const double ymax = clip.y2();

    if (x1 < xmin) {
        if (x2 <= xmin){
            return true;
        }
        y1 += (y2 - y1) / (x2 - x1) * (xmin - x1);
        x1 = xmin;
    } else if (x1 > xmax) {
        if (x2 >= xmax){
            return true;
        }
        y1 += (y2 - y1) / (x2 - x1) * (xmax - x1);
        x1 = xmax;
    }
    if (x2 < xmin) {
        y2 += (y2 - y1) / (x2 - x1) * (xmin - x2);
        x2 = xmin;
    } else if (x2 > xmax) {
        y2 += (y2 - y1) / (x2 - x1) * (xmax - x2);
        x2 = xmax;
    }

    if (y1 < ymin) {
        if (y2 <= ymin){
            return true;
        }
        x1 += (x2 - x1) / (y2 - y1) * (ymin - y1);
        y1 = ymin;
    } else if (y1 > ymax) {
        if (y2 >= ymax){
            return true;
        }
        x1 += (x2 - x1) / (y2 - y1) * (ymax - y1);
        y1 = ymax;
    }
    if (y2 < ymin) {
        x2 += (x2 - x1) / (y2 - y1) * (ymin - y2);
        y2 = ymin;
    } else if (y2 > ymax) {
        x2 += (x2 - x1) / (y2 - y1) * (ymax - y2);
        y2 = ymax;
    }

    return false;
}


void LRasterizer::rasterizeLine(const LPoint<double> &a, const LPoint<double> &b, double width, bool squareCap)
{
    if (a == b || !(width > 0.0) || m_clipRect.isEmpty()){
        return;
    }

    LPoint<double> pa = a;
    LPoint<double> pb = b;

    if (squareCap) {
        LPoint<double> delta = pb - pa;
        pa -= (0.5 * width) * delta;
        pb += (0.5 * width) * delta;
    }

    LPoint<double> offs = LPoint<double>(std::abs(b.m_y - a.m_y), std::abs(b.m_x - a.m_x)) * width * 0.5;
    const LRect<double> clip(m_clipRect.topLeft().toPointF() - offs,
                             m_clipRect.bottomRight().toPointF()  + LPoint<double>(1, 1) + offs);

    if (lClipLine(&pa, &pb, clip)){
        return;
    }

    {
        // old delta
        const LPoint<double> d0 = a - b;
        const double w0 = d0.m_x * d0.m_x + d0.m_y * d0.m_y;

        // new delta
        const LPoint<double> d = pa - pb;
        const double w = d.m_x * d.m_x + d.m_y * d.m_y;

        if (w == 0){
            return;
        }

        // adjust width which is given relative to |b - a|
        width *= std::sqrt(w0 / w);
    }

    LSpanBuffer buffer(m_blend, m_data, m_clipRect);

    if (lFixedBCompare(pa.m_y, pb.m_y)) {
        const double x = (pa.m_x + pb.m_x) * 0.5;
        const double dx = std::abs(pb.m_x - pa.m_x) * 0.5;

        const double y = pa.m_y;
        const double dy = width * dx;

        pa = LPoint<double>(x, y - dy);
        pb = LPoint<double>(x, y + dy);

        width = 1 / width;
    }

    if (lFixedBCompare(pa.m_x, pb.m_x)) {
        if (pa.m_y > pb.m_y){
            std::swap(pa, pb);
        }

        const double dy = pb.m_y - pa.m_y;
        const double halfWidth = 0.5 * width * dy;

        double left = pa.m_x - halfWidth;
        double right = pa.m_x + halfWidth;

        left = lBound(double(m_clipRect.m_x), left, double(m_clipRect.x2()+1));
        right = lBound(double(m_clipRect.m_x), right, double(m_clipRect.x2()+1));

        pa.m_y = lBound(double(m_clipRect.m_y), pa.m_y, double(m_clipRect.y2()+1));
        pb.m_y = lBound(double(m_clipRect.m_y), pb.m_y, double(m_clipRect.y2()+1));

        if (lFixedBCompare(left, right) || lFixedBCompare(pa.m_y, pb.m_y)){
            return;
        }

        if (m_antialiased) {
            const int iLeft = int(left);
            const int iRight = int(right);
            const LFixedA leftWidth = IntToLFixedA(iLeft + 1)
                                       - lSafeFloatToLFixedA(left);
            const LFixedA rightWidth = lSafeFloatToLFixedA(right)
                                        - IntToLFixedA(iRight);

            LFixedA coverage[3];
            int x[3];
            int len[3];

            int n = 1;
            if (iLeft == iRight) {
                coverage[0] = (leftWidth + rightWidth) * 255;
                x[0] = iLeft;
                len[0] = 1;
            } else {
                coverage[0] = leftWidth * 255;
                x[0] = iLeft;
                len[0] = 1;
                if (leftWidth == LFixedA(1)) {
                    len[0] = iRight - iLeft;
                } else if (iRight - iLeft > 1) {
                    coverage[1] = IntToLFixedA(255);
                    x[1] = iLeft + 1;
                    len[1] = iRight - iLeft - 1;
                    ++n;
                }
                if (rightWidth) {
                    coverage[n] = rightWidth * 255;
                    x[n] = iRight;
                    len[n] = 1;
                    ++n;
                }
            }

            const LFixedA iTopFP = IntToLFixedA(int(pa.m_y));
            const LFixedA iBottomFP = IntToLFixedA(int(pb.m_y));
            const LFixedA yPa = lSafeFloatToLFixedA(pa.m_y);
            const LFixedA yPb = lSafeFloatToLFixedA(pb.m_y);
            for (LFixedA yFP = iTopFP; yFP <= iBottomFP; yFP += LFixedA(1)) {
                const LFixedA rowHeight = std::min<LFixedA>(yFP + LFixedA(1), yPb) - std::max<LFixedA>(yFP, yPa);
                const int y = LFixedAToInt(yFP);
                if (y > m_clipRect.y2()){
                    break;
                }
                for (int i = 0; i < n; ++i) {
                    buffer.addSpan(x[i], len[i], y,
                                   LFixedAToInt(LFixedAMultiply(rowHeight, coverage[i])));
                }
            }
        } else { // aliased
            int iTop = int(pa.m_y + 0.5);
            int iBottom = pb.m_y < 0.5 ? -1 : int(pb.m_y - 0.5);
            int iLeft = int(left + 0.5);
            int iRight = right < 0.5 ? -1 : int(right - 0.5);

            int iWidth = iRight - iLeft + 1;
            for (int y = iTop; y <= iBottom; ++y){
                buffer.addSpan(iLeft, iWidth, y, 255);
            }
        }
    } else {
        if (pa.m_y > pb.m_y){
            std::swap(pa, pb);
        }

        LPoint<double> delta = pb - pa;
        delta *= 0.5 * width;
        const LPoint<double> perp(delta.m_y, -delta.m_x);

        LPoint<double> top;
        LPoint<double> left;
        LPoint<double> right;
        LPoint<double> bottom;

        if (pa.m_x < pb.m_x) {
            top = pa + perp;
            left = pa - perp;
            right = pb + perp;
            bottom = pb - perp;
        } else {
            top = pa - perp;
            left = pb - perp;
            right = pa + perp;
            bottom = pb + perp;
        }

        top = snapToFixedBGrid(top);
        bottom = snapToFixedBGrid(bottom);
        left = snapToFixedBGrid(left);
        right = snapToFixedBGrid(right);

        const double topBound = lBound(double(m_clipRect.m_y), top.m_y, double(m_clipRect.y2()));
        const double bottomBound = lBound(double(m_clipRect.m_y), bottom.m_y, double(m_clipRect.y2()));

        const LPoint<double> topLeftEdge = left - top;
        const LPoint<double> topRightEdge = right - top;
        const LPoint<double> bottomLeftEdge = bottom - left;
        const LPoint<double> bottomRightEdge = bottom - right;

        const double topLeftSlope = lSafeDivide(topLeftEdge.m_x, topLeftEdge.m_y);
        const double bottomLeftSlope = lSafeDivide(bottomLeftEdge.m_x, bottomLeftEdge.m_y);

        const double topRightSlope = lSafeDivide(topRightEdge.m_x, topRightEdge.m_y);
        const double bottomRightSlope = lSafeDivide(bottomRightEdge.m_x, bottomRightEdge.m_y);

        const LFixedA topLeftSlopeFP = lSafeFloatToLFixedA(topLeftSlope);
        const LFixedA topRightSlopeFP = lSafeFloatToLFixedA(topRightSlope);

        const LFixedA bottomLeftSlopeFP = lSafeFloatToLFixedA(bottomLeftSlope);
        const LFixedA bottomRightSlopeFP = lSafeFloatToLFixedA(bottomRightSlope);

        const LFixedA invTopLeftSlopeFP = lSafeFloatToLFixedA(lSafeDivide(1, topLeftSlope));
        const LFixedA invTopRightSlopeFP = lSafeFloatToLFixedA(lSafeDivide(1, topRightSlope));

        const LFixedA invBottomLeftSlopeFP = lSafeFloatToLFixedA(lSafeDivide(1, bottomLeftSlope));
        const LFixedA invBottomRightSlopeFP = lSafeFloatToLFixedA(lSafeDivide(1, bottomRightSlope));

        if (m_antialiased) {
            const LFixedA iTopFP = IntToLFixedA(int(topBound));
            const LFixedA iLeftFP = IntToLFixedA(int(left.m_y));
            const LFixedA iRightFP = IntToLFixedA(int(right.m_y));
            const LFixedA iBottomFP = IntToLFixedA(int(bottomBound));

            LFixedA leftIntersectAf = lSafeFloatToLFixedA(top.m_x + (int(topBound) - top.m_y) * topLeftSlope);
            LFixedA rightIntersectAf = lSafeFloatToLFixedA(top.m_x + (int(topBound) - top.m_y) * topRightSlope);
            LFixedA leftIntersectBf;
            LFixedA rightIntersectBf;

            if (iLeftFP < iTopFP){
                leftIntersectBf = lSafeFloatToLFixedA(left.m_x + (int(topBound) - left.m_y) * bottomLeftSlope);
            }

            if (iRightFP < iTopFP){
                rightIntersectBf = lSafeFloatToLFixedA(right.m_x + (int(topBound) - right.m_y) * bottomRightSlope);
            }

            LFixedA rowTop, rowBottomLeft, rowBottomRight, rowTopLeft, rowTopRight, rowBottom;
            LFixedA topLeftIntersectAf, topLeftIntersectBf, topRightIntersectAf, topRightIntersectBf;
            LFixedA bottomLeftIntersectAf, bottomLeftIntersectBf, bottomRightIntersectAf, bottomRightIntersectBf;

            int leftMin, leftMax, rightMin, rightMax;

            const LFixedA yTopFP = lSafeFloatToLFixedA(top.m_y);
            const LFixedA yLeftFP = lSafeFloatToLFixedA(left.m_y);
            const LFixedA yRightFP = lSafeFloatToLFixedA(right.m_y);
            const LFixedA yBottomFP = lSafeFloatToLFixedA(bottom.m_y);

            rowTop = std::max<LFixedA>(iTopFP, yTopFP);
            topLeftIntersectAf = leftIntersectAf +
                                 LFixedAMultiply(topLeftSlopeFP, rowTop - iTopFP);
            topRightIntersectAf = rightIntersectAf +
                                  LFixedAMultiply(topRightSlopeFP, rowTop - iTopFP);

            LFixedA yFP = iTopFP;
            while (yFP <= iBottomFP) {
                rowBottomLeft = std::min<LFixedA>(yFP + LFixedA(1), yLeftFP);
                rowBottomRight = std::min<LFixedA>(yFP + LFixedA(1), yRightFP);
                rowTopLeft = std::max<LFixedA>(yFP, yLeftFP);
                rowTopRight = std::max<LFixedA>(yFP, yRightFP);
                rowBottom = std::min<LFixedA>(yFP + LFixedA(1), yBottomFP);

                if (yFP == iLeftFP) {
                    const int y = LFixedAToInt(yFP);
                    leftIntersectBf = lSafeFloatToLFixedA(left.m_x + (y - left.m_y) * bottomLeftSlope);
                    topLeftIntersectBf = leftIntersectBf + LFixedAMultiply(bottomLeftSlopeFP, rowTopLeft - yFP);
                    bottomLeftIntersectAf = leftIntersectAf + LFixedAMultiply(topLeftSlopeFP, rowBottomLeft - yFP);
                } else {
                    topLeftIntersectBf = leftIntersectBf;
                    bottomLeftIntersectAf = leftIntersectAf + topLeftSlopeFP;
                }

                if (yFP == iRightFP) {
                    const int y = LFixedAToInt(yFP);
                    rightIntersectBf = lSafeFloatToLFixedA(right.m_x + (y - right.m_y) * bottomRightSlope);
                    topRightIntersectBf = rightIntersectBf + LFixedAMultiply(bottomRightSlopeFP, rowTopRight - yFP);
                    bottomRightIntersectAf = rightIntersectAf + LFixedAMultiply(topRightSlopeFP, rowBottomRight - yFP);
                } else {
                    topRightIntersectBf = rightIntersectBf;
                    bottomRightIntersectAf = rightIntersectAf + topRightSlopeFP;
                }

                if (yFP == iBottomFP) {
                    bottomLeftIntersectBf = leftIntersectBf + LFixedAMultiply(bottomLeftSlopeFP, rowBottom - yFP);
                    bottomRightIntersectBf = rightIntersectBf + LFixedAMultiply(bottomRightSlopeFP, rowBottom - yFP);
                } else {
                    bottomLeftIntersectBf = leftIntersectBf + bottomLeftSlopeFP;
                    bottomRightIntersectBf = rightIntersectBf + bottomRightSlopeFP;
                }

                if (yFP < iLeftFP) {
                    leftMin = LFixedAToInt(bottomLeftIntersectAf);
                    leftMax = LFixedAToInt(topLeftIntersectAf);
                } else if (yFP == iLeftFP) {
                    leftMin = LFixedAToInt(std::max<LFixedA>(bottomLeftIntersectAf, topLeftIntersectBf));
                    leftMax = LFixedAToInt(std::max<LFixedA>(topLeftIntersectAf, bottomLeftIntersectBf));
                } else {
                    leftMin = LFixedAToInt(topLeftIntersectBf);
                    leftMax = LFixedAToInt(bottomLeftIntersectBf);
                }

                leftMin = lBound(m_clipRect.m_x, leftMin, m_clipRect.x2());
                leftMax = lBound(m_clipRect.m_x, leftMax, m_clipRect.x2());

                if (yFP < iRightFP) {
                    rightMin = LFixedAToInt(topRightIntersectAf);
                    rightMax = LFixedAToInt(bottomRightIntersectAf);
                } else if (yFP == iRightFP) {
                    rightMin = LFixedAToInt(std::min<LFixedA>(topRightIntersectAf, bottomRightIntersectBf));
                    rightMax = LFixedAToInt(std::min<LFixedA>(bottomRightIntersectAf, topRightIntersectBf));
                } else {
                    rightMin = LFixedAToInt(bottomRightIntersectBf);
                    rightMax = LFixedAToInt(topRightIntersectBf);
                }

                rightMin = lBound(m_clipRect.m_x, rightMin, m_clipRect.x2());
                rightMax = lBound(m_clipRect.m_x, rightMax, m_clipRect.x2());

                if (leftMax > rightMax){
                    leftMax = rightMax;
                }
                if (rightMin < leftMin){
                    rightMin = leftMin;
                }

                LFixedA rowHeight = rowBottom - rowTop;

                int x = leftMin;
                while (x <= leftMax) {
                    LFixedA excluded;

                    if (yFP <= iLeftFP){
                        excluded += intersectPixelFP(x, rowTop, rowBottomLeft,
                                                     bottomLeftIntersectAf, topLeftIntersectAf,
                                                     topLeftSlopeFP, invTopLeftSlopeFP);
                    }
                    if (yFP >= iLeftFP){
                        excluded += intersectPixelFP(x, rowTopLeft, rowBottom,
                                                     topLeftIntersectBf, bottomLeftIntersectBf,
                                                     bottomLeftSlopeFP, invBottomLeftSlopeFP);
                    }

                    if (x >= rightMin) {
                        if (yFP <= iRightFP){
                            excluded += (rowBottomRight - rowTop) - intersectPixelFP(x, rowTop, rowBottomRight,
                                                                                     topRightIntersectAf, bottomRightIntersectAf,
                                                                                     topRightSlopeFP, invTopRightSlopeFP);
                        }
                        if (yFP >= iRightFP){
                            excluded += (rowBottom - rowTopRight) - intersectPixelFP(x, rowTopRight, rowBottom,
                                                                                     bottomRightIntersectBf, topRightIntersectBf,
                                                                                     bottomRightSlopeFP, invBottomRightSlopeFP);
                        }
                    }

                    LFixedA coverage = rowHeight - excluded;
                    buffer.addSpan(x, 1, LFixedAToInt(yFP), LFixedAToInt(255 * coverage));
                    ++x;
                }
                if (x < rightMin) {
                    buffer.addSpan(x, rightMin - x, LFixedAToInt(yFP), LFixedAToInt(255 * rowHeight));
                    x = rightMin;
                }
                while (x <= rightMax) {
                    LFixedA excluded;
                    if (yFP <= iRightFP)
                        excluded += (rowBottomRight - rowTop) - intersectPixelFP(x, rowTop, rowBottomRight,
                                                                                 topRightIntersectAf, bottomRightIntersectAf,
                                                                                 topRightSlopeFP, invTopRightSlopeFP);
                    if (yFP >= iRightFP)
                        excluded += (rowBottom - rowTopRight) - intersectPixelFP(x, rowTopRight, rowBottom,
                                                                                 bottomRightIntersectBf, topRightIntersectBf,
                                                                                 bottomRightSlopeFP, invBottomRightSlopeFP);

                    LFixedA coverage = rowHeight - excluded;
                    buffer.addSpan(x, 1, LFixedAToInt(yFP),
                                   LFixedAToInt(255 * coverage));
                    ++x;
                }

                leftIntersectAf += topLeftSlopeFP;
                leftIntersectBf += bottomLeftSlopeFP;
                rightIntersectAf += topRightSlopeFP;
                rightIntersectBf += bottomRightSlopeFP;
                topLeftIntersectAf = leftIntersectAf;
                topRightIntersectAf = rightIntersectAf;

                yFP += LFixedA(1);
                rowTop = yFP;
            }
        } else { // aliased
            int iTop = int(top.m_y + 0.5);
            int iLeft = left.m_y < 0.5 ? -1 : int(left.m_y - 0.5);
            int iRight = right.m_y < 0.5 ? -1 : int(right.m_y - 0.5);
            int iBottom = bottom.m_y < 0.5? -1 : int(bottom.m_y - 0.5);
            int iMiddle = std::min<int>(iLeft, iRight);

            LFixedA leftIntersectAf = lSafeFloatToLFixedA(top.m_x + 0.5 + (iTop + 0.5 - top.m_y) * topLeftSlope);
            LFixedA leftIntersectBf = lSafeFloatToLFixedA(left.m_x + 0.5 + (iLeft + 1.5 - left.m_y) * bottomLeftSlope);
            LFixedA rightIntersectAf = lSafeFloatToLFixedA(top.m_x - 0.5 + (iTop + 0.5 - top.m_y) * topRightSlope);
            LFixedA rightIntersectBf = lSafeFloatToLFixedA(right.m_x - 0.5 + (iRight + 1.5 - right.m_y) * bottomRightSlope);

            int ny;
            int y = iTop;
#define DO_SEGMENT(next, li, ri, ls, rs) \
            ny = std::min<int>(next + 1, m_clipRect.m_y); \
            if (y < ny) { \
                li += ls * (ny - y); \
                ri += rs * (ny - y); \
                y = ny; \
            } \
            if (next > m_clipRect.y2()){ \
                next = m_clipRect.y2(); \
            } \
            for (; y <= next; ++y) { \
                const int x1 = std::max<int>(LFixedAToInt(li), m_clipRect.m_x); \
                const int x2 = std::min<int>(LFixedAToInt(ri), m_clipRect.x2()); \
                if (x2 >= x1){ \
                    buffer.addSpan(x1, x2 - x1 + 1, y, 255); \
                } \
                li += ls; \
                ri += rs; \
            }

            DO_SEGMENT(iMiddle, leftIntersectAf, rightIntersectAf, topLeftSlopeFP, topRightSlopeFP)
            DO_SEGMENT(iRight, leftIntersectBf, rightIntersectAf, bottomLeftSlopeFP, topRightSlopeFP)
            DO_SEGMENT(iLeft, leftIntersectAf, rightIntersectBf, topLeftSlopeFP, bottomRightSlopeFP);
            DO_SEGMENT(iBottom, leftIntersectBf, rightIntersectBf, bottomLeftSlopeFP, bottomRightSlopeFP);
#undef DO_SEGMENT
        }
    }

}


LK_NAMESPACE_END
